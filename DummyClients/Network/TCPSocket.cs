using System;
using System.Buffers;
using System.IO.Pipelines;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using ClientCore.PacketHelper;

namespace ClientCore.Network
{
    internal class TCPSocket
    {
        private static Handler _handler;
        private Socket sock;
        private string _address;
        private int _port;
        private int _index;

        private static readonly int HeaderSize = Marshal.SizeOf<PacketHeader>();
        private const uint MaxPacketSize = 256 * 1024;

        // pauseWriterThreshold는 대형 스냅샷 하나가 다 모일 때까지 생산자가 멈추지 않도록 MaxPacketSize보다 크게 잡는다.
        private static readonly PipeOptions PipeOpts = new PipeOptions(
            pauseWriterThreshold: 1024 * 1024,
            resumeWriterThreshold: 512 * 1024,
            useSynchronizationContext: false);

        // 한 소켓의 SendAsync는 동시에 하나만 (Action tick과 Pong 송신 경합 방지).
        private readonly SemaphoreSlim _sendLock = new SemaphoreSlim(1, 1);

        public static void Initialize(Handler handler)
        {
            _handler = handler;
        }
        public TCPSocket(int sessionIndex)
        {
            _index = sessionIndex;
        }
        ~TCPSocket()
        {
            Close();
        }
        public async Task Connect(string address, int port)
        {
            try
            {
                _address = address;
                _port = port;
                sock?.Close();

                sock = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                // 서버→클라 브로드캐스트가 크므로 recv 버퍼에 여유를 준다. connect 전에 설정해야 윈도우 스케일에 반영됨.
                sock.ReceiveBufferSize = 256 * 1024;
                IPAddress server_addr = IPAddress.Parse(_address);
                IPEndPoint clientEP = new IPEndPoint(server_addr, _port);
                await sock.ConnectAsync(clientEP);
                _ = ReceiveLoopAsync();
            }
            catch (SocketException ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }
        public void Close()
        {
            if (sock != null)
            {
                sock.Close();
                sock.Dispose();
            }
        }

        // System.IO.Pipelines 기반 수신. 생산자(FillPipe: 소켓→PipeWriter)와 소비자(ReadPipe: 패킷 파싱)를
        // 분리해, 버퍼 이어붙이기/남은 조각 당기기를 런타임이 풀링 메모리로 처리한다(기존 recvBuffer + BlockCopy 제거).
        public async Task ReceiveLoopAsync()
        {
            var pipe = new Pipe(PipeOpts);
            Task writing = FillPipeAsync(sock, pipe.Writer);
            Task reading = ReadPipeAsync(pipe.Reader);
            await Task.WhenAll(reading, writing);
        }

        private static async Task FillPipeAsync(Socket socket, PipeWriter writer)
        {
            const int minimumBufferSize = 16 * 1024;
            Exception error = null;
            while (true)
            {
                Memory<byte> memory = writer.GetMemory(minimumBufferSize);
                int bytesRead;
                try
                {
                    bytesRead = await socket.ReceiveAsync(memory, SocketFlags.None);
                }
                catch (SocketException ex) { error = ex; break; }
                catch (ObjectDisposedException) { break; }

                if (bytesRead == 0)
                    break;

                writer.Advance(bytesRead);

                FlushResult result = await writer.FlushAsync();
                if (result.IsCompleted)
                    break;
            }
            await writer.CompleteAsync(error);
        }

        private async Task ReadPipeAsync(PipeReader reader)
        {
            Exception error = null;
            while (true)
            {
                ReadResult result = await reader.ReadAsync();
                ReadOnlySequence<byte> buffer = result.Buffer;

                SequencePosition consumed = buffer.Start;
                SequencePosition examined = buffer.End;
                try
                {
                    while (TryReadPacket(ref buffer, out byte[] packet))
                    {
                        if (packet != null)
                            _handler.HandleResponse(_index, packet);
                    }
                    consumed = buffer.Start;
                    examined = buffer.End;
                }
                catch (Exception ex)
                {
                    error = ex;
                    Console.WriteLine($"[{_index}] recv parse ex: {ex.Message}");
                    break;
                }

                reader.AdvanceTo(consumed, examined);

                if (result.IsCompleted)
                    break;
            }
            await reader.CompleteAsync(error);
        }

        // 시퀀스 앞에서 완성된 패킷 1개를 떼어낸다. 헤더는 무복사(MemoryMarshal.Read)로 해석.
        private static bool TryReadPacket(ref ReadOnlySequence<byte> buffer, out byte[] packet)
        {
            packet = null;
            if (buffer.Length < HeaderSize)
                return false;

            Span<byte> headerSpan = stackalloc byte[HeaderSize];
            buffer.Slice(0, HeaderSize).CopyTo(headerSpan);
            PacketHeader header = MemoryMarshal.Read<PacketHeader>(headerSpan);

            if (header.length < HeaderSize || header.length > MaxPacketSize)
                throw new InvalidOperationException(
                    $"invalid packet length {header.length} (opcode {header.opcode})");

            if (buffer.Length < header.length)
                return false;

            // 실제 처리하는 opcode만 복사/디스패치. 드롭될 대량 브로드캐스트는 byte[] 할당 없이 건너뛴다.
            if (Handler.ShouldHandle(header.opcode, header.flags))
                packet = buffer.Slice(0, header.length).ToArray();

            buffer = buffer.Slice(header.length);
            return true;
        }

        public async Task Send(byte[] binary)
        {
            await _sendLock.WaitAsync();
            try
            {
                await sock.SendAsync(new ArraySegment<byte>(binary), SocketFlags.None);
            }
            catch (SocketException ex)
            {
                Console.WriteLine(ex.ToString());
                throw;
            }
            catch (ObjectDisposedException)
            {
            }
            finally
            {
                _sendLock.Release();
            }
        }
    }
}
