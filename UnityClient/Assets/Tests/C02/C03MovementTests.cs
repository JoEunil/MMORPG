using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using C02;
using ClientCore;
using ClientCore.Network;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;
using UnityEngine;

namespace C02.Tests
{
    // Pure calculations and detached ClientCore fakes. No scene, Play, input, or server session.
    public sealed class C03MovementTests
    {
        public static string RunPureChecks()
        {
            var tests = new C03MovementTests();
            tests.FirstMovementPacket_Is18BytesSkill255_AndLastValueWins();
            tests.InvalidMovementCannotReplacePendingValue_AndCancelClearsIt();
            tests.ActionSendFailure_ReachesConnectionFailure_AndStopsFutureTicks();
            tests.DisposeSuppressesPendingAction();
            tests.CandidatesStayWithinOneUnitAndZone_AndUseMovementDirections();
            tests.AuthoredHeroForward_FacesEachConfirmedServerDirection();
            return "C03 pure regression: 6/6 passed. No Play, scene state, input, socket, or runtime fixture.";
        }

        [Test]
        public void FirstMovementPacket_Is18BytesSkill255_AndLastValueWins()
        {
            var data = NewData();
            Assert.That(((IViewDataModel)data).GetActionState().Item5, Is.EqualTo(255));
            data.UpdateMove(0, 10, 11);
            data.UpdateMove(3, 11, 10);
            var state = ((IViewDataModel)data).GetActionState();
            Assert.That(state.Item1, Is.True);
            var packet = (byte[])typeof(ViewModel).Assembly.GetType("ClientCore.PacketHelper.PacketBuilder", true)
                .GetMethod("CreateActionPacket", BindingFlags.Static | BindingFlags.NonPublic)
                .Invoke(null, new object[] { state.Item2, state.Item3, state.Item4, state.Item5 });
            Assert.That(packet.Length, Is.EqualTo(18));
            Assert.That(BitConverter.ToUInt16(packet, 0), Is.EqualTo(0xABCD));
            Assert.That(BitConverter.ToUInt32(packet, 2), Is.EqualTo(18U));
            Assert.That(packet[6], Is.EqualTo(11));
            Assert.That(packet[7], Is.EqualTo(1));
            Assert.That(packet[8], Is.EqualTo(3));
            Assert.That(BitConverter.ToSingle(packet, 9), Is.EqualTo(11));
            Assert.That(BitConverter.ToSingle(packet, 13), Is.EqualTo(10));
            Assert.That(packet[17], Is.EqualTo(255));
            Assert.That(((IViewDataModel)data).GetActionState().Item1, Is.False);
        }

        [Test]
        public void InvalidMovementCannotReplacePendingValue_AndCancelClearsIt()
        {
            var data = NewData();
            data.UpdateMove(2, 9, 10);
            Assert.Throws<ArgumentException>(() => data.UpdateMove(4, 1, 2));
            Assert.Throws<ArgumentException>(() => data.UpdateMove(0, float.NaN, 2));
            Assert.Throws<ArgumentException>(() => data.UpdateMove(0, 1, float.PositiveInfinity));
            Assert.Throws<ArgumentException>(() => data.UpdateMove(0, float.NegativeInfinity, 2));
            var state = ((IViewDataModel)data).GetActionState();
            Assert.That(state.Item2, Is.EqualTo(2));
            Assert.That(state.Item3, Is.EqualTo(9));
            Assert.That(state.Item4, Is.EqualTo(10));
            data.UpdateMove(0, 9, 11);
            data.CancelMove();
            Assert.That(((IViewDataModel)data).GetActionState().Item1, Is.False);
        }

        [Test]
        public void ActionSendFailure_ReachesConnectionFailure_AndStopsFutureTicks()
        {
            var data = NewData();
            var dispatcher = new QueuedDispatcher();
            var socket = new FailingSocket();
            var network = (INetworkService)Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.Services.NetworkService", true),
                BindingFlags.Instance | BindingFlags.NonPublic, null, new object[] { socket, new NullLogger() }, null);
            var model = NewModel(data, network, dispatcher);
            int failures = 0;
            model.OnConnectionFailed += _ => failures++;
            data.UpdateMove(3, 11, 10);
            Assert.DoesNotThrow(() => model.SendUpdate());
            Assert.That(failures, Is.Zero, "Failure delivery must stay on the dispatcher.");
            data.UpdateMove(3, 12, 10);
            model.SendUpdate();
            Assert.That(socket.SendCount, Is.EqualTo(1), "No repeated ACTION after a send failure.");
            dispatcher.Drain();
            Assert.That(failures, Is.EqualTo(1));
            model.Dispose();
        }

        [Test]
        public void DisposeSuppressesPendingAction()
        {
            var data = NewData();
            var socket = new FailingSocket();
            var network = (INetworkService)Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.Services.NetworkService", true),
                BindingFlags.Instance | BindingFlags.NonPublic, null, new object[] { socket, new NullLogger() }, null);
            var model = NewModel(data, network, new QueuedDispatcher());
            data.UpdateMove(0, 10, 11);
            model.Dispose();
            model.SendUpdate();
            Assert.That(socket.SendCount, Is.Zero);
        }

        [Test]
        public void CandidatesStayWithinOneUnitAndZone_AndUseMovementDirections()
        {
            var origin = new Vector2(10, 10);
            var targets = new[] { new Vector2(10, C20WorldMap.ZoneSize + 5), new Vector2(10, -5), new Vector2(-5, 10), new Vector2(C20WorldMap.ZoneSize + 5, 10), new Vector2(C20WorldMap.ZoneSize, C20WorldMap.ZoneSize) };
            var expected = new byte[] { 0, 1, 2, 3, 3 };
            for (int i = 0; i < targets.Length; i++)
            {
                var args = new object[] { (ushort)1, origin, targets[i], Vector2.zero, (byte)0 };
                Assert.That(Candidate(args), Is.True);
                Assert.That(Vector2.Distance(origin, (Vector2)args[3]), Is.EqualTo(1).Within(0.00001));
                Assert.That((byte)args[4], Is.EqualTo(expected[i]));
                var repeated = new object[] { (ushort)1, origin, targets[i], Vector2.zero, (byte)0 };
                Candidate(repeated);
                Assert.That(repeated[3], Is.EqualTo(args[3]), "No accumulated distance without a new server origin.");
            }
            Assert.That(Candidate(new object[] { (ushort)1, origin, origin, Vector2.zero, (byte)0 }), Is.False);
            Assert.That(Candidate(new object[] { (ushort)1, origin, new Vector2(C20WorldMap.ZoneSize + 6, 10), Vector2.zero, (byte)0 }), Is.False);
            Assert.That(Candidate(new object[] { (ushort)5, origin, targets[0], Vector2.zero, (byte)0 }), Is.False);
            Assert.That(Candidate(new object[] { (ushort)1, new Vector2(-6, 10), origin, Vector2.zero, (byte)0 }), Is.False);
            Assert.That(Candidate(new object[] { (ushort)1, origin, new Vector2(float.NaN, 10), Vector2.zero, (byte)0 }), Is.False);
            Assert.That(Candidate(new object[] { (ushort)1, origin, new Vector2(10, float.PositiveInfinity), Vector2.zero, (byte)0 }), Is.False);
            var near = new object[] { (ushort)1, origin, new Vector2(10.2f, 10), Vector2.zero, (byte)0 };
            Assert.That(Candidate(near), Is.True);
            Assert.That((Vector2)near[3], Is.EqualTo(new Vector2(10.2f, 10)));
        }

        [Test]
        public void AuthoredHeroForward_FacesEachConfirmedServerDirection()
        {
            var expectedForward = new[] { Vector3.forward, Vector3.back, Vector3.left, Vector3.right };
            for (byte direction = 0; direction < 4; direction++)
            {
                var rotation = (Quaternion)typeof(C02WorldScreen).GetMethod("ServerFacing", BindingFlags.Static | BindingFlags.NonPublic)
                    .Invoke(null, new object[] { direction });
                Assert.That(Vector3.Distance(rotation * Vector3.back, expectedForward[direction]), Is.LessThan(0.00001));
            }
        }

        private static bool Candidate(object[] args) => (bool)typeof(C02WorldScreen)
            .GetMethod("TryMoveCandidate", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, args);

        private static IViewDataUI NewData() => (IViewDataUI)Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData", true), true);

        private static ViewModel NewModel(IViewDataUI data, INetworkService network, IMainThreadDispatcher dispatcher)
        {
            var model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.Instance | BindingFlags.NonPublic, null,
                new object[] { network, (IViewDataModel)data, new NullLogger(), dispatcher }, null);
            C08TestSession.Ready(model, ((QueuedDispatcher)dispatcher).Drain);
            return model;
        }

        private sealed class QueuedDispatcher : IMainThreadDispatcher
        {
            private readonly List<Action> jobs = new List<Action>();
            public void Post(Action job) => jobs.Add(job);
            public void Drain() { foreach (var job in jobs.ToArray()) job(); jobs.Clear(); }
        }

        private sealed class FailingSocket : ITCPSocket
        {
            public event Action<string> OnDisconnected { add { } remove { } }
            public int SendCount;
            public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { } public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void Send(byte[] binary) { if (binary[6] == 16) return; SendCount++; throw new InvalidOperationException("Detached test send failure."); }
            public void Dispose() { }
        }

        private sealed class NullLogger : ClientCore.ILogger { public void Log(string message) { } }
    }
}
