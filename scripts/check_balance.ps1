<#
    check_balance.ps1  --  Delimiter balance sanity check for the Windhawk mod.

    Counts (), {} and [] in taskbar-quick-pin.wh.cpp and reports whether each
    pair is balanced.

    IMPORTANT: it counts delimiters in CODE ONLY. A naive raw-text count is
    misleading because the file is full of parens/braces that live inside
    comments and string literals -- the big ==WindhawkModReadme== block, URLs,
    prose like "(off by default)", smileys, and printf-style format strings.
    Those are text, not code, so counting them produces a phantom imbalance
    (e.g. more ")" than "(") even when the source is perfectly well-formed.

    To avoid that false alarm this script first strips, in order:
        1. block comments      /* ... */
        2. line comments       // ... (to end of line)
        3. string literals     "..."  (with \" escapes handled)
        4. char literals       '...'  (e.g. '(' , ')' , '\'')
    ...and only then counts the remaining (code) delimiters.

    Exit code: 0 when every pair is balanced, 1 otherwise. This lets
    run_tests.ps1 treat a genuine imbalance as a real signal.
#>

$ErrorActionPreference = "Stop"

# Resolve the mod source relative to the repo root (one level up from scripts\),
# so the script works no matter the current working directory.
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$Source    = Join-Path $RepoRoot "taskbar-quick-pin.wh.cpp"

if (-not (Test-Path $Source)) {
    Write-Host "ERROR: source not found at $Source" -ForegroundColor Red
    exit 2
}

$raw = [IO.File]::ReadAllText($Source)

# --- Line-ending stats (informational; unchanged from the original) --------
$crlf = [regex]::Matches($raw, "`r`n").Count
$lfOnly = [regex]::Matches($raw, "(?<!`r)`n").Count

# --- Strip comments and string/char literals, leaving only code ------------
# A single-pass state machine is the reliable way to do this: a regex alone
# can't tell a "(" in code from one inside a comment or string.
$sb = [System.Text.StringBuilder]::new()
$n  = $raw.Length
$i  = 0
# states: Code, LineComment, BlockComment, String, Char
$state = 'Code'
while ($i -lt $n) {
    $c    = $raw[$i]
    $next = if ($i + 1 -lt $n) { $raw[$i + 1] } else { "`0" }

    switch ($state) {
        'Code' {
            if ($c -eq '/' -and $next -eq '/') { $state = 'LineComment'; $i += 2; continue }
            if ($c -eq '/' -and $next -eq '*') { $state = 'BlockComment'; $i += 2; continue }
            if ($c -eq '"')  { $state = 'String'; $i++; continue }
            if ($c -eq "'")  { $state = 'Char';   $i++; continue }
            [void]$sb.Append($c); $i++; continue
        }
        'LineComment' {
            if ($c -eq "`n") { $state = 'Code'; [void]$sb.Append($c) }
            $i++; continue
        }
        'BlockComment' {
            if ($c -eq '*' -and $next -eq '/') { $state = 'Code'; $i += 2; continue }
            $i++; continue
        }
        'String' {
            if ($c -eq '\') { $i += 2; continue }   # skip escaped char (e.g. \" \\)
            if ($c -eq '"') { $state = 'Code' }
            $i++; continue
        }
        'Char' {
            if ($c -eq '\') { $i += 2; continue }   # skip escaped char (e.g. \' \\)
            if ($c -eq "'") { $state = 'Code' }
            $i++; continue
        }
    }
}
$code = $sb.ToString()

# --- Count delimiters in the code-only text --------------------------------
function Count-Char([string]$text, [char]$ch) {
    ($text.ToCharArray() | Where-Object { $_ -eq $ch }).Count
}

$openParen  = Count-Char $code '('
$closeParen = Count-Char $code ')'
$openBrace  = Count-Char $code '{'
$closeBrace = Count-Char $code '}'
$openBrack  = Count-Char $code '['
$closeBrack = Count-Char $code ']'

# --- Report ----------------------------------------------------------------
"CRLF=$crlf"
"LF_ONLY=$lfOnly"
"OPEN_PAREN=$openParen"
"CLOSE_PAREN=$closeParen"
"OPEN_BRACE=$openBrace"
"CLOSE_BRACE=$closeBrace"
"OPEN_BRACKET=$openBrack"
"CLOSE_BRACKET=$closeBrack"

$imbalance = 0
if ($openParen -ne $closeParen) {
    $imbalance++
    Write-Host ("PAREN IMBALANCE: {0} '(' vs {1} ')'  (diff {2})" -f $openParen, $closeParen, ($closeParen - $openParen)) -ForegroundColor Red
}
if ($openBrace -ne $closeBrace) {
    $imbalance++
    Write-Host ("BRACE IMBALANCE: {0} '{{' vs {1} '}}'  (diff {2})" -f $openBrace, $closeBrace, ($closeBrace - $openBrace)) -ForegroundColor Red
}
if ($openBrack -ne $closeBrack) {
    $imbalance++
    Write-Host ("BRACKET IMBALANCE: {0} '[' vs {1} ']'  (diff {2})" -f $openBrack, $closeBrack, ($closeBrack - $openBrack)) -ForegroundColor Red
}

if ($imbalance -eq 0) {
    Write-Host "BALANCED: () {} [] all match (comments & strings excluded)." -ForegroundColor Green
    exit 0
} else {
    exit 1
}
