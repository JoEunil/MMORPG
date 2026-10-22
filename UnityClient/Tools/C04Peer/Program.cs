using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text.Json;
using ClientCore;
using ClientCore.PacketHelper;

namespace C04Peer;

internal static class Program
{
    private static readonly ConcurrentQueue<Action> Jobs = new();

    private static async Task<int> Main(string[] args)
    {
        bool mapCheck = args.Length == 1 && args[0] == "--map-check";
        string? loginId = null;
        string? password = null;
        Initializer? client = null;
        IViewDataUI? view = null;
        string readyPath = Path.GetFullPath(mapCheck || args.Length == 0 ? "UnityClient/Captures/c04-peer-ready.txt" : args[0]);
        var deadline = Stopwatch.StartNew();
        ushort zone = 0;
        uint ownId = 0;
        float ownX = 0, ownY = 0;
        int ownHp = -1;
        int positionUpdateCount = 0;
        ulong lastEpoch = 0;
        ushort transitionTarget = 0;
        bool zoneSeen = false, ownFullSeen = false, monsterFullSeen = false, transitionPending = false;
        bool zoneFailed = false, connectionFailed = false;
        string stage = "login";
        float startX = 0, startY = 0;
        float wallStartX = 0, wallMaxX = 0, outboundX = 0, returnX = 0;
        int zoneTwoMonsterCount = 0;
        void CheckMapState()
        {
            if (connectionFailed || zoneFailed || ownHp <= 0 || deadline.Elapsed >= TimeSpan.FromSeconds(120))
                throw new InvalidOperationException();
        }
        async Task MoveTo(float target, bool xAxis, byte direction, TimeSpan timeout)
        {
            var segment = Stopwatch.StartNew();
            var lastProgress = Stopwatch.StartNew();
            float lastPosition = xAxis ? ownX : ownY;
            while (true)
            {
                DrainJobs();
                while (transitionPending || !zoneSeen || !ownFullSeen || !monsterFullSeen)
                {
                    if (connectionFailed || zoneFailed || ownHp == 0 || segment.Elapsed >= timeout ||
                        deadline.Elapsed >= TimeSpan.FromSeconds(120)) throw new InvalidOperationException();
                    DrainJobs();
                    await Task.Delay(10);
                }
                CheckMapState();
                float position = xAxis ? ownX : ownY;
                float remaining = target - position;
                if (MathF.Abs(remaining) <= 0.25f) return;
                float other = xAxis ? ownY : ownX;
                float candidate = position + Math.Clamp(remaining, -0.6f, 0.6f);
                view!.UpdateMove(direction, xAxis ? candidate : other, xAxis ? other : candidate);
                DrainJobs();
                if (MathF.Abs(position - lastPosition) > 0.02f)
                {
                    lastPosition = position;
                    lastProgress.Restart();
                }
                if (lastProgress.Elapsed >= TimeSpan.FromSeconds(2) || segment.Elapsed >= timeout)
                    throw new InvalidOperationException();
                await Task.Delay(100);
            }
        }
        try
        {
            ReadAccountInMemory(out loginId, out password);
            client = new Initializer();
            client.Initialize(new QuietLogger(), new QueueDispatcher(Jobs));
            var model = client.GetViewModel();
            view = client.GetViewData();
            int authCount = 0, listCount = 0;
            bool authenticated = false;
            model.OnAuthReceived += ok => { authenticated = ok; authCount++; };
            model.OnCharacterListResult += (_, _) => listCount++;
            model.OnConnectionFailed += _ => connectionFailed = true;
            model.OnZoneChageReceived += (receivedZone, _, internalId, _, _) =>
            {
                if (transitionPending && receivedZone != transitionTarget) zoneFailed = true;
                zone = receivedZone; ownId = internalId; zoneSeen = true;
                transitionPending = false;
            };
            model.OnZoneTransitionStarted += (epoch, targetZone) =>
            {
                if (epoch <= lastEpoch) zoneFailed = true;
                lastEpoch = epoch;
                transitionTarget = targetZone;
                transitionPending = true;
                zoneSeen = ownFullSeen = monsterFullSeen = false;
                ownHp = -1;
            };
            model.OnFullReceived += (count, states) =>
            {
                for (int i = 0; i < count; i++)
                {
                    var state = states[i];
                    if (state.zoneInternalID == ownId && ownId != 0)
                    {
                        ownFullSeen = true;
                        ownX = state.x;
                        ownY = state.y;
                        ownHp = state.hp;
                        positionUpdateCount++;
                    }
                }
            };
            model.OnDeltaReceived += (count, updates) =>
            {
                for (int i = 0; i < count; i++)
                {
                    var update = updates[i];
                    if (update.zoneInternalID != ownId) continue;
                    if (update.fieldID == 0) ownHp = unchecked((int)update.fieldVal);
                    else if (update.fieldID == 7) { ownX = BitConverter.UInt32BitsToSingle(update.fieldVal); positionUpdateCount++; }
                    else if (update.fieldID == 8) { ownY = BitConverter.UInt32BitsToSingle(update.fieldVal); positionUpdateCount++; }
                }
            };
            model.OnMonsterFullReceived += (count, states) =>
            {
                monsterFullSeen = true;
                if (zone == 2) zoneTwoMonsterCount = count;
            };
            model.OnZoneChageFailed += () => zoneFailed = true;

            await model.Login(loginId, password);
            await PumpUntil(() => listCount > 0, deadline, "character list",
                mapCheck ? TimeSpan.FromSeconds(120) : TimeSpan.FromMinutes(5), mapCheck ? () => connectionFailed : null);
            var rows = view.GetCharList();
            if (!authenticated || authCount != 1 || listCount != 1 || rows.Item1 != 1 || rows.Item2.Count != 1 ||
                rows.Item2[0].CharacterID != 5012UL || rows.Item2[0].Name.TrimEnd('\0') != "unity_ai_02_c02")
            {
                if (mapCheck) { stage = "list"; throw new InvalidOperationException(); }
                Write(new { kind = "FAIL", stage = "list", authCount, listCount, status = rows.Item1, rowCount = rows.Item2.Count });
                return 2;
            }
            model.Enter(rows.Item2[0].CharacterID);
            await PumpUntil(() => zoneSeen && ownFullSeen && (!mapCheck || monsterFullSeen), deadline,
                "server Zone and own Full", mapCheck ? TimeSpan.FromSeconds(120) : TimeSpan.FromMinutes(5),
                mapCheck ? () => connectionFailed : null);
            if (mapCheck)
            {
                stage = "town-start";
                CheckMapState();
                if (zone != 1 || MathF.Abs(ownX - 10) > 0.01f || (ownY < 10 || ownY > 44.01f) || ownHp <= 0)
                    throw new InvalidOperationException();
                startX = ownX; startY = ownY;
                stage = "wall-y";
                await MoveTo(44, false, 0, TimeSpan.FromSeconds(20));
                stage = "wall-approach";
                await MoveTo(61.5f, true, 3, TimeSpan.FromSeconds(25));
                if (zone != 1 || ownY < 39 || ownY > 49 || ownX < 61 || ownX >= 62)
                    throw new InvalidOperationException();
                wallStartX = ownX;
                wallMaxX = ownX;
                int wallStartUpdates = positionUpdateCount;
                stage = "wall-block";
                var wallTimer = Stopwatch.StartNew();
                while (wallTimer.Elapsed < TimeSpan.FromSeconds(1.5))
                {
                    CheckMapState();
                    view.UpdateMove(3, ownX + 0.6f, ownY);
                    DrainJobs();
                    wallMaxX = MathF.Max(wallMaxX, ownX);
                    if (zone != 1 || wallMaxX >= 62) throw new InvalidOperationException();
                    await Task.Delay(100);
                }
                DrainJobs();
                wallMaxX = MathF.Max(wallMaxX, ownX);
                if (wallMaxX < 61 || wallMaxX >= 62 || positionUpdateCount <= wallStartUpdates)
                    throw new InvalidOperationException();
                stage = "return-y";
                await MoveTo(10, false, 1, TimeSpan.FromSeconds(25));
                stage = "cross-to-zone-2";
                await MoveTo(208, true, 3, TimeSpan.FromSeconds(60));
                if (zone != 2 || !zoneSeen || !ownFullSeen || !monsterFullSeen)
                    throw new InvalidOperationException();
                outboundX = ownX;
                ushort outboundZone = zone;
                ulong outboundEpoch = lastEpoch;
                stage = "return-to-zone-1";
                await MoveTo(192, true, 2, TimeSpan.FromSeconds(15));
                if (zone != 1 || !zoneSeen || !ownFullSeen || !monsterFullSeen || lastEpoch <= outboundEpoch)
                    throw new InvalidOperationException();
                returnX = ownX;
                CheckMapState();
                view.CancelMove();
                Write(new { kind = "MAP_CHECK", status = "PASS", startZone = 1, startX, startY,
                    wallStartX, wallMaxX, outboundZone, outboundX,
                    outboundEpoch, returnZone = zone, returnX, returnEpoch = lastEpoch,
                    zoneTwoMonsterCount, hp = ownHp });
                return 0;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(readyPath)!);
            File.WriteAllText(readyPath, JsonSerializer.Serialize(new { ownId, zone, x = ownX, y = ownY }) + Environment.NewLine);
            Write(new { kind = "READY", ownId, zone, x = ownX, y = ownY });
            // Console's synchronized reader can block before ReadLineAsync returns on Windows.
            // Keep that wait off the deadline/dispatcher loop; stdin still follows the same stop path.
            var stopTask = Task.Run(() => Console.In.ReadLine());
            while (deadline.Elapsed < TimeSpan.FromMinutes(5))
            {
                DrainJobs();
                if (stopTask.IsCompleted) break;
                await Task.Delay(10);
            }
            if (!stopTask.IsCompleted)
            {
                Write(new { kind = "FAIL", stage = "five-minute-bound" });
                return 4;
            }
            Write(new { kind = "EXIT" });
            return 0;
        }
        catch
        {
            // Do not write exception text: HTTP/network errors may contain credentials or session data.
            if (mapCheck)
            {
                view?.CancelMove();
                Write(new { kind = "MAP_CHECK", status = "FAIL", stage, zone, x = ownX, y = ownY,
                    hp = ownHp, wallStartX, wallMaxX, outboundX, returnX, lastEpoch });
            }
            else Write(new { kind = "FAIL", stage = "peer" });
            return 3;
        }
        finally
        {
            if (mapCheck) view?.CancelMove();
            client?.Dispose();
            loginId = null;
            password = null;
        }
    }

    private static async Task PumpUntil(Func<bool> condition, Stopwatch deadline, string stage, TimeSpan? timeout = null,
        Func<bool>? abort = null)
    {
        while (!condition() && deadline.Elapsed < (timeout ?? TimeSpan.FromMinutes(5)))
        {
            DrainJobs();
            if (abort?.Invoke() == true) throw new InvalidOperationException();
            await Task.Delay(10);
        }
        if (!condition()) throw new TimeoutException(stage);
    }

    private static void DrainJobs()
    {
        while (Jobs.TryDequeue(out var job)) job();
    }

    private static void ReadAccountInMemory(out string loginId, out string password)
    {
        var path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "UnityCoordinator", "test-accounts.xml");
        if (!File.Exists(path)) path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "AppData", "Local", "UnityCoordinator", "test-accounts.xml");
        if (!File.Exists(path)) throw new InvalidOperationException();
        string script = "$ErrorActionPreference='Stop'; $rows=@(Import-Clixml -LiteralPath '" + path.Replace("'", "''") + "'); " +
            "$row=$rows | Where-Object { $_.LoginId -eq 'unity_ai_02' } | Select-Object -First 1; " +
            "if($null -eq $row){exit 3}; $b=[Runtime.InteropServices.Marshal]::SecureStringToBSTR($row.Password); " +
            "try{$pwd=[Runtime.InteropServices.Marshal]::PtrToStringBSTR($b); " +
            "[Console]::Out.Write((@{LoginId=$row.LoginId;Password=$pwd}|ConvertTo-Json -Compress))} " +
            "finally{[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($b);$pwd=$null}";
        using var process = Process.Start(new ProcessStartInfo
        {
            FileName = "powershell.exe",
            Arguments = "-NoProfile -NonInteractive -EncodedCommand " +
                Convert.ToBase64String(System.Text.Encoding.Unicode.GetBytes(script)),
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        }) ?? throw new InvalidOperationException();
        string json = process.StandardOutput.ReadToEnd();
        process.WaitForExit(10000);
        if (process.ExitCode != 0) { json = string.Empty; throw new InvalidOperationException(); }
        using var document = JsonDocument.Parse(json);
        var root = document.RootElement;
        loginId = root.GetProperty("LoginId").GetString() ?? throw new InvalidOperationException();
        password = root.GetProperty("Password").GetString() ?? throw new InvalidOperationException();
        json = string.Empty;
    }

    private static void Write<T>(T record)
    {
        Console.WriteLine(JsonSerializer.Serialize(record));
    }

    private sealed class QuietLogger : ILogger { public void Log(string message) { } }
    private sealed class QueueDispatcher(ConcurrentQueue<Action> jobs) : IMainThreadDispatcher
    {
        public void Post(Action job) => jobs.Enqueue(job);
    }
}
