param(
  [string]$WorkingRoot = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$testRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$builder = Join-Path $testRoot 'fixtures\r7-visual\BuildR7VisualSyntheticFixture.ps1'
$gate = Join-Path $testRoot 'GameplayR7VisualGate.ps1'
if (-not $WorkingRoot) { $WorkingRoot = Join-Path $env:TEMP 'aevrynd-r7-visual-gate-synthetic' }

if (Test-Path -LiteralPath $WorkingRoot) { Remove-Item -LiteralPath $WorkingRoot -Recurse -Force }
[IO.Directory]::CreateDirectory($WorkingRoot) | Out-Null
$fixtureA = Join-Path $WorkingRoot 'fixture-a'
$fixtureB = Join-Path $WorkingRoot 'fixture-b'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $builder -OutputRoot $fixtureA -Seed 7367
if ($LASTEXITCODE -ne 0) { throw 'Synthetic fixture A generation failed' }
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $builder -OutputRoot $fixtureB -Seed 7367
if ($LASTEXITCODE -ne 0) { throw 'Synthetic fixture B generation failed' }

function Get-FixtureMap([string]$Root) {
  $map = @{}
  Get-ChildItem -LiteralPath $Root -File | Where-Object Name -ne 'fixture-manifest.json' | ForEach-Object {
    $map[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
  }
  return $map
}

$mapA = Get-FixtureMap $fixtureA
$mapB = Get-FixtureMap $fixtureB
if ($mapA.Count -ne 34 -or $mapB.Count -ne 34) { throw 'Synthetic fixture exact-set count failed' }
if (@(Compare-Object @($mapA.Keys | Sort-Object) @($mapB.Keys | Sort-Object)).Count -ne 0) { throw 'Synthetic fixture path exact-set failed' }
foreach ($name in $mapA.Keys) { if ($mapA[$name] -ne $mapB[$name]) { throw "Synthetic fixture byte identity failed: $name" } }
$manifestHashA = (Get-FileHash -LiteralPath (Join-Path $fixtureA 'fixture-manifest.json') -Algorithm SHA256).Hash
$manifestHashB = (Get-FileHash -LiteralPath (Join-Path $fixtureB 'fixture-manifest.json') -Algorithm SHA256).Hash
if ($manifestHashA -ne $manifestHashB) { throw 'Synthetic fixture manifest was not byte deterministic' }

function Invoke-Gate([string]$CaseName, [string]$CaptureRoot, [string]$SidecarPath, [bool]$ExpectPass) {
  $receipt = Join-Path $WorkingRoot "$CaseName-receipt.json"
  $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$gate,'-CaptureRoot',$CaptureRoot,'-ReceiptPath',$receipt,'-DirectorReferenceScore','100')
  if ($SidecarPath) { $arguments += @('-SidecarPath',$SidecarPath) }
  & powershell.exe @arguments | Out-Host
  $exitCode = $LASTEXITCODE
  if (-not (Test-Path -LiteralPath $receipt -PathType Leaf)) { throw "Gate receipt missing: $CaseName" }
  $result = Get-Content -Raw -LiteralPath $receipt | ConvertFrom-Json
  $expectedKeys = @('schemaVersion','source','captureRoot','requiredShots','requiredShotCount','sidecar','sidecarPresent','releaseThreshold','directorReferenceScore','automatedScore','effectiveScore','rubric','componentScores','hardPass','result','issueCount','issues','shots')
  if (@(Compare-Object $expectedKeys @($result.psobject.Properties.Name)).Count -ne 0) { throw "Receipt schema exact-set failed: $CaseName" }
  if ($ExpectPass) {
    if ($exitCode -ne 0 -or $result.result -ne 'GO' -or -not $result.hardPass -or $result.effectiveScore -lt 70 -or $result.issueCount -ne 0 -or $result.shots.Count -ne 11) { throw "Positive gate failed: $CaseName" }
  } else {
    if ($exitCode -ne 2 -or $result.result -ne 'NO-GO' -or $result.hardPass -or $result.issueCount -le 0) { throw "Negative gate failed open: $CaseName" }
  }
  return $receipt
}

$passReceipt = Invoke-Gate 'pass' $fixtureA '' $true
$passHash1 = (Get-FileHash -LiteralPath $passReceipt -Algorithm SHA256).Hash
$null = Invoke-Gate 'pass' $fixtureA '' $true
$passHash2 = (Get-FileHash -LiteralPath $passReceipt -Algorithm SHA256).Hash
if ($passHash1 -ne $passHash2) { throw 'Positive receipt was not byte deterministic' }

function Write-SidecarMutation([string]$Name, [scriptblock]$Mutation) {
  $sidecar = Get-Content -Raw -LiteralPath (Join-Path $fixtureA 'r7-visual-sidecar.json') | ConvertFrom-Json
  & $Mutation $sidecar
  $path = Join-Path $WorkingRoot "$Name-sidecar.json"
  [IO.File]::WriteAllText($path, (($sidecar | ConvertTo-Json -Depth 8) + "`n"), (New-Object Text.UTF8Encoding($false)))
  return $path
}

$water = Write-SidecarMutation 'water-low-entropy' {
  param($s)
  ($s.shots | Where-Object name -eq 'third-day').water.entropy = 1.0
}
$null = Invoke-Gate 'water-low-entropy' $fixtureA $water $false
$terrain = Write-SidecarMutation 'terrain-correlated' { param($s) $s.shots[1].terrain.autocorrelation32To256 = 0.99 }
$null = Invoke-Gate 'terrain-correlated' $fixtureA $terrain $false
$actor = Write-SidecarMutation 'actor-coverage' { param($s) $s.shots[3].actor.coverage = 0.0001 }
$null = Invoke-Gate 'actor-coverage' $fixtureA $actor $false
$ui = Write-SidecarMutation 'ui-coverage' { param($s) $s.shots[4].ui.coverage = 0.5 }
$null = Invoke-Gate 'ui-coverage' $fixtureA $ui $false
$roofBaseline = Write-SidecarMutation 'roof-r75-baseline' {
  param($s)
  $roof = $s.shots | Where-Object name -eq 'god-day' | Select-Object -ExpandProperty roof
  $roof.pixelCoverage = 0.0430208333333333
  $roof.meanLuma = 0.096183361
  $roof.gradient1pxP95 = 0.054805845
  $roof.gradient4pxP90 = 0.067549904
}
$null = Invoke-Gate 'roof-r75-baseline' $fixtureA $roofBaseline $false
$roofBlurred = Write-SidecarMutation 'roof-overblurred' {
  param($s)
  ($s.shots | Where-Object name -eq 'god-day').roof.gradient4pxP90 = 0.049
}
$null = Invoke-Gate 'roof-overblurred' $fixtureA $roofBlurred $false
$roofAlgorithm = Write-SidecarMutation 'roof-algorithm' {
  param($s)
  $s.algorithms.roofFrequency = 'untrusted-roof-metric'
}
$null = Invoke-Gate 'roof-algorithm' $fixtureA $roofAlgorithm $false

$nightRoot = Join-Path $WorkingRoot 'night-dark'
Copy-Item -LiteralPath $fixtureA -Destination $nightRoot -Recurse
$dark = New-Object Drawing.Bitmap(1280, 720, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
try { $graphics=[Drawing.Graphics]::FromImage($dark); try {$graphics.Clear([Drawing.Color]::Black)} finally {$graphics.Dispose()}; $dark.Save((Join-Path $nightRoot 'first-night.png'), [Drawing.Imaging.ImageFormat]::Png) } finally { $dark.Dispose() }
$null = Invoke-Gate 'night-dark' $nightRoot '' $false

$missingSidecarRoot = Join-Path $WorkingRoot 'missing-sidecar'
Copy-Item -LiteralPath $fixtureA -Destination $missingSidecarRoot -Recurse
Remove-Item -LiteralPath (Join-Path $missingSidecarRoot 'r7-visual-sidecar.json')
$null = Invoke-Gate 'missing-sidecar' $missingSidecarRoot '' $false

$missingShotRoot = Join-Path $WorkingRoot 'missing-shot'
Copy-Item -LiteralPath $fixtureA -Destination $missingShotRoot -Recurse
Remove-Item -LiteralPath (Join-Path $missingShotRoot 'god-night-depth.png')
$null = Invoke-Gate 'missing-shot' $missingShotRoot '' $false

$summary = [ordered]@{schemaVersion=1;result='PASS';seed=7367;fixtureFileCount=34;passReceiptSha256=$passHash1;negativeCases=@('night-dark','water-low-entropy','terrain-correlated','actor-coverage','ui-coverage','roof-r75-baseline','roof-overblurred','roof-algorithm','missing-sidecar','missing-shot')}
[IO.File]::WriteAllText((Join-Path $WorkingRoot 'test-summary.json'), (($summary | ConvertTo-Json -Depth 4) + "`n"), (New-Object Text.UTF8Encoding($false)))
Write-Output "R7_VISUAL_GATE_SYNTHETIC=PASS receiptSha256=$passHash1 negatives=10"
