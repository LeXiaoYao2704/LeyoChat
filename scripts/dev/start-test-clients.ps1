[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$ExecutablePath,
    [string]$DataRoot,
    [int]$BasePort = 45454
)

$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptRoot)
. (Join-Path $scriptRoot 'dev-test-mode.lib.ps1')

$buildContext = Resolve-DevTestBuildContext -RepoRoot $repoRoot -ExecutablePath $ExecutablePath
Enter-DevTestRuntimeEnvironment -BuildContext $buildContext
$ExecutablePath = $buildContext.ExecutablePath

if ([string]::IsNullOrWhiteSpace($DataRoot)) {
    $DataRoot = Join-Path $repoRoot '.sandbox\dev-test-clients'
}
$DataRoot = [System.IO.Path]::GetFullPath($DataRoot)
New-Item -ItemType Directory -Force -Path $DataRoot | Out-Null

$profiles = @(
    @{ Profile = 'client-a'; Port = $BasePort + 0; ClientId = 'dev-client-a'; DisplayName = 'Dev-A' },
    @{ Profile = 'client-b'; Port = $BasePort + 1; ClientId = 'dev-client-b'; DisplayName = 'Dev-B' },
    @{ Profile = 'client-c'; Port = $BasePort + 2; ClientId = 'dev-client-c'; DisplayName = 'Dev-C' },
    @{ Profile = 'client-d'; Port = $BasePort + 3; ClientId = 'dev-client-d'; DisplayName = 'Dev-D' }
)

function Get-DevTestClientProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Profile
    )

    $pattern = [regex]::Escape($Profile)
    Get-CimInstance Win32_Process -Filter "Name='LeyoChat.exe'" | Where-Object {
        $_.CommandLine -and $_.CommandLine -match "--dev-test-profile(?:=|\s+)$pattern(\s|$)"
    }
}

foreach ($entry in $profiles) {
    $profileRoot = Join-Path $DataRoot $entry.Profile
    New-Item -ItemType Directory -Force -Path $profileRoot | Out-Null

    $existing = @(Get-DevTestClientProcess -Profile $entry.Profile)
    if ($existing.Count -gt 0) {
        Write-Host "Skip $($entry.Profile): already running."
        continue
    }

    $arguments = @(
        '--dev-test-profile', $entry.Profile,
        '--dev-test-data-root', $DataRoot,
        '--dev-test-port', "$($entry.Port)",
        '--dev-test-client-id', $entry.ClientId,
        '--dev-test-display-name', $entry.DisplayName
    )

    if ($PSCmdlet.ShouldProcess($entry.Profile, 'Start development test client')) {
        Start-Process -FilePath $ExecutablePath -ArgumentList $arguments | Out-Null
        Write-Host "Started $($entry.Profile) port=$($entry.Port) dataRoot=$DataRoot"
    }
}
