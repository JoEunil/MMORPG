using ClientCore;
using ClientCore.Network;
using ClientCore.PacketHelper;
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
    static public ClientSession? GetSession(int index)
    {
        if (index < 0 || index >= sessions.Length) return null;
        return sessions[index];
    }
    static public void CountReady(int index, ClientSession session)
    {
        if (ReferenceEquals(GetSession(index), session) && session.TryMarkReadyCounted())
            Interlocked.Increment(ref _ready);
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
    // Keep simulator requests below the server's per-tick movement budget.
    private const float MOVE_STEP = 0.2f;

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
            long now = Environment.TickCount64;
            byte[]? resync = session.BuildBootstrapResyncPacket(now);
            if (resync != null)
                _ = SendPacketAsync(session, resync, idx);
            if (session.TakeBootstrapFailureReport(now))
                Console.Error.WriteLine($"bootstrap failed after 3 resync attempts for session {idx + 1}; actions remain disabled");
            byte[] pkt = session.BuildMovePacket(dir, MOVE_STEP);
            if (pkt == null)
                continue; // 아직 시작 좌표를 못 받은 세션
            _ = SendActionAsync(session, pkt, idx);
        }
        _count = alive;
    }
    static async Task SendActionAsync(ClientSession session, byte[] pkt, int idx)
    {
        var sock = session.GetSocket();
        // 하나 끊겨도 전체는 계속. 죽은 슬롯만 비우고 소켓 정리.
        if (!await _network.SendPacket(sock, pkt))
        {
            if (ReferenceEquals(sessions[idx], session)) sessions[idx] = null;
            sock.Close();
        }
    }
    static async Task SendPacketAsync(ClientSession session, byte[] pkt, int idx)
    {
        var sock = session.GetSocket();
        if (!await _network.SendPacket(sock, pkt))
        {
            if (ReferenceEquals(sessions[idx], session)) sessions[idx] = null;
            sock.Close();
        }
    }
    private static void RunProtocolSelfTest()
    {
        static void Require(bool condition, string name)
        {
            if (!condition) throw new InvalidOperationException("protocol self-test failed: " + name);
        }

        const ulong epoch = 0x0102030405060708;
        var fast = PacketBuilder.CreateActionPacketFast(3, 12.5f, -4.25f, Config.NONE_SKILL, epoch);
        var marshalled = PacketBuilder.CreateActionPacket(3, 12.5f, -4.25f, Config.NONE_SKILL, epoch);
        Require(fast.Length == 26 && marshalled.Length == 26 && fast.AsSpan().SequenceEqual(marshalled), "both action builders emit the 26-byte epoch frame");
        Require(BitConverter.ToUInt64(fast, 18) == epoch, "action epoch offset");
        Require(Handler.ShouldHandle((byte)OP_CODE.ZONE_FULL_STATE_BROADCAST, Config.FLAG_SIMULATION) &&
            Handler.ShouldHandle((byte)OP_CODE.ZONE_DELTA_UPDATE_BROADCAST, Config.FLAG_SIMULATION), "snapshot frames pass the receive filter");

        var session = new ClientSession(new TCPSocket(0));
        session.SetZoneOwnership(1, 10, 20, 77, 8, 9);
        session.ObservePlayerFull(1, 10, 19, 300, true, true, 30, 40);
        session.ObserveMonsterFull(1, 10, 19, 300, true);
        Require(session.BuildMovePacket(3, MOVE_STEP) == null, "old ownership bootstrap cannot enable actions");
        session.ObservePlayerFull(1, 10, 20, 301, false, true, 30, 40);
        session.ObservePlayerFull(1, 10, 20, 301, true, false, 0, 0);
        session.ObserveMonsterFull(1, 10, 20, 302, true);
        Require(session.BuildMovePacket(3, MOVE_STEP) == null, "different bootstrap IDs cannot complete the pair");
        session.ObservePlayerFull(1, 10, 20, 302, true, true, 30, 40);
        session.ObserveMonsterFull(1, 10, 20, 302, true); // Empty monster snapshots are valid.
        Require(session.BuildMovePacket(3, MOVE_STEP) != null, "matching player and empty-monster bootstrap enables actions");

        session.SetZoneOwnership(2, 11, 21, 78, 50, 6);
        session.SetZoneOwnership(1, 10, 20, 77, 999, 999);
        Require(session.OwnZoneInternalID == 78, "old zone ownership acknowledgement cannot replace the current binding");
        Require(session.BuildMovePacket(3, MOVE_STEP) == null, "zone transition disables actions until its new bootstrap");
        session.ObservePlayerFull(1, 10, 20, 301, true, true, 999, 999);
        session.ObservePlayerFull(2, 11, 21, 400, true, true, 51, 7);
        session.ObserveMonsterFull(2, 11, 21, 400, true);
        session.ObserveDelta(2, 10, 78, 800, 800);
        session.ObserveDelta(2, 12, 78, 52, 8.5f);
        session.ObserveDelta(2, 11, 78, 999, 999);
        var moved = session.BuildMovePacket(3, MOVE_STEP)!;
        Require(Math.Abs(BitConverter.ToSingle(moved, 9) - 52.2f) < 0.001f && Math.Abs(BitConverter.ToSingle(moved, 13) - 8.5f) < 0.001f,
            "newer generation delta rebases movement while stale generation is ignored");

        var ack = new byte[47];
        BitConverter.TryWriteBytes(ack.AsSpan(0), Config.MAGIC);
        BitConverter.TryWriteBytes(ack.AsSpan(2), 47u);
        ack[6] = (byte)OP_CODE.ZONE_CHANGE_RESPONSE;
        ack[7] = Config.FLAG_SIMULATION;
        ack[8] = 1;
        BitConverter.TryWriteBytes(ack.AsSpan(9), (ushort)2);
        BitConverter.TryWriteBytes(ack.AsSpan(11), 900ul);
        BitConverter.TryWriteBytes(ack.AsSpan(19), 78u);
        BitConverter.TryWriteBytes(ack.AsSpan(23), 52f);
        BitConverter.TryWriteBytes(ack.AsSpan(27), 8f);
        BitConverter.TryWriteBytes(ack.AsSpan(31), 11ul);
        BitConverter.TryWriteBytes(ack.AsSpan(39), 21ul);
        var ackHeader = Packet.ExtractHeader(ack);
        Require(Packet.DeserializeZoneChange(ackHeader, ack).body.ownershipEpoch == 21, "zone acknowledgement decodes generation and epoch");

        Initialize(1);
        var currentSession = GetSession(0)!;
        currentSession.SetZoneOwnership(1, 10, 20, 90, 8, 9);
        var capturedOldSession = new ClientSession(new TCPSocket(0));
        capturedOldSession.SetZoneOwnership(1, 10, 19, 77, 8, 9);
        new Handler().HandleResponse(0, capturedOldSession, ack);
        Require(currentSession.OwnZoneInternalID == 90 && capturedOldSession.OwnZoneInternalID == 78,
            "response captured by a replaced session cannot mutate the current slot");

        bool rejectedShortAck = false;
        try { Packet.DeserializeZoneChange(ackHeader, ack[..^1]); }
        catch (ArgumentException) { rejectedShortAck = true; }
        Require(rejectedShortAck, "invalid acknowledgement length is rejected");
        Console.WriteLine("protocol self-test: PASS (action frames, bootstrap pair/epoch, authoritative movement, replaced-session isolation, invalid length)");
    }
    static async Task<int> Main(string[] args)
    {
        if (args.Length != 0)
        {
            if (args.Length == 1 && args[0] == "--protocol-self-test")
            {
                RunProtocolSelfTest();
                return 0;
            }
            Console.Error.WriteLine("Usage: DummyClients [--protocol-self-test]");
            return 2;
        }
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
