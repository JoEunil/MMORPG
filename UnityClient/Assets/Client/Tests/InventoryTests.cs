using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;

namespace Mmorpg.Client.Tests
{
    public sealed class InventoryTests
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
        [Test] public void ItemUseAndRewardWirePreservesFieldsAndRejectsMalformedFrames()
        {
            var serialize = typeof(Packet).GetMethod("SerializeItemUseRequest", BindingFlags.NonPublic | BindingFlags.Static);
            var request = (byte[])serialize.Invoke(null, new object[] { 0x100000002UL, 0x200000003UL, 42U });
            Assert.That(request.Length, Is.EqualTo(28)); Assert.That(request[6], Is.EqualTo(45));
            Assert.That(BitConverter.ToUInt64(request, 8), Is.EqualTo(0x100000002UL));
            Assert.That(BitConverter.ToUInt64(request, 16), Is.EqualTo(0x200000003UL));
            Assert.That(BitConverter.ToUInt32(request, 24), Is.EqualTo(42));
            Assert.Throws<TargetInvocationException>(() => serialize.Invoke(null, new object[] { 0UL, 1UL, 42U }));
            var response = ItemFrame(46, 35);
            BitConverter.GetBytes(0x100000002UL).CopyTo(response, 8);
            BitConverter.GetBytes(0x200000003UL).CopyTo(response, 16);
            BitConverter.GetBytes(42U).CopyTo(response, 24); response[28] = 4;
            BitConverter.GetBytes(ushort.MaxValue).CopyTo(response, 29);
            BitConverter.GetBytes(uint.MaxValue).CopyTo(response, 31);
            var parsed = ((ulong requestID, ulong epoch, uint itemID, byte status, ushort quantity, uint cooldown))ParseItem("DeserializeItemUse", response);
            Assert.That(parsed.requestID, Is.EqualTo(0x100000002UL)); Assert.That(parsed.epoch, Is.EqualTo(0x200000003UL));
            Assert.That(parsed.status, Is.EqualTo(4)); Assert.That(parsed.quantity, Is.EqualTo(ushort.MaxValue));
            Assert.That(parsed.cooldown, Is.EqualTo(uint.MaxValue));
            response[28] = 10;
            Assert.Throws<TargetInvocationException>(() => ParseItem("DeserializeItemUse", response));
            response[28] = 1; response[7] = 1;
            Assert.Throws<TargetInvocationException>(() => ParseItem("DeserializeItemUse", response));
            Assert.Throws<TargetInvocationException>(() => ParseItem("DeserializeItemUse", ItemFrame(46, 36)));
            var reward = ItemFrame(47, 17); BitConverter.GetBytes(42U).CopyTo(reward, 8);
            BitConverter.GetBytes((ushort)3).CopyTo(reward, 12); BitConverter.GetBytes((ushort)8).CopyTo(reward, 14); reward[16] = 1;
            var granted = ((uint itemID, ushort granted, ushort quantity, byte status))ParseItem("DeserializeItemReward", reward);
            Assert.That(granted.granted, Is.EqualTo(3)); Assert.That(granted.quantity, Is.EqualTo(8));
            reward[16] = 2;
            Assert.Throws<TargetInvocationException>(() => ParseItem("DeserializeItemReward", reward));
            Array.Clear(reward, 12, 2);
            Assert.That(((uint, ushort, ushort, byte))ParseItem("DeserializeItemReward", reward), Is.EqualTo((42U, (ushort)0, (ushort)8, (byte)2)));
        }

        [Test] public void ItemUseCorrelatesSingleRequestAndPreservesServerStatusWithoutInventingInventory()
        {
            using (var f = new Fixture())
            {
                int events = 0; byte receivedStatus = 0;
                f.Model.OnItemUseReceived += (id, epoch, item, status, quantity, cooldown) =>
                { events++; receivedStatus = status; Assert.That(quantity, Is.EqualTo(7)); Assert.That(cooldown, Is.EqualTo(12)); };
                Assert.That(f.Model.RequestItemUse(0), Is.False);
                Assert.That(f.Model.RequestItemUse(42), Is.True);
                ulong first = f.Network.ItemUseID;
                Assert.That(f.Model.RequestItemUse(42), Is.False);
                f.Model.ItemUseReceived(first + 1, 1, 42, 1, 7, 12);
                f.Model.ItemUseReceived(first, 2, 42, 1, 7, 12);
                f.Model.ItemUseReceived(first, 1, 43, 1, 7, 12); f.Drain(); Assert.That(events, Is.Zero);
                f.Model.ItemUseReceived(first, 1, 42, 4, 7, 12);
                Assert.That(events, Is.Zero); f.Drain(); Assert.That(events, Is.EqualTo(1)); Assert.That(receivedStatus, Is.EqualTo(4));
                Assert.That(f.Data.GetInventory(), Is.Empty);
                f.Model.ItemUseReceived(first, 1, 42, 1, 7, 12); f.Drain(); Assert.That(events, Is.EqualTo(1));
                Assert.That(f.Model.RequestItemUse(42), Is.True); Assert.That(f.Network.ItemUseID, Is.GreaterThan(first));
            }
        }

        [Test] public void ItemUseTransitionDisconnectAndDisposeInvalidateRepliesButInventoryStillCompletes()
        {
            using (var f = new Fixture())
            {
                int useEvents = 0, rewards = 0, inventoryEvents = 0;
                f.Model.OnItemUseReceived += (id, epoch, item, status, quantity, cooldown) => useEvents++;
                f.Model.OnItemRewardReceived += (item, granted, quantity, status) => rewards++;
                f.Model.OnInventoryReceived += _ => inventoryEvents++;
                f.Model.RequestInventory(); f.Model.RequestItemUse(42);
                ulong old = f.Network.ItemUseID;
                f.Model.ItemUseReceived(old, 1, 42, 1, 1, 0);
                f.Model.ZoneChageReceived(1, 2, 2, 2, 205, 10, 2, 2);
                Assert.That(f.Model.RequestItemUse(42), Is.False, "The receive/dispatcher window cannot send the old epoch.");
                f.Drain();
                Assert.That(useEvents, Is.Zero); Assert.That(f.Model.RequestItemUse(42), Is.False);
                f.Model.InventoryReceived(1, new[] { new InventoryItem { itemID = 42, quantity = 2 } });
                f.Drain(); Assert.That(inventoryEvents, Is.EqualTo(1));
                WorldTestSession.CompleteAutomaticTransition(f.Model, f.Drain);
                Assert.That(f.Model.RequestItemUse(42), Is.True);
                Assert.That(f.Network.ItemUseID, Is.GreaterThan(old)); Assert.That(f.Network.ItemUseEpoch, Is.EqualTo(2));
                f.Model.ItemUseReceived(old, 1, 42, 1, 1, 0); f.Drain(); Assert.That(useEvents, Is.Zero);
                f.Model.ItemRewardReceived(42, 1, 3, 1); Assert.That(rewards, Is.Zero); f.Drain(); Assert.That(rewards, Is.EqualTo(1));
                Assert.That(f.Data.GetInventory()[0].Quantity, Is.EqualTo(2));
                f.Model.ItemUseReceived(f.Network.ItemUseID, 2, 42, 1, 1, 0);
                f.Model.ItemRewardReceived(42, 1, 3, 1); f.Model.Error("fixture disconnect"); f.Drain();
                Assert.That(useEvents, Is.Zero); Assert.That(rewards, Is.EqualTo(1)); Assert.That(f.Model.RequestItemUse(42), Is.False);
            }
            var disposed = new Fixture(); int disposedEvents = 0;
            disposed.Model.OnItemUseReceived += (id, epoch, item, status, quantity, cooldown) => disposedEvents++;
            disposed.Model.RequestItemUse(42);
            disposed.Model.ItemUseReceived(disposed.Network.ItemUseID, 1, 42, 1, 0, 0);
            disposed.Dispose(); disposed.Drain(); Assert.That(disposedEvents, Is.Zero);
            Assert.That(disposed.Model.RequestItemUse(42), Is.False);
        }

        private static object ParseItem(string method, byte[] p) => typeof(Packet).GetMethod(method, BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { Packet.ExtractHeader(p), p });
        private static byte[] ItemFrame(byte opcode, uint length)
        { var p = new byte[length]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes(length).CopyTo(p, 2); p[6] = opcode; return p; }
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
                WorldTestSession.Ready(Model, Drain);
            }
            public void Post(Action job) => jobs.Enqueue(job);
            public void Drain() { while (jobs.Count > 0) jobs.Dequeue()(); }
            public void Dispose() => Model.Dispose();
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string text) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public int Requests; public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { } public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public ulong ItemUseID, ItemUseEpoch;
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { }
            public void RequestItemUse(ulong requestID, ulong ownershipEpoch, uint itemID) { ItemUseID = requestID; ItemUseEpoch = ownershipEpoch; }
            public void RequestInventory() => Requests++;
            public void RequestBazaarRegister(uint requestID, byte inventorySlot, ushort quantity, ulong totalDiamondPrice) { }
            public void RequestWallet(uint requestID) { }
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { }
            public void Chat(string message, byte scope, ulong target) { } public void Profiles(uint[] ids) { } public void Action(byte dir, float x, float y, byte slot, ulong ownershipEpoch) { }
            public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void ZoneResync(ulong ownershipEpoch) { } public void RespawnToTown(ulong ownershipEpoch) { } public void Log(string text) { }
        }
    }
}
