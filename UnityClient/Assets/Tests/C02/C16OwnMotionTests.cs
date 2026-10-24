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
        public void DisableAfterActorDestroyed_ClearsWorldWithoutAccessingNativeMotion()
        {
            using (var fixture = new WorldFixture())
            {
                fixture.EnterWithOwnState();
                Assert.That(fixture.ActorMotion, Is.Not.Null);
                UnityEngine.Object.DestroyImmediate(fixture.ActorMotion);
                Assert.That((object)fixture.ActorMotion, Is.Not.Null, "A managed wrapper still exists.");
                Assert.That(fixture.ActorMotion == null, Is.True, "Unity considers the native component destroyed.");
                Assert.DoesNotThrow(() => fixture.DisableScreen());
                Assert.That(fixture.HasOwnFull, Is.False);
                Assert.That(fixture.HasVisualPosition, Is.False);
                Assert.That(fixture.Stage, Is.EqualTo("Ready"));
            }
        }

        [Test]
        [Category("MotionRevision")]
        public void HeldInputCanReachAdjacentZoneEdge_WhileServerStateBoundsStayStrict()
        {
            using (var fixture = new WorldFixture())
            {
                fixture.EnterWithOwnState(204.8f);
                fixture.Predict(Vector2.right, 0.05f);

                Assert.That(fixture.ServerPosition.x, Is.EqualTo(204.8f).Within(0.0001f));
                Assert.That(fixture.InputTarget.x, Is.EqualTo(205.25f).Within(0.0001f),
                    "A local target may lead across the owner boundary without cancelling held input.");
                Assert.That(fixture.HasMovementIntent, Is.True);
                Assert.That(InsideZone(1, new Vector2(205.1f, 10)), Is.False,
                    "Authoritative snapshots keep the original Zone 1 bounds.");

                var move = new object[] { (ushort)1, fixture.ServerPosition, fixture.InputTarget, Vector2.zero, (byte)0 };
                Assert.That(Candidate(move), Is.True,
                    "The boundary target remains eligible for the existing move request path.");
                Assert.That((byte)move[4], Is.EqualTo(3));

                for (int i = 0; i < 3; i++) fixture.Predict(Vector2.right, .05f);
                Assert.That(fixture.InputTarget.x - fixture.ServerPosition.x, Is.EqualTo(1.8f).Within(0.0001f),
                    "Boundary requests retain a four-tick horizon while authority is delayed.");
                Assert.That(Vector2.Distance(fixture.VisualPosition, fixture.ServerPosition), Is.EqualTo(.9f).Within(.0001f),
                    "The longer request horizon does not increase rendered prediction.");
                move = new object[] { (ushort)1, fixture.ServerPosition, fixture.InputTarget, Vector2.zero, (byte)0 };
                Assert.That(Candidate(move), Is.True);
                Assert.That(Vector2.Distance(fixture.ServerPosition, (Vector2)move[3]), Is.EqualTo(1.8f).Within(.0001f),
                    "The request builder must not silently shorten the horizon to one unit.");

                fixture.AuthoritativeFullAt(205.1f);
                Assert.That(fixture.Stage, Is.EqualTo("Failed"),
                    "A server snapshot beyond the owner bounds is still rejected.");
                Assert.That(fixture.ServerPosition.x, Is.EqualTo(204.8f).Within(0.0001f));
            }
        }

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
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(10.45f).Within(0.0001),
                    "A local intent moves the rendered character while the server snapshot stays authoritative.");

                fixture.Predict(Vector2.right, 0.05f);
                Assert.That(fixture.InputTarget.x, Is.EqualTo(10.9f).Within(0.0001),
                    "Held intent integrates two ticks while confirmations are delayed.");
                Assert.That(fixture.VisualPosition.x, Is.EqualTo(10.9f).Within(.0001f));
                fixture.Delta(10.45f);
                Assert.That(fixture.ServerPosition.x, Is.EqualTo(10.45f).Within(.0001f));
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(10.9f).Within(0.0001),
                    "A delayed confirmation does not jump the rendered character backwards.");
                fixture.Predict(Vector2.right, 0.05f);
                Assert.That(fixture.InputTarget.x, Is.EqualTo(11.35f).Within(0.0001),
                    "A delayed confirmation frees bounded lead without rebuilding every target from that snapshot.");
                fixture.Delta(10.9f);

                fixture.CancelMovement();
                fixture.Predict(Vector2.zero, 0.025f);
                fixture.Predict(Vector2.zero, 0.025f);
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(11.35f).Within(0.0001),
                    "Key-up stops presentation without immediately rewinding an in-flight movement tick.");
                Assert.That(fixture.HasMovementIntent, Is.False);
                Assert.That(fixture.InputTarget, Is.EqualTo(fixture.ServerPosition));
                fixture.Delta(11.35f);
                fixture.Predict(Vector2.zero, .05f);
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(11.35f).Within(.0001f),
                    "An already sent final target can catch up without a backwards/forwards rubber band.");
                fixture.AuthoritativeFullAt(11.35f);
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(11.35f).Within(.0001f));

                fixture.Predict(Vector2.one, 0.05f);
                Assert.That(Vector2.Distance(fixture.ServerPosition, fixture.VisualPosition), Is.EqualTo(0.45f).Within(0.0001),
                    "Diagonal held input uses the same speed as a cardinal direction.");
                fixture.CancelMovement();
                for (int i = 0; i < 30; i++) fixture.Predict(Vector2.zero, .05f);
                Assert.That(Vector2.Distance(fixture.VisualPosition, fixture.ServerPosition), Is.LessThan(.0001f),
                    "Unconfirmed presentation still settles onto authority after the key-up grace.");
                fixture.Predict(Vector2.right, 0.05f);
                for (int i = 0; i < 30; i++) fixture.Predict(Vector2.right, 0.05f);
                Assert.That(fixture.OwnPosition.x, Is.EqualTo(11.35f).Within(0.0001),
                    "When authority stops advancing, the rendered actor settles even while a key remains held.");
                Assert.That(Vector2.Distance(fixture.InputTarget, fixture.ServerPosition), Is.LessThanOrEqualTo(0.4501f),
                    "A stale server receives only a single-tick bounded probe, with no accumulated invisible walk.");

                fixture.Delta(11.8f);
                fixture.Predict(Vector2.right, .05f);
                float beforeReverse = fixture.VisualPosition.x;
                fixture.Predict(Vector2.left, .05f);
                Assert.That(fixture.InputTarget.x, Is.LessThan(fixture.ServerPosition.x),
                    "Reversing input immediately discards the old forward request lead.");
                Assert.That(fixture.VisualPosition.x, Is.LessThan(beforeReverse),
                    "The visual immediately follows the new heading from its rendered position.");
                fixture.Delta(12.25f);
                Assert.That(fixture.InputTarget.x, Is.LessThanOrEqualTo(fixture.ServerPosition.x),
                    "A delayed rightward confirmation cannot turn the current leftward request back to the right.");

                fixture.Delta(15f);
                Assert.That(fixture.ServerPosition, Is.EqualTo(new Vector2(15, 10)));
                Assert.That(fixture.OwnPosition, Is.EqualTo(new Vector3(15, 0, 10)),
                    "A large server correction snaps to the authoritative position.");
                Assert.That(fixture.HasMovementIntent, Is.False);
                Assert.That(fixture.InputTarget, Is.EqualTo(fixture.ServerPosition));
                fixture.Predict(Vector2.one, 10f);
                Assert.That(Vector2.Distance(fixture.InputTarget, fixture.ServerPosition), Is.LessThanOrEqualTo(0.4501f),
                    "A large frame never integrates a backlog of missed input ticks.");
                Assert.That(Vector2.Distance(fixture.VisualPosition, fixture.ServerPosition), Is.LessThanOrEqualTo(0.9001f));

                fixture.ResetWorld();
                Assert.That(fixture.HasOwnFull, Is.False);
                Assert.That(fixture.HasVisualPosition, Is.False);
                Assert.That(fixture.HasMovementIntent, Is.False);
                Assert.That(fixture.InputTarget, Is.EqualTo(Vector2.zero));

                fixture.EnterWithOwnState();
                fixture.Predict(Vector2.right, .05f);
                Vector2 beforeTurn = fixture.VisualPosition;
                fixture.Predict(Vector2.up, .05f);
                Assert.That(fixture.InputTarget.x, Is.EqualTo(fixture.ServerPosition.x).Within(.0001f),
                    "A perpendicular turn drops all unacknowledged rightward lead.");
                Assert.That(fixture.InputTarget.y - fixture.ServerPosition.y, Is.EqualTo(.45f).Within(.0001f));
                Assert.That(fixture.VisualPosition.x, Is.EqualTo(beforeTurn.x).Within(.0001f),
                    "A perpendicular turn does not drag the rendered actor sideways towards an old snapshot.");
                Assert.That(fixture.VisualPosition.y - beforeTurn.y, Is.EqualTo(.45f).Within(.0001f));
                fixture.Predict(Vector2.one, .05f);
                Vector2 diagonalLead = fixture.InputTarget - fixture.ServerPosition;
                Assert.That(Vector2.Distance(diagonalLead, Vector2.one.normalized * .45f), Is.LessThan(.0001f),
                    "A 45-degree heading change also starts with only the current input's lead.");
                fixture.Predict(Vector2.one, .05f);
                Assert.That(Vector2.Distance(fixture.InputTarget - fixture.ServerPosition, Vector2.one.normalized * .9f),
                    Is.LessThan(.0001f), "Unchanged held direction still accumulates its bounded lead.");

                fixture.ResetWorld();
                fixture.EnterWithOwnState();
                for (int frame = 1; frame <= 12; frame++)
                {
                    float previousX = fixture.VisualPosition.x;
                    fixture.Predict(Vector2.right, 1f / 60f);
                    Assert.That(fixture.VisualPosition.x - previousX, Is.EqualTo(.15f).Within(.0001f),
                        "Between 50ms authority updates, 60fps presentation advances continuously at 9 units/s.");
                    if (frame % 3 == 0) fixture.Delta(fixture.InputTarget.x);
                }
                Assert.That(fixture.ServerPosition.x, Is.EqualTo(11.8f).Within(.0001f));

                fixture.ResetWorld();
                fixture.EnterWithOwnState();
                float acceptedX = 10;
                float budget = 0;
                var acceptedByFrame = new float[121];
                for (int frame = 1; frame <= 120; frame++)
                {
                    // A 50ms authoritative step is observed 150ms later, independently of rendering.
                    if (frame > 9 && frame % 3 == 0) fixture.Delta(acceptedByFrame[frame - 9]);
                    fixture.Predict(Vector2.right, 1f / 60f);
                    Assert.That(Vector2.Distance(fixture.VisualPosition, fixture.ServerPosition), Is.LessThanOrEqualTo(.9001f));
                    if (frame % 3 != 0) continue;

                    var request = new object[] { (ushort)1, fixture.ServerPosition, fixture.InputTarget, Vector2.zero, (byte)0 };
                    Assert.That(Candidate(request), Is.True);
                    budget = Mathf.Min(3, budget + .45f);
                    float nextX = Mathf.MoveTowards(acceptedX, ((Vector2)request[3]).x, budget);
                    budget -= Mathf.Abs(nextX - acceptedX);
                    acceptedX = nextX;
                    acceptedByFrame[frame] = acceptedX;
                }
                Assert.That(acceptedX - 10, Is.EqualTo(18f).Within(.001f),
                    "A 150ms confirmation delay must not turn the 9-units/s request stream into a feedback-limited 6-units/s stream.");
            }
        }

        [TestCase(true)]
        [TestCase(false)]
        [Category("MovementStopLatency")]
        public void ReleaseWaitsForDelayedFinalSnapshot_AndStillSettlesWithoutReply(bool finalSnapshotArrives)
        {
            using (var fixture = new WorldFixture())
            {
                fixture.EnterWithOwnState();
                fixture.Predict(Vector2.right, .05f);
                fixture.Predict(Vector2.right, .05f);
                float stoppedX = fixture.VisualPosition.x;
                fixture.CancelMovement();

                // The last movement response takes 200ms after key-up to arrive.
                for (int frame = 0; frame < 4; frame++)
                {
                    fixture.Predict(Vector2.zero, .05f);
                    Assert.That(fixture.VisualPosition.x, Is.EqualTo(stoppedX).Within(.0001f),
                        "Do not rewind to stale authority while the final move is still in flight.");
                    Assert.That(fixture.HasMovementIntent, Is.False);
                    Assert.That(fixture.InputTarget, Is.EqualTo(fixture.ServerPosition),
                        "Waiting for presentation must not queue new movement after release.");
                }

                if (finalSnapshotArrives) fixture.Delta(stoppedX);
                else
                {
                    fixture.Predict(Vector2.zero, .05f);
                    fixture.Predict(Vector2.zero, .05f);
                    Assert.That(fixture.VisualPosition.x, Is.EqualTo(stoppedX).Within(.0001f));
                    fixture.Predict(Vector2.zero, .06f);
                    Assert.That(fixture.VisualPosition.x, Is.LessThan(stoppedX),
                        "Missing responses must still start settling within the bounded stale window.");
                }

                for (int frame = 0; frame < 30; frame++) fixture.Predict(Vector2.zero, .05f);
                Assert.That(fixture.VisualPosition.x, Is.EqualTo(fixture.ServerPosition.x).Within(.0001f));
                Assert.That(fixture.ServerPosition.x, Is.EqualTo(finalSnapshotArrives ? stoppedX : 10).Within(.0001f),
                    "Presentation never advances the authoritative position itself.");
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
            public string Stage => screen.Stage.ToString();
            public C19ActorMotion ActorMotion => screen.ActorMotion;
            public void DisableScreen() => Invoke(screen, "OnDisable");

            public void EnterWithOwnState(float x = 10f, float y = 10f)
            {
                var stage = typeof(C02WorldScreen).GetProperty("Stage");
                stage.GetSetMethod(true).Invoke(screen, new[] { Enum.Parse(stage.PropertyType, "Entering") });
                typeof(C02WorldScreen).GetProperty("SelectedCharacterID").GetSetMethod(true).Invoke(screen, new object[] { 42UL });
                Invoke(screen, "ZoneReceived", (ushort)1, (ulong)0, (uint)73, x, y);
                Invoke(screen, "FullReceived", (ushort)1, new[] { new FullStateField
                {
                    zoneInternalID = 73, hp = 80, mp = 30, maxHp = 100, maxMp = 40,
                    level = 5, dir = 3, x = x, y = y
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

            public void AuthoritativeFullAt(float x) => Invoke(screen, "FullReceived", (ushort)1, new[] { new FullStateField
            {
                zoneInternalID = 73, hp = 80, mp = 30, maxHp = 100, maxMp = 40,
                level = 5, dir = 3, x = x, y = 10f
            }});

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

        private static bool InsideZone(ushort zone, Vector2 position) => (bool)typeof(C02WorldScreen)
            .GetMethod("InsideZone", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, new object[] { zone, position });

        private static bool Candidate(object[] args) => (bool)typeof(C02WorldScreen)
            .GetMethod("TryMoveCandidate", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, args);
    }
}
