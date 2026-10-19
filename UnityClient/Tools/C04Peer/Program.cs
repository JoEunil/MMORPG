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
        string? loginId = null;
        string? password = null;
        Initializer? client = null;
        string readyPath = Path.GetFullPath(args.Length == 0 ? "UnityClient/Captures/c04-peer-ready.txt" : args[0]);
        var deadline = Stopwatch.StartNew();
        ushort zone = 0;
        uint ownId = 0;
        float ownX = 0, ownY = 0;
        bool zoneSeen = false;
        bool ownFullSeen = false;
        try
        {
            ReadAccountInMemory(out loginId, out password);
            client = new Initializer();
            client.Initialize(new QuietLogger(), new QueueDispatcher(Jobs));
            var model = client.GetViewModel();
            var view = client.GetViewData();
            int authCount = 0, listCount = 0;
            bool authenticated = false;
            model.OnAuthReceived += ok => { authenticated = ok; authCount++; };
            model.OnCharacterListResult += (_, _) => listCount++;
            model.OnZoneChageReceived += (receivedZone, _, internalId, _, _) =>
            {
                zone = receivedZone; ownId = internalId; zoneSeen = true;
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
                    }
                }
            };

            await model.Login(loginId, password);
            await PumpUntil(() => listCount > 0, deadline, "character list");
            var rows = view.GetCharList();
            if (!authenticated || authCount != 1 || listCount != 1 || rows.Item1 != 1 || rows.Item2.Count != 1 ||
                rows.Item2[0].CharacterID != 5012UL || rows.Item2[0].Name.TrimEnd('\0') != "unity_ai_02_c02")
            {
                Write(new { kind = "FAIL", stage = "list", authCount, listCount, status = rows.Item1, rowCount = rows.Item2.Count });
                return 2;
            }
            model.Enter(rows.Item2[0].CharacterID);
            await PumpUntil(() => zoneSeen && ownFullSeen, deadline, "server Zone and own Full");
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
            Write(new { kind = "FAIL", stage = "peer" });
            return 3;
        }
        finally
        {
            client?.Dispose();
            loginId = null;
            password = null;
        }
    }

    private static async Task PumpUntil(Func<bool> condition, Stopwatch deadline, string stage)
    {
        while (!condition() && deadline.Elapsed < TimeSpan.FromMinutes(5))
        {
            DrainJobs();
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
            Arguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " +
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
