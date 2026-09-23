param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Output,
    [string]$Blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe',
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$sourcePath = (Resolve-Path -LiteralPath $Source).Path
$outputPath = [IO.Path]::GetFullPath($Output)
$expectedSource = '153D20AAE6D7C63A4C52689D91E8BEAD78F6D17A70E11EF4D03D19953383DE00'
if ((Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash -ne $expectedSource) { throw 'Pinned Poly Haven source hash mismatch.' }
if (-not (Test-Path -LiteralPath $Blender -PathType Leaf)) { throw "Blender is unavailable: $Blender" }

$textureHashes = [ordered]@{
    'fir_tree_01_bark_diff_1k.png'='508CA0A40D118BEEC98E8375B4611E8A16123EEC94B5D2B4870617CF4D24E6FA'
    'fir_tree_01_bark_nor_gl_1k.png'='B889E7080D6EFB0592F83E64CC5A5AFA5FB7787B4B7D1AE13A65602689E793E8'
    'fir_tree_01_bark_rough_1k.png'='B198105D3022DB6C459AE9C307ECDB7B644832EE20BCBFD89C3741CECC77881F'
    'fir_tree_01_trunk_c_diff_1k.png'='EDBCEB8657A036E98831A3550433AB2D78928FCF9060E400CA84748CEDCAF962'
    'fir_tree_01_trunk_c_nor_gl_1k.png'='08A40A7486AD3A8D97CF3DA0AFE5951F1A038CB51395020E2C4A2113F46D1610'
    'fir_tree_01_trunk_c_rough_1k.png'='48FCCED840B4BEED4E03E09A352A67DD36185C9CD4247090069391BFB9746DE9'
    'fir_tree_01_twig_diff_1k.png'='76F6D0C0044D76BAC08FBBD3D2AC1C9B66BB046DE7D0B1A22AA9B319308F3833'
    'fir_tree_01_twig_nor_gl_1k.png'='CBB6B8179A9F7F6E84FD15D1F422BC342841D20845B2BAD437612F03FB0067DE'
    'fir_tree_01_twig_rough_1k.png'='A4F0A4C8BC93946DEA229DEF779FFE5D32C555C717E4BF7B0E4B4D86BC21987A'
    'fir_tree_01_twig_alpha_1k.png'='3F399629337E7AC3AF27A01B08FFB9C0D60224A9DD374A1427761A5C2EEA9D6E'
}
$textureRoot = Join-Path (Split-Path -Parent $sourcePath) 'textures'
foreach ($entry in $textureHashes.GetEnumerator()) {
    $path = Join-Path $textureRoot $entry.Key
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.Value) {
        throw "Pinned offline texture is missing or changed: $($entry.Key)"
    }
}
[IO.Directory]::CreateDirectory($outputPath) | Out-Null
$blenderScript = Join-Path $PSScriptRoot 'blender/cook-poly-haven-fir.py'
& $Blender --background --python-exit-code 1 --python $blenderScript -- --source $sourcePath --output $outputPath --asset-name fir-tree-01-c
if ($LASTEXITCODE -ne 0) { throw "Blender fir cook failed with exit code $LASTEXITCODE" }

$converter = Join-Path $PSScriptRoot 'convert-glb-to-aemesh.py'
foreach ($lod in 0..2) {
    $glb = Join-Path $outputPath "fir-tree-01-c-lod$lod.glb"
    $meshOutput = Join-Path $outputPath "aemesh/lod$lod"
    $meshReceipt = Join-Path $outputPath "fir-tree-01-c-lod$lod-aemesh-receipt.json"
    & $Python $converter --source $glb --output-directory $meshOutput --receipt $meshReceipt --asset fir-tree-01-c --lod $lod
    if ($LASTEXITCODE -ne 0) { throw "AEMESH conversion failed for LOD $lod" }
}
& $Python (Join-Path $PSScriptRoot 'verify-poly-haven-fir-cook.py') --receipt (Join-Path $outputPath 'fir-tree-01-c-cook-receipt.json')
if ($LASTEXITCODE -ne 0) { throw 'Fir cook verification failed.' }
Write-Output "FIR_OFFLINE_COOK=PASS OUTPUT=$outputPath"
