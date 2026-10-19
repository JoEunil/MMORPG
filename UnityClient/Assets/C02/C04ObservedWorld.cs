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
        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private C02WorldScreen worldScreen;
        [SerializeField] private Transform displayRoot;
        [SerializeField] private GameObject playerPrefab;
        [SerializeField] private GameObject[] monsterPrefabs;

        private sealed class PlayerObservation
        {
            public FullStateField State;
            public GameObject Visual;
            public double LastFullSeen;
        }

        private sealed class MonsterObservation
        {
            public MonsterFullField State;
            public GameObject Visual;
            public C13EntityVitals Vitals;
            public double LastFullSeen;
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
            worldScreen.Stage == C02Stage.InWorld && worldScreen.HasOwnFull && displayRoot != null &&
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
            model.OnFullReceived += playerFull;
            model.OnDeltaReceived += playerDelta;
            model.OnMonsterFullReceived += monsterFull;
            model.OnMonsterDeltaReceived += monsterDelta;
        }

        private void Unsubscribe()
        {
            if (model == null) return;
            model.OnFullReceived -= playerFull;
            model.OnDeltaReceived -= playerDelta;
            model.OnMonsterFullReceived -= monsterFull;
            model.OnMonsterDeltaReceived -= monsterDelta;
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
            Expire(Time.realtimeSinceStartupAsDouble);
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
                    players.Add(state.zoneInternalID, entry);
                }
                entry.State = state;
                entry.LastFullSeen = now;
                Place(entry.Visual, state.x, state.y, state.dir);
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
                    entry.Vitals = entry.Visual.AddComponent<C13EntityVitals>();
                    entry.Vitals.SetCamera(worldScreen.ViewCamera);
                    monsters.Add(state.internalId, entry);
                }
                entry.State = state;
                entry.LastFullSeen = now;
                entry.Vitals.SetHealth(state.hp, state.maxHp);
                Place(entry.Visual, state.x, state.y, state.dir);
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
                entry.State = pair.Value.state;
                Place(entry.Visual, entry.State.x, entry.State.y, entry.State.dir);
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
                entry.State = pair.Value.state;
                entry.Vitals.SetHealth(entry.State.hp, entry.State.maxHp);
                Place(entry.Visual, entry.State.x, entry.State.y, entry.State.dir);
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
        private static float FloatBits(uint raw) => BitConverter.ToSingle(BitConverter.GetBytes(raw), 0);
        private static void Place(GameObject visual, float x, float y, byte direction) =>
            visual.transform.SetPositionAndRotation(new Vector3(x, 0, y), C02WorldScreen.ServerFacing(direction));

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
