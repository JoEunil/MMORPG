
using ClientCore.PacketHelper;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;
public class MonsterManager : MonoBehaviour
{
    private Dictionary<ushort, (MonsterState state, MonsterControl control) > _monsters
    = new Dictionary<ushort, (MonsterState state, MonsterControl control)>();

    private HashSet<ushort> _deleteSet = new HashSet<ushort>();
    private HashSet<ushort> _dirtySet = new HashSet<ushort>();
    [SerializeField] private GameObject[] monsterPrefabs;
    public void SpawnMonster(ushort internalID, MonsterFullField field)
    {
        if (_monsters.ContainsKey(internalID)) return;
        GameObject go = Instantiate(monsterPrefabs[field.monsterId], new Vector3(field.x, field.y, transform.position.y), Quaternion.identity);
        MonsterControl control = go.GetComponent<MonsterControl>();
        MonsterState state = go.GetComponentInChildren<MonsterState>();
        state.SetMonster(field);
        control.Teleport(field.x, field.y);
        _monsters[internalID] = (state, control);
    }
    public void ZoneChangeMonster()
    {
        foreach (var key in _monsters.Keys.ToList())
        {
            RemoveMonster(key);
        }
    }

    public void UpdateAllMonsters(ushort count, MonsterFullField[] fields)
    {
        foreach (var monster in _monsters)
        {
            _deleteSet.Add(monster.Key);
        }
        for (int i = 0; i < count && i < fields.Length; i++)
        {
            var internalID = fields[i].internalId;
            if (_monsters.TryGetValue(internalID, out var monster))
            {
                monster.state.SetMonster(fields[i]);
                _deleteSet.Remove(internalID);
                _dirtySet.Add(internalID);
            }
            else
            {
                SpawnMonster(internalID, fields[i]);
            }
        }
        ApplyDirtyUpdatesMonster();
    }
    public void ApplyDeltaMonster(ushort count, MonsterDeltaField[] fields)
    {
        for (int i = 0; i < count && i < fields.Length; i++)
        {
            if (_monsters.TryGetValue(fields[i].internalId, out var monster))
            {
                monster.state.UpdateDelta(fields[i]);
                _dirtySet.Add(fields[i].internalId);
            }
        }
        ApplyDirtyUpdatesMonster();
    }
    private void ApplyDirtyUpdatesMonster()
    {
        foreach (var internalID in _dirtySet.ToList())
        {
            if (_monsters.TryGetValue(internalID, out var monster))
            {
                monster.control.UpdatePosition(monster.state.dir, monster.state.x, monster.state.y);
                monster.control.UpdateHP(monster.state.HP, monster.state.MAXHP);
                monster.control.ShowDamage(monster.state.damage);
                monster.state.damage = 0;
                if (monster.state.HP == 0)
                    KillMonster(internalID);
            }
        }

        _dirtySet.Clear();

        foreach (var id in _deleteSet)
        {
            RemoveMonster(id);
        }

        _deleteSet.Clear();
    }
    public void KillMonster(ushort internalID)
    {
        if (_monsters.TryGetValue(internalID, out var monster))
        {
            monster.control.DestroyMonster();
            _monsters.Remove(internalID);
            _dirtySet.Remove(internalID);
        }
    }
    public void RemoveMonster(ushort internalID)
    {
        if (_monsters.TryGetValue(internalID, out var monster))
        {
            Destroy(monster.control.gameObject);
            Destroy(monster.state.gameObject);
            _monsters.Remove(internalID);
            _dirtySet.Remove(internalID);
        }
    }
}
