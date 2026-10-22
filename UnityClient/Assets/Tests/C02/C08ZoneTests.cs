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
        public void AutomaticTransitionKeepsOldZoneLockedUntilPlayerAndMonsterBootstrapPair()
        {
            using (var f = new Fixture())
            {
                C08TestSession.Ready(f.Model, f.Drain);
                int full = 0, other = 0;
                int transitions = 0;
                int failures = 0;
                ushort arrivedZone = 0;
                uint arrivedInternalID = 0;
                f.Model.OnFullReceived += (_, __) => full++;
                f.Model.OnDeltaReceived += (_, __) => other++;
                f.Model.OnMonsterFullReceived += (_, __) => other++;
                f.Model.OnMonsterDeltaReceived += (_, __) => other++;
                f.Model.OnActionResultReceived += (_, __) => other++;
                f.Model.OnZoneTransitionStarted += (_, zone) => { transitions++; Assert.That(zone, Is.EqualTo((ushort)2)); };
                f.Model.OnZoneChageFailed += () => failures++;
                f.Model.OnZoneChageReceived += (zone, _, internalID, __, ___) => { arrivedZone = zone; arrivedInternalID = internalID; };
                ((IViewDataUI)f.Data).UpdateMove(3, 101, 10);
                ((IViewModelTick)f.Model).SendUpdate();
                Assert.That(f.Network.LastActionEpoch, Is.EqualTo(1), "The outgoing movement carries the current ownership epoch.");
                Assert.That(f.Network.Zones, Is.EqualTo(1), "Only initial world entry uses ZONE_CHANGE.");
                f.Model.ZoneChageReceived(1, 2, 2, 2, 205, 10, 2, 2);
                f.Model.ZoneChageReceived(1, 2, 2, 2, 205, 10, 2, 2);
                f.Drain();
                Assert.That(transitions, Is.EqualTo(1));
                Assert.That(failures, Is.Zero, "A duplicate accepted ACK queued before dispatch must not be treated as a rejection.");
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestInventory());
                Assert.Throws<InvalidOperationException>(() => f.Model.Chat("wait", 0, 0));
                f.Model.FullReceived(1, 1, 0, 0, false, 1, Self());
                Others(f.Model, 1, 1); f.Drain();
                Assert.That(full, Is.Zero); Assert.That(other, Is.Zero, "Old-zone simulation cannot enter while the new baseline is pending.");
                f.Model.FullReceived(2, 2, 2, 2, true, 1, Self(2, 205, 10)); f.Drain();
                Assert.That(full, Is.Zero, "Player bootstrap alone cannot finish the paired baseline.");
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestInventory());
                typeof(ViewModel).GetField("_bootstrapDeadline", BindingFlags.Instance | BindingFlags.NonPublic).SetValue(f.Model, 0L);
                ((IViewModelTick)f.Model).SendUpdate();
                Assert.That(f.Network.LastResyncEpoch, Is.EqualTo(2), "A missing half requests the current epoch baseline again.");
                f.Model.MonsterFullReceived(2, 2, 2, 3, true, 0, Array.Empty<MonsterFullField>()); f.Drain();
                Assert.That(full, Is.Zero, "A higher bootstrap ID cannot pair with the previous player half.");
                f.Model.FullReceived(2, 2, 2, 2, true, 1, Self(2)); f.Drain();
                Assert.That(full, Is.Zero, "An older resync half is discarded.");
                f.Model.FullReceived(2, 2, 2, 3, true, 1, Self(2)); f.Drain();
                Assert.That(full, Is.EqualTo(1));
                Assert.That(other, Is.EqualTo(1), "The paired empty monster bootstrap is applied once with the player baseline.");
                Assert.That(arrivedZone, Is.EqualTo((ushort)2));
                Assert.That(arrivedInternalID, Is.EqualTo(2));
                f.Model.FullReceived(1, 3, 0, 0, false, 1, Self()); Others(f.Model, 1, 3); f.Drain();
                Assert.That(full, Is.EqualTo(1)); Assert.That(other, Is.EqualTo(1), "The old Zone remains rejected after transition.");
                f.Model.FullReceived(2, 1, 0, 0, false, 1, Self()); Others(f.Model, 2, 1); f.Drain();
                Assert.That(full, Is.EqualTo(1)); Assert.That(other, Is.EqualTo(1), "Lower generation periodic data is stale.");
                f.Model.FullReceived(2, 3, 0, 0, false, 1, Self(2, 205, 10)); Others(f.Model, 2, 3); f.Drain();
                Assert.That(full, Is.EqualTo(2)); Assert.That(other, Is.EqualTo(5));
            }
        }

        [Test]
        public void DuplicateOldTransitionAndInFlightInventoryCannotUnlockOrReplaceNewWorld()
        {
            using (var f = new Fixture())
            {
                C08TestSession.Ready(f.Model, f.Drain);
                f.Model.RequestInventory(); int responses = 0; f.Model.OnInventoryReceived += _ => responses++;
                f.Model.InventoryReceived(0, Array.Empty<InventoryItem>()); f.Drain();
                Assert.That(responses, Is.EqualTo(1));
                C08TestSession.BeginAutomaticTransition(f.Model, f.Drain);
                f.Model.ZoneChageReceived(1, 1, 0, 1, 10, 10, 1, 1); f.Drain();
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestInventory());
                C08TestSession.CompleteAutomaticTransition(f.Model, f.Drain);
                Assert.DoesNotThrow(() => f.Model.RequestInventory());
                Assert.DoesNotThrow(() => f.Model.Chat("ready", 0, 0));
                Assert.Throws<ArgumentOutOfRangeException>(() => f.Model.ZoneChange(4));
            }
        }

        [TestCase(7)] [TestCase(8)] [TestCase(9)] [TestCase(10)] [TestCase(12)]
        public void SimulationEnvelopeRequiresZoneAndNonzeroGeneration(byte opcode)
        {
            bool full = opcode == 7 || opcode == 9;
            var frame = new byte[full ? 37 : 20]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(frame, 0);
            BitConverter.GetBytes((uint)frame.Length).CopyTo(frame, 2); frame[6] = opcode; frame[7] = 1;
            BitConverter.GetBytes((ushort)2).CopyTo(frame, 10); BitConverter.GetBytes(17UL).CopyTo(frame, 12);
            ushort decodedZone; ulong decodedGeneration;
            if (full)
            {
                var decoded = Packet.DeserializeFullSnapshot<FullStateField>(Packet.ExtractHeader(frame), frame);
                decodedZone = decoded.zoneID; decodedGeneration = decoded.generation;
            }
            else
            {
                var decoded = Packet.DeserializeVariablePacket<FullStateField>(Packet.ExtractHeader(frame), frame);
                decodedZone = decoded.zoneID; decodedGeneration = decoded.generation;
            }
            Assert.That(decodedZone, Is.EqualTo(2)); Assert.That(decodedGeneration, Is.EqualTo(17));
            frame[10] = 0; Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<FullStateField>(Packet.ExtractHeader(frame), frame));
            frame[10] = 2; Array.Clear(frame, 12, 8);
            Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<FullStateField>(Packet.ExtractHeader(frame), frame));
        }

        [Test]
        public void ZoneAckRejectsLegacyLengthBadFlagsAndInvalidSuccessButAllowsZeroedFailure()
        {
            var p = new byte[47]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes(47U).CopyTo(p, 2); p[6] = 17; p[7] = 1;
            var parse = typeof(Packet).GetMethod("DeserializeZoneChange", BindingFlags.Static | BindingFlags.NonPublic);
            Assert.DoesNotThrow(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            p[8] = 1; Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            p[9] = 1; p[19] = 1; p[31] = 1; p[39] = 1;
            Assert.DoesNotThrow(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            foreach (byte zone in new byte[] { 5, 6 })
            {
                p[9] = zone;
                Assert.DoesNotThrow(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
                var frame = new byte[20]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(frame, 0);
                BitConverter.GetBytes(20U).CopyTo(frame, 2); frame[6] = 7; frame[7] = 1;
                BitConverter.GetBytes((ushort)zone).CopyTo(frame, 10); BitConverter.GetBytes(17UL).CopyTo(frame, 12);
                Assert.That(Packet.DeserializeVariablePacket<FullStateField>(Packet.ExtractHeader(frame), frame).zoneID, Is.EqualTo(zone));
                frame[10] = 7;
                Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<FullStateField>(Packet.ExtractHeader(frame), frame));
            }
            p[9] = 7; Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            p[9] = 1;
            p[7] = 0; Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(p), p }));
            var old = new byte[39]; Array.Copy(p, old, 39); old[7] = 1; BitConverter.GetBytes(39U).CopyTo(old, 2);
            Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(old), old }));
        }

        private static FullStateField[] Self(uint id = 1, float x = 0, float y = 0) =>
            new[] { new FullStateField { zoneInternalID = id, x = x, y = y } };
        private static void Others(ViewModel m, ushort zone, ulong generation)
        {
            m.DeltaReceived(zone, generation, 0, Array.Empty<DeltaUpdateField>());
            m.MonsterFullReceived(zone, generation, 0, 0, false, 0, Array.Empty<MonsterFullField>());
            m.MonsterDeltaReceived(zone, generation, 0, Array.Empty<MonsterDeltaField>());
            m.ActionResultReceived(zone, generation, 0, Array.Empty<ActionResultField>());
        }
        private sealed class Fixture : IMainThreadDispatcher, IDisposable
        {
            public readonly FakeNetwork Network = new FakeNetwork(); public readonly ViewModel Model;
            public readonly IViewDataUI Data;
            private readonly Queue<Action> jobs = new Queue<Action>();
            public Fixture() { Data = (IViewDataUI)Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData"), true); Model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.NonPublic | BindingFlags.Instance, null, new object[] { Network, Data, new NullLogger(), this }, null); }
            public void Post(Action job) => jobs.Enqueue(job); public void Drain() { while (jobs.Count != 0) jobs.Dequeue()(); } public void Dispose() => Model.Dispose();
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string text) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public int Zones; public ulong LastActionEpoch; public ulong LastResyncEpoch;
            public void ZoneChange(byte op) => Zones++;
            public void ZoneResync(ulong ownershipEpoch) => LastResyncEpoch = ownershipEpoch;
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { }
            public void Chat(string message, byte scope, ulong target) { } public void Profiles(uint[] ids) { } public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { } public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { } public void RequestInventory() { }
            public void Action(byte dir, float x, float y, byte slot, ulong ownershipEpoch) => LastActionEpoch = ownershipEpoch;
            public void Pong(ulong time) { } public void Log(string text) { }
        }
    }
}
