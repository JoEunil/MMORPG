using ClientCore;
using ClientCore.PacketHelper;
using NUnit.Framework.Internal;
using TMPro;
using UnityEngine;
using UnityEngine.UI;
using static UnityEngine.PlayerLoop.PreUpdate;

public class GameView : MonoBehaviour
{
    private IViewModelUI _viewModel;
    private IViewDataUI _viewData;
    [SerializeField] private PlayerManager playerManager;
    [SerializeField] private MonsterManager monsterManager;
    [SerializeField] private ActionManager actionManager;
    [SerializeField] private TestUI testUI;
    [SerializeField] private ZoneChageTrigger trigger;

    bool _zoneChangeReceived = false;
    bool _fulllStateRecived = false;
    private void Awake()
    {
        _viewModel = CoreManager.Instance.VM;
        _viewData = CoreManager.Instance.VD;

        testUI.SetPing(0);
        _viewModel.OnZoneChageReceived += ZoneChange;
        _viewModel.OnDeltaReceived += DeltaUpdate;
        _viewModel.OnFullReceived += FullState;
        _viewModel.OnMonsterDeltaReceived += MonsterDelta;
        _viewModel.OnMonsterFullReceived += MonsterFull;
        _viewModel.OnActionResultReceived += ActionResult;
    }
    void ZoneChange(ushort zoneID, ulong chatID, ulong InternalId, float x, float y)
    {
        testUI.SetZoneID(zoneID);
        playerManager.ZoneChange(InternalId, x, y);
        monsterManager.ZoneChangeMonster();
        _fulllStateRecived = false;
        if (_zoneChangeReceived == false)
        {
            playerManager.SetChatID(chatID);
        }
        _zoneChangeReceived = true;

        trigger.ResetTrigger((int)zoneID);
    }

    void DeltaUpdate(ushort count, DeltaUpdateField[] data)
    {
        if (_fulllStateRecived)
            playerManager.ApplyDelta(count, data);
    }
    void FullState(ushort count, FullStateField[] data)
    {
        if (_zoneChangeReceived)
        {
            testUI.SetUserCnt(count);
            playerManager.UpdateAllPlayers(count, data);
            _fulllStateRecived = true;
        }
    }
    void MonsterDelta(ushort count, MonsterDeltaField[] data)
    {
        if (_fulllStateRecived)
            monsterManager.ApplyDeltaMonster(count, data);

    }
    void MonsterFull(ushort count, MonsterFullField[] data)
    {
        if (_zoneChangeReceived)
        {
            testUI.SetMonsterCnt(count);
            monsterManager.UpdateAllMonsters(count, data);
        }
    }
    void ActionResult(ushort count, ActionResultField[] data)
    {
        if (_fulllStateRecived)
        {
            actionManager.ProcessAction(count, data);
        }
    }
    void PingUpdate(ulong rtt)
    {
        testUI.SetPing(rtt);
    }
}
