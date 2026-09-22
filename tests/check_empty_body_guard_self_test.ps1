[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("taskbar-empty-body-guard-{0}" -f [guid]::NewGuid().ToString("N"))

try {
    $tempScripts = Join-Path $tempRoot "scripts"
    New-Item -ItemType Directory -Path $tempScripts -Force | Out-Null
    Copy-Item (Join-Path $repoRoot "scripts\check_empty_body.ps1") $tempScripts
    $guard = Join-Path $tempScripts "check_empty_body.ps1"

    # --- Assertion 1: RED -- inject an empty-body if into the temp mod copy --
    # An `if (...)` header immediately followed by `}` (deleted body) is exactly
    # the bug that made clang report "expected statement". The guard MUST reject it.
    $tempMod = Join-Path $tempRoot "taskbar-quick-pin.wh.cpp"
    Copy-Item (Join-Path $repoRoot "taskbar-quick-pin.wh.cpp") $tempMod
    Add-Content -Path $tempMod -Value "`n// regression injection: empty-body if`n    if (!result.empty())`n    }"

    & powershell -NoProfile -ExecutionPolicy Bypass -File $guard -Quiet
    $redExit = $LASTEXITCODE
    if ($redExit -eq 0) {
        throw "guard did not catch empty-body if (expected non-zero exit, got $redExit)."
    }

    # --- Assertion 2: GREEN -- a clean temp copy must NOT be flagged ----------
    Remove-Item -Force $tempMod
    Copy-Item (Join-Path $repoRoot "taskbar-quick-pin.wh.cpp") $tempMod

    & powershell -NoProfile -ExecutionPolicy Bypass -File $guard -Quiet
    $greenExit = $LASTEXITCODE
    if ($greenExit -ne 0) {
        throw "false positive: guard flagged the clean mod source (expected exit 0, got $greenExit)."
    }

    Write-Host ("PASS: guard rejected the injected empty-body if (exit {0}) and passed the clean copy (exit {1})." -f $redExit, $greenExit) -ForegroundColor Green
    exit 0
} finally {
    if (Test-Path $tempRoot) {
        Remove-Item -Recurse -Force $tempRoot
    }
}
