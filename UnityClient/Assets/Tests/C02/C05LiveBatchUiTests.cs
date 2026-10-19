#if UNITY_EDITOR
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using C01;
using ClientCore;
using ClientCore.PacketHelper;
using NUnit.Framework;
using UnityEditor;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UI;

namespace C02.Tests
{
    // Live C05 phases are started separately so each bounded run can be inspected before continuing.
    public sealed class C05LiveBatchUiTests
    {
        private Mouse mouse;
        private Keyboard keyboard;
        private IViewModelUI model;
        private C01LoginScreen login;
        private C02WorldScreen world;
        private C05CombatScreen combat;
        private C04ObservedWorld observed;
        private readonly Dictionary<uint, FullStateField> players = new Dictionary<uint, FullStateField>();
        private readonly Dictionary<ushort, MonsterFullField> monsters = new Dictionary<ushort, MonsterFullField>();
        private readonly Dictionary<ushort, MonsterFullField> lastMonsterFull = new Dictionary<ushort, MonsterFullField>();
        private readonly Dictionary<ushort, double> firstMonsterZeroAt = new Dictionary<ushort, double>();
        private readonly Dictionary<ushort, double> firstAliveFullAfterZeroAt = new Dictionary<ushort, double>();
        private readonly Dictionary<ushort, MonsterFullField> firstAliveFullAfterZero = new Dictionary<ushort, MonsterFullField>();
        private readonly Dictionary<ushort, double> firstAliveVisualAfterZeroAt = new Dictionary<ushort, double>();
        private readonly HashSet<ushort> zeroVisualRemoved = new HashSet<ushort>();
        private readonly List<ActionResultField> actionResults = new List<ActionResultField>();
        private Action<ushort, FullStateField[]> fullHandler;
        private Action<ushort, DeltaUpdateField[]> deltaHandler;
        private Action<ushort, MonsterFullField[]> monsterFullHandler;
        private Action<ushort, MonsterDeltaField[]> monsterDeltaHandler;
        private Action<ushort, ActionResultField[]> actionHandler;
        private string resultPath;

        public IEnumerator AccountOneCombatAndMonsterRespawn()
        {
            var focus = EditorWindow.focusedWindow;
            bool previousBackground = Application.runInBackground;
            try
            {
                Application.runInBackground = true;
                mouse = InputSystem.AddDevice<Mouse>();
                keyboard = InputSystem.AddDevice<Keyboard>();
                var gameView = FindGameView();
                Assert.That(gameView != null, Is.True, "Open the Game view before the live C05 input phase.");
                gameView.Focus();
                yield return null;
                Assert.That(Application.isFocused, Is.True, "The Unity Editor process must have OS focus before native combat input.");
                BindLiveWorld();
                Assert.That(world.ZoneID == 1 && world.SelectedCharacterID == 5011UL && world.CanAct, Is.True,
                    "The shared account-one real UI entry must remain active with its server Full.");
                PreserveAccountOneEntryEvidence();
                Subscribe();

                yield return WaitFor(() => players.ContainsKey(world.InternalID) && monsters.Count > 0,
                    15f, "independent Full observations for the own character and C04 monsters");
                var selectedId = FindTypeZeroMonsterWithin(7.0f);
                Assert.That(selectedId.HasValue, Is.True,
                    "A real type-zero monster must be visible within the server circle range before casting.");
                ushort selectedMonsterId = selectedId.Value;
                var initialMonster = monsters[selectedMonsterId];
                Assert.That(initialMonster.hp > 0, Is.True, "The tracked server monster must be alive before the live cast.");
                File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                    "INFO stationary Ring setup; own=({0:F3},{1:F3}); monsterID={2}; monster=({3:F3},{4:F3}); distance={5:F3}{6}",
                    world.ServerPosition.x, world.ServerPosition.y, selectedMonsterId, initialMonster.x, initialMonster.y,
                    Vector2.Distance(world.ServerPosition, new Vector2(initialMonster.x, initialMonster.y)), Environment.NewLine));
                double combatDeadline = Time.realtimeSinceStartupAsDouble + 120;
                int mpBeforeFirstCast = players[world.InternalID].mp;
                var basicHpBaseline = SnapshotMonsterHp();
                int basicPhasesBefore = CountOwnPhases(0);

                // Send the native 1/2/3 keys and an actual slot-button click; only server events and Full/Delta
                // observations count as results. Phase messages alone are not treated as damage acknowledgements.
                yield return PressSkillKey(Key.Digit1);
                yield return WaitFor(() => CountOwnPhases(0) >= basicPhasesBefore + 2 && SawOwnPhase(0, 0) && SawOwnPhase(0, 1),
                    5f, "new own server start/release phases for native key 1");
                yield return WaitFor(() => players[world.InternalID].mp < mpBeforeFirstCast, 4f,
                    "independent server MP decrease after accepted native key 1");
                AssertOwnVitalsMatchRaw();
                File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                    "PASS native key1 new own phases and raw/C02/C05 MP agreement; mp={0}->{1}; no specific basic-attack target promised.{2}",
                    mpBeforeFirstCast, players[world.InternalID].mp, Environment.NewLine));
                foreach (var pair in basicHpBaseline)
                    if (monsters.TryGetValue(pair.Key, out var hit) && hit.hp < pair.Value)
                        File.AppendAllText(ResultPath, $"OBSERVED basic-attack HP decrease id={pair.Key}; hp={pair.Value}->{hit.hp}\n");

                yield return WaitSeconds(1.2f);
                var ringHpBaseline = SnapshotMonsterHp();
                int ringPhasesBefore = CountOwnPhases(1);
                yield return PressSkillKey(Key.Digit2);
                yield return WaitFor(() => CountOwnPhases(1) >= ringPhasesBefore + 2 && SawOwnPhase(1, 0) && SawOwnPhase(1, 1),
                    4f, "new own server phases for native key 2");
                yield return WaitFor(() => FindDamagedTypeZero(ringHpBaseline).HasValue, 4f,
                    "actual type-zero HP decrease after Ring; ID zero is valid");
                selectedMonsterId = FindDamagedTypeZero(ringHpBaseline).Value;
                File.AppendAllText(ResultPath, $"PASS Ring observed type0 HP decrease; locked id={selectedMonsterId}; hp={ringHpBaseline[selectedMonsterId]}->{monsters[selectedMonsterId].hp}\n");
                RecordObservedPhase(1, "native key 2", ringPhasesBefore);
                int phasesBeforeRepeat = CountOwnPhases(1);
                int mpBeforeRepeat = world.OwnFull.mp;
                yield return PressSkillKey(Key.Digit2);
                yield return WaitSeconds(0.8f);
                Assert.That(CountOwnPhases(1), Is.EqualTo(phasesBeforeRepeat),
                    "A same-skill request during server cooldown must not produce another successful phase.");
                Assert.That(world.OwnFull.mp, Is.EqualTo(mpBeforeRepeat),
                    "A cooldown-rejected repeat must not consume server MP.");
                yield return WaitSeconds(6.2f);
                int phasesBeforeKey3 = CountOwnPhases(2);
                yield return PressSkillKey(Key.Digit3);
                yield return WaitForOptional(() => CountOwnPhases(2) > phasesBeforeKey3, 3f);
                RecordObservedPhase(2, "native key 3", phasesBeforeKey3);
                yield return WaitSeconds(4.2f);
                int phasesBeforeButton = CountOwnPhases(2);
                yield return ClickButton(GetSkillButton(2));
                yield return WaitForOptional(() => CountOwnPhases(2) > phasesBeforeButton, 3f);
                RecordObservedPhase(2, "slot button 3", phasesBeforeButton);

                // Kill only through the same native input path and only while the tracked monster remains
                // visible/alive; no target state is injected and no combat handler is called directly.
                int ringFollowups = 0;
                while (!firstMonsterZeroAt.ContainsKey(selectedMonsterId) && monsters.TryGetValue(selectedMonsterId, out var current) && current.hp > 0 &&
                    world.OwnFull.mp >= 20 && ringFollowups < 2 && Time.realtimeSinceStartupAsDouble + 11 < combatDeadline)
                {
                    var hpBefore = current.hp;
                    yield return WaitSeconds(6.2f);
                    yield return PressSkillKey(Key.Digit2);
                    ringFollowups++;
                    yield return WaitForOptional(() => firstMonsterZeroAt.ContainsKey(selectedMonsterId) ||
                        monsters.TryGetValue(selectedMonsterId, out var next) && next.hp < hpBefore, 4f);
                }
                yield return WaitFor(() => firstMonsterZeroAt.ContainsKey(selectedMonsterId),
                    1f, "server MonsterDelta HP 0 for the same tracked monster ID after the finite cast budget");
                double deathObservedAt = firstMonsterZeroAt[selectedMonsterId];
                if (!MonsterVisualExists(selectedMonsterId)) zeroVisualRemoved.Add(selectedMonsterId);
                yield return WaitFor(() => zeroVisualRemoved.Contains(selectedMonsterId) || !MonsterVisualExists(selectedMonsterId), 2f,
                    "the existing C04 observer to remove the HP-zero visual");
                File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                    "PASS same monster HP0 and visual removal; id={0}; observedHP=0{1}",
                    selectedMonsterId, Environment.NewLine));
                double respawnDeadline = Math.Min(deathObservedAt + 22.0, combatDeadline);
                while (Time.realtimeSinceStartupAsDouble < respawnDeadline &&
                    !HasRespawnFull(selectedMonsterId))
                {
                    ObserveAliveVisual(selectedMonsterId);
                    yield return null;
                }
                ObserveAliveVisual(selectedMonsterId);
                Assert.That(HasRespawnFull(selectedMonsterId), Is.True,
                    "The same ID requires a new alive Full and a live active visual after its first HP-zero delta.");
                Assert.That(MonsterVisualExists(selectedMonsterId), Is.True,
                    "An alive same-ID Full must restore the existing C04 visual.");
                var respawnState = firstAliveFullAfterZero[selectedMonsterId];
                var respawnAt = firstAliveFullAfterZeroAt[selectedMonsterId];
                bool earlyAliveFullBeforeRespawnThreshold = respawnAt - deathObservedAt < 14;
                File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                    "PASS same-ID raw HP0/removal/alive Full/active visual; id={0}; FIRST aliveFullElapsed={1:F2}s; hp={2}; maxHp={3}; earlyAliveFullBeforeThreshold={4}; FIRST visualElapsed={6:F2}s; ringFollowups={7} (max2, max accepted MP101).{5}" +
                    "NOT OBSERVED C05 account-one phase checks for any slot lacking a server phase are reported separately above.\n",
                    selectedMonsterId, respawnAt - deathObservedAt, respawnState.hp, respawnState.maxHp,
                    earlyAliveFullBeforeRespawnThreshold, Environment.NewLine,
                    firstAliveVisualAfterZeroAt[selectedMonsterId] - deathObservedAt, ringFollowups));
                File.AppendAllText(ResultPath, earlyAliveFullBeforeRespawnThreshold
                    ? "TIMING UNCONFIRMED: first alive Full arrived before14s; packet ordering is not guaranteed. Later repeating Full is not first respawn evidence.\n"
                    : "OBSERVED first alive Full timing is consistent with about15s respawn; no new server sequence contract assumed.\n");
                yield return Capture("c05-combat-account1.png");
            }
            finally
            {
                Unsubscribe();
                RemoveTestDevices();
                Application.runInBackground = previousBackground;
                if (focus != null) focus.Focus();
            }
        }

        public IEnumerator AccountTwoLogin()
        {
            var oldLogin = Environment.GetEnvironmentVariable("C01_TEST_LOGIN_ID");
            var focus = EditorWindow.focusedWindow;
            bool previousBackground = Application.runInBackground;
            try
            {
                Application.runInBackground = true;
                mouse = InputSystem.AddDevice<Mouse>();
                keyboard = InputSystem.AddDevice<Keyboard>();
                var entryGameView = FindGameView();
                Assert.That(entryGameView != null, Is.True, "Open the Game view before the account switch phase.");
                entryGameView.Focus();
                yield return null;
                Assert.That(Application.isFocused, Is.True, "The Unity Editor process must have OS focus before the account switch phase.");
                BindLiveWorld();
                var existingDisconnect = GameObject.Find("DisconnectButton")?.GetComponent<Button>();
                Assert.That(existingDisconnect != null && existingDisconnect.interactable, Is.True,
                    "The account-one phase must end through its existing normal disconnect UI before account two enters.");
                yield return ClickButton(existingDisconnect);
                yield return WaitFor(() => !world.IsWorldVisible, 8f, "account-one normal disconnect before account-two login");
                Environment.SetEnvironmentVariable("C01_TEST_LOGIN_ID", "unity_ai_02");
                yield return new C02EmptyListUiTests().RealCharacterSelectionAndEntry_AndCapture(5012UL);
                Environment.SetEnvironmentVariable("C01_TEST_LOGIN_ID", oldLogin);
                BindLiveWorld();
                Assert.That(world.SelectedCharacterID == 5012UL && world.ZoneID == 1 && world.CanAct, Is.True,
                    "The second account must enter character 5012 through the real login/list/selection UI.");
                File.AppendAllText(ResultPath, "PASS account2 real DPAPI-selected login, one-row character list, selection 5012, and server-confirmed Zone entry.\n");
            }
            finally
            {
                Environment.SetEnvironmentVariable("C01_TEST_LOGIN_ID", oldLogin);
                RemoveTestDevices();
                Application.runInBackground = previousBackground;
                if (focus != null) focus.Focus();
            }
        }

        public IEnumerator AccountTwoBossAggro()
        {
            var focus = EditorWindow.focusedWindow;
            bool previousBackground = Application.runInBackground;
            try
            {
                Application.runInBackground = true;
                mouse = InputSystem.AddDevice<Mouse>();
                keyboard = InputSystem.AddDevice<Keyboard>();
                var gameView = FindGameView();
                Assert.That(gameView != null, Is.True, "Open the Game view before account-two native boss approach.");
                gameView.Focus();
                yield return null;
                Assert.That(Application.isFocused, Is.True, "Unity Editor OS focus is required for account-two native input.");
                BindLiveWorld();
                Assert.That(world.SelectedCharacterID == 5012UL && world.ZoneID == 1 && world.CanAct, Is.True,
                    "Account two must already be entered through the real UI before the boss approach phase.");
                Subscribe();
                yield return WaitFor(() => players.ContainsKey(world.InternalID) && monsters.Count > 0, 15f,
                    "independent raw own Full and monster Full observations for account two");
                var camera = GameObject.Find("QuarterViewCamera")?.GetComponent<Camera>();
                var own = GameObject.Find("OwnCharacter")?.transform;
                Assert.That(camera != null && own != null, Is.True, "The existing world camera and player visual are required.");
                yield return MoveAlongVisiblePoints(camera, world, own, new Vector2(45f, 45f));
                yield return WaitFor(() => FindMonsterType(2).HasValue, 15f,
                    "center boss Full entering account two's AOI after the native approach to (45,45)");
                var bossId = FindMonsterType(2).Value;
                var boss = monsters[bossId];
                yield return MoveAlongVisiblePoints(camera, world, own, new Vector2(boss.x + 1f, boss.y + 1f));
                var bossHpBeforeAggro = monsters[bossId].hp;
                yield return PressSkillKey(Key.Digit2);
                yield return WaitForOptional(() => monsters.TryGetValue(bossId, out var currentBoss) && currentBoss.hp < bossHpBeforeAggro, 3f);
                bool bossDamaged = monsters.TryGetValue(bossId, out var bossAfterAggro) && bossAfterAggro.hp < bossHpBeforeAggro;
                File.AppendAllText(ResultPath, bossDamaged
                    ? "PASS native Ring key2 account-two boss aggro input produced a server boss HP change; account-two is in the boss encounter area.\n"
                    : "NOT OBSERVED native Ring key2 account-two boss damage/aggro confirmation; location is prepared without claiming aggro.\n");
                yield return Capture("c05-boss-account2.png");
            }
            finally
            {
                Unsubscribe();
                RemoveTestDevices();
                Application.runInBackground = previousBackground;
                if (focus != null) focus.Focus();
            }
        }

        public IEnumerator AccountTwoHpZeroAndDisconnect() => OwnHpZeroAndDisconnect(5012UL, "account2");
        public IEnumerator AccountOneHpZeroAndDisconnect() => OwnHpZeroAndDisconnect(5011UL, "account1");

        private IEnumerator OwnHpZeroAndDisconnect(ulong expectedCharacterId, string accountLabel)
        {
            Assert.That(expectedCharacterId == 5011UL || expectedCharacterId == 5012UL, Is.True,
                "Only the two dedicated test characters may enter the HP0 verification path.");
            var focus = EditorWindow.focusedWindow;
            bool previousBackground = Application.runInBackground;
            try
            {
                Application.runInBackground = true;
                mouse = InputSystem.AddDevice<Mouse>();
                keyboard = InputSystem.AddDevice<Keyboard>();
                var gameView = FindGameView();
                Assert.That(gameView != null, Is.True, "Open the Game view before HP-zero observation.");
                gameView.Focus();
                yield return null;
                Assert.That(Application.isFocused, Is.True, "Unity Editor OS focus is required for HP-zero input checks.");
                BindLiveWorld();
                Assert.That(world.SelectedCharacterID == expectedCharacterId && world.ZoneID == 1 && world.Stage == C02Stage.InWorld && world.HasOwnFull, Is.True,
                    "The expected dedicated test character must be connected in Zone 1 with its own Full.");
                Subscribe();
                yield return WaitFor(() => players.ContainsKey(world.InternalID), 15f,
                    "fresh independent raw Full for the expected test character after subscribing");
                AssertOwnVitalsMatchRaw();
                bool enteredAtZero = players[world.InternalID].hp == 0;
                Assert.That(expectedCharacterId != 5011UL || enteredAtZero, Is.True,
                    "Account1 must already have fresh raw HP0; do not wait for or induce additional damage if it is alive.");
                var bossIdMaybe = FindMonsterType(2);
                if (!enteredAtZero)
                    yield return WaitFor(() => FindMonsterType(2).HasValue, 15f, "independent center boss Full");
                bossIdMaybe = FindMonsterType(2);
                Assert.That(enteredAtZero || bossIdMaybe.HasValue, Is.True,
                    "The center boss must be independently visible before the HP-zero observation phase.");
                var bossId = bossIdMaybe.HasValue ? bossIdMaybe.Value.ToString(CultureInfo.InvariantCulture) : "unobserved";
                var deadline = Time.realtimeSinceStartup + 85f;
                int priorHp = world.OwnFull.hp;
                File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                    "INFO " + accountLabel + " HP0 observation begins; ownID={0}; boss={1}; ownHP={2}; ownMP={3}{4}",
                    world.InternalID, bossId, priorHp, world.OwnFull.mp, Environment.NewLine));
                if (enteredAtZero) File.AppendAllText(ResultPath,
                    "OBSERVED " + accountLabel + " already HP0 on fresh phase entry; no new death transition claimed. Existing HP0 guards/disconnect are still checked.\n");
                while (world.OwnFull.hp > 0 && Time.realtimeSinceStartup < deadline)
                    yield return null;

                if (world.OwnFull.hp == 0)
                {
                    Assert.That(players.TryGetValue(world.InternalID, out var rawOwn) && rawOwn.hp == 0, Is.True,
                        "An independent raw server Full/Delta observer must also confirm own HP zero.");
                    var vitals = FindCombatText("vitalsText");
                    var status = FindCombatText("combatStatusText");
                    Assert.That(vitals.text.Contains("HP 0 /"), Is.True, "C05 vitals must show the server-confirmed own HP zero.");
                    Assert.That(status.text.Contains("HP 0") && status.text.Contains("stopped"), Is.True,
                        "C05 must show the HP-zero movement/combat stop state.");
                    Assert.That(world.CanAct, Is.False, "The existing world view must stop actions at server-confirmed HP zero.");
                    var skillButtons = GetSkillButtons();
                    foreach (var button in skillButtons) Assert.That(button.interactable, Is.False);
                    int actionCountAtZero = CountOwnPhasesAnySlot();
                    int mpAtZero = world.OwnFull.mp;
                    var positionAtZero = world.ServerPosition;
                    yield return PressSkillKey(Key.Digit1);
                    var cameraAtZero = GameObject.Find("QuarterViewCamera")?.GetComponent<Camera>();
                    Assert.That(cameraAtZero != null, Is.True, "The current camera is required for the HP0 ground-click guard check.");
                    var groundScreen = cameraAtZero.WorldToScreenPoint(new Vector3(positionAtZero.x + 2f, 0, positionAtZero.y));
                    Assert.That(groundScreen.z > 0 && groundScreen.x >= 0 && groundScreen.x < Screen.width &&
                        groundScreen.y >= 0 && groundScreen.y < Screen.height, Is.True,
                        "The HP0 input-blocking ground click must be a visible native click.");
                    yield return ClickAt(new Vector2(groundScreen.x, groundScreen.y));
                    yield return WaitSeconds(0.8f);
                    Assert.That(world.ServerPosition, Is.EqualTo(positionAtZero), "HP0 ground click must not move the server-confirmed position.");
                    Assert.That(world.OwnFull.mp, Is.EqualTo(mpAtZero), "HP0 skill input must not consume MP.");
                    Assert.That(CountOwnPhasesAnySlot(), Is.EqualTo(actionCountAtZero),
                        "HP0 skill input must not produce another own server phase.");
                    File.AppendAllText(ResultPath, "PASS " + accountLabel + " HP0 native key/ground-click guard: no new action phase, MP use or server movement.\n");
                    File.AppendAllText(ResultPath, enteredAtZero
                        ? "PASS " + accountLabel + " existing raw HP0 state, C05 vitals, action buttons disabled, and CanAct=false; new transition NOT OBSERVED.\n"
                        : "PASS " + accountLabel + " observed raw HP-to-zero transition, C05 vitals, action buttons disabled, and CanAct=false.\n");
                    yield return Capture(expectedCharacterId == 5011UL ? "c05-hp0-account1.png" : "c05-hp0-account2.png");
                }
                else
                    File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                        "NOT OBSERVED " + accountLabel + " own HP0 within 85s; serverHP={0}; boss={1}; no state was changed or restored.{2}",
                        world.OwnFull.hp, bossId, Environment.NewLine));

                var disconnect = GameObject.Find("DisconnectButton")?.GetComponent<Button>();
                Assert.That(disconnect != null && disconnect.interactable, Is.True,
                    "The real existing disconnect button must remain available after the bounded HP0 phase.");
                yield return ClickButton(disconnect);
                yield return WaitFor(() => !world.IsWorldVisible, 8f, "normal expected-character disconnect cleanup");
                File.AppendAllText(ResultPath, "PASS " + accountLabel + " normal UI disconnect; no revive or database restoration attempted.\n");
            }
            finally
            {
                Unsubscribe();
                RemoveTestDevices();
                Application.runInBackground = previousBackground;
                if (focus != null) focus.Focus();
            }
        }

        private void BindLiveWorld()
        {
            login = GameObject.Find("C01Client")?.GetComponent<C01LoginScreen>();
            world = GameObject.Find("C02Client")?.GetComponent<C02WorldScreen>();
            combat = GameObject.Find("C02Client")?.GetComponent<C05CombatScreen>();
            observed = GameObject.Find("C02Client")?.GetComponent<C04ObservedWorld>();
            Assert.That(login != null && world != null && combat != null && observed != null && login.Model != null,
                Is.True, "C01, C02, C04 and C05 live components must be present in the active scene.");
            model = login.Model;
        }

        private void Subscribe()
        {
            fullHandler = (count, states) => { for (int i = 0; i < count; i++) players[states[i].zoneInternalID] = states[i]; };
            deltaHandler = ApplyPlayerDelta;
            monsterFullHandler = ApplyMonsterFull;
            monsterDeltaHandler = ApplyMonsterDelta;
            actionHandler = (count, states) => { for (int i = 0; i < count; i++) actionResults.Add(states[i]); };
            model.OnFullReceived += fullHandler;
            model.OnDeltaReceived += deltaHandler;
            model.OnMonsterFullReceived += monsterFullHandler;
            model.OnMonsterDeltaReceived += monsterDeltaHandler;
            model.OnActionResultReceived += actionHandler;
        }

        private void ApplyMonsterFull(ushort count, MonsterFullField[] states)
        {
            for (int i = 0; i < count; i++)
            {
                monsters[states[i].internalId] = states[i];
                double seenAt = Time.realtimeSinceStartupAsDouble;
                lastMonsterFull[states[i].internalId] = states[i];
                ushort id = states[i].internalId;
                if (states[i].hp > 0 && firstMonsterZeroAt.TryGetValue(id, out var zeroAt) && seenAt > zeroAt &&
                    !firstAliveFullAfterZeroAt.ContainsKey(id))
                {
                    firstAliveFullAfterZeroAt[id] = seenAt;
                    firstAliveFullAfterZero[id] = states[i];
                }
                ObserveAliveVisual(id);
            }
        }

        private void Unsubscribe()
        {
            if (model == null) return;
            if (fullHandler != null) model.OnFullReceived -= fullHandler;
            if (deltaHandler != null) model.OnDeltaReceived -= deltaHandler;
            if (monsterFullHandler != null) model.OnMonsterFullReceived -= monsterFullHandler;
            if (monsterDeltaHandler != null) model.OnMonsterDeltaReceived -= monsterDeltaHandler;
            if (actionHandler != null) model.OnActionResultReceived -= actionHandler;
            model = null;
        }

        private void ApplyPlayerDelta(ushort count, DeltaUpdateField[] updates)
        {
            for (int i = 0; i < count; i++)
            {
                var delta = updates[i];
                if (!players.TryGetValue(delta.zoneInternalID, out var state)) continue;
                if (delta.fieldID == 0) state.hp = unchecked((int)delta.fieldVal);
                else if (delta.fieldID == 1) state.mp = unchecked((int)delta.fieldVal);
                else if (delta.fieldID == 2) state.maxHp = unchecked((int)delta.fieldVal);
                else if (delta.fieldID == 3) state.maxMp = unchecked((int)delta.fieldVal);
                players[delta.zoneInternalID] = state;
            }
        }

        private void ApplyMonsterDelta(ushort count, MonsterDeltaField[] updates)
        {
            for (int i = 0; i < count; i++)
            {
                var update = updates[i];
                if (update.fieldId == 0 && update.fieldVal == 0 && !firstMonsterZeroAt.ContainsKey(update.internalId))
                {
                    firstMonsterZeroAt[update.internalId] = Time.realtimeSinceStartupAsDouble;
                    if (!MonsterVisualExists(update.internalId)) zeroVisualRemoved.Add(update.internalId);
                }
                if (!monsters.TryGetValue(update.internalId, out var state)) continue;
                if (update.fieldId == 0) state.hp = unchecked((int)update.fieldVal);
                else if (update.fieldId == 1) state.x = BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0);
                else if (update.fieldId == 2) state.y = BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0);
                else if (update.fieldId == 3 && update.fieldVal <= 3) state.dir = (byte)update.fieldVal;
                monsters[update.internalId] = state;
            }
        }

        private ushort? FindTypeZeroMonsterWithin(float range)
        {
            foreach (var pair in monsters)
                if (pair.Value.monsterId == 0 && pair.Value.hp > 0 &&
                    Vector2.Distance(world.ServerPosition, new Vector2(pair.Value.x, pair.Value.y)) <= range)
                    return pair.Key;
            return null;
        }

        private ushort? FindMonsterType(ushort type)
        {
            foreach (var pair in monsters) if (pair.Value.monsterId == type && pair.Value.hp > 0) return pair.Key;
            return null;
        }

        private bool MonsterVisualExists(ushort id)
        {
            var table = (IDictionary)GetField(observed, "monsters");
            if (!table.Contains(id)) return false;
            var visual = table[id].GetType().GetField("Visual").GetValue(table[id]) as GameObject;
            return visual != null && visual.activeInHierarchy;
        }

        private bool HasRespawnFull(ushort id)
        {
            if (!lastMonsterFull.TryGetValue(id, out var state) || state.hp <= 0 ||
                !firstMonsterZeroAt.TryGetValue(id, out var zeroAt) || !firstAliveFullAfterZeroAt.ContainsKey(id) ||
                !firstAliveVisualAfterZeroAt.ContainsKey(id) || !MonsterVisualExists(id)) return false;
            var table = (IDictionary)GetField(observed, "monsters");
            var entry = table[id];
            var last = (double)entry.GetType().GetField("LastFullSeen").GetValue(entry);
            var displayedState = entry.GetType().GetField("State").GetValue(entry);
            return last > zeroAt && (int)displayedState.GetType().GetField("hp").GetValue(displayedState) > 0;
        }

        private void ObserveAliveVisual(ushort id)
        {
            if (firstAliveFullAfterZeroAt.ContainsKey(id) && !firstAliveVisualAfterZeroAt.ContainsKey(id) && MonsterVisualExists(id))
                firstAliveVisualAfterZeroAt[id] = Time.realtimeSinceStartupAsDouble;
        }

        private Dictionary<ushort, int> SnapshotMonsterHp()
        {
            var hp = new Dictionary<ushort, int>();
            foreach (var pair in monsters)
                if (pair.Value.hp > 0 && MonsterVisualExists(pair.Key)) hp[pair.Key] = pair.Value.hp;
            return hp;
        }

        private ushort? FindDamagedTypeZero(Dictionary<ushort, int> baseline)
        {
            foreach (var pair in baseline)
                if (monsters.TryGetValue(pair.Key, out var state) && state.monsterId == 0 && state.hp < pair.Value) return pair.Key;
            return null;
        }

        private void AssertOwnVitalsMatchRaw()
        {
            var raw = players[world.InternalID];
            var confirmed = world.OwnFull;
            Assert.That(confirmed.hp, Is.EqualTo(raw.hp)); Assert.That(confirmed.mp, Is.EqualTo(raw.mp));
            string expected = string.Format(CultureInfo.InvariantCulture, "HP {0} / {1}    MP {2} / {3}", raw.hp, raw.maxHp, raw.mp, raw.maxMp);
            Assert.That(FindCombatText("vitalsText").text, Is.EqualTo(expected), "C05 displayed numbers must match independent raw and C02 confirmed values in this same frame.");
            File.AppendAllText(ResultPath, $"PASS raw/C02/C05 vitals same-frame agreement: {expected}\n");
        }

        private bool SawOwnPhase(byte slot, ushort phase) => actionResults.Exists(value =>
            value.casterType == 0 && value.casterId == world.InternalID && value.skillSlot == slot &&
            value.skillId == slot && value.skillPhase == phase);
        private int CountOwnPhasesAnySlot() => actionResults.FindAll(value =>
            value.casterType == 0 && value.casterId == world.InternalID && value.skillSlot <= 2 &&
            value.skillId == value.skillSlot && value.skillPhase <= 1).Count;
        private int CountOwnPhases(byte slot) => actionResults.FindAll(value =>
            value.casterType == 0 && value.casterId == world.InternalID && value.skillSlot == slot && value.skillId == slot).Count;

        private IEnumerator PressSkillKey(Key key)
        {
            InputSystem.QueueStateEvent(keyboard, new KeyboardState(key));
            yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(keyboard, new KeyboardState());
            yield return NextPlayerFrame();
        }

        private IEnumerator MoveAlongVisiblePoints(Camera camera, C02WorldScreen targetWorld, Transform own, Vector2 destination)
        {
            double approachDeadline = Time.realtimeSinceStartupAsDouble + 120;
            yield return WaitFor(() => GetField(targetWorld, "moveTarget") == null, 12f,
                "existing native movement to finish before the approach");
            for (int step = 0; step < 12 && Vector2.Distance(targetWorld.ServerPosition, destination) > 1.8f; step++)
            {
                // Camera follows confirmed coordinates. Keep it still while the native move/down/up
                // frames use the screen projection; never cancel the product's target from the helper.
                yield return NextPlayerFrame();
                yield return NextPlayerFrame();
                Assert.That(Time.realtimeSinceStartupAsDouble < approachDeadline, Is.True,
                    "The native approach must remain inside its 120-second budget.");
                var start = targetWorld.ServerPosition;
                var point = Vector2.MoveTowards(start, destination, Mathf.Min(5f, Vector2.Distance(start, destination)));
                var screen = camera.WorldToScreenPoint(new Vector3(point.x, 0, point.y));
                Assert.That(screen.z > 0 && screen.x >= 0 && screen.x < Screen.width && screen.y >= 0 && screen.y < Screen.height,
                    Is.True, "Boss approach must use visible in-zone world clicks.");
                var startDistance = Vector2.Distance(start, destination);
                RecordApproachClick(targetWorld, screen, point, startDistance);
                yield return ClickAt(new Vector2(screen.x, screen.y));
                yield return WaitFor(() => Vector2.Distance(targetWorld.ServerPosition, destination) < startDistance - 0.75f ||
                    Vector2.Distance(targetWorld.ServerPosition, destination) <= 1.8f,
                    Mathf.Max(0, Mathf.Min(12f, (float)(approachDeadline - Time.realtimeSinceStartupAsDouble))),
                    "server-confirmed progress along the visible boss approach");
                yield return WaitFor(() => GetField(targetWorld, "moveTarget") == null,
                    Mathf.Max(0, Mathf.Min(12f, (float)(approachDeadline - Time.realtimeSinceStartupAsDouble))),
                    "current native approach target to finish before the next projection");
                File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                    "INFO native approach step {0}; requested=({1:F2},{2:F2}); server=({3:F2},{4:F2}); remaining={5:F2}{6}",
                    step + 1, point.x, point.y, targetWorld.ServerPosition.x, targetWorld.ServerPosition.y,
                    Vector2.Distance(targetWorld.ServerPosition, destination), Environment.NewLine));
            }
            yield return WaitFor(() => GetField(targetWorld, "moveTarget") == null,
                Mathf.Max(0, Mathf.Min(12f, (float)(approachDeadline - Time.realtimeSinceStartupAsDouble))),
                "final native approach target to finish before returning");
            Assert.That(Vector2.Distance(targetWorld.ServerPosition, destination), Is.LessThanOrEqualTo(1.8f),
                "The final server-confirmed position must be within the observed 1.8-unit approach tolerance.");
            Assert.That(own.position, Is.EqualTo(new Vector3(targetWorld.ServerPosition.x, 0, targetWorld.ServerPosition.y)));
        }

        private static Button GetSkillButton(int index)
        {
            var component = GameObject.Find("C02Client")?.GetComponent<C05CombatScreen>();
            var buttons = (Button[])GetField(component, "skillButtons");
            return buttons[index];
        }
        private Button[] GetSkillButtons() => (Button[])GetField(combat, "skillButtons");
        private TMPro.TMP_Text FindCombatText(string name) => (TMPro.TMP_Text)GetField(combat, name);
        private static object GetField(object target, string name) =>
            target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private IEnumerator ClickButton(Selectable target)
        {
            var rect = target.transform as RectTransform;
            Assert.That(rect != null, Is.True, "Existing skill/disconnect button requires a RectTransform.");
            yield return ClickAt(RectTransformUtility.WorldToScreenPoint(null, rect.position));
        }
        private IEnumerator ClickAt(Vector2 position)
        {
            InputSystem.QueueStateEvent(mouse, new MouseState { position = position }); yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = position }.WithButton(MouseButton.Left)); yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = position }); yield return NextPlayerFrame();
        }
        private static IEnumerator NextPlayerFrame()
        {
            int frame = Time.frameCount; double deadline = EditorApplication.timeSinceStartup + 3;
            EditorApplication.QueuePlayerLoopUpdate();
            while (Time.frameCount == frame && EditorApplication.timeSinceStartup < deadline) yield return null;
            Assert.That(Time.frameCount > frame, Is.True, "Queued native input requires a real Player frame.");
        }
        private IEnumerator WaitFor(Func<bool> condition, float seconds, string label)
        {
            RecordWait("begin", label);
            float deadline = Time.realtimeSinceStartup + seconds;
            while (!condition() && Time.realtimeSinceStartup < deadline) yield return null;
            bool passed = condition();
            RecordWait(passed ? "pass" : "timeout", label);
            Assert.That(passed, Is.True, "Timed out waiting for " + label + ".");
        }

        private void RecordWait(string transition, string label)
        {
            // All callers supply fixed source labels. Log only numeric/boolean state, never credentials,
            // model strings or exception text; the shared C01 runner keeps its existing redaction policy.
            bool hasRaw = world != null && players.ContainsKey(world.InternalID);
            var raw = hasRaw ? players[world.InternalID] : default;
            var own = world != null ? world.OwnFull : default;
            File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                "WAIT {0}: {1}; stage={2}; zone={3}; ownID={4}; ownFull={5}; canAct={6}; rawOwn={7}; players={8}; monsters={9}; ownPhases={10}; hp={11}; mp={12}; rawHp={13}; rawMp={14}; x={15:F3}; y={16:F3}; moveTarget={17}; appFocus={18}{19}",
                transition, label, world != null ? (int)world.Stage : -1, world != null ? world.ZoneID : 0,
                world != null ? world.InternalID : 0, world != null && world.HasOwnFull, world != null && world.CanAct,
                hasRaw, players.Count, monsters.Count, world != null ? CountOwnPhasesAnySlot() : 0,
                world != null ? own.hp : -1, world != null ? own.mp : -1, hasRaw ? raw.hp : -1, hasRaw ? raw.mp : -1,
                own.x, own.y, world != null && GetField(world, "moveTarget") != null, Application.isFocused, Environment.NewLine));
        }

        private void RecordApproachClick(C02WorldScreen targetWorld, Vector3 screen, Vector2 point, float distance)
        {
            var events = EventSystem.current;
            var hits = new List<RaycastResult>();
            if (events != null) events.RaycastAll(new PointerEventData(events) { position = new Vector2(screen.x, screen.y) }, hits);
            bool graphicHit = hits.Exists(hit => hit.module is UnityEngine.UI.GraphicRaycaster);
            var hud = GetField(targetWorld, "worldHud") as GameObject;
            var rect = hud != null ? hud.transform as RectTransform : null;
            var canvas = hud != null ? hud.GetComponentInParent<Canvas>() : null;
            bool hudContains = hud != null && hud.activeInHierarchy && rect != null && RectTransformUtility.RectangleContainsScreenPoint(
                rect, new Vector2(screen.x, screen.y), canvas != null && canvas.renderMode != RenderMode.ScreenSpaceOverlay ? canvas.worldCamera : null);
            File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                "APPROACH click diagnostic; screen=({0:F1},{1:F1}); target=({2:F3},{3:F3}); remaining={4:F3}; eventSystem={5}; graphicRaycasterHit={6}; hitCount={7}; hitHUDrect={8}{9}",
                screen.x, screen.y, point.x, point.y, distance, events != null, graphicHit, hits.Count, hudContains, Environment.NewLine));
        }
        private static IEnumerator WaitForOptional(Func<bool> condition, float seconds)
        {
            float deadline = Time.realtimeSinceStartup + seconds;
            while (!condition() && Time.realtimeSinceStartup < deadline) yield return null;
        }
        private static IEnumerator WaitSeconds(float seconds)
        {
            float deadline = Time.realtimeSinceStartup + seconds;
            while (Time.realtimeSinceStartup < deadline) yield return null;
        }
        private static IEnumerator Capture(string fileName)
        {
            Assert.That(fileName == "c05-combat-account1.png" || fileName == "c05-boss-account2.png" || fileName == "c05-hp0-account2.png" || fileName == "c05-hp0-account1.png",
                Is.True, "Only this helper's four dedicated C05 output files may be replaced.");
            var path = Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures", fileName));
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            if (File.Exists(path)) File.Delete(path);
            ScreenCapture.CaptureScreenshot(path);
            float deadline = Time.realtimeSinceStartup + 5f;
            while ((!File.Exists(path) || new FileInfo(path).Length == 0) && Time.realtimeSinceStartup < deadline) yield return null;
            Assert.That(File.Exists(path) && new FileInfo(path).Length > 0, Is.True, "C05 screenshot was not created.");
        }
        private void RecordObservedPhase(byte slot, string input, int previousPhaseCount = 0)
        {
            var result = CountOwnPhases(slot) > previousPhaseCount ? "PASS: new server phase observed" : "NOT OBSERVED: new server phase";
            File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture, "{0} for {1}; damage is judged only by HP deltas.{2}", result, input, Environment.NewLine));
        }
        private void RemoveTestDevices()
        {
            if (keyboard != null && keyboard.added) InputSystem.RemoveDevice(keyboard);
            if (mouse != null && mouse.added) InputSystem.RemoveDevice(mouse);
            keyboard = null; mouse = null;
        }
        private static EditorWindow FindGameView()
        {
            foreach (var window in Resources.FindObjectsOfTypeAll<EditorWindow>())
                if (window != null && window.GetType().Name == "GameView") return window;
            return null;
        }
        private string ResultPath => resultPath;

        private void PreserveAccountOneEntryEvidence()
        {
            var captures = Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures"));
            var source = Path.Combine(captures, "c02-live-entry.png");
            var destination = Path.Combine(captures, "c05-account1-entry.png");
            if (!File.Exists(source))
            {
                File.AppendAllText(ResultPath, "NOT OBSERVED account-one B01 entry PNG was not present to preserve before account-two entry.\n");
                return;
            }
            if (File.Exists(destination))
                destination = Path.Combine(captures, "c05-account1-entry-" + DateTime.UtcNow.ToString("yyyyMMddTHHmmssZ", CultureInfo.InvariantCulture) + ".png");
            File.Copy(source, destination);
            File.AppendAllText(ResultPath, "PASS preserved account-one B01 entry PNG before the account-two login overwrites the C02 standard capture.\n");
        }

        // Detached oracle regression only. This never starts a live scenario or queues input.
        public static string RunOracleChecks()
        {
            var fixtureRoot = new GameObject("C05 oracle detached observer"); fixtureRoot.SetActive(false);
            var visual = new GameObject("C05 oracle detached visual");
            try
            {
                var helper = new C05LiveBatchUiTests { observed = fixtureRoot.AddComponent<C04ObservedWorld>() };
                var table = (IDictionary)GetField(helper.observed, "monsters");
                var entryType = typeof(C04ObservedWorld).GetNestedType("MonsterObservation", BindingFlags.NonPublic);
                var entry = Activator.CreateInstance(entryType, true);
                var state = new MonsterFullField { internalId = 0, monsterId = 0, hp = 100, maxHp = 100 };
                entryType.GetField("Visual").SetValue(entry, visual);
                entryType.GetField("State").SetValue(entry, state);
                table.Add((ushort)0, entry);
                helper.monsters[0] = state;
                var baseline = helper.SnapshotMonsterHp();
                helper.ApplyMonsterDelta(1, new[] { new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 50 } });
                Assert.That(helper.FindDamagedTypeZero(baseline), Is.EqualTo((ushort)0), "ID zero is a real damaged type0 candidate.");
                visual.SetActive(false);
                Assert.That(helper.MonsterVisualExists(0), Is.False, "A retained dictionary entry with an inactive visual is not a rendered monster.");
                var zero = new[] { new MonsterDeltaField { internalId = 0, fieldId = 0, fieldVal = 0 } };
                helper.ApplyMonsterDelta(1, zero);
                double firstZero = helper.firstMonsterZeroAt[0];
                helper.ApplyMonsterDelta(1, zero);
                Assert.That(helper.firstMonsterZeroAt[0], Is.EqualTo(firstZero), "Repeated zero packets cannot reset the first death clock.");
                Assert.That(helper.zeroVisualRemoved.Contains(0), Is.True);
                // A synthetic one-second gap tests early Full ordering without waiting or pretending live timing.
                helper.firstMonsterZeroAt[0] = Time.realtimeSinceStartupAsDouble - 1;
                entryType.GetField("LastFullSeen").SetValue(entry, Time.realtimeSinceStartupAsDouble);
                helper.ApplyMonsterFull(1, new[] { state });
                double firstAlive = helper.firstAliveFullAfterZeroAt[0];
                Assert.That(firstAlive - helper.firstMonsterZeroAt[0], Is.LessThan(14));
                Assert.That(helper.HasRespawnFull(0), Is.False, "Raw alive Full alone cannot restore an inactive visual.");
                visual.SetActive(true);
                helper.ObserveAliveVisual(0);
                Assert.That(helper.HasRespawnFull(0), Is.True);
                state.hp = 75;
                helper.ApplyMonsterFull(1, new[] { state });
                Assert.That(helper.firstAliveFullAfterZeroAt[0], Is.EqualTo(firstAlive));
                Assert.That(helper.firstAliveFullAfterZero[0].hp, Is.EqualTo(100), "Later repeating Full cannot replace first reappearance evidence.");
                visual.SetActive(false);
                Assert.That(helper.HasRespawnFull(0), Is.False);
                return "C05 helper oracle checks passed: ID0 damage selection, first HP0/early alive Full retention, inactive/active visual evidence. No Play/input/server/session.";
            }
            finally { UnityEngine.Object.DestroyImmediate(visual); UnityEngine.Object.DestroyImmediate(fixtureRoot); }
        }

        public static void StartAccountOneCombat()
        {
            var instance = new C05LiveBatchUiTests();
            Start(instance, instance.AccountOneCombatAndMonsterRespawn(), "c05-account1-live.txt",
                "COMPLETED: account-one native C05 combat/monster respawn phase; inspect per-slot observations in the result file.");
        }
        public static void StartAccountTwoHpZero()
        {
            var instance = new C05LiveBatchUiTests();
            Start(instance, instance.AccountTwoHpZeroAndDisconnect(), "c05-account2-hp0.txt",
                "COMPLETED: account-two bounded 85-second boss/HP0/disconnect phase; inspect HP0 observation status in the result file.");
        }
        public static void StartAccountOneHpZero()
        {
            var instance = new C05LiveBatchUiTests();
            Start(instance, instance.AccountOneHpZeroAndDisconnect(), "c05-account1-hp0.txt",
                "COMPLETED: account-one existing HP0 guards/disconnect phase; no new death transition claimed.");
        }
        public static void StartAccountTwoLogin()
        {
            var instance = new C05LiveBatchUiTests();
            Start(instance, instance.AccountTwoLogin(), "c05-account2-login.txt",
                "COMPLETED: account-two real UI login and entry; inspect the entry result file.");
        }
        public static void StartAccountTwoBossAggro()
        {
            var instance = new C05LiveBatchUiTests();
            Start(instance, instance.AccountTwoBossAggro(), "c05-account2-boss-approach.txt",
                "COMPLETED: account-two native center approach and boss aggro observation; inspect result file.");
        }
        private static void Start(C05LiveBatchUiTests instance, IEnumerator scenario, string resultName, string passMessage)
        {
            if (!EditorApplication.isPlaying) throw new InvalidOperationException("C05 live UI phase requires Play mode.");
            var runner = FindLoadedType("C01.Tests.C01EditorInputRun");
            var method = runner.GetMethod("Start", new[] { typeof(IEnumerator), typeof(string), typeof(string) });
            var path = Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures", resultName));
            instance.resultPath = path;
            method.Invoke(null, new object[] { scenario, path, passMessage });
        }
        private static Type FindLoadedType(string name)
        {
            foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                var type = assembly.GetType(name, false);
                if (type != null) return type;
            }
            throw new InvalidOperationException("The shared C01 Editor-update runner is not loaded.");
        }
    }
}
#endif
