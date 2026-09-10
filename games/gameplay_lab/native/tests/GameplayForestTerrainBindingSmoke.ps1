param([Parameter(Mandatory=$true)][string]$SourceRoot,[Parameter(Mandatory=$true)][string]$AssetRoot)
$ErrorActionPreference='Stop'
$source=Get-Content (Join-Path $SourceRoot 'games/gameplay_lab/native/src/GameplayLabGame.cpp') -Raw
$manifest=Get-Content (Join-Path $AssetRoot 'photoreal-cooked/runtime-render-manifest.json') -Raw|ConvertFrom-Json
$hashes=@('00850c7c033be2bcb2818c0ab3db91a62c8353eba746daede3c0de373c3e32b4','3427bf0a83e0c33326a207f700305620402fd0fb6c532240f204bcebefeabcc4','56e687b2a208f2eaaf9b0e3670530f0a36e5e88d6e74b26090633daaa4e9db93')
$names=@('base','arm','normal');$formats=@('BC7_UNORM_SRGB','BC7_UNORM','BC5_UNORM');$dxgi=@(99,98,83)
function Assert-Binding([string]$text){
  $resolution=[regex]::Matches($text,'terrain\.textures\.maximumResolution\s*=\s*([^;]+);')
  if($resolution.Count-ne1 -or $resolution[0].Groups[1].Value.Trim()-ne'2048'){throw 'terrain maximumResolution must be 2048'}
  $root=[regex]::Match($text,'const auto forestLayerRoot=[^;]+;').Value
  $layer=[regex]::Match($text,'terrain\.textures\.additionalLayers\[0\]=[^;]+;').Value
  if($root -ne 'const auto forestLayerRoot=assetRoot/"photoreal-cooked/materials/forest-ground-04";'){throw 'forest root mismatch'}
  if(!$layer.Contains('MaterialSurfaceMapEncoding3D::PackedOrm') -or $layer.Contains('RoughnessR')){throw 'forest encoding mismatch'}
  foreach($name in $names){if(!$layer.Contains('forestLayerRoot/"slot-0-'+$name+'.dds"')){throw 'forest map mismatch'}}
  foreach($hash in $hashes){if(!$layer.Contains('MaterialTextureIntegrity3D{5592580,"'+$hash+'"}')){throw 'forest seal mismatch'}}
  if(!$text.Contains('MaterialAssetDefinition3D{"ground",{"ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg"')){throw 'base semantic mismatch'}
  $rock=[regex]::Match($text,'terrain\.textures\.additionalLayers\[1\]=[^;]+;').Value
  if(!$rock.Contains('poly-haven/rock_surface/1k/base-color.jpg') -or !$rock.Contains('MaterialSurfaceMapEncoding3D::RoughnessR')){throw 'rock semantic mismatch'}
}
Assert-Binding $source
$resolutionRejected=$false
try{Assert-Binding ($source.Replace('terrain.textures.maximumResolution=2048;','terrain.textures.maximumResolution=1024;'))}catch{$resolutionRejected=$true}
if(!$resolutionRejected){throw 'hostile 1024 resolution accepted'}
foreach($mutation in @($source.Replace('photoreal-cooked/materials/forest-ground-04','photoreal-cooked/materials/Ground108'),$source.Replace('photoreal-cooked/materials/forest-ground-04','poly-haven/aerial_ground_rock/1k'),$source.Replace('MaterialSurfaceMapEncoding3D::PackedOrm','MaterialSurfaceMapEncoding3D::RoughnessR'),$source.Replace($hashes[0],('0'*64)))){
  $rejected=$false;try{Assert-Binding $mutation}catch{$rejected=$true};if(!$rejected){throw 'hostile binding accepted'}
}
for($i=0;$i-lt3;$i++){
  $relative='materials/forest-ground-04/slot-0-'+$names[$i]+'.dds'
  $entry=@($manifest.files|Where-Object path -eq $relative)
  if($entry.Count-ne1 -or $entry[0].bytes-ne5592580 -or $entry[0].sha256-ne$hashes[$i] -or $entry[0].format-ne$formats[$i]){throw 'manifest seal mismatch'}
  $path=Join-Path $AssetRoot ('photoreal-cooked/'+$relative)
  $bytes=[IO.File]::ReadAllBytes($path)
  $sha=[System.Security.Cryptography.SHA256]::Create()
  try{$actualSha=[BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
  if($bytes.Length-ne5592580 -or $actualSha-ne$hashes[$i] -or [Text.Encoding]::ASCII.GetString($bytes,0,4)-ne'DDS ' -or [Text.Encoding]::ASCII.GetString($bytes,84,4)-ne'DX10' -or [BitConverter]::ToUInt32($bytes,128)-ne$dxgi[$i]){throw 'DDS integrity/format mismatch'}
  Write-Output "FOREST_MAP_PASS $relative bytes=$($bytes.Length) DXGI=$($dxgi[$i])"
}
Write-Output 'FOREST_BINDING_PASS layers=Ground037/forest_ground_04/rock_surface maximumResolution=2048 hostile=5 source_contract_only GPU_NOT_RUN'
