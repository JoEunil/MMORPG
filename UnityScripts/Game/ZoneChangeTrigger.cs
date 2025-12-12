using ClientCore;
using System.Collections.Generic;
using UnityEngine;
class ZoneChageTrigger : MonoBehaviour
{
    private IViewModelUI _viewModel;
    private class ZoneBoundary
    {
        public float xMin, xMax, yMin, yMax;
        public ZoneBoundary(float xMin, float xMax, float yMin, float yMax)
        {
            this.xMin = xMin;
            this.xMax = xMax;
            this.yMin = yMin;
            this.yMax = yMax;
        }

        public int CheckPos(float x, float y)
        {

            if (x < xMin) return 3; 
            if (x > xMax) return 4; 
            if (y < yMin) return 2;
            if (y > yMax) return 1;
            return 0;
        }
    }

    private Dictionary<int, ZoneBoundary> _zoneMap = new Dictionary<int, ZoneBoundary>();
    private int _zoneID = 1;
    private bool _trigger = false;
    private void Awake()
    {
        _viewModel = CoreManager.Instance.VM;
        _viewModel.OnZoneChageFailed += ResetTrigger;
        float ZONE_SIZE = 100f;
        float TRIGGER_BOUNDARY = 3f;
        int ZONE_HORIZON = 3;

        int totalZones = 9; 
        for (int zone = 1; zone <= totalZones; zone++)
        {
            int x = (zone - 1) % ZONE_HORIZON;
            int y = (zone - 1) / ZONE_HORIZON;

            float xMin = x * ZONE_SIZE - TRIGGER_BOUNDARY;
            float xMax = (x + 1) * ZONE_SIZE + TRIGGER_BOUNDARY;
            float yMin = y * ZONE_SIZE - TRIGGER_BOUNDARY;
            float yMax = (y + 1) * ZONE_SIZE + TRIGGER_BOUNDARY;

            _zoneMap[zone] = new ZoneBoundary(xMin, xMax, yMin, yMax);

            Debug.Log($"Zone {zone} trigger boundary: x[{xMin},{xMax}] y[{yMin},{yMax}]");
        }
    }


    public void CheckZoneChange(float x, float y)
    {
        if (!_trigger || _zoneID == -1)
            return ;
        var res = _zoneMap[_zoneID].CheckPos(x, y);
        if (res == 0)
            return;
        _viewModel.ZoneChange(res);
        _trigger = false;
        Debug.Log($"Zone chagne op {res}");
    }

    public void ResetTrigger(int zoneID)
    {
        _trigger = true;
        _zoneID = zoneID;
    }

    public void ResetTrigger()
    {
        _trigger = true;
    }

}
