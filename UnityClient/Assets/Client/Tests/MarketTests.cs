using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;

namespace Mmorpg.Client.Tests
{
    public sealed class MarketTests
    {
        [Test]
        [Category("Equipment")]
        public void EquipmentWireAndLifecycleRejectMalformedLateAndCanceledAuthorityWithoutRetryingWrites()
        {
            var serializeGet = typeof(Packet).GetMethod("SerializeEquipmentRequest", BindingFlags.NonPublic | BindingFlags.Static);
            var serializeSet = typeof(Packet).GetMethod("SerializeEquipWeaponRequest", BindingFlags.NonPublic | BindingFlags.Static);
            var parse = typeof(Packet).GetMethod("DeserializeEquipment", BindingFlags.NonPublic | BindingFlags.Static);
            var get = (byte[])serializeGet.Invoke(null, new object[] { 5UL, 7UL });
            var set = (byte[])serializeSet.Invoke(null, new object[] { 8UL, 7UL, 3U });
            Assert.That(get.Length, Is.EqualTo(24)); Assert.That(get[6], Is.EqualTo(53));
            Assert.That(set.Length, Is.EqualTo(28)); Assert.That(set[6], Is.EqualTo(54));
            Assert.That(BitConverter.ToUInt64(set, 16), Is.EqualTo(7)); Assert.That(BitConverter.ToUInt32(set, 24), Is.EqualTo(3));
            Assert.Throws<TargetInvocationException>(() => serializeSet.Invoke(null, new object[] { 1UL, 1UL, 4U }));
            var reply = new byte[36]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(reply, 0);
            BitConverter.GetBytes(36U).CopyTo(reply, 2); reply[6] = 55;
            BitConverter.GetBytes(8UL).CopyTo(reply, 8); BitConverter.GetBytes(7UL).CopyTo(reply, 16);
            reply[24] = 1; reply[25] = 1; BitConverter.GetBytes(3U).CopyTo(reply, 26);
            BitConverter.GetBytes((ushort)10).CopyTo(reply, 30); BitConverter.GetBytes((ushort)2).CopyTo(reply, 32);
            BitConverter.GetBytes((ushort)12).CopyTo(reply, 34);
            var result = ((ulong requestID, ulong epoch, byte operation, byte status, uint weapon, ushort basis, ushort bonus, ushort total))
                parse.Invoke(null, new object[] { Packet.ExtractHeader(reply), reply });
            Assert.That(result.weapon, Is.EqualTo(3)); Assert.That(result.total, Is.EqualTo(12));
            BitConverter.GetBytes(ushort.MaxValue).CopyTo(reply, 30); BitConverter.GetBytes(ushort.MaxValue).CopyTo(reply, 34);
            result = ((ulong, ulong, byte, byte, uint, ushort, ushort, ushort))parse.Invoke(null, new object[] { Packet.ExtractHeader(reply), reply });
            Assert.That(result.total, Is.EqualTo(ushort.MaxValue));
            reply[25] = 2;
            Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(reply), reply }));
            reply[25] = 1; reply[34] = 13;
            Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(reply), reply }));
            using (var f = new Fixture())
            {
                typeof(ViewModel).GetField("_selfHP", BindingFlags.NonPublic | BindingFlags.Instance).SetValue(f.Model, 100);
                int callbacks = 0; byte lastOperation = 0;
                f.Model.OnEquipmentReceived += (op, status, weapon, basis, bonus, total) => { callbacks++; lastOperation = op; };
                f.Model.RequestEquipment(); ulong old = f.Network.EquipmentGets[0];
                f.Model.EquipmentReceived(old, 1, 0, 1, 0, 10, 0, 10);
                Assert.That(f.Model.RequestEquipWeapon(3), Is.True);
                Assert.That(f.Model.RequestEquipWeapon(0), Is.False);
                f.Drain(); Assert.That(callbacks, Is.Zero, "A read sampled before a write cannot replace its state.");
                f.Model.EquipmentReceived(f.Network.EquipmentSets[0], 2, 1, 1, 3, 10, 2, 12); f.Drain();
                Assert.That(callbacks, Is.Zero);
                f.Model.EquipmentReceived(f.Network.EquipmentSets[0], 1, 1, 1, 3, 10, 2, 12); f.Drain();
                Assert.That(callbacks, Is.EqualTo(1)); Assert.That(lastOperation, Is.EqualTo(1));
                f.Model.EquipmentReceived(f.Network.EquipmentSets[0], 1, 1, 1, 3, 10, 2, 12); f.Drain();
                Assert.That(callbacks, Is.EqualTo(1)); Assert.That(f.Network.EquipmentSets.Count, Is.EqualTo(1));
                f.Model.RequestEquipment(); ulong beforeTravel = f.Network.EquipmentGets[1];
                f.Model.EquipmentReceived(beforeTravel, 1, 0, 1, 3, 10, 2, 12);
                WorldTestSession.BeginAutomaticTransition(f.Model, f.Drain); Assert.That(callbacks, Is.EqualTo(1));
                WorldTestSession.CompleteAutomaticTransition(f.Model, f.Drain);
                f.Model.RequestEquipment();
                f.Model.EquipmentReceived(f.Network.EquipmentGets[2], 2, 0, 1, 3, ushort.MaxValue, 2, ushort.MaxValue); f.Drain();
                Assert.That(callbacks, Is.EqualTo(2)); Assert.That(lastOperation, Is.Zero);
            }
        }

        [Test]
        [Category("ReceiptInventory")]
        public void ReceiptListKeepsProcessingSeparateRejectsInvalidRowsAndIgnoresCanceledResponses()
        {
            var serialize = typeof(Packet).GetMethod("SerializeBazaarOutboxListRequest", BindingFlags.NonPublic | BindingFlags.Static);
            var request = (byte[])serialize.Invoke(null, new object[] { 7U });
            Assert.That(request.Length, Is.EqualTo(12)); Assert.That(request[6], Is.EqualTo(51));
            var parse = typeof(Packet).GetMethod("DeserializeBazaarOutboxList", BindingFlags.NonPublic | BindingFlags.Static);
            var frame = new byte[41];
            BitConverter.GetBytes((ushort)0xabcd).CopyTo(frame, 0); BitConverter.GetBytes(41U).CopyTo(frame, 2);
            frame[6] = 52; BitConverter.GetBytes(7U).CopyTo(frame, 8); frame[12] = 1;
            BitConverter.GetBytes((ushort)2).CopyTo(frame, 13); BitConverter.GetBytes((ushort)1).CopyTo(frame, 15);
            BitConverter.GetBytes(0x100000001UL).CopyTo(frame, 17); BitConverter.GetBytes(99UL).CopyTo(frame, 25);
            BitConverter.GetBytes(1U).CopyTo(frame, 33); BitConverter.GetBytes(4U).CopyTo(frame, 37);
            var parsed = ((uint requestID, byte status, ushort processingCount, BazaarOutboxItem[] rows))parse.Invoke(null, new object[] { Packet.ExtractHeader(frame), frame });
            Assert.That(parsed.processingCount, Is.EqualTo(2)); Assert.That(parsed.rows[0].eventID, Is.EqualTo(0x100000001UL));
            frame[12] = 2;
            Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(frame), frame }));
            frame[12] = 1; BitConverter.GetBytes(65536U).CopyTo(frame, 37);
            Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(frame), frame }));
            using (var f = new Fixture())
            {
                int callbacks = 0; BazaarOutboxItem[] observed = null;
                f.Model.OnBazaarOutboxListReceived += (status, processing, rows) => { callbacks++; observed = rows; };
                f.Model.RequestBazaarOutboxList(); uint old = f.Network.IDs[0];
                f.Model.BazaarOutboxListReceived(old, 1, 2, parsed.rows);
                f.Model.CancelBazaar(); f.Drain(); Assert.That(callbacks, Is.Zero);
                f.Model.RequestBazaarOutboxList();
                f.Model.BazaarOutboxListReceived(old, 1, 2, parsed.rows); f.Drain(); Assert.That(callbacks, Is.Zero);
                f.Model.BazaarOutboxListReceived(f.Network.IDs[1], 1, 2, parsed.rows);
                parsed.rows[0].itemID = 99; f.Drain();
                Assert.That(callbacks, Is.EqualTo(1)); Assert.That(observed[0].itemID, Is.EqualTo(1));
            }
        }

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
        public void RegisterRequestKeepsSlotQuantityAndFullWidthDiamondPrice()
        {
            var serialize = typeof(Packet).GetMethod("SerializeBazaarRegisterRequest", BindingFlags.NonPublic | BindingFlags.Static);
            var bytes = (byte[])serialize.Invoke(null, new object[] { 91U, (byte)7, (ushort)513, 0x100000002UL });
            Assert.That(bytes.Length, Is.EqualTo(23)); Assert.That(bytes[6], Is.EqualTo(40));
            Assert.That(BitConverter.ToUInt32(bytes, 8), Is.EqualTo(91));
            Assert.That(bytes[12], Is.EqualTo(7)); Assert.That(BitConverter.ToUInt16(bytes, 13), Is.EqualTo(513));
            Assert.That(BitConverter.ToUInt64(bytes, 15), Is.EqualTo(0x100000002UL));
            Assert.Throws<TargetInvocationException>(() => serialize.Invoke(null, new object[] { 0U, (byte)0, (ushort)1, 1UL }));
        }

        [Test]
        public void WalletResponsesPreserveUlongBalancesAndRejectMalformedFrames()
        {
            var gold = WalletFrame(43, 1, 0x100000002UL);
            var parseGold = typeof(Packet).GetMethod("DeserializeWalletGold", BindingFlags.NonPublic | BindingFlags.Static);
            var parsed = ((uint requestID, byte status, ulong gold))parseGold.Invoke(null, new object[] { Packet.ExtractHeader(gold), gold });
            Assert.That(parsed.requestID, Is.EqualTo(23)); Assert.That(parsed.status, Is.EqualTo(1));
            Assert.That(parsed.gold, Is.EqualTo(0x100000002UL));

            var diamond = WalletFrame(44, 1, ulong.MaxValue);
            var parseDiamond = typeof(Packet).GetMethod("DeserializeWalletDiamond", BindingFlags.NonPublic | BindingFlags.Static);
            var parsedDiamond = ((uint requestID, byte status, ulong diamond))parseDiamond.Invoke(null, new object[] { Packet.ExtractHeader(diamond), diamond });
            Assert.That(parsedDiamond.diamond, Is.EqualTo(ulong.MaxValue));
            diamond[12] = 2;
            Assert.Throws<TargetInvocationException>(() => parseDiamond.Invoke(null, new object[] { Packet.ExtractHeader(diamond), diamond }));
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
                f.Model.RequestBazaar(1, 0);
                WorldTestSession.BeginAutomaticTransition(f.Model, f.Drain);
                f.Model.BazaarReceived(f.Network.IDs[0], 1, Rows()); f.Drain(); Assert.That(events, Is.Zero);
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaar(1, 0));
                WorldTestSession.CompleteAutomaticTransition(f.Model, f.Drain);
                f.Model.RequestBazaar(1, 0); f.Model.BazaarReceived(f.Network.IDs[1], 1, Rows()); f.Model.Dispose(); f.Drain();
                Assert.That(events, Is.Zero); Assert.That(f.Data.GetBazaar(), Is.Empty);
            }
        }

        [Test]
        public void GoldRewardsCoalesceWalletRefreshAndRespectTimeoutTransitionAndDisconnect()
        {
            using (var f = new Fixture())
            {
                int results = 0;
                f.Model.OnWalletReceived += (_, __, ___, ____) => results++;
                f.Model.RequestWallet();
                uint old = f.Network.WalletIDs[0];
                f.Model.GoldRewardReceived(1); f.Model.GoldRewardReceived(1); f.Drain();
                Assert.That(f.Network.WalletIDs.Count, Is.EqualTo(1));
                f.Model.WalletGoldReceived(old, 1, 10); f.Model.WalletDiamondReceived(old, 1, 0); f.Drain();
                Assert.That(f.Network.WalletIDs.Count, Is.EqualTo(2));
                uint current = f.Network.WalletIDs[1];
                f.Model.WalletGoldReceived(old, 1, 0); f.Model.WalletDiamondReceived(old, 1, 0); f.Drain();
                Assert.That(results, Is.EqualTo(1));
                f.Model.GoldRewardReceived(1); f.Drain();
                var generation = typeof(ViewModel).GetField("_walletGeneration", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(f.Model);
                typeof(ViewModel).GetMethod("WalletTimedOut", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(f.Model, new object[] { current, generation });
                Assert.That(f.Network.WalletIDs.Count, Is.EqualTo(3));
                current = f.Network.WalletIDs[2];
                f.Model.WalletGoldReceived(current, 1, 30); f.Model.WalletDiamondReceived(current, 1, 0); f.Drain();
                f.Model.GoldRewardReceived(0); f.Drain();
                Assert.That(f.Network.WalletIDs.Count, Is.EqualTo(3));
                WorldTestSession.BeginAutomaticTransition(f.Model, f.Drain);
                f.Model.GoldRewardReceived(1); f.Drain();
                Assert.That(f.Network.WalletIDs.Count, Is.EqualTo(3));
                WorldTestSession.CompleteAutomaticTransition(f.Model, f.Drain);
                Assert.That(f.Network.WalletIDs.Count, Is.EqualTo(4));
                f.Model.Error("closed"); f.Model.GoldRewardReceived(1); f.Drain();
                Assert.That(f.Network.WalletIDs.Count, Is.EqualTo(4));
            }
            var frame = new byte[9];
            BitConverter.GetBytes((ushort)0xabcd).CopyTo(frame, 0);
            BitConverter.GetBytes(9U).CopyTo(frame, 2); frame[6] = 50; frame[8] = 1;
            var parse = typeof(Packet).GetMethod("DeserializeGoldReward", BindingFlags.NonPublic | BindingFlags.Static);
            Assert.That(parse.Invoke(null, new object[] { Packet.ExtractHeader(frame), frame }), Is.EqualTo((byte)1));
            frame[8] = 2;
            Assert.Throws<TargetInvocationException>(() => parse.Invoke(null, new object[] { Packet.ExtractHeader(frame), frame }));
            using (var f = new Fixture())
            {
                var handlerType = typeof(ViewModel).Assembly.GetType("ClientCore.Network.Handler");
                var handler = Activator.CreateInstance(handlerType, true);
                handlerType.GetMethod("Inject", BindingFlags.Instance | BindingFlags.NonPublic).Invoke(handler, new object[] { f.Model });
                var handle = handlerType.GetMethod("HandleResponse");
                var badFlags = (byte[])frame.Clone(); badFlags[8] = 1; badFlags[7] = 1;
                var badLength = (byte[])frame.Clone(); badLength[8] = 1; BitConverter.GetBytes(10U).CopyTo(badLength, 2);
                foreach (var malformed in new[] { frame, badFlags, badLength })
                {
                    var error = Assert.Throws<TargetInvocationException>(() => handle.Invoke(handler, new object[] { malformed }));
                    Assert.That(error.InnerException, Is.TypeOf<ArgumentException>(), "Malformed gold rewards must reach the socket disconnect boundary.");
                }
                f.Drain();
                Assert.That(f.Network.WalletIDs, Is.Empty);
            }
        }

        private static BazaarListing[] Rows() => new[] { new BazaarListing { listingID = 1, sellerCharacterID = 5011, itemID = 1, quantity = 2, price = ulong.MaxValue, registeredAt = ulong.MaxValue } };
        private static byte[] Request(uint id, ushort type, ushort page) => (byte[])typeof(Packet).GetMethod("SerializeBazaarRequest", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { id, type, page });
        private static byte[] WalletFrame(byte opcode, byte status, ulong balance)
        {
            var p = new byte[21]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes(21U).CopyTo(p, 2);
            p[6] = opcode; BitConverter.GetBytes(23U).CopyTo(p, 8); p[12] = status; BitConverter.GetBytes(balance).CopyTo(p, 13);
            return p;
        }
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
            public Fixture() { var data = Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData"), true); Data = (IViewDataUI)data; Model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.NonPublic | BindingFlags.Instance, null, new object[] { Network, data, new NullLogger(), this }, null); WorldTestSession.Ready(Model, Drain); }
            public void Post(Action job) => jobs.Enqueue(job); public void Drain() { while (jobs.Count != 0) jobs.Dequeue()(); } public void Dispose() => Model.Dispose();
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string text) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public readonly List<uint> IDs = new List<uint>(); public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarOutboxList(uint requestID) { IDs.Add(requestID); }
            public void RequestBazaarCheckOutbox(uint requestID) { } public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint id, ushort type, ushort page) => IDs.Add(id);
            public void RequestBazaarRegister(uint requestID, byte inventorySlot, ushort quantity, ulong totalDiamondPrice) { }
            public readonly List<uint> WalletIDs = new List<uint>();
            public void RequestWallet(uint requestID) => WalletIDs.Add(requestID);
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { }
            public void Chat(string message, byte scope, ulong target) { } public void Profiles(uint[] ids) { } public void RequestItemUse(ulong requestID, ulong ownershipEpoch, uint itemID) { }
            public readonly List<ulong> EquipmentGets = new List<ulong>(), EquipmentSets = new List<ulong>();
            public void RequestEquipment(ulong requestID, ulong ownershipEpoch) { EquipmentGets.Add(requestID); }
            public void RequestEquipWeapon(ulong requestID, ulong ownershipEpoch, uint itemID) { EquipmentSets.Add(requestID); }
            public void RequestParry(ulong requestID, ulong ownershipEpoch) { }
            public void RequestInventory() { }
            public void Action(byte dir, float x, float y, byte slot, ulong ownershipEpoch, float aimAngle) { } public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void ZoneResync(ulong ownershipEpoch) { } public void RespawnToTown(ulong ownershipEpoch) { } public void Log(string text) { }
        }
    }
}
