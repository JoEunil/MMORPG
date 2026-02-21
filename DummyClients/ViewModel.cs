using ClientCore.Network;
using ClientCore.PacketHelper;
using ClientCore.Services;
using System;
using System.Drawing;
using System.Net.Http;
using System.Threading.Tasks;
using System.Xml.Linq;


namespace ClientCore
{
    public class ViewModel
    {
        private NetworkService _network;
        internal ViewModel(NetworkService network)
        {
            _network = network;
        }
        public void AuthReceived(int index, byte resStatus)
        {
            bool success = resStatus != 0;
            if (success)
                _network.CharacterList(Program.GetSocket(index));
            else
                Console.WriteLine("Auth request failed index: " + index);
        }

        public void ChatacterListReceived(int index, byte resStatus, ushort count, PacketHelper.CharacterInfo[] characters)
        {
            bool success = resStatus != 0;
            if (success)
            {
                _network.Enter(Program.GetSocket(index), characters[0].characterID);
            } 
            else
            {
                Console.WriteLine("CharacterList recv failed index: " + index);
            }
        }

        public void EnterReceived(int index, byte resStatus, byte[] name, ushort attack, ushort level, uint exp, int hp, int mp, int maxHP, int maxMP, byte dir, float startX, float startY, ushort currentZone)
        {
            bool success = resStatus != 0;
            if (success)
            {
                _network.ZoneChange(Program.GetSocket(index), (byte)ZONE_CHANGE.ENTER);
            }
            else
            {
                Console.WriteLine("Enter Failed index: " + index);
            }

        }
        public void ZoneChageReceived(int index, byte resStatus)
        {
            bool success = resStatus != 0;
            if (success)
            {
                Program.Ready();
            }
            else
            {
                Console.WriteLine("Zone Change Failed index: " + index);
            }
        }

        public void PingReceived(int index, ulong servertimeMs, ulong rtt)
        {
            _network.Pong(Program.GetSocket(index), servertimeMs);
        }
    }
}