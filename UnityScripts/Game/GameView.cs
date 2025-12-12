using ClientCore;
using ClientCore.PacketHelper;
using NUnit.Framework.Internal;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class GameView : MonoBehaviour
{
    private IViewModelUI _viewModel;
    private IViewDataUI _viewData;
    [SerializeField] private PlayerManager playerManager;
    [SerializeField] private TestUI testUI;
    [SerializeField] private ZoneChageTrigger trigger;

    bool _zoneChangeReceived = false;
    bool _fulllStateRecived = false;
    private void Awake()
    {
        _viewModel = CoreManager.Instance.VM;
        _viewData = CoreManager.Instance.VD;

        _viewModel.OnZoneChageReceived += ZoneChange;
        _viewModel.OnDeltaReceived += DeltaUpdate;
        _viewModel.OnFullReceived += FullState;
    }
    void ZoneChange(ushort zoneID, ulong InternalId, float x, float y)
    {
        testUI.SetZoneID(zoneID);
        playerManager.ZoneChange(InternalId, x, y);
        _fulllStateRecived = false;
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
}
