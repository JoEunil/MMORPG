using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

using static ClientCore.Config;
using static ClientCore.PacketHelper.Packet;

namespace ClientCore.Network
{
    internal class Handler
    {
        private ViewModel _viewModel;
        public void Initialize(ViewModel viewModel) 
        {
            _viewModel = viewModel;
        }
        public void HandleResponse(int index, byte[] buffer)
        {
            try
            {
                var header = Packet.ExtractHeader(buffer);
                if ((header.flags & FLAG_SIMULATION) == FLAG_SIMULATION)
                {
                    if ((OP_CODE)header.opcode == OP_CODE.ZONE_CHANGE_RESPONSE)
                        HandleZoneChange(index, Packet.Deserialize<ZoneChangeResponseBody>(header, buffer));
                    return;
                }
                switch ((OP_CODE)header.opcode)
                {
                    case OP_CODE.AUTH_RESPONSE:
                        HandleAuthRes(index, Packet.Deserialize<AuthResponseBody>(header, buffer));
                        break;
                    case OP_CODE.CHARACTER_LIST_RESPONSE:
                        HandleCharacterList(index, Packet.Deserialize<CharacterListResponseBody>(header, buffer));
                        break;
                    case OP_CODE.ENTER_WORLD_RESPONSE:
                        HandleEnterRes(index, Packet.Deserialize<EnterWorldResponseBody>(header, buffer));
                        break;
                    
                    case OP_CODE.PING:
                        HandlePing(index, Packet.Deserialize<Ping>(header, buffer));
                        break;
                    default:
                        break;
                }
            }
            catch (Exception e)
            {
            }
        }
        private void HandleAuthRes(int index, STPacket<AuthResponseBody> packet)
        {
            _viewModel.AuthReceived(index, packet.body.resStatus);
        }
        private void HandleCharacterList(int index, STPacket<CharacterListResponseBody> packet)
        {
            _viewModel.ChatacterListReceived(index, packet.body.resStatus, packet.body.count, packet.body.characters);
        }
        private void HandleEnterRes(int index, STPacket<EnterWorldResponseBody> packet)
        {
            _viewModel.EnterReceived(index, packet.body.resStatus, packet.body.name, packet.body.attack, packet.body.level, packet.body.exp, packet.body.hp, packet.body.mp, packet.body.maxHp, packet.body.maxMp, packet.body.dir, packet.body.startX, packet.body.startY, packet.body.currentZone);
        }
        private void HandleZoneChange(int index, STPacket<ZoneChangeResponseBody> packet)
        {
            _viewModel.ZoneChageReceived(index, packet.body.resStatus);
        }
        public void HandlePing(int index, STPacket<Ping> packet)
        {
            _viewModel.PingReceived(index, packet.body.serverTimeMs, packet.body.rtt);
        }

    }

}
