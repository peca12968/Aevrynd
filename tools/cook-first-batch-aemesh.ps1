param(
    [string]$AssetRoot = "engine/assets/realistic-cc0-first-batch",
    [string]$OutputRoot = "engine/assets/realistic-cc0-first-batch/cooked/meshes",
    [string]$PythonExecutable = ''
)
$ErrorActionPreference = 'Stop'
$bundledPython = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$python = if ($PythonExecutable) { $PythonExecutable } elseif (Test-Path -LiteralPath $bundledPython -PathType Leaf) { $bundledPython } else { (Get-Command python -ErrorAction Stop).Source }
if (& $python --version 2>&1 | Select-String -Quiet '^Python 3\.') { } else { throw 'A functional Python 3 executable is required.' }
$converter = Join-Path $PSScriptRoot 'convert-glb-to-aemesh.py'
$assetRootPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $AssetRoot))
$outputRootPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputRoot))
$models = @('boulder-01', 'coastal-cliff-01', 'dead-tree-trunk-02', 'island-tree-01')
$aggregate = [System.Collections.Generic.List[object]]::new()
foreach ($model in $models) {
    for ($lod = 0; $lod -lt 3; ++$lod) {
        $source = Join-Path $assetRootPath "$model/$model-lod$lod.glb"
        $destination = Join-Path $outputRootPath $model
        $receipt = Join-Path $destination "$model-lod$lod-aemesh-receipt.json"
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing GLB: $source" }
        & $python $converter --source $source --output-directory $destination --receipt $receipt --asset $model --lod $lod
        if ($LASTEXITCODE -ne 0) { throw "AEMESH conversion failed: $model LOD $lod" }
        $parsed = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
        if (-not $parsed.memoryGate -or $parsed.peakWorkingSetScope -ne 'converter-process-no-children' -or -not $parsed.materialPrimitiveBijection -or $parsed.outputCount -lt 1) { throw "AEMESH receipt gate failed: $model LOD $lod" }
        $aggregate.Add($parsed)
    }
}
$allOutputs = @($aggregate | ForEach-Object outputs)
$manifest = [ordered]@{
    schemaVersion = 1
    converter = 'convert-glb-to-aemesh.py'
    assets = $models
    lodReceipts = $aggregate.Count
    outputFiles = $allOutputs.Count
    totalBytes = [long](($allOutputs | Measure-Object bytes -Sum).Sum)
    totalTriangles = [long](($allOutputs | Measure-Object triangles -Sum).Sum)
    maximumPeakWorkingSetBytes = [long](($aggregate | Measure-Object peakWorkingSetBytes -Maximum).Maximum)
    peakWorkingSetScope = 'converter-process-no-children'
    peakLimitBytes = 2GB
    memoryGate = [bool](($aggregate | Where-Object { -not $_.memoryGate }).Count -eq 0)
    islandMaterialSlotsByLod = @($aggregate | Where-Object assetName -eq 'island-tree-01' | ForEach-Object { [ordered]@{ lod = $_.lod; slots = @($_.outputs.materialIndex | Sort-Object -Unique) } })
}
$manifestPath = Join-Path $outputRootPath 'first-batch-aemesh-manifest.json'
$manifestJson = ($manifest | ConvertTo-Json -Depth 12) + "`n"
[System.IO.File]::WriteAllText($manifestPath, $manifestJson, [System.Text.UTF8Encoding]::new($false))
Write-Output ("AEMESH_BATCH=" + ($manifest | ConvertTo-Json -Compress -Depth 12))
