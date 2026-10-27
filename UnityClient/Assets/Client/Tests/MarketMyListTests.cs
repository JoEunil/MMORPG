using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using ClientCore;
using ClientCore.Network;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;

namespace Mmorpg.Client.Tests
{
    public sealed class MarketMyListTests
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
                if (ending == "zone") WorldTestSession.BeginAutomaticTransition(f.Model, f.Drain);
                else if (ending == "disconnect") f.Model.Error("Fixture disconnect");
                else f.Model.Dispose();
                f.Model.BazaarMyListReceived(old, 1, Rows()); f.Drain();
                Assert.That(events, Is.Zero); Assert.That(f.Data.GetBazaar(), Is.Empty);
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarMyList());
                if (ending == "zone")
                {
                    WorldTestSession.CompleteAutomaticTransition(f.Model, f.Drain);
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

        [Test, Category("MarketClaim")]
        public void ClaimWireAndDispatcherKeepAmountCorrelationAndIgnoreCancelledOrDisconnectedResults()
        {
            var serialize = typeof(Packet).GetMethod("SerializeBazaarClaimRequest", BindingFlags.NonPublic | BindingFlags.Static);
            var request = (byte[])serialize.Invoke(null, new object[] { uint.MaxValue, ulong.MaxValue });
            Assert.That(request.Length, Is.EqualTo(20)); Assert.That(request[6], Is.EqualTo(30)); Assert.That(request[7], Is.Zero);
            Assert.That(BitConverter.ToUInt16(request, 0), Is.EqualTo(0xabcd));
            Assert.That(BitConverter.ToUInt32(request, 2), Is.EqualTo(20));
            Assert.That(BitConverter.ToUInt32(request, 8), Is.EqualTo(uint.MaxValue));
            Assert.That(BitConverter.ToUInt64(request, 12), Is.EqualTo(ulong.MaxValue));
            Assert.Throws<TargetInvocationException>(() => serialize.Invoke(null, new object[] { 0U, 1UL }));
            Assert.Throws<TargetInvocationException>(() => serialize.Invoke(null, new object[] { 1U, 0UL }));
            var deserialize = typeof(Packet).GetMethod("DeserializeBazaarClaim", BindingFlags.NonPublic | BindingFlags.Static);
            for (int invalid = 0; invalid < 10; invalid++)
            {
                var frame = ClaimFrame(1, 1, 1, ulong.MaxValue);
                if (invalid == 0) frame[0] = 0;
                if (invalid == 1) frame[6] = 29;
                if (invalid == 2) frame[7] = 1;
                if (invalid == 3) frame[2]++;
                if (invalid == 4) Array.Clear(frame, 8, 4);
                if (invalid == 5) Array.Clear(frame, 12, 8);
                if (invalid == 6) frame[20] = 5;
                if (invalid == 7) frame[20] = 4; // Failed results must never contain an amount.
                if (invalid == 8) Array.Resize(ref frame, 28);
                if (invalid == 9) Array.Resize(ref frame, 30);
                Assert.Throws<TargetInvocationException>(() => deserialize.Invoke(null, new object[] { Packet.ExtractHeader(frame), frame }), "boundary " + invalid);
            }
            using (var f = new Fixture())
            {
                var type = typeof(ViewModel).Assembly.GetType("ClientCore.Network.Handler");
                var handler = (IHandlerSock)Activator.CreateInstance(type, true);
                var tick = new FakeTick();
                type.GetMethod("Inject", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(handler, new object[] { f.Model, tick });
                var malformed = ClaimFrame(1, 1, 1, 1); malformed[7] = 1;
                Assert.Throws<ArgumentException>(() => handler.HandleResponse(malformed));
                malformed = ClaimFrame(1, 1, 4, 1);
                Assert.Throws<ArgumentException>(() => handler.HandleResponse(malformed));
                var results = new List<(ulong listing, byte status, ulong amount)>();
                f.Model.OnBazaarClaimReceived += (listing, status, amount) => results.Add((listing, status, amount));
                f.Model.RequestBazaarMyList();
                f.Model.BazaarMyListReceived(f.Network.MyListIDs[0], 1, Rows()); f.Drain();
                f.Model.RequestBazaarClaim(1); uint first = f.Network.ClaimIDs[0];
                Assert.That(f.Data.GetBazaar().Count, Is.EqualTo(1));
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarClaim(1));
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarMyList());
                f.Model.BazaarMyListReceived(first, 1, Array.Empty<BazaarListing>());
                handler.HandleResponse(ClaimFrame(first, 2, 1, ulong.MaxValue));
                handler.HandleResponse(ClaimFrame(first + 1, 1, 1, ulong.MaxValue)); f.Drain();
                Assert.That(results, Is.Empty);
                handler.HandleResponse(ClaimFrame(first, 1, 1, ulong.MaxValue));
                Assert.That(results, Is.Empty); f.Drain();
                Assert.That(results, Is.EqualTo(new[] { (1UL, (byte)1, ulong.MaxValue) }));
                Assert.That(f.Data.GetBazaar().Count, Is.EqualTo(1)); Assert.That(tick.Queued, Is.Zero);
                f.Model.RequestBazaarClaim(1); uint cancelled = f.Network.ClaimIDs[1];
                handler.HandleResponse(ClaimFrame(cancelled, 1, 1, 5));
                f.Model.CancelBazaar(); f.Model.RequestBazaarClaim(2);
                handler.HandleResponse(ClaimFrame(cancelled, 1, 1, 5)); f.Drain(); Assert.That(results.Count, Is.EqualTo(1));
                uint current = f.Network.ClaimIDs[2];
                handler.HandleResponse(ClaimFrame(current, 2, 4, 0)); f.Drain();
                Assert.That(results[1], Is.EqualTo((2UL, (byte)4, 0UL)));
                f.Model.RequestBazaarClaim(3); uint disconnected = f.Network.ClaimIDs[3];
                handler.HandleResponse(ClaimFrame(disconnected, 3, 1, 5));
                f.Model.Error("Fixture disconnect"); f.Drain();
                Assert.That(results.Count, Is.EqualTo(2));
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarClaim(3));
            }
        }

        [Test, Category("MarketPurchase")]
        public void PurchaseWireAndLifetimeSeparateBuyFromDeliveryAndRejectLateResults()
        {
            var serializeBuy = typeof(Packet).GetMethod("SerializeBazaarBuyRequest", BindingFlags.NonPublic | BindingFlags.Static);
            var serializeOutbox = typeof(Packet).GetMethod("SerializeBazaarCheckOutboxRequest", BindingFlags.NonPublic | BindingFlags.Static);
            var buyRequest = (byte[])serializeBuy.Invoke(null, new object[] { uint.MaxValue, ulong.MaxValue });
            Assert.That(buyRequest.Length, Is.EqualTo(20)); Assert.That(buyRequest[6], Is.EqualTo(32)); Assert.That(buyRequest[7], Is.Zero);
            Assert.That(BitConverter.ToUInt32(buyRequest, 8), Is.EqualTo(uint.MaxValue)); Assert.That(BitConverter.ToUInt64(buyRequest, 12), Is.EqualTo(ulong.MaxValue));
            var outboxRequest = (byte[])serializeOutbox.Invoke(null, new object[] { uint.MaxValue });
            Assert.That(outboxRequest.Length, Is.EqualTo(12)); Assert.That(outboxRequest[6], Is.EqualTo(34)); Assert.That(outboxRequest[7], Is.Zero);
            Assert.That(BitConverter.ToUInt32(outboxRequest, 8), Is.EqualTo(uint.MaxValue));
            Assert.Throws<TargetInvocationException>(() => serializeBuy.Invoke(null, new object[] { 1U, 0UL }));
            Assert.Throws<TargetInvocationException>(() => serializeOutbox.Invoke(null, new object[] { 0U }));
            var parseBuy = typeof(Packet).GetMethod("DeserializeBazaarBuy", BindingFlags.NonPublic | BindingFlags.Static);
            var parseOutbox = typeof(Packet).GetMethod("DeserializeBazaarCheckOutbox", BindingFlags.NonPublic | BindingFlags.Static);
            for (int invalid = 0; invalid < 11; invalid++)
            {
                var frame = BuyFrame(1, 1, 1, uint.MaxValue, uint.MaxValue, ulong.MaxValue);
                if (invalid == 0) frame[0] = 0;
                if (invalid == 1) frame[6] = 31;
                if (invalid == 2) frame[7] = 1;
                if (invalid == 3) frame[2]++;
                if (invalid == 4) Array.Clear(frame, 8, 4);
                if (invalid == 5) Array.Clear(frame, 12, 8);
                if (invalid == 6) frame[20] = 2;
                if (invalid == 7) frame[20] = 5;
                if (invalid == 8) Array.Clear(frame, 25, 4);
                if (invalid == 9) Array.Resize(ref frame, 36);
                if (invalid == 10) Array.Resize(ref frame, 38);
                Assert.Throws<TargetInvocationException>(() => parseBuy.Invoke(null, new object[] { Packet.ExtractHeader(frame), frame }), "buy boundary " + invalid);
            }
            for (int invalid = 0; invalid < 8; invalid++)
            {
                var frame = OutboxFrame(1, 1, 1, 1, 1);
                if (invalid == 0) frame[0] = 0;
                if (invalid == 1) frame[6] = 33;
                if (invalid == 2) frame[7] = 1;
                if (invalid == 3) Array.Clear(frame, 8, 4);
                if (invalid == 4) frame[12] = 2;
                if (invalid == 5) BitConverter.GetBytes(uint.MaxValue).CopyTo(frame, 13);
                if (invalid == 6) Array.Resize(ref frame, 24);
                if (invalid == 7) Array.Resize(ref frame, 26);
                Assert.Throws<TargetInvocationException>(() => parseOutbox.Invoke(null, new object[] { Packet.ExtractHeader(frame), frame }), "delivery boundary " + invalid);
            }
            using (var f = new Fixture())
            {
                var type = typeof(ViewModel).Assembly.GetType("ClientCore.Network.Handler");
                var handler = (IHandlerSock)Activator.CreateInstance(type, true);
                var tick = new FakeTick();
                type.GetMethod("Inject", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(handler, new object[] { f.Model, tick });
                var malformedBuy = BuyFrame(1, 1, 1, 1, 1, 1); malformedBuy[7] = 1;
                Assert.Throws<ArgumentException>(() => handler.HandleResponse(malformedBuy));
                var malformedOutbox = OutboxFrame(1, 1, 31, 0, 0);
                Assert.Throws<ArgumentException>(() => handler.HandleResponse(malformedOutbox));
                var buys = new List<(ulong listing, byte status, uint item, uint quantity, ulong amount)>();
                var deliveries = new List<(byte status, uint delivered, uint duplicated, uint blocked)>();
                f.Model.OnBazaarBuyReceived += (listing, status, item, quantity, amount) => buys.Add((listing, status, item, quantity, amount));
                f.Model.OnBazaarCheckOutboxReceived += (status, delivered, duplicated, blocked) => deliveries.Add((status, delivered, duplicated, blocked));
                f.Model.RequestBazaar(1, 0); var rows = Rows(); rows[0].status = 0;
                f.Model.BazaarReceived(f.Network.SearchIDs[0], 1, rows); f.Drain();
                f.Model.RequestBazaarBuy(1); uint first = f.Network.BuyIDs[0];
                Assert.That(f.Data.GetBazaar().Count, Is.EqualTo(1));
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarBuy(1));
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarCheckOutbox());
                f.Model.BazaarClaimReceived(first, 1, 1, 5);
                handler.HandleResponse(BuyFrame(first, 2, 1, 1, 1, 1));
                handler.HandleResponse(BuyFrame(first + 1, 1, 1, 1, 1, 1)); f.Drain();
                Assert.That(buys, Is.Empty);
                handler.HandleResponse(BuyFrame(first, 1, 1, uint.MaxValue, uint.MaxValue, ulong.MaxValue));
                Assert.That(buys, Is.Empty); f.Drain();
                Assert.That(buys[0], Is.EqualTo((1UL, (byte)1, uint.MaxValue, uint.MaxValue, ulong.MaxValue)));
                Assert.That(f.Data.GetInventory(), Is.Empty); // A committed purchase never inserts local items.
                f.Model.RequestBazaarCheckOutbox(); uint outbox = f.Network.OutboxIDs[0];
                handler.HandleResponse(BuyFrame(outbox, 1, 1, 1, 1, 1)); f.Drain(); Assert.That(buys.Count, Is.EqualTo(1));
                handler.HandleResponse(OutboxFrame(outbox, 0, 2, 1, 1));
                Assert.That(deliveries, Is.Empty); f.Drain();
                Assert.That(deliveries[0], Is.EqualTo(((byte)0, 2U, 1U, 1U))); // Failure retains an observed cache prefix.
                f.Model.RequestBazaarBuy(2); uint cancelled = f.Network.BuyIDs[1];
                handler.HandleResponse(BuyFrame(cancelled, 2, 1, 1, 1, 1));
                f.Model.CancelBazaar(); f.Model.RequestBazaarCheckOutbox();
                handler.HandleResponse(BuyFrame(cancelled, 2, 1, 1, 1, 1)); f.Drain(); Assert.That(buys.Count, Is.EqualTo(1));
                handler.HandleResponse(OutboxFrame(f.Network.OutboxIDs[1], 1, 0, 0, 0)); f.Drain();
                Assert.That(deliveries[1], Is.EqualTo(((byte)1, 0U, 0U, 0U)));
                f.Model.RequestBazaarBuy(3);
                handler.HandleResponse(BuyFrame(f.Network.BuyIDs[2], 3, 5, 0, 0, 0)); f.Drain();
                Assert.That(buys[1], Is.EqualTo((3UL, (byte)5, 0U, 0U, 0UL)));
                f.Model.RequestBazaarCheckOutbox();
                handler.HandleResponse(OutboxFrame(f.Network.OutboxIDs[2], 1, 1, 0, 0));
                f.Model.Error("Fixture disconnect"); f.Drain();
                Assert.That(deliveries.Count, Is.EqualTo(2)); Assert.That(tick.Queued, Is.Zero);
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarBuy(3));
                Assert.Throws<InvalidOperationException>(() => f.Model.RequestBazaarCheckOutbox());
            }
        }

        private static byte[] BuyFrame(uint requestID, ulong listingID, byte status, uint itemID, uint quantity, ulong amount)
        {
            var p = new byte[37]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes(37U).CopyTo(p, 2);
            p[6] = 33; BitConverter.GetBytes(requestID).CopyTo(p, 8); BitConverter.GetBytes(listingID).CopyTo(p, 12); p[20] = status;
            BitConverter.GetBytes(itemID).CopyTo(p, 21); BitConverter.GetBytes(quantity).CopyTo(p, 25); BitConverter.GetBytes(amount).CopyTo(p, 29); return p;
        }
        private static byte[] OutboxFrame(uint requestID, byte status, uint delivered, uint duplicated, uint blocked)
        {
            var p = new byte[25]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0); BitConverter.GetBytes(25U).CopyTo(p, 2);
            p[6] = 35; BitConverter.GetBytes(requestID).CopyTo(p, 8); p[12] = status;
            BitConverter.GetBytes(delivered).CopyTo(p, 13); BitConverter.GetBytes(duplicated).CopyTo(p, 17); BitConverter.GetBytes(blocked).CopyTo(p, 21); return p;
        }

        private static byte[] ClaimFrame(uint requestID, ulong listingID, byte status, ulong amount)
        {
            var p = new byte[29]; BitConverter.GetBytes((ushort)0xabcd).CopyTo(p, 0);
            BitConverter.GetBytes(29U).CopyTo(p, 2); p[6] = 31;
            BitConverter.GetBytes(requestID).CopyTo(p, 8); BitConverter.GetBytes(listingID).CopyTo(p, 12);
            p[20] = status; BitConverter.GetBytes(amount).CopyTo(p, 21); return p;
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
                WorldTestSession.Ready(Model, Drain);
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
            public readonly List<uint> ClaimIDs = new List<uint>();
            public readonly List<uint> BuyIDs = new List<uint>(); public readonly List<uint> OutboxIDs = new List<uint>();
            public void RequestBazaarBuy(uint requestID, ulong listingID) => BuyIDs.Add(requestID);
            public void RequestBazaarCheckOutbox(uint requestID) => OutboxIDs.Add(requestID);
            public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) => ClaimIDs.Add(requestID);
            public void RequestBazaarMyList(uint requestID) { MyListIDs.Add(requestID); if (FailMyList) throw new InvalidOperationException("Fixture send failure"); }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) => SearchIDs.Add(requestID);
            public void RequestBazaarRegister(uint requestID, byte inventorySlot, ushort quantity, ulong totalDiamondPrice) { }
            public void RequestWallet(uint requestID) { }
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { }
            public void Chat(string message, byte scope, ulong target) { } public void Profiles(uint[] ids) { } public void RequestInventory() { }
            public void Action(byte dir, float x, float y, byte slot, ulong ownershipEpoch) { } public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void ZoneResync(ulong ownershipEpoch) { } public void RespawnToTown(ulong ownershipEpoch) { } public void Log(string text) { }
        }
    }
}
