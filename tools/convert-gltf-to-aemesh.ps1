param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Output,
    [float]$Scale = 1
)
$ErrorActionPreference = 'Stop'
$sourcePath = (Resolve-Path -LiteralPath $Source).Path
$document = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json
if ($document.buffers.Count -ne 1 -or $document.meshes.Count -ne 1) { throw 'Only one-buffer, one-mesh glTF assets are supported.' }
$binaryPath = Join-Path (Split-Path -Parent $sourcePath) $document.buffers[0].uri
$binary = [System.IO.File]::ReadAllBytes($binaryPath)

function Read-Accessor([int]$Index) {
    $accessor = $document.accessors[$Index]
    $view = $document.bufferViews[$accessor.bufferView]
    $componentCount = @{ SCALAR = 1; VEC2 = 2; VEC3 = 3 }[$accessor.type]
    $componentSize = @{ 5123 = 2; 5125 = 4; 5126 = 4 }[[int]$accessor.componentType]
    if (-not $componentCount -or -not $componentSize) { throw 'Unsupported glTF accessor format.' }
    $stride = if ($view.byteStride) { [int]$view.byteStride } else { $componentCount * $componentSize }
    $offset = [int]$view.byteOffset + [int]$accessor.byteOffset
    $values = [System.Collections.Generic.List[object]]::new([int]$accessor.count)
    $stream = [System.IO.MemoryStream]::new($binary, $false)
    $reader = [System.IO.BinaryReader]::new($stream)
    for ($item = 0; $item -lt $accessor.count; ++$item) {
        $stream.Position = $offset + $item * $stride
        $components = [float[]]::new($componentCount)
        for ($component = 0; $component -lt $componentCount; ++$component) {
            $components[$component] = switch ([int]$accessor.componentType) { 5123 { $reader.ReadUInt16() } 5125 { $reader.ReadUInt32() } 5126 { $reader.ReadSingle() } }
        }
        $values.Add($components)
    }
    $reader.Dispose()
    return $values
}

function Convert-SnormChannel([float]$Value) {
    $clamped = [Math]::Max(-1.0, [Math]::Min(1.0, $Value))
    $rounded = [int]($clamped * 127.0 + $(if ($clamped -ge 0) { 0.5 } else { -0.5 }))
    return [byte]($rounded -band 255)
}

function Convert-PackedNormal([float[]]$Value) {
    $length = [Math]::Sqrt($Value[0] * $Value[0] + $Value[1] * $Value[1] + $Value[2] * $Value[2])
    if ($length -lt 0.000001) { $Value = [float[]](0, 1, 0); $length = 1 }
    $x = Convert-SnormChannel ($Value[0] / $length)
    $y = Convert-SnormChannel ($Value[1] / $length)
    $z = Convert-SnormChannel (-$Value[2] / $length)
    return [uint32]([uint32]$x -bor ([uint32]$y -shl 8) -bor ([uint32]$z -shl 16) -bor ([uint32]127 -shl 24))
}

$primitive = $document.meshes[0].primitives[0]
$positions = Read-Accessor $primitive.attributes.POSITION
$normals = Read-Accessor $primitive.attributes.NORMAL
$textureCoordinates = Read-Accessor $primitive.attributes.TEXCOORD_0
$indexValues = Read-Accessor $primitive.indices
if ($positions.Count -ne $normals.Count -or $positions.Count -ne $textureCoordinates.Count -or $indexValues.Count % 3) { throw 'glTF attribute counts are inconsistent.' }
$outputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Output))
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($outputPath)) | Out-Null
$stream = [System.IO.File]::Open($outputPath, [System.IO.FileMode]::Create)
$writer = [System.IO.BinaryWriter]::new($stream)
$writer.Write([uint32]0x334D4541); $writer.Write([uint32]1); $writer.Write([uint32]$positions.Count); $writer.Write([uint32]$indexValues.Count)
for ($index = 0; $index -lt $positions.Count; ++$index) {
    $position = $positions[$index]; $uv = $textureCoordinates[$index]
    $writer.Write([float]($position[0] * $Scale)); $writer.Write([float]($position[1] * $Scale)); $writer.Write([float](-$position[2] * $Scale)); $writer.Write((Convert-PackedNormal $normals[$index])); $writer.Write([uint32]4294967295); $writer.Write([float]$uv[0]); $writer.Write([float]$uv[1])
}
for ($triangle = 0; $triangle -lt $indexValues.Count; $triangle += 3) {
    $writer.Write([uint32]$indexValues[$triangle][0]); $writer.Write([uint32]$indexValues[$triangle + 2][0]); $writer.Write([uint32]$indexValues[$triangle + 1][0])
}
$writer.Dispose()
$length = (Get-Item -LiteralPath $outputPath).Length
Write-Output "vertices=$($positions.Count) triangles=$($indexValues.Count / 3) bytes=$length"
