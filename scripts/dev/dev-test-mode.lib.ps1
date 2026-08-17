function Resolve-DevTestBuildContext {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [string]$ExecutablePath
    )

    $scriptsRoot = Join-Path $RepoRoot 'scripts'
    . (Join-Path $scriptsRoot 'package-windows.lib.ps1')

    if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
        $ExecutablePath = Join-Path $RepoRoot 'build\LeyoChat.exe'
    }

    $resolvedExe = [System.IO.Path]::GetFullPath($ExecutablePath)
    if (-not (Test-Path -LiteralPath $resolvedExe)) {
        throw "Missing LeyoChat executable: $resolvedExe"
    }

    $buildDir = Split-Path -Parent $resolvedExe
    $cmakeCache = Join-Path $buildDir 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cmakeCache)) {
        $cmakeCache = Join-Path (Split-Path -Parent $buildDir) 'CMakeCache.txt'
    }
    if (-not (Test-Path -LiteralPath $cmakeCache)) {
        throw "Missing CMake cache near executable: $resolvedExe"
    }

    $qtBinDir = Resolve-QtBinDirFromCache -CMakeCachePath $cmakeCache
    $compilerBinDir = Resolve-CompilerBinDirFromCache -CMakeCachePath $cmakeCache
    if ([string]::IsNullOrWhiteSpace($qtBinDir) -or -not (Test-Path -LiteralPath $qtBinDir)) {
        throw "Cannot resolve Qt bin dir from: $cmakeCache"
    }

    $qtRoot = Split-Path -Parent $qtBinDir
    $qtPluginsDir = Join-Path $qtRoot 'plugins'

    [PSCustomObject]@{
        ExecutablePath = $resolvedExe
        BuildDir = $buildDir
        CMakeCache = $cmakeCache
        QtBinDir = $qtBinDir
        QtPluginsDir = $qtPluginsDir
        CompilerBinDir = $compilerBinDir
    }
}

function Enter-DevTestRuntimeEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$BuildContext
    )

    $pathEntries = @()
    if (-not [string]::IsNullOrWhiteSpace($BuildContext.CompilerBinDir) -and (Test-Path -LiteralPath $BuildContext.CompilerBinDir)) {
        $pathEntries += [System.IO.Path]::GetFullPath($BuildContext.CompilerBinDir)
    }
    $pathEntries += [System.IO.Path]::GetFullPath($BuildContext.QtBinDir)

    $existingPath = ($env:PATH -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    $mergedPath = @($pathEntries + $existingPath | Select-Object -Unique)
    $env:PATH = ($mergedPath -join ';')

    if (Test-Path -LiteralPath $BuildContext.QtPluginsDir) {
        $env:QT_PLUGIN_PATH = [System.IO.Path]::GetFullPath($BuildContext.QtPluginsDir)
    }
}
