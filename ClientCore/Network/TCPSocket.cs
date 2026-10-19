using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading.Tasks;
using ClientCore.PacketHelper;

namespace ClientCore.Network
{
    internal class TCPSocket : ITCPSocket, IDisposable
    {
        private readonly IHandlerSock _handler;
        private readonly object _gate = new object();
        private Socket _socket;
        private bool _disposed;
        public event Action<string> OnDisconnected;
        public TCPSocket(IHandlerSock handler, ILogger logger) { _handler = handler; }
        public async Task Connect(string address, int port)
        {
            var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            lock (_gate)
            {
                if (_disposed) { socket.Dispose(); throw new ObjectDisposedException(nameof(TCPSocket)); }
                _socket?.Dispose();
                _socket = socket;
            }
            try
            {
                await socket.ConnectAsync(new IPEndPoint(IPAddress.Parse(address), port)).ConfigureAwait(false);
                lock (_gate)
                {
                    if (_socket != socket) throw new OperationCanceledException();
                }
                _ = ReceiveLoopAsync(socket);
            }
            catch
            {
                lock (_gate)
                {
                    if (_socket == socket) _socket = null;
                    socket.Dispose();
                }
                throw;
            }
        }
        private async Task ReceiveLoopAsync(Socket socket)
        {
            var buffer = new byte[4096];
            var pending = new List<byte>();
            string failure = null;
            try
            {
                while (true)
                {
                    int received = await socket.ReceiveAsync(new ArraySegment<byte>(buffer), SocketFlags.None).ConfigureAwait(false);
                    if (received == 0) { failure = "Game server disconnected."; break; }
                    pending.AddRange(buffer.Take(received));
                    lock (_gate)
                    {
                        if (_socket != socket) return;
                        while (pending.Count >= 8)
                        {
                            var header = Packet.ExtractHeader(pending.ToArray());
                            // Bound frames to avoid zero-progress loops and unbounded buffering.
                            if (header.magic != 0xABCD || header.length < 8 || header.length > 1024 * 1024)
                                throw new InvalidDataException();
                            if (header.opcode == 2 && (header.length != 9 || header.flags != 0) ||
                                header.opcode == 14 && (header.length < 22 || header.flags != 0) ||
                                header.opcode == 15 && (header.length < 173 || header.flags != 0) ||
                                header.opcode == 25 && (header.length < 10 || header.length > 2010 || header.flags != 0) ||
                                header.opcode == 4 && (header.length != 430 || header.flags != 0) ||
                                header.opcode == 6 && (header.length != 76 || header.flags != 0) ||
                                header.opcode == 17 && (header.length != 39 || header.flags != 1) ||
                                (header.opcode == 7 || header.opcode == 8 || header.opcode == 9 ||
                                 header.opcode == 10 || header.opcode == 12) &&
                                (header.length < 20 || header.flags != 1))
                                throw new InvalidDataException();
                            if (pending.Count < header.length) break;
                            byte[] packet = pending.GetRange(0, (int)header.length).ToArray();
                            if (header.opcode == 4 && packet[9] > 10) throw new InvalidDataException();
                            pending.RemoveRange(0, packet.Length);
                            _handler.HandleResponse(packet);
                            if (_socket != socket) return;
                        }
                    }
                }
            }
            catch (InvalidDataException) { failure = "Invalid game server packet."; }
            catch (SocketException) { failure = "Game server receive failed."; }
            catch (ObjectDisposedException) { failure = "Game server connection closed."; }
            catch (Exception) { failure = "Game server response failed."; }
            finally
            {
                lock (_gate)
                {
                    if (_socket == socket)
                    {
                        _socket = null;
                        socket.Dispose();
                        if (!_disposed && failure != null) OnDisconnected?.Invoke(failure);
                    }
                }
            }
        }
        public void Send(byte[] binary)
        {
            lock (_gate)
            {
                if (_socket == null) throw new InvalidOperationException("Game server is not connected.");
                // Serialize complete frames, including partial Socket.Send results.
                int offset = 0;
                while (offset < binary.Length)
                {
                    int sent = _socket.Send(binary, offset, binary.Length - offset, SocketFlags.None);
                    if (sent == 0) throw new IOException("Game server send failed.");
                    offset += sent;
                }
            }
        }
        public void Dispose()
        {
            lock (_gate)
            {
                _disposed = true;
                _socket?.Dispose();
                _socket = null;
            }
        }
    }
}
