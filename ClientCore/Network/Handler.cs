using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Reflection;
using System.Reflection.Emit;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

using static ClientCore.Config;
using static ClientCore.PacketHelper.Packet;

namespace ClientCore.Network
{
    internal class Handler : IHandlerSock, IHandlerTick
    {
        private IViewModelNet _viewModel;
        private IReceiveTick _recvTick;
        internal void Inject(IViewModelNet vm, IReceiveTick t)
        {
            _viewModel = vm;
            _recvTick = t;
        }
        public void HandleResponse(byte[] buffer)
        {
            try
            {
                var header = Packet.ExtractHeader(buffer);
                if ((header.flags & FLAG_SIMULATION) == FLAG_SIMULATION)
                {
                    _recvTick.Enqueue(buffer);
                    return;
                }
                switch ((OP_CODE)header.opcode)
                {
                    case OP_CODE.CHAT_WHISPER:
                        HandleChatWhisper(Packet.DeserializeChatWhisper(header, buffer));
                        break;
                    case OP_CODE.CHAT_BROADCAST:
                        HandleChatBatch(Packet.DeserializeChatBatch(header, buffer));
                        break;
                    case OP_CODE.AUTH_RESPONSE:
                        HandleAuthRes(Packet.Deserialize<AuthResponseBody>(header, buffer));
                        break;
                    case OP_CODE.CHARACTER_LIST_RESPONSE:
                        HandleCharacterList(Packet.Deserialize<CharacterListResponseBody>(header, buffer));
                        break;
                    case OP_CODE.ENTER_WORLD_RESPONSE:
                        HandleEnterRes(Packet.Deserialize<EnterWorldResponseBody>(header, buffer));
                        break;
                    case OP_CODE.INVENTORY_RES:
                        HandleInventoryRes(Packet.Deserialize<InventoryResBody>(header, buffer));
                        break;
                    case OP_CODE.PING:
                        HandlePing(Packet.Deserialize<Ping>(header, buffer));
                        break;
                    default:
                        ErrorLog("Invalid opcode" + header.opcode + header.flags);
                        break;
                }
            }
            catch (Exception e)
            {
                ErrorLog(e.Message);
            }
        }
        private void HandleChatWhisper(Message message)
        {
            _viewModel.ChatReceived(message);
        }
        private void HandleChatBatch(Message[] messages)
        {
            foreach (Message message in messages)
            {
                _viewModel.ChatReceived(message);
            }
        }
        private void HandleAuthRes(STPacket<AuthResponseBody> packet)
        {
            _viewModel.AuthReceived(packet.body.resStatus);
        }
        private void HandleCharacterList(STPacket<CharacterListResponseBody> packet)
        {
            _viewModel.ChatacterListReceived(packet.body.resStatus, packet.body.count, packet.body.characters);
        }
        private void HandleEnterRes(STPacket<EnterWorldResponseBody> packet)
        {
            _viewModel.EnterReceived(packet.body.resStatus, packet.body.name, packet.body.attack, packet.body.level, packet.body.exp, packet.body.hp, packet.body.mp, packet.body.maxHp, packet.body.maxMp, packet.body.dir, packet.body.startX, packet.body.startY, packet.body.currentZone);
        }
        private void HandleInventoryRes(STPacket<InventoryResBody> packet)
        {
            _viewModel.InventoryReceived(packet.body.resStatus, packet.body.items);
        }
        public void HandleZoneChangeRes(STPacket<ZoneChangeResponseBody> packet)
        {
            _viewModel.ZoneChageReceived(packet.body.resStatus, packet.body.zoneID, packet.body.chatID, packet.body.zoneInternalID, packet.body.startX, packet.body.startY);
        }
        public void HandleDeltaUpdate((ushort, DeltaUpdateField[]) res)
        {
            _viewModel.DeltaReceived(res.Item1, res.Item2);
        }
        public void HandleFullState((ushort, FullStateField[]) res)
        {
            _viewModel.FullReceived(res.Item1, res.Item2);
        } 
        public void HandleMonsterDelta((ushort, MonsterDeltaField[]) res)
        {
            _viewModel.MonsterDeltaReceived(res.Item1, res.Item2);
        }
        public void HandleMonsterFull((ushort, MonsterFullField[]) res)
        {
            _viewModel.MonsterFullReceived(res.Item1, res.Item2);
        }
        public void HandleActionReult((ushort, ActionResultField[]) res)
        {
            _viewModel.ActionResultReceived(res.Item1, res.Item2);
        }
        public void HandlePing(STPacket<Ping> packet)
        {
            _viewModel.PingReceived(packet.body.serverTimeMs, packet.body.rtt);
        }
        private void WriteLog(string msg)
        {
            _viewModel.Log(msg);
        }

        private void ErrorLog(string msg)
        {
            _viewModel.Error(msg);
        }

    }

}
