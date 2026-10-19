param(
    [Parameter(Mandatory = $true)][string]$Coordinator,
    [Parameter(Mandatory = $true)][string]$CodexPath,
    [switch]$TestReconnect
)
$ErrorActionPreference = 'Stop'
if ($Coordinator -ne '01a14d46-8b23-7590-8ad5-7efc20f8ecd9') { throw 'Unexpected active client Coordinator.' }
# One receiver per Windows session. The OS releases this mutex after a crash.
$mutex = [System.Threading.Mutex]::new($false, 'Local\UnityCoordinatorSlackReceiver')
$owned = $false
$tokens = $null
try {
    try { $owned = $mutex.WaitOne(0) }
    catch [System.Threading.AbandonedMutexException] { $owned = $true }
    if (-not $owned) { throw 'A Slack receiver is already running.' }
    if (-not (Test-Path -LiteralPath $CodexPath -PathType Leaf)) { throw 'Codex executable not found.' }
    $tokenPath = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'UnityCoordinator/slack-tokens.xml'
    $tokens = Import-Clixml -LiteralPath $tokenPath
    $env:UNITY_SLACK_APP_TOKEN = [System.Net.NetworkCredential]::new('', $tokens.AppToken).Password
    $env:UNITY_SLACK_BOT_TOKEN = [System.Net.NetworkCredential]::new('', $tokens.BotToken).Password
    $receiverArgs = @((Join-Path $PSScriptRoot 'slack-receiver.mjs'), $Coordinator, $CodexPath)
    if ($TestReconnect) { $receiverArgs += '--test-reconnect' }
    & node @receiverArgs
    if ($LASTEXITCODE -ne 0) { throw 'Slack receiver stopped with an error.' }
}
finally {
    Remove-Item Env:UNITY_SLACK_APP_TOKEN, Env:UNITY_SLACK_BOT_TOKEN -ErrorAction SilentlyContinue
    if ($tokens) { $tokens.AppToken.Dispose(); $tokens.BotToken.Dispose() }
    if ($owned) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
