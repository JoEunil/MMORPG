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
    internal class ViewModel
    {
        private NetworkService _network;
        internal ViewModel(NetworkService network)
        {
            _network = network;
        }
        public void AuthReceived(int index, ClientSession session, byte resStatus)
        {
            bool success = resStatus != 0;
            if (success)
                _network.CharacterList(session.GetSocket());
            else
                Console.WriteLine("Auth request failed index: " + index);
        }

        public void ChatacterListReceived(int index, ClientSession session, byte resStatus, ushort count, PacketHelper.CharacterInfo[] characters)
        {
            bool success = resStatus != 0;
            if (success)
            {
                _network.Enter(session.GetSocket(), characters[0].characterID);
            } 
            else
            {
                Console.WriteLine("CharacterList recv failed index: " + index);
            }
        }

        public void EnterReceived(int index, ClientSession session, byte resStatus, byte[] name, ushort attack, ushort level, uint exp, int hp, int mp, int maxHP, int maxMP, byte dir, float startX, float startY, ushort currentZone)
        {
            bool success = resStatus != 0;
            if (success)
            {
                // 이동 시뮬레이션 시드 — 서버가 준 시작 좌표에서 출발해야 검증을 통과한다.
                session.SetPosition(startX, startY);
                _network.ZoneChange(session.GetSocket(), (byte)ZONE_CHANGE.ENTER);
            }
            else
            {
                Console.WriteLine("Enter Failed index: " + index);
            }

        }
        public void PingReceived(int index, ClientSession session, ulong servertimeMs, ulong rtt)
        {
            _network.Pong(session.GetSocket(), servertimeMs);
        }
    }
}
