using System.Collections.Concurrent;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml.Linq;
using ClientCore;

namespace C06Peer;

internal static class Program
{
    private static readonly ConcurrentQueue<Action> Jobs = new();
    private static async Task<int> Main(string[] args)
    {
        // All command-line values are public run metadata; credentials never cross a process boundary.
        if (args.Length < 3 || args.Length > 4 || !Regex.IsMatch(args[0], "^[A-Za-z0-9_-]{1,48}$") ||
            args.Length == 4 && (!int.TryParse(args[3], out int limit) || limit < 30 || limit > 300))
        { Console.WriteLine("FAIL invalid-arguments"); return 2; }
        string runId = args[0], readyPath = Path.GetFullPath(args[1]), resultPath = Path.GetFullPath(args[2]);
        int timeout = args.Length == 4 ? int.Parse(args[3]) : 180;
        string? loginId = null, password = null;
        Initializer? client = null;
        var deadline = Stopwatch.StartNew();
        ulong ownChatId = 0, receivedChatId = 0;
        uint ownId = 0; ushort zone = 0;
        bool zoneSeen = false, ownFullSeen = false, authenticated = false, failed = false, sent = false;
        int authCount = 0, listCount = 0, received = 0, echoed = 0, code = 3;
        string stage = "credentials";
        string expected = "C06 " + runId + " 123 <b>literal</b>", reply = "C06 peer " + runId;
        try
        {
            ReadAccountInMemory(out loginId, out password);
            client = new Initializer(); client.Initialize(new QuietLogger(), new QueueDispatcher());
            var model = client.GetViewModel(); var view = client.GetViewData();
            model.OnAuthReceived += ok => { authenticated = ok; authCount++; };
            model.OnCharacterListResult += (_, _) => listCount++;
            model.OnConnectionFailed += _ => failed = true;
            model.OnZoneChageReceived += (z, chat, id, _, _) => { zone = z; ownChatId = chat; ownId = id; zoneSeen = true; };
            model.OnFullReceived += (count, states) => { for (int i = 0; i < count; i++) if (states[i].zoneInternalID == ownId && ownId != 0) ownFullSeen = true; };
            model.OnChatReceived += (scope, chat, _, text) =>
            {
                if (scope != 0) return;
                if (text == expected && chat != ownChatId)
                {
                    received++; receivedChatId = chat;
                    if (!sent) { model.Chat(reply, 0, 0); sent = true; }
                }
                if (text == reply && chat == ownChatId) echoed++;
            };
            stage = "login"; await model.Login(loginId, password); loginId = null; password = null;
            await PumpUntil(() => listCount > 0, () => failed, deadline, timeout);
            var rows = view.GetCharList();
            if (!authenticated || authCount != 1 || listCount != 1 || rows.Item1 != 1 || rows.Item2.Count != 1 || rows.Item2[0].CharacterID != 5012UL)
                throw new InvalidOperationException();
            stage = "enter"; model.Enter(5012);
            await PumpUntil(() => zoneSeen && ownFullSeen && ownChatId != 0, () => failed, deadline, timeout);
            Save(readyPath, "status=READY", "runId=" + runId, "chatId=" + ownChatId, "zone=" + zone, "internalId=" + ownId);
            Console.WriteLine("READY " + runId);
            stage = "chat"; await PumpUntil(() => received == 1 && echoed == 1, () => failed || received > 1 || echoed > 1, deadline, timeout);
            // Keep the loaded profile available briefly for the other client's normal profile request.
            var grace = Stopwatch.StartNew();
            while (grace.Elapsed < TimeSpan.FromSeconds(3)) { Drain(); if (failed || received != 1 || echoed != 1) throw new InvalidOperationException(); await Task.Delay(10); }
            stage = "complete"; code = 0;
        }
        catch { Console.WriteLine("FAIL " + stage); }
        finally
        {
            client?.Dispose(); loginId = null; password = null;
            Save(resultPath, "status=" + (code == 0 ? "PASS" : "FAIL"), "stage=" + stage, "runId=" + runId,
                "chatId=" + ownChatId, "otherChatId=" + receivedChatId, "received=" + received, "sent=" + (sent ? 1 : 0), "echoed=" + echoed, "disposed=1");
        }
        Console.WriteLine(code == 0 ? "PASS normal-dispose" : "FAIL normal-dispose");
        return code;
    }
    private static async Task PumpUntil(Func<bool> condition, Func<bool> failed, Stopwatch elapsed, int timeout)
    {
        while (!condition()) { Drain(); if (failed() || elapsed.Elapsed >= TimeSpan.FromSeconds(timeout)) throw new InvalidOperationException(); await Task.Delay(10); }
    }
    private static void Drain() { while (Jobs.TryDequeue(out var job)) job(); }
    private static void Save(string path, params string[] lines) { Directory.CreateDirectory(Path.GetDirectoryName(path)!); File.WriteAllLines(path, lines); }
    private static void ReadAccountInMemory(out string loginId, out string password)
    {
        // Same DPAPI CLIXML and dedicated second account used by C04Peer, without secret subprocess stdout.
        string path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnityCoordinator", "test-accounts.xml");
        if (!File.Exists(path)) path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "AppData", "Local", "UnityCoordinator", "test-accounts.xml");
        var document = XDocument.Load(path); XNamespace ns = "http://schemas.microsoft.com/powershell/2004/04";
        var members = document.Descendants(ns + "MS").Single(row => row.Elements(ns + "S").Any(e => (string?)e.Attribute("N") == "LoginId" && e.Value == "unity_ai_02"));
        loginId = "unity_ai_02";
        byte[] cipher = Convert.FromHexString(members.Elements(ns + "SS").Single(e => (string?)e.Attribute("N") == "Password").Value);
        var input = new DataBlob { Length = cipher.Length, Data = Marshal.AllocHGlobal(cipher.Length) };
        var output = new DataBlob();
        try
        {
            Marshal.Copy(cipher, 0, input.Data, cipher.Length);
            if (!CryptUnprotectData(ref input, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, 1, out output)) throw new InvalidOperationException();
            byte[] plain = new byte[output.Length];
            try { Marshal.Copy(output.Data, plain, 0, plain.Length); password = Encoding.Unicode.GetString(plain); }
            finally { CryptographicOperations.ZeroMemory(plain); }
        }
        finally
        {
            for (int i = 0; i < input.Length; i++) Marshal.WriteByte(input.Data, i, 0);
            Marshal.FreeHGlobal(input.Data); CryptographicOperations.ZeroMemory(cipher);
            if (output.Data != IntPtr.Zero) { for (int i = 0; i < output.Length; i++) Marshal.WriteByte(output.Data, i, 0); LocalFree(output.Data); }
        }
    }
    [StructLayout(LayoutKind.Sequential)] private struct DataBlob { public int Length; public IntPtr Data; }
    [DllImport("crypt32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CryptUnprotectData(ref DataBlob input, IntPtr description, IntPtr entropy, IntPtr reserved, IntPtr prompt, int flags, out DataBlob output);
    [DllImport("kernel32.dll")] private static extern IntPtr LocalFree(IntPtr memory);
    private sealed class QuietLogger : ILogger { public void Log(string message) { } }
    private sealed class QueueDispatcher : IMainThreadDispatcher { public void Post(Action job) => Jobs.Enqueue(job); }
}
