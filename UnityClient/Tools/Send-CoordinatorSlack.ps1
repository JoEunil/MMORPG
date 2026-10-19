param(
    [Parameter(Mandatory=$true)][string]$MessageFile,
    [ValidateSet('C0C9VFZMV43','C0CA7BSKN3B')][string]$ChannelId = 'C0C9VFZMV43',
    [string]$ThreadTs,
    [string]$UpdateTs
)
$ErrorActionPreference = 'Stop'
$taskBstr = [IntPtr]::Zero
$taskTokens = $null
$taskHeaders = $null
$taskStage = 'load'
$taskSafeError = ''
try {
    if ($ThreadTs -and $ThreadTs -notmatch '^\d+\.\d+$') { throw 'Invalid thread timestamp.' }
    if ($UpdateTs -and ($UpdateTs -notmatch '^\d+\.\d+$' -or $ThreadTs)) { throw 'Invalid update timestamp.' }
    $taskText = [IO.File]::ReadAllText([IO.Path]::GetFullPath($MessageFile), [Text.Encoding]::UTF8)
    if ([string]::IsNullOrWhiteSpace($taskText) -or $taskText.Length -gt 12000) { throw 'Invalid message length.' }
    $taskTokenPath = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'UnityCoordinator/slack-tokens.xml'
    $taskTokens = Import-Clixml -LiteralPath $taskTokenPath
    $taskBstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($taskTokens.BotToken)
    $taskBotToken = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($taskBstr)
    $taskHeaders = @{ Authorization = 'Bearer ' + $taskBotToken }
    $taskStage = 'auth'
    $taskAuth = Invoke-RestMethod -Method Post -Uri 'https://slack.com/api/auth.test' -Headers $taskHeaders -ContentType 'application/json; charset=utf-8' -Body '{}' -TimeoutSec 20
    if (!$taskAuth.ok -or $taskAuth.team_id -ne 'T0C9ECC3GS3' -or $taskAuth.user_id -ne 'U0C9ZRTQ6F8') {
        throw 'Expected Unity Coordinator bot identity was not verified.'
    }
    if ($ChannelId -eq 'C0CA7BSKN3B' -and ($ThreadTs -or $UpdateTs)) { throw 'Completion channel accepts new summaries only.' }
    $taskBody = @{ channel=$ChannelId; text=$taskText; unfurl_links=$false; unfurl_media=$false }
    if ($ThreadTs) { $taskBody.thread_ts = $ThreadTs }
    $taskMethod = 'chat.postMessage'
    if ($UpdateTs) { $taskBody.ts = $UpdateTs; $taskMethod = 'chat.update' }
    $taskJson = $taskBody | ConvertTo-Json -Compress
    $taskStage = $taskMethod
    $taskPosted = Invoke-RestMethod -Method Post -Uri ('https://slack.com/api/' + $taskMethod) -Headers $taskHeaders -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($taskJson)) -TimeoutSec 20
    if (!$taskPosted.ok) {
        if ([string]$taskPosted.error -match '^[a-z_]+$') { $taskSafeError = [string]$taskPosted.error }
        throw 'Slack post failed; inspect before retrying an unknown outcome.'
    }
    [pscustomobject]@{ SenderUserID=$taskAuth.user_id; Channel=$taskPosted.channel; Ts=$taskPosted.ts;
        Link=('https://aiagent-vqu1541.slack.com/archives/' + $taskPosted.channel + '/p' + $taskPosted.ts.Replace('.','')) }
} catch {
    # Do not print HTTP exceptions, response bodies, headers, or credentials.
    Write-Error ('Coordinator bot send failed or outcome unknown. Stage=' + $taskStage + '; API code=' + $taskSafeError + '. Read the channel before retrying; no tokens were logged.') -ErrorAction Continue
    exit 1
} finally {
    if ($taskBstr -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($taskBstr) }
    $taskBotToken = $null
    if ($taskHeaders) { $taskHeaders.Clear() }
    if ($taskTokens -and $taskTokens.BotToken) { $taskTokens.BotToken.Dispose() }
    if ($taskTokens -and $taskTokens.AppToken) { $taskTokens.AppToken.Dispose() }
    $taskTokens = $null
}
