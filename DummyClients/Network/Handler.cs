using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using System;

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
        public void HandleResponse(int index, ClientSession session, byte[] buffer)
        {
            try
            {
                var header = Packet.ExtractHeader(buffer);
                if ((header.flags & FLAG_SIMULATION) == FLAG_SIMULATION)
                {
                    switch ((OP_CODE)header.opcode)
                    {
                        case OP_CODE.ZONE_CHANGE_RESPONSE:
                            HandleZoneChange(session, header, buffer);
                            break;
                        case OP_CODE.ZONE_FULL_STATE_BROADCAST:
                            HandleFull(index, session, header, buffer, false);
                            break;
                        case OP_CODE.MONSTER_FULL_STATE_BROADCAST:
                            HandleFull(index, session, header, buffer, true);
                            break;
                        case OP_CODE.ZONE_DELTA_UPDATE_BROADCAST:
                            HandleDelta(session, header, buffer);
                            break;
                    }
                    return;
                }
                switch ((OP_CODE)header.opcode)
                {
                    case OP_CODE.AUTH_RESPONSE:
                        HandleAuthRes(index, session, Packet.Deserialize<AuthResponseBody>(header, buffer));
                        break;
                    case OP_CODE.CHARACTER_LIST_RESPONSE:
                        HandleCharacterList(index, session, Packet.Deserialize<CharacterListResponseBody>(header, buffer));
                        break;
                    case OP_CODE.ENTER_WORLD_RESPONSE:
                        HandleEnterRes(index, session, Packet.Deserialize<EnterWorldResponseBody>(header, buffer));
                        break;

                    case OP_CODE.PING:
                        HandlePing(index, session, Packet.Deserialize<Ping>(header, buffer));
                        break;
                    default:
                        break;
                }
            }
            catch (Exception e)
            {
                Console.WriteLine($"[{index}] handler ex: {e.Message}");
            }
        }

        // byte[] 할당 없이 건너뛸 수 있도록, 실제 처리하는 opcode만 true를 반환한다.
        public static bool ShouldHandle(byte opcode, byte flags)
        {
            if ((flags & FLAG_SIMULATION) == FLAG_SIMULATION)
                return (OP_CODE)opcode == OP_CODE.ZONE_CHANGE_RESPONSE ||
                    (OP_CODE)opcode == OP_CODE.ZONE_FULL_STATE_BROADCAST ||
                    (OP_CODE)opcode == OP_CODE.ZONE_DELTA_UPDATE_BROADCAST ||
                    (OP_CODE)opcode == OP_CODE.MONSTER_FULL_STATE_BROADCAST;

            switch ((OP_CODE)opcode)
            {
                case OP_CODE.AUTH_RESPONSE:
                case OP_CODE.CHARACTER_LIST_RESPONSE:
                case OP_CODE.ENTER_WORLD_RESPONSE:
                case OP_CODE.PING:
                    return true;
                default:
                    return false;
            }
        }
        private void HandleAuthRes(int index, ClientSession session, STPacket<AuthResponseBody> packet)
        {
            _viewModel.AuthReceived(index, session, packet.body.resStatus);
        }
        private void HandleCharacterList(int index, ClientSession session, STPacket<CharacterListResponseBody> packet)
        {
            _viewModel.ChatacterListReceived(index, session, packet.body.resStatus, packet.body.count, packet.body.characters);
        }
        private void HandleEnterRes(int index, ClientSession session, STPacket<EnterWorldResponseBody> packet)
        {
            _viewModel.EnterReceived(index, session, packet.body.resStatus, packet.body.name, packet.body.attack, packet.body.level, packet.body.exp, packet.body.hp, packet.body.mp, packet.body.maxHp, packet.body.maxMp, packet.body.dir, packet.body.startX, packet.body.startY, packet.body.currentZone);
        }
        private void HandleZoneChange(ClientSession session, PacketHeader header, byte[] buffer)
        {
            STPacket<ZoneChangeResponseBody> packet;
            try { packet = Packet.DeserializeZoneChange(header, buffer); }
            catch (ArgumentException) { return; }
            var body = packet.body;
            if (body.resStatus == 1)
                session.SetZoneOwnership(body.zoneID, body.zoneGeneration, body.ownershipEpoch,
                    body.zoneInternalID, body.startX, body.startY);
        }

        private static void HandleFull(int index, ClientSession session, PacketHeader header, byte[] buffer, bool monsters)
        {
            const int packetHeaderSize = 8;
            const int snapshotPrefixSize = 29;
            if (header.flags != FLAG_SIMULATION || header.length != buffer.Length || header.length < packetHeaderSize + snapshotPrefixSize) return;
            ushort count = BitConverter.ToUInt16(buffer, packetHeaderSize);
            ushort zoneID = BitConverter.ToUInt16(buffer, packetHeaderSize + 2);
            ulong generation = BitConverter.ToUInt64(buffer, packetHeaderSize + 4);
            ulong epoch = BitConverter.ToUInt64(buffer, packetHeaderSize + 12);
            ulong bootstrapID = BitConverter.ToUInt64(buffer, packetHeaderSize + 20);
            byte bootstrapEnd = buffer[packetHeaderSize + 28];
            int fieldSize = monsters ? System.Runtime.InteropServices.Marshal.SizeOf<MonsterFullField>() :
                System.Runtime.InteropServices.Marshal.SizeOf<FullStateField>();
            int maxCount = monsters ? MAX_MONSTER_COUNT : MAX_ZONE_CAPACITY;
            if (count > maxCount || zoneID < 1 || zoneID > 6 || generation == 0 || bootstrapEnd > 1 ||
                ((bootstrapID == 0) != (epoch == 0)) ||
                (long)packetHeaderSize + snapshotPrefixSize + (long)count * fieldSize != buffer.Length) return;

            if (monsters)
            {
                session.ObserveMonsterFull(zoneID, generation, epoch, bootstrapID, bootstrapEnd != 0);
                Program.CountReady(index, session);
                return;
            }
            bool ownFound = false;
            float ownX = 0, ownY = 0;
            int offset = packetHeaderSize + snapshotPrefixSize;
            for (int i = 0; i < count; i++, offset += fieldSize)
            {
                var state = Packet.ReadStruct<FullStateField>(buffer, offset);
                if (session.OwnZoneInternalID == state.zoneInternalID)
                {
                    ownFound = true;
                    ownX = state.x;
                    ownY = state.y;
                }
            }
            session.ObservePlayerFull(zoneID, generation, epoch, bootstrapID, bootstrapEnd != 0, ownFound, ownX, ownY);
            Program.CountReady(index, session);
        }

        private static void HandleDelta(ClientSession session, PacketHeader header, byte[] buffer)
        {
            const int packetHeaderSize = 8;
            const int snapshotPrefixSize = 12;
            int fieldSize = System.Runtime.InteropServices.Marshal.SizeOf<DeltaUpdateField>();
            if (header.flags != FLAG_SIMULATION || header.length != buffer.Length || header.length < packetHeaderSize + snapshotPrefixSize) return;
            ushort count = BitConverter.ToUInt16(buffer, packetHeaderSize);
            ushort zoneID = BitConverter.ToUInt16(buffer, packetHeaderSize + 2);
            ulong generation = BitConverter.ToUInt64(buffer, packetHeaderSize + 4);
            if (count > DELTA_UPDATE_COUNT || zoneID < 1 || zoneID > 6 || generation == 0 ||
                (long)packetHeaderSize + snapshotPrefixSize + (long)count * fieldSize != buffer.Length) return;
            uint ownID = session.OwnZoneInternalID;
            float? ownX = null, ownY = null;
            int offset = packetHeaderSize + snapshotPrefixSize;
            for (int i = 0; i < count; i++, offset += fieldSize)
            {
                var field = Packet.ReadStruct<DeltaUpdateField>(buffer, offset);
                if (field.zoneInternalID != ownID) continue;
                if (field.fieldID == 7) ownX = BitConverter.Int32BitsToSingle(unchecked((int)field.fieldVal));
                else if (field.fieldID == 8) ownY = BitConverter.Int32BitsToSingle(unchecked((int)field.fieldVal));
            }
            session.ObserveDelta(zoneID, generation, ownID, ownX, ownY);
        }
        public void HandlePing(int index, ClientSession session, STPacket<Ping> packet)
        {
            _viewModel.PingReceived(index, session, packet.body.serverTimeMs, packet.body.rtt);
        }

    }

}
