using ClientCore.Network;
using ClientCore.PacketHelper;
using System;
using System.Collections.Generic;
using System.Data;
using System.Text;
using System.Threading.Tasks;

namespace ClientCore.Services
{
    internal class NetworkService : INetworkService
    {
        private ITCPSocket _sock;
        private ILogger _logger;
        internal NetworkService(ITCPSocket sock, ILogger logger)
        {
            _sock = sock;
            _logger = logger;
        }

        public async Task Connect(string address, int port)
        {
            try
            {
                await _sock.Connect(address, port);
                _sock.Send(PacketBuilder.CreateAuthPacket());
            }
            catch (Exception ex)
            {
                Log(ex.Message);
            }
        }

        public void CharacterList()
        {
            try
            {
                _sock.Send(PacketBuilder.CreateCharacterListPacket());
            }
            catch (Exception ex)
            {
                Log(ex.Message);
            }
        }

        public void Enter(ulong charID)
        {
            try
            {
                _sock.Send(PacketBuilder.CreateEnterWorldPacket(charID));
            }
            catch (Exception ex)
            {
                Log(ex.Message);
            }
        }

        public void Chat(string message, byte scope, ulong targetID)
        {
            try
            {
                _sock.Send(PacketBuilder.CreateChatPacket(message, scope, targetID));
            }
            catch (Exception ex)
            {
                Log(ex.Message);
            }
        }
        public void Move(byte dir, float speed)
        {
            try
            {
                _sock.Send(PacketBuilder.CreateActionPacket(dir, speed));
            }
            catch (Exception ex)
            {
                Log(ex.Message);
            }
        }

        public void ZoneChange(byte op)
        {
            try
            {
                _sock.Send(PacketBuilder.CreateZoneChangePacket(op));
            }
            catch (Exception ex)
            {
                Log(ex.Message);
            }
        }

        public void Pong(ulong serverTimeMs)
        {
            try
            {
                _sock.Send(PacketBuilder.CreatePongPacket(serverTimeMs));
            }
            catch (Exception ex)
            {
                Log(ex.Message);
            }
        }
        public void Log(string msg)
        {
            _logger.Log(msg);
        }
    }
}
