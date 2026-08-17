function Convert-CMakePathToWindows {
    param(
        [AllowNull()]
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    return ($Path.Trim() -replace '/', '\')
}

function Get-CMakeCacheValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CMakeCachePath,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    if (!(Test-Path $CMakeCachePath)) {
        throw "CMake cache not found: $CMakeCachePath"
    }

    $pattern = '^{0}(?::[^=]+)?=(.*)$' -f [regex]::Escape($Key)
    $match = Select-String -Path $CMakeCachePath -Pattern $pattern | Select-Object -First 1
    if (-not $match) {
        return $null
    }

    return $match.Matches[0].Groups[1].Value.Trim()
}

function Resolve-QtBinDirFromCache {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CMakeCachePath
    )

    $qtDir = Get-CMakeCacheValue -CMakeCachePath $CMakeCachePath -Key "Qt6_DIR"
    if ($qtDir) {
        $qt6Dir = Convert-CMakePathToWindows $qtDir
        $qtRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $qt6Dir))
        return (Join-Path $qtRoot "bin")
    }

    $prefixPath = Get-CMakeCacheValue -CMakeCachePath $CMakeCachePath -Key "CMAKE_PREFIX_PATH"
    if ($prefixPath) {
        $firstPrefix = ($prefixPath -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -First 1)
        if ($firstPrefix) {
            return (Join-Path (Convert-CMakePathToWindows $firstPrefix) "bin")
        }
    }

    return $null
}

function Resolve-CompilerBinDirFromCache {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CMakeCachePath
    )

    $compilerPath = Get-CMakeCacheValue -CMakeCachePath $CMakeCachePath -Key "CMAKE_CXX_COMPILER"
    if (-not $compilerPath) {
        return $null
    }

    $compilerPath = Convert-CMakePathToWindows $compilerPath
    return Split-Path -Parent $compilerPath
}

function Resolve-MsvcRuntimeDirFromCompilerPath {
    param(
        [AllowNull()]
        [string]$CompilerPath
    )

    if ([string]::IsNullOrWhiteSpace($CompilerPath)) {
        return $null
    }

    $compilerPath = Convert-CMakePathToWindows $CompilerPath
    if ($compilerPath -notmatch 'MSVC[/\\]([^/\\]+)[/\\]bin') {
        return $null
    }

    $msvcVer = $Matches[1]
    $vsRoot = ($compilerPath -replace 'VC[/\\]Tools[/\\].*$', '')
    $vsRoot = Convert-CMakePathToWindows $vsRoot
    $redistRoot = Join-Path $vsRoot "VC\Redist\MSVC"

    $preferredRoot = Join-Path $redistRoot "$msvcVer\x64"
    if (Test-Path $preferredRoot) {
        $preferred = Get-ChildItem -LiteralPath $preferredRoot -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            Select-Object -First 1
        if ($preferred) {
            return $preferred.FullName
        }
    }

    if (-not (Test-Path $redistRoot)) {
        return $null
    }

    $fallback = Get-ChildItem -LiteralPath $redistRoot -Directory -Recurse -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -match '[\\/]+x64[\\/]Microsoft\.VC[^\\/]+\.CRT$' -and
            $_.FullName -notmatch '[\\/]debug_nonredist[\\/]' -and
            $_.FullName -notmatch '[\\/]onecore[\\/]'
        } |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if ($fallback) {
        return $fallback.FullName
    }

    return $null
}

function Copy-MsvcRuntimeDllsFromCache {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CMakeCachePath,
        [Parameter(Mandatory = $true)]
        [string]$DestinationDir
    )

    $compilerPath = Get-CMakeCacheValue -CMakeCachePath $CMakeCachePath -Key "CMAKE_CXX_COMPILER"
    $crtDir = Resolve-MsvcRuntimeDirFromCompilerPath -CompilerPath $compilerPath
    if (-not $crtDir) {
        return $false
    }

    $runtimeDlls = Get-ChildItem -LiteralPath $crtDir -File -Filter "*.dll" -ErrorAction Stop
    if (-not $runtimeDlls) {
        return $false
    }

    if (-not (Test-Path -LiteralPath $DestinationDir)) {
        New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
    }

    Write-Host "==> Copy MSVC runtime from: $crtDir"
    foreach ($dll in $runtimeDlls) {
        Copy-Item -LiteralPath $dll.FullName -Destination $DestinationDir -Force -ErrorAction Stop
    }
    return $true
}

function Reset-AppVersionBuildArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildPath
    )

    $objectPath = Join-Path $BuildPath "CMakeFiles\LeyoChat.dir\src\app\ApplicationInfo.cpp.obj"
    $removed = @()
    if (Test-Path -LiteralPath $objectPath) {
        Remove-Item -LiteralPath $objectPath -Force
        $removed += $objectPath
    }

    return $removed
}

function Find-BytePattern {
    param(
        [Parameter(Mandatory = $true)]
        [byte[]]$Bytes,
        [Parameter(Mandatory = $true)]
        [byte[]]$Pattern,
        [int]$StartAt = 0
    )

    if ($Pattern.Length -eq 0 -or $Bytes.Length -lt $Pattern.Length) {
        return -1
    }

    for ($i = [Math]::Max(0, $StartAt); $i -le $Bytes.Length - $Pattern.Length; ++$i) {
        $matched = $true
        for ($j = 0; $j -lt $Pattern.Length; ++$j) {
            if ($Bytes[$i + $j] -ne $Pattern[$j]) {
                $matched = $false
                break
            }
        }
        if ($matched) {
            return $i
        }
    }

    return -1
}

function Test-BinaryContainsUtf16StringsInOrder {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BinaryPath,
        [Parameter(Mandatory = $true)]
        [string[]]$Strings
    )

    if (-not (Test-Path -LiteralPath $BinaryPath)) {
        return $false
    }

    $bytes = [System.IO.File]::ReadAllBytes($BinaryPath)
    $offset = 0
    foreach ($value in $Strings) {
        $pattern = [System.Text.Encoding]::Unicode.GetBytes($value)
        $foundAt = Find-BytePattern -Bytes $bytes -Pattern $pattern -StartAt $offset
        if ($foundAt -lt 0) {
            return $false
        }
        $offset = $foundAt + $pattern.Length
    }

    return $true
}

function Assert-AppExecutableRuntimeVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExecutablePath,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedVersion
    )

    $companyName = "LeXiaoYao2704"
    $expectedRuntimeStrings = @(
        "LeyoChat",
        $companyName,
        $ExpectedVersion,
        ":/docs/release-notes/current.txt"
    )
    if (-not (Test-BinaryContainsUtf16StringsInOrder -BinaryPath $ExecutablePath -Strings $expectedRuntimeStrings)) {
        throw "LeyoChat runtime version in $ExecutablePath does not match expected version $ExpectedVersion. Rebuild ApplicationInfo.cpp before packaging."
    }
}

function Resolve-WindeployQtPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CMakeCachePath
    )

    $qtBinDir = Resolve-QtBinDirFromCache -CMakeCachePath $CMakeCachePath
    if ($qtBinDir) {
        $candidate = Join-Path $qtBinDir "windeployqt.exe"
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $command = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    return $null
}

function Get-ProjectVersionFromCache {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CMakeCachePath
    )

    $version = Get-CMakeCacheValue -CMakeCachePath $CMakeCachePath -Key "CMAKE_PROJECT_VERSION"
    if ($version) {
        return $version
    }

    return "0.1.0"
}
