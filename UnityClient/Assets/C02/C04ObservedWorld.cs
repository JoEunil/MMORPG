using System;
using System.Collections.Generic;
using C01;
using ClientCore;
using ClientCore.PacketHelper;
using UnityEngine;

namespace C02
{
    public sealed class C04ObservedWorld : MonoBehaviour
    {
        private const double PresentationDelay = 0.075;
        private const int MotionSampleCapacity = 4;

        private struct MotionSample
        {
            public double Time;
            public float X;
            public float Y;
        }

        private sealed class MotionHistory
        {
            private readonly MotionSample[] samples = new MotionSample[MotionSampleCapacity];
            private int count;

            public void Reset(double time, float x, float y)
            {
                count = 1;
                samples[0] = new MotionSample { Time = time, X = x, Y = y };
            }

            public void Add(double time, float x, float y)
            {
                if (count != 0 && time <= samples[count - 1].Time)
                {
                    samples[count - 1] = new MotionSample { Time = time, X = x, Y = y };
                    return;
                }
                if (count == samples.Length)
                {
                    Array.Copy(samples, 1, samples, 0, samples.Length - 1);
                    count--;
                }
                samples[count++] = new MotionSample { Time = time, X = x, Y = y };
            }

            public Vector3 PositionAt(double time)
            {
                if (count == 1 || time <= samples[0].Time)
                    return new Vector3(samples[0].X, 0, samples[0].Y);
                for (int i = 1; i < count; i++)
                {
                    var next = samples[i];
                    if (time > next.Time) continue;
                    var previous = samples[i - 1];
                    float amount = (float)((time - previous.Time) / (next.Time - previous.Time));
                    return new Vector3(Mathf.Lerp(previous.X, next.X, amount), 0,
                        Mathf.Lerp(previous.Y, next.Y, amount));
                }
                var latest = samples[count - 1];
                return new Vector3(latest.X, 0, latest.Y);
            }
        }

        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private C02WorldScreen worldScreen;
        [SerializeField] private Transform displayRoot;
        [SerializeField] private GameObject playerPrefab;
        [SerializeField] private GameObject[] monsterPrefabs;
        [SerializeField] private C14ServerContent serverContent;
        [SerializeField] private Material monsterRangeMaterial;

        private sealed class PlayerObservation
        {
            public FullStateField State;
            public GameObject Visual;
            public C19ActorMotion ActorMotion;
            public double LastFullSeen;
            public MotionHistory Motion = new MotionHistory();
        }

        private sealed class MonsterObservation
        {
            public MonsterFullField State;
            public GameObject Visual;
            public C13EntityVitals Vitals;
            public C19ActorMotion ActorMotion;
            public LineRenderer RangePreview;
            public double RangeUntil;
            public LineRenderer BossFanPreviewFirst;
            public double BossFanUntilFirst;
            public LineRenderer BossFanPreviewSecond;
            public double BossFanUntilSecond;
            public readonly LineRenderer[] BossSquareOuterPreviews = new LineRenderer[5];
            public readonly LineRenderer[] BossSquareInnerPreviews = new LineRenderer[5];
            public readonly double[] BossSquareUntil = new double[5];
            public double LastFullSeen;
            public MotionHistory Motion = new MotionHistory();
        }

        private readonly Dictionary<uint, PlayerObservation> players = new Dictionary<uint, PlayerObservation>();
        private readonly Dictionary<ushort, MonsterObservation> monsters = new Dictionary<ushort, MonsterObservation>();
        private IViewModelUI model;
        private int generation;
        private ushort observedZone;
        private uint ownId;
        private Action<ushort, FullStateField[]> playerFull;
        private Action<ushort, DeltaUpdateField[]> playerDelta;
        private Action<ushort, MonsterFullField[]> monsterFull;
        private Action<ushort, MonsterDeltaField[]> monsterDelta;
        private Action<ushort, ActionResultField[]> actionResults;

        private void OnEnable()
        {
            if (loginScreen == null || worldScreen == null || displayRoot == null || playerPrefab == null ||
                monsterPrefabs == null || monsterPrefabs.Length != 3 || Array.Exists(monsterPrefabs, prefab => prefab == null))
            {
                Debug.LogError("C04 observation references are missing.", this);
                enabled = false;
                return;
            }
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            (worldScreen.Stage == C02Stage.InWorld || worldScreen.Stage == C02Stage.TransitionPending) && worldScreen.HasOwnFull && displayRoot != null &&
            displayRoot.gameObject.activeInHierarchy;

        private void ChangeClient(IViewModelUI next)
        {
            Unsubscribe();
            generation++;
            ClearObservations();
            observedZone = 0;
            ownId = 0;
            model = next;
            Subscribe();
            WorldChanged();
        }

        private void Subscribe()
        {
            if (model == null) return;
            var source = model;
            int version = generation;
            playerFull = (count, states) => { if (Accept(source, version)) ApplyPlayerFull(count, states, Time.realtimeSinceStartupAsDouble); };
            playerDelta = (count, updates) => { if (Accept(source, version)) ApplyPlayerDelta(count, updates); };
            monsterFull = (count, states) => { if (Accept(source, version)) ApplyMonsterFull(count, states, Time.realtimeSinceStartupAsDouble); };
            monsterDelta = (count, updates) => { if (Accept(source, version)) ApplyMonsterDelta(count, updates); };
            actionResults = (count, results) => { if (Accept(source, version)) ApplyMonsterActionResults(count, results); };
            model.OnFullReceived += playerFull;
            model.OnDeltaReceived += playerDelta;
            model.OnMonsterFullReceived += monsterFull;
            model.OnMonsterDeltaReceived += monsterDelta;
            model.OnActionResultReceived += actionResults;
        }

        private void Unsubscribe()
        {
            if (model == null) return;
            model.OnFullReceived -= playerFull;
            model.OnDeltaReceived -= playerDelta;
            model.OnMonsterFullReceived -= monsterFull;
            model.OnMonsterDeltaReceived -= monsterDelta;
            model.OnActionResultReceived -= actionResults;
        }

        private void WorldChanged()
        {
            if (WorldAvailable)
            {
                if (observedZone == 0)
                {
                    // Opening the first own Full keeps the current packet's captured handlers valid.
                    observedZone = worldScreen.ZoneID;
                    ownId = worldScreen.InternalID;
                    return;
                }
                if (observedZone == worldScreen.ZoneID && ownId == worldScreen.InternalID) return;
            }
            Unsubscribe();
            generation++;
            ClearObservations();
            observedZone = WorldAvailable ? worldScreen.ZoneID : (ushort)0;
            ownId = WorldAvailable ? worldScreen.InternalID : 0;
            Subscribe();
        }

        private bool Accept(IViewModelUI source, int version)
        {
            if (!isActiveAndEnabled || source != model || version != generation || !WorldAvailable) return false;
            WorldChanged();
            return source == model && version == generation;
        }

        private void Update()
        {
            if (!WorldAvailable || model == null)
            {
                if (observedZone != 0 || players.Count != 0 || monsters.Count != 0) WorldChanged();
                return;
            }
            WorldChanged();
            // A delayed bootstrap must not age out the field being shown during transition.
            if (worldScreen.Stage != C02Stage.TransitionPending) Expire(Time.realtimeSinceStartupAsDouble);
            RenderObservations(Time.realtimeSinceStartupAsDouble);
        }

        private void ApplyPlayerFull(ushort count, FullStateField[] states, double now)
        {
            if (states == null || count > states.Length) return;
            for (int i = 0; i < count; i++)
            {
                var state = states[i];
                if (state.zoneInternalID == ownId || state.zoneInternalID == 0 || !ValidPlayer(state)) continue;
                if (!players.TryGetValue(state.zoneInternalID, out var entry))
                {
                    entry = new PlayerObservation { Visual = Instantiate(playerPrefab, displayRoot) };
                    entry.Visual.name = "ObservedPlayer " + state.zoneInternalID;
                    entry.ActorMotion = entry.Visual.AddComponent<C19ActorMotion>();
                    entry.ActorMotion.Configure();
                    Face(entry.Visual, state.dir);
                    entry.Motion.Reset(now, state.x, state.y);
                    entry.Visual.transform.position = new Vector3(state.x, 0, state.y);
                    players.Add(state.zoneInternalID, entry);
                }
                else
                {
                    entry.Motion.Add(now, state.x, state.y);
                    if (state.hp < entry.State.hp) entry.ActorMotion.PlayHit();
                }
                entry.State = state;
                entry.LastFullSeen = now;
            }
        }

        private void ApplyMonsterFull(ushort count, MonsterFullField[] states, double now)
        {
            if (states == null || count > states.Length) return;
            for (int i = 0; i < count; i++)
            {
                var state = states[i];
                if (!ValidMonster(state)) continue;
                if (state.hp == 0) { RemoveMonster(state.internalId); continue; }
                if (monsters.TryGetValue(state.internalId, out var entry) && entry.State.monsterId != state.monsterId)
                { RemoveMonster(state.internalId); entry = null; }
                if (entry == null)
                {
                    entry = new MonsterObservation { Visual = Instantiate(monsterPrefabs[state.monsterId], displayRoot) };
                    entry.Visual.name = "ObservedMonster " + state.internalId + " type " + state.monsterId;
                    entry.ActorMotion = entry.Visual.AddComponent<C19ActorMotion>();
                    entry.ActorMotion.Configure();
                    entry.Vitals = entry.Visual.AddComponent<C13EntityVitals>();
                    entry.Vitals.SetCamera(worldScreen.ViewCamera);
                    Face(entry.Visual, state.dir);
                    entry.Motion.Reset(now, state.x, state.y);
                    entry.Visual.transform.position = new Vector3(state.x, 0, state.y);
                    monsters.Add(state.internalId, entry);
                }
                else
                {
                    entry.Motion.Add(now, state.x, state.y);
                    if (state.hp < entry.State.hp)
                    {
                        entry.Vitals.PulseDamage();
                        entry.ActorMotion.PlayHit();
                    }
                }
                entry.State = state;
                entry.LastFullSeen = now;
                entry.Vitals.SetHealth(state.hp, state.maxHp);
            }
        }

        private void ApplyPlayerDelta(ushort count, DeltaUpdateField[] updates)
        {
            if (updates == null || count > updates.Length) return;
            var candidates = new Dictionary<uint, (FullStateField state, bool invalid)>();
            for (int i = 0; i < count; i++)
            {
                var update = updates[i];
                if (!players.TryGetValue(update.zoneInternalID, out var entry)) continue;
                if (!candidates.TryGetValue(update.zoneInternalID, out var candidate)) candidate = (entry.State, false);
                switch (update.fieldID)
                {
                    case 0: candidate.state.hp = unchecked((int)update.fieldVal); break;
                    case 2: candidate.state.maxHp = unchecked((int)update.fieldVal); break;
                    case 6:
                        if (update.fieldVal > 3) candidate.invalid = true;
                        else candidate.state.dir = (byte)update.fieldVal;
                        break;
                    case 7: candidate.state.x = FloatBits(update.fieldVal); break;
                    case 8: candidate.state.y = FloatBits(update.fieldVal); break;
                    default: continue;
                }
                candidates[update.zoneInternalID] = candidate;
            }
            foreach (var pair in candidates)
            {
                if (pair.Value.invalid || !ValidPlayer(pair.Value.state)) continue;
                var entry = players[pair.Key];
                if (pair.Value.state.hp < entry.State.hp) entry.ActorMotion.PlayHit();
                entry.Motion.Add(Time.realtimeSinceStartupAsDouble, pair.Value.state.x, pair.Value.state.y);
                entry.State = pair.Value.state;
            }
        }

        private void ApplyMonsterDelta(ushort count, MonsterDeltaField[] updates)
        {
            if (updates == null || count > updates.Length) return;
            var candidates = new Dictionary<ushort, (MonsterFullField state, bool invalid, bool deathSeen)>();
            for (int i = 0; i < count; i++)
            {
                var update = updates[i];
                if (!monsters.TryGetValue(update.internalId, out var entry)) continue;
                if (!candidates.TryGetValue(update.internalId, out var candidate)) candidate = (entry.State, false, false);
                switch (update.fieldId)
                {
                    case 0:
                        candidate.state.hp = unchecked((int)update.fieldVal);
                        candidate.invalid |= candidate.state.hp < 0;
                        candidate.deathSeen |= candidate.state.hp == 0;
                        break;
                    case 1:
                        candidate.state.x = FloatBits(update.fieldVal);
                        candidate.invalid |= float.IsNaN(candidate.state.x) || float.IsInfinity(candidate.state.x);
                        break;
                    case 2:
                        candidate.state.y = FloatBits(update.fieldVal);
                        candidate.invalid |= float.IsNaN(candidate.state.y) || float.IsInfinity(candidate.state.y);
                        break;
                    case 3:
                        if (update.fieldVal > 3) candidate.invalid = true;
                        else candidate.state.dir = (byte)update.fieldVal;
                        break;
                    default: continue;
                }
                candidates[update.internalId] = candidate;
            }
            foreach (var pair in candidates)
            {
                if (pair.Value.invalid || !ValidMonster(pair.Value.state)) continue;
                if (pair.Value.deathSeen) { RemoveMonster(pair.Key); continue; }
                var entry = monsters[pair.Key];
                bool damaged = pair.Value.state.hp < entry.State.hp;
                entry.Motion.Add(Time.realtimeSinceStartupAsDouble, pair.Value.state.x, pair.Value.state.y);
                entry.State = pair.Value.state;
                entry.Vitals.SetHealth(entry.State.hp, entry.State.maxHp);
                if (damaged) { entry.Vitals.PulseDamage(); entry.ActorMotion.PlayHit(); }
            }
        }

        private void PresentActionMotion(ActionResultField result)
        {
            if (serverContent == null || result.dir > 3 || !Finite(result.x) || !Finite(result.y)) return;
            if (result.casterType == 0 && (result.skillSlot > 2 || result.skillId != result.skillSlot) ||
                result.casterType == 1 && result.skillSlot != 0) return;
            var skill = result.skillId == 1 ? serverContent.ring : serverContent.otherSkills == null ? null :
                Array.Find(serverContent.otherSkills, value => value != null && value.skillID == result.skillId);
            if (skill == null || skill.skillID != result.skillId || skill.phases == null || result.skillPhase >= skill.phases.Length) return;
            var phase = skill.phases[result.skillPhase];
            if (phase == null || phase.action != C14SkillAction.Charging && phase.action != C14SkillAction.Hit) return;
            bool released = phase.action == C14SkillAction.Hit;
            if (result.casterType == 0 && players.TryGetValue(result.casterId, out var player) && player.State.hp > 0)
                player.ActorMotion.PlayAttack(result.dir, released);
            else if (result.casterType == 1 && result.casterId <= ushort.MaxValue &&
                monsters.TryGetValue((ushort)result.casterId, out var monster) && monster.State.hp > 0)
                monster.ActorMotion.PlayAttack(result.dir, released);
        }

        private void ApplyMonsterActionResults(ushort count, ActionResultField[] results)
        {
            if (results == null || count > results.Length) return;
            for (int i = 0; i < count; i++)
            {
                var result = results[i];
                PresentActionMotion(result);
                if (result.casterType != 1 || result.casterId > ushort.MaxValue || result.skillSlot != 0 ||
                    result.dir > 3 || result.skillPhase > 9 || float.IsNaN(result.x) || float.IsInfinity(result.x) ||
                    float.IsNaN(result.y) || float.IsInfinity(result.y) ||
                    !monsters.TryGetValue((ushort)result.casterId, out var entry) || entry.State.hp == 0)
                    continue;

                if (entry.State.monsterId == 2)
                {
                    if (result.skillId == 4)
                    {
                        if (serverContent == null || serverContent.otherSkills == null) continue;
                        C14ServerContent.RingSkill bossSquareSkill = Array.Find(serverContent.otherSkills,
                            candidate => candidate != null && candidate.skillID == 4);
                        if (bossSquareSkill == null || bossSquareSkill.skillID != 4 || bossSquareSkill.phases == null || bossSquareSkill.phases.Length != 10)
                            continue;
                        bool supportedPhases = true;
                        for (int phaseIndex = 0; phaseIndex < bossSquareSkill.phases.Length; phaseIndex++)
                        {
                            var phase = bossSquareSkill.phases[phaseIndex];
                            if (phase == null || phase.action != (phaseIndex % 2 == 0 ? C14SkillAction.Charging : C14SkillAction.Hit))
                            {
                                supportedPhases = false;
                                break;
                            }
                        }
                        if (!supportedPhases) continue;

                        int pairIndex = result.skillPhase / 2;
                        if ((result.skillPhase & 1) != 0)
                        {
                            if (entry.BossSquareOuterPreviews[pairIndex] != null)
                                entry.BossSquareOuterPreviews[pairIndex].gameObject.SetActive(false);
                            if (entry.BossSquareInnerPreviews[pairIndex] != null)
                                entry.BossSquareInnerPreviews[pairIndex].gameObject.SetActive(false);
                            continue;
                        }
                        if (monsterRangeMaterial == null) continue;

                        var bossSquareHit = bossSquareSkill.phases[result.skillPhase + 1];
                        if (bossSquareHit.range != C14SkillRange.Boss2 || float.IsNaN(bossSquareHit.range1) ||
                            float.IsInfinity(bossSquareHit.range1) || float.IsNaN(bossSquareHit.range2) ||
                            float.IsInfinity(bossSquareHit.range2) || bossSquareHit.range1 <= 0 ||
                            bossSquareHit.range2 < 0 || bossSquareHit.range2 >= bossSquareHit.range1 || bossSquareHit.waitTicks == 0)
                            continue;

                        LineRenderer outer = entry.BossSquareOuterPreviews[pairIndex];
                        if (outer == null)
                        {
                            var visual = new GameObject("Boss Square Outer Preview " + pairIndex);
                            visual.transform.SetParent(entry.Visual.transform, false);
                            outer = visual.AddComponent<LineRenderer>();
                            outer.sharedMaterial = monsterRangeMaterial;
                            outer.useWorldSpace = true;
                            outer.loop = true;
                            outer.positionCount = 4;
                            outer.widthMultiplier = 0.1f;
                            outer.startColor = outer.endColor = Color.red;
                            entry.BossSquareOuterPreviews[pairIndex] = outer;
                        }
                        SetSquarePreview(outer, result.x, result.y, bossSquareHit.range1);
                        outer.gameObject.SetActive(true);

                        if (bossSquareHit.range2 > 0)
                        {
                            LineRenderer inner = entry.BossSquareInnerPreviews[pairIndex];
                            if (inner == null)
                            {
                                var visual = new GameObject("Boss Square Inner Preview " + pairIndex);
                                visual.transform.SetParent(entry.Visual.transform, false);
                                inner = visual.AddComponent<LineRenderer>();
                                inner.sharedMaterial = monsterRangeMaterial;
                                inner.useWorldSpace = true;
                                inner.loop = true;
                                inner.positionCount = 4;
                                inner.widthMultiplier = 0.1f;
                                inner.startColor = inner.endColor = Color.red;
                                entry.BossSquareInnerPreviews[pairIndex] = inner;
                            }
                            SetSquarePreview(inner, result.x, result.y, bossSquareHit.range2);
                            inner.gameObject.SetActive(true);
                        }
                        else if (entry.BossSquareInnerPreviews[pairIndex] != null)
                        {
                            entry.BossSquareInnerPreviews[pairIndex].gameObject.SetActive(false);
                        }

                        entry.BossSquareUntil[pairIndex] = Time.realtimeSinceStartupAsDouble + 0.05 * bossSquareHit.waitTicks + 0.1;
                        continue;
                    }

                    if (result.skillId != 3) continue;
                    if (result.skillPhase == 1 || result.skillPhase == 3)
                    {
                        LineRenderer matchingPreview = result.skillPhase == 1 ? entry.BossFanPreviewFirst : entry.BossFanPreviewSecond;
                        if (matchingPreview != null) matchingPreview.gameObject.SetActive(false);
                        continue;
                    }
                    if (result.skillPhase != 0 && result.skillPhase != 2) continue;
                    if (serverContent == null || monsterRangeMaterial == null || serverContent.otherSkills == null) continue;
                    C14ServerContent.RingSkill bossSkill = Array.Find(serverContent.otherSkills,
                        candidate => candidate != null && candidate.skillID == 3);
                    if (bossSkill == null || bossSkill.skillID != 3 || bossSkill.phases == null || bossSkill.phases.Length != 4 ||
                        bossSkill.phases[0] == null || bossSkill.phases[0].action != C14SkillAction.Charging ||
                        bossSkill.phases[1] == null || bossSkill.phases[1].action != C14SkillAction.Hit ||
                        bossSkill.phases[2] == null || bossSkill.phases[2].action != C14SkillAction.Charging ||
                        bossSkill.phases[3] == null || bossSkill.phases[3].action != C14SkillAction.Hit)
                        continue;

                    int hitIndex = result.skillPhase + 1;
                    var bossHit = bossSkill.phases[hitIndex];
                    C14SkillRange expectedRange = result.skillPhase == 0 ? C14SkillRange.Boss1_1 : C14SkillRange.Boss1_2;
                    if (bossHit.range != expectedRange || float.IsNaN(bossHit.range1) || float.IsInfinity(bossHit.range1) ||
                        bossHit.range1 <= 0 || bossHit.waitTicks == 0) continue;

                    bool firstFan = result.skillPhase == 0;
                    LineRenderer preview = firstFan ? entry.BossFanPreviewFirst : entry.BossFanPreviewSecond;
                    if (preview == null)
                    {
                        var visual = new GameObject(firstFan ? "Boss Fan Preview First" : "Boss Fan Preview Second");
                        visual.transform.SetParent(entry.Visual.transform, false);
                        preview = visual.AddComponent<LineRenderer>();
                        preview.sharedMaterial = monsterRangeMaterial;
                        preview.useWorldSpace = true;
                        preview.loop = true;
                        preview.widthMultiplier = 0.1f;
                        preview.startColor = preview.endColor = Color.red;
                        if (firstFan) entry.BossFanPreviewFirst = preview;
                        else entry.BossFanPreviewSecond = preview;
                    }

                    preview.positionCount = 36;
                    preview.SetPosition(0, new Vector3(result.x, 0.055f, result.y));
                    float sweepStep = result.skillPhase == 0 ? 7.5f : 3.75f;
                    for (int point = 0; point <= 16; point++)
                    {
                        float firstAngle = result.skillPhase == 0 ? 30f + point * sweepStep : 330f + point * sweepStep;
                        preview.SetPosition(point + 1, new Vector3(
                            result.x + Mathf.Cos(firstAngle * Mathf.Deg2Rad) * bossHit.range1, 0.055f,
                            result.y + Mathf.Sin(firstAngle * Mathf.Deg2Rad) * bossHit.range1));
                    }
                    preview.SetPosition(18, new Vector3(result.x, 0.055f, result.y));
                    for (int point = 0; point <= 16; point++)
                    {
                        float secondAngle = result.skillPhase == 0 ? 210f + point * sweepStep : 150f + point * sweepStep;
                        preview.SetPosition(point + 19, new Vector3(
                            result.x + Mathf.Cos(secondAngle * Mathf.Deg2Rad) * bossHit.range1, 0.055f,
                            result.y + Mathf.Sin(secondAngle * Mathf.Deg2Rad) * bossHit.range1));
                    }
                    preview.gameObject.SetActive(true);
                    double deadline = Time.realtimeSinceStartupAsDouble + 0.05 * bossHit.waitTicks + 0.1;
                    if (firstFan) entry.BossFanUntilFirst = deadline;
                    else entry.BossFanUntilSecond = deadline;
                    continue;
                }

                if (entry.State.monsterId > 1 || result.skillPhase > 1 ||
                    entry.State.monsterId == 0 && result.skillId != 0 || entry.State.monsterId == 1 && result.skillId > 1)
                    continue;
                if (result.skillPhase == 1)
                {
                    if (entry.RangePreview != null) entry.RangePreview.gameObject.SetActive(false);
                    continue;
                }
                if (serverContent == null || monsterRangeMaterial == null) continue;

                C14ServerContent.RingSkill skill = result.skillId == 1 ? serverContent.ring :
                    serverContent.otherSkills == null ? null : Array.Find(serverContent.otherSkills,
                        candidate => candidate != null && candidate.skillID == result.skillId);
                if (skill == null || skill.skillID != result.skillId || skill.phases == null || skill.phases.Length != 2 ||
                    skill.phases[0] == null || skill.phases[0].action != C14SkillAction.Charging || skill.phases[1] == null)
                    continue;
                var hit = skill.phases[1];
                if (hit.action != C14SkillAction.Hit || hit.range != C14SkillRange.Circle || float.IsNaN(hit.range1) || float.IsInfinity(hit.range1) ||
                    hit.range1 <= 0 || hit.waitTicks == 0) continue;

                if (entry.RangePreview == null)
                {
                    var preview = new GameObject("Monster Range Preview");
                    preview.transform.SetParent(entry.Visual.transform, false);
                    entry.RangePreview = preview.AddComponent<LineRenderer>();
                    entry.RangePreview.sharedMaterial = monsterRangeMaterial;
                    entry.RangePreview.useWorldSpace = true;
                    entry.RangePreview.loop = true;
                    entry.RangePreview.positionCount = 32;
                    entry.RangePreview.widthMultiplier = 0.1f;
                    entry.RangePreview.startColor = entry.RangePreview.endColor = Color.red;
                }

                for (int point = 0; point < entry.RangePreview.positionCount; point++)
                {
                    float angle = Mathf.PI * 2f * point / entry.RangePreview.positionCount;
                    entry.RangePreview.SetPosition(point, new Vector3(
                        result.x + Mathf.Cos(angle) * hit.range1, 0.055f,
                        result.y + Mathf.Sin(angle) * hit.range1));
                }
                entry.RangePreview.gameObject.SetActive(true);
                entry.RangeUntil = Time.realtimeSinceStartupAsDouble + 0.05 * hit.waitTicks + 0.1;
            }
        }

        private void Expire(double now)
        {
            // This is a local observation lifetime, not a server despawn decision. Deltas never extend it.
            var stalePlayers = new List<uint>();
            foreach (var pair in players)
                if (now - pair.Value.LastFullSeen >= 3) stalePlayers.Add(pair.Key);
            foreach (uint id in stalePlayers) RemovePlayer(id);
            var staleMonsters = new List<ushort>();
            foreach (var pair in monsters)
                if (now - pair.Value.LastFullSeen >= 3) staleMonsters.Add(pair.Key);
            foreach (ushort id in staleMonsters) RemoveMonster(id);
        }

        private static bool ValidPlayer(FullStateField state) => ValidState(state.x, state.y, state.dir, state.hp, state.maxHp);
        private static bool ValidMonster(MonsterFullField state) => state.monsterId <= 2 && ValidState(state.x, state.y, state.dir, state.hp, state.maxHp);
        private static bool ValidState(float x, float y, byte direction, int hp, int maxHp) =>
            !float.IsNaN(x) && !float.IsInfinity(x) && !float.IsNaN(y) && !float.IsInfinity(y) && direction <= 3 && maxHp > 0 && hp >= 0 && hp <= maxHp;
        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
        private static float FloatBits(uint raw) => BitConverter.ToSingle(BitConverter.GetBytes(raw), 0);
        private static void Face(GameObject visual, byte direction) =>
            visual.transform.rotation = C02WorldScreen.ServerFacing(direction);

        private void RenderObservations(double now)
        {
            double presentationTime = now - PresentationDelay;
            foreach (var entry in players.Values)
                if (entry.Visual != null) entry.Visual.transform.position = entry.Motion.PositionAt(presentationTime);
            foreach (var entry in monsters.Values)
            {
                if (entry.Visual != null) entry.Visual.transform.position = entry.Motion.PositionAt(presentationTime);
                if (entry.RangePreview != null && now >= entry.RangeUntil) entry.RangePreview.gameObject.SetActive(false);
                if (entry.BossFanPreviewFirst != null && now >= entry.BossFanUntilFirst) entry.BossFanPreviewFirst.gameObject.SetActive(false);
                if (entry.BossFanPreviewSecond != null && now >= entry.BossFanUntilSecond) entry.BossFanPreviewSecond.gameObject.SetActive(false);
                for (int pairIndex = 0; pairIndex < entry.BossSquareOuterPreviews.Length; pairIndex++)
                {
                    if (now < entry.BossSquareUntil[pairIndex]) continue;
                    if (entry.BossSquareOuterPreviews[pairIndex] != null)
                        entry.BossSquareOuterPreviews[pairIndex].gameObject.SetActive(false);
                    if (entry.BossSquareInnerPreviews[pairIndex] != null)
                        entry.BossSquareInnerPreviews[pairIndex].gameObject.SetActive(false);
                }
            }
        }

        private static void SetSquarePreview(LineRenderer preview, float x, float y, float sideLength)
        {
            float halfLength = sideLength * 0.5f;
            preview.SetPosition(0, new Vector3(x - halfLength, 0.055f, y - halfLength));
            preview.SetPosition(1, new Vector3(x - halfLength, 0.055f, y + halfLength));
            preview.SetPosition(2, new Vector3(x + halfLength, 0.055f, y + halfLength));
            preview.SetPosition(3, new Vector3(x + halfLength, 0.055f, y - halfLength));
        }

        private void RemovePlayer(uint id)
        {
            if (!players.TryGetValue(id, out var entry)) return;
            if (entry.Visual != null) { entry.Visual.SetActive(false); Destroy(entry.Visual); }
            players.Remove(id);
        }

        private void RemoveMonster(ushort id)
        {
            if (!monsters.TryGetValue(id, out var entry)) return;
            if (entry.Visual != null) { entry.Visual.SetActive(false); Destroy(entry.Visual); }
            monsters.Remove(id);
        }

        private void ClearObservations()
        {
            foreach (var entry in players.Values)
                if (entry.Visual != null) { entry.Visual.SetActive(false); Destroy(entry.Visual); }
            foreach (var entry in monsters.Values)
                if (entry.Visual != null) { entry.Visual.SetActive(false); Destroy(entry.Visual); }
            players.Clear();
            monsters.Clear();
        }

        private void OnDisable()
        {
            if (loginScreen != null) loginScreen.ClientChanged -= ChangeClient;
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            Unsubscribe();
            generation++;
            model = null;
            observedZone = 0;
            ownId = 0;
            ClearObservations();
        }
    }
}
