param(
    [switch]$PreflightOnly,
    [int]$QuitHelperPid = 0,
    [ValidateSet('Original', 'New')][string]$QuitImage = 'Original',
    [long]$QuitStartTicks = 0,
    [long]$QuitDeadlineUtcTicks = 0
)
$ErrorActionPreference = 'Stop'
$originalExe = 'C:\Users\joeun\Desktop\workspace\Game-UnityClient\x64\Release\MainServer.exe'
$originalRoot = 'C:\Users\joeun\Desktop\workspace\Game-UnityClient'
$originalHash = 'C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825'
$newExe = 'C:\Users\joeun\Desktop\workspace\Game-UnityClient\UnityClient\Builds\R01-Candidate\bin\MainServer.exe'
$newRoot = 'C:\Users\joeun\Desktop\workspace\Game-UnityClient'
$newHash = '01C6261AB5C3448E2BA5F336CE70FB20E4DBA4B9ED47AD70002F22C88594D108'
$captures = Join-Path $newRoot 'UnityClient\Captures'
$requiredDlls = @{
    'event_core.dll'='028EF81FD65415BC48F5E2AB0AC073CC80E4F0496CFEF8F5CBB3380672710B17'
    'event_extra.dll'='400755B1387783C49DAA61192069947355BB9AF2A252AF1D6B919A845CBB5780'
    'event.dll'='FBA74431B5441EB7E5A4F48DEE890796A331E923FCFDE003EC71C8CAAF618FD0'
    'hiredis.dll'='FC0D6F13F43ADE48A71F9FF3AFD9B9B0751165FC325D3A05D19A01999D29FE5C'
    'mysqlcppconn-9-vs14.dll'='2070CD998CDF625425DE4AA5476F131D92165068998CC634B13C4D3138E0F088'
}

function Assert-Image([string]$Image, [string]$Hash) {
    if ((Get-FileHash -LiteralPath $Image -Algorithm SHA256).Hash -ne $Hash) { throw 'Fixed server image hash mismatch.' }
    foreach ($item in $requiredDlls.GetEnumerator()) {
        $dependency = Join-Path ([IO.Path]::GetDirectoryName($Image)) $item.Key
        if ((Get-FileHash -LiteralPath $dependency -Algorithm SHA256).Hash -ne $item.Value) { throw 'Fixed server dependency hash mismatch.' }
    }
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
    if ($PreflightOnly) { throw 'Preflight cannot request console input.' }
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
public static class R01ConsoleQuit {
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
    [R01ConsoleQuit]::Send([uint32]$QuitHelperPid, $QuitDeadlineUtcTicks)
    exit 0
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R01HelperExit {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetExitCodeProcess(IntPtr process, out uint code);
}
'@

$stamp = (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $PID
$receipt = Join-Path $captures "r01-switch-$stamp.txt"
$evidenceFolder = Join-Path $captures "r01-switch-$stamp-evidence"
function Record([string]$Message) {
    $line = (Get-Date -Format o) + ' ' + $Message
    Add-Content -LiteralPath $receipt -Value $line -Encoding UTF8
    Write-Host $line
}
function Get-WalSegments {
    Get-ChildItem -LiteralPath $newRoot -File -ErrorAction Stop | Where-Object { $_.Name -cmatch '^(redo|backpressure)\.[0-9]+$' }
}
function Assert-EmptyWal {
    foreach ($file in @(Get-WalSegments)) {
        if ($file.Length -ne 0) { throw 'Nonempty existing WAL requires review; no startup replay is authorized by this tool.' }
    }
}
function Preserve-LogsAndWal([string]$Label) {
    if ($Label -notmatch '^[a-z0-9-]+$') { throw 'Invalid evidence label.' }
    foreach ($name in @('system','game','error','perf')) {
        $source = Join-Path $newRoot "logs\$name.log"
        $target = Join-Path $evidenceFolder "$Label-$name.log"
        Copy-Item -LiteralPath $source -Destination $target -ErrorAction Stop
    }
    foreach ($file in @(Get-WalSegments)) {
        try {
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $evidenceFolder "$Label-$($file.Name)") -ErrorAction Stop
        } catch [IO.IOException] {
            $observed = Get-Item -LiteralPath $file.FullName -ErrorAction Stop
            $nativeError = $_.Exception.HResult -band 0xFFFF
            if ($Label -ne 'before-current-stop' -or $observed.Length -ne 0 -or $nativeError -notin @(32,33)) { throw }
            # The running server opens active WAL exclusively. Do not fabricate a byte copy.
            @("Source=$($observed.FullName)", 'ObservedLength=0', "LastWriteUtc=$($observed.LastWriteTimeUtc.ToString('o'))", 'ByteCopy=not taken; active handle locked; mandatory copy after exit') |
                Set-Content -LiteralPath (Join-Path $evidenceFolder "$Label-$($file.Name)-metadata.txt") -Encoding UTF8
        }
    }
    Record "Preserved logs/WAL evidence: $Label. Locked active empty WAL uses metadata only before q; post-exit copies are mandatory."
}
function Save-PersistenceSnapshot([string]$Label) {
    if ($Label -notmatch '^[a-z0-9-]+$') { throw 'Invalid snapshot label.' }
    $inspector = Join-Path $newRoot 'UnityClient\Tools\Inspect-R01Persistence.cjs'
    if ((Get-FileHash -LiteralPath $inspector -Algorithm SHA256).Hash -ne '170D9921CAE996F267F02BBCA4F441D8369CFBD339BD46A649A8B801A05D9C0D') { throw 'Read-only inspector hash mismatch.' }
    $snapshotText = & 'C:\Program Files\nodejs\node.exe' $inspector 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Read-only persistence inspection failed; no further process action.' }
    $snapshotText = $snapshotText -join [Environment]::NewLine
    $snapshot = $snapshotText | ConvertFrom-Json -ErrorAction Stop
    $ids = @($snapshot.characters | ForEach-Object { [string]$_.char_id })
    if ($snapshot.mode -ne 'READ ONLY' -or $ids.Count -ne 2 -or ($ids -join ',') -ne '5011,5012') { throw 'Unexpected read-only snapshot scope.' }
    [IO.File]::WriteAllText((Join-Path $evidenceFolder "$Label-persistence.txt"), $snapshotText, [Text.UTF8Encoding]::new($false))
    Record "Recorded dedicated characters read-only snapshot: $Label. Values are evidence, not a claim of SQL update success."
    return (Join-Path $evidenceFolder "$Label-persistence.txt")
}
function Assert-LoadedDependencies([Diagnostics.Process]$Process, [string]$Image) {
    $modules = @($Process.Modules)
    $manifest = @()
    foreach ($name in @('event.dll','hiredis.dll','mysqlcppconn-9-vs14.dll')) {
        $found = @($modules | Where-Object { $_.ModuleName -ieq $name })
        $expected = Join-Path ([IO.Path]::GetDirectoryName($Image)) $name
        if ($found.Count -ne 1 -or $found[0].FileName -ine $expected -or
            (Get-FileHash -LiteralPath $found[0].FileName -Algorithm SHA256).Hash -ne $requiredDlls[$name]) { throw 'Loaded runtime dependency path/hash mismatch.' }
        $manifest += $name + ' ' + $found[0].FileName + ' ' + $requiredDlls[$name]
    }
    $manifest | Set-Content -LiteralPath (Join-Path $evidenceFolder "loaded-$($Process.Id).txt") -Encoding UTF8
    Record "Verified loaded project DLL paths/disk hashes for PID=$($Process.Id); not an in-memory hash claim."
}
function Assert-StablePersistence([string]$AfterStop, [string]$BeforeStart) {
    $first = (Get-Content -LiteralPath $AfterStop -Raw | ConvertFrom-Json).characters | ConvertTo-Json -Depth 6 -Compress
    $second = (Get-Content -LiteralPath $BeforeStart -Raw | ConvertFrom-Json).characters | ConvertTo-Json -Depth 6 -Compress
    if ($first -cne $second) { throw 'Dedicated character values changed after stop; no new process will be started.' }
}
function Get-CharacterUpdateFailureCount {
    $events = @(Get-Content -LiteralPath (Join-Path $newRoot 'logs\game.log') -Encoding UTF8 | ForEach-Object { $_ | ConvertFrom-Json -ErrorAction Stop })
    return @($events | Where-Object { $_.event -eq 'cache handler' -and $_.msg -eq 'character state update failed' }).Count
}
function Assert-StopEvidence([long]$ErrorLength, [int]$UpdateFailureCount) {
    $errorPath = Join-Path $newRoot 'logs\error.log'
    if ((Get-Item -LiteralPath $errorPath).Length -ne $ErrorLength) { throw 'Shutdown error-log delta requires review; no startup will follow.' }
    if ((Get-CharacterUpdateFailureCount) -ne $UpdateFailureCount) { throw 'Character-state update failure requires review; no startup will follow.' }
    $events = @(Get-Content -LiteralPath (Join-Path $newRoot 'logs\system.log') -Encoding UTF8 | ForEach-Object { $_ | ConvertFrom-Json -ErrorAction Stop })
    $cacheStops = @($events | Where-Object { $_.event -eq 'cache mq' -and $_.msg -eq 'mq thread stopped' }).Count
    $dbStops = @($events | Where-Object { $_.event -eq 'DB worker thread' -and $_.msg -eq 'DB worker thread stopped' }).Count
    if ($cacheStops -ne 3 -or $dbStops -ne 4) { throw 'Expected cache MQ/DB worker drain termination evidence missing; no startup will follow.' }
    Record 'Observed cache MQ stops=3/DB worker stops=4, no new shutdown error-log bytes/character-state update failures. This is not a per-row SQL success receipt.'
}
function Stop-Normally([Diagnostics.Process]$Process, [string]$ImageKind, [string]$Image, [string]$Hash) {
    $ticks = $Process.StartTime.ToUniversalTime().Ticks
    $null = Get-ExactProcess $Process.Id $Image $Hash $ticks
    Assert-NoClients
    $serverHandle = $Process.Handle
    $errorLength = (Get-Item -LiteralPath (Join-Path $newRoot 'logs\error.log')).Length
    $updateFailureCount = Get-CharacterUpdateFailureCount
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $helperOut = Join-Path $captures "r01-switch-$stamp-quit-$($Process.Id).out.txt"
    $helperErr = Join-Path $captures "r01-switch-$stamp-quit-$($Process.Id).err.txt"
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
    if (-not [R01HelperExit]::GetExitCodeProcess($helperHandle, [ref]$helperExitCode) -or $helperExitCode -eq 259) {
        Record "ABORT: quit helper PID=$($helper.Id) native exit status unavailable; outcome inconclusive; no further process actions."
        $failure = [InvalidOperationException]::new('Cannot verify quit helper termination.')
        $failure.Data['HelperUnresolved'] = $true
        throw $failure
    }
    Record "Quit helper PID=$($helper.Id) native exit code=$helperExitCode."
    if ($helperExitCode -ne 0) { throw 'Exclusive-console quit helper failed; no force stop will be used.' }
    $remaining = [Math]::Max(0, 30000 - [int]$watch.ElapsedMilliseconds)
    if (-not $Process.WaitForExit($remaining)) { throw 'Server did not exit normally within 30 seconds; no force stop will be used.' }
    [uint32]$serverExitCode = 259
    if (-not [R01HelperExit]::GetExitCodeProcess($serverHandle, [ref]$serverExitCode) -or $serverExitCode -ne 0) { throw 'Server native exit was not confirmed zero; no startup will follow.' }
    Assert-PortFree
    Preserve-LogsAndWal ("stopped-" + $ImageKind)
    $script:lastStoppedSnapshot = Save-PersistenceSnapshot ("post-stop-" + $ImageKind)
    Assert-StopEvidence $errorLength $updateFailureCount
    Record "Normal native exit0 confirmed: PID=$($Process.Id)."
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
        if ($listeners.Count -gt 0 -and $ready) {
            Assert-NoClients
            Assert-LoadedDependencies $Process $Image
            if ((Get-Item -LiteralPath (Join-Path $newRoot 'logs\error.log')).Length -ne 0) { throw 'Startup error log requires review.' }
            return $true
        }
        Start-Sleep -Milliseconds 250
    }
    return $false
}
function Start-Fixed([string]$Image, [string]$Hash, [string]$WorkingRoot, [string]$Label) {
    Assert-Image $Image $Hash
    Assert-NoTestFlags
    Assert-PortFree
    Assert-EmptyWal
    Preserve-LogsAndWal ("pre-start-" + $Label)
    $preStart = Save-PersistenceSnapshot ("pre-start-" + $Label)
    Assert-StablePersistence $script:lastStoppedSnapshot $preStart
    if ($Label -eq 'rollback') { Assert-StablePersistence $script:originalStoppedSnapshot $preStart }
    $out = Join-Path $captures "r01-switch-$stamp-$Label.out.txt"
    $err = Join-Path $captures "r01-switch-$stamp-$Label.err.txt"
    $process = Start-Process -FilePath $Image -WorkingDirectory $WorkingRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    Record "Spawned fixed $Label process PID=$($process.Id); pinning handle before further checks."
    $handle = $process.Handle
    return @{ Process = $process; Output = $out; Handle = $handle }
}

Assert-NoTestFlags
Assert-Image $originalExe $originalHash
Assert-Image $newExe $newHash
$original = Get-ExactProcess 20024 $originalExe $originalHash 639279045410504905
Assert-NoClients
Assert-Listener $original.Id
New-Item -ItemType Directory -Path $captures -Force | Out-Null
New-Item -ItemType Directory -Path $evidenceFolder -ErrorAction Stop | Out-Null
Record 'R01-DEPLOY-04/r1; execution requires explicit approval recorded by Coordinator. Normal shutdown may persist dedicated character states; no direct DB writes or data restoration.'
Assert-EmptyWal
Preserve-LogsAndWal 'before-current-stop'
$null = Save-PersistenceSnapshot 'before-current-stop'
Assert-LoadedDependencies $original $originalExe
if ($PreflightOnly) {
    Record 'PREFLIGHT_ONLY: read-only checks/evidence complete; no quit input, server startup, or DB write was performed.'
    exit 0
}
try {
    Stop-Normally $original 'Original' $originalExe $originalHash
    $script:originalStoppedSnapshot = $script:lastStoppedSnapshot
} catch {
    Record 'ABORT: stop, native exit, or shutdown evidence was not confirmed. No candidate/recovery start or force stop will follow.'
    throw
}
$newProcess = $null
$candidateHandle = [IntPtr]::Zero
try {
    $started = Start-Fixed $newExe $newHash $newRoot 'new'
    $newProcess = $started.Process
    $candidateHandle = $started.Handle
    Record "Started fixed new image: PID=$($newProcess.Id), SHA256=$newHash."
    if (-not (Wait-Ready $newProcess $newExe $newHash $started.Output)) { throw 'New server readiness failed.' }
    Record "SUCCESS: new server ready on 9999; PID=$($newProcess.Id). Original image remains untouched."
} catch {
    Record ('New startup/readiness verification failed; reason=StartupOrReadinessCheck; exceptionType=' + $_.Exception.GetBaseException().GetType().FullName + '; evaluating verified conditional recovery only.')
    if ($null -eq $newProcess) {
        Record 'ABORT: candidate was not returned by Start-Process; pre-start guard failure or startup outcome is unresolved. No recovery start will follow.'
        throw
    }
    if ($null -ne $newProcess) {
        $newProcess.Refresh()
        if (-not $newProcess.HasExited) {
            # Any timeout, nonzero exit, or stop-evidence failure blocks recovery too.
            Stop-Normally $newProcess 'New' $newExe $newHash
        } else {
            [uint32]$candidateExitCode = 259
            if ($candidateHandle -eq [IntPtr]::Zero -or
                -not [R01HelperExit]::GetExitCodeProcess($candidateHandle, [ref]$candidateExitCode) -or $candidateExitCode -eq 259) { throw 'Candidate exit outcome is unresolved; no recovery start will follow.' }
            Record "Startup-failed candidate native exit verified: PID=$($newProcess.Id), code=$candidateExitCode."
            Preserve-LogsAndWal 'failed-candidate-exited'
            $script:lastStoppedSnapshot = Save-PersistenceSnapshot 'post-failed-candidate'
            Assert-StablePersistence $script:originalStoppedSnapshot $script:lastStoppedSnapshot
        }
    }
    Assert-PortFree
    $restored = Start-Fixed $originalExe $originalHash $originalRoot 'rollback'
    if (-not (Wait-Ready $restored.Process $originalExe $originalHash $restored.Output)) { throw 'Original rollback readiness failed. No force stop or duplicate start was attempted.' }
    Record "ROLLED BACK: original server ready on 9999; PID=$($restored.Process.Id)."
    throw 'New server not applied; original server was restored. Review the local receipt and startup logs.'
}
