[CmdletBinding()]
param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$BuildDir = '',
    [string]$QtBin = '',
    [string]$MinGwBin = '',
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$packageLib = Join-Path $ProjectRoot 'scripts\package-windows.lib.ps1'
if (-not (Test-Path $packageLib)) {
    throw "Packaging library not found: $packageLib"
}
. $packageLib

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $ProjectRoot 'build'
}

$cmakeCache = Join-Path $BuildDir 'CMakeCache.txt'
if (-not (Test-Path $cmakeCache)) {
    throw "CMake cache not found: $cmakeCache"
}

if ([string]::IsNullOrWhiteSpace($QtBin)) {
    $QtBin = Resolve-QtBinDirFromCache -CMakeCachePath $cmakeCache
}
if ([string]::IsNullOrWhiteSpace($MinGwBin)) {
    $MinGwBin = Resolve-CompilerBinDirFromCache -CMakeCachePath $cmakeCache
}

$stageDir = Join-Path $BuildDir 'package\stage'
$portableDir = Join-Path $BuildDir 'package\portable'
$installerDir = Join-Path $BuildDir 'package\installer'
$exePath = Join-Path $BuildDir 'LeyoChat.exe'
$issPath = Join-Path $BuildDir 'package\LeyoChat.iss'
$issTemplatePath = Join-Path $ProjectRoot 'windows\LeyoChat.iss.in'
$iconPath = Join-Path $ProjectRoot 'windows\leyochat-icon.ico'
$languagePath = Join-Path $ProjectRoot 'windows\ChineseSimplified.isl'
$releaseNotesPath = Join-Path $ProjectRoot 'release-notes\installer-release-notes.txt'
$appPublisher = 'LeXiaoYao2704'
$windeployqt = if ([string]::IsNullOrWhiteSpace($QtBin)) { $null } else { Join-Path $QtBin 'windeployqt.exe' }
$isccCommand = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
$programFilesX86Iscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
$iscc = if ($isccCommand) {
    $isccCommand.Source
} elseif (Test-Path $programFilesX86Iscc) {
    $programFilesX86Iscc
} else {
    Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'
}

if (-not $windeployqt -or -not (Test-Path $windeployqt)) {
    throw "windeployqt.exe not found: $windeployqt"
}

if (-not (Test-Path $issTemplatePath)) {
    throw "Installer template not found: $issTemplatePath"
}

if (-not (Test-Path $iconPath)) {
    throw "Installer icon not found: $iconPath"
}

if (-not (Test-Path $languagePath)) {
    throw "Installer language file not found: $languagePath"
}

if (-not (Test-Path $releaseNotesPath)) {
    throw "Installer release notes not found: $releaseNotesPath"
}

cmake -S $ProjectRoot -B $BuildDir | Out-Null
# Installer builds must never reuse objects compiled against an older public
# header layout. Keep this legacy entry point aligned with the primary client
# and server packaging scripts.
cmake --build $BuildDir --config $Configuration --clean-first --target LeyoChat

$appVersion = Get-ProjectVersionFromCache -CMakeCachePath $cmakeCache
$portableZipPath = Join-Path $portableDir ("LeyoChat-{0}-portable.zip" -f $appVersion)

if (-not (Test-Path $exePath)) {
    throw "Built executable not found: $exePath"
}

if (Test-Path $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
New-Item -ItemType Directory -Force -Path $portableDir | Out-Null
New-Item -ItemType Directory -Force -Path $installerDir | Out-Null

Copy-Item -LiteralPath $exePath -Destination $stageDir

$licenseDir = Join-Path $stageDir 'licenses'
New-Item -ItemType Directory -Force -Path $licenseDir | Out-Null
$licenseFiles = @(
    @{ Source = 'LICENSE'; Destination = 'LICENSE' },
    @{ Source = 'NOTICE'; Destination = 'NOTICE' },
    @{ Source = 'THIRD_PARTY_NOTICES.md'; Destination = 'THIRD_PARTY_NOTICES.md' },
    @{ Source = 'third_party\ElaWidgetTools\LICENSE'; Destination = 'ElaWidgetTools-MIT.txt' },
    @{ Source = 'third_party\ElaWidgetTools\UPSTREAM.md'; Destination = 'ElaWidgetTools-UPSTREAM.md' },
    @{ Source = 'third_party\md4c\LICENSE.md'; Destination = 'md4c-MIT.txt' },
    @{ Source = 'windows\ChineseSimplified.LICENSE'; Destination = 'InnoSetup-ChineseSimplified-MIT.txt' },
    @{ Source = 'LICENSES\LGPL-3.0-only.txt'; Destination = 'LGPL-3.0-only.txt' },
    @{ Source = 'LICENSES\GPL-3.0-only.txt'; Destination = 'GPL-3.0-only.txt' },
    @{ Source = 'LICENSES\OFL-1.1.txt'; Destination = 'OFL-1.1.txt' }
)
foreach ($licenseFile in $licenseFiles) {
    Copy-Item `
        -LiteralPath (Join-Path $ProjectRoot $licenseFile.Source) `
        -Destination (Join-Path $licenseDir $licenseFile.Destination) `
        -Force
}

& $windeployqt --dir $stageDir --release --force (Join-Path $stageDir 'LeyoChat.exe')

$runtimeDlls = @(
    'libstdc++-6.dll',
    'libgcc_s_seh-1.dll',
    'libwinpthread-1.dll'
)

foreach ($runtimeDll in $runtimeDlls) {
    $source = Join-Path $MinGwBin $runtimeDll
    if (-not (Test-Path $source)) {
        throw "Required MinGW runtime not found: $source"
    }
    Copy-Item -LiteralPath $source -Destination $stageDir -Force
}

if (Test-Path $portableZipPath) {
    Remove-Item -LiteralPath $portableZipPath -Force
}
Compress-Archive -Path (Join-Path $stageDir '*') -DestinationPath $portableZipPath -CompressionLevel Optimal

$escapedStageDir = $stageDir -replace '\\', '\\'
$escapedOutputDir = $installerDir -replace '\\', '\\'
$escapedIconPath = $iconPath -replace '\\', '\\'
$escapedLanguagePath = $languagePath -replace '\\', '\\'
$escapedReleaseNotesPath = $releaseNotesPath -replace '\\', '\\'

$issTemplate = Get-Content -LiteralPath $issTemplatePath -Raw
$issTemplate = $issTemplate.Replace('@APP_VERSION@', $appVersion)
$issTemplate = $issTemplate.Replace('@APP_ID@', '{{4A2F16E7-8A48-4B0E-9E4F-E6B26D6BC68C}}')
$issTemplate = $issTemplate.Replace('@APP_PUBLISHER@', $appPublisher)
$issTemplate = $issTemplate.Replace('@STAGE_DIR@', $escapedStageDir)
$issTemplate = $issTemplate.Replace('@OUTPUT_DIR@', $escapedOutputDir)
$issTemplate = $issTemplate.Replace('@ICON_FILE@', $escapedIconPath)
$issTemplate = $issTemplate.Replace('@LANGUAGE_FILE@', $escapedLanguagePath)
$issTemplate = $issTemplate.Replace('@RELEASE_NOTES_FILE@', $escapedReleaseNotesPath)
Set-Content -LiteralPath $issPath -Value $issTemplate -Encoding UTF8

Get-ChildItem -LiteralPath $installerDir -Filter '*.exe' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue

if (-not (Test-Path $iscc)) {
    throw "ISCC.exe not found: $iscc"
}

& $iscc $issPath

Write-Output "VERSION=$appVersion"
Write-Output "STAGE_PATH=$stageDir"
Write-Output "PORTABLE_PATH=$portableZipPath"
Write-Output "INSTALLER_PATH=$(Join-Path $installerDir ('LeyoChat-{0}-setup.exe' -f $appVersion))"
