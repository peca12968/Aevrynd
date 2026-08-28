param(
    [string]$BuildDirectory = 'engine/build',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')][string]$Configuration = 'Release',
    [int]$Parallel = 4
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$resolvedBuild = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
$blender = Get-ChildItem 'C:\Program Files\Blender Foundation' -Recurse -Filter blender.exe -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake is not installed.' }
if (-not $blender) { throw 'Blender is not installed.' }
# 將外部輸入限制在保守的工作數，避免長時間資產驗證推高記憶體。
$effectiveParallel = [Math]::Max(1, [Math]::Min(4, $Parallel))

$fixtureDirectory = Join-Path $resolvedBuild 'skinned-asset-pipeline'
New-Item -ItemType Directory -Force -Path $fixtureDirectory | Out-Null
$generator = Join-Path $projectRoot 'tools\blender\create-skinned-validation-asset.py'
$converter = Join-Path $projectRoot 'tools\convert-gltf-to-aeskin.ps1'
$gltfPath = Join-Path $fixtureDirectory 'validation-character.gltf'
$assetPath = Join-Path $fixtureDirectory 'validation-character.aeskin'

& $blender --background --factory-startup --python $generator -- $fixtureDirectory
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $gltfPath)) { throw 'Blender skinned glTF generation failed.' }
& $converter -Source $gltfPath -Output $assetPath
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $assetPath)) { throw 'glTF to AESK conversion failed.' }
& $cmake --build $resolvedBuild --config $Configuration --parallel $effectiveParallel --target AevryndSkinnedModelAssetTest
if ($LASTEXITCODE -ne 0) { throw 'Skinned model asset test build failed.' }
$testExecutable = Join-Path $resolvedBuild "bin\$Configuration\AevryndSkinnedModelAssetTest.exe"
& $testExecutable $assetPath
if ($LASTEXITCODE -ne 0) { throw 'Skinned asset end-to-end validation failed.' }

$gltf = Get-Item -LiteralPath $gltfPath
$asset = Get-Item -LiteralPath $assetPath
[pscustomobject]@{
    blender = (& $blender --version | Select-Object -First 1)
    gltfBytes = $gltf.Length
    aeskinBytes = $asset.Length
    testPassed = $true
}
