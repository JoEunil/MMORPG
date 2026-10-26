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
            public C13EntityVitals Vitals;
            public C19ActorMotion ActorMotion;
            public SkillTelegraph RangePreview;
            public double LastFullSeen;
            public MotionHistory Motion = new MotionHistory();
        }

        private sealed class SkillTelegraph
        {
            public MeshRenderer Renderer;
            public Mesh Mesh;
            public double StartedAt;
            public double Deadline;
            public float Duration;
            private Color[] colors;
            private Color tint;

            public void Show(Transform parent, Material material, string name, Vector3[] vertices, int[] triangles,
                double now, float duration, Color? tint = null)
            {
                this.tint = tint ?? new Color(1f, 0.18f, 0.045f, 1f);
                if (Renderer == null)
                {
                    var visual = new GameObject(name);
                    visual.transform.SetParent(parent, false);
                    var filter = visual.AddComponent<MeshFilter>();
                    Renderer = visual.AddComponent<MeshRenderer>();
                    Renderer.sharedMaterial = material;
                    Renderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
                    Renderer.receiveShadows = false;
                    Mesh = new Mesh { name = name + " Mesh" };
                    Mesh.MarkDynamic();
                    filter.sharedMesh = Mesh;
                }
                Mesh.Clear();
                Mesh.vertices = vertices;
                Mesh.triangles = triangles;
                colors = new Color[vertices.Length];
                Mesh.colors = colors;
                Mesh.RecalculateBounds();
                StartedAt = now;
                Duration = duration;
                Deadline = now + duration + 0.1;
                Renderer.gameObject.SetActive(true);
                Render(now);
            }

            public void Render(double now)
            {
                if (Renderer == null || !Renderer.gameObject.activeSelf) return;
                if (now >= Deadline)
                {
                    Renderer.gameObject.SetActive(false);
                    return;
                }
                float charge = Duration <= 0 ? 1 : Mathf.Clamp01((float)((now - StartedAt) / Duration));
                float alpha = Mathf.Lerp(0.14f, 0.58f, charge);
                for (int i = 0; i < colors.Length; i++) colors[i] = new Color(tint.r, tint.g, tint.b, alpha);
                Mesh.colors = colors;
            }

            public void Clear()
            {
                if (Renderer != null) Renderer.gameObject.SetActive(false);
            }

            public void Dispose()
            {
                if (Renderer != null)
                {
                    Renderer.gameObject.SetActive(false);
                    UnityEngine.Object.Destroy(Renderer.gameObject);
                }
                if (Mesh != null) UnityEngine.Object.Destroy(Mesh);
                Renderer = null;
                Mesh = null;
                colors = null;
            }
        }

        private sealed class MonsterObservation
        {
            public MonsterFullField State;
            public GameObject Visual;
            public C13EntityVitals Vitals;
            public C19ActorMotion ActorMotion;
            public SkillTelegraph RangePreview;
            public SkillTelegraph BossFanPreviewFirst;
            public SkillTelegraph BossFanPreviewSecond;
            public readonly SkillTelegraph[] BossSquarePreviews = new SkillTelegraph[5];
            public double LastFullSeen;
            public MotionHistory Motion = new MotionHistory();

            public void DisposeTelegraphs()
            {
                RangePreview?.Dispose();
                BossFanPreviewFirst?.Dispose();
                BossFanPreviewSecond?.Dispose();
                foreach (var preview in BossSquarePreviews) preview?.Dispose();
            }
        }

        private readonly Dictionary<uint, PlayerObservation> players = new Dictionary<uint, PlayerObservation>();
        private readonly Dictionary<ushort, MonsterObservation> monsters = new Dictionary<ushort, MonsterObservation>();
        private readonly List<(ushort Id, GameObject Visual)> defeatedVisuals = new List<(ushort, GameObject)>();
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

        // Copy authoritative positions without exposing mutable observation entries or visual interpolation.
        public bool CopyMinimapPositions(List<Vector2> playerPositions, List<Vector2> monsterPositions)
        {
            playerPositions.Clear();
            monsterPositions.Clear();
            if (!isActiveAndEnabled || model == null || !WorldAvailable || worldScreen.Stage != C02Stage.InWorld ||
                observedZone == 0 || observedZone != worldScreen.ZoneID || ownId == 0 || ownId != worldScreen.InternalID)
                return false;
            foreach (var pair in players)
                if (pair.Key != ownId) playerPositions.Add(new Vector2(pair.Value.State.x, pair.Value.State.y));
            foreach (var pair in monsters)
                if (pair.Value.State.hp > 0) monsterPositions.Add(new Vector2(pair.Value.State.x, pair.Value.State.y));
            return true;
        }

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
            for (int i = defeatedVisuals.Count - 1; i >= 0; i--)
                if (defeatedVisuals[i].Visual == null) defeatedVisuals.RemoveAt(i);
            if (!WorldAvailable || model == null)
            {
                if (observedZone != 0 || players.Count != 0 || monsters.Count != 0 || defeatedVisuals.Count != 0) WorldChanged();
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
                    entry.Vitals = entry.Visual.AddComponent<C13EntityVitals>();
                    entry.Vitals.SetCamera(worldScreen.ViewCamera);
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
                    if (state.hp < entry.State.hp)
                    {
                        entry.Vitals.PulseDamage();
                        if (state.hp > 0) entry.ActorMotion.PlayHit();
                    }
                }
                entry.State = state;
                entry.LastFullSeen = now;
                entry.Vitals.SetHealth(state.hp, state.maxHp);
                entry.ActorMotion.SetDowned(state.hp == 0);
                if (state.hp == 0) entry.RangePreview?.Clear();
            }
        }

        private void ApplyMonsterFull(ushort count, MonsterFullField[] states, double now)
        {
            if (states == null || count > states.Length) return;
            for (int i = 0; i < count; i++)
            {
                var state = states[i];
                if (!ValidMonster(state)) continue;
                if (state.hp == 0) { RemoveMonster(state.internalId, true); continue; }
                ClearDefeatedVisual(state.internalId);
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
                entry.ActorMotion.SetDowned(pair.Value.state.hp == 0);
                if (pair.Value.state.hp == 0) entry.RangePreview?.Clear();
                if (pair.Value.state.hp < entry.State.hp)
                {
                    entry.Vitals.PulseDamage();
                    if (pair.Value.state.hp > 0) entry.ActorMotion.PlayHit();
                }
                entry.Motion.Add(Time.realtimeSinceStartupAsDouble, pair.Value.state.x, pair.Value.state.y);
                entry.State = pair.Value.state;
                entry.Vitals.SetHealth(entry.State.hp, entry.State.maxHp);
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
                if (pair.Value.deathSeen) { RemoveMonster(pair.Key, true); continue; }
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
            byte motionSlot = result.skillSlot;
            if (result.casterType == 1)
            {
                if (result.skillId == 0) motionSlot = 0;
                else
                {
                    var shapePhase = released ? phase : result.skillPhase + 1 < skill.phases.Length
                        ? skill.phases[result.skillPhase + 1] : phase;
                    if (shapePhase == null) return;
                    motionSlot = shapePhase.range == C14SkillRange.Circle || shapePhase.range == C14SkillRange.Boss1_1 ||
                        shapePhase.range == C14SkillRange.Boss1_2 ? (byte)1 :
                        shapePhase.range == C14SkillRange.Rectangle || shapePhase.range == C14SkillRange.Boss2 ? (byte)2 : (byte)0;
                }
            }
            float chargeDuration = !released && result.skillPhase + 1 < skill.phases.Length && skill.phases[result.skillPhase + 1] != null
                ? Mathf.Min(skill.phases[result.skillPhase + 1].waitTicks * .05f, 2f) : 0;
            if (result.casterType == 0 && players.TryGetValue(result.casterId, out var player) && player.State.hp > 0)
                player.ActorMotion.PlayAttack(result.dir, released, motionSlot, chargeDuration);
            else if (result.casterType == 1 && result.casterId <= ushort.MaxValue &&
                monsters.TryGetValue((ushort)result.casterId, out var monster) && monster.State.hp > 0)
                monster.ActorMotion.PlayAttack(result.dir, released, motionSlot, chargeDuration);
        }

        private void ApplyMonsterActionResults(ushort count, ActionResultField[] results)
        {
            if (results == null || count > results.Length) return;
            for (int i = 0; i < count; i++)
            {
                var result = results[i];
                PresentActionMotion(result);
                PresentPlayerSkill(result);
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
                            entry.BossSquarePreviews[pairIndex]?.Clear();
                            continue;
                        }
                        if (monsterRangeMaterial == null) continue;

                        var bossSquareHit = bossSquareSkill.phases[result.skillPhase + 1];
                        if (bossSquareHit.range != C14SkillRange.Boss2 || float.IsNaN(bossSquareHit.range1) ||
                            float.IsInfinity(bossSquareHit.range1) || float.IsNaN(bossSquareHit.range2) ||
                            float.IsInfinity(bossSquareHit.range2) || bossSquareHit.range1 <= 0 ||
                            bossSquareHit.range2 < 0 || bossSquareHit.range2 >= bossSquareHit.range1 || bossSquareHit.waitTicks == 0)
                            continue;

                        if (entry.BossSquarePreviews[pairIndex] == null) entry.BossSquarePreviews[pairIndex] = new SkillTelegraph();
                        BuildBossSquare(result.x, result.y, bossSquareHit.range1, bossSquareHit.range2, out var squareVertices, out var squareTriangles);
                        entry.BossSquarePreviews[pairIndex].Show(displayRoot, monsterRangeMaterial, "Boss Square Telegraph " + pairIndex,
                            squareVertices, squareTriangles, Time.realtimeSinceStartupAsDouble, bossSquareHit.waitTicks * .05f);
                        continue;
                    }

                    if (result.skillId != 3) continue;
                    if (result.skillPhase == 1 || result.skillPhase == 3)
                    {
                        SkillTelegraph matchingPreview = result.skillPhase == 1 ? entry.BossFanPreviewFirst : entry.BossFanPreviewSecond;
                        if (matchingPreview != null) matchingPreview.Clear();
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
                    SkillTelegraph preview = firstFan ? entry.BossFanPreviewFirst : entry.BossFanPreviewSecond;
                    if (preview == null)
                    {
                        preview = new SkillTelegraph();
                        if (firstFan) entry.BossFanPreviewFirst = preview;
                        else entry.BossFanPreviewSecond = preview;
                    }
                    BuildBossFan(result.x, result.y, bossHit.range1, result.skillPhase == 0, out var fanVertices, out var fanTriangles);
                    preview.Show(displayRoot, monsterRangeMaterial, firstFan ? "Boss Fan Telegraph First" : "Boss Fan Telegraph Second",
                        fanVertices, fanTriangles, Time.realtimeSinceStartupAsDouble, bossHit.waitTicks * .05f);
                    continue;
                }

                if (entry.State.monsterId != 1 || result.skillId != 1 || result.skillPhase > 1)
                    continue;
                if (result.skillPhase == 1)
                {
                    entry.RangePreview?.Clear();
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

                if (entry.RangePreview == null) entry.RangePreview = new SkillTelegraph();
                BuildCircle(result.x, result.y, hit.range1, out var circleVertices, out var circleTriangles);
                entry.RangePreview.Show(displayRoot, monsterRangeMaterial, "Monster Circle Telegraph", circleVertices, circleTriangles,
                    Time.realtimeSinceStartupAsDouble, hit.waitTicks * .05f);
            }
        }

        private void PresentPlayerSkill(ActionResultField result)
        {
            if (result.casterType != 0 || result.casterId == ownId || result.skillSlot > 2 ||
                result.skillId != result.skillSlot || result.dir > 3 || !Finite(result.x) || !Finite(result.y) ||
                serverContent == null || monsterRangeMaterial == null ||
                !players.TryGetValue(result.casterId, out var entry) || entry.State.hp == 0) return;
            var skill = result.skillId == 1 ? serverContent.ring : serverContent.otherSkills == null ? null :
                Array.Find(serverContent.otherSkills, value => value != null && value.skillID == result.skillId);
            if (skill == null || skill.skillID != result.skillId || skill.phases == null || result.skillPhase >= skill.phases.Length) return;
            var phase = skill.phases[result.skillPhase];
            if (phase == null) return;
            bool charging = phase.action == C14SkillAction.Charging;
            var hit = charging && result.skillPhase + 1 < skill.phases.Length ? skill.phases[result.skillPhase + 1] : phase;
            if (hit == null || hit.action != C14SkillAction.Hit || !Finite(hit.range1) || hit.range1 <= 0 ||
                charging && hit.waitTicks == 0) return;
            Vector3[] vertices;
            int[] triangles;
            if (hit.range == C14SkillRange.Circle && hit.range2 == 0)
                BuildCircle(result.x, result.y, hit.range1, out vertices, out triangles);
            else if (hit.range == C14SkillRange.Rectangle && Finite(hit.range2) && hit.range2 > 0)
            {
                vertices = new Vector3[4];
                for (int point = 0; point < 4; point++)
                {
                    float side = (point == 0 || point == 3 ? -.5f : .5f) * hit.range2;
                    float reach = point < 2 ? 0 : hit.range1;
                    if (result.dir == 1 || result.dir == 2) reach = -reach;
                    vertices[point] = result.dir < 2 ? PreviewPoint(result.x + side, result.y + reach) :
                        PreviewPoint(result.x + reach, result.y + side);
                }
                triangles = new[] { 0, 1, 2, 0, 2, 3 };
            }
            else return;
            if (entry.RangePreview == null) entry.RangePreview = new SkillTelegraph();
            entry.RangePreview.Show(displayRoot, monsterRangeMaterial, "Player Skill Area " + result.casterId,
                vertices, triangles, Time.realtimeSinceStartupAsDouble, charging ? hit.waitTicks * .05f : .12f,
                charging ? new Color(.25f, .72f, 1f) : new Color(1f, .82f, .38f));
        }

        private Vector3 PreviewPoint(float x, float z) => displayRoot.InverseTransformPoint(new Vector3(x, 0.06f, z));

        private void BuildCircle(float x, float z, float radius, out Vector3[] vertices, out int[] triangles)
        {
            const int segments = 40;
            vertices = new Vector3[segments + 1]; triangles = new int[segments * 3];
            vertices[0] = PreviewPoint(x, z);
            for (int i = 0; i < segments; i++)
            {
                float angle = Mathf.PI * 2 * i / segments;
                vertices[i + 1] = PreviewPoint(x + Mathf.Cos(angle) * radius, z + Mathf.Sin(angle) * radius);
                int t = i * 3; triangles[t] = 0; triangles[t + 1] = i + 1; triangles[t + 2] = (i + 1) % segments + 1;
            }
        }

        private void BuildBossFan(float x, float z, float radius, bool first, out Vector3[] vertices, out int[] triangles)
        {
            const int segments = 16;
            vertices = new Vector3[(segments + 2) * 2]; triangles = new int[segments * 3 * 2];
            for (int sector = 0; sector < 2; sector++)
            {
                int vertexOffset = sector * (segments + 2), triangleOffset = sector * segments * 3;
                vertices[vertexOffset] = PreviewPoint(x, z);
                float start = first ? (sector == 0 ? 30 : 210) : (sector == 0 ? 330 : 150);
                float step = first ? 7.5f : 3.75f;
                for (int i = 0; i <= segments; i++)
                {
                    float angle = (start + i * step) * Mathf.Deg2Rad;
                    vertices[vertexOffset + i + 1] = PreviewPoint(x + Mathf.Cos(angle) * radius, z + Mathf.Sin(angle) * radius);
                    if (i < segments)
                    {
                        int t = triangleOffset + i * 3;
                        triangles[t] = vertexOffset; triangles[t + 1] = vertexOffset + i + 1; triangles[t + 2] = vertexOffset + i + 2;
                    }
                }
            }
        }

        private void BuildBossSquare(float x, float z, float outerSide, float innerSide, out Vector3[] vertices, out int[] triangles)
        {
            float outerHalf = outerSide * .5f;
            var outer = new[] { PreviewPoint(x - outerHalf, z - outerHalf), PreviewPoint(x - outerHalf, z + outerHalf),
                PreviewPoint(x + outerHalf, z + outerHalf), PreviewPoint(x + outerHalf, z - outerHalf) };
            if (innerSide <= 0)
            {
                vertices = outer;
                triangles = new[] { 0, 1, 2, 0, 2, 3 };
                return;
            }
            float innerHalf = innerSide * .5f;
            var inner = new[] { PreviewPoint(x - innerHalf, z - innerHalf), PreviewPoint(x - innerHalf, z + innerHalf),
                PreviewPoint(x + innerHalf, z + innerHalf), PreviewPoint(x + innerHalf, z - innerHalf) };
            vertices = new Vector3[16]; triangles = new int[24];
            for (int side = 0; side < 4; side++)
            {
                int next = (side + 1) % 4, v = side * 4, t = side * 6;
                vertices[v] = outer[side]; vertices[v + 1] = outer[next]; vertices[v + 2] = inner[next]; vertices[v + 3] = inner[side];
                triangles[t] = v; triangles[t + 1] = v + 1; triangles[t + 2] = v + 2;
                triangles[t + 3] = v; triangles[t + 4] = v + 2; triangles[t + 5] = v + 3;
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
            {
                if (entry.Visual != null) entry.Visual.transform.position = entry.Motion.PositionAt(presentationTime);
                entry.RangePreview?.Render(now);
            }
            foreach (var entry in monsters.Values)
            {
                if (entry.Visual != null) entry.Visual.transform.position = entry.Motion.PositionAt(presentationTime);
                entry.RangePreview?.Render(now);
                entry.BossFanPreviewFirst?.Render(now);
                entry.BossFanPreviewSecond?.Render(now);
                foreach (var preview in entry.BossSquarePreviews) preview?.Render(now);
            }
        }

        private void RemovePlayer(uint id)
        {
            if (!players.TryGetValue(id, out var entry)) return;
            entry.RangePreview?.Dispose();
            if (entry.Visual != null) { entry.Visual.SetActive(false); Destroy(entry.Visual); }
            players.Remove(id);
        }

        private void RemoveMonster(ushort id, bool defeated = false)
        {
            if (!monsters.TryGetValue(id, out var entry)) return;
            entry.DisposeTelegraphs();
            monsters.Remove(id);
            if (entry.Visual == null) return;
            if (defeated && entry.ActorMotion.PlayDefeat())
            {
                entry.Vitals.enabled = false;
                entry.Visual.transform.Find("MonsterVitals")?.gameObject.SetActive(false);
                foreach (var collider in entry.Visual.GetComponentsInChildren<Collider>()) collider.enabled = false;
                defeatedVisuals.Add((id, entry.Visual));
            }
            else { entry.Visual.SetActive(false); Destroy(entry.Visual); }
        }

        private void ClearDefeatedVisual(ushort id)
        {
            for (int i = defeatedVisuals.Count - 1; i >= 0; i--)
            {
                var defeated = defeatedVisuals[i];
                if (defeated.Id != id) continue;
                if (defeated.Visual != null) { defeated.Visual.SetActive(false); Destroy(defeated.Visual); }
                defeatedVisuals.RemoveAt(i);
            }
        }

        private void ClearObservations()
        {
            foreach (var entry in players.Values)
            {
                entry.RangePreview?.Dispose();
                if (entry.Visual != null) { entry.Visual.SetActive(false); Destroy(entry.Visual); }
            }
            foreach (var entry in monsters.Values)
            {
                entry.DisposeTelegraphs();
                if (entry.Visual != null) { entry.Visual.SetActive(false); Destroy(entry.Visual); }
            }
            players.Clear();
            monsters.Clear();
            foreach (var defeated in defeatedVisuals)
                if (defeated.Visual != null) { defeated.Visual.SetActive(false); Destroy(defeated.Visual); }
            defeatedVisuals.Clear();
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
