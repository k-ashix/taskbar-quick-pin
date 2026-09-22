[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("taskbar-log-guard-{0}" -f [guid]::NewGuid().ToString("N"))

try {
    $tempScripts = Join-Path $tempRoot "scripts"
    New-Item -ItemType Directory -Path $tempScripts -Force | Out-Null
    Copy-Item (Join-Path $repoRoot "scripts\check_log_layer.ps1") $tempScripts
    $tempMod = Join-Path $tempRoot "taskbar-quick-pin.wh.cpp"
    Copy-Item (Join-Path $repoRoot "taskbar-quick-pin.wh.cpp") $tempMod
    Add-Content -Path $tempMod -Value "`n// regression injection`nbool g_debugLogging = true;"

    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $tempScripts "check_log_layer.ps1") -Quiet
    $guardExit = $LASTEXITCODE
    if ($guardExit -eq 0) {
        throw "Expected the copied log-layer guard to reject the injected banned symbol."
    }

    Write-Host ("PASS: copied guard rejected the isolated injected source (exit {0})." -f $guardExit) -ForegroundColor Green
    exit 0
} finally {
    if (Test-Path $tempRoot) {
        Remove-Item -Recurse -Force $tempRoot
    }
}
