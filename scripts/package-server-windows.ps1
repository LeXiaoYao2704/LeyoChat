param(
    [string]$BuildDir = "build-msvc",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspaceRoot = Split-Path -Parent $scriptDir
. (Join-Path $scriptDir "package-windows.lib.ps1")

function Resolve-BuiltExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildPath,
        [Parameter(Mandatory = $true)]
        [string]$ConfigurationName,
        [Parameter(Mandatory = $true)]
        [string]$ExecutableName
    )

    $candidates = @(
        (Join-Path $BuildPath $ExecutableName),
        (Join-Path (Join-Path $BuildPath $ConfigurationName) $ExecutableName)
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    throw "Built executable not found: $ExecutableName"
}

$cmakeExe = Join-Path $workspaceRoot "CMake\bin\cmake.exe"
if (!(Test-Path $cmakeExe)) {
    $cmakeExe = "cmake"
}

$buildPath = Join-Path $workspaceRoot $BuildDir
$stageDir = Join-Path $buildPath "package\server-stage"
$outputDir = Join-Path $buildPath "package\server-installer"
$issTemplate = Join-Path $workspaceRoot "windows\LeyoChatServer.iss.in"
$issFile = Join-Path $buildPath "package\LeyoChatServer.iss"
$defaultIscc = Join-Path $workspaceRoot "InnoSetup6\ISCC.exe"
$iconFile = Join-Path $workspaceRoot "windows\leyochat-icon.ico"
$languageFile = Join-Path $workspaceRoot "windows\ChineseSimplified.isl"
$releaseNotesFile = Join-Path $workspaceRoot "release-notes\installer-release-notes.txt"
$appPublisher = "LeXiaoYao2704"
$cmakeCache = Join-Path $buildPath "CMakeCache.txt"

if (!(Test-Path $buildPath)) {
    throw "Build directory does not exist: $buildPath"
}
if (!(Test-Path $cmakeCache)) {
    throw "CMake cache not found: $cmakeCache"
}
if (!(Test-Path $issTemplate)) {
    throw "Server installer template not found: $issTemplate"
}
if (!(Test-Path $iconFile)) {
    throw "Cannot find app icon: $iconFile"
}
if (!(Test-Path $languageFile)) {
    throw "Cannot find installer language file: $languageFile"
}
if (!(Test-Path $releaseNotesFile)) {
    throw "Cannot find installer release notes file: $releaseNotesFile"
}

$windeployqt = Resolve-WindeployQtPath -CMakeCachePath $cmakeCache
if (-not $windeployqt) {
    throw "Cannot find windeployqt.exe. Configure Qt in CMake or put windeployqt on PATH."
}

$compilerBinDir = Resolve-CompilerBinDirFromCache -CMakeCachePath $cmakeCache
if (-not $compilerBinDir) {
    throw "Cannot resolve CMAKE_CXX_COMPILER from: $cmakeCache"
}

$compilerExe = Get-CMakeCacheValue -CMakeCachePath $cmakeCache -Key "CMAKE_CXX_COMPILER"
if ($compilerExe -and (Split-Path -Leaf $compilerExe) -eq "cl.exe") {
    $vcInstallDir = $compilerExe
    while ($vcInstallDir -and !(Test-Path (Join-Path $vcInstallDir "Auxiliary\Build\vcvars64.bat"))) {
        $vcInstallDir = Split-Path -Parent $vcInstallDir
        if ([string]::IsNullOrEmpty($vcInstallDir)) { break }
    }
    if ($vcInstallDir -and (Test-Path (Join-Path $vcInstallDir "Auxiliary\Build\vcvars64.bat"))) {
        $vcvarsPath = Join-Path $vcInstallDir "Auxiliary\Build\vcvars64.bat"
        Write-Host "==> Initializing MSVC environment from: $vcvarsPath"
        cmd /c "`"$vcvarsPath`" >nul 2>&1 && set" | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)$') {
                [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
            }
        }
    } else {
        Write-Warning "MSVC compiler detected but vcvars64.bat not found. Build may fail."
    }
}

New-Item -ItemType Directory -Force -Path (Join-Path $buildPath "package") | Out-Null
if (Test-Path $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
if (Test-Path $outputDir) {
    Remove-Item -LiteralPath $outputDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Write-Host "==> Clean and build LeyoChatService and LeyoChatServiceHost"
# Keep release binaries free of stale objects compiled against older shared
# header layouts; see the matching client packaging safeguard.
& $cmakeExe --build $buildPath --config $Configuration --clean-first --target LeyoChatService LeyoChatServiceHost
if ($LASTEXITCODE -ne 0) {
    throw "Server targets build failed."
}

Write-Host "==> Stage server executables"
$serviceExeBuilt = Resolve-BuiltExecutable -BuildPath $buildPath -ConfigurationName $Configuration -ExecutableName "LeyoChatService.exe"
$hostExeBuilt = Resolve-BuiltExecutable -BuildPath $buildPath -ConfigurationName $Configuration -ExecutableName "LeyoChatServiceHost.exe"
Copy-Item -LiteralPath $serviceExeBuilt -Destination (Join-Path $stageDir "LeyoChatService.exe") -Force
Copy-Item -LiteralPath $hostExeBuilt -Destination (Join-Path $stageDir "LeyoChatServiceHost.exe") -Force

$licenseDir = Join-Path $stageDir "licenses"
New-Item -ItemType Directory -Force -Path $licenseDir | Out-Null
$licenseFiles = @(
    @{ Source = "LICENSE"; Destination = "LICENSE" },
    @{ Source = "NOTICE"; Destination = "NOTICE" },
    @{ Source = "THIRD_PARTY_NOTICES.md"; Destination = "THIRD_PARTY_NOTICES.md" },
    @{ Source = "third_party\ElaWidgetTools\LICENSE"; Destination = "ElaWidgetTools-MIT.txt" },
    @{ Source = "third_party\ElaWidgetTools\UPSTREAM.md"; Destination = "ElaWidgetTools-UPSTREAM.md" },
    @{ Source = "third_party\md4c\LICENSE.md"; Destination = "md4c-MIT.txt" },
    @{ Source = "windows\ChineseSimplified.LICENSE"; Destination = "InnoSetup-ChineseSimplified-MIT.txt" },
    @{ Source = "LICENSES\LGPL-3.0-only.txt"; Destination = "LGPL-3.0-only.txt" },
    @{ Source = "LICENSES\GPL-3.0-only.txt"; Destination = "GPL-3.0-only.txt" },
    @{ Source = "LICENSES\OFL-1.1.txt"; Destination = "OFL-1.1.txt" }
)
foreach ($licenseFile in $licenseFiles) {
    Copy-Item `
        -LiteralPath (Join-Path $workspaceRoot $licenseFile.Source) `
        -Destination (Join-Path $licenseDir $licenseFile.Destination) `
        -Force
}

$serviceExe = Join-Path $stageDir "LeyoChatService.exe"
$hostExe = Join-Path $stageDir "LeyoChatServiceHost.exe"

Write-Host "==> Collect Qt runtime with windeployqt"
Write-Host "    Qt tool: $windeployqt"
Write-Host "    Compiler: $compilerBinDir"
& $windeployqt `
    --release `
    --compiler-runtime `
    --no-translations `
    --force `
    $serviceExe
if ($LASTEXITCODE -ne 0) {
    Write-Warning "windeployqt for LeyoChatService returned non-zero"
}
& $windeployqt `
    --release `
    --compiler-runtime `
    --no-translations `
    --force `
    $hostExe
if ($LASTEXITCODE -ne 0) {
    Write-Warning "windeployqt for LeyoChatServiceHost returned non-zero"
}

$qtBinDir = Split-Path -Parent $windeployqt
$qtRoot = Split-Path -Parent $qtBinDir
$sqlDriverSource = Join-Path $qtRoot "plugins\sqldrivers"
if (Test-Path $sqlDriverSource) {
    $sqlDriverDest = Join-Path $stageDir "sqldrivers"
    New-Item -ItemType Directory -Force -Path $sqlDriverDest | Out-Null
    Copy-Item -LiteralPath (Join-Path $sqlDriverSource "qsqlite.dll") -Destination $sqlDriverDest -Force -ErrorAction SilentlyContinue
    Copy-Item -LiteralPath (Join-Path $sqlDriverSource "qsqlodbc.dll") -Destination $sqlDriverDest -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path (Join-Path $stageDir "vcruntime140.dll"))) {
    if (-not (Copy-MsvcRuntimeDllsFromCache -CMakeCachePath $cmakeCache -DestinationDir $stageDir)) {
        Write-Host "WARNING: MSVC runtime DLLs not found from CMAKE_CXX_COMPILER."
    }
}

$vcRedistExe = Join-Path $stageDir "vc_redist.x64.exe"
if (Test-Path $vcRedistExe) {
    Remove-Item $vcRedistExe -Force
}

$iscc = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
if (-not $iscc -and (Test-Path $defaultIscc)) {
    $iscc = @{ Source = $defaultIscc }
}
if (-not $iscc) {
    throw "Cannot find iscc.exe from Inno Setup 6."
}

$appVersion = Get-ProjectVersionFromCache -CMakeCachePath $cmakeCache
$appId = "{{D879A4AF-A555-4A2A-A72D-B747D1530FC6}}"
$template = Get-Content -Path $issTemplate -Raw
$template = $template.Replace("@APP_VERSION@", $appVersion)
$template = $template.Replace("@APP_ID@", $appId)
$template = $template.Replace("@APP_PUBLISHER@", $appPublisher)
$template = $template.Replace("@STAGE_DIR@", $stageDir.Replace('\', '\\'))
$template = $template.Replace("@OUTPUT_DIR@", $outputDir.Replace('\', '\\'))
$template = $template.Replace("@ICON_FILE@", $iconFile.Replace('\', '\\'))
$template = $template.Replace("@LANGUAGE_FILE@", $languageFile.Replace('\', '\\'))
$template = $template.Replace("@RELEASE_NOTES_FILE@", $releaseNotesFile.Replace('\', '\\'))
Set-Content -Path $issFile -Value $template -Encoding UTF8

Write-Host "==> Build LeyoChatServer inner installer"
& $iscc.Source $issFile
if ($LASTEXITCODE -ne 0) {
    throw "Server installer generation failed."
}

$installer = Get-ChildItem -Path $outputDir -Filter "LeyoChatServer-*.exe" |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $installer) {
    throw "Server installer generation failed: no exe found in output directory."
}

$sha256 = (Get-FileHash -Path $installer.FullName -Algorithm SHA256).Hash.ToLower()
$serverLatestObj = @{
    version        = $appVersion
    file           = $installer.Name
    sha256         = $sha256
    publishedAtUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    minClientVersion = $appVersion
    notes          = ""
}
$serverLatestJson = $serverLatestObj | ConvertTo-Json -Depth 1
$serverLatestJsonPath = Join-Path $outputDir "server-latest.json"
$serverLatestJson | Out-File -FilePath $serverLatestJsonPath -Encoding utf8

Write-Host ""
Write-Host "Server installer created:"
Write-Host $installer.FullName
Write-Host "server-latest.json -> $serverLatestJsonPath"
