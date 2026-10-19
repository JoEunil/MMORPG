param([switch]$Send)
$ErrorActionPreference = 'Stop'
# Only the first test peer whose stdin session handle was lost. Never stop a server.
$taskImage = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'C04Peer/bin/Debug/net9.0/C04Peer.exe'))
$taskExpectedHash = '64369E2C5468A00085EADA7E56B91EEA1B9F335A9B72DB8B7FC4987AFF805BA1'
function Get-ExactPeer {
    $taskPeer = Get-Process -Id 30528 -ErrorAction Stop
    if ($taskPeer.StartTime.ToUniversalTime().Ticks -ne 639279134932903697 -or
        $taskPeer.Path -ne $taskImage -or (Get-FileHash -LiteralPath $taskPeer.Path).Hash -ne $taskExpectedHash) {
        throw 'Peer identity changed; no input sent.'
    }
    $taskInfo = Get-CimInstance Win32_Process -Filter 'ProcessId=30528'
    if ($taskInfo.ParentProcessId -ne 9652) { throw 'Peer parent changed; no input sent.' }
    $taskParent = Get-Process -Id 9652 -ErrorAction Stop
    if ($taskParent.Path -ne 'C:\Program Files\dotnet\dotnet.exe') { throw 'Unexpected peer parent.' }
    if ((Get-CimInstance Win32_Process -Filter 'ProcessId=9652').ParentProcessId -ne 320) { throw 'Unexpected parent shell.' }
    $taskShell = Get-Process -Id 320 -ErrorAction Stop
    if ($taskShell.Path -ne 'C:\Users\joeun\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe') { throw 'Unexpected test shell image.' }
    return $taskPeer
}
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class C04PeerConsole {
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    public struct Key { public int down; public ushort repeat, key, scan; public char character; public uint control; }
    [StructLayout(LayoutKind.Explicit, Size=20)]
    public struct Record { [FieldOffset(0)] public ushort type; [FieldOffset(4)] public Key key; }
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool FreeConsole();
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool AttachConsole(uint pid);
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint GetConsoleProcessList([Out] uint[] ids, uint count);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern SafeFileHandle CreateFileW(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool WriteConsoleInputW(SafeFileHandle input, Record[] records, uint count, out uint written);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool GetExitCodeProcess(IntPtr handle, out uint code);
    static string Check() {
        var ids = new uint[64];
        uint count = GetConsoleProcessList(ids, 64);
        uint self = (uint)System.Diagnostics.Process.GetCurrentProcess().Id;
        bool target = false, helper = false;
        if (count < 2 || count > 4) throw new InvalidOperationException("Console is not exclusive to this test peer; count=" + count + "; ids=" + string.Join(",", ids));
        for (int i=0; i<count; i++) {
            if (ids[i] == 30528) target = true;
            else if (ids[i] == self) helper = true;
            else if (ids[i] != 9652 && ids[i] != 320) throw new InvalidOperationException("Unrelated console client; no input sent.");
        }
        if (!target || !helper) throw new InvalidOperationException("Target/helper missing from console.");
        return "Exclusive console verified; clients=" + count;
    }
    public static string Run(bool send) {
        FreeConsole();
        if (!AttachConsole(30528)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot attach test peer console.");
        try {
            string result = Check();
            if (!send) return result + "; dry-run; no input sent.";
            using (var input = CreateFileW("CONIN$", 0xC0000000, 3, IntPtr.Zero, 3, 0, IntPtr.Zero)) {
                if (input.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error());
                var records = new Record[2];
                records[0].type = records[1].type = 1;
                records[0].key = new Key { down=1, repeat=1, key=13, scan=28, character='\r' };
                records[1].key = new Key { down=0, repeat=1, key=13, scan=28, character='\r' };
                Check();
                uint written;
                if (!WriteConsoleInputW(input, records, 2, out written) || written != 2)
                    throw new Win32Exception(Marshal.GetLastWin32Error());
            }
            return result + "; Enter down/up sent to normal ReadLine stop path.";
        } finally { FreeConsole(); }
    }
}
'@
$taskReceipt = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Captures/c04-orphan-peer-stop.txt'))
try {
    [IO.File]::AppendAllText($taskReceipt, [DateTime]::UtcNow.ToString('O') + ' send=' + $Send.IsPresent + [Environment]::NewLine)
    $taskPeer = Get-ExactPeer
    $taskHandle = $taskPeer.Handle
    $null = Get-ExactPeer
    [IO.File]::AppendAllText($taskReceipt, 'Exact test peer identity verified.' + [Environment]::NewLine)
    $taskResult = [C04PeerConsole]::Run($Send.IsPresent)
    [IO.File]::AppendAllText($taskReceipt, $taskResult + [Environment]::NewLine)
    if ($Send) {
        if (!$taskPeer.WaitForExit(10000)) { throw 'Peer did not exit within 10 seconds; no force stop attempted.' }
        [uint32]$taskCode = 259
        if (![C04PeerConsole]::GetExitCodeProcess($taskHandle, [ref]$taskCode)) { throw 'Cannot verify peer exit code.' }
        [IO.File]::AppendAllText($taskReceipt, 'Peer exited through normal stdin path; exit code=' + $taskCode + [Environment]::NewLine)
        if ($taskCode -ne 0) { throw 'Peer exited with a nonzero code; preserve result.' }
    }
    exit 0
} catch {
    [IO.File]::AppendAllText($taskReceipt, 'FAIL: ' + $_.Exception.Message + [Environment]::NewLine)
    exit 1
}
