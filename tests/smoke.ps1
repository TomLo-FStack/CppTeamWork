$ErrorActionPreference = 'Stop'

$BuildDir = if ($args.Count -gt 0) { $args[0] } else { 'build' }
$Exe = Join-Path $BuildDir 'expense_app.exe'
if (-not (Test-Path $Exe)) {
    throw "missing executable: $Exe"
}

$Tmp = Join-Path $BuildDir ('smoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $Tmp | Out-Null
$Data = Join-Path $Tmp 'ledger.etx'
$Settlement = Join-Path $Tmp 'settlement.txt'

& $Exe --data $Data add 2026-04-01 12.50 food 'campus lunch'
& $Exe --data $Data add 2026-04-02 8.25 transit 'metro card'
& $Exe --data $Data add 2026-04-15 99.99 books 'systems programming text'
$summary = & $Exe --data $Data summary 2026-04
if (($summary -join "`n") -notmatch 'total:\s+120\.74') {
    $summary | Write-Output
    throw 'summary total mismatch'
}
if (($summary -join "`n") -notmatch 'Food\s+12\.50') {
    $summary | Write-Output
    throw 'category canonicalization mismatch'
}
$list = & $Exe --data $Data list month 2026-04
if (($list -join "`n") -notmatch '3 record') {
    $list | Write-Output
    throw 'list count mismatch'
}
& $Exe --data $Data --settlement $Settlement settle 2026-04
if (-not (Test-Path $Settlement)) {
    throw 'settlement state was not written'
}
Write-Output 'smoke ok'
