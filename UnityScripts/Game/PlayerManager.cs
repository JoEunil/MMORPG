using ClientCore;
using ClientCore.PacketHelper;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml.Serialization;
using UnityEngine;
public class PlayerManager : MonoBehaviour
{
    private Dictionary<ulong, (PlayerState state, HeroControl control)> _players
    = new Dictionary<ulong, (PlayerState, HeroControl)>();

    private HashSet<ulong> _deleteSet = new HashSet<ulong>();
    private HashSet<ulong> _dirtySet = new HashSet<ulong>();
    [SerializeField] private GameObject playerPrefab;
    [SerializeField] private PlayerState _myPlayerState;
    [SerializeField] private MyHeroControl _myPlayerControl;

    bool _myPlayerInitialized = false;
    public ulong GetMyPlayerID()
    {
        return _myPlayerState.zoneInternalID;
    }
    public string GetPlayerName(ulong id)
    {
        if (_players.TryGetValue(id, out var player))
        {
            return _players[id].state.charName;
        }
        return null;
    }
    public void SpawnPlayer(ulong zoneInternalID, Vector3 pos, string name, ushort level)
    {
        if (_players.ContainsKey(zoneInternalID)) return;
        GameObject go = Instantiate(playerPrefab, pos, Quaternion.identity);
        PlayerState state = go.GetComponent<PlayerState>();
        HeroControl control = go.GetComponent<HeroControl>();
        control.SetNameLevel(name, level);
        _players[zoneInternalID] = (state, control);
        
    }
    public void ZoneChange(ulong id, float x, float y)
    {
        _myPlayerState.ZoneMove(id, x, y);
        foreach (var key in _players.Keys.ToList())
        {
            RemovePlayer(key);
        }
    }

    public void UpdateAllPlayers(ushort count, FullStateField[] fields)
    {
        foreach(var player in _players)
        {
            _deleteSet.Add(player.Key);
        }
        for (int i = 0; i < count && i < fields.Length; i++)
        {
            var internalID = fields[i].zoneInternalID;
            if (internalID == _myPlayerState.zoneInternalID)
            {
                if(!_myPlayerInitialized)
                {
                    _myPlayerControl.SetNameLevel(Encoding.UTF8.GetString(fields[i].charName), fields[i].level);
                    _myPlayerControl.Teleport(fields[i].dir, fields[i].x, fields[i].y);
                    _myPlayerInitialized = true;
                }
                _myPlayerState.SetPlayer(fields[i]);
                _dirtySet.Add(internalID);
            }
            else if (_players.TryGetValue(internalID, out var player))
            {
                player.state.SetPlayer(fields[i]);
                _deleteSet.Remove(internalID);
                _dirtySet.Add(internalID);
            } 
            else
            {
                SpawnPlayer(internalID, new Vector3(fields[i].x, fields[i].y, transform.position.z), Encoding.UTF8.GetString(fields[i].charName), fields[i].level);
            }
        }
        ApplyDirtyUpdates();
    }
    public void ApplyDelta(ushort count, DeltaUpdateField[] fields)
    {
        for (int i = 0; i < count || i < fields.Length; i++)
        {
            if (fields[i].zoneInternalID == _myPlayerState.zoneInternalID)
            {
                _myPlayerState.UpdateDelta(fields[i]);
                _dirtySet.Add(fields[i].zoneInternalID);
                continue;
            }
            if (_players.TryGetValue(fields[i].zoneInternalID, out var player))
            {
                player.state.UpdateDelta(fields[i]);
                _dirtySet.Add(fields[i].zoneInternalID);
            }
        }
        ApplyDirtyUpdates();
    }
    private void ApplyDirtyUpdates()
    {
        foreach (var zoneID in _dirtySet)
        {
            if (zoneID == _myPlayerState.zoneInternalID)
            {
                _myPlayerControl.UpdatePosition(_myPlayerState.dir, _myPlayerState.x, _myPlayerState.y);
                _myPlayerControl.UpdateHPMP(_myPlayerState.HP, _myPlayerState.MP);
                continue;
            }

            var (state, control) = _players[zoneID];
            control.UpdatePosition(state.dir, state.x, state.y);
            control.UpdateHPMP(state.HP, state.MP);
        }

        _dirtySet.Clear();

        foreach (var id in _deleteSet)
        {
            RemovePlayer(id);
        }

        _deleteSet.Clear();
    }
    public void RemovePlayer(ulong zoneInternalID)
    {
        if (_players.TryGetValue(zoneInternalID, out var player))
        {
            Destroy(player.control.gameObject);
            _players.Remove(zoneInternalID);
            _dirtySet.Remove(zoneInternalID);
        }
    }

}
