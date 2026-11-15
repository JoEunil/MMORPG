# Downloads only the native analysis library and matching Clang resource headers.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$runtimeDir = Join-Path $PSScriptRoot 'obj/clang'
$resourceHeaders = Join-Path $runtimeDir 'resources/include'
New-Item -ItemType Directory -Force -Path $runtimeDir, $resourceHeaders | Out-Null

function Get-ClangArchive([string]$Name, [string]$Url, [string]$Sha256) {
    $archive = Join-Path $runtimeDir $Name
    if (!(Test-Path -LiteralPath $archive) -or (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $Sha256) {
        $partial = "$archive.partial"
        Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $partial
        if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ne $Sha256) {
            throw "SHA-256 mismatch: $Name"
        }
        Move-Item -LiteralPath $partial -Destination $archive -Force
    }
    return $archive
}

$version = '21.1.8'
$packageUrl = "https://api.nuget.org/v3-flatcontainer/libclang.runtime.win-x64/$version/libclang.runtime.win-x64.$version.nupkg"
$sourceUrl = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$version/clang-$version.src.tar.xz"
$package = Get-ClangArchive "libclang.runtime.win-x64.$version.nupkg" $packageUrl '1296AA72D506A3511E3F509F4966365133AF9C935D301A63EC2242BD8C3180CE'
$source = Get-ClangArchive "clang-$version.src.tar.xz" $sourceUrl '6090E3F23720D003CDD84483A47D0EEC6D01ADBB5E0C714AC0C8B58DE546AA62'

tar.exe -xf $source -C $resourceHeaders --strip-components 3 "clang-$version.src/lib/Headers"
if ($LASTEXITCODE -ne 0) { throw 'Could not extract the Clang resource headers.' }
tar.exe -xf $package -C $runtimeDir LICENSE.TXT
if ($LASTEXITCODE -ne 0) { throw 'Could not extract the LLVM license.' }
Move-Item -LiteralPath (Join-Path $runtimeDir 'LICENSE.TXT') -Destination (Join-Path $runtimeDir 'LLVM-LICENSE.txt') -Force
tar.exe -xf $package -C $runtimeDir --strip-components 3 runtimes/win-x64/native/libclang.dll
if ($LASTEXITCODE -ne 0) { throw 'Could not extract libclang.dll.' }

Write-Output "Clang $version prepared in $runtimeDir"
