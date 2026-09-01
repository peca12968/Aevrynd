param(
    [string]$Blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe',
    [string]$Bundle = $env:AEVRYND_HUMAN_BASE_MESH_BUNDLE,
    [string]$BuildDirectory = 'engine/build',
    [string]$ExpectedBlenderVersion = 'Blender 5.2.1 LTS'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
# 中文：允許 CI 或其他工作站覆寫 bundle；未指定時從 repo 同層的 AssetCache 探測，避免提交使用者機器絕對路徑。
if ([string]::IsNullOrWhiteSpace($Bundle)) {
    $workspaceRoot = [IO.Directory]::GetParent($projectRoot).FullName
    $Bundle = Join-Path $workspaceRoot 'AssetCache\BlenderHumanBaseMeshes\v1.4.1\human-base-meshes-bundle-v1.4.1\human_base_meshes_bundle.blend'
}
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
    foreach ($name in @('run-a', 'run-b', 'run-c')) {
        $root = Join-Path $temporary $name
        $female = Join-Path $root 'female'
        $male = Join-Path $root 'male'
        [IO.Directory]::CreateDirectory($female) | Out-Null
        [IO.Directory]::CreateDirectory($male) | Out-Null
        $blenderStdoutPath = Join-Path $root 'blender.stdout.txt'
        $blenderStderrPath = Join-Path $root 'blender.stderr.txt'
        $commandLine = '"{0}" --background --factory-startup --python "{1}" -- "{2}" "{3}" "{4}" > "{5}" 2> "{6}"' -f `
            $Blender, $generator, $Bundle, $female, $male, $blenderStdoutPath, $blenderStderrPath
        & cmd.exe /d /s /c $commandLine
        $blenderExitCode = $LASTEXITCODE
        $generationOutput = @(
            Get-Content -LiteralPath $blenderStdoutPath
            Get-Content -LiteralPath $blenderStderrPath
        )
        if ($blenderExitCode -ne 0) { throw "Character generation failed for $name." }
        $generationText = $generationOutput -join "`n"
        if ($generationText -notmatch 'CC0_CHARACTER=F2 .*GARMENT_FACES=1602 GARMENT_VERTICES=1679 GARMENT_RIM=158 GARMENT_BOUNDARY=158 COLLAR_VERTICES=124 GARMENT_TRIANGLES=3644') {
            throw "Female garment topology contract failed for $name."
        }
        if ($generationText -notmatch 'CC0_CHARACTER=M2 .*GARMENT_FACES=1581 GARMENT_VERTICES=1667 GARMENT_RIM=176 GARMENT_BOUNDARY=176 COLLAR_VERTICES=128 GARMENT_TRIANGLES=3642') {
            throw "Male garment topology contract failed for $name."
        }
        $runs += [pscustomobject]@{ Root = $root; Female = $female; Male = $male }
    }

    $femaleGltfA = Join-Path $runs[0].Female 'realistic-female.gltf'
    $maleGltfA = Join-Path $runs[0].Male 'realistic-male-character.gltf'
    $femaleAssetA = Join-Path $runs[0].Female 'realistic-female.aeskin'
    $maleAssetA = Join-Path $runs[0].Male 'realistic-male.aeskin'
    foreach ($run in $runs) {
        & $converter -Source (Join-Path $run.Female 'realistic-female.gltf') -Output (Join-Path $run.Female 'realistic-female.aeskin')
        & $converter -Source (Join-Path $run.Male 'realistic-male-character.gltf') -Output (Join-Path $run.Male 'realistic-male.aeskin')
    }
    foreach ($run in $runs[1..($runs.Count - 1)]) {
        [void](Assert-FileEqual $femaleGltfA (Join-Path $run.Female 'realistic-female.gltf') 'Female glTF JSON')
        [void](Assert-FileEqual $maleGltfA (Join-Path $run.Male 'realistic-male-character.gltf') 'Male glTF JSON')
        [void](Assert-FileEqual (Join-Path $runs[0].Female 'realistic-female.bin') (Join-Path $run.Female 'realistic-female.bin') 'Female glTF BIN')
        [void](Assert-FileEqual (Join-Path $runs[0].Male 'realistic-male-character.bin') (Join-Path $run.Male 'realistic-male-character.bin') 'Male glTF BIN')
        [void](Assert-FileEqual $femaleAssetA (Join-Path $run.Female 'realistic-female.aeskin') 'Female AESKIN')
        [void](Assert-FileEqual $maleAssetA (Join-Path $run.Male 'realistic-male.aeskin') 'Male AESKIN')
    }
    $femaleGltfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $femaleGltfA).Hash
    $maleGltfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $maleGltfA).Hash
    $femaleBinHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runs[0].Female 'realistic-female.bin')).Hash
    $maleBinHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runs[0].Male 'realistic-male-character.bin')).Hash
    $femaleAssetHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $femaleAssetA).Hash
    $maleAssetHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $maleAssetA).Hash

    $committedFemaleGltf = Join-Path $projectRoot 'games\character_lab\assets\model-packs\realistic-female\source\realistic-female.gltf'
    $committedFemaleBin = Join-Path $projectRoot 'games\character_lab\assets\model-packs\realistic-female\source\realistic-female.bin'
    $committedMaleGltf = Join-Path $projectRoot 'games\character_lab\assets\model-packs\realistic-male\source\realistic-male-character.gltf'
    $committedMaleBin = Join-Path $projectRoot 'games\character_lab\assets\model-packs\realistic-male\source\realistic-male-character.bin'
    $committedFemale = Join-Path $projectRoot 'games\character_lab\assets\model-packs\realistic-female\realistic-female.aeskin'
    $committedMale = Join-Path $projectRoot 'games\character_lab\assets\model-packs\realistic-male\realistic-male.aeskin'
    [void](Assert-FileEqual $femaleGltfA $committedFemaleGltf 'Committed female glTF JSON')
    [void](Assert-FileEqual (Join-Path $runs[0].Female 'realistic-female.bin') $committedFemaleBin 'Committed female glTF BIN')
    [void](Assert-FileEqual $maleGltfA $committedMaleGltf 'Committed male glTF JSON')
    [void](Assert-FileEqual (Join-Path $runs[0].Male 'realistic-male-character.bin') $committedMaleBin 'Committed male glTF BIN')
    [void](Assert-FileEqual $femaleAssetA $committedFemale 'Committed female AESKIN')
    [void](Assert-FileEqual $maleAssetA $committedMale 'Committed male AESKIN')

    $inspector = Join-Path ([IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))) 'bin\Release\AevryndSkinnedModelAssetV2Inspect.exe'
    if (-not (Test-Path -LiteralPath $inspector -PathType Leaf)) { throw "Inspector is missing: $inspector" }
    $femaleInspection = & $inspector $committedFemale
    if ($LASTEXITCODE -ne 0 -or $femaleInspection -notmatch 'vertices=20174 indices=102180 joints=24 submeshes=9 materials=9 clips=1') {
        throw "Female asset inspection failed: $femaleInspection"
    }
    $maleInspection = & $inspector $committedMale
    if ($LASTEXITCODE -ne 0 -or $maleInspection -notmatch 'vertices=20066 indices=101688 joints=24 submeshes=9 materials=9 clips=1') {
        throw "Male asset inspection failed: $maleInspection"
    }

    Write-Output "CHARACTER_ASSET_DETERMINISM=PASS blender='$version' female_gltf=$femaleGltfHash female_bin=$femaleBinHash female_aeskin=$femaleAssetHash male_gltf=$maleGltfHash male_bin=$maleBinHash male_aeskin=$maleAssetHash"
} finally {
    # 測試只刪除本次於系統暫存區建立的唯一目錄，避免長時間工作累積記憶體映射與磁碟資源。
    if (Test-Path -LiteralPath $temporary) { [IO.Directory]::Delete($temporary, $true) }
}
