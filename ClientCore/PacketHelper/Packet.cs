using ClientCore.PacketHelper;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using static ClientCore.Config;
using static ClientCore.PacketHelper.Packet;

namespace ClientCore.PacketHelper
{
    internal enum OP_CODE
    {
        AUTH = 1,
        AUTH_RESPONSE = 2,
        CHARACTER_LIST = 3,
        CHARACTER_LIST_RESPONSE = 4,
        ENTER_WORLD = 5,
        ENTER_WORLD_RESPONSE = 6,

        ZONE_FULL_STATE_BROADCAST = 7,
        ZONE_DELTA_UPDATE_BROADCAST = 8,
        MONSTER_FULL_STATE_BROADCAST = 9,
        MONSTER_DELTA_UPDATE_BROADCAST = 10,

        ACTION = 11,                 // 이동 + 공격 + 스킬 사용 등 행동 패킷
        ACTION_RESULT = 12,
        CHAT = 13,
        CHAT_WHISPER = 14,
        CHAT_BROADCAST = 15,

        ZONE_CHANGE = 16,
        ZONE_CHANGE_RESPONSE = 17,

        INVENTORY_UPDATE = 18,
        INVENTORY_UPDATE_RES = 19,
        INVENTORY_REQ = 20,
        INVENTORY_RES = 21,
        PING = 22, // 서버 송신
        PONG = 23, // 클라이언트 송신
    }
    
    public enum ZONE_CHANGE : byte
    {
        ENTER = 0,
        UP = 1,
        DOWN = 2,
        LEFT = 3,
        RIGHT = 4
    }
    public enum CHAT_SCOPE : byte
    {
        Global = 0,
        Zone = 1,
        Whisper = 2,
    };

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketHeader
    {
        public ushort magic;
        public ushort length;
        public byte opcode;
        public byte flags;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct AuthRequestBody
    {
        public ulong userID;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 37)]
        public byte[] sessionToken;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct AuthResponseBody
    {
        public byte resStatus;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct CharacterInfo
    {
        public ulong characterID;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_CHARNAME_LEN)]
        public byte[] name;

        public ushort level;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct CharacterListResponseBody
    {
        public byte resStatus;
        public byte count;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_CHARACTER)]
        public CharacterInfo[] characters;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct EnterWorldRequestBody
    {
        public ulong characterID;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct EnterWorldResponseBody
    {
        public byte resStatus;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_CHARNAME_LEN)]
        public byte[] name;

        public ushort attack;
        public ushort level;
        public uint exp;
        public int hp;
        public int mp;
        public int maxHp;
        public int maxMp;
        public byte dir;
        public float startX;
        public float startY;
        public ushort currentZone;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct InventoryUpdateBody
    {
        public uint itemID;
        public byte op;
        public short change;
        public byte slot;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct InventoryItem
    {
        public uint itemID;
        public ushort quantity;
        public byte slot;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct InventoryResBody
    {
        public byte resStatus;
        public ushort itemCount;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_INVENTORY_ITEMS)]
        public InventoryItem[] items;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ZoneChangeBody
    {
        public byte op;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ZoneChangeResponseBody
    {
        public byte resStatus;
        public ushort zoneID;
        public ulong chatID;
        public ulong zoneInternalID;
        public float startX;
        public float startY;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ChatRequestBody
    {
        public CHAT_SCOPE scope;
        public ushort messageLength;
        public ulong targetChatID;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ChatWhisperBody
    {
        public short messageLen;
        public ulong senderChatID;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_CHARNAME_LEN)]
        public byte[] name;
    };// + raw char*

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ChatEntity
    {
        public ulong senderChatID; // sender
        public ushort offset;
        public ushort messageLength;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_CHARNAME_LEN)]
        public byte[] name;
    };

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ChatBatchNotifyBody
    {
        public ushort chatCnt;
        public ushort totalMessageLength;
        public CHAT_SCOPE scope;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_CHAT_PACKET)]
        public ChatEntity[] entities;
    };


    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ActionRequestBody
    {
        public byte dir;
        public float speed;
        public byte skillSlot;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ActionResultField
    {
        public byte casterType;
        public ulong casterId;
        public byte skillSlot;
        public byte dir;
        public float x, y;
        public uint skillId;
        public ushort skillPhase;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ActionResultBody
    {
        public ushort count;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = ACTION_RESULT_COUNT)]
        public ActionResultField[] actions;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct DeltaUpdateField
    {
        public ulong zoneInternalID;
        public ushort fieldID;
        public uint fieldVal;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct DeltaSnapshotBody
    {
        public ushort count;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = DELTA_UPDATE_COUNT)]
        public DeltaUpdateField[] updates;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct FullStateField
    {
        public ulong zoneInternalID;
        public int hp;
        public int mp;
        public int maxHp;
        public int maxMp;
        public uint exp;
        public ushort level;
        public byte dir;
        public float x;
        public float y;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_CHARNAME_LEN)]
        public byte[] charName;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct FullSnapshotBody
    {
        public ushort count;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_ZONE_CAPACITY)]
        public FullStateField[] states;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct MonsterDeltaField
    {
        public ushort internalId;
        public ushort fieldId;
        public uint fieldVal;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct MonsterDeltaSnapshotBody
    {
        public ushort count;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_MONSTER_DELTA)]
        public MonsterDeltaField[] updates;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct MonsterFullField
    {
        public ushort internalId;
        public int hp;
        public int maxHp;
        public uint attacked;
        public float x;
        public float y;
        public byte dir;
        public ushort monsterId;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct MonsterFullSnapshotBody
    {
        public ushort count;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MAX_MONSTER_COUNT)]
        public MonsterFullField[] states;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Ping
    {
        public ulong serverTimeMs;
        public ulong rtt;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Pong
    {
        public ulong serverTimeMs;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct STPacket<T> where T : struct
    {
        public PacketHeader header;
        public T body;
    }
    public class Packet
    {
        
        private static T ByteArrayToStructure<T>(byte[] bytes) where T : struct
        {
            GCHandle handle = GCHandle.Alloc(bytes, GCHandleType.Pinned);
            // 객체가 GarbageCollector에 의해 이동되지 않도록 고정 -> 포인터 처럼 사용 가능해짐
            try
            {
                return Marshal.PtrToStructure<T>(handle.AddrOfPinnedObject());
            }
            finally
            {
                handle.Free();
            }
        }
        internal static byte[] Serialize<T>(STPacket<T> packet) where T : struct
        {
            int headerSize = Marshal.SizeOf<PacketHeader>();
            int bodySize = Marshal.SizeOf<T>();
            int totalSize = headerSize + bodySize;

            byte[] arr = new byte[totalSize];
            packet.header.length = (ushort)totalSize;

            IntPtr ptr = Marshal.AllocHGlobal(totalSize);

            try
            {
                Marshal.StructureToPtr(packet.header, ptr, false);
                Marshal.Copy(ptr, arr, 0, headerSize);

                IntPtr bodyPtr = IntPtr.Add(ptr, headerSize);
                Marshal.StructureToPtr(packet.body, bodyPtr, false);
                Marshal.Copy(bodyPtr, arr, headerSize, bodySize);
                // generic 타입의 인스턴스를 StructureToPtr로 넘기면 에러 발생
                // packet.body는 보내는 시점에 해당 body 타입의 인스턴스임
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }

            return arr;
        }
        internal static byte[] SerializeHeaderOnly(PacketHeader header)
        {
            int headerSize = Marshal.SizeOf<PacketHeader>();
            byte[] arr = new byte[headerSize];

            header.length = (ushort)headerSize;

            IntPtr ptr = Marshal.AllocHGlobal(headerSize);
            try
            {
                Marshal.StructureToPtr(header, ptr, false);
                Marshal.Copy(ptr, arr, 0, headerSize);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }

            return arr;
        }

        internal static byte[] SerializeChatPacket(PacketHeader header, ChatRequestBody body, string message, byte scope, ulong targetID)
        {
            byte[] messageBytes = EncodingHelper.EncodeUtf8(message+'\0');
            // c# 문자열에는 \0가 안붙어서 c스타일 문자열로 바꿔주기 위해 \0 붙임

            int bodySize = Marshal.SizeOf<ChatRequestBody>();
            // 전체 패킷 사이즈
            int totalSize = Marshal.SizeOf<PacketHeader>() + bodySize + messageBytes.Length;
            header.length = (ushort)totalSize;
            body.messageLength = (ushort)messageBytes.Length;
            body.scope = (CHAT_SCOPE)scope;
            body.targetChatID = targetID;

            byte[] bodyBytes = new byte[bodySize];

            //body 직렬화
            IntPtr ptr = Marshal.AllocHGlobal(bodySize);
            try
            {
                Marshal.StructureToPtr(body, ptr, false);
                Marshal.Copy(ptr, bodyBytes, 0, bodySize);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }

            byte[] result = new byte[totalSize];

            // header 복사
            IntPtr headerPtr = Marshal.AllocHGlobal(Marshal.SizeOf<PacketHeader>());
            try
            {
                Marshal.StructureToPtr(header, headerPtr, false);
                Marshal.Copy(headerPtr, result, 0, Marshal.SizeOf<PacketHeader>());
            }
            finally
            {
                Marshal.FreeHGlobal(headerPtr);
            }

            // body + message 복사
            Buffer.BlockCopy(bodyBytes, 0, result, Marshal.SizeOf<PacketHeader>(), bodySize);
            Buffer.BlockCopy(messageBytes, 0, result, Marshal.SizeOf<PacketHeader>() + bodySize, messageBytes.Length);

            return result;
        }
        public static PacketHeader ExtractHeader(byte[] arr)
        {
            if (arr.Length < Marshal.SizeOf<PacketHeader>())
                throw new ArgumentException("Byte array is too small for packet header");

            int headerSize = Marshal.SizeOf<PacketHeader>();
            IntPtr headerPtr = Marshal.AllocHGlobal(headerSize);

            try
            {
                Marshal.Copy(arr, 0, headerPtr, headerSize);
                return Marshal.PtrToStructure<PacketHeader>(headerPtr);
            }
            finally
            {
                Marshal.FreeHGlobal(headerPtr);
            }
        }
        public struct Message 
        {
            public byte scope;
            public ulong senderID;
            public string senderName;
            public string message;
        }

        internal static Message DeserializeChatWhisper(PacketHeader header, byte[] buffer)
        {
            int headerSize = Marshal.SizeOf<PacketHeader>();
            int fixedBodySize = Marshal.SizeOf<ChatWhisperBody>();

            if (buffer.Length < headerSize + fixedBodySize)
                throw new ArgumentException("Byte array too small");

            STPacket<ChatWhisperBody> packet = new STPacket<ChatWhisperBody>();
            packet.header = header;

            // 고정 필드만 읽기
            byte[] bodyBytes = new byte[fixedBodySize];
            Array.Copy(buffer, headerSize, bodyBytes, 0, fixedBodySize);
            packet.body = ByteArrayToStructure<ChatWhisperBody>(bodyBytes);

            // 메시지 배열 읽기
            int messageOffset = headerSize + fixedBodySize;
            Message message = new Message();

            if (messageOffset + packet.body.messageLen > buffer.Length)
                throw new ArgumentException("Message length or offset exceeds buffer size messageLen:"  + packet.body.messageLen + " bufferLen: " + buffer.Length);

            message.scope = (byte)CHAT_SCOPE.Whisper;
            message.senderID = packet.body.senderChatID;
            message.senderName = EncodingHelper.DecodeUtf8(packet.body.name).TrimEnd('\0');
            message.message = EncodingHelper.DecodeUtf8(buffer, messageOffset, packet.body.messageLen).TrimEnd('\0');
            

            return message;
        }
        internal static  Message[] DeserializeChatBatch(PacketHeader header, byte[] buffer)
        {
            int headerSize = Marshal.SizeOf<PacketHeader>();
            int fixedBodySize = Marshal.SizeOf<ChatBatchNotifyBody>();

            if (buffer.Length < headerSize + fixedBodySize)
                throw new ArgumentException("Byte array too small");
        
            STPacket<ChatBatchNotifyBody> packet = new STPacket<ChatBatchNotifyBody>();
            packet.header = header;

            // 고정 필드만 읽기
            byte[] bodyBytes = new byte[fixedBodySize];
            Array.Copy(buffer, headerSize, bodyBytes, 0, fixedBodySize);
            packet.body = ByteArrayToStructure<ChatBatchNotifyBody>(bodyBytes);

            // 메시지 배열 읽기
            int messageOffset = headerSize + fixedBodySize;
            Message[] messages = new Message[packet.body.chatCnt];

            for (int i = 0; i < packet.body.chatCnt; i++)
            {
                var entity = packet.body.entities[i];

                if (messageOffset + entity.offset + entity.messageLength > buffer.Length)
                    throw new ArgumentException("Message length or offset exceeds buffer size" + messageOffset + " " + entity.offset + " " + entity.messageLength + " "  +  buffer.Length);

                messages[i].scope = (byte)packet.body.scope;
                messages[i].senderID = entity.senderChatID;
                messages[i].senderName = EncodingHelper.DecodeUtf8(entity.name).TrimEnd('\0');
                messages[i].message = EncodingHelper.DecodeUtf8(buffer, messageOffset + entity.offset, entity.messageLength).TrimEnd('\0');
            }

            return messages;
        }

        // 고정 길이 패킷
        internal static STPacket<T> Deserialize<T>(PacketHeader header, byte[] buffer) where T : struct
        {
            int headerSize = Marshal.SizeOf<PacketHeader>();
            int bodySize = (int)(header.length - headerSize);

            if (bodySize < Marshal.SizeOf<T>())
                throw new ArgumentException("Byte array too small " + bodySize + " " + Marshal.SizeOf<T>());

            STPacket<T> packet = new STPacket<T>();
            packet.header = header;

            if (bodySize > 0)
            {
                byte[] bodyBytes = new byte[bodySize];
                Array.Copy(buffer, headerSize, bodyBytes, 0, bodySize);
                packet.body = ByteArrayToStructure<T>(bodyBytes);
            }
            else
            {
                packet.body = default(T);
            }

            return packet;
        }
        public static T ReadStruct<T>(byte[] buffer, int offset) where T : struct
        {
            int size = Marshal.SizeOf<T>();
            byte[] slice = new byte[size];
            Buffer.BlockCopy(buffer, offset, slice, 0, size);
            return ByteArrayToStructure<T>(slice);
        }

        // 가변 길이 패킷, T는 필드 타입
        public static (ushort count, T[] items) DeserializeVariablePacket<T> (PacketHeader header, byte[] buffer) where T : struct
        {
            int offset = Marshal.SizeOf<PacketHeader>();
            int bodySize = (int)(header.length - offset);
            if (bodySize  < Marshal.SizeOf<ushort>())
                throw new ArgumentException("Byte array too small");

            ushort count = BitConverter.ToUInt16(buffer, offset);
            offset += sizeof(ushort);

            int itemSize = Marshal.SizeOf<T>();
            T[] items = new T[count];

            for (int i = 0; i < count; i++)
            {
                items[i] = ReadStruct<T>(buffer, offset);
                offset += itemSize;
            }

            return (count, items);
        }
    }
}
