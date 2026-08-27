param([string]$Blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe', [string]$BuildDirectory = 'build-native')
$ErrorActionPreference = 'Stop'
$temporary = Join-Path ([IO.Path]::GetTempPath()) ("aevrynd-aeskin-v2-" + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temporary) | Out-Null
try {
    & $Blender --background --python (Join-Path $PSScriptRoot 'blender/create-aeskin-v2-validation-asset.py') -- $temporary
    if ($LASTEXITCODE) { throw 'Blender fixture generation failed.' }
    $source = Join-Path $temporary 'aeskin-v2-validation.gltf'
    if (-not (Test-Path -LiteralPath $source)) { throw 'Blender fixture output is missing.' }
    $output = Join-Path $temporary 'validation.aeskin'
    & (Join-Path $PSScriptRoot 'convert-gltf-to-aeskin-v2.ps1') -Source $source -Output $output
    if ($LASTEXITCODE) { throw 'AESKIN v2 conversion failed.' }
    $bytes = [IO.File]::ReadAllBytes($output)
    $reader = [IO.BinaryReader]::new([IO.MemoryStream]::new($bytes, $false))
    try {
        if ($reader.ReadUInt32() -ne 0x4B534541 -or $reader.ReadUInt32() -ne 2 -or $reader.ReadUInt32() -ne 64 -or $reader.ReadUInt32() -ne 0) { throw 'Header identity failed.' }
        if ($reader.ReadUInt64() -ne $bytes.LongLength -or $reader.ReadUInt64() -ne 64 -or $reader.ReadUInt32() -ne 12) { throw 'Header bounds failed.' }
        $reader.BaseStream.Position = 64
        $types = for ($entry = 0; $entry -lt 12; ++$entry) {
            $type = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
            $version = $reader.ReadUInt16(); $flags = $reader.ReadUInt16()
            $offset = $reader.ReadUInt64()
            $size = $reader.ReadUInt64(); $count = $reader.ReadUInt32(); $stride = $reader.ReadUInt32()
            if ($type -eq 'VERT') { $vertexOffset = $offset }
            if ($type -eq 'MATL') { if ($version -ne 1 -or $flags -ne 0 -or $size -ne 128 -or $count -ne 2 -or $stride -ne 64) { throw 'MATL TOC contract failed.' }; $materialOffset = $offset }
            $type
        }
        if (($types -join ',') -ne 'VERT,INDX,JOIN,WGHT,IBND,JNAM,SUBM,MATL,CLIP,TRAK,KEYF,STRS') { throw 'TOC chunk order failed.' }
        $reader.BaseStream.Position = $vertexOffset + 16
        $firstVertexColor = $reader.ReadUInt32()
        if ($firstVertexColor -eq 0 -or $firstVertexColor -eq 0xFFFFFFFF) { throw 'Vertex color packing regression detected.' }
        $reader.BaseStream.Position = $materialOffset
        $slot = $reader.ReadUInt32(); $materialFlags = $reader.ReadUInt32(); $baseColor = 1..4 | ForEach-Object { $reader.ReadSingle() }; $emissive = 1..3 | ForEach-Object { $reader.ReadSingle() }; $emissiveStrength = $reader.ReadSingle(); $roughness = $reader.ReadSingle(); $metallic = $reader.ReadSingle(); $alphaCutoff = $reader.ReadSingle(); $normalScale = $reader.ReadSingle(); $occlusion = $reader.ReadSingle(); $reserved = $reader.ReadSingle()
        if ($slot -ne 0 -or $materialFlags -ne 4 -or [Math]::Abs($baseColor[0] - 0.8) -gt 0.001 -or [Math]::Abs($roughness - 0.35) -gt 0.001 -or [Math]::Abs($metallic - 0.1) -gt 0.001 -or $emissiveStrength -ne 1 -or $alphaCutoff -ne 0.5 -or $normalScale -ne 1 -or $occlusion -ne 1 -or $reserved -ne 0) { throw 'MATL payload or strict defaults failed.' }
    } finally { $reader.Dispose() }

    cmake --build $BuildDirectory --config Release --target AevryndSkinnedModelAssetV2Inspect
    if ($LASTEXITCODE) { throw 'Official AESKIN v2 inspector build failed.' }
    $inspector = Get-ChildItem -LiteralPath $BuildDirectory -Recurse -Filter AevryndSkinnedModelAssetV2Inspect.exe | Select-Object -First 1 -ExpandProperty FullName
    if (-not $inspector) { throw 'Official AESKIN v2 inspector was not found.' }
    $inspection = & $inspector $output
    if ($LASTEXITCODE -or $inspection -notmatch 'vertices=6 indices=6 joints=2 submeshes=2 materials=2 clips=1') { throw "Official loader rejected fixture: $inspection" }

    $stepSource = Join-Path $temporary 'step.gltf'
    $stepDocument = Get-Content -LiteralPath $source -Raw | ConvertFrom-Json
    foreach ($sampler in $stepDocument.animations[0].samplers) { $sampler | Add-Member -NotePropertyName interpolation -NotePropertyValue STEP -Force }
    [IO.File]::WriteAllText($stepSource, ($stepDocument | ConvertTo-Json -Depth 100 -Compress))
    $failed = $false
    try { & (Join-Path $PSScriptRoot 'convert-gltf-to-aeskin-v2.ps1') -Source $stepSource -Output (Join-Path $temporary 'step.aeskin') } catch { $failed = $_.Exception.Message -like '*STEP*unsupported*' }
    if (-not $failed) { throw 'STEP animation was not explicitly rejected.' }
    Write-Output "AESKIN_V2_CI=PASS bytes=$($bytes.Length) chunks=12 primitives=2 materials=2 animations=1 matl=validated loader=LoadDetailed"
} finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}
