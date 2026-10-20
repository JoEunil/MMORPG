using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;

namespace C02.Tests
{
    public sealed class C09BazaarTests
    {
        [Test]
        public void RequestWireIsSixteenBytesAndRejectsZeroIdAndUnsupportedType()
        {
            var bytes = Request(17, 3, ushort.MaxValue);
            Assert.That(bytes.Length, Is.EqualTo(16)); Assert.That(bytes[6], Is.EqualTo(26)); Assert.That(bytes[7], Is.Zero);
            Assert.That(BitConverter.ToUInt32(bytes, 8), Is.EqualTo(17));
            Assert.That(BitConverter.ToUInt16(bytes, 12), Is.EqualTo(3)); Assert.That(BitConverter.ToUInt16(bytes, 14), Is.EqualTo(ushort.MaxValue));
            Assert.Throws<TargetInvocationException>(() => Request(0, 1, 0));
            Assert.Throws<TargetInvocationException>(() => Request(1, 0, 0));
            Assert.Throws<TargetInvocationException>(() => Request(1, 4, 0));
        }

        [Test]
        public void EmptyFailureAndThirtyPackedRowsAreDistinctAndPreserveUlongValues()
        {
            Assert.That(Marshal.SizeOf<BazaarListing>(), Is.EqualTo(39));
            Assert.That(Parse(Frame(1, 0, 0)).status, Is.Zero);
            Assert.That(Parse(Frame(1, 1, 0)).rows, Is.Empty);
            var p = Frame(12, 1, 30); var result = Parse(p);
            Assert.That(p.Length, Is.EqualTo(1185)); Assert.That(result.requestID, Is.EqualTo(12)); Assert.That(result.rows.Length, Is.EqualTo(30));
            Assert.That(result.rows[29].listingID, Is.EqualTo(30)); Assert.That(result.rows[29].price, Is.EqualTo(ulong.MaxValue));
            Assert.That(result.rows[29].registeredAt, Is.EqualTo(ulong.MaxValue));
        }

        [Test]
        public void MalformedRowsLengthStatusAndFlagsCannotPopulateTheModel()
        {
            var p = Frame(1, 1, 1); p[7] = 1; Reject(p);
            p = Frame(1, 2, 0); Reject(p);
            p = Frame(0, 1, 0); Reject(p);
            p = Frame(1, 0, 1); Reject(p);
            p = Frame(1, 1, 1); p[13] = 2; Reject(p);
            p = Frame(1, 1, 1); Array.Clear(p, 15, 8); Reject(p);
            p = Frame(1, 1, 1); Array.Clear(p, 23, 8); Reject(p);
            p = Frame(1, 1, 1); Array.Clear(p, 31, 4); Reject(p);
            p = Frame(1, 1, 1); Array.Clear(p, 35, 2); Reject(p);
            p = Frame(1, 1, 1); p[53] = 1; Reject(p);
            p = Frame(1, 1, 2); Array.Copy(p, 15, p, 54, 8); Reject(p);
        }

        [Test]
        public void SingleRequestDispatcherCorrelationAndSnapshotCopyAreEnforced()
        {
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnBazaarReceived += _ => events++;
                f.Model.RequestBazaar(1, 0); Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaar(2, 0));
                f.Model.BazaarReceived(99, 1, Rows()); f.Drain(); Assert.That(events, Is.Zero);
                f.Model.BazaarReceived(f.Network.IDs[0], 1, Rows());
                Assert.That(f.Data.GetBazaar(), Is.Empty); f.Drain();
                Assert.That(events, Is.EqualTo(1)); Assert.That(f.Data.GetBazaar()[0].Price, Is.EqualTo(ulong.MaxValue));
                var snapshot = f.Data.GetBazaar(); snapshot.Clear(); Assert.That(f.Data.GetBazaar().Count, Is.EqualTo(1));
                f.Model.BazaarReceived(f.Network.IDs[0], 0, Array.Empty<BazaarListing>()); f.Drain();
                Assert.That(events, Is.EqualTo(1));
                f.Model.RequestBazaar(2, 1); Assert.That(f.Data.GetBazaar(), Is.Empty);
                f.Model.BazaarReceived(f.Network.IDs[1], 0, Rows()); f.Drain(); Assert.That(f.Data.GetBazaar(), Is.Empty);
            }
        }

        [Test]
        public void CancelCloseOrTimeoutAllowsNewIdAndRejectsLateQueuedResponse()
        {
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnBazaarReceived += _ => events++;
                f.Model.RequestBazaar(1, 0); var old = f.Network.IDs[0];
                f.Model.BazaarReceived(old, 1, Rows()); f.Model.CancelBazaar(); f.Model.RequestBazaar(3, 0); f.Drain();
                Assert.That(events, Is.Zero); Assert.That(f.Data.GetBazaar(), Is.Empty);
                Assert.That(f.Network.IDs[1], Is.GreaterThan(old));
                f.Model.BazaarReceived(f.Network.IDs[1], 1, Array.Empty<BazaarListing>()); f.Drain(); Assert.That(events, Is.EqualTo(1));
            }
        }

        [Test]
        public void ZoneTransitionFailureAndDisposeCannotPublishAnOldSearch()
        {
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnBazaarReceived += _ => events++;
                f.Model.RequestBazaar(1, 0); f.Model.ZoneChange(4);
                f.Model.BazaarReceived(f.Network.IDs[0], 1, Rows()); f.Drain(); Assert.That(events, Is.Zero);
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaar(1, 0));
                f.Model.ZoneChageReceived(0, 0, 0, 0, 0, 0, 0); f.Drain();
                f.Model.RequestBazaar(1, 0); f.Model.BazaarReceived(f.Network.IDs[1], 1, Rows()); f.Model.Dispose(); f.Drain();
                Assert.That(events, Is.Zero); Assert.That(f.Data.GetBazaar(), Is.Empty);
            }
        }

        private static BazaarListing[] Rows() => new[] { new BazaarListing { listingID = 1, sellerCharacterID = 5011, itemID = 1, quantity = 2, price = ulong.MaxValue, registeredAt = ulong.MaxValue } };
        private static byte[] Request(uint id, ushort type, ushort page) => (byte[])typeof(Packet).GetMethod("SerializeBazaarRequest", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { id, type, page });
        private static (uint requestID, byte status, BazaarListing[] rows) Parse(byte[] p) => ((uint, byte, BazaarListing[]))typeof(Packet).GetMethod("DeserializeBazaar", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { Packet.ExtractHeader(p), p });
        private static void Reject(byte[] p) => Assert.Throws<TargetInvocationException>(() => Parse(p));
        private static byte[] Frame(uint id, byte status, ushort count)
        {
            var p = new byte[15 + 39 * count]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes((uint)p.Length).CopyTo(p, 2);
            p[6] = 27; BitConverter.GetBytes(id).CopyTo(p, 8); p[12] = status; BitConverter.GetBytes(count).CopyTo(p, 13);
            for (int i = 0; i < count; i++) { int n = 15 + i * 39; BitConverter.GetBytes((ulong)i + 1).CopyTo(p, n); BitConverter.GetBytes(5011UL).CopyTo(p, n + 8); BitConverter.GetBytes(1U).CopyTo(p, n + 16); BitConverter.GetBytes((ushort)2).CopyTo(p, n + 20); BitConverter.GetBytes(ulong.MaxValue).CopyTo(p, n + 22); BitConverter.GetBytes(ulong.MaxValue).CopyTo(p, n + 30); }
            return p;
        }
        private sealed class Fixture : IMainThreadDispatcher, IDisposable
        {
            public readonly FakeNetwork Network = new FakeNetwork(); public readonly ViewModel Model; public readonly IViewDataUI Data;
            private readonly Queue<Action> jobs = new Queue<Action>();
            public Fixture() { var data = Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData"), true); Data = (IViewDataUI)data; Model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.NonPublic | BindingFlags.Instance, null, new object[] { Network, data, new NullLogger(), this }, null); C08TestSession.Ready(Model, Drain); }
            public void Post(Action job) => jobs.Enqueue(job); public void Drain() { while (jobs.Count != 0) jobs.Dequeue()(); } public void Dispose() => Model.Dispose();
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string text) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public readonly List<uint> IDs = new List<uint>(); public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint id, ushort type, ushort page) => IDs.Add(id);
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { }
            public void Chat(string message, byte scope, ulong target) { } public void Profiles(uint[] ids) { } public void RequestInventory() { }
            public void Action(byte dir, float x, float y, byte slot) { } public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void Log(string text) { }
        }
    }
}
