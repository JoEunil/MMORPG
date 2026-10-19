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
    public sealed class C07InventoryTests
    {
        [Test] public void FixedWireUsesCountAndSupportsEmptyAndTenRows()
        {
            Assert.That(Parse(Frame(1, 0)).items.Length, Is.Zero);
            var packet = Frame(1, 10);
            for (int i = 0; i < 10; i++) Row(packet, i, (uint)(100 + i), (ushort)(i + 1), (byte)i);
            var result = Parse(packet);
            Assert.That(result.items.Length, Is.EqualTo(10));
            Assert.That(result.items[9].itemID, Is.EqualTo(109));
            Assert.That(result.items[9].quantity, Is.EqualTo(10));
            Assert.That(Parse(Frame(0, 0)).resStatus, Is.Zero);
        }
        [Test] public void CountDoesNotExposeUnusedFixedSlots()
        {
            var packet = Frame(1, 1); Row(packet, 0, 42, 3, 9); Row(packet, 1, 666, 9, 0);
            Assert.That(Parse(packet).items.Length, Is.EqualTo(1));
            Assert.That(Parse(packet).items[0].slot, Is.EqualTo(9));
        }
        [Test] public void MalformedInventoryFramesAndRowsAreRejected()
        {
            var good = Frame(1, 1); Row(good, 0, 5, 2, 0);
            foreach (int n in new[] { 0, 2, 6, 7, 8 })
            { var bad = (byte[])good.Clone(); bad[n] ^= 1; Reject(bad); }
            Reject(Frame(1, 11)); Reject(Frame(0, 1)); Reject(Frame(2, 0));
            var shortFrame = new byte[80]; Array.Copy(good, shortFrame, 80); Reject(shortFrame);
            foreach (var item in new[] { new InventoryItem { itemID = 0, quantity = 1 },
                new InventoryItem { itemID = 1, quantity = 0 }, new InventoryItem { itemID = 1, quantity = 1, slot = 10 } })
            { var bad = Frame(1, 1); Row(bad, 0, item.itemID, item.quantity, item.slot); Reject(bad); }
            var duplicate = Frame(1, 2); Row(duplicate, 0, 1, 2, 0); Row(duplicate, 1, 2, 2, 0); Reject(duplicate);
        }
        [Test] public void RequestIsHeaderOnlyAndMalformedSimulationFlagCannotBypassHandler()
        {
            var builder = typeof(Packet).Assembly.GetType("ClientCore.PacketHelper.PacketBuilder");
            var req = (byte[])builder.GetMethod("CreateInventoryRequestPacket", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, null);
            Assert.That(req.Length, Is.EqualTo(8)); Assert.That(req[6], Is.EqualTo(20));
            Assert.That(BitConverter.ToUInt32(req, 2), Is.EqualTo(8));
            var handler = Activator.CreateInstance(typeof(Packet).Assembly.GetType("ClientCore.Network.Handler"), true);
            var invalid = Frame(1, 0); invalid[7] = 1;
            Assert.Throws<TargetInvocationException>(() => handler.GetType().GetMethod("HandleResponse").Invoke(handler, new object[] { invalid }));
        }
        [Test] public void PendingRequestIsSingleAndDataAndEventWaitForDispatcher()
        {
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnInventoryReceived += ok => { Assert.That(ok, Is.True); events++; };
                f.Model.RequestInventory(); Assert.Throws<InvalidOperationException>(() => f.Model.RequestInventory());
                f.Model.InventoryReceived(1, new[] { new InventoryItem { itemID = 7, quantity = 3, slot = 2 } });
                Assert.That(f.Data.GetInventory().Count, Is.Zero); Assert.That(events, Is.Zero);
                f.Drain(); Assert.That(events, Is.EqualTo(1)); Assert.That(f.Network.Requests, Is.EqualTo(1));
                var snapshot = f.Data.GetInventory(); snapshot.Clear();
                Assert.That(f.Data.GetInventory()[0].Quantity, Is.EqualTo(3), "UI cannot mutate the shared inventory.");
            }
        }
        [Test] public void FailedReplyClearsPreviousRowsAndAllowsExplicitRefresh()
        {
            using (var f = new Fixture())
            {
                f.Model.RequestInventory(); f.Model.InventoryReceived(1, new[] { new InventoryItem { itemID = 8, quantity = 1 } }); f.Drain();
                bool? status = null; f.Model.OnInventoryReceived += ok => status = ok;
                f.Model.RequestInventory(); Assert.That(f.Data.GetInventory().Count, Is.Zero);
                f.Model.InventoryReceived(0, Array.Empty<InventoryItem>()); f.Drain(); Assert.That(status, Is.False);
                f.Model.RequestInventory(); Assert.That(f.Network.Requests, Is.EqualTo(3));
            }
        }
        [Test] public void UnsolicitedDuplicateAndDisposedQueuedReplyDoNotPublish()
        {
            var f = new Fixture(); int events = 0; f.Model.OnInventoryReceived += _ => events++;
            f.Model.InventoryReceived(1, Array.Empty<InventoryItem>()); f.Drain(); Assert.That(events, Is.Zero);
            f.Model.RequestInventory(); f.Model.InventoryReceived(1, Array.Empty<InventoryItem>()); f.Drain();
            f.Model.InventoryReceived(1, Array.Empty<InventoryItem>()); f.Drain(); Assert.That(events, Is.EqualTo(1));
            f.Model.RequestInventory(); f.Model.InventoryReceived(1, new[] { new InventoryItem { itemID = 99, quantity = 1 } });
            f.Dispose(); f.Drain(); Assert.That(events, Is.EqualTo(1)); Assert.That(f.Data.GetInventory().Count, Is.Zero);
            Assert.Throws<InvalidOperationException>(() => f.Model.RequestInventory());
        }
        [Test] public void ConnectionFailureInvalidatesQueuedInventoryReply()
        {
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnInventoryReceived += _ => events++;
                f.Model.RequestInventory(); f.Model.InventoryReceived(1, new[] { new InventoryItem { itemID = 8, quantity = 1 } });
                f.Model.Error("fixture close"); f.Drain(); Assert.That(events, Is.Zero);
                Assert.That(f.Data.GetInventory().Count, Is.Zero);
            }
        }
        private static InventoryResBody Parse(byte[] p) => (InventoryResBody)typeof(Packet).GetMethod("DeserializeInventory", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { Packet.ExtractHeader(p), p });
        private static void Reject(byte[] p) => Assert.Throws<TargetInvocationException>(() => Parse(p));
        private static byte[] Frame(byte status, ushort count)
        { var p = new byte[81]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes(81U).CopyTo(p, 2); p[6] = 21; p[8] = status; BitConverter.GetBytes(count).CopyTo(p, 9); return p; }
        private static void Row(byte[] p, int i, uint id, ushort quantity, byte slot)
        { int n = 11 + 7 * i; BitConverter.GetBytes(id).CopyTo(p, n); BitConverter.GetBytes(quantity).CopyTo(p, n + 4); p[n + 6] = slot; }
        private sealed class Fixture : IMainThreadDispatcher, IDisposable
        {
            public readonly FakeNetwork Network = new FakeNetwork(); public readonly ViewModel Model; public readonly IViewDataUI Data;
            private readonly Queue<Action> jobs = new Queue<Action>();
            public Fixture()
            {
                var data = Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData"), true); Data = (IViewDataUI)data;
                Model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.NonPublic | BindingFlags.Instance, null, new object[] { Network, data, new NullLogger(), this }, null);
                Model.ZoneChange(0); Model.ZoneChageReceived(1, 1, 1, 1, 0, 0); Drain();
            }
            public void Post(Action job) => jobs.Enqueue(job);
            public void Drain() { while (jobs.Count > 0) jobs.Dequeue()(); }
            public void Dispose() => Model.Dispose();
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string text) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public int Requests; public void RequestInventory() => Requests++;
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { }
            public void Chat(string message, byte scope, ulong target) { } public void Profiles(uint[] ids) { } public void Action(byte dir, float x, float y, byte slot) { }
            public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void Log(string text) { }
        }
    }
}
