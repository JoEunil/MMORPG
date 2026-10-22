using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using C02;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace C02.Tests
{
    // These are state/dispatcher fixtures. They do not represent real UI input or a server session.
    public sealed class C02EntryStateFixtureTests
    {
        [Test]
        public void EnterAndZoneFailures_AreReportedThroughQueuedDispatcher()
        {
            var dispatcher = new QueuedDispatcher();
            var network = new FakeNetworkService();
            var model = CreateViewModel(network, dispatcher);
            bool? enterResult = null;
            var zoneFailed = false;
            model.OnEnterSuccess += success => enterResult = success;
            model.OnZoneChageFailed += () => zoneFailed = true;

            model.Enter(456UL);
            Assert.That(network.EnteredCharacterID, Is.EqualTo(456UL));
            ((IViewModelNet)model).EnterReceived(0, Array.Empty<byte>(), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            Assert.That(enterResult, Is.Null, "ViewModel events are delivered through the dispatcher.");
            dispatcher.Drain();
            Assert.That(enterResult, Is.False);
            Assert.That(network.ZoneChangeCount, Is.Zero, "A rejected Enter must not request Zone entry.");

            model.Enter(456UL);
            ((IViewModelNet)model).EnterReceived(1, Array.Empty<byte>(), 0, 0, 0, 100, 100, 100, 100, 0, 8, 9, 0);
            dispatcher.Drain();
            Assert.That(network.ZoneChangeCount, Is.EqualTo(1));
            ((IViewModelNet)model).ZoneChageReceived(0, 0, 0, 0, 0, 0, 1, 0);
            Assert.That(zoneFailed, Is.False);
            dispatcher.Drain();
            Assert.That(zoneFailed, Is.True);
            ((IDisposable)model).Dispose();
        }

        [Test]
        public void DisposeBeforeQueuedResponses_AgeOutEnterZoneAndFullCallbacks()
        {
            var dispatcher = new QueuedDispatcher();
            var network = new FakeNetworkService();
            var model = CreateViewModel(network, dispatcher);
            var callbacks = 0;
            model.OnEnterSuccess += _ => callbacks++;
            model.OnZoneChageReceived += (_, __, ___, ____, _____) => callbacks++;
            model.OnFullReceived += (_, __) => callbacks++;

            model.Enter(789UL);
            var net = (IViewModelNet)model;
            net.EnterReceived(1, Array.Empty<byte>(), 0, 0, 0, 100, 100, 100, 100, 0, 2, 3, 0);
            net.ZoneChageReceived(1, 4, 5, 6, 7, 8, 1, 1);
            net.FullReceived(4, 1, 1, 1, true, 1, new[] { new FullStateField { zoneInternalID = 6 } });
            net.MonsterFullReceived(4, 1, 1, 1, true, 0, Array.Empty<MonsterFullField>());
            ((IDisposable)model).Dispose();
            dispatcher.Drain();

            Assert.That(callbacks, Is.Zero, "Disposed ViewModel callbacks must be ignored when the dispatcher drains late work.");
        }

        [TestCase(false)]
        [TestCase(true)]
        public void OwnFullAndZoneResult_InEitherArrivalOrder_ExposeOnlyMatchingServerState(bool fullFirst)
        {
            using (var screen = new WorldScreenFixture())
            {
                var own = new FullStateField
                {
                    zoneInternalID = 73,
                    profileId = 2,
                    profileVersion = 4,
                    hp = 88,
                    mp = 41,
                    maxHp = 100,
                    maxMp = 50,
                    exp = 1234,
                    level = 9,
                    dir = 1,
                    x = 17.25f,
                    y = -4.5f
                };
                var other = own;
                other.zoneInternalID = 99;
                other.x = 900;

                screen.EnterPending();
                if (fullFirst)
                {
                    screen.Full(other, own);
                    Assert.That(screen.WorldVisible, Is.False, "A Full frame alone cannot establish the current Zone identity.");
                    screen.Zone(1, 73, 15, -5);
                }
                else
                {
                    screen.Zone(1, 73, 15, -5);
                    Assert.That(screen.WorldVisible, Is.False, "Zone success alone cannot establish the own Full state.");
                    screen.Full(other, own);
                }

                Assert.That(screen.WorldVisible, Is.True);
                Assert.That(screen.HasOwnFull, Is.True);
                Assert.That(screen.Stage, Is.EqualTo("InWorld"));
                Assert.That(screen.InternalID, Is.EqualTo(73U));
                Assert.That(screen.ZoneID, Is.EqualTo((ushort)1));
                Assert.That(screen.ServerPosition, Is.EqualTo(new Vector2(17.25f, -4.5f)));
                Assert.That(screen.OwnPosition, Is.EqualTo(new Vector3(17.25f, 0, -4.5f)));
                Assert.That(screen.StatusContainsOwnIDs, Is.True);
            }
        }

        [TestCase(5, 10, 10, 0)]
        [TestCase(1, 106, 10, 0)]
        [TestCase(1, 10, 10, 4)]
        public void InvalidOwnZonePositionOrDirection_CannotExposeWorld(int zone, float x, float y, int direction)
        {
            using (var screen = new WorldScreenFixture())
            {
                screen.EnterPending();
                screen.Zone((ushort)zone, 73, 10, 10);
                screen.Full(new FullStateField { zoneInternalID = 73, x = x, y = y, dir = (byte)direction });
                Assert.That(screen.Stage, Is.EqualTo("Failed"));
                Assert.That(screen.HasOwnFull, Is.False);
                Assert.That(screen.WorldVisible, Is.False);
            }
        }

        private static IViewModelUI CreateViewModel(FakeNetworkService network, QueuedDispatcher dispatcher)
        {
            var assembly = typeof(IViewModelUI).Assembly;
            var viewDataType = assembly.GetType("ClientCore.ViewData", true);
            var viewData = Activator.CreateInstance(viewDataType, true);
            var viewDataModel = (IViewDataModel)viewData;
            var viewModelType = assembly.GetType("ClientCore.ViewModel", true);
            var model = (IViewModelUI)Activator.CreateInstance(viewModelType,
                BindingFlags.Instance | BindingFlags.NonPublic, null,
                new object[] { network, viewDataModel, new NullLogger(), dispatcher }, null);
            return model;
        }

        private sealed class WorldScreenFixture : IDisposable
        {
            private readonly GameObject root;
            private readonly C02WorldScreen screen;
            private readonly Component login;
            private readonly TMP_Text status;

            public WorldScreenFixture()
            {
                root = new GameObject("C02 state fixture");
                root.SetActive(false);
                screen = root.AddComponent<C02WorldScreen>();
                login = root.AddComponent<C01.C01LoginScreen>();
                Set(login, "loginIdInput", Input("login id"));
                Set(login, "passwordInput", Input("password"));
                Set(login, "loginButton", Button("login button"));
                Set(login, "statusText", Text("login status"));
                Set(login, "characterListText", Text("character list"));

                var panel = Child("selection panel");
                var background = Child("background");
                var worldRoot = Child("world root");
                var worldHud = Child("world HUD");
                var ownCharacter = Child("own capsule");
                var fieldPlane = Child("field plane");
                var camera = Child("quarter view camera");
                status = Text("world status");
                Set(screen, "loginScreen", login);
                Set(screen, "characterChoices", Array.Empty<Button>());
                Set(screen, "enterButton", Button("enter"));
                Set(screen, "disconnectButton", Button("disconnect"));
                Set(screen, "entryStatusText", Text("entry status"));
                Set(screen, "worldStatusText", status);
                Set(screen, "selectionPanel", panel);
                Set(screen, "background", background);
                Set(screen, "worldRoot", worldRoot);
                Set(screen, "worldHud", worldHud);
                Set(screen, "ownCharacter", ownCharacter.transform);
                Set(screen, "fieldPlane", fieldPlane.transform);
                Set(screen, "quarterViewCamera", camera.transform);
                Invoke(screen, "ChangeClient", new object[] { null });
            }

            public string Stage => screen.Stage.ToString();
            public bool WorldVisible => screen.IsWorldVisible;
            public bool HasOwnFull => screen.HasOwnFull;
            public uint InternalID => screen.InternalID;
            public ushort ZoneID => screen.ZoneID;
            public Vector2 ServerPosition => screen.ServerPosition;
            public Vector3 OwnPosition => ((Transform)Get(screen, "ownCharacter")).position;
            public bool StatusContainsOwnIDs => status.text.Contains("Character ID 42") && status.text.Contains("Internal ID 73");

            public void EnterPending()
            {
                var stage = typeof(C02WorldScreen).GetProperty("Stage");
                stage.GetSetMethod(true).Invoke(screen, new[] { Enum.Parse(stage.PropertyType, "Entering") });
                typeof(C02WorldScreen).GetProperty("SelectedCharacterID").GetSetMethod(true).Invoke(screen, new object[] { 42UL });
            }

            public void Zone(ushort zone, uint id, float x, float y) =>
                Invoke(screen, "ZoneReceived", zone, (ulong)500, id, x, y);

            public void Full(params FullStateField[] states) =>
                Invoke(screen, "FullReceived", (ushort)states.Length, states);

            public void Dispose()
            {
                UnityEngine.Object.DestroyImmediate(root);
            }

            private GameObject Child(string name)
            {
                var go = new GameObject(name, typeof(RectTransform));
                go.transform.SetParent(root.transform, false);
                return go;
            }

            private TMP_InputField Input(string name)
            {
                var go = Child(name);
                return go.AddComponent<TMP_InputField>();
            }

            private Button Button(string name)
            {
                var go = Child(name);
                return go.AddComponent<Button>();
            }

            private TMP_Text Text(string name)
            {
                var go = Child(name);
                return go.AddComponent<TextMeshProUGUI>();
            }

            private static object Get(Component target, string name) =>
                target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);

            private static void Set(Component target, string name, object value) =>
                target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);

            private static void Invoke(C02WorldScreen target, string name, params object[] args) =>
                typeof(C02WorldScreen).GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);
        }

        private sealed class QueuedDispatcher : IMainThreadDispatcher
        {
            private readonly List<Action> jobs = new List<Action>();
            public void Post(Action job) => jobs.Add(job);
            public void Drain()
            {
                var pending = jobs.ToArray();
                jobs.Clear();
                foreach (var job in pending) job();
            }
        }

        private sealed class FakeNetworkService : INetworkService
        {
            public ulong EnteredCharacterID { get; private set; }
            public int ZoneChangeCount { get; private set; }
            public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { } public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { } public void RequestInventory() { } public void Profiles(uint[] profileIds) { }
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong charID) => EnteredCharacterID = charID;
            public void Chat(string message, byte scope, ulong targetID) { }
            public void Action(byte dir, float x, float y, byte skillSlot, ulong ownershipEpoch) { }
            public void Pong(ulong serverTimeMs) { }
            public void ZoneChange(byte op) => ZoneChangeCount++;
            public void ZoneResync(ulong ownershipEpoch) { }
            public void Log(string msg) { }
        }

        private sealed class NullLogger : ClientCore.ILogger
        {
            public void Log(string message) { }
        }
    }
}
