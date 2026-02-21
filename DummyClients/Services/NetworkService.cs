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

        public void CharacterList(TCPSocket sock)
        {
            try
            {
                sock.Send(PacketBuilder.CreateCharacterListPacket());
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public void Enter(TCPSocket sock, ulong charID)
        {
            try
            {
                sock.Send(PacketBuilder.CreateEnterWorldPacket(charID));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public bool Chat(TCPSocket sock,string message, byte scope, ulong targetID)
        {
            try
            {
                sock.Send(PacketBuilder.CreateChatPacket(message, scope, targetID));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                return false;
            }
            return true;
        }
        public bool Action(TCPSocket sock, byte dir, float speed, byte skillSlot)
        {
            try
            {
                sock.Send(PacketBuilder.CreateActionPacket(dir, speed, skillSlot));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                return false;
            }
            return true;
        }

        public void ZoneChange(TCPSocket sock, byte op)
        {
            try
            {
                sock.Send(PacketBuilder.CreateZoneChangePacket(op));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public void Pong(TCPSocket sock, ulong serverTimeMs)
        {
            try
            {
                sock.Send(PacketBuilder.CreatePongPacket(serverTimeMs));
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }
    }
}
