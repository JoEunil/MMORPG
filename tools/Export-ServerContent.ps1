[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repository 'UnityClient/Builds/ContentTransfer'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'Installed Visual Studio vswhere.exe was not found.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or !$installation) { throw 'Installed MSVC C++ tools were not found.' }
$vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvars64.bat'
if (!(Test-Path -LiteralPath $vcvars -PathType Leaf)) { throw 'Installed vcvars64.bat was not found.' }

# Import only the build environment, then invoke cl directly with explicit paths.
$environment = & $env:ComSpec /d /c "call `"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'vcvars64.bat failed.' }
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
$compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
if (!(Test-Path -LiteralPath $compiler -PathType Leaf)) { throw 'The configured cl.exe was not found.' }
$null = New-Item -ItemType Directory -Path $buildDirectory -Force
$executable = Join-Path $buildDirectory 'Export-ServerContent.exe'
$arguments = @('/nologo', '/Y-', '/std:c++20', '/EHsc', '/W4', '/utf-8',
    ('/I' + (Join-Path $repository 'CoreLib')),
    ('/Fo' + $buildDirectory + '\'), ('/Fe' + $executable),
    (Join-Path $PSScriptRoot 'Export-ServerContent.cpp'),
    (Join-Path $repository 'CoreLib/SkillData.cpp'))
& $compiler @arguments
if ($LASTEXITCODE -ne 0) { throw 'Server content exporter compilation failed.' }
$resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
$null = New-Item -ItemType Directory -Path (Split-Path -Parent $resolvedOutput) -Force
& $executable $resolvedOutput
if ($LASTEXITCODE -ne 0) { throw 'Server content export failed.' }
