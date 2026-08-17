param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

$helpersPath = Join-Path $RepoRoot "scripts\package-windows.lib.ps1"
. $helpersPath

$releaseNotesPath = Join-Path $RepoRoot "release-notes\installer-release-notes.txt"
if (-not (Test-Path $releaseNotesPath)) {
    throw "Expected release notes file for installer packaging: $releaseNotesPath"
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("leyochat-package-tests-" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

try {
    $cachePath = Join-Path $tempRoot "CMakeCache.txt"
    @'
CMAKE_CXX_COMPILER:FILEPATH=C:/toolchains/winlibs/bin/c++.exe
Qt6_DIR:PATH=C:/Qt/6.6.3/mingw_64/lib/cmake/Qt6
'@ | Set-Content -Path $cachePath -Encoding UTF8

    $qtBinDir = Resolve-QtBinDirFromCache -CMakeCachePath $cachePath
    if ($qtBinDir -ne "C:\Qt\6.6.3\mingw_64\bin") {
        throw "Expected Qt bin dir from Qt6_DIR, got: $qtBinDir"
    }

    $compilerBinDir = Resolve-CompilerBinDirFromCache -CMakeCachePath $cachePath
    if ($compilerBinDir -ne "C:\toolchains\winlibs\bin") {
        throw "Expected compiler bin dir from CMAKE_CXX_COMPILER, got: $compilerBinDir"
    }

    $fakeVsRoot = Join-Path $tempRoot "VS"
    $fakeCompilerPath = Join-Path $fakeVsRoot "VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fakeCompilerPath) | Out-Null
    New-Item -ItemType File -Force -Path $fakeCompilerPath | Out-Null
    $fakeRuntimeDir = Join-Path $fakeVsRoot "VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT"
    New-Item -ItemType Directory -Force -Path $fakeRuntimeDir | Out-Null
    New-Item -ItemType File -Force -Path (Join-Path $fakeRuntimeDir "vcruntime140.dll") | Out-Null

    $resolvedRuntimeDir = Resolve-MsvcRuntimeDirFromCompilerPath -CompilerPath $fakeCompilerPath
    if ($resolvedRuntimeDir -ne $fakeRuntimeDir) {
        throw "Expected MSVC runtime dir to support VC143/redist version mismatch, got: $resolvedRuntimeDir"
    }

    $fakeMsvcCachePath = Join-Path $tempRoot "MsvcCache.txt"
    "CMAKE_CXX_COMPILER:FILEPATH=$fakeCompilerPath" | Set-Content -Path $fakeMsvcCachePath -Encoding UTF8
    $runtimeDest = Join-Path $tempRoot "runtime-dest"
    if (-not (Copy-MsvcRuntimeDllsFromCache -CMakeCachePath $fakeMsvcCachePath -DestinationDir $runtimeDest)) {
        throw "Expected MSVC runtime copy helper to copy DLLs"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $runtimeDest "vcruntime140.dll"))) {
        throw "Expected MSVC runtime DLL to be copied into destination"
    }

    $fakeBuildPath = Join-Path $tempRoot "build-version-artifacts"
    $fakeGeneratedDir = Join-Path $fakeBuildPath "generated"
    $fakeObjectDir = Join-Path $fakeBuildPath "CMakeFiles\LeyoChat.dir\src\app"
    New-Item -ItemType Directory -Force -Path $fakeGeneratedDir, $fakeObjectDir | Out-Null
    $fakeHeader = Join-Path $fakeGeneratedDir "AppBuildInfo.h"
    $fakeObject = Join-Path $fakeObjectDir "ApplicationInfo.cpp.obj"
    $companyName = "LeXiaoYao2704"
    "#define LEYOCHAT_APP_VERSION `"0.4.0`"" | Set-Content -Path $fakeHeader -Encoding UTF8
    [System.IO.File]::WriteAllBytes($fakeObject, [System.Text.Encoding]::Unicode.GetBytes("LeyoChat $companyName 0.3.3 :/docs/release-notes/current.txt"))
    (Get-Item $fakeHeader).LastWriteTimeUtc = [DateTime]::UtcNow
    (Get-Item $fakeObject).LastWriteTimeUtc = [DateTime]::UtcNow.AddMinutes(-5)

    $removedVersionObjects = @(Reset-AppVersionBuildArtifacts -BuildPath $fakeBuildPath)
    if ($removedVersionObjects.Count -ne 1 -or $removedVersionObjects[0] -ne $fakeObject) {
        throw "Expected stale ApplicationInfo object to be removed before packaging"
    }
    if (Test-Path -LiteralPath $fakeObject) {
        throw "Expected stale ApplicationInfo object file to be deleted"
    }

    $fakeExe = Join-Path $fakeBuildPath "LeyoChat.exe"
    [System.IO.File]::WriteAllBytes(
        $fakeExe,
        [System.Text.Encoding]::Unicode.GetBytes("LeyoChat $companyName 0.4.0 :/docs/release-notes/current.txt"))
    Assert-AppExecutableRuntimeVersion -ExecutablePath $fakeExe -ExpectedVersion "0.4.0"

    [System.IO.File]::WriteAllBytes(
        $fakeExe,
        [System.Text.Encoding]::Unicode.GetBytes("LeyoChat $companyName 0.3.3 :/docs/release-notes/current.txt"))
    $staleVersionRejected = $false
    try {
        Assert-AppExecutableRuntimeVersion -ExecutablePath $fakeExe -ExpectedVersion "0.4.0"
    } catch {
        $staleVersionRejected = $true
    }
    if (-not $staleVersionRejected) {
        throw "Expected runtime version assertion to reject stale ApplicationInfo version"
    }

    $missingQtCachePath = Join-Path $tempRoot "MissingQtCache.txt"
    @'
CMAKE_PREFIX_PATH:UNINITIALIZED=C:/SDKs/Qt/6.7.2/mingw_64;C:/other/path
'@ | Set-Content -Path $missingQtCachePath -Encoding UTF8

    $fallbackQtBinDir = Resolve-QtBinDirFromCache -CMakeCachePath $missingQtCachePath
    if ($fallbackQtBinDir -ne "C:\SDKs\Qt\6.7.2\mingw_64\bin") {
        throw "Expected Qt bin dir from CMAKE_PREFIX_PATH, got: $fallbackQtBinDir"
    }

    $clientTemplatePath = Join-Path $RepoRoot "windows\LeyoChat.iss.in"
    $serverTemplatePath = Join-Path $RepoRoot "windows\LeyoChatServer.iss.in"
    $clientPackageScript = Join-Path $RepoRoot "scripts\package-windows.ps1"
    $clientSfxScript = Join-Path $RepoRoot "scripts\package-installer.ps1"
    $legacyClientPackageScript = Join-Path $RepoRoot "windows\Build-Installer.ps1"
    $serverPackageScript = Join-Path $RepoRoot "scripts\package-server-windows.ps1"
    $serverSfxScript = Join-Path $RepoRoot "scripts\package-server-installer.ps1"
    $installerBackendHeader = Join-Path $RepoRoot "installer\InstallerBackend.h"
    $installerBackendCpp = Join-Path $RepoRoot "installer\InstallerBackend.cpp"
    $installerOptionsQml = Join-Path $RepoRoot "installer\qml\OptionsPage.qml"

    if (-not (Test-Path $clientTemplatePath)) {
        throw "Expected client installer template: $clientTemplatePath"
    }
    if (-not (Test-Path $clientPackageScript)) {
        throw "Expected client packaging script: $clientPackageScript"
    }
    if (-not (Test-Path $clientSfxScript)) {
        throw "Expected client SFX packaging script: $clientSfxScript"
    }
    if (-not (Test-Path $legacyClientPackageScript)) {
        throw "Expected legacy client packaging script: $legacyClientPackageScript"
    }
    foreach ($installerFile in @($installerBackendHeader, $installerBackendCpp, $installerOptionsQml)) {
        if (-not (Test-Path $installerFile)) {
            throw "Expected installer UI file: $installerFile"
        }
    }

    $clientTemplate = Get-Content -Raw -Path $clientTemplatePath
    if ($clientTemplate -notmatch [regex]::Escape('LeyoChat-@APP_VERSION@-setup')) {
        throw "Client installer output must remain LeyoChat-<version>-setup"
    }
    if ($clientTemplate -match 'LeyoFileService\.exe') {
        throw "Client installer must not package LeyoFileService.exe"
    }
    if ($clientTemplate -match 'Name:\s*"fileservice"') {
        throw "Client installer must not expose the old optional file-service task"
    }
    foreach ($needle in @(
        '#define MyLauncherExeName "LeyoChatLauncher.exe"',
        'Filename: "{app}\{#MyLauncherExeName}"',
        'ValueData: """{app}\{#MyLauncherExeName}"""',
        'ExecHidden(''taskkill.exe'', ''/IM "{#MyLauncherExeName}" /F'')'
    )) {
        if ($clientTemplate -notmatch [regex]::Escape($needle)) {
            throw "Client installer must route user launch entrypoints through launcher: $needle"
        }
    }
    foreach ($needle in @('IsProcessRunningByName', 'Could not stop LeyoChat processes')) {
        if ($clientTemplate -notmatch [regex]::Escape($needle)) {
            throw "Client installer must fail closed when application processes cannot stop: $needle"
        }
    }
    if ($clientTemplate -notmatch 'program=""\{app\}\\\{#MyAppExeName\}""') {
        throw "Client firewall rules must continue to target LeyoChat.exe"
    }

    $clientPackageScriptText = Get-Content -Raw -Path $clientPackageScript
    if ($clientPackageScriptText -match 'LeyoFileService') {
        throw "Client packaging script must not build, stage, or deploy LeyoFileService"
    }
    if ($clientPackageScriptText -match 'LeyoChatService|LeyoChatServiceHost') {
        throw "Client packaging script must not build, stage, or deploy server service targets"
    }
    foreach ($needle in @('LeyoChat', 'LeyoChatLauncher', 'latest.json')) {
        if ($clientPackageScriptText -notmatch [regex]::Escape($needle)) {
            throw "Client packaging script missing expected content: $needle"
        }
    }
    if ($clientPackageScriptText -notmatch [regex]::Escape('--clean-first')) {
        throw "Client packaging must clean stale target objects before a release build"
    }

    $legacyClientPackageScriptText = Get-Content -Raw -Path $legacyClientPackageScript
    if ($legacyClientPackageScriptText -notmatch [regex]::Escape('--clean-first')) {
        throw "Every client packaging entry point must clean stale target objects"
    }

    $clientSfxScriptText = Get-Content -Raw -Path $clientSfxScript
    foreach ($needle in @(
        'package-windows.ps1',
        'LeyoChat-$appVersion-setup.exe',
        'latest.json',
        'LeyoChatLauncher.pdb'
    )) {
        if ($clientSfxScriptText -notmatch [regex]::Escape($needle)) {
            throw "Client SFX packaging script missing expected content: $needle"
        }
    }
    if ($clientSfxScriptText -notmatch [regex]::Escape("throw 'LeyoChatLauncher.pdb was not archived.'")) {
        throw "Client SFX packaging must fail when LeyoChatLauncher.pdb is missing"
    }
    if ($clientSfxScriptText -notmatch [regex]::Escape("Remove-Item -LiteralPath `$pdbDir -Recurse -Force")) {
        throw "Client SFX packaging must clear stale symbols before archiving PDB files"
    }
    foreach ($forbidden in @(
        'package-server-windows.ps1',
        'LeyoChatServer',
        'server-latest.json',
        '--installer-mode'
    )) {
        if ($clientSfxScriptText -match [regex]::Escape($forbidden)) {
            throw "Client SFX packaging script must not contain server packaging content: $forbidden"
        }
    }

    foreach ($installerFile in @($installerBackendHeader, $installerBackendCpp, $installerOptionsQml)) {
        $installerText = Get-Content -Raw -Path $installerFile
        if ($installerText -match 'installFileService') {
            throw "Installer UI/backend must not expose the removed file-service install option: $installerFile"
        }
        if ($installerText -match '安装本地文件服务') {
            throw "Installer UI must not show the removed local file-service checkbox: $installerFile"
        }
    }
    $installerBackendText = Get-Content -Raw -Path $installerBackendCpp
    foreach ($needle in @('LeyoChatLauncher.exe', 'launcherProcessName()', 'launch launcher')) {
        if ($installerBackendText -notmatch [regex]::Escape($needle)) {
            throw "Installer backend must stop and launch through LeyoChatLauncher: $needle"
        }
    }

    if (-not (Test-Path $serverTemplatePath)) {
        throw "Expected server installer template: $serverTemplatePath"
    }
    if (-not (Test-Path $serverPackageScript)) {
        throw "Expected server packaging script: $serverPackageScript"
    }
    if (-not (Test-Path $serverSfxScript)) {
        throw "Expected server SFX packaging script: $serverSfxScript"
    }

    $serverTemplate = Get-Content -Raw -Path $serverTemplatePath
    if ($serverTemplate -match 'CreateGUID') {
        throw "Server installer template must not call unsupported Inno Pascal function: CreateGUID"
    }
    foreach ($needle in @(
        'LeyoChatServer-@APP_VERSION@-setup',
        'LeyoChatServiceHost.exe',
        'LeyoChatService.exe',
        'sc.exe',
        'failure "LeyoChatService"',
        'leyochat-service.json',
        'configVersion',
        'GenerateInstallToken',
        'RandomNumberGenerator',
        '"legacyFileAccess": false',
        '"workspaces": ["'
    )) {
        if ($serverTemplate -notmatch [regex]::Escape($needle)) {
            throw "Server installer template missing expected content: $needle"
        }
    }
    foreach ($unsafeDefault in @(
        'leyochat-default-token',
        '"legacyFileAccess": true',
        '"workspaces": "*"'
    )) {
        if ($serverTemplate -match [regex]::Escape($unsafeDefault)) {
            throw "Server installer template retains unsafe default: $unsafeDefault"
        }
    }

    $serverScript = Get-Content -Raw -Path $serverPackageScript
    foreach ($needle in @(
        'LeyoChatService',
        'LeyoChatServiceHost',
        'server-latest.json',
        'LeyoChatServer'
    )) {
        if ($serverScript -notmatch [regex]::Escape($needle)) {
            throw "Server packaging script missing expected content: $needle"
        }
    }
    if ($serverScript -notmatch [regex]::Escape('--clean-first')) {
        throw "Server packaging must clean stale target objects before a release build"
    }

    $serverSfxScriptText = Get-Content -Raw -Path $serverSfxScript
    foreach ($needle in @(
        'package-server-windows.ps1',
        'LeyoChatServer',
        'server-latest.json',
        '--installer-mode',
        'server'
    )) {
        if ($serverSfxScriptText -notmatch [regex]::Escape($needle)) {
            throw "Server SFX packaging script missing expected content: $needle"
        }
    }
}
finally {
    if (Test-Path $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
