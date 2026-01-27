using UnityEngine;
using TMPro;

class TestUI : MonoBehaviour
{
    [SerializeField] private TextMeshProUGUI zoneID;
    [SerializeField] private TextMeshProUGUI UserCnt;
    [SerializeField] private TextMeshProUGUI position;
    [SerializeField] private TextMeshProUGUI ping;

    private void Awake()
    {
        zoneID.text = "Zone: 0";
        UserCnt.text = "User count: 0";
        position.text = "x: 0, y: 0";
    }
    public void SetZoneID(ushort id)
    {
        zoneID.text = "Zone: " + id;
    }
    public void SetUserCnt(int cnt)
    {
        UserCnt.text = "User count: " + cnt;
    }
    public void SetPosition(float x, float y)
    {
        position.text = "x: " + x + ", Y: " + y;
    }
    public void SetPing(ulong rtt)
    {
        ping.text = "Ping(rtt): " + (int)(rtt) + " ms";
    }
}
