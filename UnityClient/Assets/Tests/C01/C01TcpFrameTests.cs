using System;
using System.Collections;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Threading.Tasks;
using ClientCore;
using ClientCore.Network;
using NUnit.Framework;
using UnityEngine.TestTools;

namespace C01.Tests
{
    public sealed class C01TcpFrameTests
    {
        private const int FrameLimit = 1024 * 1024;

        [UnityTest, Timeout(30000)]
        public IEnumerator SplitAndCoalescedAuthAndListFrames_AreDispatchedOnceInOrder()
        {
            var handler = new RecordingHandler();
            var disconnected = new TaskCompletionSource<string>(TaskCreationOptions.RunContinuationsAsynchronously);
            var socket = CreateSocket(handler);
            socket.OnDisconnected += message => disconnected.TrySetResult(message);
            var listener = new TcpListener(IPAddress.Loopback, 0);
            listener.Start();
            var endpoint = (IPEndPoint)listener.LocalEndpoint;
            var server = SendSplitThenCombined(listener, CreateAuthResponse(), CreateListResponse(1, 1));

            try
            {
                var connect = socket.Connect(IPAddress.Loopback.ToString(), endpoint.Port);
                yield return WaitFor(() => connect.IsCompleted, 5f);
                if (connect.IsFaulted) throw connect.Exception;
                yield return WaitFor(() => handler.Packets.Count == 2, 5f);
                Assert.That(handler.Packets.Count, Is.EqualTo(2));
                Assert.That(handler.Packets[0], Is.EqualTo(CreateAuthResponse()));
                Assert.That(handler.Packets[1], Is.EqualTo(CreateListResponse(1, 1)));
                yield return WaitFor(() => server.IsCompleted, 2f);
            }
            finally
            {
                socket.Dispose();
                listener.Stop();
            }
        }

        [UnityTest, Timeout(30000)]
        public IEnumerator InvalidC01Frames_DisconnectWithoutDispatch()
        {
            for (var kind = 0; kind < 8; kind++)
            {
                var validation = CheckInvalidFrame(kind);
                while (validation.MoveNext())
                    yield return validation.Current;
            }
        }

        [UnityTest, Timeout(30000)]
        public IEnumerator C08SplitAckAndCoalescedSnapshotCrossTheRealTcpBoundary()
        {
            var handler = new RecordingHandler();
            var socket = CreateSocket(handler);
            var listener = new TcpListener(IPAddress.Loopback, 0);
            listener.Start();
            var ack = Header(39, 17); ack[7] = 1;
            var full = Header(63, 7); full[7] = 1; full[8] = 1;
            BitConverter.GetBytes((ushort)1).CopyTo(full, 10);
            BitConverter.GetBytes(1UL).CopyTo(full, 12);
            var server = SendSplitThenCombined(listener, ack, full);
            try
            {
                var connect = socket.Connect(IPAddress.Loopback.ToString(), ((IPEndPoint)listener.LocalEndpoint).Port);
                yield return WaitFor(() => connect.IsCompleted, 5f);
                if (connect.IsFaulted) throw connect.Exception;
                yield return WaitFor(() => handler.Packets.Count == 2, 5f);
                Assert.That(handler.Packets[0], Is.EqualTo(ack));
                Assert.That(handler.Packets[1], Is.EqualTo(full));
                yield return WaitFor(() => server.IsCompleted, 2f);
            }
            finally { socket.Dispose(); listener.Stop(); }
        }

        private IEnumerator CheckInvalidFrame(int kind)
        {
            var handler = new RecordingHandler();
            var disconnected = new TaskCompletionSource<string>(TaskCreationOptions.RunContinuationsAsynchronously);
            var socket = CreateSocket(handler);
            socket.OnDisconnected += message => disconnected.TrySetResult(message);
            var listener = new TcpListener(IPAddress.Loopback, 0);
            listener.Start();
            var endpoint = (IPEndPoint)listener.LocalEndpoint;
            var frame = CreateInvalidFrame(kind);
            var server = SendFrame(listener, frame);

            try
            {
                var connect = socket.Connect(IPAddress.Loopback.ToString(), endpoint.Port);
                yield return WaitFor(() => connect.IsCompleted, 5f);
                if (connect.IsFaulted) throw connect.Exception;
                yield return WaitFor(() => disconnected.Task.IsCompleted, 5f);
                Assert.That(disconnected.Task.Result, Is.EqualTo("Invalid game server packet."), "Invalid frame case " + kind);
                Assert.That(handler.Packets, Is.Empty, "Invalid frame case " + kind);
                yield return WaitFor(() => server.IsCompleted, 2f);
            }
            finally
            {
                socket.Dispose();
                listener.Stop();
            }
        }

        private static IEnumerator WaitFor(Func<bool> condition, float timeoutSeconds)
        {
            var deadline = UnityEngine.Time.realtimeSinceStartup + timeoutSeconds;
            while (!condition() && UnityEngine.Time.realtimeSinceStartup < deadline)
                yield return null;
            Assert.That(condition(), Is.True, "Timed out waiting for loopback socket frame handling.");
        }

        private static Task SendSplitThenCombined(TcpListener listener, byte[] first, byte[] second)
        {
            return Task.Run(async () =>
            {
                using (var client = await listener.AcceptTcpClientAsync())
                using (var stream = client.GetStream())
                {
                    await stream.WriteAsync(first, 0, 2);
                    await stream.FlushAsync();
                    await Task.Delay(50);
                    var restAndNext = new byte[first.Length - 2 + second.Length];
                    Buffer.BlockCopy(first, 2, restAndNext, 0, first.Length - 2);
                    Buffer.BlockCopy(second, 0, restAndNext, first.Length - 2, second.Length);
                    await stream.WriteAsync(restAndNext, 0, restAndNext.Length);
                    await stream.FlushAsync();
                    await Task.Delay(100);
                }
            });
        }

        private static Task SendFrame(TcpListener listener, byte[] frame)
        {
            return Task.Run(async () =>
            {
                using (var client = await listener.AcceptTcpClientAsync())
                using (var stream = client.GetStream())
                {
                    await stream.WriteAsync(frame, 0, frame.Length);
                    await stream.FlushAsync();
                    await Task.Delay(100);
                }
            });
        }

        private static ITCPSocket CreateSocket(IHandlerSock handler)
        {
            var assembly = typeof(ITCPSocket).Assembly;
            var type = assembly.GetType("ClientCore.Network.TCPSocket", true);
            var instance = Activator.CreateInstance(type, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
                null, new object[] { handler, new NullLogger() }, null);
            return (ITCPSocket)instance;
        }

        private static byte[] CreateAuthResponse()
        {
            var frame = Header(9, 2);
            frame[8] = 1;
            return frame;
        }

        private static byte[] CreateListResponse(byte status, byte count)
        {
            var frame = Header(430, 4);
            frame[8] = status;
            frame[9] = count;
            return frame;
        }

        private static byte[] CreateInvalidFrame(int kind)
        {
            switch (kind)
            {
                case 0:
                    var badMagic = Header(9, 2);
                    badMagic[0] = 0;
                    return badMagic;
                case 1:
                    return Header(0, 2);
                case 2:
                    return Header(FrameLimit + 1, 2);
                case 3:
                    return Header(8, 2);
                case 4:
                    return Header(429, 4);
                case 5:
                    var badCount = Header(430, 4);
                    badCount[9] = 11;
                    return badCount;
                case 6:
                    var oldAck = Header(31, 17); oldAck[7] = 1;
                    return oldAck;
                case 7:
                    var oldSnapshot = Header(10, 7); oldSnapshot[7] = 1;
                    return oldSnapshot;
                default:
                    throw new ArgumentOutOfRangeException(nameof(kind));
            }
        }

        private static byte[] Header(int length, byte opcode)
        {
            var frame = new byte[length >= 8 && length <= 430 ? length : 8];
            frame[0] = 0xCD;
            frame[1] = 0xAB;
            frame[2] = (byte)length;
            frame[3] = (byte)(length >> 8);
            frame[4] = (byte)(length >> 16);
            frame[5] = (byte)(length >> 24);
            frame[6] = opcode;
            frame[7] = 0;
            return frame;
        }

        private sealed class RecordingHandler : IHandlerSock
        {
            public readonly List<byte[]> Packets = new List<byte[]>();

            public void HandleResponse(byte[] buffer)
            {
                Packets.Add(buffer);
            }
        }

        private sealed class NullLogger : ILogger
        {
            public void Log(string message) { }
        }
    }
}
