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
        PROFILE_BATCH = 24,
        PROFILE_BATCH_RES = 25,
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
        public uint length;
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
        public uint zoneInternalID;
        public float startX;
        public float startY;
        public ulong zoneGeneration;
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
        public ushort messageLen;
        public ulong senderChatID;
        public uint senderProfileId;
    };// + raw char*

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ChatEntity
    {
        public ulong senderChatID; // sender
        public ushort offset;
        public ushort messageLength;
        public uint senderProfileId;
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

    public struct ProfileEntry
    {
        public uint profileId;
        public uint version;
        public string name;
    }


    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ActionRequestBody
    {
        public byte dir;      // 애니메이션/스킬 시전 방향
        public float x, y;    // 이동 후 최종 좌표(절대). 서버가 경과 시간 대비 이동량을 검증한다.
        public byte skillSlot;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ActionResultField
    {
        public byte casterType;
        public uint casterId;
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
        public uint zoneInternalID;
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
        public uint zoneInternalID;
        public uint profileId;
        public uint profileVersion;
        public int hp;
        public int mp;
        public int maxHp;
        public int maxMp;
        public uint exp;
        public ushort level;
        public byte dir;
        public float x;
        public float y;
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
        private static readonly UTF8Encoding StrictUtf8 = new UTF8Encoding(false, true);
        
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
            packet.header.length = (uint)totalSize;

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

            header.length = (uint)headerSize;

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
            if (string.IsNullOrWhiteSpace(message) || message.IndexOfAny(new[] { '\0', '\r', '\n' }) >= 0)
                throw new ArgumentException("Enter a single-line chat message.");
            if (scope > (byte)CHAT_SCOPE.Whisper || (scope != (byte)CHAT_SCOPE.Whisper && targetID != 0))
                throw new ArgumentException("Invalid chat destination.");
            byte[] messageBytes = StrictUtf8.GetBytes(message);
            if (messageBytes.Length > 128) throw new ArgumentException("Chat message exceeds 128 UTF8 bytes.");

            int bodySize = Marshal.SizeOf<ChatRequestBody>();
            // 전체 패킷 사이즈
            int totalSize = Marshal.SizeOf<PacketHeader>() + bodySize + messageBytes.Length;
            header.length = (uint)totalSize;
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
            public uint senderProfileId;
            public string message;
        }

        internal static Message DeserializeChatWhisper(PacketHeader header, byte[] buffer)
        {
            ValidateChatProfileFrame(header, buffer, 14, 22);
            ushort length = BitConverter.ToUInt16(buffer, 8);
            if (22 + length != buffer.Length) throw new ArgumentException("Invalid whisper length.");
            return new Message
            {
                scope = (byte)CHAT_SCOPE.Whisper,
                senderID = BitConverter.ToUInt64(buffer, 10),
                senderProfileId = BitConverter.ToUInt32(buffer, 18),
                message = StrictUtf8.GetString(buffer, 22, length)
            };
        }
        internal static  Message[] DeserializeChatBatch(PacketHeader header, byte[] buffer)
        {
            // Ten fixed 16-byte entity slots precede the variable message region.
            ValidateChatProfileFrame(header, buffer, 15, 173);
            ushort count = BitConverter.ToUInt16(buffer, 8);
            ushort total = BitConverter.ToUInt16(buffer, 10);
            byte scope = buffer[12];
            if (count > 10 || scope > (byte)CHAT_SCOPE.Whisper || 173 + total != buffer.Length)
                throw new ArgumentException("Invalid chat count, scope or payload length.");
            var messages = new Message[count];
            for (int i = 0; i < count; i++)
            {
                int entity = 13 + i * 16;
                ushort offset = BitConverter.ToUInt16(buffer, entity + 8);
                ushort length = BitConverter.ToUInt16(buffer, entity + 10);
                if ((int)offset + length > total) throw new ArgumentException("Invalid chat message range.");
                messages[i] = new Message
                {
                    scope = scope,
                    senderID = BitConverter.ToUInt64(buffer, entity),
                    senderProfileId = BitConverter.ToUInt32(buffer, entity + 12),
                    message = StrictUtf8.GetString(buffer, 173 + offset, length)
                };
            }
            return messages;
        }

        private static void ValidateChatProfileFrame(PacketHeader header, byte[] buffer, byte opcode, int minimum)
        {
            if (buffer == null || buffer.Length < minimum || header.magic != MAGIC ||
                header.length != buffer.Length || header.opcode != opcode || header.flags != 0)
                throw new ArgumentException("Invalid chat/profile frame.");
            var actual = ExtractHeader(buffer);
            if (actual.magic != header.magic || actual.length != header.length ||
                actual.opcode != header.opcode || actual.flags != header.flags)
                throw new ArgumentException("Mismatched packet header.");
        }

        internal static ProfileEntry[] DeserializeProfiles(PacketHeader header, byte[] buffer)
        {
            ValidateChatProfileFrame(header, buffer, 25, 10);
            ushort count = BitConverter.ToUInt16(buffer, 8);
            if (count > 50 || 10 + count * 40 != buffer.Length)
                throw new ArgumentException("Invalid profile count or length.");
            var result = new ProfileEntry[count];
            var ids = new HashSet<uint>();
            for (int i = 0; i < count; i++)
            {
                int offset = 10 + i * 40;
                uint id = BitConverter.ToUInt32(buffer, offset);
                if (id == 0 || !ids.Add(id)) throw new ArgumentException("Invalid or duplicate profile ID.");
                int length = Array.IndexOf(buffer, (byte)0, offset + 8, 32) - (offset + 8);
                if (length < 0) throw new ArgumentException("Unterminated profile name.");
                result[i] = new ProfileEntry
                {
                    profileId = id,
                    version = BitConverter.ToUInt32(buffer, offset + 4),
                    name = StrictUtf8.GetString(buffer, offset + 8, length)
                };
            }
            return result;
        }

        internal static InventoryResBody DeserializeInventory(PacketHeader header, byte[] buffer)
        {
            ValidateChatProfileFrame(header, buffer, 21, 81);
            if (buffer.Length != 81 || buffer[8] > 1)
                throw new ArgumentException("Invalid inventory frame.");
            ushort count = BitConverter.ToUInt16(buffer, 9);
            if (count > 10 || (buffer[8] == 0 && count != 0))
                throw new ArgumentException("Invalid inventory count.");
            var items = new InventoryItem[count];
            var slots = new HashSet<byte>();
            for (int i = 0; i < count; i++)
            {
                int offset = 11 + 7 * i;
                var item = new InventoryItem { itemID = BitConverter.ToUInt32(buffer, offset),
                    quantity = BitConverter.ToUInt16(buffer, offset + 4), slot = buffer[offset + 6] };
                if (item.itemID == 0 || item.quantity == 0 || item.slot >= 10 || !slots.Add(item.slot))
                    throw new ArgumentException("Invalid inventory row.");
                items[i] = item;
            }
            return new InventoryResBody { resStatus = buffer[8], itemCount = count, items = items };
        }

        internal static byte[] SerializeProfileRequest(uint[] profileIds)
        {
            if (profileIds == null || profileIds.Length == 0 || profileIds.Length > 50)
                throw new ArgumentException("Invalid profile request count.");
            var ids = new HashSet<uint>();
            var result = new byte[10 + profileIds.Length * 4];
            Buffer.BlockCopy(SerializeHeaderOnly(new PacketHeader { magic = MAGIC, opcode = (byte)OP_CODE.PROFILE_BATCH }), 0, result, 0, 8);
            Buffer.BlockCopy(BitConverter.GetBytes((uint)result.Length), 0, result, 2, 4);
            Buffer.BlockCopy(BitConverter.GetBytes((ushort)profileIds.Length), 0, result, 8, 2);
            for (int i = 0; i < profileIds.Length; i++)
            {
                if (profileIds[i] == 0 || !ids.Add(profileIds[i])) throw new ArgumentException("Invalid or duplicate profile ID.");
                Buffer.BlockCopy(BitConverter.GetBytes(profileIds[i]), 0, result, 10 + i * 4, 4);
            }
            return result;
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
        public static (ushort count, ushort zoneID, ulong generation, T[] items) DeserializeVariablePacket<T> (PacketHeader header, byte[] buffer) where T : struct
        {
            int offset = Marshal.SizeOf<PacketHeader>();
            if (buffer == null || header.flags != FLAG_SIMULATION || header.length != buffer.Length || buffer.Length < offset + 12)
                throw new ArgumentException("Invalid variable packet length.");

            ushort count = BitConverter.ToUInt16(buffer, offset);
            ushort zoneID = BitConverter.ToUInt16(buffer, offset + 2);
            ulong generation = BitConverter.ToUInt64(buffer, offset + 4);
            if (zoneID < 1 || zoneID > 4 || generation == 0)
                throw new ArgumentException("Invalid simulation zone identity.");
            offset += 12;

            int itemSize = Marshal.SizeOf<T>();
            // Validate before allocating or reading any unmanaged struct.
            if ((long)offset + (long)count * itemSize != buffer.Length)
                throw new ArgumentException("Invalid variable packet count or item length.");
            T[] items = new T[count];

            for (int i = 0; i < count; i++)
            {
                items[i] = ReadStruct<T>(buffer, offset);
                offset += itemSize;
            }

            return (count, zoneID, generation, items);
        }

        internal static STPacket<ZoneChangeResponseBody> DeserializeZoneChange(PacketHeader header, byte[] buffer)
        {
            if (header.flags != FLAG_SIMULATION || header.length != 39 || buffer.Length != 39)
                throw new ArgumentException("Invalid zone response length or flags.");
            var packet = Deserialize<ZoneChangeResponseBody>(header, buffer);
            var body = packet.body;
            if (body.resStatus > 1 || body.resStatus == 1 && (body.zoneID < 1 || body.zoneID > 4 ||
                body.zoneGeneration == 0 || body.zoneInternalID == 0 || body.zoneInternalID == uint.MaxValue ||
                float.IsNaN(body.startX) || float.IsInfinity(body.startX) || float.IsNaN(body.startY) || float.IsInfinity(body.startY)))
                throw new ArgumentException("Invalid zone response identity.");
            return packet;
        }
    }
}
