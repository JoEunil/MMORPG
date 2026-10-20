using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;

namespace C02.Tests
{
    public sealed class C06ChatPacketTests
    {
        public static string RunPureChecks()
        {
            var t = new C06ChatPacketTests();
            t.BroadcastUsesTenFixedSlotsAndEachOffset();
            t.WhisperUsesProfileIdAndPreservesRawBody();
            t.MalformedBroadcastsAndWhispersAreRejected();
            t.ProfilePacketsAreVariableAndCountsAreBounded();
            t.ProfileIdsNamesAndUtf8AreValidated();
            t.SendUsesUtf8BytesWithoutNul();
            t.MissingProfilesFallbackImmediatelyAndRetryOnlyAfterNewVersion();
            t.VersionRefreshRejectsLateLowerResponse();
            t.RequestsSplitAtFiftyAndDisposedCallbacksAreSuppressed();
            t.ProfileDeadlineDoesNotLetLateEmptyOrLowerReplyAcknowledgeNewBatch();
            return "C06 wire/profile: 10/10 passed; no socket, services, Play or input.";
        }
        [Test] public void BroadcastUsesTenFixedSlotsAndEachOffset()
        {
            var p = Broadcast("first한글", 2);
            Entity(p, 0, 101, 5, 6, 77);
            Entity(p, 1, 102, 0, 5, 88);
            var messages = Parse<Packet.Message[]>("DeserializeChatBatch", p);
            Assert.That(messages.Length, Is.EqualTo(2));
            Assert.That(messages[0].message, Is.EqualTo("한글"));
            Assert.That(messages[0].senderProfileId, Is.EqualTo(77));
            Assert.That(messages[1].message, Is.EqualTo("first"));
            Assert.That(messages[1].senderID, Is.EqualTo(102));
        }
        [Test] public void WhisperUsesProfileIdAndPreservesRawBody()
        {
            var p = Frame(14, 26); Put(p, 8, (ushort)4); Put(p, 10, 900UL); Put(p, 18, 12U);
            Encoding.UTF8.GetBytes("a\0bc").CopyTo(p, 22);
            var m = Parse<Packet.Message>("DeserializeChatWhisper", p);
            Assert.That(m.senderProfileId, Is.EqualTo(12));
            Assert.That(m.scope, Is.EqualTo(2));
            Assert.That(m.message, Is.EqualTo("a\0bc"));
        }
        [Test] public void MalformedBroadcastsAndWhispersAreRejected()
        {
            var p = Broadcast("ok", 1); Entity(p, 0, 1, 0, 2, 1);
            var count = (byte[])p.Clone(); Put(count, 8, (ushort)11); Reject("DeserializeChatBatch", count);
            var range = (byte[])p.Clone(); Put(range, 21, (ushort)2); Reject("DeserializeChatBatch", range);
            var total = (byte[])p.Clone(); Put(total, 10, (ushort)3); Reject("DeserializeChatBatch", total);
            var flags = (byte[])p.Clone(); flags[7] = 1; Reject("DeserializeChatBatch", flags);
            var scope = (byte[])p.Clone(); scope[12] = 3; Reject("DeserializeChatBatch", scope);
            var utf = (byte[])p.Clone(); utf[173] = 0xc3; utf[174] = 0x28; Reject("DeserializeChatBatch", utf);
            Reject("DeserializeChatBatch", Frame(15, 172));
            var whisper = Frame(14, 22); Put(whisper, 8, (ushort)1); Reject("DeserializeChatWhisper", whisper);
            var magic = (byte[])p.Clone(); magic[0] = 0; Reject("DeserializeChatBatch", magic);
        }
        [Test] public void ProfilePacketsAreVariableAndCountsAreBounded()
        {
            Assert.That(Parse<ProfileEntry[]>("DeserializeProfiles", Profiles()).Length, Is.Zero);
            var rows = Parse<ProfileEntry[]>("DeserializeProfiles", Profiles(new ProfileEntry { profileId = 3, version = 4, name = "이름" }, new ProfileEntry { profileId = 5, version = 6, name = "Other" }));
            Assert.That(rows[0].name, Is.EqualTo("이름")); Assert.That(rows[1].version, Is.EqualTo(6));
            var bad = Frame(25, 10); Put(bad, 8, (ushort)51); Reject("DeserializeProfiles", bad);
            var truncated = Frame(25, 49); Put(truncated, 8, (ushort)1); Reject("DeserializeProfiles", truncated);
            var request = (byte[])Call("SerializeProfileRequest", new uint[] { 3, 5 });
            Assert.That(request.Length, Is.EqualTo(18)); Assert.That(request[6], Is.EqualTo(24)); Assert.That(request[7], Is.Zero);
            Assert.That(BitConverter.ToUInt16(request, 8), Is.EqualTo(2)); Assert.That(BitConverter.ToUInt32(request, 14), Is.EqualTo(5));
            Assert.Throws<TargetInvocationException>(() => Call("SerializeProfileRequest", Array.Empty<uint>()));
            Assert.Throws<TargetInvocationException>(() => Call("SerializeProfileRequest", new uint[51]));
        }
        [Test] public void ProfileIdsNamesAndUtf8AreValidated()
        {
            var row = new ProfileEntry { profileId = 3, version = 1, name = "a" };
            Reject("DeserializeProfiles", Profiles(row, row));
            var id = Profiles(row); Put(id, 10, 0U); Reject("DeserializeProfiles", id);
            var utf = Profiles(row); utf[18] = 0xff; Reject("DeserializeProfiles", utf);
            var noNul = Profiles(row); for (int i = 18; i < 50; i++) noNul[i] = 65; Reject("DeserializeProfiles", noNul);
        }
        [Test] public void SendUsesUtf8BytesWithoutNul()
        {
            string text = new string('가', 42) + "ab";
            var p = Send(text); Assert.That(p.Length, Is.EqualTo(147)); Assert.That(BitConverter.ToUInt16(p, 9), Is.EqualTo(128));
            Assert.That(p[147 - 1], Is.EqualTo((byte)'b'));
            foreach (string bad in new[] { text + "c", " ", "a\0b", "a\nb", "a\rb", "\ud800" })
                Assert.Throws<TargetInvocationException>(() => Send(bad));
        }
        [Test] public void MissingProfilesFallbackImmediatelyAndRetryOnlyAfterNewVersion()
        {
            using (var f = new Fixture())
            {
                string name = null; int events = 0;
                f.Model.OnChatReceived += (_, id, n, _) => { Assert.That(id, Is.EqualTo(123)); name = n; events++; };
                f.Model.ChatReceived(new Packet.Message { senderID = 123, senderProfileId = 7, message = "hello" }); f.Drain();
                Assert.That(events, Is.EqualTo(1)); Assert.That(name, Is.EqualTo("플레이어 #123")); Assert.That(f.Network.Requests.Count, Is.EqualTo(1));
                f.Model.ProfilesReceived(Array.Empty<ProfileEntry>()); f.Drain();
                f.Expire();
                f.Model.ChatReceived(new Packet.Message { senderID = 123, senderProfileId = 7 }); f.Drain();
                Assert.That(f.Network.Requests.Count, Is.EqualTo(1));
                f.Model.FullReceived(1, 1, 1, new[] { new FullStateField { zoneInternalID = 8, profileId = 7, profileVersion = 1 } }); f.Drain();
                Assert.That(f.Network.Requests.Count, Is.EqualTo(2));
                f.Model.ProfilesReceived(new[] { new ProfileEntry { profileId = 7, version = 1, name = "Actual" } }); f.Drain();
                f.Model.ChatReceived(new Packet.Message { senderID = 123, senderProfileId = 7 }); f.Drain(); Assert.That(name, Is.EqualTo("Actual"));
            }
        }
        [Test] public void VersionRefreshRejectsLateLowerResponse()
        {
            using (var f = new Fixture())
            {
                string name = null; f.Model.OnChatReceived += (_, _, n, _) => name = n;
                f.Model.FullReceived(1, 1, 1, new[] { new FullStateField { zoneInternalID = 8, profileId = 7, profileVersion = 1 } }); f.Drain();
                f.Model.ProfilesReceived(new[] { new ProfileEntry { profileId = 7, version = 1, name = "First" } }); f.Drain();
                f.Model.DeltaReceived(1, 1, 1, new[] { new DeltaUpdateField { zoneInternalID = 8, fieldID = 9, fieldVal = 3 } }); f.Drain();
                f.Model.ProfilesReceived(new[] { new ProfileEntry { profileId = 7, version = 2, name = "Late" } }); f.Drain();
                f.Model.ChatReceived(new Packet.Message { senderID = 123, senderProfileId = 7 }); f.Drain(); Assert.That(name, Is.EqualTo("First"));
                Assert.That(f.Network.Requests.Count, Is.EqualTo(2));
                f.Expire();
                f.Model.DeltaReceived(1, 1, 1, new[] { new DeltaUpdateField { zoneInternalID = 8, fieldID = 9, fieldVal = 4 } }); f.Drain();
                f.Model.ProfilesReceived(new[] { new ProfileEntry { profileId = 7, version = 4, name = "Newest" } }); f.Drain();
                f.Model.ChatReceived(new Packet.Message { senderID = 123, senderProfileId = 7 }); f.Drain(); Assert.That(name, Is.EqualTo("Newest"));
            }
        }
        [Test] public void RequestsSplitAtFiftyAndDisposedCallbacksAreSuppressed()
        {
            using (var f = new Fixture())
            {
                var states = Enumerable.Range(1, 51).Select(i => new FullStateField { zoneInternalID = (uint)i, profileId = (uint)i, profileVersion = 1 }).ToArray();
                f.Model.FullReceived(1, 1, 51, states); f.Drain(); Assert.That(f.Network.Requests[0].Length, Is.EqualTo(50));
                f.Model.ProfilesReceived(Array.Empty<ProfileEntry>()); f.Drain(); f.Expire();
                f.Model.ChatReceived(new Packet.Message { senderID = 3, senderProfileId = 1 }); f.Drain();
                Assert.That(f.Network.Requests[1], Is.EqualTo(new uint[] { 51 }));
                int events = 0; f.Model.OnChatReceived += (_, _, _, _) => events++;
                f.Model.ChatReceived(new Packet.Message { senderID = 4, senderProfileId = 1 }); f.Model.Dispose(); f.Drain(); Assert.That(events, Is.Zero);
            }
            using (var f = new Fixture())
            {
                int events = 0; f.Model.OnChatReceived += (_, _, _, _) => events++;
                f.Model.ChatReceived(new Packet.Message { senderID = 4, senderProfileId = 1 }); f.Model.Error("Disconnected"); f.Drain(); Assert.That(events, Is.Zero);
            }
        }
        [Test] public void ProfileDeadlineDoesNotLetLateEmptyOrLowerReplyAcknowledgeNewBatch()
        {
            using (var f = new Fixture())
            {
                f.Model.FullReceived(1, 1, 1, new[] { new FullStateField { zoneInternalID = 8, profileId = 7, profileVersion = 1 } }); f.Drain();
                f.Expire();
                f.Model.DeltaReceived(1, 1, 1, new[] { new DeltaUpdateField { zoneInternalID = 8, fieldID = 9, fieldVal = 2 } }); f.Drain();
                Assert.That(f.Network.Requests.Count, Is.EqualTo(2));
                f.Model.ProfilesReceived(Array.Empty<ProfileEntry>());
                f.Model.ProfilesReceived(new[] { new ProfileEntry { profileId = 7, version = 1, name = "Late" } }); f.Drain();
                Assert.That(((IDictionary<uint, uint>)typeof(ViewModel).GetField("_profilesPending", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(f.Model)).Count, Is.EqualTo(1));
                f.Model.ProfilesReceived(new[] { new ProfileEntry { profileId = 7, version = 2, name = "Current" } }); f.Drain();
                string name = null; f.Model.OnChatReceived += (_, _, n, _) => name = n;
                f.Model.ChatReceived(new Packet.Message { senderID = 3, senderProfileId = 7 }); f.Drain();
                Assert.That(name, Is.EqualTo("Current")); Assert.That(f.Network.Requests.Count, Is.EqualTo(2));
            }
        }
        private static object Call(string name, params object[] args) => typeof(Packet).GetMethod(name, BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, args);
        private static T Parse<T>(string name, byte[] p) => (T)Call(name, Packet.ExtractHeader(p), p);
        private static void Reject(string name, byte[] p) => Assert.Throws<TargetInvocationException>(() => Parse<object>(name, p));
        private static byte[] Send(string text) => (byte[])typeof(Packet).Assembly.GetType("ClientCore.PacketHelper.PacketBuilder").GetMethod("CreateChatPacket", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, new object[] { text, (byte)0, 0UL });
        private static byte[] Frame(byte op, int length) { var p = new byte[length]; Put(p, 0, (ushort)0xabcd); Put(p, 2, (uint)length); p[6] = op; return p; }
        private static byte[] Broadcast(string text, ushort count) { var raw = Encoding.UTF8.GetBytes(text); var p = Frame(15, 173 + raw.Length); Put(p, 8, count); Put(p, 10, (ushort)raw.Length); raw.CopyTo(p, 173); return p; }
        private static void Entity(byte[] p, int i, ulong chatId, ushort offset, ushort length, uint profile) { int n = 13 + i * 16; Put(p, n, chatId); Put(p, n + 8, offset); Put(p, n + 10, length); Put(p, n + 12, profile); }
        private static byte[] Profiles(params ProfileEntry[] rows) { var p = Frame(25, 10 + 40 * rows.Length); Put(p, 8, (ushort)rows.Length); for (int i = 0; i < rows.Length; i++) { Put(p, 10 + 40 * i, rows[i].profileId); Put(p, 14 + 40 * i, rows[i].version); Encoding.UTF8.GetBytes(rows[i].name).CopyTo(p, 18 + 40 * i); } return p; }
        private static void Put(byte[] p, int n, ushort value) => BitConverter.GetBytes(value).CopyTo(p, n);
        private static void Put(byte[] p, int n, uint value) => BitConverter.GetBytes(value).CopyTo(p, n);
        private static void Put(byte[] p, int n, ulong value) => BitConverter.GetBytes(value).CopyTo(p, n);
        private sealed class Fixture : IMainThreadDispatcher, IDisposable
        {
            public readonly FakeNetwork Network = new FakeNetwork(); public readonly ViewModel Model; private readonly Queue<Action> jobs = new Queue<Action>();
            public Fixture() { var data = Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData"), true); Model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.Instance | BindingFlags.NonPublic, null, new object[] { Network, data, new NullLogger(), this }, null); C08TestSession.Ready(Model, Drain); }
            public void Post(Action job) => jobs.Enqueue(job); public void Drain() { while (jobs.Count != 0) jobs.Dequeue()(); } public void Dispose() => Model.Dispose();
            public void Expire() => typeof(ViewModel).GetField("_profileRequestDeadline", BindingFlags.NonPublic | BindingFlags.Instance).SetValue(Model, 0L);
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string message) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public readonly List<uint[]> Requests = new List<uint[]>(); public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { } public void RequestInventory() { } public void Profiles(uint[] ids) => Requests.Add(ids);
            public Task Connect(string address, int port) => Task.CompletedTask; public void CharacterList() { } public void Enter(ulong id) { } public void Chat(string message, byte scope, ulong target) { } public void Action(byte dir, float x, float y, byte slot) { } public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void Log(string message) { }
        }
    }
}
