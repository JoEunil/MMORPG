param([ValidateSet('Debug','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$repository = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$project = Join-Path $repository 'ClientCore/ClientCore.csproj'
& dotnet build $project --configuration $Configuration --nologo
if ($LASTEXITCODE -ne 0) { throw 'ClientCore build failed.' }
$target = Join-Path $repository 'UnityClient/Assets/Plugins/ClientCore'
New-Item -ItemType Directory -Force -Path $target | Out-Null
Copy-Item -LiteralPath (Join-Path $repository "ClientCore/bin/$Configuration/netstandard2.0/ClientCore.dll") -Destination $target -Force
$lock = Get-Content -LiteralPath (Join-Path $repository 'UnityClient/Packages/packages-lock.json') -Raw | ConvertFrom-Json
if ($null -eq $lock.dependencies.'com.unity.nuget.newtonsoft-json') {
    throw 'The Unity Newtonsoft.Json package is required; do not add a second Json DLL.'
}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $target 'ClientCore.dll') | Format-Table -AutoSize
Write-Output ('Json dependency: com.unity.nuget.newtonsoft-json ' + $lock.dependencies.'com.unity.nuget.newtonsoft-json'.version)
