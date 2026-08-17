param(
    [string]$BuildDir = "build-msvc",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspaceRoot = Split-Path -Parent $scriptDir
. (Join-Path $scriptDir "package-windows.lib.ps1")

$cmakeExe = Join-Path $workspaceRoot "CMake\bin\cmake.exe"
if (!(Test-Path $cmakeExe)) {
    $cmakeExe = "cmake"
}

$buildPath = Join-Path $workspaceRoot $BuildDir
$stageDir = Join-Path $buildPath "package\stage"
$outputDir = Join-Path $buildPath "package\installer"
$issTemplate = Join-Path $workspaceRoot "windows\LeyoChat.iss.in"
$issFile = Join-Path $buildPath "package\LeyoChat.iss"
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
$appVersion = Get-ProjectVersionFromCache -CMakeCachePath $cmakeCache

# 如果编译器是 MSVC (cl.exe)，自动初始化 vcvars 环境
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

$removedVersionArtifacts = @(Reset-AppVersionBuildArtifacts -BuildPath $buildPath)
if ($removedVersionArtifacts.Count -gt 0) {
    Write-Host "==> Rebuild version-bound object files"
    foreach ($artifact in $removedVersionArtifacts) {
        Write-Host "    Removed stale artifact: $artifact"
    }
}

Write-Host "==> Clean and build LeyoChat and LeyoChatLauncher"
# Release packaging must not trust incremental MSVC dependency state. A stale
# object compiled against an older public struct layout can still link and then
# crash at startup. --clean-first makes every packaged client internally ABI
# consistent even when /showIncludes dependency capture is unavailable.
& $cmakeExe --build $buildPath --config $Configuration --clean-first --target LeyoChat LeyoChatLauncher
if ($LASTEXITCODE -ne 0) {
    throw "LeyoChat build failed."
}
$builtAppExe = Join-Path $buildPath "LeyoChat.exe"
$builtLauncherExe = Join-Path $buildPath "LeyoChatLauncher.exe"
if (!(Test-Path $builtLauncherExe)) {
    throw "LeyoChatLauncher build output not found: $builtLauncherExe"
}
Assert-AppExecutableRuntimeVersion -ExecutablePath $builtAppExe -ExpectedVersion $appVersion

Write-Host "==> Install to staging directory"
& $cmakeExe --install $buildPath --config $Configuration --prefix $stageDir
if ($LASTEXITCODE -ne 0) {
    throw "Install to staging directory failed."
}

$appExe = Join-Path $stageDir "LeyoChat.exe"
$launcherExe = Join-Path $stageDir "LeyoChatLauncher.exe"
if (!(Test-Path $appExe)) {
    throw "Main executable not found after install: $appExe"
}
if (!(Test-Path $launcherExe)) {
    throw "Launcher executable not found after install: $launcherExe"
}
Assert-AppExecutableRuntimeVersion -ExecutablePath $appExe -ExpectedVersion $appVersion

Write-Host "==> Collect Qt runtime with windeployqt"
Write-Host "    Qt tool: $windeployqt"
Write-Host "    Compiler: $compilerBinDir"
& $windeployqt `
    --release `
    --compiler-runtime `
    --no-translations `
    --force `
    $appExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed for LeyoChat."
}

# Copy MSVC C++ runtime DLLs if building with MSVC and windeployqt didn't
# collect them (windeployqt --compiler-runtime relies on vcredist being in a
# specific location that may not exist in all VS installs).
if (-not (Test-Path (Join-Path $stageDir "vcruntime140.dll"))) {
    if (-not (Copy-MsvcRuntimeDllsFromCache -CMakeCachePath $cmakeCache -DestinationDir $stageDir)) {
        Write-Host "WARNING: MSVC runtime DLLs not found from CMAKE_CXX_COMPILER."
    }
}

# Remove the full vc_redist installer that windeployqt may have copied — the
# individual runtime DLLs above are sufficient and save ~24 MB.
$vcRedistExe = Join-Path $stageDir "vc_redist.x64.exe"
if (Test-Path $vcRedistExe) {
    Remove-Item $vcRedistExe -Force
    Write-Host "    Removed vc_redist.x64.exe (individual DLLs are shipped instead)"
}

# Strip runtime-unnecessary files from staging directory
Write-Host "==> Strip unnecessary files from staging"
$stageStripped = 0; $stageStrippedBytes = 0L

# d3dcompiler_47.dll — Qt 6 RHI 不依赖它，Windows SDK 自带
# opengl32sw.dll 保留：虚拟机/无 GPU 环境下 WebEngine 需要软件渲染
foreach ($name in @('d3dcompiler_47.dll')) {
    $f = Join-Path $stageDir $name
    if (Test-Path $f) {
        $stageStrippedBytes += (Get-Item $f).Length; $stageStripped++
        Remove-Item $f -Force
    }
}

# qmltooling — QML 调试器，生产环境不需要
$qmltoolingDir = Join-Path $stageDir 'qmltooling'
if (Test-Path $qmltoolingDir) {
    $fs = Get-ChildItem $qmltoolingDir -Recurse -File
    $stageStrippedBytes += ($fs | Measure-Object Length -Sum).Sum
    $stageStripped += $fs.Count
    Remove-Item $qmltoolingDir -Recurse -Force
}

# ElaWidgetTools 开发文件（include/ 和 lib/cmake/）— 运行时不加载
foreach ($devPath in @('ElaWidgetTools\include', 'ElaWidgetTools\lib\cmake')) {
    $d = Join-Path $stageDir $devPath
    if (Test-Path $d) {
        $fs = Get-ChildItem $d -Recurse -File
        $stageStrippedBytes += ($fs | Measure-Object Length -Sum).Sum
        $stageStripped += $fs.Count
        Remove-Item $d -Recurse -Force
    }
}

# qtwebengine_devtools_resources.pak — Chrome DevTools，只有 remote debug 时才用
$devtoolsPak = Join-Path $stageDir 'resources\qtwebengine_devtools_resources.pak'
if (Test-Path $devtoolsPak) {
    $stageStrippedBytes += (Get-Item $devtoolsPak).Length; $stageStripped++
    Remove-Item $devtoolsPak -Force
}

Write-Host "    Stripped $stageStripped files, saved $([math]::Round($stageStrippedBytes/1MB,1)) MB"

# Copy ElaWidgetTools DLL
$elaDir = Join-Path $buildPath "third_party\ElaWidgetTools"
$elaDll = Join-Path $elaDir "ElaWidgetTools.dll"
if (Test-Path $elaDll) {
    Write-Host "==> Copy ElaWidgetTools.dll to staging"
    Copy-Item -LiteralPath $elaDll -Destination $stageDir -Force
} else {
    Write-Warning "ElaWidgetTools.dll not found at $elaDll"
}

# Copy built-in sticker packs
$stickersSource = Join-Path $workspaceRoot "stickers"
if (Test-Path $stickersSource) {
    $stickersDest = Join-Path $stageDir "stickers"
    Write-Host "==> Copy sticker packs to staging"
    Copy-Item -Path $stickersSource -Destination $stickersDest -Recurse -Force
}

$iscc = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
if (-not $iscc -and (Test-Path $defaultIscc)) {
    $iscc = @{ Source = $defaultIscc }
}
if (-not $iscc) {
    throw "Cannot find iscc.exe from Inno Setup 6."
}

$appId = "{{4A2F16E7-8A48-4B0E-9E4F-E6B26D6BC68C}}"
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

Write-Host "==> Build installer"
& $iscc.Source $issFile
if ($LASTEXITCODE -ne 0) {
    throw "Installer generation failed."
}

$installer = Get-ChildItem -Path $outputDir -Filter "*.exe" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $installer) {
    throw "Installer generation failed: no exe found in output directory."
}

Write-Host ""
Write-Host "Installer created:"
Write-Host $installer.FullName

# ── 生成 latest.json（供自动升级使用）──
$sha256 = (Get-FileHash -Path $installer.FullName -Algorithm SHA256).Hash.ToLower()
$releaseNotesText = [System.IO.File]::ReadAllText($releaseNotesFile, [System.Text.Encoding]::UTF8)
$latestJson = @{
    version = $appVersion
    file = $installer.Name
    sha256 = $sha256
    releaseNotes = $releaseNotesText
    minVersion = ""
} | ConvertTo-Json -Depth 1
$latestJsonPath = Join-Path $outputDir "latest.json"
$latestJson | Out-File -FilePath $latestJsonPath -Encoding utf8
Write-Host "Generated latest.json at $latestJsonPath"
