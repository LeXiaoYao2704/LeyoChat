<#
.SYNOPSIS
    在客户端 Inno Setup 安装器外面包一层 Qt Quick UI，生成最终客户端单 exe。
.DESCRIPTION
    1. 调用 package-windows.ps1 生成客户端 Inno Setup 安装器
    2. 编译 LeyoChatSetup.exe（Qt Quick UI 皮肤，不含安装逻辑）
    3. 用 windeployqt 收集 Qt Quick 运行时
    4. 将 Qt Quick UI + 客户端内层安装器打入 SFX 外壳
    5. 生成 latest.json
.PARAMETER BuildDir
    构建目录名（相对于项目根目录），默认 build-msvc
#>
param(
    [string]$BuildDir = 'build-msvc'
)

$ErrorActionPreference = 'Stop'

$scriptDir     = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspaceRoot = Split-Path -Parent $scriptDir
. (Join-Path $scriptDir 'package-windows.lib.ps1')

$buildPath  = Join-Path $workspaceRoot $BuildDir
$cmakeCache = Join-Path $buildPath 'CMakeCache.txt'
$releaseNotesFile = Join-Path $workspaceRoot 'release-notes\installer-release-notes.txt'

if (!(Test-Path $cmakeCache)) {
    throw "CMake cache not found: $cmakeCache"
}

$cmakeExe = Join-Path $workspaceRoot 'CMake\bin\cmake.exe'
if (!(Test-Path $cmakeExe)) { $cmakeExe = 'cmake' }

$windeployqt = Resolve-WindeployQtPath -CMakeCachePath $cmakeCache
if (-not $windeployqt) { throw 'Cannot find windeployqt.exe.' }

$appVersion = Get-ProjectVersionFromCache -CMakeCachePath $cmakeCache

Write-Host '================================================='
Write-Host '  LeyoChat Installer (Qt Quick UI + Inno Setup)'
Write-Host "  Version: $appVersion"
Write-Host '================================================='

# ━━ 第一步：调用 package-windows.ps1 生成客户端 Inno Setup 安装器 ━━━━━━━
Write-Host ''
Write-Host '==> Step 1: Build client Inno Setup installer via package-windows.ps1'
$origScript = Join-Path $scriptDir 'package-windows.ps1'
if (!(Test-Path $origScript)) {
    throw "Cannot find original packaging script: $origScript"
}
& $origScript -BuildDir $BuildDir
if ($LASTEXITCODE -ne 0) { throw 'package-windows.ps1 failed.' }

# 找到生成的客户端内层安装器
$origOutputDir = Join-Path $buildPath 'package\installer'
$origInstaller = Get-ChildItem -Path $origOutputDir -Filter '*.exe' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $origInstaller) {
    throw "Client inner installer exe not found in: $origOutputDir"
}
Write-Host "    Client inner installer: $($origInstaller.FullName)"

# ━━ 第二步：编译 Qt Quick UI ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Write-Host ''
Write-Host '==> Step 2: Build LeyoChatSetup (Qt Quick UI)'
& $cmakeExe --build $buildPath --config Release --target LeyoChatSetup
if ($LASTEXITCODE -ne 0) { throw 'LeyoChatSetup build failed.' }

$setupExeSrc = Join-Path $buildPath 'installer\LeyoChatSetup.exe'
if (!(Test-Path $setupExeSrc)) {
    throw "LeyoChatSetup.exe not found: $setupExeSrc"
}

# ━━ 第三步：打包 SFX 目录（仅 Qt Quick UI + DLLs，不含 inner-setup） ━━━━━
Write-Host ''
Write-Host '==> Step 3: Stage SFX package directory (UI only, no inner-setup)'
$sfxPkgDir = Join-Path $buildPath 'package\sfx-package'
if (Test-Path $sfxPkgDir) {
    Remove-Item -LiteralPath $sfxPkgDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $sfxPkgDir | Out-Null

# 3a. 复制 Qt Quick UI
Copy-Item -LiteralPath $setupExeSrc -Destination $sfxPkgDir -Force
$setupExeDst = Join-Path $sfxPkgDir 'LeyoChatSetup.exe'

# 3b. windeployqt 收集 Qt Quick 运行时
$qmlDir = Join-Path $workspaceRoot 'installer\qml'
Write-Host '    windeployqt for LeyoChatSetup (Qt Quick)'
& $windeployqt --release --no-translations --force --qmldir $qmlDir $setupExeDst
if ($LASTEXITCODE -ne 0) { Write-Warning 'windeployqt for UI returned non-zero' }

# 3c. 清理安装器不需要的文件（仅保留 Basic 样式，删除软件渲染和 IDE 文件）
Write-Host '    Stripping unnecessary files from UI package...'
$stripFiles = @(
    'opengl32sw.dll',        # 软件 OpenGL 渲染器（~20MB，现代显卡不需要）
    'D3Dcompiler_47.dll'     # D3D 着色器编译器（安装器无 3D 渲染）
)
$stripDllPatterns = @(
    'Qt6QuickControls2Imagine*.dll',
    'Qt6QuickControls2Material*.dll',
    'Qt6QuickControls2Fusion*.dll',
    'Qt6QuickControls2Universal*.dll'
)
$stripQmlDirs = @(
    'Imagine', 'Material', 'Fusion', 'Universal', 'NativeStyle'
)
$stripped = 0; $strippedBytes = 0

# 删除不需要的顶层文件
foreach ($name in $stripFiles) {
    $f = Join-Path $sfxPkgDir $name
    if (Test-Path $f) {
        $strippedBytes += (Get-Item $f).Length; $stripped++
        Remove-Item $f -Force
    }
}

# 删除未使用的样式 DLL
foreach ($pat in $stripDllPatterns) {
    Get-ChildItem $sfxPkgDir -Filter $pat -ErrorAction SilentlyContinue | ForEach-Object {
        $strippedBytes += $_.Length; $stripped++
        Remove-Item $_.FullName -Force
    }
}

# 删除未使用的样式 QML 目录
$controlsDir = Join-Path $sfxPkgDir 'qml\QtQuick\Controls'
if (Test-Path $controlsDir) {
    foreach ($style in $stripQmlDirs) {
        $d = Join-Path $controlsDir $style
        if (Test-Path $d) {
            $fs = Get-ChildItem $d -Recurse -File
            $strippedBytes += ($fs | Measure-Object Length -Sum).Sum
            $stripped += $fs.Count
            Remove-Item $d -Recurse -Force
        }
    }
}

# 删除 .qmltypes 文件（仅 Qt Creator IDE 设计器需要，运行时不需要）
Get-ChildItem $sfxPkgDir -Recurse -Filter '*.qmltypes' | ForEach-Object {
    $strippedBytes += $_.Length; $stripped++
    Remove-Item $_.FullName -Force
}

Write-Host "    Stripped $stripped files, saved $([math]::Round($strippedBytes/1MB,1)) MB"

# 3e. 复制 MSVC CRT DLLs（inner-setup 不放进 zip，改为后面 raw 附加）
$compilerPath = Get-CMakeCacheValue -CMakeCachePath $cmakeCache -Key 'CMAKE_CXX_COMPILER'
if ($compilerPath -and $compilerPath -match 'MSVC[/\\]([^/\\]+)[/\\]bin') {
    $msvcVer = $Matches[1]
    $vsRoot = ($compilerPath -replace 'VC[/\\]Tools[/\\].*$', '')
    $crtDir = Join-Path (Convert-CMakePathToWindows $vsRoot) "VC\Redist\MSVC\$msvcVer\x64\Microsoft.VC142.CRT"
    if (Test-Path $crtDir) {
        Write-Host '    Copy MSVC runtime DLLs'
        Copy-Item (Join-Path $crtDir '*.dll') $sfxPkgDir -Force
    }
}

# 3f. 清理不需要的 vc_redist
$vcRedist = Join-Path $sfxPkgDir 'vc_redist.x64.exe'
if (Test-Path $vcRedist) { Remove-Item $vcRedist -Force }

$uiZipSizeMB = [math]::Round((Get-ChildItem $sfxPkgDir -Recurse | Measure-Object Length -Sum).Sum / 1MB, 1)
Write-Host "    UI package size: ~${uiZipSizeMB} MB (before zip)"

# ━━ 第四步：构建自解压安装包（两段式布局）━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Write-Host ''
Write-Host '==> Step 4: Build self-extracting installer (two-phase layout)'

$sfxOutputDir = Join-Path $buildPath 'package\installer-output'
New-Item -ItemType Directory -Force -Path $sfxOutputDir | Out-Null

# 4a. 编译 SfxStub
Write-Host '    Building SfxStub...'
& $cmakeExe --build $buildPath --config Release --target SfxStub
if ($LASTEXITCODE -ne 0) { throw 'SfxStub build failed.' }

$sfxStubExe = Join-Path $buildPath 'installer\SfxStub.exe'
if (!(Test-Path $sfxStubExe)) { throw "SfxStub.exe not found: $sfxStubExe" }

# 4b. 创建 ui.bundle（自定义格式，无压缩，纯顺序读写，无需子进程解压）
#    格式：magic(4) + num_files(4) + per file: [pathLen(2)][path(utf16le)][dataSize(4)][data]
$bundleFile = Join-Path $buildPath 'package\sfx-payload.bundle'
if (Test-Path $bundleFile) { Remove-Item $bundleFile -Force }
Write-Host '    Creating ui.bundle (custom format, no compression, no subprocess needed)...'

$uiFiles = Get-ChildItem -Path $sfxPkgDir -Recurse -File | Sort-Object FullName
$bundleStream = [System.IO.File]::Create($bundleFile)
$bw = [System.IO.BinaryWriter]::new($bundleStream, [System.Text.Encoding]::Unicode, $true)

# magic "HCBL" = 0x4C424348
$bw.Write([uint32]0x4C424348)
$bw.Write([uint32]$uiFiles.Count)

foreach ($f in $uiFiles) {
    $relPath = $f.FullName.Substring($sfxPkgDir.TrimEnd('\').Length + 1)
    $pathLen = [uint16]$relPath.Length
    $bw.Write($pathLen)
    $bw.Write([System.Text.Encoding]::Unicode.GetBytes($relPath))
    $data = [System.IO.File]::ReadAllBytes($f.FullName)
    $bw.Write([uint32]$data.Length)
    $bw.Write($data)
}
$bw.Flush(); $bw.Dispose()
$bundleStream.Flush(); $bundleStream.Dispose()

$uiBundleSize = (Get-Item $bundleFile).Length
Write-Host "    ui.bundle: $([math]::Round($uiBundleSize/1MB,1)) MB  ($($uiFiles.Count) files, no compression)"

# 4c. 拼接：SfxStub.exe + ui.bundle + inner-setup(raw) + 12字节尾部
#    尾部格式：uiBundleSize(4) + innerSetupSize(4) + magic(4)  共 12 字节
$finalExePath = Join-Path $sfxOutputDir "LeyoChat-$appVersion-setup.exe"
Write-Host '    Assembling final exe (stub + bundle + inner-setup-raw + trailer)...'

$innerSetupSize = (Get-Item $origInstaller.FullName).Length

$stubBytes   = [System.IO.File]::ReadAllBytes($sfxStubExe)
$bundleBytes = [System.IO.File]::ReadAllBytes($bundleFile)

# 12字节尾部
$trailer = [byte[]]::new(12)
[System.BitConverter]::GetBytes([uint32]$uiBundleSize).CopyTo($trailer, 0)
[System.BitConverter]::GetBytes([uint32]$innerSetupSize).CopyTo($trailer, 4)
[System.BitConverter]::GetBytes([uint32]0x48435346).CopyTo($trailer, 8)

$output = [System.IO.File]::Create($finalExePath)
try {
    # stub
    $output.Write($stubBytes, 0, $stubBytes.Length)
    # ui.bundle
    $output.Write($bundleBytes, 0, $bundleBytes.Length)
    # inner-setup (stream, 不一次性读入内存)
    $innerStream = [System.IO.File]::OpenRead($origInstaller.FullName)
    $buf = [byte[]]::new(1MB)
    $rem = $innerStream.Length
    while ($rem -gt 0) {
        $n = $innerStream.Read($buf, 0, [Math]::Min($buf.Length, $rem))
        $output.Write($buf, 0, $n)
        $rem -= $n
    }
    $innerStream.Close()
    # 12字节尾部
    $output.Write($trailer, 0, $trailer.Length)
} finally {
    $output.Close()
}

$finalExe = Get-Item $finalExePath
Write-Host "    Stub:        $([math]::Round($stubBytes.Length/1KB, 0)) KB"
Write-Host "    ui.bundle:   $([math]::Round($uiBundleSize/1MB, 1)) MB (no compression)"
Write-Host "    inner-setup: $([math]::Round($innerSetupSize/1MB, 1)) MB (raw)"

# ━━ 第五步：生成 latest.json ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
$sha256 = (Get-FileHash -Path $finalExe.FullName -Algorithm SHA256).Hash.ToLower()
$releaseNotesText = [System.IO.File]::ReadAllText($releaseNotesFile, [System.Text.Encoding]::UTF8)
$latestObj = @{
    version      = $appVersion
    file         = $finalExe.Name
    sha256       = $sha256
    releaseNotes = $releaseNotesText
    minVersion   = ''
}
$latestJson = $latestObj | ConvertTo-Json -Depth 1
$latestJsonPath = Join-Path $sfxOutputDir 'latest.json'
$latestJson | Out-File -FilePath $latestJsonPath -Encoding utf8
Write-Host "latest.json -> $latestJsonPath"

# ━━ 第六步：归档 PDB 符号文件（崩溃分析必需） ━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Write-Host ''
Write-Host '==> Step 6: Archive PDB symbol files'
$pdbDir = Join-Path $sfxOutputDir 'symbols'
if (Test-Path -LiteralPath $pdbDir) {
    Remove-Item -LiteralPath $pdbDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $pdbDir | Out-Null

# 收集主程序和自编译模块的 PDB
$pdbPatterns = @('LeyoChat.pdb', 'LeyoChatLauncher.pdb', 'LeyoChatSetup.pdb', 'SfxStub.pdb', 'ElaWidgetTools*.pdb')
$pdbCollected = 0
foreach ($pat in $pdbPatterns) {
    Get-ChildItem -Path $buildPath -Recurse -Filter $pat -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notlike "$pdbDir*" } |
        ForEach-Object {
            Copy-Item $_.FullName -Destination $pdbDir -Force
            $pdbCollected++
            Write-Host "    + $($_.Name)"
        }
}
if ($pdbCollected -eq 0) {
    Write-Warning 'No PDB files found. Ensure build was done with debug symbols (/Zi).'
} else {
    Write-Host "    Archived $pdbCollected PDB file(s) -> $pdbDir"
}
if (-not (Test-Path (Join-Path $pdbDir 'LeyoChatLauncher.pdb'))) {
    throw 'LeyoChatLauncher.pdb was not archived.'
}

# ━━ Summary ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
$finalSize = [math]::Round($finalExe.Length / 1MB, 1)
Write-Host ''
Write-Host '================================================='
Write-Host '  Single-exe installer created!'
Write-Host "  $($finalExe.FullName)"
Write-Host "  Size: ${finalSize} MB"
Write-Host '================================================='
Write-Host ''
Write-Host '  Architecture:'
Write-Host '    SFX outer shell -> extracts to temp'
Write-Host '      -> LeyoChatSetup.exe (Qt Quick UI, visual only)'
Write-Host '      -> LeyoChat-inner-setup.exe (client Inno Setup, all install logic)'
Write-Host '  Symbols: (for crash analysis)'
Write-Host "    $pdbDir"
Write-Host '================================================='
