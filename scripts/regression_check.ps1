<#
    regression_check.ps1  --  One-command regression gate for taskbar-quick-pin.

    Location: scripts\. Run from anywhere:

        powershell -ExecutionPolicy Bypass -File scripts\regression_check.ps1

    --------------------------------------------------------------------------
    What this does (and why it exists on top of run_tests.ps1)
    --------------------------------------------------------------------------
    taskbar-quick-pin.wh.cpp is a Windhawk mod: it #includes Win32/Windhawk
    headers and has no standalone main(), so the FULL translation unit cannot be
    compiled or executed here. run_tests.ps1 covers every pure decision that was
    MIRRORED into tests\*.h. But some fixes are STRUCTURAL properties of the mod
    source itself (an include must be first; a deleted logging layer must stay
    deleted; a busy-loop guard must stay present). A unit test cannot see those,
    so a regression there would slip through.

    This script is the regression gate for BOTH kinds of check:

      PART 1  Run the entire unit-test suite (delegates to run_tests.ps1).
              Any build/test failure fails the gate.

      PART 2  Assert source-level invariants over taskbar-quick-pin.wh.cpp --
              the things previous rounds fixed that must never silently come
              back. Each is a named MUST / MUST-NOT check with a clear message.

    Exit code is 0 only when EVERYTHING passes, so it drops straight into CI
    (see .github/workflows if present) as a required status check.

    Options:
        -SkipUnitTests   run only the source-invariant checks (PART 2)
        -Compiler g++    forwarded to run_tests.ps1 (default: g++)
        -Std c++17       forwarded to run_tests.ps1 (default: c++17)
#>

[CmdletBinding()]
param(
    [switch]$SkipUnitTests,
    [string]$Compiler = "g++",
    [string]$Std = "c++17"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$ModFile   = Join-Path $RepoRoot "taskbar-quick-pin.wh.cpp"
$ReadmeFile = Join-Path $RepoRoot "README.md"

$fail = 0
function Ok   ($m) { Write-Host ("  [PASS] " + $m) -ForegroundColor Green }
function Bad  ($m) { Write-Host ("  [FAIL] " + $m) -ForegroundColor Red; $script:fail++ }
function Head ($m) { Write-Host ""; Write-Host $m -ForegroundColor Cyan }

# Count regex matches (single-line) across the whole mod source.
$modText = Get-Content -Path $ModFile -Raw
function CountMatches ($pattern) {
    return ([regex]::Matches($modText, $pattern)).Count
}
# A "MUST NOT appear at all" check.
function AssertAbsent ($label, $pattern) {
    $n = CountMatches $pattern
    if ($n -eq 0) { Ok ("{0} -- absent (0)" -f $label) }
    else          { Bad ("{0} -- expected 0, found {1}" -f $label, $n) }
}
# A "MUST appear at least once" check.
function AssertPresent ($label, $pattern) {
    $n = CountMatches $pattern
    if ($n -ge 1) { Ok ("{0} -- present ({1})" -f $label, $n) }
    else          { Bad ("{0} -- expected >=1, found 0" -f $label) }
}

Write-Host "=== taskbar-quick-pin regression gate ===" -ForegroundColor White

# --------------------------------------------------------------------------
# PART 1  --  full unit-test suite
# --------------------------------------------------------------------------
if (-not $SkipUnitTests) {
    Head "PART 1: unit-test suite (run_tests.ps1)"
    $runner = Join-Path $ScriptDir "run_tests.ps1"
    if (-not (Test-Path $runner)) {
        Bad "run_tests.ps1 not found -- cannot run unit tests"
    } else {
        # Run ONLY the unit tests here. run_tests.ps1 now auto-chains the
        # balance check, the log-layer guard, AND this regression gate; if we
        # let it do that it would re-invoke THIS script -> infinite recursion.
        # So suppress all of its post-test sub-checks. This gate runs its own
        # source invariants (PART 2) directly below. Forward compiler/std.
        & powershell -ExecutionPolicy Bypass -File $runner `
            -SkipBalanceCheck -SkipLogLayerCheck -SkipRegressionCheck `
            -Compiler $Compiler -Std $Std
        if ($LASTEXITCODE -eq 0) { Ok "unit-test suite passed" }
        else                     { Bad ("unit-test suite failed (exit {0})" -f $LASTEXITCODE) }
    }
} else {
    Head "PART 1: unit-test suite  (skipped via -SkipUnitTests)"
}

# --------------------------------------------------------------------------
# PART 2  --  source-level invariants (things prior fixes established)
# --------------------------------------------------------------------------

Head "PART 2a: header ordering (initguid must be first include)"
# <initguid.h> defines DEFINE_GUID to EMIT storage; it must precede every other
# header or the CLSID/IID GUIDs get declared (extern) before it, breaking linkage.
$firstInclude = [regex]::Match($modText, '(?m)^\s*#include\s*[<"][^>"]+[>"]')
if ($firstInclude.Success -and $firstInclude.Value -match 'initguid\.h') {
    Ok "first #include is <initguid.h>"
} else {
    Bad ("first #include is NOT <initguid.h> (was: '{0}')" -f ($firstInclude.Value).Trim())
}

Head "PART 2b: redundant logging layer stays deleted"
# The second in-mod logging switch (on top of Windhawk's own Wh_Log gate) was
# removed. None of these may reappear.
AssertAbsent "g_debugLogging global/flag" 'g_debugLogging'
AssertAbsent "DEBUG_LOG macro/call"       '\bDEBUG_LOG\b'
AssertAbsent "TRACE_LOG macro/call"       '\bTRACE_LOG\b'
AssertAbsent "LOG_ERROR macro/call"       '\bLOG_ERROR\b'
AssertAbsent "LOG_IMPORTANT macro/call"   '\bLOG_IMPORTANT\b'
AssertAbsent "LOG_RATE macro/call"        '\bLOG_RATE\b'
AssertAbsent "DragDebugLog helper/call"        '\bDragDebugLog\b'
AssertAbsent "DragTraceVerboseLog helper/call" '\bDragTraceVerboseLog\b'
AssertAbsent "debugLogging setting key"        'debugLogging'
# The one intentional state-transition helper stays.
AssertPresent "DragTraceLog (kept) helper/calls" '\bDragTraceLog\b'
# Logging must go straight to Wh_Log now.
AssertPresent "Wh_Log direct calls" '\bWh_Log\s*\('

Head "PART 2c: Issue 2 -- unsupported-layout branch is not a busy loop"
# The branch must park on the exit event and release the hi-res timer, not
# fall straight back to the 0ms poll at the loop head.
AssertPresent "unsupported branch waits on g_exitEvent" 'g_layoutUnsupported[\s\S]{0,120}WaitForSingleObject\(g_exitEvent'
AssertPresent "unsupported branch releases hi-res timer" 'g_layoutUnsupported[\s\S]{0,120}SetHighResTimer\(false\)'

Head "PART 2d: Issue 3 -- visibility gated; nothing shown 'first'"
# RepositionOverlay must gate on ownership before showing anything.
AssertPresent "RepositionOverlay guards on g_dockOwnershipDecided" 'RepositionOverlay\(\)[\s\S]{0,400}g_dockOwnershipDecided'
# DISOWN worker branch tears the UI thread down.
AssertPresent "DISOWN posts WM_QUIT to UI thread" 'QP_STARTUP_DISOWN[\s\S]{0,900}PostThreadMessageW\(g_uiThreadId,\s*WM_QUIT'
# Windows 11 build gate present in Wh_ModInit.
AssertPresent "Windows 11 (build >= 22000) init gate" '22000'
AssertPresent "QpIsWindows11OrGreater helper"          'QpIsWindows11OrGreater'

Head "PART 2e: README documents the (now Windhawk-native) logging toggle"
if (Test-Path $ReadmeFile) {
    $readme = Get-Content -Path $ReadmeFile -Raw
    if ($readme -match 'Verbose debug logging') { Ok "README has the 'Verbose debug logging' row" }
    else { Bad "README is missing the 'Verbose debug logging' settings row" }
} else {
    Bad "README.md not found"
}

# --------------------------------------------------------------------------
# Verdict
# --------------------------------------------------------------------------
Write-Host ""
if ($fail -eq 0) {
    Write-Host "REGRESSION GATE: PASS (all checks green)" -ForegroundColor Green
    exit 0
} else {
    Write-Host ("REGRESSION GATE: FAIL ({0} check(s) failed)" -f $fail) -ForegroundColor Red
    exit 1
}
