
$ErrorActionPreference = "Stop"

$exe = Join-Path $PSScriptRoot "..\miniptx.exe"
$exe = (Resolve-Path $exe).Path
$temp = Join-Path $PSScriptRoot "generated.ptx"

function Assert-True($condition, $message) {
    if (-not $condition) {
        throw "FAIL: $message"
    }
    Write-Host "PASS: $message" -ForegroundColor Green
}

# Multiplication must be emitted before addition.
$output = & $exe "2 + 3 * 4" 2>&1 | Out-String
Assert-True ($LASTEXITCODE -eq 0) "Valid expression compiles"
Assert-True ($output.Contains("mul.lo.s32")) "Multiplication instruction emitted"
Assert-True ($output.Contains("add.s32")) "Addition instruction emitted"
Assert-True ($output.Contains(".visible .entry miniptx_kernel")) "PTX kernel emitted"
Assert-True ($output.Contains("st.global.s32")) "Result store emitted"

# Parentheses must change the order of operations.
$output = & $exe "(2 + 3) * 4" 2>&1 | Out-String
Assert-True ($LASTEXITCODE -eq 0) "Parenthesized expression compiles"
$addPos = $output.IndexOf("add.s32")
$mulPos = $output.IndexOf("mul.lo.s32")
Assert-True ($addPos -ge 0 -and $mulPos -gt $addPos) `
    "Parentheses cause addition before multiplication"

# Invalid syntax must fail.
$output = & $exe "2 + * 3" 2>&1 | Out-String
Assert-True ($LASTEXITCODE -ne 0) "Invalid expression rejected"

$output = & $exe "2 + (3 * 4" 2>&1 | Out-String
Assert-True ($LASTEXITCODE -ne 0) "Missing closing parenthesis rejected"

Write-Host "`nAll MiniPTX tests passed."
