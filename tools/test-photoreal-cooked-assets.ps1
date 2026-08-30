[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$CookedRoot
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $CookedRoot).Path
$manifestPath = Join-Path $root 'material-ibl-asset-tree.manifest.json'
function Read-DdsContract([string]$Path) {
    [byte[]]$data = [IO.File]::ReadAllBytes($Path)
    if ($data.Length -lt 148 -or [Text.Encoding]::ASCII.GetString($data,0,4) -cne 'DDS ' -or [Text.Encoding]::ASCII.GetString($data,84,4) -cne 'DX10') { throw "DDS DX10 header failed: $Path" }
    $width=[BitConverter]::ToUInt32($data,16);$height=[BitConverter]::ToUInt32($data,12);$mips=[BitConverter]::ToUInt32($data,28)
    $format=[BitConverter]::ToUInt32($data,128);$dimension=[BitConverter]::ToUInt32($data,132);$misc=[BitConverter]::ToUInt32($data,136);$array=[BitConverter]::ToUInt32($data,140);$caps2=[BitConverter]::ToUInt32($data,112)
    if (!$width -or !$height -or !$mips -or $dimension -ne 3 -or $array -ne 1) { throw "DDS topology header failed: $Path" }
    [uint64]$expected=148;[uint32]$faces=if($misc -eq 4){6}else{1}
    if (($faces -eq 6 -and $caps2 -ne 0xFE00) -or ($faces -eq 1 -and $caps2 -ne 0)) { throw "DDS cube caps failed: $Path" }
    foreach($face in 1..$faces){foreach($mip in 0..($mips-1)){$w=[Math]::Max(1,$width-shr$mip);$h=[Math]::Max(1,$height-shr$mip);if($format -eq 34){$expected += [uint64]$w*$h*4}else{$expected += [uint64](($w+3)-shr2)*(($h+3)-shr2)*16}}}
    if ($expected -ne [uint64]$data.Length) { throw "DDS exact subresource length failed: $Path" }
    [pscustomobject]@{width=$width;height=$height;mips=$mips;format=$format;faces=$faces;bytes=[uint64]$data.Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$files = @('materials','ibl' | ForEach-Object { Get-ChildItem -LiteralPath (Join-Path $root $_) -Recurse -File })
if ($manifest.schemaVersion -ne 1 -or $manifest.fileCount -ne $files.Count) { throw 'Cooked tree count mismatch.' }
[uint64]$bytes = 0
foreach ($entry in $manifest.files) {
    if ($entry.path -notmatch '^[A-Za-z0-9._/-]+$' -or $entry.path -match '(^|/)\.\.(/|$)') { throw "Unsafe manifest path: $($entry.path)" }
    $path = Join-Path $root $entry.path
    $item = Get-Item -LiteralPath $path
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $item.Length -ne $entry.bytes) { throw "Cooked file metadata mismatch: $($entry.path)" }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $entry.sha256) { throw "Cooked file hash mismatch: $($entry.path)" }
    $bytes += [uint64]$item.Length
}
if ($bytes -ne [uint64]$manifest.totalBytes) { throw 'Cooked tree total byte mismatch.' }
$material = Get-Content -LiteralPath (Join-Path $root 'materials/material-bake-receipt.json') -Raw | ConvertFrom-Json
$ibl = Get-Content -LiteralPath (Join-Path $root 'ibl/ibl-bake-receipt.json') -Raw | ConvertFrom-Json
if ($material.status -ne 'pass' -or $material.quality -ne 'Ultra' -or $material.mipPolicy -ne 'complete-chain' -or
    $material.memoryGate -ne 'pass' -or [uint64]$material.peakWorkingSetLimitBytes -ne 2GB -or @($material.assets).Count -ne 9) { throw 'Material receipt gate failed.' }
$expectedAssets = @('boulder-01','bricks100','coastal-cliff-01','dead-tree-trunk-02','forest-ground-04','ground108','island-tree-01','roofing-tiles014-b','stony-dirt-path')
$actualAssetSet = @($material.assets.assetName | Sort-Object) -join ','
$expectedAssetSet = $expectedAssets -join ','
if ($actualAssetSet -cne $expectedAssetSet) { throw 'Material exact asset set failed.' }
$materialOutputs = @($material.assets.materials.outputs)
if ($materialOutputs.Count -ne 33) { throw 'Material DDS count failed.' }
foreach ($asset in $material.assets) {
    if ($asset.license -ne 'CC0-1.0' -or !$asset.commercialUse -or $asset.outputResolution -notin @(1024,2048) -or
        $asset.expectedMipCount -ne ([Math]::Log($asset.outputResolution,2)+1)) { throw "Material provenance or resolution tier failed: $($asset.assetName)" }
    foreach ($output in @($asset.materials.outputs)) {
        $actual = Read-DdsContract (Join-Path $root "materials/$($output.path)")
        if ($output.mips -ne $asset.expectedMipCount -or [Math]::Max($output.width,$output.height) -ne $asset.outputResolution -or
            ($output.channel -eq 'base' -and $output.format -ne 99) -or ($output.channel -eq 'normal' -and $output.format -ne 83) -or
            ($output.channel -eq 'arm' -and $output.format -ne 98) -or $actual.faces -ne 1 -or $actual.width -ne $output.width -or
            $actual.height -ne $output.height -or $actual.mips -ne $output.mips -or $actual.format -ne $output.format -or
            $actual.bytes -ne $output.bytes -or $actual.sha256 -cne $output.sha256) { throw "Material DDS topology failed: $($output.path)" }
    }
}
if ($ibl.status -ne 'pass' -or $ibl.sourceLicense -ne 'CC0-1.0' -or !$ibl.commercialUse -or
    $ibl.memoryGate -ne 'pass' -or [uint64]$ibl.peakWorkingSetLimitBytes -ne 2GB) { throw 'IBL provenance or memory gate failed.' }
if ($ibl.tool.sha256 -cne 'dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06') { throw 'DirectXTex pin failed.' }
$iblOutputs = @($ibl.outputs)
if ($iblOutputs.Count -ne 3 -or @($iblOutputs | Where-Object {($_.path -like '*specular*' -and ($_.format -ne 95 -or $_.mips -ne 8)) -or
    ($_.path -like '*irradiance*' -and ($_.format -ne 95 -or $_.mips -ne 6)) -or ($_.path -like '*brdf*' -and ($_.format -ne 34 -or $_.mips -ne 1))}).Count) { throw 'IBL DDS topology failed.' }
foreach($output in $iblOutputs){$actual=Read-DdsContract (Join-Path $root "ibl/$($output.path)");$expectedFaces=if($output.path -like '*brdf*'){1}else{6};if($actual.faces-ne$expectedFaces-or$actual.width-ne$output.width-or$actual.height-ne$output.height-or$actual.mips-ne$output.mips-or$actual.format-ne$output.format-or$actual.bytes-ne$output.bytes-or$actual.sha256-cne$output.sha256){throw "IBL DDS receipt mismatch: $($output.path)"}}
"files=$($manifest.fileCount) bytes=$bytes materials=$($materialOutputs.Count) ibl=$($iblOutputs.Count) memoryGate=pass"
