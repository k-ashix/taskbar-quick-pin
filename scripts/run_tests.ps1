<#
    run_tests.ps1  --  Build & run all unit tests for taskbar-quick-pin.

    Location: scripts\. Run it from anywhere; it locates the repo root (one
    level up from this script) and the tests\ folder under it, so a fresh
    clone just needs:

        powershell -ExecutionPolicy Bypass -File scripts\run_tests.ps1

    --------------------------------------------------------------------------
    Why the tests are NOT linked against taskbar-quick-pin.wh.cpp
    --------------------------------------------------------------------------
    taskbar-quick-pin.wh.cpp is a Windhawk mod: it #includes Windhawk/Win32
    headers and has no standalone main(), so it cannot be compiled on its own.
    Its comments say as much. To keep the logic testable, each pure decision is
    MIRRORED into a small header under tests\ (e.g. click_vs_drag.h), and each
    tests\*_test.cpp is a self-contained program with its own main() that
    includes that header (or inlines verbatim copies of the mod helpers, e.g.
    subrect_clear_test.cpp). The tests validate the mod's logic without needing
    the un-compilable Win32 translation unit.

    This script:
      1. Compiles every tests\*_test.cpp independently into tests\build\
      2. Runs each resulting .exe and records pass/fail (exit code 0 == pass)
      3. Writes a FULL transcript to a timestamped log in the repo root:
             test_result_run_{HH_MM_AM/PM}.log   (e.g. test_result_run_05_38_AM.log)
      4. Keeps the TERMINAL quiet: only a compact progress line + final summary
         are printed. Per-test detail and any failure output go to the log.
         (Failures are still echoed to the terminal so problems aren't hidden.)
      5. Cleans up: removes tests\build\ so no .exe artifacts remain next to
         the source. All .cpp / .h source is left untouched.

    After the tests, it also auto-invokes check_balance.ps1 (in scripts\)
    so one command does both. That step is best-effort (a missing script only
    warns) and can be turned off with -SkipBalanceCheck.

    Options:
        -KeepBinaries       keep the compiled exes in tests\build\ for debugging
        -SkipBalanceCheck   do NOT run check_balance.ps1 after the tests
        -SkipEmptyBodyCheck do NOT run check_empty_body.ps1 after the tests
        -ShowPass           also print per-test PASS lines to the terminal
        -Compiler g++       choose the C++ compiler (default: g++)
        -Std c++17          choose the language standard (default: c++17)
#>

[CmdletBinding()]
param(
    [switch]$KeepBinaries,
    [switch]$SkipBalanceCheck,
    [switch]$SkipLogLayerCheck,
    [switch]$SkipEmptyBodyCheck,
    [switch]$SkipRegressionCheck,
    [switch]$ShowPass,
    [string]$Compiler = "g++",
    [string]$Std = "c++17"
)

$ErrorActionPreference = "Stop"

# This script lives in scripts\ ; the repo root is one level up, and tests
# live in <root>\tests. check_balance.ps1 is a sibling in this same scripts\ dir.
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$TestsDir  = Join-Path $RepoRoot "tests"
$BuildDir  = Join-Path $TestsDir "build"

# --- Timestamped transcript log in the repo root -------------------------
# Filename form: test_result_run_HH_MM_AM.log / ..._PM.log  (12-hour clock).
# Include seconds so two runs in the same minute (e.g. a nested invocation)
# never write to the SAME file and clobber each other's transcript.
$stamp   = (Get-Date).ToString("hh_mm_ss_tt").ToUpper()
$LogFile = Join-Path $RepoRoot ("test_result_run_{0}.log" -f $stamp)

# Everything the run "says" is funnelled through Write-Log. Terminal output is
# intentionally sparse; the log file gets the full, timestamp-headed transcript.
function Write-Log {
    param(
        [string]$Message = "",
        [ConsoleColor]$Color,
        [switch]$ToConsole   # when set, also echo to the terminal
    )
    Add-Content -Path $LogFile -Value $Message
    if ($ToConsole) {
        if ($PSBoundParameters.ContainsKey('Color')) {
            Write-Host $Message -ForegroundColor $Color
        } else {
            Write-Host $Message
        }
    }
}

# Start the log with a header.
Set-Content -Path $LogFile -Value ("Test run  :  " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss"))
Write-Host ("Log        : {0}" -f $LogFile) -ForegroundColor Cyan

if (-not (Test-Path $TestsDir)) {
    Write-Log ("ERROR: tests directory not found at {0}" -f $TestsDir) -Color Red -ToConsole
    exit 2
}

# Verify the compiler exists.
$cc = Get-Command $Compiler -ErrorAction SilentlyContinue
if (-not $cc) {
    Write-Log ("ERROR: compiler '{0}' not found on PATH." -f $Compiler) -Color Red -ToConsole
    Write-Log "       Install MinGW-w64 (g++) or pass -Compiler <name>." -Color Red -ToConsole
    exit 2
}

# Fresh build dir.
if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Path $BuildDir | Out-Null

$tests = Get-ChildItem -Path $TestsDir -Filter *_test.cpp | Sort-Object Name
if ($tests.Count -eq 0) {
    Write-Log ("No *_test.cpp files found in {0}" -f $TestsDir) -Color Yellow -ToConsole
    exit 0
}

Write-Log ("Compiler : {0}" -f $cc.Source)
Write-Log ("Standard : {0}" -f $Std)
Write-Log ("Tests    : {0} file(s)" -f $tests.Count)
Write-Log ("-" * 60)

Write-Host ("Building & running {0} test(s) ... (full detail -> log)" -f $tests.Count)

$passed = 0
$failed = 0
$idx    = 0

foreach ($t in $tests) {
    $idx++
    $name = [System.IO.Path]::GetFileNameWithoutExtension($t.Name)
    $exe  = Join-Path $BuildDir ($name + ".exe")

    # Compact, single-line progress indicator on the terminal (overwritten
    # each iteration so it stays to one line and doesn't spam the console).
    Write-Host ("`r  [{0,2}/{1}] {2,-40}" -f $idx, $tests.Count, $name) -NoNewline

    # --- Compile (headers are found via -I tests\) ---
    $compileLog = & $Compiler "-std=$Std" "-I", $TestsDir, $t.FullName "-o", $exe 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Log ("[BUILD FAIL] {0}" -f $name)
        $compileLog | ForEach-Object { Write-Log ("    $_") }
        # Surface build failures on the terminal (clear the progress line first).
        Write-Host ("`r{0}`r" -f (" " * 60)) -NoNewline
        Write-Host ("[BUILD FAIL] {0}  (see log)" -f $name) -ForegroundColor Red
        $failed++
        continue
    }

    # --- Run ---
    $runOut = & $exe 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Log ("[PASS] {0}" -f $name)
        if ($ShowPass) {
            Write-Host ("`r{0}`r" -f (" " * 60)) -NoNewline
            Write-Host ("[PASS] {0}" -f $name) -ForegroundColor Green
        }
        $passed++
    } else {
        Write-Log ("[FAIL] {0}" -f $name)
        $runOut | ForEach-Object { Write-Log ("    $_") }
        # Always surface failures on the terminal (clear the progress line first).
        Write-Host ("`r{0}`r" -f (" " * 60)) -NoNewline
        Write-Host ("[FAIL] {0}  (see log)" -f $name) -ForegroundColor Red
        $runOut | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
        $failed++
    }
}

# Clear the trailing progress line.
Write-Host ("`r{0}`r" -f (" " * 60)) -NoNewline

Write-Log ("-" * 60)
$summary = ("SUMMARY: {0} passed, {1} failed, {2} total" -f $passed, $failed, $tests.Count)
Write-Log $summary -Color ($(if ($failed -eq 0) { "Green" } else { "Red" })) -ToConsole

# --- Cleanup: remove all build artifacts so no .exe lingers by the source ---
if (-not $KeepBinaries) {
    Remove-Item -Recurse -Force $BuildDir
    Write-Log "Cleaned build artifacts (tests\build\ removed)."
} else {
    Write-Log ("Kept build artifacts in: {0}" -f $BuildDir) -ToConsole
}

# --- Auto-invoke check_balance.ps1 (scripts\) after the test run ----------
# Runs the project's balance check so a single `run_tests.ps1` invocation does
# both. It is best-effort: a missing script only warns, and it never overrides
# a test failure. Skip it with -SkipBalanceCheck (e.g. in CI).
# --------------------------------------------------------------------------
# Post-test auto-chain: one `run_tests.ps1` invocation runs the whole gate.
#   1. check_log_layer.ps1   -- single-Wh_Log-surface lint
#   2. check_empty_body.ps1  -- empty-body control-statement lint
#   3. check_balance.ps1     -- delimiter balance sanity check
#   4. regression_check.ps1  -- source-invariant regression gate (PART 2 only;
#                               -SkipUnitTests, since WE are the unit tests)
# Each is best-effort (a missing script only warns) but a real failure flips
# $extraFailed so the overall exit code is non-zero (CI treats it as a fail).
#
# RECURSION NOTE: regression_check.ps1 calls THIS script to run the unit tests.
# To avoid an infinite loop it passes -SkipRegressionCheck (and skips the other
# sub-checks, which it runs itself). So: when invoked from the gate, we do NOT
# re-invoke the gate.
# --------------------------------------------------------------------------
$extraFailed = 0
$subCheckStatuses = [ordered]@{}

function Invoke-SubCheck {
    # NOTE: the param is $ExtraArgs, NOT $Args. $Args is a PowerShell AUTOMATIC
    # variable; using it as a param name does not bind reliably, which silently
    # dropped the -SkipUnitTests we pass to the regression gate and caused a
    # nested unit run (and a same-minute log-file collision). Do not rename back.
    param([string]$ScriptName, [string]$Label, [string[]]$ExtraArgs = @())
    $path = Join-Path $ScriptDir $ScriptName
    if (-not (Test-Path $path)) {
        Write-Log ("{0} not found at {1} (skipping)." -f $ScriptName, $path) -Color Yellow -ToConsole
        $script:subCheckStatuses[$Label] = "SKIPPED (missing)"
        return
    }
    Write-Log ("-" * 60)
    Write-Log ("Running {0} ..." -f $ScriptName)
    $out = & powershell -ExecutionPolicy Bypass -File $path @ExtraArgs 2>&1
    $code = $LASTEXITCODE
    $out | ForEach-Object { Write-Log ("    $_") }
    if ($code -ne 0) {
        Write-Log ("{0}: FAIL (exit {1}) -- see log." -f $Label, $code) -Color Red -ToConsole
        $script:subCheckStatuses[$Label] = "FAIL (exit $code)"
        $script:extraFailed++
    } else {
        Write-Log ("{0}: OK." -f $Label)
        $script:subCheckStatuses[$Label] = "PASS"
    }
}

if (-not $SkipLogLayerCheck) {
    Invoke-SubCheck "check_log_layer.ps1" "log-layer guard"
    Invoke-SubCheck "..\tests\check_log_layer_guard_self_test.ps1" "log-layer guard self-test"
} else {
    $subCheckStatuses["log-layer guard"] = "SKIPPED"
    $subCheckStatuses["log-layer guard self-test"] = "SKIPPED"
}

if (-not $SkipEmptyBodyCheck) {
    Invoke-SubCheck "check_empty_body.ps1" "empty-body guard"
    Invoke-SubCheck "..\tests\check_empty_body_guard_self_test.ps1" "empty-body guard self-test"
} else {
    $subCheckStatuses["empty-body guard"] = "SKIPPED"
    $subCheckStatuses["empty-body guard self-test"] = "SKIPPED"
}

if (-not $SkipBalanceCheck) {
    Invoke-SubCheck "check_balance.ps1" "balance check"
} else {
    $subCheckStatuses["balance check"] = "SKIPPED"
}

if (-not $SkipRegressionCheck) {
    # -SkipUnitTests: the gate's PART 1 is exactly this run; only run its
    # source-invariant checks (PART 2) to avoid re-running the suite.
    Invoke-SubCheck "regression_check.ps1" "regression gate" @("-SkipUnitTests")
} else {
    $subCheckStatuses["regression gate"] = "SKIPPED"
}

$overallPassed = ($failed -eq 0 -and $extraFailed -eq 0)
Write-Log ("=" * 60)
Write-Log "FINAL SUMMARY" -ToConsole
Write-Log ("Unit tests : {0} passed, {1} failed, {2} total" -f $passed, $failed, $tests.Count) -ToConsole
foreach ($entry in $subCheckStatuses.GetEnumerator()) {
    Write-Log ("{0,-27}: {1}" -f $entry.Key, $entry.Value) -ToConsole
}
Write-Log ("Sub-checks : {0} enabled failure(s)" -f $extraFailed) -ToConsole
Write-Log ("Transcript : {0}" -f $LogFile) -ToConsole
Write-Log ("OVERALL    : {0}" -f $(if ($overallPassed) { "PASS" } else { "FAIL" })) -Color ($(if ($overallPassed) { "Green" } else { "Red" })) -ToConsole

# Non-zero exit if any unit test OR any auto-chained sub-check failed (CI).
if ($overallPassed) { exit 0 } else { exit 1 }
