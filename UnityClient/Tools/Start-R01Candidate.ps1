param(
    [switch]$PreflightOnly,
    [switch]$C07Candidate,
    [ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$C07Hash,
    [ValidateSet('R01-START-05/r1')][string]$ExecuteApprovedStart
)
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\joeun\Desktop\workspace\Game-UnityClient'
$image = Join-Path $root 'UnityClient\Builds\R01-Candidate\bin\MainServer.exe'
$imageHash = '01C6261AB5C3448E2BA5F336CE70FB20E4DBA4B9ED47AD70002F22C88594D108'
if ($C07Candidate) {
    if (-not $C07Hash -or $ExecuteApprovedStart) { throw 'C07 needs its reviewed SHA256 and uses current local-operation authorization.' }
    $image = Join-Path $root 'UnityClient\Builds\C07-Candidate\final\bin\MainServer.exe'
    $imageHash = $C07Hash.ToUpperInvariant()
} elseif ($C07Hash) { throw 'C07 hash requires C07 mode.' }
$inspector = Join-Path $root 'UnityClient\Tools\Inspect-R01Persistence.cjs'
$captures = Join-Path $root 'UnityClient\Captures'
$dllHashes = @{
    'event_core.dll'='028EF81FD65415BC48F5E2AB0AC073CC80E4F0496CFEF8F5CBB3380672710B17'
    'event_extra.dll'='400755B1387783C49DAA61192069947355BB9AF2A252AF1D6B919A845CBB5780'
    'event.dll'='FBA74431B5441EB7E5A4F48DEE890796A331E923FCFDE003EC71C8CAAF618FD0'
    'hiredis.dll'='FC0D6F13F43ADE48A71F9FF3AFD9B9B0751165FC325D3A05D19A01999D29FE5C'
    'mysqlcppconn-9-vs14.dll'='2070CD998CDF625425DE4AA5476F131D92165068998CC634B13C4D3138E0F088'
}
if (-not $C07Candidate -and $PreflightOnly -eq ($ExecuteApprovedStart -eq 'R01-START-05/r1')) {
    throw 'Choose exactly one mode: -PreflightOnly, or the exact approved start identifier.'
}
if (-not $C07Candidate -and -not $PreflightOnly -and $ExecuteApprovedStart -ne 'R01-START-05/r1') {
    throw 'Starting the candidate requires the separately approved R01-START-05/r1 identifier.'
}

$stamp = (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $PID
$evidence = Join-Path $captures "r01-start-$stamp-evidence"
$receipt = Join-Path $captures "r01-start-$stamp.txt"
New-Item -ItemType Directory -Path $evidence -ErrorAction Stop | Out-Null
function Record([string]$Text) {
    $line = (Get-Date -Format o) + ' ' + $Text
    Add-Content -LiteralPath $receipt -Value $line -Encoding UTF8
    Write-Host $line
}
function Assert-CandidateFiles {
    if ((Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash -ne $imageHash) { throw 'Candidate image SHA256 mismatch.' }
    foreach ($item in $dllHashes.GetEnumerator()) {
        $path = Join-Path ([IO.Path]::GetDirectoryName($image)) $item.Key
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.Value) { throw 'Candidate DLL SHA256 mismatch.' }
    }
    if ((Get-FileHash -LiteralPath $inspector -Algorithm SHA256).Hash -ne '170D9921CAE996F267F02BBCA4F441D8369CFBD339BD46A649A8B801A05D9C0D') { throw 'Read-only inspector SHA256 mismatch.' }
}
function Assert-TestFlagsUnset {
    foreach ($scope in @('Process','User','Machine')) {
        foreach ($entry in [Environment]::GetEnvironmentVariables($scope).GetEnumerator()) {
            if ($entry.Key -like 'TEST*' -and -not [string]::IsNullOrEmpty([string]$entry.Value)) { throw 'A TEST* environment variable is set.' }
        }
    }
}
function Get-PortState {
    @(Get-NetTCPConnection -ErrorAction Stop | Where-Object LocalPort -eq 9999)
}
function Assert-ServerAbsent {
    $servers = @(Get-CimInstance Win32_Process -Filter "Name = 'MainServer.exe'" -ErrorAction Stop)
    if ($servers.Count -ne 0) { throw 'A MainServer.exe process exists; no start is permitted.' }
    if ((Get-PortState).Count -ne 0) { throw 'Port 9999 is not completely unused.' }
}
function Save-ReadOnlySnapshot([string]$Label) {
    $result = & 'C:\Program Files\nodejs\node.exe' $inspector 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Read-only persistence snapshot failed.' }
    $raw = $result -join [Environment]::NewLine
    $parsed = $raw | ConvertFrom-Json -ErrorAction Stop
    $ids = @($parsed.characters | ForEach-Object { [string]$_.char_id })
    if ($parsed.mode -ne 'READ ONLY' -or ($ids -join ',') -ne '5011,5012') { throw 'Unexpected database inspection scope.' }
    $path = Join-Path $evidence "$Label-persistence.txt"
    [IO.File]::WriteAllText($path, $raw, [Text.UTF8Encoding]::new($false))
    return $path
}
function Assert-SnapshotsStable([string]$First, [string]$Second) {
    $a = (Get-Content -LiteralPath $First -Raw | ConvertFrom-Json).characters | ConvertTo-Json -Depth 6 -Compress
    $b = (Get-Content -LiteralPath $Second -Raw | ConvertFrom-Json).characters | ConvertTo-Json -Depth 6 -Compress
    if ($a -cne $b) { throw 'The two read-only character snapshots differ.' }
}
function Preserve-LogsAndWal {
    foreach ($name in @('system','game','error','perf')) {
        Copy-Item -LiteralPath (Join-Path $root "logs\$name.log") -Destination (Join-Path $evidence "before-start-$name.log") -ErrorAction Stop
        $sourceHash = (Get-FileHash -LiteralPath (Join-Path $root "logs\$name.log") -Algorithm SHA256).Hash
        if ($sourceHash -ne (Get-FileHash -LiteralPath (Join-Path $evidence "before-start-$name.log") -Algorithm SHA256).Hash) { throw 'Log evidence copy mismatch.' }
        Record "Preserved log $name SHA256=$sourceHash"
    }
    $segments = @(Get-ChildItem -LiteralPath $root -File -ErrorAction Stop | Where-Object Name -Match '^(redo|backpressure)\.[0-9]+$')
    foreach ($segment in $segments) {
        if ($segment.Length -ne 0) { throw 'A numeric redo/backpressure WAL segment is nonempty; start blocked.' }
        Copy-Item -LiteralPath $segment.FullName -Destination (Join-Path $evidence "before-start-$($segment.Name)") -ErrorAction Stop
        if ((Get-FileHash -LiteralPath $segment.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $evidence "before-start-$($segment.Name)") -Algorithm SHA256).Hash) { throw 'WAL evidence copy mismatch.' }
    }
    Record "Preserved four logs and $($segments.Count) empty numeric WAL files with verified copies."
}
function Assert-PreservedWal {
    $current = @(Get-ChildItem -LiteralPath $root -File -ErrorAction Stop | Where-Object Name -Match '^(redo|backpressure)\.[0-9]+$' | Sort-Object Name)
    $saved = @(Get-ChildItem -LiteralPath $evidence -File -ErrorAction Stop | Where-Object Name -Match '^before-start-(redo|backpressure)\.[0-9]+$' | Sort-Object Name)
    if ($current.Count -ne $saved.Count) { throw 'Numeric WAL file set changed after preservation.' }
    for ($i = 0; $i -lt $current.Count; $i++) {
        if ($saved[$i].Name -cne ('before-start-' + $current[$i].Name) -or $current[$i].Length -ne 0 -or (Get-FileHash -LiteralPath $current[$i].FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $saved[$i].FullName -Algorithm SHA256).Hash) { throw 'WAL changed after preservation; startup blocked.' }
    }
}
function Assert-ExactProcess([Diagnostics.Process]$Process) {
    $info = Get-CimInstance Win32_Process -Filter "ProcessId = $($Process.Id)" -ErrorAction Stop
    if ($null -eq $info -or $info.ExecutablePath -ine $image -or $info.CommandLine -notmatch ('^\s*"?' + [regex]::Escape($image) + '"?\s*$')) { throw 'Started process path or arguments differ from the fixed candidate.' }
    if ($Process.StartTime.ToUniversalTime().Ticks -ne $script:startedTicks) { throw 'Started process identity changed.' }
    if ((Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash -ne $imageHash) { throw 'Candidate image changed after spawn.' }
    foreach ($item in $dllHashes.GetEnumerator()) {
        if ((Get-FileHash -LiteralPath (Join-Path ([IO.Path]::GetDirectoryName($image)) $item.Key) -Algorithm SHA256).Hash -ne $item.Value) { throw 'Candidate DLL changed after spawn.' }
    }
}
function Assert-LoadedDlls([Diagnostics.Process]$Process) {
    $modules = @($Process.Modules)
    foreach ($name in @('event.dll','hiredis.dll','mysqlcppconn-9-vs14.dll')) {
        $found = @($modules | Where-Object ModuleName -IEQ $name)
        $expected = Join-Path ([IO.Path]::GetDirectoryName($image)) $name
        if ($found.Count -ne 1 -or $found[0].FileName -ine $expected -or (Get-FileHash -LiteralPath $found[0].FileName -Algorithm SHA256).Hash -ne $dllHashes[$name]) { throw 'Loaded candidate dependency path/hash mismatch.' }
    }
}
function Test-Ready([Diagnostics.Process]$Process, [string]$Output) {
    $timer = $script:startupTimer
    while ($timer.Elapsed.TotalSeconds -lt 30) {
        $Process.Refresh()
        if ($Process.HasExited) { return $false }
        Assert-ExactProcess $Process
        $connections = Get-PortState
        $listeners = @($connections | Where-Object State -eq Listen)
        if (@($listeners | Where-Object OwningProcess -ne $Process.Id).Count -ne 0 -or @($connections | Where-Object State -eq Established).Count -ne 0) { throw 'Unexpected listener or client connection during startup.' }
        if ($listeners.Count -eq 1 -and (Test-Path -LiteralPath $Output)) {
            $stream = [IO.FileStream]::new($Output, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
            $buffer = [IO.MemoryStream]::new()
            try { $stream.CopyTo($buffer); $bytes = $buffer.ToArray() } finally { $buffer.Dispose(); $stream.Dispose() }
            if ([Text.Encoding]::UTF8.GetString($bytes).Contains('서버 준비 완료') -or [Text.Encoding]::GetEncoding(949).GetString($bytes).Contains('서버 준비 완료')) {
                Assert-LoadedDlls $Process
                if ((Get-Item -LiteralPath (Join-Path $root 'logs\error.log')).Length -ne 0) { throw 'Candidate error.log is nonempty.' }
                if ((Get-Item -LiteralPath $script:errorOutput).Length -ne 0) { throw 'Candidate stderr is nonempty.' }
                if (@(Select-String -LiteralPath (Join-Path $root 'logs\game.log') -SimpleMatch 'character state update failed' -ErrorAction Stop).Count -ne 0) { throw 'Candidate game log reports a persistence failure.' }
                if ($timer.Elapsed.TotalSeconds -ge 30) { throw 'Startup deadline elapsed during readiness verification.' }
                return $true
            }
        }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

try {
    Assert-CandidateFiles
    Assert-TestFlagsUnset
    Assert-ServerAbsent
    Preserve-LogsAndWal
    $first = Save-ReadOnlySnapshot 'before-start-first'
    $second = Save-ReadOnlySnapshot 'before-start-second'
    Assert-SnapshotsStable $first $second
    Assert-CandidateFiles
    Assert-TestFlagsUnset
    Assert-PreservedWal
    Assert-ServerAbsent
    Record 'Preflight PASS: fixed candidate/DLL hashes, no MainServer or port 9999 use, TEST* unset, logs/WAL preserved, two READ ONLY snapshots stable.'
    if ($PreflightOnly) {
        Record 'PreflightOnly: evidence preparation ended; no process was started and no database was written.'
        exit 0
    }

    $output = Join-Path $evidence 'candidate.stdout.txt'
    $errorOutput = Join-Path $evidence 'candidate.stderr.txt'
    $script:startupTimer = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $image -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardOutput $output -RedirectStandardError $errorOutput
    $handle = $process.Handle
    $script:startedTicks = $process.StartTime.ToUniversalTime().Ticks
    Record "StartProcess returned PID=$($process.Id); native handle pinned; StartUtcTicks=$script:startedTicks."
    Assert-ExactProcess $process
    if (-not (Test-Ready $process $output)) {
        Record "START BLOCKED: readiness was not confirmed in 30 seconds; no retry, q, kill, or recovery was attempted. PID=$($process.Id)."
        throw 'Candidate did not reach verified readiness; inspect the saved evidence and actual process state.'
    }
    Record "START PASS: candidate PID=$($process.Id), image hash fixed, port 9999 ready, loaded DLLs verified, error.log empty, established clients=0."
} catch {
    Record ('STOP: ' + $_.Exception.Message)
    throw
}
