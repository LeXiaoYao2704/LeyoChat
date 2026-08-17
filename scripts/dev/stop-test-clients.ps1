[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string[]]$Profiles = @('client-a', 'client-b', 'client-c', 'client-d')
)

$ErrorActionPreference = 'Stop'

function Get-DevTestClientProcesses {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$ProfileList
    )

    $escapedProfiles = $ProfileList | ForEach-Object { [regex]::Escape($_) }
    Get-CimInstance Win32_Process -Filter "Name='LeyoChat.exe'" | Where-Object {
        if (-not $_.CommandLine) {
            return $false
        }

        foreach ($profile in $escapedProfiles) {
            if ($_.CommandLine -match "--dev-test-profile(?:=|\s+)$profile(\s|$)") {
                return $true
            }
        }

        return $false
    }
}

$targets = @(Get-DevTestClientProcesses -ProfileList $Profiles)
if ($targets.Count -eq 0) {
    Write-Host 'No development test clients found.'
    exit 0
}

foreach ($process in $targets) {
    $name = if ($process.CommandLine -match "--dev-test-profile(?:=|\s+)([A-Za-z0-9._-]+)") {
        $Matches[1]
    } else {
        "pid-$($process.ProcessId)"
    }

    if ($PSCmdlet.ShouldProcess($name, "Stop development test client pid=$($process.ProcessId)")) {
        Stop-Process -Id $process.ProcessId -Force
        Write-Host "Stopped $name pid=$($process.ProcessId)"
    }
}
