using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;

namespace C02.Tests
{
    public sealed class C08ZoneTests
    {
        [Test]
        public void RoundTripRejectsOldZoneAndOldVisitAcrossAllSimulationKinds()
        {
            using (var f = new Fixture())
            {
                C08TestSession.Ready(f.Model, f.Drain);
                int full = 0, other = 0;
                f.Model.OnFullReceived += (_, __) => full++;
                f.Model.OnDeltaReceived += (_, __) => other++;
                f.Model.OnMonsterFullReceived += (_, __) => other++;
                f.Model.OnMonsterDeltaReceived += (_, __) => other++;
                f.Model.OnActionResultReceived += (_, __) => other++;
                f.Model.ZoneChange(4); f.Model.ZoneChange(4);
                Assert.That(f.Network.Zones, Is.EqualTo(2), "Initial ENTER plus one transition.");
                f.Model.FullReceived(1, 1, 1, Self()); f.Drain();
                Assert.That(full, Is.Zero, "Old data cannot unlock while ACK is pending.");
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestInventory());
                f.Model.ZoneChageReceived(1, 2, 0, 1, 100, 10, 1); f.Drain();
                Assert.Throws<InvalidOperationException>(() => f.Model.Chat("wait", 0, 0));
                f.Model.FullReceived(1, 1, 1, Self()); f.Drain(); Assert.That(full, Is.Zero);
                f.Model.FullReceived(2, 1, 1, Self()); f.Drain(); Assert.That(full, Is.EqualTo(1));
                f.Model.ZoneChange(3); f.Model.ZoneChageReceived(1, 1, 0, 1, 100, 10, 2); f.Drain();
                f.Model.FullReceived(1, 1, 1, Self());
                Others(f.Model, 1, 1); f.Drain();
                Assert.That(full, Is.EqualTo(1)); Assert.That(other, Is.Zero);
                f.Model.FullReceived(1, 2, 1, Self()); Others(f.Model, 1, 2); f.Drain();
                Assert.That(full, Is.EqualTo(2)); Assert.That(other, Is.EqualTo(4));
                // Other characters entering increment the Zone generation; they do not invalidate us.
                f.Model.FullReceived(1, 3, 1, Self()); f.Drain(); Assert.That(full, Is.EqualTo(3));
            }
        }

        [Test]
        public void RejectionPreservesOldWorldAndInFlightSameCharacterInventory()
        {
            using (var f = new Fixture())
            {
                C08TestSession.Ready(f.Model, f.Drain);
                f.Model.RequestInventory(); int responses = 0; f.Model.OnInventoryReceived += _ => responses++;
                f.Model.ZoneChange(4);
                f.Model.InventoryReceived(0, Array.Empty<InventoryItem>()); f.Drain();
                Assert.That(responses, Is.EqualTo(1));
                f.Model.ZoneChageReceived(0, 0, 0, 0, 0, 0, 0); f.Drain();
                Assert.DoesNotThrow(() => f.Model.RequestInventory());
                Assert.DoesNotThrow(() => f.Model.Chat("ready", 0, 0));
                Assert.Throws<ArgumentOutOfRangeException>(() => f.Model.ZoneChange(260));
            }
        }

        [TestCase(7)] [TestCase(8)] [TestCase(9)] [TestCase(10)] [TestCase(12)]
        public void SimulationEnvelopeRequiresZoneAndNonzeroGeneration(byte opcode)
        {
            var frame = new byte[20]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(frame, 0);
            BitConverter.GetBytes(20U).CopyTo(frame, 2); frame[6] = opcode; frame[7] = 1;
            BitConverter.GetBytes((ushort)2).CopyTo(frame, 10); BitConverter.GetBytes(17UL).CopyTo(frame, 12);
            var decoded = Packet.DeserializeVariablePacket<FullStateField>(Packet.ExtractHeader(frame), frame);
            Assert.That(decoded.zoneID, Is.EqualTo(2)); Assert.That(decoded.generation, Is.EqualTo(17));
            frame[10] = 0; Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<FullStateField>(Packet.ExtractHeader(frame), frame));
            frame[10] = 2; Array.Clear(frame, 12, 8);
            Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<FullStateField>(Packet.ExtractHeader(frame), frame));
        }

        [Test]
        public void ZoneAckRejectsLegacyLengthBadFlagsAndInvalidSuccessButAllowsZeroedFailure()
        {
            var p = new byte[39]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes(39U).CopyTo(p, 2); p[6] = 17; p[7] = 1;
            var parse = typeof(Packet).GetMethod("DeserializeZoneChange", BindingFlags.Static | BindingFlags.NonPublic);
            Assert.DoesNotThrow(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            p[8] = 1; Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            p[9] = 1; p[19] = 1; p[31] = 1;
            Assert.DoesNotThrow(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            p[7] = 0; Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            var old = new byte[31]; Array.Copy(p, old, 31); old[7] = 1; BitConverter.GetBytes(31U).CopyTo(old, 2);
            Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(old), old }));
        }

        private static FullStateField[] Self() => new[] { new FullStateField { zoneInternalID = 1 } };
        private static void Others(ViewModel m, ushort zone, ulong generation)
        {
            m.DeltaReceived(zone, generation, 0, Array.Empty<DeltaUpdateField>());
            m.MonsterFullReceived(zone, generation, 0, Array.Empty<MonsterFullField>());
            m.MonsterDeltaReceived(zone, generation, 0, Array.Empty<MonsterDeltaField>());
            m.ActionResultReceived(zone, generation, 0, Array.Empty<ActionResultField>());
        }
        private sealed class Fixture : IMainThreadDispatcher, IDisposable
        {
            public readonly FakeNetwork Network = new FakeNetwork(); public readonly ViewModel Model;
            private readonly Queue<Action> jobs = new Queue<Action>();
            public Fixture() { var data = Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData"), true); Model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.NonPublic | BindingFlags.Instance, null, new object[] { Network, data, new NullLogger(), this }, null); }
            public void Post(Action job) => jobs.Enqueue(job); public void Drain() { while (jobs.Count != 0) jobs.Dequeue()(); } public void Dispose() => Model.Dispose();
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string text) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public int Zones; public void ZoneChange(byte op) => Zones++;
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { }
            public void Chat(string message, byte scope, ulong target) { } public void Profiles(uint[] ids) { } public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { } public void RequestInventory() { }
            public void Action(byte dir, float x, float y, byte slot) { } public void Pong(ulong time) { } public void Log(string text) { }
        }
    }
}
