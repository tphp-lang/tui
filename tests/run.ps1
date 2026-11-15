# tphp/tui 冒烟一键驱动（Task 9）
# 用法（cwd = tui 包根）：
#   powershell -ExecutionPolicy Bypass -File tests\run.ps1 [-Tphp <tphp路径>]
# 串行执行 tests\smoke.php 与 tests\smoke-*.php，汇总 PASS/FAIL；
# 任一失败退出码非 0。
param(
    [string]$Tphp = ""
)

$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here

if ($Tphp -eq "") {
    $cand = Join-Path $root "..\TinyPHP.rt\build\tphp.exe"
    $Tphp = (Resolve-Path $cand -ErrorAction SilentlyContinue)
    if (-not $Tphp) {
        Write-Host "error: tphp not found (use -Tphp <path>): $cand"
        exit 1
    }
}
if (-not (Test-Path $Tphp)) {
    Write-Host "error: tphp not found: $Tphp"
    exit 1
}

$tests = @()
$base = Join-Path $here "smoke.php"
if (Test-Path $base) { $tests += $base }
$tests += (Get-ChildItem (Join-Path $here "smoke-*.php") | Sort-Object Name | ForEach-Object { $_.FullName })

if ($tests.Count -eq 0) {
    Write-Host "error: no smoke tests found under $here"
    exit 1
}

$pass = 0
$fail = 0
$results = @()
foreach ($t in $tests) {
    $name = Split-Path -Leaf $t
    $out = & $Tphp run $t 2>&1 | Out-String
    $code = $LASTEXITCODE
    $ok = ($code -eq 0) -and ($out -match "SMOKE=OK")
    if ($ok) { $pass++; $results += "PASS  $name" }
    else {
        $fail++
        $results += "FAIL  $name (exit=$code)"
        # 打印失败明细（FAIL 行与异常行）
        foreach ($line in ($out -split "`r?`n")) {
            if ($line -match "FAIL |Fatal|Uncaught|Exception|error") { $results += "      $line" }
        }
    }
}
$results | ForEach-Object { Write-Host $_ }
Write-Host "PASS=$pass FAIL=$fail"
if ($fail -gt 0) { exit 1 }
exit 0
