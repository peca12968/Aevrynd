param(
  [string]$WorkingRoot = ''
)

$ErrorActionPreference = 'Stop'
$testRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$builder = Join-Path $testRoot 'fixtures\r7-visual\BuildR7VisualSyntheticFixture.ps1'
$gate = Join-Path $testRoot 'GameplayR7VisualGate.ps1'
if (-not $WorkingRoot) {
  $WorkingRoot = Join-Path $env:TEMP ("aevrynd-r7-third-dusk-gate-" + [guid]::NewGuid())
}

$fixture = Join-Path $WorkingRoot 'fixture'
[IO.Directory]::CreateDirectory($fixture) | Out-Null
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $builder -OutputRoot $fixture -Seed 7367 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Synthetic fixture generation failed' }

$sidecarPath = Join-Path $fixture 'r7-visual-sidecar.json'
$sidecar = Get-Content -Raw -LiteralPath $sidecarPath | ConvertFrom-Json
$thirdDusk = @($sidecar.shots | Where-Object name -eq 'third-dusk')
if ($thirdDusk.Count -ne 1) { throw 'third-dusk sidecar identity failed' }
$thirdDusk[0].water.normalVariance = 0.00007611
[IO.File]::WriteAllText(
  $sidecarPath,
  (($sidecar | ConvertTo-Json -Depth 8) + "`n"),
  [Text.UTF8Encoding]::new($false)
)

$receiptPath = Join-Path $WorkingRoot 'third-dusk-low-normal-variance-receipt.json'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $gate `
  -CaptureRoot $fixture `
  -ReceiptPath $receiptPath `
  -DirectorReferenceScore 100 | Out-Host
$exitCode = $LASTEXITCODE
if ($exitCode -ne 2) { throw "third-dusk low normal variance did not fail closed: exit=$exitCode" }

$receipt = Get-Content -Raw -LiteralPath $receiptPath | ConvertFrom-Json
$row = @($receipt.shots | Where-Object name -eq 'third-dusk')
if ($row.Count -ne 1 -or -not $row[0].waterApplicable -or $row[0].waterPass) {
  throw 'third-dusk water receipt did not record the failed applicable check'
}
if (@($receipt.issues | Where-Object { $_ -eq 'water-quality:third-dusk' }).Count -ne 1) {
  throw 'third-dusk water failure was not uniquely reported'
}

Write-Output "R7_THIRD_DUSK_WATER_GATE=PASS receipt=$receiptPath"
