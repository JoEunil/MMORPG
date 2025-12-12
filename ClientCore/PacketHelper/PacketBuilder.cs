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
        internal static byte[] CreateAuthPacket()
        {
            var sessionBody = new AuthRequestBody
            {
                userID = ClientSession.UserID,
                sessionToken = new byte[37]
            };
            var tokenBytes = EncodingHelper.EncodeUtf8(ClientSession.SessionToken);
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
        internal static byte[] CreateChatPacket(string message)
        {
            var header = new PacketHeader {
                magic = 0xABCD,
                length = 0,  // Serialize에서 계산
                opcode = (byte)OP_CODE.CHAT,
                flags = 0x00, 
            };
            var body = new ChatRequestBody();
            return Packet.SerializeChatPacket(header, body, message);
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

        internal static byte[] CreateActionPacket(byte dir, float speed)
        {
            var body = new ActionRequestBody
            {
                dir = dir,
                speed = speed
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
        internal static byte[] CreatePongPacket()
        {
            var header = new PacketHeader
            {
                magic = 0xABCD,
                length = 0,
                opcode = (byte)OP_CODE.PONG,
                flags = 0x00,
            };
            return Packet.SerializeHeaderOnly(header);
        }
    }
}
