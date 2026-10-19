using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using ClientCore;
using ClientCore.Network;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;

namespace C02.Tests
{
    public sealed class C10BazaarMyListTests
    {
        [Test]
        public void RequestContainsOnlyTheNonzeroCorrelationId()
        {
            var packet = (byte[])typeof(Packet).GetMethod("SerializeBazaarMyListRequest", BindingFlags.NonPublic | BindingFlags.Static)
                .Invoke(null, new object[] { uint.MaxValue });
            Assert.That(packet.Length, Is.EqualTo(12));
            Assert.That(BitConverter.ToUInt16(packet, 0), Is.EqualTo(0xabcd));
            Assert.That(BitConverter.ToUInt32(packet, 2), Is.EqualTo(12));
            Assert.That(packet[6], Is.EqualTo(28)); Assert.That(packet[7], Is.Zero);
            Assert.That(BitConverter.ToUInt32(packet, 8), Is.EqualTo(uint.MaxValue));
            Assert.Throws<TargetInvocationException>(() => typeof(Packet).GetMethod("SerializeBazaarMyListRequest", BindingFlags.NonPublic | BindingFlags.Static)
                .Invoke(null, new object[] { 0U }));
        }

        [Test]
        public void ZeroOneAndThirtyRowsPreserveStatusesAndUnsignedValues()
        {
            Assert.That(Parse(Frame(4, 0, 0)).status, Is.Zero);
            var empty = Parse(Frame(5, 1, 0));
            Assert.That(empty.status, Is.EqualTo(1)); Assert.That(empty.rows, Is.Empty);
            Assert.That(Parse(Frame(6, 1, 1)).rows[0].listingID, Is.EqualTo(1));
            var full = Parse(Frame(7, 1, 30));
            Assert.That(full.requestID, Is.EqualTo(7)); Assert.That(full.rows.Length, Is.EqualTo(30));
            Assert.That(full.rows[29].status, Is.EqualTo(2));
            Assert.That(full.rows[29].price, Is.EqualTo(ulong.MaxValue));
            Assert.That(full.rows[29].registeredAt, Is.EqualTo(ulong.MaxValue));
            Assert.That(full.rows[29].sellerCharacterID, Is.EqualTo(5011));
        }

        [Test]
        public void MalformedEnvelopeAndRowsAreRejectedAsAWhole()
        {
            var p = Frame(1, 1, 1); p[7] = 1; Reject(p);
            p = Frame(1, 1, 1); p[0] = 0; Reject(p);
            p = Frame(1, 1, 1); p[6] = 27; Reject(p);
            Reject(Frame(0, 1, 0)); Reject(Frame(1, 2, 0)); Reject(Frame(1, 0, 1)); Reject(Frame(1, 1, 31));
            p = Frame(1, 1, 1); p[13] = 2; Reject(p);
            p = Frame(1, 1, 1); Array.Resize(ref p, p.Length - 1); Reject(p);
            p = Frame(1, 1, 1); Array.Resize(ref p, p.Length + 1); Reject(p);
            foreach (int offset in new[] { 15, 23, 31, 35 })
            {
                p = Frame(1, 1, 1); Array.Clear(p, offset, offset < 31 ? 8 : offset == 31 ? 4 : 2); Reject(p);
            }
            p = Frame(1, 1, 1); p[53] = 3; Reject(p);
            p = Frame(1, 1, 1); p[53] = 255; Reject(p);
            p = Frame(1, 1, 2); Array.Copy(p, 15, p, 54, 8); Reject(p);
            p = Frame(1, 1, 1); var header = Packet.ExtractHeader(p); header.opcode = 27;
            Assert.Throws<TargetInvocationException>(() => Parse(header, p));
            header = Packet.ExtractHeader(p); header.length++;
            Assert.Throws<TargetInvocationException>(() => Parse(header, p));
        }

        [Test]
        public void HandlerRoutesMyListBeforeSimulationAndRejectsMalformedFrames()
        {
            using (var f = new Fixture())
            {
                var type = typeof(ViewModel).Assembly.GetType("ClientCore.Network.Handler");
                var handler = (IHandlerSock)Activator.CreateInstance(type, true);
                var tick = new FakeTick();
                type.GetMethod("Inject", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(handler, new object[] { f.Model, tick });
                f.Model.RequestBazaarMyList();
                handler.HandleResponse(Frame(f.Network.MyListIDs[0], 1, 3));
                Assert.That(f.Data.GetBazaar(), Is.Empty); f.Drain();
                Assert.That(f.Data.GetBazaar().Count, Is.EqualTo(3));
                Assert.That(f.Data.GetBazaar()[2].Status, Is.EqualTo(2));
                var malformed = Frame(2, 1, 1); malformed[7] = 1;
                Assert.Throws<ArgumentException>(() => handler.HandleResponse(malformed));
                malformed = Frame(2, 1, 1); malformed[53] = 3;
                Assert.Throws<ArgumentException>(() => handler.HandleResponse(malformed));
                Assert.That(tick.Queued, Is.Zero);
            }
        }

        [Test]
        public void PendingKindAndIdCannotBeAcknowledgedByTheOtherResponse()
        {
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnBazaarReceived += _ => events++;
                f.Model.RequestBazaarMyList(); uint myId = f.Network.MyListIDs[0];
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaar(1, 0));
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarMyList());
                f.Model.BazaarReceived(myId, 1, Rows());
                f.Model.BazaarMyListReceived(myId + 1, 1, Rows()); f.Drain();
                Assert.That(events, Is.Zero); Assert.That(f.Data.GetBazaar(), Is.Empty);
                f.Model.BazaarMyListReceived(myId, 1, Rows()); f.Drain(); Assert.That(events, Is.EqualTo(1));
                f.Model.RequestBazaar(1, 0); uint searchId = f.Network.SearchIDs[0];
                Assert.That(searchId, Is.GreaterThan(myId));
                f.Model.BazaarMyListReceived(searchId, 1, Rows()); f.Drain();
                Assert.That(events, Is.EqualTo(1)); Assert.That(f.Data.GetBazaar(), Is.Empty);
                f.Model.BazaarReceived(searchId, 1, Array.Empty<BazaarListing>()); f.Drain();
                Assert.That(events, Is.EqualTo(2));
            }
        }

        [Test]
        public void DispatcherOwnsTheReceivedArrayAndFailureDiffersFromEmptySuccess()
        {
            using (var f = new Fixture())
            {
                var results = new List<bool>(); f.Model.OnBazaarReceived += results.Add;
                f.Model.RequestBazaarMyList(); var rows = Rows();
                f.Model.BazaarMyListReceived(f.Network.MyListIDs[0], 1, rows);
                rows[0].price = 123; rows[0].status = 0;
                Assert.That(f.Data.GetBazaar(), Is.Empty); f.Drain();
                Assert.That(f.Data.GetBazaar()[0].Price, Is.EqualTo(ulong.MaxValue));
                Assert.That(f.Data.GetBazaar()[0].Status, Is.EqualTo(2));
                var published = f.Data.GetBazaar(); published.Clear(); Assert.That(f.Data.GetBazaar().Count, Is.EqualTo(1));
                f.Model.RequestBazaarMyList(); Assert.That(f.Data.GetBazaar(), Is.Empty);
                f.Model.BazaarMyListReceived(f.Network.MyListIDs[1], 0, Array.Empty<BazaarListing>()); f.Drain();
                f.Model.RequestBazaarMyList();
                f.Model.BazaarMyListReceived(f.Network.MyListIDs[2], 1, Array.Empty<BazaarListing>()); f.Drain();
                Assert.That(results, Is.EqualTo(new[] { true, false, true })); Assert.That(f.Data.GetBazaar(), Is.Empty);
            }
        }

        [Test]
        public void CancelForTabCloseOrTimeoutInvalidatesAlreadyQueuedAndLateResponses()
        {
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnBazaarReceived += _ => events++;
                f.Model.RequestBazaarMyList(); uint old = f.Network.MyListIDs[0];
                f.Model.BazaarMyListReceived(old, 1, Rows()); f.Model.CancelBazaar(); f.Model.RequestBazaar(1, 0);
                f.Model.BazaarMyListReceived(old, 1, Rows()); f.Drain();
                Assert.That(events, Is.Zero); Assert.That(f.Data.GetBazaar(), Is.Empty);
                f.Model.CancelBazaar(); f.Model.RequestBazaarMyList();
                uint current = f.Network.MyListIDs[1]; Assert.That(current, Is.GreaterThan(f.Network.SearchIDs[0]));
                f.Model.BazaarMyListReceived(old, 1, Rows());
                f.Model.BazaarMyListReceived(current, 1, Array.Empty<BazaarListing>()); f.Drain();
                Assert.That(events, Is.EqualTo(1)); Assert.That(f.Data.GetBazaar(), Is.Empty);
            }
        }

        [TestCase("zone")]
        [TestCase("disconnect")]
        [TestCase("dispose")]
        public void SessionEndInvalidatesQueuedMyList(string ending)
        {
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnBazaarReceived += _ => events++;
                f.Model.RequestBazaarMyList(); uint old = f.Network.MyListIDs[0];
                f.Model.BazaarMyListReceived(old, 1, Rows());
                if (ending == "zone") f.Model.ZoneChange(4);
                else if (ending == "disconnect") f.Model.Error("Fixture disconnect");
                else f.Model.Dispose();
                f.Model.BazaarMyListReceived(old, 1, Rows()); f.Drain();
                Assert.That(events, Is.Zero); Assert.That(f.Data.GetBazaar(), Is.Empty);
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarMyList());
                if (ending == "zone")
                {
                    f.Model.ZoneChageReceived(0, 0, 0, 0, 0, 0, 0); f.Drain();
                    f.Model.RequestBazaarMyList();
                    f.Model.BazaarMyListReceived(old, 1, Rows());
                    f.Model.BazaarMyListReceived(f.Network.MyListIDs[1], 1, Array.Empty<BazaarListing>()); f.Drain();
                    Assert.That(events, Is.EqualTo(1));
                }
            }
        }

        [Test]
        public void SendFailureReleasesPendingAndIdExhaustionNeverReusesAnOldId()
        {
            using (var f = new Fixture())
            {
                f.Network.FailMyList = true;
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarMyList());
                f.Network.FailMyList = false; f.Model.RequestBazaarMyList();
                Assert.That(f.Network.MyListIDs[1], Is.GreaterThan(f.Network.MyListIDs[0]));
                f.Model.CancelBazaar();
                typeof(ViewModel).GetField("_bazaarRequestID", BindingFlags.NonPublic | BindingFlags.Instance).SetValue(f.Model, uint.MaxValue);
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarMyList());
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaar(1, 0));
                Assert.That(f.Network.MyListIDs.Count, Is.EqualTo(2)); Assert.That(f.Network.SearchIDs, Is.Empty);
            }
        }

        private static BazaarListing[] Rows() => new[] { new BazaarListing { listingID = 1, sellerCharacterID = 5011, itemID = 1, quantity = 2, price = ulong.MaxValue, registeredAt = ulong.MaxValue, status = 2 } };
        private static (uint requestID, byte status, BazaarListing[] rows) Parse(byte[] p) => Parse(Packet.ExtractHeader(p), p);
        private static (uint requestID, byte status, BazaarListing[] rows) Parse(PacketHeader header, byte[] p) =>
            ((uint, byte, BazaarListing[]))typeof(Packet).GetMethod("DeserializeBazaarMyList", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { header, p });
        private static void Reject(byte[] p) => Assert.Throws<TargetInvocationException>(() => Parse(p));
        private static byte[] Frame(uint id, byte status, ushort count)
        {
            var p = new byte[15 + 39 * count]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes((uint)p.Length).CopyTo(p, 2);
            p[6] = 29; BitConverter.GetBytes(id).CopyTo(p, 8); p[12] = status; BitConverter.GetBytes(count).CopyTo(p, 13);
            for (int i = 0; i < count; i++)
            {
                int n = 15 + i * 39;
                BitConverter.GetBytes((ulong)i + 1).CopyTo(p, n); BitConverter.GetBytes(5011UL).CopyTo(p, n + 8);
                BitConverter.GetBytes(1U).CopyTo(p, n + 16); BitConverter.GetBytes((ushort)2).CopyTo(p, n + 20);
                BitConverter.GetBytes(ulong.MaxValue).CopyTo(p, n + 22); BitConverter.GetBytes(ulong.MaxValue).CopyTo(p, n + 30); p[n + 38] = (byte)(i % 3);
            }
            return p;
        }
        private sealed class Fixture : IMainThreadDispatcher, IDisposable
        {
            public readonly FakeNetwork Network = new FakeNetwork(); public readonly ViewModel Model; public readonly IViewDataUI Data;
            private readonly Queue<Action> jobs = new Queue<Action>();
            public Fixture()
            {
                var data = Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData"), true); Data = (IViewDataUI)data;
                Model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.NonPublic | BindingFlags.Instance, null, new object[] { Network, data, new NullLogger(), this }, null);
                C08TestSession.Ready(Model, Drain);
            }
            public void Post(Action job) => jobs.Enqueue(job);
            public void Drain() { while (jobs.Count != 0) jobs.Dequeue()(); }
            public void Dispose() => Model.Dispose();
        }
        private sealed class FakeTick : IReceiveTick { public int Queued; public void Enqueue(byte[] buffer) => Queued++; }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string text) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public readonly List<uint> MyListIDs = new List<uint>(); public readonly List<uint> SearchIDs = new List<uint>();
            public bool FailMyList;
            public void RequestBazaarMyList(uint requestID) { MyListIDs.Add(requestID); if (FailMyList) throw new InvalidOperationException("Fixture send failure"); }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) => SearchIDs.Add(requestID);
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { }
            public void Chat(string message, byte scope, ulong target) { } public void Profiles(uint[] ids) { } public void RequestInventory() { }
            public void Action(byte dir, float x, float y, byte slot) { } public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void Log(string text) { }
        }
    }
}
