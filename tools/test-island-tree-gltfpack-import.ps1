param([string]$AssetRoot = (Join-Path $PSScriptRoot '../engine/assets/realistic-cc0-first-batch/island-tree-01'))
$ErrorActionPreference = 'Stop'
$receipt = Get-Content -LiteralPath (Join-Path $AssetRoot 'island-tree-01-lod-receipt.json') -Raw | ConvertFrom-Json
if ($receipt.decimator.name -ne 'gltfpack' -or $receipt.decimator.tag -ne 'v1.2' -or $receipt.decimator.commit -ne '9d9890c73011d75920af614485296d1e03e95448') { throw 'Pinned gltfpack provenance failed.' }
if ($receipt.decimator.archiveSha256 -ne 'e40f71b809cdf3361b9a4def85fd44534e8733ce29d4b943c145b76859e4c2b4' -or $receipt.decimator.executableSha256 -ne '1517a5e6fd3af9fc8a07b57e9bc018d882fec1058e77120777db9c79d938288b' -or $receipt.decimator.license -ne 'MIT') { throw 'Pinned gltfpack archive, executable, or license provenance failed.' }
if ($receipt.decimator.memoryGate -ne 'pass' -or $receipt.decimator.memoryGateMethod -ne 'gltfpack-process-tree' -or $receipt.decimator.peakProcessTreeBytes -le 0 -or $receipt.decimator.peakProcessTreeBytes -ge $receipt.decimator.peakLimitBytes -or $receipt.decimator.memorySampleCount -lt 3 -or $receipt.decimator.maxProcessCount -lt 1) { throw 'gltfpack process-tree memory gate failed.' }
$levels = @($receipt.levels)
if ($levels.Count -ne 3 -or $levels[1].triangles -ge $levels[0].triangles -or $levels[2].triangles -ge $levels[1].triangles) { throw 'LOD triangle gate failed.' }
foreach ($level in $levels) {
    if (-not $level.hasUv -or -not $level.hasNormals -or -not $level.materialSlots -or -not $level.visualNonEmpty) { throw "LOD $($level.level) channel or visual gate failed." }
    $path = Join-Path $AssetRoot $level.file
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $level.sha256) { throw "LOD $($level.level) hash gate failed." }
}
$runtime = Get-Content -LiteralPath (Join-Path (Split-Path $AssetRoot -Parent) 'runtime-asset-manifest.json') -Raw | ConvertFrom-Json
if ($runtime.memoryGate -ne 'pass' -or $runtime.memoryGateMethod -ne 'gltfpack-process-tree' -or $runtime.peakProcessTreeBytes -ne $receipt.decimator.peakProcessTreeBytes) { throw 'Runtime memory gate does not match the import receipt.' }
Write-Output "ISLAND_TREE_GLTFPACK_TEST=PASS peak=$($receipt.decimator.peakProcessTreeBytes) triangles=$($levels.triangles -join '->')"
