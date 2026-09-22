param(
  [Parameter(Mandatory = $true)][string]$CaptureRoot,
  [Parameter(Mandatory = $true)][string]$ReceiptPath,
  [string]$SidecarPath = '',
  [int]$DirectorReferenceScore = 34
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$requiredShots = @(
  'first-day', 'first-dusk', 'first-night',
  'third-day', 'third-dusk', 'third-night',
  'god-day', 'god-dusk', 'god-night',
  'palette-building', 'melee-combat'
)
$nightShots = @('first-night', 'third-night', 'god-night')
$waterShots = @('third-day', 'third-dusk', 'god-day', 'god-dusk')
$actorShots = @('third-day', 'third-dusk', 'third-night', 'god-day', 'god-dusk', 'god-night', 'palette-building', 'melee-combat')
$roofBaseline = [ordered]@{pixelCoverage=0.0430208333333333;meanLuma=0.096183361;gradient1pxP95=0.054805845;gradient4pxP90=0.067549904}
$issues = [Collections.Generic.List[string]]::new()
$shotReceipts = [Collections.Generic.List[object]]::new()

function Get-ImageMetrics([string]$Path) {
  $source = [Drawing.Bitmap]::FromFile($Path)
  try {
    if ($source.Width -ne 1280 -or $source.Height -ne 720) { throw "Unexpected image dimensions: $Path" }
    $bitmap = [Drawing.Bitmap]::new($source.Width, $source.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
      $graphics = [Drawing.Graphics]::FromImage($bitmap)
      try { $graphics.DrawImageUnscaled($source, 0, 0) } finally { $graphics.Dispose() }
      $rectangle = [Drawing.Rectangle]::new(0, 0, $bitmap.Width, $bitmap.Height)
      $data = $bitmap.LockBits($rectangle, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
      try {
        $stride = $data.Stride
        if ($stride -le 0) { throw "Unsupported bitmap stride: $stride" }
        $bytes = [byte[]]::new($stride * $data.Height)
        [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
      } finally { $bitmap.UnlockBits($data) }
    } finally { $bitmap.Dispose() }
  } finally { $source.Dispose() }

  $histogram = [int[]]::new(256)
  $black = 0
  $nonZero = 0
  $lumaSum = 0.0
  $count = 0
  for ($y = 0; $y -lt 720; $y += 2) {
    for ($x = 0; $x -lt 1280; $x += 2) {
      $offset = $y * $stride + $x * 4
      $blue = [double]$bytes[$offset]
      $green = [double]$bytes[$offset + 1]
      $red = [double]$bytes[$offset + 2]
      $value = 0.0722 * $blue + 0.7152 * $green + 0.2126 * $red
      $lumaSum += $value
      $count = $count + 1
      $bucketIndex = [Math]::Max(0, [Math]::Min(255, [int][Math]::Floor($value)))
      $histogram[$bucketIndex] = $histogram[$bucketIndex] + 1
      if ($value -lt 20.0) { $black = $black + 1 }
      if (($red + $green + $blue) -gt 0.0) { $nonZero = $nonZero + 1 }
    }
  }
  $mean = $lumaSum / $count
  $entropy = 0.0
  foreach ($bucket in $histogram) {
    if ($bucket -gt 0) { $probability = [double]$bucket / $count; $entropy -= $probability * [Math]::Log($probability, 2.0) }
  }
  $p10Target = [int][Math]::Floor(($count - 1) * 0.10)
  $p90Target = [int][Math]::Floor(($count - 1) * 0.90)
  $p10 = 0
  $p90 = 255
  $cumulative = 0
  for ($index = 0; $index -lt $histogram.Count; ++$index) {
    $cumulative += $histogram[$index]
    if ($cumulative -gt $p10Target) { $p10 = $index; break }
  }
  $cumulative = 0
  for ($index = 0; $index -lt $histogram.Count; ++$index) {
    $cumulative += $histogram[$index]
    if ($cumulative -gt $p90Target) { $p90 = $index; break }
  }
  $result = [ordered]@{}
  $result.width = 1280
  $result.height = 720
  $result.samples = $count
  $result.meanLuma = [Math]::Round($mean, 6)
  $result.p10Luma = [double]$p10
  $result.p90Luma = [double]$p90
  $result.blackRatio = [Math]::Round([double]$black / $count, 8)
  $result.entropy = [Math]::Round($entropy, 6)
  $result.nonZeroRatio = [Math]::Round([double]$nonZero / $count, 8)
  return [pscustomobject]$result
}

if (-not (Test-Path -LiteralPath $CaptureRoot -PathType Container)) { throw "Capture root is missing: $CaptureRoot" }
if (-not $SidecarPath) { $SidecarPath = Join-Path $CaptureRoot 'r7-visual-sidecar.json' }
$sidecar = $null
if (Test-Path -LiteralPath $SidecarPath -PathType Leaf) {
  try { $sidecar = Get-Content -Raw -LiteralPath $SidecarPath | ConvertFrom-Json } catch { $issues.Add("sidecar-invalid:$($_.Exception.Message)") }
} else { $issues.Add('sidecar-missing') }

$sidecarRows = @{}
if ($sidecar) {
  if ($sidecar.schemaVersion -ne 1) { $issues.Add('sidecar-schema') }
  if ([int]$sidecar.evaluatorVersion -lt 3 -or
      [string]$sidecar.algorithms.roofFrequency -ne 'beauty-srgb-to-linear-rec709-token472-forward-axis-mean-gradient-p95-1px-p90-4px-v1') {
    $issues.Add('sidecar-roof-algorithm')
  }
  foreach ($row in @($sidecar.shots)) {
    if (-not $row.name -or $sidecarRows.ContainsKey([string]$row.name)) { $issues.Add('sidecar-shot-identity'); continue }
    $sidecarRows[[string]$row.name] = $row
  }
  $actualNames = @($sidecarRows.Keys | Sort-Object)
  if (@(Compare-Object ($requiredShots | Sort-Object) $actualNames).Count -ne 0) { $issues.Add('sidecar-shot-exact-set') }
}

$artifactPasses = 0
$nightPasses = 0
$waterPasses = 0
$terrainPasses = 0
$vegetationPasses = 0
$actorPasses = 0
$uiPasses = 0
foreach ($name in $requiredShots) {
  $beautyPath = Join-Path $CaptureRoot "$name.png"
  $objectIdPath = Join-Path $CaptureRoot "$name-object-id.png"
  $depthPath = Join-Path $CaptureRoot "$name-depth.png"
  $missing = @(@($beautyPath, $objectIdPath, $depthPath) | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
  if ($missing.Count) { $issues.Add("shot-artifact-missing:$name"); continue }
  try {
    $beauty = Get-ImageMetrics $beautyPath
    $objectId = Get-ImageMetrics $objectIdPath
    $depth = Get-ImageMetrics $depthPath
    if ($objectId.nonZeroRatio -le 0.001 -or $depth.entropy -le 0.05) { $issues.Add("shot-semantic-empty:$name") } else { ++$artifactPasses }
  } catch { $issues.Add("shot-readback-invalid:${name}:$($_.Exception.Message)"); continue }

  $semantic = if ($sidecarRows.ContainsKey($name)) { $sidecarRows[$name] } else { $null }
  $nightPass = $true
  if ($nightShots -contains $name) {
    $nightPass = $beauty.blackRatio -lt 0.45 -and $beauty.meanLuma -ge 30.0 -and $beauty.p10Luma -ge 10.0 -and
      $semantic -and $semantic.hdr -and $semantic.hdr.meanLuma -gt 0 -and $semantic.hdr.p10Luma -gt 0
    if ($nightPass) { ++$nightPasses } else { $issues.Add("night-readability:$name") }
  }
  $waterPass = $true
  $waterApplicable = $waterShots -contains $name
  if ($waterApplicable) {
    $waterPass = $semantic -and $semantic.water -and $semantic.water.pixelCoverage -ge 0.002 -and
      $semantic.water.entropy -ge 5.5 -and $semantic.water.normalVariance -ge 0.0005 -and $semantic.water.reflectionVariance -ge 0.0005
    if ($waterPass) { ++$waterPasses } else { $issues.Add("water-quality:$name") }
  }
  $terrainPass = $semantic -and $semantic.terrain -and $semantic.terrain.autocorrelation32To256 -lt 0.65
  if ($terrainPass) { ++$terrainPasses } else { $issues.Add("terrain-repeat:$name") }
  $vegetationPass = $semantic -and $semantic.vegetation -and $semantic.vegetation.coverage -ge 0.06 -and $semantic.vegetation.spatialEntropy -ge 3.0
  if ($vegetationPass) { ++$vegetationPasses } else { $issues.Add("vegetation-density:$name") }
  $actorPass = $true
  if ($actorShots -contains $name) {
    $minimumActor = if ($name.StartsWith('god-')) { 0.0015 } else { 0.03 }
    $actorPass = $semantic -and $semantic.actor -and $semantic.actor.coverage -ge $minimumActor -and $semantic.actor.coverage -le 0.10
    if ($actorPass) { ++$actorPasses } else { $issues.Add("actor-coverage:$name") }
  }
  $uiPass = $semantic -and $semantic.ui -and $semantic.ui.coverage -le 0.08 -and $semantic.ui.heroOverlap -eq 0
  if ($uiPass) { ++$uiPasses } else { $issues.Add("ui-composition:$name") }
  $roofApplicable = $name -eq 'god-day'
  $roofPass = $true
  if ($roofApplicable) {
    $roof = if ($semantic) { $semantic.roof } else { $null }
    $roofPass = $roof -and
      [double]$roof.pixelCoverage -ge ($roofBaseline.pixelCoverage - 0.001) -and
      [double]$roof.pixelCoverage -le ($roofBaseline.pixelCoverage + 0.001) -and
      [double]$roof.meanLuma -ge ($roofBaseline.meanLuma * 0.95) -and
      [double]$roof.meanLuma -le ($roofBaseline.meanLuma * 1.05) -and
      [double]$roof.gradient1pxP95 -le 0.045 -and
      [double]$roof.gradient1pxP95 -le ($roofBaseline.gradient1pxP95 * 0.80) -and
      [double]$roof.gradient4pxP90 -ge 0.050
    if (-not $roofPass) { $issues.Add("roof-frequency:$name") }
  }
  $shotReceipts.Add([ordered]@{name=$name;beauty=$beauty;objectId=$objectId;depth=$depth;nightPass=$nightPass;waterApplicable=$waterApplicable;waterPass=[bool]$waterPass;terrainPass=[bool]$terrainPass;vegetationPass=[bool]$vegetationPass;actorPass=[bool]$actorPass;roofApplicable=$roofApplicable;roofPass=[bool]$roofPass;uiPass=[bool]$uiPass})
}

$artifactScore = [Math]::Floor(10.0 * $artifactPasses / $requiredShots.Count)
$nightScore = [Math]::Floor(20.0 * $nightPasses / $nightShots.Count)
$waterScore = [Math]::Floor(15.0 * $waterPasses / $waterShots.Count)
$terrainScore = [Math]::Floor(15.0 * $terrainPasses / $requiredShots.Count)
$vegetationScore = [Math]::Floor(15.0 * $vegetationPasses / $requiredShots.Count)
$actorScore = [Math]::Floor(15.0 * $actorPasses / $actorShots.Count)
$uiScore = [Math]::Floor(10.0 * $uiPasses / $requiredShots.Count)
$automatedScore = $artifactScore + $nightScore + $waterScore + $terrainScore + $vegetationScore + $actorScore + $uiScore
$effectiveScore = [Math]::Min($automatedScore, $DirectorReferenceScore)
$hardPass = $issues.Count -eq 0 -and $effectiveScore -ge 70
$receipt = [ordered]@{
  schemaVersion=1; source='GameplayR7VisualGate'; captureRoot=[IO.Path]::GetFullPath($CaptureRoot)
  requiredShots=$requiredShots; requiredShotCount=$requiredShots.Count; sidecar=[IO.Path]::GetFullPath($SidecarPath); sidecarPresent=[bool]$sidecar
  releaseThreshold=70; directorReferenceScore=$DirectorReferenceScore; automatedScore=$automatedScore; effectiveScore=$effectiveScore
  rubric=[ordered]@{composition=20;lighting=20;materials=20;worldDensity=15;characters=15;presentation=10}
  componentScores=[ordered]@{artifact=$artifactScore;night=$nightScore;water=$waterScore;terrain=$terrainScore;vegetation=$vegetationScore;actor=$actorScore;ui=$uiScore}
  hardPass=$hardPass; result=if($hardPass){'GO'}else{'NO-GO'}; issueCount=$issues.Count; issues=@($issues); shots=@($shotReceipts)
}
$receiptDirectory = Split-Path -Parent $ReceiptPath
if ($receiptDirectory) { [IO.Directory]::CreateDirectory($receiptDirectory) | Out-Null }
[IO.File]::WriteAllText($ReceiptPath, (($receipt | ConvertTo-Json -Depth 12) + "`n"), [Text.UTF8Encoding]::new($false))
Write-Output "R7_VISUAL_GATE=$($receipt.result) automated=$automatedScore director=$DirectorReferenceScore effective=$effectiveScore issues=$($issues.Count) receipt=$ReceiptPath"
if (-not $hardPass) { exit 2 }
