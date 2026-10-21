using System;
using System.Reflection;
using C02;
using ClientCore.PacketHelper;
using NUnit.Framework;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace C02.Tests
{
    public sealed class C16OwnMotionTests
    {
        [Test]
        [Category("MotionRevision")]
        [Category("PlayFeedbackRevision")]
        public void OwnFullAndDeltaKeepServerAuthorityWhileVisualPredictionReconcilesAndResets()
        {
            using (var fixture = new WorldFixture())
            {
                fixture.EnterWithOwnState();
                fixture.Predict(Vector2.right, 0.05f);
                Assert.That(fixture.ServerPosition, Is.EqualTo(new Vector2(10, 10)));
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(10.3f).Within(0.0001),
                    "A local intent moves the rendered character while the server snapshot stays authoritative.");

                fixture.Predict(Vector2.right, 0.05f);
                fixture.Predict(Vector2.right, 0.05f);
                Assert.That(fixture.InputTarget.x, Is.EqualTo(10.6f).Within(0.0001),
                    "Held intent accumulates across delayed confirmations, bounded to 0.6 units ahead.");
                Assert.That(fixture.VisualPosition.x, Is.EqualTo(10.3f).Within(.0001f),
                    "A two-tick request lead does not become two ticks of rendered prediction.");
                fixture.Delta(10.3f);
                Assert.That(fixture.ServerPosition, Is.EqualTo(new Vector2(10.3f, 10)));
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(10.3f).Within(0.0001),
                    "Presentation leads by one tick, and callbacks do not jump the rendered character.");
                fixture.Predict(Vector2.right, 0.05f);
                Assert.That(fixture.InputTarget.x, Is.EqualTo(10.9f).Within(0.0001),
                    "A delayed confirmation frees bounded lead without rebuilding every target from that snapshot.");
                fixture.Delta(10.6f);

                fixture.CancelMovement();
                fixture.Predict(Vector2.zero, 0.025f);
                fixture.Predict(Vector2.zero, 0.025f);
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(10.6f).Within(0.0001),
                    "After input cancellation, the frame update eases the visual back to current authority.");
                Assert.That(fixture.HasMovementIntent, Is.False);
                Assert.That(fixture.InputTarget, Is.EqualTo(fixture.ServerPosition));

                fixture.Predict(Vector2.one, 0.05f);
                Assert.That(Vector2.Distance(fixture.ServerPosition, fixture.VisualPosition), Is.EqualTo(0.3f).Within(0.0001),
                    "Diagonal held input uses the same speed as a cardinal direction.");
                fixture.CancelMovement();
                fixture.Predict(Vector2.zero, 0.05f);
                fixture.Predict(Vector2.right, 0.05f);
                for (int i = 0; i < 10; i++) fixture.Predict(Vector2.right, 0.05f);
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(10.6f).Within(0.0001),
                    "When authority stops advancing, the rendered actor settles even while a key remains held.");
                Assert.That(Vector2.Distance(fixture.InputTarget, fixture.ServerPosition), Is.LessThanOrEqualTo(0.3001f),
                    "A stale server receives only a single-tick bounded probe, with no accumulated invisible walk.");

                fixture.Delta(10.9f);
                fixture.Predict(Vector2.right, .05f);
                fixture.Predict(Vector2.left, .05f);
                Assert.That(fixture.InputTarget.x, Is.LessThan(fixture.ServerPosition.x),
                    "Reversing input immediately discards the old forward request lead.");
                fixture.Delta(11.1f);
                Assert.That(fixture.InputTarget.x, Is.LessThanOrEqualTo(fixture.ServerPosition.x),
                    "A delayed rightward confirmation cannot turn the current leftward request back to the right.");

                fixture.Delta(15f);
                Assert.That(fixture.ServerPosition, Is.EqualTo(new Vector2(15, 10)));
                Assert.That(fixture.OwnPosition, Is.EqualTo(new Vector3(15, 0, 10)),
                    "A large server correction snaps to the authoritative position.");
                Assert.That(fixture.HasMovementIntent, Is.False);
                Assert.That(fixture.InputTarget, Is.EqualTo(fixture.ServerPosition));
                fixture.Predict(Vector2.one, 10f);
                Assert.That(Vector2.Distance(fixture.InputTarget, fixture.ServerPosition), Is.LessThanOrEqualTo(0.3001f),
                    "A large frame never integrates a backlog of missed input ticks.");
                Assert.That(Vector2.Distance(fixture.VisualPosition, fixture.ServerPosition), Is.LessThanOrEqualTo(0.3001f));

                fixture.ResetWorld();
                Assert.That(fixture.HasOwnFull, Is.False);
                Assert.That(fixture.HasVisualPosition, Is.False);
                Assert.That(fixture.HasMovementIntent, Is.False);
                Assert.That(fixture.InputTarget, Is.EqualTo(Vector2.zero));
            }
        }

        private sealed class WorldFixture : IDisposable
        {
            private readonly GameObject root;
            private readonly C02WorldScreen screen;

            public WorldFixture()
            {
                root = new GameObject("C16 own motion fixture");
                root.SetActive(false);
                screen = root.AddComponent<C02WorldScreen>();
                var login = root.AddComponent<C01.C01LoginScreen>();
                Set(login, "loginIdInput", Input("login id"));
                Set(login, "passwordInput", Input("password"));
                Set(login, "loginButton", Button("login button"));
                Set(login, "statusText", Text("login status"));
                Set(login, "characterListText", Text("character list"));
                var selection = Child("selection panel");
                var background = Child("background");
                var world = Child("world root");
                var hud = Child("world HUD");
                var character = Child("character");
                var plane = Child("field plane");
                var camera = Child("camera");
                Set(screen, "loginScreen", login);
                Set(screen, "characterChoices", Array.Empty<Button>());
                Set(screen, "enterButton", Button("enter"));
                Set(screen, "disconnectButton", Button("disconnect"));
                Set(screen, "entryStatusText", Text("entry status"));
                Set(screen, "worldStatusText", Text("world status"));
                Set(screen, "selectionPanel", selection);
                Set(screen, "background", background);
                Set(screen, "worldRoot", world);
                Set(screen, "worldHud", hud);
                Set(screen, "ownCharacter", character.transform);
                Set(screen, "fieldPlane", plane.transform);
                Set(screen, "quarterViewCamera", camera.transform);
                Invoke(screen, "ChangeClient", new object[] { null });
            }

            public Vector2 ServerPosition => screen.ServerPosition;
            public Vector3 OwnPosition => ((Transform)Get(screen, "ownCharacter")).position;
            public bool HasOwnFull => screen.HasOwnFull;
            public bool HasVisualPosition => (bool)Get(screen, "hasVisualPosition");
            public Vector2 InputTarget => (Vector2)Get(screen, "inputTarget");
            public Vector2 VisualPosition => (Vector2)Get(screen, "visualPosition");
            public bool HasMovementIntent => (bool)Get(screen, "hasMovementIntent");

            public void EnterWithOwnState()
            {
                var stage = typeof(C02WorldScreen).GetProperty("Stage");
                stage.GetSetMethod(true).Invoke(screen, new[] { Enum.Parse(stage.PropertyType, "Entering") });
                typeof(C02WorldScreen).GetProperty("SelectedCharacterID").GetSetMethod(true).Invoke(screen, new object[] { 42UL });
                Invoke(screen, "ZoneReceived", (ushort)1, (ulong)0, (uint)73, 10f, 10f);
                Invoke(screen, "FullReceived", (ushort)1, new[] { new FullStateField
                {
                    zoneInternalID = 73, hp = 80, mp = 30, maxHp = 100, maxMp = 40,
                    level = 5, dir = 3, x = 10f, y = 10f
                }});
            }

            public void Predict(Vector2 direction, float deltaSeconds) =>
                Invoke(screen, "UpdateVisualPosition", direction, deltaSeconds);

            public void Delta(float value) => Invoke(screen, "DeltaReceived", (ushort)1,
                new[] { new DeltaUpdateField
                {
                    zoneInternalID = 73,
                    fieldID = 7,
                    fieldVal = BitConverter.ToUInt32(BitConverter.GetBytes(value), 0)
                }});

            public void CancelMovement() => screen.CancelMovement();

            public void ResetWorld() => Invoke(screen, "ResetWorld");

            public void Dispose() => UnityEngine.Object.DestroyImmediate(root);

            private GameObject Child(string name)
            {
                var child = new GameObject(name, typeof(RectTransform));
                child.transform.SetParent(root.transform, false);
                return child;
            }

            private TMP_InputField Input(string name) => Child(name).AddComponent<TMP_InputField>();
            private Button Button(string name) => Child(name).AddComponent<Button>();
            private TMP_Text Text(string name) => Child(name).AddComponent<TextMeshProUGUI>();
            private static object Get(Component target, string name) => target.GetType()
                .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
            private static void Set(Component target, string name, object value) => target.GetType()
                .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
            private static void Invoke(C02WorldScreen target, string name, params object[] args) =>
                typeof(C02WorldScreen).GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);
        }
    }
}
