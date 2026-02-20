using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static ClientCore.Config;
namespace ClientCore.PacketHelper
{
    internal class PacketBuilder
    {
        internal static byte[] CreateAuthPacket(ClientSession session)
        {
            var sessionBody = new AuthRequestBody
            {
                userID = session.UserID,
                sessionToken = new byte[37]
            };
            var tokenBytes = EncodingHelper.EncodeUtf8(session.SessionToken);
            if (tokenBytes.Length > sessionBody.sessionToken.Length)
                throw new ArgumentException($"sessionToken is too long (max {sessionBody.sessionToken.Length} bytes)");
            Array.Copy(tokenBytes, sessionBody.sessionToken, tokenBytes.Length);

            var header = new PacketHeader
            {
                magic = 0xABCD,
                length = 0,  // Serialize에서 계산
                opcode = (byte)OP_CODE.AUTH,
                flags = 0x00,
            };

            var packet = new STPacket<AuthRequestBody>
            {
                header = header,
                body = sessionBody
            };

            return Packet.Serialize(packet);
        }
        internal static byte[] CreateChatPacket(string message, byte scope, ulong targetID)
        {
            var header = new PacketHeader {
                magic = 0xABCD,
                length = 0,  // Serialize에서 계산
                opcode = (byte)OP_CODE.CHAT,
                flags = 0x00, 
            };
            var body = new ChatRequestBody();
            return Packet.SerializeChatPacket(header, body, message, scope, targetID);
        }


        internal static byte[] CreateZoneChangePacket(byte op)
        {
            var body = new ZoneChangeBody { op = op };
            var header = new PacketHeader
            {
                magic = 0xABCD,
                length = 0,
                opcode = (byte)OP_CODE.ZONE_CHANGE,
                flags = 0x00
            };

            var packet = new STPacket<ZoneChangeBody>
            {
                header = header,
                body = body
            };

            return Packet.Serialize(packet);
        }

        internal static byte[] CreateActionPacket(byte dir, float speed, byte skillSlot)
        {
            var body = new ActionRequestBody
            {
                dir = dir,
                speed = speed,
                skillSlot = skillSlot
            };

            var header = new PacketHeader
            {
                magic = 0xABCD,
                length = 0,
                opcode = (byte)OP_CODE.ACTION,
                flags = 0x00,
            };
            header.flags |= FLAG_SIMULATION;

            var packet = new STPacket<ActionRequestBody>
            {
                header = header,
                body = body
            };

            return Packet.Serialize(packet);
        }

        internal static byte[] CreateCharacterListPacket()
        {
            var header = new PacketHeader
            {
                magic = 0xABCD,
                length = 0,
                opcode = (byte)OP_CODE.CHARACTER_LIST,
                flags = 0x00,
            };

            return Packet.SerializeHeaderOnly(header);
        }

        internal static byte[] CreateEnterWorldPacket(ulong charID)
        {
            var body = new EnterWorldRequestBody
            {
                characterID = charID
            };

            var header = new PacketHeader
            {
                magic = 0xABCD,
                length = 0,
                opcode = (byte)OP_CODE.ENTER_WORLD,
                flags = 0x00,
            };

            var packet = new STPacket<EnterWorldRequestBody>
            {
                header = header,
                body = body
            };

            return Packet.Serialize(packet);
        }

        internal static byte[] CreateInventoryRequestPacket()
        {
            var header = new PacketHeader
            {
                magic = 0xABCD,
                length = 0,
                opcode = (byte)OP_CODE.INVENTORY_REQ,
                flags = 0x00,
            };
            return Packet.SerializeHeaderOnly(header);
        }
        internal static byte[] CreatePongPacket(ulong serverTimeMs)
        {
            var header = new PacketHeader
            {
                magic = 0xABCD,
                length = 0,
                opcode = (byte)OP_CODE.PONG,
                flags = 0x00,
            };

            var body = new Pong
            {
                serverTimeMs = serverTimeMs
            };

            var packet = new STPacket<Pong>
            {
                header = header,
                body = body
            };

            return Packet.Serialize(packet);
        }
    }
}
