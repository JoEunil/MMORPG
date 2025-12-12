using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net.Sockets;
using System.Security.Cryptography.X509Certificates;
using System.Net;
using static System.Net.Mime.MediaTypeNames;
using ClientCore.PacketHelper;
using System.Runtime.InteropServices;

namespace ClientCore.Network
{
    internal class TCPSocket : ITCPSocket
    {
        private readonly IHandlerSock _handler;
        private readonly ILogger _logger;
        private Socket sock;
        private string _address;
        private int _port;

        public TCPSocket(IHandlerSock handler, ILogger logger)
        {
            _handler = handler;
            _logger = logger;
        }
        ~TCPSocket()
        {
            Close();
        }
        public async Task Connect(string address, int port)
        {
            try
            {
               _logger.Log("Connect" + address + ", " + port);
                _address = address;
                _port = port;
                sock?.Close();

                sock = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                IPAddress server_addr = IPAddress.Parse(_address);
                IPEndPoint clientEP = new IPEndPoint(server_addr, _port);
                await sock.ConnectAsync(clientEP);
                _logger.Log("Server connected");
                _ = ReceiveLoopAsync(); // 수신 루프 백그라운드 실행
            }
            catch (SocketException ex) 
            {
                _logger.Log("Socket failed:"+ ex);
            }
        }
        private void Close()
        {
            if (sock != null)

            {
                sock.Close();
                sock.Dispose();
            }
        }
        private List<byte> recvBuffer = new List<byte>();
        public async Task ReceiveLoopAsync()
        {
            var buffer = new byte[4096];
            while (true)
            {
                try
                {
                    int received = await sock.ReceiveAsync(new ArraySegment<byte>(buffer), SocketFlags.None);
                    if (received == 0)
                    {
                        _logger.Log("Server disconected");
                        break; // 연결 종료
                    }
                        
                    // tcp 패킷 자르기, 동적 배열 기반으로 복사로 처리함.

                    recvBuffer.AddRange(buffer.Take(received));

                    while (true)
                    {
                        if (recvBuffer.Count < Marshal.SizeOf<PacketHeader>()) 
                            break;

                        // 헤더 확인
                        PacketHeader header = Packet.ExtractHeader(recvBuffer.ToArray());
                        if (recvBuffer.Count < header.length) break; // 아직 전체 패킷 안들어옴

                        byte[] packetData = recvBuffer.Take(header.length).ToArray();
                        recvBuffer.RemoveRange(0, header.length);
                        // 배열을 앞으로 당기면서 복사 발생.. 

                        // 패킷 처리
                        _handler.HandleResponse(packetData);
                    }
                }
                catch (SocketException ex)
                {
                    _logger.Log("Recv failed:" + ex);
                    break;
                }
                catch (ObjectDisposedException)
                {
                    _logger.Log("Recv failed:" );
                    break;
                }
            }
        }
        public void Send(byte[] binary)
        {
            try
            {
                int sent = sock.Send(binary);
                if (sent == 0)
                    _logger.Log("Send returned 0 bytes");
            }
            catch (SocketException ex)
            {
                _logger.Log("SocketException during Send: " + ex);
            }
            catch (ObjectDisposedException)
            {
                _logger.Log("Socket disposed during Send");
            }
        }
    }
}
