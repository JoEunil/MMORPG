using System.Reflection;
using Mmorpg.Client;
using ClientCore.PacketHelper;
using NUnit.Framework;
using UnityEngine;

namespace Mmorpg.Client.Tests
{
    [Category("MotionRevision")]
    public sealed class RemoteMotionTests
    {
        [Test]
        public void FullSamplesInterpolateWithoutChangingState_AndResetDropsOldFrames()
        {
            var root = new GameObject("C16 detached motion fixture");
            root.SetActive(false);
            try
            {
                var display = new GameObject("display");
                display.transform.SetParent(root.transform, false);
                var prefab = new GameObject("player template");
                prefab.transform.SetParent(root.transform, false);
                var observer = root.AddComponent<ObservedWorld>();
                Set(observer, "displayRoot", display.transform);
                Set(observer, "playerPrefab", prefab);

                ApplyFull(observer, 1, Player(42, 10));
                var visual = ((GameObject)GetField(((System.Collections.IDictionary)Get(observer, "players"))[42u], "Visual")).transform;
                Assert.That(visual.position.x, Is.EqualTo(10), "The initial Full must place a new visual immediately.");
                ApplyFull(observer, 1.1, Player(42, 20));
                Assert.That(((FullStateField)GetField(((System.Collections.IDictionary)Get(observer, "players"))[42u], "State")).x,
                    Is.EqualTo(20), "The latest server state is committed immediately.");

                Render(observer, 1.125); // presentation time 1.050
                Assert.That(visual.position.x, Is.EqualTo(15).Within(0.001));
                Render(observer, 1.175); // presentation time 1.100
                Assert.That(visual.position.x, Is.EqualTo(20).Within(0.001));
                Render(observer, 1.4); // after the newest sample: hold, never extrapolate
                Assert.That(visual.position.x, Is.EqualTo(20).Within(0.001));

                Invoke(observer, "ApplyPlayerDelta", (ushort)1, new[]
                {
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(25) }
                });
                Assert.That(((FullStateField)GetField(((System.Collections.IDictionary)Get(observer, "players"))[42u], "State")).x,
                    Is.EqualTo(25), "A Delta commits the newest server position immediately.");
                Render(observer, Time.realtimeSinceStartupAsDouble + 0.075);
                Assert.That(visual.position.x, Is.EqualTo(25).Within(0.001), "The delayed presentation must reach the Delta endpoint.");

                Invoke(observer, "ClearObservations");
                ApplyFull(observer, 2, Player(42, 30));
                visual = ((GameObject)GetField(((System.Collections.IDictionary)Get(observer, "players"))[42u], "Visual")).transform;
                Render(observer, 1.95);
                Assert.That(visual.position.x, Is.EqualTo(30).Within(0.001), "A reset must discard all prior presentation samples.");
            }
            finally { UnityEngine.Object.DestroyImmediate(root); }
        }

        private static FullStateField Player(uint id, float x) =>
            new FullStateField { zoneInternalID = id, hp = 70, maxHp = 100, x = x, y = 12, dir = 0 };
        private static uint Bits(float value) => System.BitConverter.ToUInt32(System.BitConverter.GetBytes(value), 0);

        private static void ApplyFull(ObservedWorld observer, double time, FullStateField state) =>
            Invoke(observer, "ApplyPlayerFull", (ushort)1, new[] { state }, time);

        private static void Render(ObservedWorld observer, double time) => Invoke(observer, "RenderObservations", time);
        private static object Get(object target, string field) => target.GetType().GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private static object GetField(object target, string field) => target.GetType().GetField(field).GetValue(target);
        private static void Set(object target, string field, object value) => target.GetType().GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
        private static void Invoke(object target, string method, params object[] args) => target.GetType().GetMethod(method, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);
    }
}
