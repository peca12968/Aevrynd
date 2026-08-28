param(
    [string]$Blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe',
    [string]$Bundle = 'D:\Download\Aevrynd\AssetCache\BlenderHumanBaseMeshes\v1.4.1\human-base-meshes-bundle-v1.4.1\human_base_meshes_bundle.blend',
    [string]$BuildDirectory = 'engine/build',
    [string]$ExpectedBlenderVersion = 'Blender 5.2.1 LTS'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$temporary = Join-Path ([IO.Path]::GetTempPath()) ("aevrynd-character-determinism-" + [Guid]::NewGuid().ToString('N'))
$generator = Join-Path $PSScriptRoot 'blender\create-cc0-f2-m2-characters.py'
$converter = Join-Path $PSScriptRoot 'convert-gltf-to-aeskin-v2.ps1'

function Assert-FileEqual([string]$Left, [string]$Right, [string]$Label) {
    $leftHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Left).Hash
    $rightHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Right).Hash
    if ($leftHash -ne $rightHash) { throw "$Label is not deterministic: $leftHash != $rightHash" }
    return $leftHash
}

try {
    foreach ($required in @($Blender, $Bundle, $generator, $converter)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file is missing: $required" }
    }
    $version = [string](& $Blender --version | Select-Object -First 1)
    $version = $version.Trim()
    if ($version -ne $ExpectedBlenderVersion) {
        throw "Blender version is not pinned: expected '$ExpectedBlenderVersion', received '$version'."
    }

    [IO.Directory]::CreateDirectory($temporary) | Out-Null
    $runs = @()
    foreach ($name in @('run-a', 'run-b')) {
        $root = Join-Path $temporary $name
        $female = Join-Path $root 'female'
        $male = Join-Path $root 'male'
        [IO.Directory]::CreateDirectory($female) | Out-Null
        [IO.Directory]::CreateDirectory($male) | Out-Null
        $generationOutput = @(& $Blender --background --factory-startup --python $generator -- $Bundle $female $male 2>&1)
        if ($LASTEXITCODE -ne 0) { throw "Character generation failed for $name." }
        $generationText = $generationOutput -join "`n"
        if ($generationText -notmatch 'CC0_CHARACTER=F2 .*GARMENT_FACES=1608 GARMENT_VERTICES=1685 GARMENT_RIM=158 GARMENT_BOUNDARY=158 COLLAR_VERTICES=124 GARMENT_TRIANGLES=3656') {
            throw "Female garment topology contract failed for $name."
        }
        if ($generationText -notmatch 'CC0_CHARACTER=M2 .*GARMENT_FACES=1560 GARMENT_VERTICES=1646 GARMENT_RIM=176 GARMENT_BOUNDARY=176 COLLAR_VERTICES=128 GARMENT_TRIANGLES=3600') {
            throw "Male garment topology contract failed for $name."
        }
        $runs += [pscustomobject]@{ Root = $root; Female = $female; Male = $male }
    }

    $femaleGltfA = Join-Path $runs[0].Female 'realistic-female.gltf'
    $femaleGltfB = Join-Path $runs[1].Female 'realistic-female.gltf'
    $maleGltfA = Join-Path $runs[0].Male 'realistic-male-character.gltf'
    $maleGltfB = Join-Path $runs[1].Male 'realistic-male-character.gltf'
    # glTF JSON 應完全固定；Blender 的三角形輸出順序則由轉換器正規化。
    $femaleGltfHash = Assert-FileEqual $femaleGltfA $femaleGltfB 'Female glTF JSON'
    $maleGltfHash = Assert-FileEqual $maleGltfA $maleGltfB 'Male glTF JSON'

    $femaleAssetA = Join-Path $runs[0].Female 'realistic-female.aeskin'
    $femaleAssetB = Join-Path $runs[1].Female 'realistic-female.aeskin'
    $maleAssetA = Join-Path $runs[0].Male 'realistic-male.aeskin'
    $maleAssetB = Join-Path $runs[1].Male 'realistic-male.aeskin'
    & $converter -Source $femaleGltfA -Output $femaleAssetA
    & $converter -Source $femaleGltfB -Output $femaleAssetB
    & $converter -Source $maleGltfA -Output $maleAssetA
    & $converter -Source $maleGltfB -Output $maleAssetB
    $femaleAssetHash = Assert-FileEqual $femaleAssetA $femaleAssetB 'Female AESKIN'
    $maleAssetHash = Assert-FileEqual $maleAssetA $maleAssetB 'Male AESKIN'

    $committedFemale = Join-Path $projectRoot 'games\character_lab\assets\model-packs\realistic-female\realistic-female.aeskin'
    $committedMale = Join-Path $projectRoot 'games\character_lab\assets\model-packs\realistic-male\realistic-male.aeskin'
    [void](Assert-FileEqual $femaleAssetA $committedFemale 'Committed female AESKIN')
    [void](Assert-FileEqual $maleAssetA $committedMale 'Committed male AESKIN')

    $inspector = Join-Path ([IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))) 'bin\Release\AevryndSkinnedModelAssetV2Inspect.exe'
    if (-not (Test-Path -LiteralPath $inspector -PathType Leaf)) { throw "Inspector is missing: $inspector" }
    $femaleInspection = & $inspector $committedFemale
    if ($LASTEXITCODE -ne 0 -or $femaleInspection -notmatch 'vertices=20236 indices=103236 joints=20 submeshes=9 materials=9 clips=1') {
        throw "Female asset inspection failed: $femaleInspection"
    }
    $maleInspection = & $inspector $committedMale
    if ($LASTEXITCODE -ne 0 -or $maleInspection -notmatch 'vertices=20216 indices=102948 joints=20 submeshes=9 materials=9 clips=1') {
        throw "Male asset inspection failed: $maleInspection"
    }

    Write-Output "CHARACTER_ASSET_DETERMINISM=PASS blender='$version' female_gltf=$femaleGltfHash male_gltf=$maleGltfHash female_aeskin=$femaleAssetHash male_aeskin=$maleAssetHash"
} finally {
    # 測試只刪除本次於系統暫存區建立的唯一目錄，避免長時間工作累積記憶體映射與磁碟資源。
    if (Test-Path -LiteralPath $temporary) { [IO.Directory]::Delete($temporary, $true) }
}
