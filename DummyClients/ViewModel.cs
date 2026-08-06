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
                // 이동 시뮬레이션 시드 — 서버가 준 시작 좌표에서 출발해야 검증을 통과한다.
                Program.SetSessionPosition(index, startX, startY);
                _network.ZoneChange(Program.GetSocket(index), (byte)ZONE_CHANGE.ENTER);
            }
            else
            {
                Console.WriteLine("Enter Failed index: " + index);
            }

        }
        public void ZoneChageReceived(int index, byte resStatus, float startX, float startY)
        {
            bool success = resStatus != 0;
            if (success)
            {
                // 존 진입 후 좌표로 재시드. 이게 서버가 들고 있는 값이므로
                // 여기서 맞추지 않으면 첫 이동 패킷이 큰 점프가 되어 이동 예산 검증에 걸린다.
                Program.SetSessionPosition(index, startX, startY);
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