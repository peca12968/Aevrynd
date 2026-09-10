param(
  [Parameter(Mandatory = $true)][string]$OutputRoot,
  [int]$Seed = 7367
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$shots = @(
  'first-day', 'first-dusk', 'first-night',
  'third-day', 'third-dusk', 'third-night',
  'god-day', 'god-dusk', 'god-night',
  'palette-building', 'melee-combat'
)
$waterShots = @('third-day', 'god-day', 'god-dusk')

function Write-PatternPng([string]$Path, [string]$Kind, [int]$ShotIndex) {
  $width = 1280
  $height = 720
  $bitmap = New-Object Drawing.Bitmap($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
  try {
    $rectangle = New-Object Drawing.Rectangle(0, 0, $width, $height)
    $data = $bitmap.LockBits($rectangle, [Drawing.Imaging.ImageLockMode]::WriteOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
      if ($data.Stride -le 0) { throw "Unsupported bitmap stride: $($data.Stride)" }
      $bytes = New-Object byte[] ($data.Stride * $height)
      for ($x = 0; $x -lt $width; ++$x) {
          $offset = $x * 4
          if ($Kind -eq 'beauty') {
            $base = 52 + (($x * 13 + $ShotIndex * 17 + $Seed) % 132)
            $bytes[$offset] = [byte][Math]::Min(255, $base + 12)
            $bytes[$offset + 1] = [byte][Math]::Min(255, $base + 5)
            $bytes[$offset + 2] = [byte]$base
          } elseif ($Kind -eq 'object-id') {
            $bytes[$offset] = [byte](32 + (($x / 32 + $ShotIndex) % 180))
            $bytes[$offset + 1] = [byte](48 + (($x / 24 + $ShotIndex * 3) % 160))
            $bytes[$offset + 2] = [byte](64 + (($x / 40 + $ShotIndex * 5) % 140))
          } else {
            $depth = 8 + (($x + $ShotIndex * 29) % 240)
            $bytes[$offset] = [byte]$depth
            $bytes[$offset + 1] = [byte]$depth
            $bytes[$offset + 2] = [byte]$depth
          }
          $bytes[$offset + 3] = 255
      }
      for ($y = 1; $y -lt $height; ++$y) {
        [Buffer]::BlockCopy($bytes, 0, $bytes, $y * $data.Stride, $data.Stride)
      }
      [Runtime.InteropServices.Marshal]::Copy($bytes, 0, $data.Scan0, $bytes.Length)
    } finally { $bitmap.UnlockBits($data) }
    $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
  } finally { $bitmap.Dispose() }
}

[IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
$rows = @()
for ($index = 0; $index -lt $shots.Count; ++$index) {
  $name = $shots[$index]
  Write-PatternPng (Join-Path $OutputRoot "$name.png") 'beauty' $index
  Write-PatternPng (Join-Path $OutputRoot "$name-object-id.png") 'object-id' $index
  Write-PatternPng (Join-Path $OutputRoot "$name-depth.png") 'depth' $index
  $rows += [ordered]@{
    name = $name
    hdr = [ordered]@{meanLuma=1.25;p10Luma=0.18}
    water = if ($waterShots -contains $name) {
      [ordered]@{pixelCoverage=0.08;entropy=6.2;normalVariance=0.004;reflectionVariance=0.008}
    } else {
      [ordered]@{pixelCoverage=0.0;entropy=0.0;normalVariance=0.0;reflectionVariance=0.0}
    }
    terrain = [ordered]@{autocorrelation32To256=0.28}
    vegetation = [ordered]@{coverage=0.18;spatialEntropy=4.2}
    actor = [ordered]@{coverage=0.035}
    ui = [ordered]@{coverage=0.035;heroOverlap=0}
  }
}
$sidecar = [ordered]@{schemaVersion=1;seed=$Seed;shots=$rows}
[IO.File]::WriteAllText((Join-Path $OutputRoot 'r7-visual-sidecar.json'), (($sidecar | ConvertTo-Json -Depth 8) + "`n"), (New-Object Text.UTF8Encoding($false)))

$manifestRows = @(Get-ChildItem -LiteralPath $OutputRoot -File | Sort-Object Name | ForEach-Object {
  [ordered]@{path=$_.Name;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
$manifest = [ordered]@{schemaVersion=1;seed=$Seed;fileCount=$manifestRows.Count;files=$manifestRows}
[IO.File]::WriteAllText((Join-Path $OutputRoot 'fixture-manifest.json'), (($manifest | ConvertTo-Json -Depth 6) + "`n"), (New-Object Text.UTF8Encoding($false)))
Write-Output "R7_SYNTHETIC_FIXTURE=$OutputRoot files=$($manifestRows.Count) seed=$Seed"
