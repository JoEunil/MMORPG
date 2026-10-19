using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using C01;
using C02;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;
using TMPro;
using UnityEngine;

namespace C02.Tests
{
    // Detached state fixtures. These do not count as real UI input, server observation, or account sessions.
    public sealed class C04ObservationTests
    {
        [Test]
        public void MonsterPacketsUse25And8ByteFields_AllowIdZero_AndRejectMalformedCounts()
        {
            Assert.That(Marshal.SizeOf<MonsterFullField>(), Is.EqualTo(25));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("internalId").ToInt32(), Is.Zero);
            Assert.That(Marshal.OffsetOf<MonsterFullField>("hp").ToInt32(), Is.EqualTo(2));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("maxHp").ToInt32(), Is.EqualTo(6));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("x").ToInt32(), Is.EqualTo(14));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("dir").ToInt32(), Is.EqualTo(22));
            Assert.That(Marshal.OffsetOf<MonsterFullField>("monsterId").ToInt32(), Is.EqualTo(23));
            Assert.That(Marshal.SizeOf<MonsterDeltaField>(), Is.EqualTo(8));
            Assert.That(Marshal.OffsetOf<MonsterDeltaField>("fieldVal").ToInt32(), Is.EqualTo(4));
            var full = Frame(9, Monster(0, 2));
            var decoded = Packet.DeserializeVariablePacket<MonsterFullField>(Packet.ExtractHeader(full), full);
            Assert.That(decoded.count, Is.EqualTo(1));
            Assert.That(decoded.items[0].internalId, Is.Zero);
            Assert.That(decoded.items[0].monsterId, Is.EqualTo(2));
            Assert.That(decoded.items[0].maxHp, Is.EqualTo(100));
            Assert.That(decoded.items[0].x, Is.EqualTo(11));
            var delta = Frame(10, new MonsterDeltaField { internalId = 0, fieldId = 2, fieldVal = Bits(13.25f) });
            var update = Packet.DeserializeVariablePacket<MonsterDeltaField>(Packet.ExtractHeader(delta), delta).items[0];
            Assert.That(update.internalId, Is.Zero);
            Assert.That(update.fieldId, Is.EqualTo(2));
            Assert.That(BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0), Is.EqualTo(13.25f));
            foreach (var frame in new[] { full, delta })
            {
                var truncated = new byte[frame.Length - 1];
                Array.Copy(frame, truncated, truncated.Length);
                BitConverter.GetBytes((uint)truncated.Length).CopyTo(truncated, 2);
                AssertMalformed(truncated, frame[6]);
                var exaggerated = (byte[])frame.Clone();
                exaggerated[8] = exaggerated[9] = 255;
                AssertMalformed(exaggerated, frame[6]);
            }
        }

        [Test]
        public void FullCreatesSeparateKinds_ExcludesOwn_AndOnlyFullRenewsThreeSecondLifetime()
        {
            using (var fixture = new ObservationFixture())
            {
                var player = Player(42); player.hp = 0;
                fixture.Players(100, Player(7), player);
                fixture.Monsters(100, Monster(0, 0), Monster(42, 1), Monster(43, 2));
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "Own ID is excluded; player HP0 is not deletion.");
                Assert.That(fixture.Count("monsters"), Is.EqualTo(3), "Monster ID0 and types0/1/2 are valid; same numeric player ID is separate.");
                fixture.Players(101);
                fixture.Monsters(101);
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(12) });
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 1, fieldVal = Bits(14) });
                Assert.That(fixture.PlayerState(42).x, Is.EqualTo(12));
                Assert.That(fixture.MonsterState(0).x, Is.EqualTo(14));
                fixture.Expire(102.999);
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "Absence in one Full cannot remove an observation.");
                Assert.That(fixture.Count("monsters"), Is.EqualTo(3));
                fixture.Expire(103);
                Assert.That(fixture.Count("players"), Is.Zero, "Delta must not extend Full lifetime.");
                Assert.That(fixture.Count("monsters"), Is.Zero);
                fixture.PlayerDeltas(new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(15) });
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 50 });
                Assert.That(fixture.Count("players"), Is.Zero);
                Assert.That(fixture.Count("monsters"), Is.Zero);
                fixture.Players(104, Player(42));
                fixture.Monsters(104, Monster(0, 0));
                fixture.Players(105.5, Player(42));
                fixture.Monsters(105.5, Monster(0, 0));
                fixture.Expire(107.1);
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "A later valid Full renews the observation.");
                Assert.That(fixture.Count("monsters"), Is.EqualTo(1));
            }
        }

        [Test]
        public void MonsterDeathHidesImmediately_AndOnlyLiveFullCanReappear()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.Monsters(100, Monster(0, 0));
                var visual = fixture.Visual("monsters", (ushort)0);
                fixture.MonsterDeltas(
                    new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 },
                    new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 50 });
                Assert.That(visual.activeSelf, Is.False);
                Assert.That(fixture.Count("monsters"), Is.Zero);
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 0, fieldId = 1, fieldVal = Bits(25) });
                fixture.MonsterDeltas(new MonsterDeltaField { internalId = 99, fieldId = 0, fieldVal = 100 });
                var dead = Monster(0, 0); dead.hp = 0;
                fixture.Monsters(101, dead);
                Assert.That(fixture.Count("monsters"), Is.Zero);
                fixture.Monsters(102, Monster(0, 0));
                Assert.That(fixture.Count("monsters"), Is.EqualTo(1));
                fixture.Monsters(103, Monster(0, 2));
                Assert.That(fixture.MonsterState(0).monsterId, Is.EqualTo(2), "A live Full owns the observed type.");
            }
        }

        [Test]
        public void InvalidEntitiesDoNotPoisonOtherIds_AndDeltaCoordinatesAndHpAreAtomic()
        {
            using (var fixture = new ObservationFixture())
            {
                fixture.Players(100, Player(42), Player(43));
                fixture.Monsters(100, Monster(0, 0), Monster(1, 1));
                var badPlayer = Player(42); badPlayer.x = float.NaN;
                fixture.Players(101, badPlayer);
                var badMonster = Monster(0, 0); badMonster.dir = 4;
                fixture.Monsters(101, badMonster);
                badMonster = Monster(2, 3); fixture.Monsters(101, badMonster);
                badMonster = Monster(2, 0); badMonster.maxHp = 0; fixture.Monsters(101, badMonster);
                badMonster = Monster(2, 0); badMonster.hp = -1; fixture.Monsters(101, badMonster);
                badMonster = Monster(2, 0); badMonster.y = float.PositiveInfinity; fixture.Monsters(101, badMonster);
                fixture.PlayerDeltas(
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(12) },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 8, fieldVal = Bits(13) },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 0, fieldVal = 150 },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 2, fieldVal = 200 });
                Assert.That(fixture.PlayerState(42).hp, Is.EqualTo(150), "Validate HP with maxHP from the same packet.");
                Assert.That(fixture.PlayerState(42).x, Is.EqualTo(12));
                Assert.That(fixture.PlayerState(42).y, Is.EqualTo(13));
                fixture.PlayerDeltas(
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 6, fieldVal = 256 },
                    new DeltaUpdateField { zoneInternalID = 42, fieldID = 7, fieldVal = Bits(50) },
                    new DeltaUpdateField { zoneInternalID = 43, fieldID = 7, fieldVal = Bits(15) });
                Assert.That(fixture.PlayerState(42).x, Is.EqualTo(12), "Raw direction256 cannot wrap to0.");
                Assert.That(fixture.PlayerState(43).x, Is.EqualTo(15));
                fixture.MonsterDeltas(
                    new MonsterDeltaField { internalId = 0, fieldId = 1, fieldVal = Bits(float.NaN) },
                    new MonsterDeltaField { internalId = 1, fieldId = 1, fieldVal = Bits(16) });
                Assert.That(fixture.MonsterState(0).x, Is.EqualTo(11));
                Assert.That(fixture.MonsterState(1).x, Is.EqualTo(16));
                Assert.That(fixture.Count("monsters"), Is.EqualTo(2));
                fixture.Expire(103);
                Assert.That(fixture.Count("players"), Is.Zero, "Invalid Full and valid Delta never renew LastFullSeen.");
                Assert.That(fixture.Count("monsters"), Is.Zero);
            }
        }

        [Test]
        public void SessionWorldAndDisableInvalidateCapturedCallbacks_AndCurrentModelRebinds()
        {
            using (var fixture = new ObservationFixture())
            {
                var source = fixture.NewModel();
                fixture.Bind(source);
                var old = fixture.PlayerCallback;
                old(1, new[] { Player(42) });
                Assert.That(fixture.Count("players"), Is.EqualTo(1));
                fixture.CloseWorld();
                Assert.That(fixture.Count("players"), Is.Zero);
                old(1, new[] { Player(42) });
                Assert.That(fixture.Count("players"), Is.Zero);
                fixture.OpenWorld();
                old(1, new[] { Player(42) });
                Assert.That(fixture.Count("players"), Is.Zero, "A previously captured generation cannot populate a new world.");
                ((IViewModelNet)source).FullReceived(1, new[] { Player(43) });
                fixture.Drain();
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "Current source callbacks must be reattached after a world reset.");
                var previousClient = fixture.PlayerCallback;
                var next = fixture.NewModel();
                fixture.Bind(next);
                previousClient(1, new[] { Player(44) });
                ((IViewModelNet)source).FullReceived(1, new[] { Player(45) });
                fixture.Drain();
                Assert.That(fixture.Count("players"), Is.Zero);
                ((IViewModelNet)next).MonsterFullReceived(1, new[] { Monster(0, 0) });
                fixture.Drain();
                Assert.That(fixture.Count("monsters"), Is.EqualTo(1));
                var beforeDisable = fixture.MonsterCallback;
                fixture.DisableObserver();
                Assert.That(fixture.Count("monsters"), Is.Zero);
                beforeDisable(1, new[] { Monster(0, 0) });
                Assert.That(fixture.Count("monsters"), Is.Zero);
            }
        }

        [Test]
        public void DestroyedVisualsAndDisplayRoot_DoNotInterruptRemoveClearOrDisable()
        {
            foreach (bool clearBeforeDisable in new[] { false, true })
            {
                using (var fixture = new ObservationFixture())
                {
                    fixture.Players(100, Player(42));
                    fixture.Monsters(100, Monster(0, 0));
                    UnityEngine.Object.DestroyImmediate(fixture.Visual("players", (uint)42));
                    UnityEngine.Object.DestroyImmediate(fixture.Visual("monsters", (ushort)0));
                    Assert.DoesNotThrow(() => fixture.RemovePlayer(42));
                    Assert.DoesNotThrow(() => fixture.RemoveMonster(0));
                    Assert.That(fixture.Count("players"), Is.Zero);
                    Assert.That(fixture.Count("monsters"), Is.Zero);
                    fixture.Players(101, Player(42));
                    fixture.Monsters(101, Monster(0, 0));
                    fixture.DestroyDisplayRoot();
                    if (clearBeforeDisable) Assert.DoesNotThrow(fixture.Clear);
                    Assert.DoesNotThrow(fixture.DisableObserver);
                    Assert.That(fixture.Count("players"), Is.Zero);
                    Assert.That(fixture.Count("monsters"), Is.Zero);
                }
            }
        }

        [Test]
        public void InvalidMonsterDeltaAndDeath_AreRejectedAtomicallyInEitherOrder()
        {
            foreach (bool deathFirst in new[] { false, true })
            foreach (bool invalidDirection in new[] { false, true })
            {
                using (var fixture = new ObservationFixture())
                {
                    fixture.Monsters(100, Monster(0, 0), Monster(1, 1));
                    var invalid = new MonsterDeltaField
                    {
                        internalId = 0,
                        fieldId = invalidDirection ? (ushort)3 : (ushort)1,
                        fieldVal = invalidDirection ? 4U : Bits(float.NaN)
                    };
                    var death = new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 };
                    fixture.MonsterDeltas(deathFirst ? death : invalid, deathFirst ? invalid : death,
                        new MonsterDeltaField { internalId = 1, fieldId = 1, fieldVal = Bits(15) });
                    Assert.That(fixture.Count("monsters"), Is.EqualTo(2));
                    Assert.That(fixture.MonsterState(0).hp, Is.EqualTo(70));
                    Assert.That(fixture.MonsterState(0).x, Is.EqualTo(11));
                    Assert.That(fixture.MonsterState(0).dir, Is.Zero);
                    Assert.That(fixture.Visual("monsters", (ushort)0).activeSelf, Is.True);
                    Assert.That(fixture.MonsterState(1).x, Is.EqualTo(15), "A different valid ID must still apply.");
                }
            }
        }

        [Test]
        public void QueuedPacketsKeepEnqueueSubscribers_AndCannotPopulateReopenedWorld()
        {
            using (var fixture = new ObservationFixture())
            {
                var model = fixture.NewModel();
                fixture.Bind(model);
                var net = (IViewModelNet)model;
                fixture.Players(100, Player(43));
                fixture.Monsters(100, Monster(1, 1));
                net.FullReceived(1, new[] { Player(42) });
                net.DeltaReceived(1, new[] { new DeltaUpdateField { zoneInternalID = 43, fieldID = 7, fieldVal = Bits(99) } });
                net.MonsterFullReceived(1, new[] { Monster(0, 0) });
                net.MonsterDeltaReceived(1, new[] { new MonsterDeltaField { internalId = 1, fieldId = 1, fieldVal = Bits(99) } });
                Assert.That(fixture.QueuedCount, Is.EqualTo(4));
                fixture.CloseWorld();
                fixture.OpenWorld();
                fixture.Players(101, Player(43));
                fixture.Monsters(101, Monster(1, 1));
                fixture.Drain();
                Assert.That(fixture.Count("players"), Is.EqualTo(1), "Old queued Full cannot create an observation in the new scope.");
                Assert.That(fixture.Count("monsters"), Is.EqualTo(1));
                Assert.That(fixture.PlayerState(43).x, Is.EqualTo(11), "Old queued Delta cannot modify a newly observed ID.");
                Assert.That(fixture.MonsterState(1).x, Is.EqualTo(11));
                net.FullReceived(1, new[] { Player(42) });
                net.DeltaReceived(1, new[] { new DeltaUpdateField { zoneInternalID = 43, fieldID = 7, fieldVal = Bits(14) } });
                net.MonsterFullReceived(1, new[] { Monster(0, 0) });
                net.MonsterDeltaReceived(1, new[] { new MonsterDeltaField { internalId = 1, fieldId = 1, fieldVal = Bits(15) } });
                fixture.Drain();
                Assert.That(fixture.Count("players"), Is.EqualTo(2));
                Assert.That(fixture.Count("monsters"), Is.EqualTo(2));
                Assert.That(fixture.PlayerState(43).x, Is.EqualTo(14));
                Assert.That(fixture.MonsterState(1).x, Is.EqualTo(15));
            }
        }

        [Test]
        public void DisposedViewModelDropsQueuedAndNewSnapshotCallbacks()
        {
            var dispatcher = new QueuedDispatcher();
            var model = NewModel(dispatcher);
            int callbacks = 0;
            model.OnFullReceived += (_, __) => callbacks++;
            model.OnDeltaReceived += (_, __) => callbacks++;
            model.OnMonsterFullReceived += (_, __) => callbacks++;
            model.OnMonsterDeltaReceived += (_, __) => callbacks++;
            var net = (IViewModelNet)model;
            net.FullReceived(1, new[] { Player(42) });
            net.DeltaReceived(0, Array.Empty<DeltaUpdateField>());
            net.MonsterFullReceived(1, new[] { Monster(0, 0) });
            net.MonsterDeltaReceived(0, Array.Empty<MonsterDeltaField>());
            Assert.That(dispatcher.Count, Is.EqualTo(4));
            model.Dispose();
            dispatcher.Drain();
            net.FullReceived(1, new[] { Player(42) });
            net.DeltaReceived(0, Array.Empty<DeltaUpdateField>());
            net.MonsterFullReceived(1, new[] { Monster(0, 0) });
            net.MonsterDeltaReceived(0, Array.Empty<MonsterDeltaField>());
            Assert.That(callbacks, Is.Zero);
            Assert.That(dispatcher.Count, Is.Zero);
        }

        private static FullStateField Player(uint id) => new FullStateField { zoneInternalID = id, hp = 70, maxHp = 100, x = 11, y = 12, dir = 0 };
        private static MonsterFullField Monster(ushort id, ushort type) => new MonsterFullField { internalId = id, monsterId = type, hp = 70, maxHp = 100, x = 11, y = 12, dir = 0 };
        private static uint Bits(float value) => BitConverter.ToUInt32(BitConverter.GetBytes(value), 0);

        private static byte[] Frame<T>(byte opcode, T state) where T : struct
        {
            int size = Marshal.SizeOf<T>();
            var bytes = new byte[10 + size];
            BitConverter.GetBytes((ushort)0xABCD).CopyTo(bytes, 0);
            BitConverter.GetBytes((uint)bytes.Length).CopyTo(bytes, 2);
            bytes[6] = opcode; bytes[7] = 1; bytes[8] = 1;
            IntPtr buffer = Marshal.AllocHGlobal(size);
            try { Marshal.StructureToPtr(state, buffer, false); Marshal.Copy(buffer, bytes, 10, size); }
            finally { Marshal.FreeHGlobal(buffer); }
            return bytes;
        }

        private static void AssertMalformed(byte[] frame, byte opcode)
        {
            if (opcode == 9) Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<MonsterFullField>(Packet.ExtractHeader(frame), frame));
            else Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<MonsterDeltaField>(Packet.ExtractHeader(frame), frame));
        }

        private static ViewModel NewModel(QueuedDispatcher dispatcher)
        {
            var data = Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData", true), true);
            return (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.Instance | BindingFlags.NonPublic, null,
                new object[] { new NoNetwork(), data, new NullLogger(), dispatcher }, null);
        }

        private sealed class ObservationFixture : IDisposable
        {
            private readonly GameObject root;
            private readonly C02WorldScreen world;
            private readonly C04ObservedWorld observer;
            private readonly QueuedDispatcher dispatcher = new QueuedDispatcher();
            private readonly List<ViewModel> models = new List<ViewModel>();

            public ObservationFixture()
            {
                root = new GameObject("C04 detached state fixture"); root.SetActive(false);
                var login = root.AddComponent<C01LoginScreen>();
                Set(login, "loginIdInput", Child("id").AddComponent<TMP_InputField>());
                Set(login, "passwordInput", Child("password").AddComponent<TMP_InputField>());
                Set(login, "loginButton", Button("login"));
                Set(login, "statusText", Text("login status"));
                Set(login, "characterListText", Text("list"));
                world = root.AddComponent<C02WorldScreen>();
                Set(world, "loginScreen", login);
                Set(world, "characterChoices", Array.Empty<UnityEngine.UI.Button>());
                Set(world, "enterButton", Button("enter"));
                Set(world, "disconnectButton", Button("disconnect"));
                Set(world, "entryStatusText", Text("entry"));
                Set(world, "worldStatusText", Text("world status"));
                Set(world, "selectionPanel", Child("selection"));
                Set(world, "background", Child("background"));
                var field = Child("field"); Set(world, "worldRoot", field);
                Set(world, "worldHud", Child("HUD"));
                Set(world, "ownCharacter", Child("own").transform);
                Set(world, "fieldPlane", Child("plane").transform);
                Set(world, "quarterViewCamera", Child("camera").transform);
                observer = root.AddComponent<C04ObservedWorld>();
                Set(observer, "loginScreen", login); Set(observer, "worldScreen", world);
                var display = Child("observations"); display.transform.SetParent(field.transform, false);
                Set(observer, "displayRoot", display.transform);
                Set(observer, "playerPrefab", Child("player visual template"));
                Set(observer, "monsterPrefabs", new[] { Child("type0 template"), Child("type1 template"), Child("type2 template") });
                root.SetActive(true);
                OpenWorld();
                Bind(NewModel());
            }

            public ViewModel NewModel() { var model = C04ObservationTests.NewModel(dispatcher); models.Add(model); return model; }
            public void Bind(ViewModel model) => Invoke(observer, "ChangeClient", model);
            public void Drain() => dispatcher.Drain();
            public int QueuedCount => dispatcher.Count;
            public Action<ushort, FullStateField[]> PlayerCallback => (Action<ushort, FullStateField[]>)Get(observer, "playerFull");
            public Action<ushort, MonsterFullField[]> MonsterCallback => (Action<ushort, MonsterFullField[]>)Get(observer, "monsterFull");
            public void Players(double now, params FullStateField[] states) => Invoke(observer, "ApplyPlayerFull", (ushort)states.Length, states, now);
            public void Monsters(double now, params MonsterFullField[] states) => Invoke(observer, "ApplyMonsterFull", (ushort)states.Length, states, now);
            public void PlayerDeltas(params DeltaUpdateField[] updates) => Invoke(observer, "ApplyPlayerDelta", (ushort)updates.Length, updates);
            public void MonsterDeltas(params MonsterDeltaField[] updates) => Invoke(observer, "ApplyMonsterDelta", (ushort)updates.Length, updates);
            public void Expire(double now) => Invoke(observer, "Expire", now);
            public void RemovePlayer(uint id) => Invoke(observer, "RemovePlayer", id);
            public void RemoveMonster(ushort id) => Invoke(observer, "RemoveMonster", id);
            public void Clear() => Invoke(observer, "ClearObservations");
            public void DestroyDisplayRoot() => UnityEngine.Object.DestroyImmediate(((Transform)Get(observer, "displayRoot")).gameObject);
            public int Count(string name) => ((IDictionary)Get(observer, name)).Count;
            public FullStateField PlayerState(uint id) => (FullStateField)EntryField("players", id, "State");
            public MonsterFullField MonsterState(ushort id) => (MonsterFullField)EntryField("monsters", id, "State");
            public GameObject Visual(string name, object id) => (GameObject)EntryField(name, id, "Visual");
            public void CloseWorld() => Invoke(world, "ResetWorld");
            public void DisableObserver() => observer.enabled = false;
            public void OpenWorld()
            {
                typeof(C02WorldScreen).GetProperty("Stage").GetSetMethod(true).Invoke(world, new object[] { C02Stage.Entering });
                Invoke(world, "ZoneReceived", (ushort)1, (ulong)0, (uint)7, 10f, 10f);
                Invoke(world, "FullReceived", (ushort)1, new[] { Player(7) });
            }
            public void Dispose()
            {
                UnityEngine.Object.DestroyImmediate(root);
                foreach (var model in models) model.Dispose();
            }

            private object EntryField(string name, object id, string field)
            {
                var entry = ((IDictionary)Get(observer, name))[id];
                return entry.GetType().GetField(field).GetValue(entry);
            }
            private GameObject Child(string name) { var child = new GameObject(name, typeof(RectTransform)); child.transform.SetParent(root.transform, false); return child; }
            private UnityEngine.UI.Button Button(string name) => Child(name).AddComponent<UnityEngine.UI.Button>();
            private TMP_Text Text(string name) => Child(name).AddComponent<TextMeshProUGUI>();
        }

        private static object Get(object target, string field) => target.GetType().GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private static void Set(object target, string field, object value) => target.GetType().GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
        private static void Invoke(object target, string method, params object[] args) => target.GetType().GetMethod(method, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);
        private sealed class QueuedDispatcher : IMainThreadDispatcher
        {
            private readonly List<Action> jobs = new List<Action>();
            public int Count => jobs.Count;
            public void Post(Action job) => jobs.Add(job);
            public void Drain() { var pending = jobs.ToArray(); jobs.Clear(); foreach (var job in pending) job(); }
        }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string message) { } }
        private sealed class NoNetwork : INetworkService
        {
            public void RequestInventory() { } public void Profiles(uint[] profileIds) { }
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong character) { }
            public void Chat(string message, byte scope, ulong target) { }
            public void Action(byte direction, float x, float y, byte skill) { }
            public void Pong(ulong time) { }
            public void ZoneChange(byte operation) { }
            public void Log(string message) { }
        }
    }
}
