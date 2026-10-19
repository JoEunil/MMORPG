using System;
using System.Runtime.InteropServices;
using ClientCore.PacketHelper;
using NUnit.Framework;

namespace C02.Tests
{
    public sealed class C02FullPacketFixtureTests
    {
        [Test]
        public void FullStateField_MatchesServer43ByteLayoutAndValues()
        {
            Assert.That(Marshal.SizeOf<FullStateField>(), Is.EqualTo(43));
            Assert.That(Marshal.OffsetOf<FullStateField>("zoneInternalID").ToInt32(), Is.EqualTo(0));
            Assert.That(Marshal.OffsetOf<FullStateField>("profileId").ToInt32(), Is.EqualTo(4));
            Assert.That(Marshal.OffsetOf<FullStateField>("profileVersion").ToInt32(), Is.EqualTo(8));
            Assert.That(Marshal.OffsetOf<FullStateField>("hp").ToInt32(), Is.EqualTo(12));
            Assert.That(Marshal.OffsetOf<FullStateField>("mp").ToInt32(), Is.EqualTo(16));
            Assert.That(Marshal.OffsetOf<FullStateField>("maxHp").ToInt32(), Is.EqualTo(20));
            Assert.That(Marshal.OffsetOf<FullStateField>("maxMp").ToInt32(), Is.EqualTo(24));
            Assert.That(Marshal.OffsetOf<FullStateField>("exp").ToInt32(), Is.EqualTo(28));
            Assert.That(Marshal.OffsetOf<FullStateField>("level").ToInt32(), Is.EqualTo(32));
            Assert.That(Marshal.OffsetOf<FullStateField>("dir").ToInt32(), Is.EqualTo(34));
            Assert.That(Marshal.OffsetOf<FullStateField>("x").ToInt32(), Is.EqualTo(35));
            Assert.That(Marshal.OffsetOf<FullStateField>("y").ToInt32(), Is.EqualTo(39));

            var frame = CreateFullFrame(0x10203040, 0x50607080, 0x90A0B0C0, 321, 123, 999, 456, 7654321, 17, 3, 12.5f, -7.25f);
            var header = Packet.ExtractHeader(frame);
            var decoded = Packet.DeserializeVariablePacket<FullStateField>(header, frame);

            Assert.That(decoded.count, Is.EqualTo(1));
            Assert.That(decoded.items, Has.Length.EqualTo(1));
            var state = decoded.items[0];
            Assert.That(state.zoneInternalID, Is.EqualTo(0x10203040));
            Assert.That(state.profileId, Is.EqualTo(0x50607080));
            Assert.That(state.profileVersion, Is.EqualTo(0x90A0B0C0));
            Assert.That(state.hp, Is.EqualTo(321));
            Assert.That(state.mp, Is.EqualTo(123));
            Assert.That(state.maxHp, Is.EqualTo(999));
            Assert.That(state.maxMp, Is.EqualTo(456));
            Assert.That(state.exp, Is.EqualTo(7654321));
            Assert.That(state.level, Is.EqualTo(17));
            Assert.That(state.dir, Is.EqualTo(3));
            Assert.That(state.x, Is.EqualTo(12.5f));
            Assert.That(state.y, Is.EqualTo(-7.25f));
        }

        [TestCase(9, 1)]
        [TestCase(62, 1)]
        [TestCase(63, 2)]
        [TestCase(64, 1)]
        [TestCase(63, ushort.MaxValue)]
        public void FullFrameWithTruncatedOrMismatchedCount_IsRejected(int frameLength, int count)
        {
            var frame = CreateFrame(frameLength, checked((ushort)count));
            var header = Packet.ExtractHeader(frame);
            Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<FullStateField>(header, frame));
        }

        private static byte[] CreateFullFrame(uint internalId, uint profileId, uint profileVersion,
            int hp, int mp, int maxHp, int maxMp, uint exp, ushort level, byte direction, float x, float y)
        {
            var frame = CreateFrame(63, 1);
            var offset = 20;
            Write(frame, ref offset, internalId);
            Write(frame, ref offset, profileId);
            Write(frame, ref offset, profileVersion);
            Write(frame, ref offset, hp);
            Write(frame, ref offset, mp);
            Write(frame, ref offset, maxHp);
            Write(frame, ref offset, maxMp);
            Write(frame, ref offset, exp);
            Write(frame, ref offset, level);
            frame[offset++] = direction;
            Write(frame, ref offset, x);
            Write(frame, ref offset, y);
            return frame;
        }

        private static byte[] CreateFrame(int frameLength, ushort count)
        {
            var frame = new byte[Math.Max(frameLength, 0)];
            if (frame.Length >= 8)
            {
                frame[0] = 0xCD;
                frame[1] = 0xAB;
                Buffer.BlockCopy(BitConverter.GetBytes((uint)frameLength), 0, frame, 2, 4);
                frame[6] = 7;
                frame[7] = 1;
            }
            if (frame.Length >= 10)
                Buffer.BlockCopy(BitConverter.GetBytes(count), 0, frame, 8, 2);
            if (frame.Length >= 20)
            {
                BitConverter.GetBytes((ushort)1).CopyTo(frame, 10);
                BitConverter.GetBytes(1UL).CopyTo(frame, 12);
            }
            return frame;
        }

        private static void Write(byte[] frame, ref int offset, uint value) => WriteBytes(frame, ref offset, BitConverter.GetBytes(value));
        private static void Write(byte[] frame, ref int offset, int value) => WriteBytes(frame, ref offset, BitConverter.GetBytes(value));
        private static void Write(byte[] frame, ref int offset, ushort value) => WriteBytes(frame, ref offset, BitConverter.GetBytes(value));
        private static void Write(byte[] frame, ref int offset, float value) => WriteBytes(frame, ref offset, BitConverter.GetBytes(value));

        private static void WriteBytes(byte[] frame, ref int offset, byte[] value)
        {
            Buffer.BlockCopy(value, 0, frame, offset, value.Length);
            offset += value.Length;
        }
    }
}
