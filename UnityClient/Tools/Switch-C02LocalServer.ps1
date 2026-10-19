param(
    [int]$QuitHelperPid = 0,
    [ValidateSet('Original', 'New')][string]$QuitImage = 'Original',
    [long]$QuitStartTicks = 0,
    [long]$QuitDeadlineUtcTicks = 0
)
$ErrorActionPreference = 'Stop'
$originalExe = 'C:\Users\joeun\Desktop\workspace\Game\x64\Release\MainServer.exe'
$originalRoot = 'C:\Users\joeun\Desktop\workspace\Game'
$originalHash = '529D16A75BD84417ADCFC4DD632830758D6F2939362B367FF0170B41511A9BD6'
$newExe = 'C:\Users\joeun\Desktop\workspace\Game-UnityClient\x64\Release\MainServer.exe'
$newRoot = 'C:\Users\joeun\Desktop\workspace\Game-UnityClient'
$newHash = 'C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825'
$captures = Join-Path $newRoot 'UnityClient\Captures'

function Assert-Image([string]$Image, [string]$Hash) {
    if ((Get-FileHash -LiteralPath $Image -Algorithm SHA256).Hash -ne $Hash) { throw 'Fixed server image hash mismatch.' }
}
function Get-ExactProcess([int]$ProcessId, [string]$Image, [string]$Hash, [long]$StartTicks = 0) {
    Assert-Image $Image $Hash
    $info = Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" -ErrorAction Stop
    if ($null -eq $info -or $info.ExecutablePath -ine $Image) { throw 'Target PID/path mismatch.' }
    if ($info.CommandLine -notmatch ('^\s*"?' + [regex]::Escape($Image) + '"?\s*$')) { throw 'Unexpected server command-line arguments.' }
    $process = Get-Process -Id $ProcessId -ErrorAction Stop
    if ($StartTicks -ne 0 -and $process.StartTime.ToUniversalTime().Ticks -ne $StartTicks) { throw 'Target PID was reused.' }
    return $process
}
function Assert-NoTestFlags {
    foreach ($scope in @('Process', 'User', 'Machine')) {
        foreach ($item in [Environment]::GetEnvironmentVariables($scope).GetEnumerator()) {
            if ($item.Key -like 'TEST*' -and -not [string]::IsNullOrEmpty([string]$item.Value)) { throw 'TEST environment flags must be unset.' }
        }
    }
}
function Get-GameTcp {
    # Query failures must not be confused with an unused port.
    return @(Get-NetTCPConnection -ErrorAction Stop | Where-Object { $_.LocalPort -eq 9999 })
}
function Assert-NoClients {
    if (@(Get-GameTcp | Where-Object State -eq Established).Count -ne 0) { throw 'Game clients are still connected.' }
}
function Assert-Listener([int]$ProcessId) {
    $listeners = @(Get-GameTcp | Where-Object State -eq Listen)
    if ($listeners.Count -eq 0 -or @($listeners | Where-Object OwningProcess -ne $ProcessId).Count -ne 0) { throw 'Port 9999 listener ownership mismatch.' }
}
function Assert-PortFree {
    Assert-NoClients
    if (@(Get-GameTcp | Where-Object State -eq Listen).Count -ne 0) { throw 'Port 9999 is still listening.' }
}

# Native console access runs only in a dedicated child. The main shell's console is never detached.
if ($QuitHelperPid -gt 0) {
    if ($QuitDeadlineUtcTicks -le [DateTime]::UtcNow.Ticks) { throw 'Quit helper deadline is missing or expired.' }
    $image = $originalExe; $hash = $originalHash
    if ($QuitImage -eq 'New') { $image = $newExe; $hash = $newHash }
    $null = Get-ExactProcess $QuitHelperPid $image $hash $QuitStartTicks
    Assert-NoClients
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class C02ConsoleQuit {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct KeyEvent {
        public int down;
        public ushort repeat, key, scan;
        public char character;
        public uint control;
    }
    [StructLayout(LayoutKind.Explicit, Size = 20)]
    public struct InputRecord {
        [FieldOffset(0)] public ushort type;
        [FieldOffset(4)] public KeyEvent key;
    }
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool FreeConsole();
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool AttachConsole(uint pid);
    [DllImport("kernel32.dll", SetLastError = true)] static extern uint GetConsoleProcessList([Out] uint[] list, uint count);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern SafeFileHandle CreateFileW(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool WriteConsoleInputW(SafeFileHandle input, InputRecord[] events, uint count, out uint written);
    static void CheckClients(uint target) {
        var clients = new uint[64];
        uint count = GetConsoleProcessList(clients, (uint)clients.Length);
        uint self = (uint)System.Diagnostics.Process.GetCurrentProcess().Id;
        if (count != 2 || !((clients[0] == target && clients[1] == self) || (clients[1] == target && clients[0] == self)))
            throw new InvalidOperationException("Target console is not exclusive to target and helper.");
    }
    static void CheckDeadline(long deadlineUtcTicks) {
        if (deadlineUtcTicks <= 0 || DateTime.UtcNow.Ticks >= deadlineUtcTicks)
            throw new InvalidOperationException("Quit helper deadline expired; no input sent.");
    }
    public static void Send(uint target, long deadlineUtcTicks) {
        FreeConsole();
        CheckDeadline(deadlineUtcTicks);
        if (!AttachConsole(target)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot attach target console.");
        try {
            CheckClients(target);
            using (var input = CreateFileW("CONIN$", 0xC0000000, 3, IntPtr.Zero, 3, 0, IntPtr.Zero)) {
                if (input.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot open target console input.");
                var records = new InputRecord[2];
                records[0].type = records[1].type = 1;
                records[0].key = new KeyEvent { down = 1, repeat = 1, key = 0x51, scan = 0x10, character = 'q' };
                records[1].key = new KeyEvent { down = 0, repeat = 1, key = 0x51, scan = 0x10, character = 'q' };
                CheckClients(target);
                uint written;
                CheckDeadline(deadlineUtcTicks);
                if (!WriteConsoleInputW(input, records, 2, out written) || written != 2)
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not write exact quit key events.");
            }
        } finally { FreeConsole(); }
    }
}
'@
    $null = Get-ExactProcess $QuitHelperPid $image $hash $QuitStartTicks
    Assert-NoClients
    [C02ConsoleQuit]::Send([uint32]$QuitHelperPid, $QuitDeadlineUtcTicks)
    exit 0
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class C02HelperExit {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetExitCodeProcess(IntPtr process, out uint code);
}
'@

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$receipt = Join-Path $captures "c02-auth-switch-$stamp.txt"
function Record([string]$Message) {
    $line = (Get-Date -Format o) + ' ' + $Message
    Add-Content -LiteralPath $receipt -Value $line -Encoding UTF8
    Write-Host $line
}
function Stop-Normally([Diagnostics.Process]$Process, [string]$ImageKind, [string]$Image, [string]$Hash) {
    $ticks = $Process.StartTime.ToUniversalTime().Ticks
    $null = Get-ExactProcess $Process.Id $Image $Hash $ticks
    Assert-NoClients
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $helperOut = Join-Path $captures "c02-auth-switch-$stamp-quit-$($Process.Id).out.txt"
    $helperErr = Join-Path $captures "c02-auth-switch-$stamp-quit-$($Process.Id).err.txt"
    $deadlineUtcTicks = [DateTime]::UtcNow.AddSeconds(12).Ticks
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -QuitHelperPid ' + $Process.Id + ' -QuitImage ' + $ImageKind + ' -QuitStartTicks ' + $ticks + ' -QuitDeadlineUtcTicks ' + $deadlineUtcTicks
    $helper = Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $helperOut -RedirectStandardError $helperErr
    # Pin the process handle before the child exits; do not rely on PassThru.ExitCode.
    try { $helperHandle = $helper.Handle } catch {
        Record "ABORT: cannot pin quit helper PID=$($helper.Id); outcome inconclusive; no further process actions."
        $_.Exception.Data['HelperUnresolved'] = $true
        throw
    }
    if (-not $helper.WaitForExit(15000)) {
        Record "ABORT: quit helper PID=$($helper.Id) is still running; outcome inconclusive. Deadline forbids later input; no further process actions."
        $failure = [InvalidOperationException]::new('Quit helper timeout; no force stop or further process actions will be used.')
        $failure.Data['HelperUnresolved'] = $true
        throw $failure
    }
    $helper.Refresh()
    [uint32]$helperExitCode = 259
    if (-not [C02HelperExit]::GetExitCodeProcess($helperHandle, [ref]$helperExitCode) -or $helperExitCode -eq 259) {
        Record "ABORT: quit helper PID=$($helper.Id) native exit status unavailable; outcome inconclusive; no further process actions."
        $failure = [InvalidOperationException]::new('Cannot verify quit helper termination.')
        $failure.Data['HelperUnresolved'] = $true
        throw $failure
    }
    Record "Quit helper PID=$($helper.Id) native exit code=$helperExitCode."
    if ($helperExitCode -ne 0) { throw 'Exclusive-console quit helper failed; no force stop will be used.' }
    $remaining = [Math]::Max(0, 30000 - [int]$watch.ElapsedMilliseconds)
    if (-not $Process.WaitForExit($remaining)) { throw 'Server did not exit normally within 30 seconds; no force stop will be used.' }
    Assert-PortFree
    Record "Normal exit confirmed: PID=$($Process.Id)."
}
function Wait-Ready([Diagnostics.Process]$Process, [string]$Image, [string]$Hash, [string]$OutputFile) {
    $ticks = $Process.StartTime.ToUniversalTime().Ticks
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.ElapsedMilliseconds -lt 30000) {
        $Process.Refresh()
        if ($Process.HasExited) { return $false }
        $null = Get-ExactProcess $Process.Id $Image $Hash $ticks
        $listeners = @(Get-GameTcp | Where-Object State -eq Listen)
        if (@($listeners | Where-Object OwningProcess -ne $Process.Id).Count -ne 0) { throw 'Unexpected listener appeared during startup.' }
        $ready = $false
        if (Test-Path -LiteralPath $OutputFile) {
            $stream = $null
            $buffer = $null
            try {
                $stream = [IO.FileStream]::new($OutputFile, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
                $buffer = [IO.MemoryStream]::new()
                $stream.CopyTo($buffer)
                $bytes = $buffer.ToArray()
            } catch {
                Record ('Readiness verification failed; reason=ReadinessLogRead; exceptionType=' + $_.Exception.GetBaseException().GetType().FullName + '.')
                throw
            } finally {
                if ($null -ne $buffer) { $buffer.Dispose() }
                if ($null -ne $stream) { $stream.Dispose() }
            }
            $ready = [Text.Encoding]::UTF8.GetString($bytes).Contains('서버 준비 완료') -or [Text.Encoding]::GetEncoding(949).GetString($bytes).Contains('서버 준비 완료')
        }
        if ($listeners.Count -gt 0 -and $ready) { Assert-NoClients; return $true }
        Start-Sleep -Milliseconds 250
    }
    return $false
}
function Start-Fixed([string]$Image, [string]$Hash, [string]$WorkingRoot, [string]$Label) {
    Assert-Image $Image $Hash
    Assert-NoTestFlags
    Assert-PortFree
    $out = Join-Path $captures "c02-auth-switch-$stamp-$Label.out.txt"
    $err = Join-Path $captures "c02-auth-switch-$stamp-$Label.err.txt"
    $process = Start-Process -FilePath $Image -WorkingDirectory $WorkingRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    return @{ Process = $process; Output = $out }
}

Assert-NoTestFlags
Assert-Image $originalExe $originalHash
Assert-Image $newExe $newHash
$original = Get-ExactProcess 28924 $originalExe $originalHash
Assert-NoClients
Assert-Listener $original.Id
New-Item -ItemType Directory -Path $captures -Force | Out-Null
Record 'C02-AUTH-DEPLOY-01/r1; approved human reply ts1792304457.908929. No DB/dependency/source changes.'
try {
    Stop-Normally $original 'Original' $originalExe $originalHash
} catch {
    if ($_.Exception.Data['HelperUnresolved']) { throw }
    $original.Refresh()
    if ($original.HasExited) {
        Assert-PortFree
        Record 'ABORT: original exited but normal-stop verification failed; restoring original only.'
        $restored = Start-Fixed $originalExe $originalHash $originalRoot 'rollback-stop'
        if (-not (Wait-Ready $restored.Process $originalExe $originalHash $restored.Output)) { throw 'Original recovery readiness failed; no duplicate start was attempted.' }
        Record "ROLLED BACK: original server ready on 9999; PID=$($restored.Process.Id). New image was not started."
        throw 'Stop verification failed; original server was restored. Review the local receipt.'
    }
    Record 'ABORT: original has not exited; no replacement or force stop attempted.'
    throw
}
$newProcess = $null
try {
    $started = Start-Fixed $newExe $newHash $newRoot 'new'
    $newProcess = $started.Process
    Record "Started fixed new image: PID=$($newProcess.Id), SHA256=$newHash."
    if (-not (Wait-Ready $newProcess $newExe $newHash $started.Output)) { throw 'New server readiness failed.' }
    Record "SUCCESS: new server ready on 9999; PID=$($newProcess.Id). Original image remains untouched."
} catch {
    Record ('New startup/readiness verification failed; reason=StartupOrReadinessCheck; exceptionType=' + $_.Exception.GetBaseException().GetType().FullName + '; attempting only approved normal-stop/known-original rollback.')
    if ($null -ne $newProcess) {
        $newProcess.Refresh()
        if (-not $newProcess.HasExited) {
            try { Stop-Normally $newProcess 'New' $newExe $newHash } catch {
                if ($_.Exception.Data['HelperUnresolved']) { throw }
                $newProcess.Refresh()
                if (-not $newProcess.HasExited) { throw }
                Record 'New process already exited despite stop verification failure; continuing approved original rollback.'
            }
        }
    }
    Assert-PortFree
    $restored = Start-Fixed $originalExe $originalHash $originalRoot 'rollback'
    if (-not (Wait-Ready $restored.Process $originalExe $originalHash $restored.Output)) { throw 'Original rollback readiness failed. No force stop or duplicate start was attempted.' }
    Record "ROLLED BACK: original server ready on 9999; PID=$($restored.Process.Id)."
    throw 'New server not applied; original server was restored. Review the local receipt and startup logs.'
}
