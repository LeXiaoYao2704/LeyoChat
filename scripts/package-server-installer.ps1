<#
.SYNOPSIS
    Builds the single-file LeyoChat Server installer.
.DESCRIPTION
    1. Calls package-server-windows.ps1 to generate the inner Inno Setup server installer.
    2. Builds the Qt Quick installer shell and SfxStub.
    3. Bundles the shell and appends the inner server installer as raw payload.
    4. Emits LeyoChatServer-<version>-setup.exe and server-latest.json.
#>
param(
    [string]$BuildDir = 'build-msvc'
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspaceRoot = Split-Path -Parent $scriptDir
. (Join-Path $scriptDir 'package-windows.lib.ps1')

$buildPath = Join-Path $workspaceRoot $BuildDir
$cmakeCache = Join-Path $buildPath 'CMakeCache.txt'

if (!(Test-Path $cmakeCache)) {
    throw "CMake cache not found: $cmakeCache"
}

$cmakeExe = Join-Path $workspaceRoot 'CMake\bin\cmake.exe'
if (!(Test-Path $cmakeExe)) {
    $cmakeExe = 'cmake'
}

$windeployqt = Resolve-WindeployQtPath -CMakeCachePath $cmakeCache
if (-not $windeployqt) {
    throw 'Cannot find windeployqt.exe.'
}

$appVersion = Get-ProjectVersionFromCache -CMakeCachePath $cmakeCache
$installerModeArgs = @('--installer-mode', 'server')

Write-Host '================================================='
Write-Host '  LeyoChatServer Installer (Qt Quick UI + Inno)'
Write-Host "  Version: $appVersion"
Write-Host "  Mode: $($installerModeArgs -join ' ')"
Write-Host '================================================='

Write-Host ''
Write-Host '==> Step 1: Build inner server installer via package-server-windows.ps1'
$serverPackageScript = Join-Path $scriptDir 'package-server-windows.ps1'
if (!(Test-Path $serverPackageScript)) {
    throw "Cannot find server packaging script: $serverPackageScript"
}
& $serverPackageScript -BuildDir $BuildDir
if ($LASTEXITCODE -ne 0) {
    throw 'package-server-windows.ps1 failed.'
}

$innerOutputDir = Join-Path $buildPath 'package\server-installer'
$innerInstaller = Get-ChildItem -Path $innerOutputDir -Filter 'LeyoChatServer-*.exe' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $innerInstaller) {
    throw "Inner server installer exe not found in: $innerOutputDir"
}
Write-Host "    Inner server installer: $($innerInstaller.FullName)"

Write-Host ''
Write-Host '==> Step 2: Build LeyoChatSetup and SfxStub'
& $cmakeExe --build $buildPath --config Release --target LeyoChatSetup SfxStub
if ($LASTEXITCODE -ne 0) {
    throw 'LeyoChatSetup/SfxStub build failed.'
}

$setupExeSrc = Join-Path $buildPath 'installer\LeyoChatSetup.exe'
$sfxStubExe = Join-Path $buildPath 'installer\SfxStub.exe'
if (!(Test-Path $setupExeSrc)) {
    throw "LeyoChatSetup.exe not found: $setupExeSrc"
}
if (!(Test-Path $sfxStubExe)) {
    throw "SfxStub.exe not found: $sfxStubExe"
}

Write-Host ''
Write-Host '==> Step 3: Stage Qt Quick installer shell'
$sfxPkgDir = Join-Path $buildPath 'package\server-sfx-package'
if (Test-Path $sfxPkgDir) {
    Remove-Item -LiteralPath $sfxPkgDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $sfxPkgDir | Out-Null

Copy-Item -LiteralPath $setupExeSrc -Destination $sfxPkgDir -Force
$setupExeDst = Join-Path $sfxPkgDir 'LeyoChatSetup.exe'

$qmlDir = Join-Path $workspaceRoot 'installer\qml'
& $windeployqt --release --no-translations --force --qmldir $qmlDir $setupExeDst
if ($LASTEXITCODE -ne 0) {
    Write-Warning 'windeployqt for LeyoChatSetup returned non-zero'
}

foreach ($name in @('opengl32sw.dll', 'D3Dcompiler_47.dll', 'vc_redist.x64.exe')) {
    $candidate = Join-Path $sfxPkgDir $name
    if (Test-Path $candidate) {
        Remove-Item $candidate -Force
    }
}

Get-ChildItem $sfxPkgDir -Recurse -Filter '*.qmltypes' | ForEach-Object {
    Remove-Item $_.FullName -Force
}

$compilerPath = Get-CMakeCacheValue -CMakeCachePath $cmakeCache -Key 'CMAKE_CXX_COMPILER'
if ($compilerPath -and $compilerPath -match 'MSVC[/\\]([^/\\]+)[/\\]bin') {
    $msvcVer = $Matches[1]
    $vsRoot = ($compilerPath -replace 'VC[/\\]Tools[/\\].*$', '')
    $crtDir = Join-Path (Convert-CMakePathToWindows $vsRoot) "VC\Redist\MSVC\$msvcVer\x64\Microsoft.VC142.CRT"
    if (Test-Path $crtDir) {
        Copy-Item (Join-Path $crtDir '*.dll') $sfxPkgDir -Force
    }
}

Write-Host ''
Write-Host '==> Step 4: Create UI bundle'
$bundleFile = Join-Path $buildPath 'package\server-sfx-payload.bundle'
if (Test-Path $bundleFile) {
    Remove-Item $bundleFile -Force
}

$uiFiles = Get-ChildItem -Path $sfxPkgDir -Recurse -File | Sort-Object FullName
$bundleStream = [System.IO.File]::Create($bundleFile)
$bw = [System.IO.BinaryWriter]::new($bundleStream, [System.Text.Encoding]::Unicode, $true)
try {
    $bw.Write([uint32]0x4C424348)
    $bw.Write([uint32]$uiFiles.Count)

    foreach ($file in $uiFiles) {
        $relPath = $file.FullName.Substring($sfxPkgDir.TrimEnd('\').Length + 1)
        $bw.Write([uint16]$relPath.Length)
        $bw.Write([System.Text.Encoding]::Unicode.GetBytes($relPath))
        $data = [System.IO.File]::ReadAllBytes($file.FullName)
        $bw.Write([uint32]$data.Length)
        $bw.Write($data)
    }
} finally {
    $bw.Flush()
    $bw.Dispose()
    $bundleStream.Dispose()
}

$uiBundleSize = (Get-Item $bundleFile).Length

Write-Host ''
Write-Host '==> Step 5: Assemble final LeyoChatServer installer'
$sfxOutputDir = Join-Path $buildPath 'package\server-installer-output'
New-Item -ItemType Directory -Force -Path $sfxOutputDir | Out-Null

$finalExePath = Join-Path $sfxOutputDir "LeyoChatServer-$appVersion-setup.exe"
if (Test-Path $finalExePath) {
    Remove-Item $finalExePath -Force
}

$innerSetupSize = (Get-Item $innerInstaller.FullName).Length
$stubBytes = [System.IO.File]::ReadAllBytes($sfxStubExe)
$bundleBytes = [System.IO.File]::ReadAllBytes($bundleFile)

$trailer = [byte[]]::new(12)
[System.BitConverter]::GetBytes([uint32]$uiBundleSize).CopyTo($trailer, 0)
[System.BitConverter]::GetBytes([uint32]$innerSetupSize).CopyTo($trailer, 4)
[System.BitConverter]::GetBytes([uint32]0x48435346).CopyTo($trailer, 8)

$output = [System.IO.File]::Create($finalExePath)
try {
    $output.Write($stubBytes, 0, $stubBytes.Length)
    $output.Write($bundleBytes, 0, $bundleBytes.Length)

    $innerStream = [System.IO.File]::OpenRead($innerInstaller.FullName)
    try {
        $buf = [byte[]]::new(1MB)
        $remaining = $innerStream.Length
        while ($remaining -gt 0) {
            $n = $innerStream.Read($buf, 0, [Math]::Min($buf.Length, $remaining))
            $output.Write($buf, 0, $n)
            $remaining -= $n
        }
    } finally {
        $innerStream.Close()
    }

    $output.Write($trailer, 0, $trailer.Length)
} finally {
    $output.Close()
}

$finalExe = Get-Item $finalExePath
$sha256 = (Get-FileHash -Path $finalExe.FullName -Algorithm SHA256).Hash.ToLower()
$serverLatestObj = @{
    version = $appVersion
    file = $finalExe.Name
    sha256 = $sha256
    publishedAtUtc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    minClientVersion = $appVersion
    notes = ''
}
$serverLatestJson = $serverLatestObj | ConvertTo-Json -Depth 1
$serverLatestJsonPath = Join-Path $sfxOutputDir 'server-latest.json'
$serverLatestJson | Out-File -FilePath $serverLatestJsonPath -Encoding utf8

Write-Host ''
Write-Host '================================================='
Write-Host '  Single-exe server installer created!'
Write-Host "  $($finalExe.FullName)"
Write-Host "  server-latest.json -> $serverLatestJsonPath"
Write-Host '  SfxStub infers LeyoChatServer and passes --installer-mode server.'
Write-Host '================================================='
