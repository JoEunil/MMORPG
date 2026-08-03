using ClientCore.Network;
using ClientCore.PacketHelper;
using System;
using System.Collections.Generic;
using System.Data;
using System.Text;
using System.Threading.Tasks;

namespace ClientCore.Services
{
    internal class NetworkService
    {
        public async Task Connect(ClientSession session, string address, int port)
        {
            try
            {
                await session.GetSocket().Connect(address, port);
                session.GetSocket().Send(PacketBuilder.CreateAuthPacket(session));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public async void CharacterList(TCPSocket? sock)
        {
            if (sock == null) return;
            try
            {
                await sock.Send(PacketBuilder.CreateCharacterListPacket());
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public async void Enter(TCPSocket? sock, ulong charID)
        {
            if (sock == null) return;
            try
            {
                await sock.Send(PacketBuilder.CreateEnterWorldPacket(charID));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public async Task<bool> Chat(TCPSocket sock,string message, byte scope, ulong targetID)
        {
            try
            {
                await sock.Send(PacketBuilder.CreateChatPacket(message, scope, targetID));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                return false;
            }
            return true;
        }
        public async Task<bool> Action(TCPSocket sock, byte dir, float speed, byte skillSlot)
        {
            try
            {
                await sock.Send(PacketBuilder.CreateActionPacket(dir, speed, skillSlot));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                return false;
            }
            return true;
        }

        // 틱당 한 번만 빌드해서 모든 세션이 공유 전송(send마다 마샬링/할당 반복 제거).
        public byte[] BuildActionPacket(byte dir, float speed, byte skillSlot)
            => PacketBuilder.CreateActionPacket(dir, speed, skillSlot);

        public async Task<bool> SendPacket(TCPSocket sock, byte[] packet)
        {
            try
            {
                await sock.Send(packet);
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                return false;
            }
            return true;
        }

        public async void ZoneChange(TCPSocket? sock, byte op)
        {
            if (sock == null) return;
            try
            {
                await sock.Send(PacketBuilder.CreateZoneChangePacket(op));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public async void Pong(TCPSocket? sock, ulong serverTimeMs)
        {
            if (sock == null) return;
            try
            {
                await sock.Send(PacketBuilder.CreatePongPacket(serverTimeMs));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }
    }
}
