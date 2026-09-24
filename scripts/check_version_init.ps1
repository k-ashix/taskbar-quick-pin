<#
    check_version_init.ps1  --  Version / INIT consistency guard for taskbar-quick-pin.

    Runs automatically as part of every test run (invoked by run_tests.ps1).
    Can also be run standalone at any time:

        powershell -ExecutionPolicy Bypass -File scripts\check_version_init.ps1

    --------------------------------------------------------------------------
    What this checks (and why it matters)
    --------------------------------------------------------------------------
    taskbar-quick-pin.wh.cpp has TWO places that must always agree on the
    version number:

      1. The @version metadata header  (e.g.  // @version         2.5.3)
         -- this is what Windhawk reads and displays to the user.

      2. The startup INIT diagnostic    (e.g.  Wh_Log(L"INIT: v2.5.3 OK ..."))
         -- this is what appears in the Windhawk log when the mod loads.

    If they drift (e.g. metadata says 2.5.3 but INIT still says 2.5.2) the
    log is misleading and version-specific bug reports become ambiguous.

    This script:
      - Extracts the canonical version from @version
      - Verifies the INIT log line reports the SAME version
      - Verifies NO stale older-version string appears in any INIT log line
      - Exits 0 on success, 1 on any mismatch (CI-safe)

    --------------------------------------------------------------------------
    Self-test
    --------------------------------------------------------------------------
    Pass -SelfTest to run the built-in regression suite that verifies the
    detection logic itself (does not touch the real mod file):

        powershell -ExecutionPolicy Bypass -File scripts\check_version_init.ps1 -SelfTest
#>

[CmdletBinding()]
param(
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$ModFile   = Join-Path $RepoRoot "taskbar-quick-pin.wh.cpp"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
$script:fail = 0
function Ok  ($m) { Write-Host ("  [PASS] " + $m) -ForegroundColor Green }
function Bad ($m) { Write-Host ("  [FAIL] " + $m) -ForegroundColor Red; $script:fail++ }

# ---------------------------------------------------------------------------
# Core check logic (accepts raw text so the self-test can inject fake content)
# ---------------------------------------------------------------------------
function Invoke-VersionInitCheck {
    param([string]$SourceText, [string]$Label)

    Write-Host ""
    Write-Host "--- $Label ---" -ForegroundColor Cyan

    # 1. Extract @version from the WindhawkMod header block.
    $versionMatch = [regex]::Match($SourceText, '(?m)^//\s*@version\s+([\d]+\.[\d]+\.[\d]+)')
    if (-not $versionMatch.Success) {
        Bad "Could not find '// @version X.Y.Z' in source"
        return
    }
    $canonical = $versionMatch.Groups[1].Value   # e.g. "2.5.3"
    Ok "Canonical @version found: v$canonical"

    # 2. INIT log must report the SAME version.
    $initPattern = "INIT:\s*v$([regex]::Escape($canonical))"
    $initMatches = [regex]::Matches($SourceText, $initPattern)
    if ($initMatches.Count -ge 1) {
        Ok "INIT log reports v$canonical ($($initMatches.Count) occurrence(s))"
    } else {
        Bad "INIT log does NOT report v$canonical -- startup diagnostic will be wrong"
    }

    # 3. No INIT line may report a DIFFERENT (older) version.
    #    We look for any  INIT: vX.Y.Z  where X.Y.Z != canonical.
    $allInitVersions = [regex]::Matches($SourceText, 'INIT:\s*v([\d]+\.[\d]+\.[\d]+)')
    $stale = @()
    foreach ($m in $allInitVersions) {
        $found = $m.Groups[1].Value
        if ($found -ne $canonical) { $stale += "v$found" }
    }
    if ($stale.Count -eq 0) {
        Ok "No stale INIT version strings found"
    } else {
        $staleList = $stale -join ", "
        Bad "Stale INIT version(s) still present: $staleList  (expected only v$canonical)"
    }
}

# ---------------------------------------------------------------------------
# Self-test: verify the detection logic with synthetic inputs
# ---------------------------------------------------------------------------
function Invoke-SelfTest {
    Write-Host ""
    Write-Host "=== check_version_init self-test ===" -ForegroundColor White

    $script:selfFail = 0
    function ST_Ok  ($m) { Write-Host ("  [SELF-PASS] " + $m) -ForegroundColor Green }
    function ST_Bad ($m) { Write-Host ("  [SELF-FAIL] " + $m) -ForegroundColor Red; $script:selfFail++ }

    # Helper: run the check on synthetic text and return pass/fail count.
    function Run-Check ($text) {
        $before = $script:fail
        Invoke-VersionInitCheck -SourceText $text -Label "synthetic"
        $delta = $script:fail - $before
        # Reset so self-test failures don't bleed into the real fail counter.
        $script:fail = $before
        return $delta   # 0 = all checks passed, >0 = some failed
    }

    # Case 1: version and INIT agree -> should PASS (0 failures)
    $good = @"
// @version         2.5.3
Wh_Log(L"INIT: v2.5.3 OK. state=%d", s);
"@
    $d = Run-Check $good
    if ($d -eq 0) { ST_Ok "Case 1 (matching v2.5.3): correctly PASSED" }
    else          { ST_Bad "Case 1 (matching v2.5.3): expected PASS, got $d failure(s)" }

    # Case 2: INIT still says old version -> should FAIL (>=1 failure)
    $staleInit = @"
// @version         2.5.3
Wh_Log(L"INIT: v2.5.2 OK. state=%d", s);
"@
    $d = Run-Check $staleInit
    if ($d -ge 1) { ST_Ok "Case 2 (stale INIT v2.5.2 vs @version 2.5.3): correctly FAILED" }
    else          { ST_Bad "Case 2 (stale INIT v2.5.2 vs @version 2.5.3): expected FAIL, got PASS" }

    # Case 3: no @version at all -> should FAIL
    $noVersion = @"
Wh_Log(L"INIT: v2.5.3 OK. state=%d", s);
"@
    $d = Run-Check $noVersion
    if ($d -ge 1) { ST_Ok "Case 3 (missing @version): correctly FAILED" }
    else          { ST_Bad "Case 3 (missing @version): expected FAIL, got PASS" }

    # Case 4: @version present but no INIT log at all -> should FAIL
    $noInit = @"
// @version         2.5.3
// no Wh_Log INIT line here
"@
    $d = Run-Check $noInit
    if ($d -ge 1) { ST_Ok "Case 4 (no INIT log): correctly FAILED" }
    else          { ST_Bad "Case 4 (no INIT log): expected FAIL, got PASS" }

    # Case 5: both old and new INIT lines present -> should FAIL (stale detected)
    $bothInits = @"
// @version         2.5.3
Wh_Log(L"INIT: v2.5.3 OK. state=%d", s);
Wh_Log(L"INIT: v2.5.2 OK. state=%d", s);
"@
    $d = Run-Check $bothInits
    if ($d -ge 1) { ST_Ok "Case 5 (both v2.5.3 and stale v2.5.2 INIT): correctly FAILED" }
    else          { ST_Bad "Case 5 (both v2.5.3 and stale v2.5.2 INIT): expected FAIL, got PASS" }

    Write-Host ""
    if ($script:selfFail -eq 0) {
        Write-Host "SELF-TEST: PASS (all $5 cases correct)" -ForegroundColor Green
        exit 0
    } else {
        Write-Host ("SELF-TEST: FAIL ({0} case(s) wrong)" -f $script:selfFail) -ForegroundColor Red
        exit 1
    }
}

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if ($SelfTest) {
    Invoke-SelfTest
    # Invoke-SelfTest calls exit internally; this line is unreachable.
}

Write-Host "=== version / INIT consistency check ===" -ForegroundColor White

if (-not (Test-Path $ModFile)) {
    Bad "taskbar-quick-pin.wh.cpp not found at: $ModFile"
    Write-Host ""
    Write-Host "VERSION/INIT CHECK: FAIL (source file missing)" -ForegroundColor Red
    exit 1
}

$modText = Get-Content -Path $ModFile -Raw
Invoke-VersionInitCheck -SourceText $modText -Label $ModFile

Write-Host ""
if ($script:fail -eq 0) {
    Write-Host "VERSION/INIT CHECK: PASS" -ForegroundColor Green
    exit 0
} else {
    Write-Host ("VERSION/INIT CHECK: FAIL ({0} issue(s))" -f $script:fail) -ForegroundColor Red
    exit 1
}
