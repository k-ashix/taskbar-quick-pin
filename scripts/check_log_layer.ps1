<#
    check_log_layer.ps1  --  Guards the "single logging layer" invariant.

    Location: scripts\. Run from anywhere:

        powershell -ExecutionPolicy Bypass -File scripts\check_log_layer.ps1

    --------------------------------------------------------------------------
    Why this exists
    --------------------------------------------------------------------------
    taskbar-quick-pin.wh.cpp must log ONLY through Windhawk's own Wh_Log(), whose
    "Logging enabled" toggle is the single gate. A previous round removed a
    redundant SECOND logging layer that sat on top of Wh_Log:

        * a g_debugLogging flag + the "debugLogging" setting (a duplicate gate)
        * wrapper macros: LOG_ERROR / LOG_IMPORTANT / DEBUG_LOG / TRACE_LOG / LOG_RATE
        * high-frequency, per-frame trace helpers: DragDebugLog / DragTraceVerboseLog

    Those made the log noisy and duplicated a gate Windhawk already provides.
    This script is a lightweight lint that FAILS if any of that layer comes back,
    or if someone introduces a new home-grown gate/macro. It is source-text only
    (the mod TU can't be compiled standalone), so it runs anywhere in ~1 second.

    Allowed logging surface:
        * Wh_Log(...)                      -- the one true logger
        * DragTraceLog(...)                -- thin always-on drag STATE helper
                                              (a genuine transition, never per-frame)

    Exit code: 0 when the invariant holds, 1 otherwise. Designed to be auto-
    invoked by run_tests.ps1 and used as a CI status check.

    Options:
        -Quiet   only print the final verdict line (still exits 0/1)
#>

[CmdletBinding()]
param(
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$ModFile   = Join-Path $RepoRoot "taskbar-quick-pin.wh.cpp"

if (-not (Test-Path $ModFile)) {
    Write-Host "ERROR: mod source not found at $ModFile" -ForegroundColor Red
    exit 2
}

$modText = [IO.File]::ReadAllText($ModFile)
$fail = 0

function Say  ($m, $c) { if (-not $Quiet) { if ($c) { Write-Host $m -ForegroundColor $c } else { Write-Host $m } } }
function Ok   ($m) { Say ("  [PASS] " + $m) Green }
function Bad  ($m) { Write-Host ("  [FAIL] " + $m) -ForegroundColor Red; $script:fail++ }

function CountMatches ($pattern) {
    return ([regex]::Matches($modText, $pattern)).Count
}
# BANNED: this token must not appear anywhere in the mod source.
function AssertBanned ($label, $pattern) {
    $n = CountMatches $pattern
    if ($n -eq 0) { Ok ("banned symbol absent: {0}" -f $label) }
    else          { Bad ("{0} reappeared -- found {1} occurrence(s). Log via Wh_Log() directly instead." -f $label, $n) }
}
# REQUIRED: this token must still be present.
function AssertKept ($label, $pattern) {
    $n = CountMatches $pattern
    if ($n -ge 1) { Ok ("{0} present ({1})" -f $label, $n) }
    else          { Bad ("{0} is missing -- expected the single logging surface to remain" -f $label) }
}

Say "=== log-layer guard (single Wh_Log surface) ===" White

# --- The removed second-layer symbols must never come back ------------------
Say "" $null
Say "Banned second-layer symbols:" Cyan
AssertBanned "g_debugLogging (duplicate gate flag)" 'g_debugLogging'
AssertBanned "debugLogging (duplicate setting key)" 'debugLogging'
AssertBanned "LOG_ERROR macro"     '\bLOG_ERROR\b'
AssertBanned "LOG_IMPORTANT macro" '\bLOG_IMPORTANT\b'
AssertBanned "DEBUG_LOG macro"     '\bDEBUG_LOG\b'
AssertBanned "TRACE_LOG macro"     '\bTRACE_LOG\b'
AssertBanned "LOG_RATE macro"      '\bLOG_RATE\b'
AssertBanned "DragDebugLog helper (per-frame)"        '\bDragDebugLog\b'
AssertBanned "DragTraceVerboseLog helper (per-frame)" '\bDragTraceVerboseLog\b'

# --- Catch a NEW home-grown gate before it spreads --------------------------
# A `#define <NAME>_LOG(` that wraps Wh_Log is exactly the pattern we removed.
# Flag any new *_LOG(...) logging macro so a second layer can't sneak in under
# a fresh name.
Say "" $null
Say "No new logging wrapper macros:" Cyan
$logMacro = [regex]::Matches($modText, '(?m)^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*_LOG)\s*\(')
if ($logMacro.Count -eq 0) {
    Ok "no '#define *_LOG(...)' wrapper macros"
} else {
    foreach ($m in $logMacro) {
        Bad ("new logging wrapper macro '{0}' -- call Wh_Log() directly, do not add a second layer" -f $m.Groups[1].Value)
    }
}
# A new `static bool g_*Log*` gate flag is the other half of the removed pattern.
$gateFlag = [regex]::Matches($modText, '(?m)^\s*static\s+bool\s+(g_[A-Za-z0-9_]*[Ll]og[A-Za-z0-9_]*)\s*=')
if ($gateFlag.Count -eq 0) {
    Ok "no home-grown 'g_*log*' gate flag"
} else {
    foreach ($m in $gateFlag) {
        Bad ("new logging gate flag '{0}' -- Windhawk's 'Logging enabled' toggle is the only gate" -f $m.Groups[1].Value)
    }
}

# --- The single allowed surface must still be there -------------------------
Say "" $null
Say "Allowed logging surface intact:" Cyan
AssertKept "Wh_Log() calls" '\bWh_Log\s*\('
AssertKept "DragTraceLog() (state-transition helper)" '\bDragTraceLog\b'

# --- Verdict ----------------------------------------------------------------
Write-Host ""
if ($fail -eq 0) {
    Write-Host "LOG-LAYER GUARD: PASS (single Wh_Log surface)" -ForegroundColor Green
    exit 0
} else {
    Write-Host ("LOG-LAYER GUARD: FAIL ({0} issue(s)) -- a redundant logging layer was reintroduced" -f $fail) -ForegroundColor Red
    exit 1
}
