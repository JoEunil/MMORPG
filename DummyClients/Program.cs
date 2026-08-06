using ClientCore;
using ClientCore.Network;
using ClientCore.Services;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

static class Program
{
    static private ClientSession?[] sessions = new ClientSession?[0];
    static private ViewModel _viewModel;
    static private NetworkService _network;
    static private Handler _handler;
    static private ClientTick _clientTick;
    static private int _ready = 0;
    static private int _count = 0;
    static private ulong tick = 0;
    static private byte dir = 0;

    static public TCPSocket? GetSocket(int index)
    {
        if (index < 0 || index >= sessions.Length)
            return null;
        return sessions[index]?.GetSocket();
    }
    static public void SetSessionPosition(int index, float x, float y)
    {
        if (index < 0 || index >= sessions.Length)
            return;
        sessions[index]?.SetPosition(x, y);
    }
    static public void Initialize(int count)
    {
        _count = count;
        _network = new NetworkService();
        _viewModel = new ViewModel(_network);
        _handler = new Handler();
        _clientTick = new ClientTick();
        _handler.Initialize(_viewModel);
        TCPSocket.Initialize(_handler);
        sessions = new ClientSession?[count];
        for (int i = 0; i < count; i++)
        {
            sessions[i] = new ClientSession(new TCPSocket(i));
        }
        Console.WriteLine("Session initialized " + count);
    }
    static public async Task Connect()
    {
        // 순차+Sleep 대신 동시 로그인 N개로 제한.
        int maxConcurrent = 100;
        using var gate = new SemaphoreSlim(maxConcurrent);
        var tasks = new List<Task>();

        async Task One(int idx)
        {
            try
            {
                for (int attempt = 1; attempt <= 3; attempt++)
                {
                    var session = sessions[idx];
                    if (session == null) return;
                    try
                    {
                        await AuthService.Instance.LoginAsync(session, "test" + (idx + 1), "12345");
                        (var address, var port) = await AuthService.Instance.GetSessionAsync(session);
                        await _network.Connect(session, address, port);
                        return;
                    }
                    catch (Exception e)
                    {
                        if (attempt == 3)
                        {
                            Console.WriteLine($"connect fail {idx + 1}: {e.Message}");
                        }
                        else
                        {
                            // 소켓이 살아있으면 재시도가 안 되므로 소켓/세션을 새로 만들어 처음부터.
                            session.GetSocket().Close();
                            sessions[idx] = new ClientSession(new TCPSocket(idx));
                            await Task.Delay(100 * attempt);
                        }
                    }
                }
            }
            finally { gate.Release(); }
        }

        for (int idx = 0; idx < sessions.Length; idx++)
        {
            var session = sessions[idx];
            if (session == null) continue;
            await gate.WaitAsync();
            tasks.Add(One(idx));
        }
        await Task.WhenAll(tasks);
        Console.WriteLine("connect ");
    }
    static public void Ready()
    {
        _ready++;
    }
    static public void StartAction()
    {
        int last = -1, stable = 0;
        while (true)
        {
            int r = _ready;
            Console.WriteLine($"ready={r}/{_count}");
            if (r >= _count) break;
            if (r == last) { if (++stable >= 5) { Console.WriteLine($"[start] {r} ready, {_count - r} failed -> 진행"); break; } }
            else { last = r; stable = 0; }
            Thread.Sleep(1000);
        }
    }
    // 서버 틱당 이동 예산(MOVE_BUDGET_PER_TICK = 1.0)의 절반만 쓴다. Unity 클라이언트와 같은 속도다.
    // 예산과 동일하게 맞추면 부하 상황에서 서버 틱이 밀릴 때
    // 정상 이동이 거부되고 치트 카운트가 쌓여 세션이 끊긴다.
    private const float MOVE_STEP = 0.5f;

    static public void Action()
    {
        tick++;
        if ((tick & 15) == 15)
        {
            dir++;
            dir &= 3;
        }
        // 좌표 기반으로 바뀌면서 세션마다 패킷이 달라져 더 이상 공유할 수 없다.
        // 대신 마샬링 없는 고정 레이아웃 기록(CreateActionPacketFast)으로 빌드 비용을 낮췄다.
        int alive = 0;
        for (int idx = 0; idx < sessions.Length; idx++)
        {
            var session = sessions[idx];
            if (session == null)
                continue;
            alive++;
            byte[] pkt = session.BuildMovePacket(dir, MOVE_STEP);
            if (pkt == null)
                continue; // 아직 시작 좌표를 못 받은 세션
            _ = SendActionAsync(session.GetSocket(), pkt, idx);
        }
        _count = alive;
    }
    static async Task SendActionAsync(TCPSocket sock, byte[] pkt, int idx)
    {
        // 하나 끊겨도 전체는 계속. 죽은 슬롯만 비우고 소켓 정리.
        if (!await _network.SendPacket(sock, pkt))
        {
            sessions[idx] = null;
            sock.Close();
        }
    }
    static async Task Main(string[] args)
    {
        // 하이브리드 CPU: P코어(논리 0~11)에만 배치 → tick/수신 continuation이 E코어로 강등돼 stall하는 것 방지.
        try
        {
            nint pMask = (nint)0xFFF;
            System.Diagnostics.Process.GetCurrentProcess().ProcessorAffinity = pMask;
            Console.WriteLine($"client affinity set: 0x{(long)pMask:X}");
        }
        catch (Exception e) { Console.WriteLine("affinity set fail: " + e.Message); }

        ThreadPool.SetMinThreads(12, 12);

        int clientCount = 5000;

        Initialize(clientCount);
        await Connect();
        StartAction();

        Console.WriteLine("Dummy clients running...");
        _clientTick.Start();
        while (true)
        {
            Console.WriteLine($"alive={_count}");
            Thread.Sleep(10000);
        }
    }
}
