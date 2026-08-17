param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Test executable not found: $ExePath"
}

$logRoot = Join-Path $env:TEMP ("leyochat-qt-test-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $logRoot | Out-Null

try {
    $functionsOutput = & $ExePath -functions 2>&1
    if ($LASTEXITCODE -ne 0) {
        if ($functionsOutput) {
            $functionsOutput
        }
        exit $LASTEXITCODE
    }

    $functions =
        $functionsOutput |
        ForEach-Object { $_.Trim().TrimEnd('(', ')') } |
        Where-Object { $_ -ne "" }

    foreach ($functionName in $functions) {
        $safeName = ($functionName -replace '[^A-Za-z0-9_.-]', '_')
        $runLog = Join-Path $logRoot "$safeName.txt"
        & $ExePath $functionName -txt "-o" "$runLog,txt" | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Output "FAILED::$functionName"
            if (Test-Path -LiteralPath $runLog) {
                Get-Content -LiteralPath $runLog
            }
            exit $LASTEXITCODE
        }
    }
}
finally {
    if (Test-Path -LiteralPath $logRoot) {
        Remove-Item -LiteralPath $logRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Output "ALL-PASSED-INDIVIDUALLY"
