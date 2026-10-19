using System;
using System.Collections.Generic;
using System.Text;
using ClientCore;
using ClientCore.PacketHelper;
using NUnit.Framework;

namespace C01.Tests
{
    public sealed class C01CharacterListFixtureTests
    {
        [Test]
        public void SuccessfulSnapshotUsesServerCount_AndStatusZeroClearsPreviousCharacters()
        {
            var initializer = new Initializer();
            var dispatcher = new QueuedDispatcher();
            initializer.Initialize(new NullLogger(), dispatcher);

            try
            {
                var viewModel = initializer.GetViewModel();
                var net = (IViewModelNet)viewModel;
                byte receivedStatus = byte.MaxValue;
                ushort receivedCount = ushort.MaxValue;
                bool? success = null;
                viewModel.OnCharacterListResult += (status, count) =>
                {
                    receivedStatus = status;
                    receivedCount = count;
                };
                viewModel.OnCharacterListReceived += result => success = result;

                net.ChatacterListReceived(1, 2, CreateCharacters());
                dispatcher.Drain();
                var snapshot = initializer.GetViewData().GetCharList();
                Assert.That(snapshot.Item1, Is.EqualTo(2));
                Assert.That(snapshot.Item2, Has.Count.EqualTo(2));
                Assert.That(snapshot.Item2[0].Name, Is.EqualTo("First"));
                Assert.That(snapshot.Item2[1].Name, Is.EqualTo("Second"));
                Assert.That(receivedStatus, Is.EqualTo(1));
                Assert.That(receivedCount, Is.EqualTo(2));
                Assert.That(success, Is.True);

                net.ChatacterListReceived(0, 0, Array.Empty<CharacterInfo>());
                dispatcher.Drain();
                snapshot = initializer.GetViewData().GetCharList();
                Assert.That(snapshot.Item1, Is.Zero);
                Assert.That(snapshot.Item2, Is.Empty, "A failed response must not leave the previous character snapshot visible.");
                Assert.That(receivedStatus, Is.Zero);
                Assert.That(receivedCount, Is.Zero);
                Assert.That(success, Is.False);
            }
            finally
            {
                initializer.Dispose();
            }
        }

        private static CharacterInfo[] CreateCharacters()
        {
            var result = new CharacterInfo[10];
            result[0] = new CharacterInfo { characterID = 101, name = NameBytes("First"), level = 3 };
            result[1] = new CharacterInfo { characterID = 202, name = NameBytes("Second"), level = 7 };
            result[2] = new CharacterInfo { characterID = 303, name = NameBytes("OutsideCount"), level = 9 };
            return result;
        }

        private static byte[] NameBytes(string value)
        {
            var bytes = new byte[32];
            var encoded = Encoding.UTF8.GetBytes(value);
            Buffer.BlockCopy(encoded, 0, bytes, 0, Math.Min(encoded.Length, bytes.Length - 1));
            return bytes;
        }

        private sealed class QueuedDispatcher : IMainThreadDispatcher
        {
            private readonly List<Action> _jobs = new List<Action>();

            public void Post(Action job)
            {
                _jobs.Add(job);
            }

            public void Drain()
            {
                var jobs = _jobs.ToArray();
                _jobs.Clear();
                foreach (var job in jobs) job();
            }
        }

        private sealed class NullLogger : ILogger
        {
            public void Log(string message) { }
        }
    }
}
