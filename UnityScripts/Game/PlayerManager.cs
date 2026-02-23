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

    [SerializeField] private PlayerStatsUI playerStatsUI;
    [SerializeField] private SkillUI skillUI;

    private ulong _chatID = 0;

    bool _myPlayerInitialized = false;

    public void SetChatID(ulong chatID)
    {
        _chatID = chatID;
    }
    public ulong GetChatID()
    {
        return _chatID;
    }
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
    public void SpawnPlayer(ulong zoneInternalID, FullStateField field)
    {
        if (_players.ContainsKey(zoneInternalID)) return;
        GameObject go = Instantiate(playerPrefab, new Vector3(field.x, field.y, transform.position.y), Quaternion.identity);
        PlayerState state = go.GetComponent<PlayerState>();
        HeroControl control = go.GetComponent<HeroControl>();
        state.SetPlayer(field);
        control.SetNameLevel(Encoding.UTF8.GetString(field.charName), field.level);
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
                SpawnPlayer(internalID, fields[i]);
            }
        }
        ApplyDirtyUpdates();
        playerStatsUI.UpdateAll(_myPlayerState);
    }
    public void ApplyDelta(ushort count, DeltaUpdateField[] fields)
    {
        for (int i = 0; i < count && i < fields.Length; i++)
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
        playerStatsUI.UpdateAll(_myPlayerState);
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
            control.UpdateHPMP(state.HP, state.MP, state.MAXHP, state.MAXMP);
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
            Destroy(player.state.gameObject);
            _players.Remove(zoneInternalID);
            _dirtySet.Remove(zoneInternalID);
        }
    }
    public void SkillAnimation(ulong casterID, byte slotID, uint skillId, byte dir)
    {
        if (casterID == _myPlayerState.zoneInternalID)
        {
            _myPlayerControl.StartSkill(dir, skillId);
            skillUI.UseSkill(slotID);
        }
        else if (_players.TryGetValue(casterID, out var player))
        {
            player.control.StartSkill(dir, skillId);
        }
    }
}
