param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Output,
    [float]$Scale = 1
)

$ErrorActionPreference = 'Stop'
if (-not [float]::IsFinite($Scale) -or $Scale -le 0) { throw 'Scale must be finite and greater than zero.' }

$sourcePath = (Resolve-Path -LiteralPath $Source).Path
$document = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json
if (@($document.buffers).Count -ne 1 -or @($document.meshes).Count -ne 1 -or @($document.skins).Count -ne 1) {
    throw 'Exactly one buffer, mesh, and skin are required.'
}

$bufferUri = [string]$document.buffers[0].uri
if ($bufferUri.StartsWith('data:')) {
    $comma = $bufferUri.IndexOf(',')
    if ($comma -lt 0 -or -not $bufferUri.Substring(0, $comma).EndsWith(';base64')) { throw 'Only base64 data buffers are supported.' }
    $binary = [Convert]::FromBase64String($bufferUri.Substring($comma + 1))
} else {
    $binaryPath = Join-Path (Split-Path -Parent $sourcePath) $bufferUri
    $binary = [System.IO.File]::ReadAllBytes($binaryPath)
}
if ([long]$document.buffers[0].byteLength -ne $binary.LongLength) { throw 'glTF buffer length is inconsistent.' }

function Read-Accessor([int]$Index) {
    if ($Index -lt 0 -or $Index -ge @($document.accessors).Count) { throw 'glTF accessor index is invalid.' }
    $accessor = $document.accessors[$Index]
    if ($null -ne $accessor.sparse) { throw 'Sparse glTF accessors are not supported.' }
    $viewIndex = [int]$accessor.bufferView
    if ($viewIndex -lt 0 -or $viewIndex -ge @($document.bufferViews).Count) { throw 'glTF buffer view index is invalid.' }
    $view = $document.bufferViews[$viewIndex]
    $componentCount = @{ SCALAR = 1; VEC2 = 2; VEC3 = 3; VEC4 = 4; MAT4 = 16 }[[string]$accessor.type]
    $componentSize = @{ 5121 = 1; 5123 = 2; 5125 = 4; 5126 = 4 }[[int]$accessor.componentType]
    if (-not $componentCount -or -not $componentSize) { throw 'Unsupported glTF accessor format.' }
    $packedStride = $componentCount * $componentSize
    $stride = if ($null -ne $view.byteStride) { [int]$view.byteStride } else { $packedStride }
    if ($stride -lt $packedStride) { throw 'glTF accessor stride is invalid.' }
    $viewOffset = if ($null -ne $view.byteOffset) { [long]$view.byteOffset } else { 0L }
    $accessorOffset = if ($null -ne $accessor.byteOffset) { [long]$accessor.byteOffset } else { 0L }
    $offset = $viewOffset + $accessorOffset
    $count = [int]$accessor.count
    if ($count -le 0 -or $offset -lt 0) { throw 'glTF accessor range is invalid.' }
    $end = $offset + [long]($count - 1) * $stride + $packedStride
    if ($end -gt $binary.LongLength -or $end -gt $viewOffset + [long]$view.byteLength) { throw 'glTF accessor exceeds its buffer view.' }
    $normalized = $accessor.normalized -eq $true
    $values = [System.Collections.Generic.List[object]]::new($count)
    $stream = [System.IO.MemoryStream]::new($binary, $false)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        for ($item = 0; $item -lt $count; ++$item) {
            $stream.Position = $offset + [long]$item * $stride
            $components = [float[]]::new($componentCount)
            for ($component = 0; $component -lt $componentCount; ++$component) {
                $value = switch ([int]$accessor.componentType) {
                    5121 { [float]$reader.ReadByte() }
                    5123 { [float]$reader.ReadUInt16() }
                    5125 { [float]$reader.ReadUInt32() }
                    5126 { $reader.ReadSingle() }
                }
                if ($normalized) {
                    if ([int]$accessor.componentType -eq 5121) { $value /= 255.0 }
                    elseif ([int]$accessor.componentType -eq 5123) { $value /= 65535.0 }
                    elseif ([int]$accessor.componentType -ne 5126) { throw 'Unsupported normalized accessor component type.' }
                }
                $components[$component] = [float]$value
            }
            $values.Add($components)
        }
    } finally {
        $reader.Dispose()
    }
    return [pscustomobject]@{ Count = $count; Components = $componentCount; ComponentType = [int]$accessor.componentType; Values = $values }
}

function Convert-SnormChannel([float]$Value) {
    $clamped = [Math]::Max(-1.0, [Math]::Min(1.0, $Value))
    $rounded = [int]($clamped * 127.0 + $(if ($clamped -ge 0) { 0.5 } else { -0.5 }))
    return [byte]($rounded -band 255)
}

function Convert-PackedNormal([float[]]$Value) {
    $length = [Math]::Sqrt($Value[0] * $Value[0] + $Value[1] * $Value[1] + $Value[2] * $Value[2])
    if ($length -lt 0.000001) { throw 'glTF normal cannot be zero.' }
    $x = Convert-SnormChannel ($Value[0] / $length)
    $y = Convert-SnormChannel ($Value[1] / $length)
    $z = Convert-SnormChannel (-$Value[2] / $length)
    return [uint32]([uint64]$x -bor ([uint64]$y -shl 8) -bor ([uint64]$z -shl 16) -bor ([uint64]127 -shl 24))
}

function Get-NodeTransform([int]$NodeIndex) {
    $node = $document.nodes[$NodeIndex]
    if ($null -ne $node.matrix) { throw 'Matrix-authored joint nodes are not supported by AESK v1.' }
    $translation = if ($null -ne $node.translation) { @($node.translation) } else { @(0.0, 0.0, 0.0) }
    $rotation = if ($null -ne $node.rotation) { @($node.rotation) } else { @(0.0, 0.0, 0.0, 1.0) }
    $nodeScale = if ($null -ne $node.scale) { @($node.scale) } else { @(1.0, 1.0, 1.0) }
    if ($translation.Count -ne 3 -or $rotation.Count -ne 4 -or $nodeScale.Count -ne 3) { throw 'Joint TRS component count is invalid.' }
    $qx = -[float]$rotation[0]; $qy = -[float]$rotation[1]; $qz = [float]$rotation[2]; $qw = [float]$rotation[3]
    $length = [Math]::Sqrt($qx * $qx + $qy * $qy + $qz * $qz + $qw * $qw)
    if (-not [double]::IsFinite($length) -or $length -lt 0.000001) { throw 'Joint quaternion is invalid.' }
    $result = [float[]]@(
        ([float]$translation[0] * $Scale), ([float]$translation[1] * $Scale), (-[float]$translation[2] * $Scale),
        ($qx / $length), ($qy / $length), ($qz / $length), ($qw / $length),
        [float]$nodeScale[0], [float]$nodeScale[1], [float]$nodeScale[2]
    )
    foreach ($value in $result) { if (-not [float]::IsFinite($value)) { throw 'Joint transform contains a non-finite value.' } }
    if ($result[7] -eq 0 -or $result[8] -eq 0 -or $result[9] -eq 0) { throw 'Joint scale cannot be zero.' }
    return $result
}

function Test-IdentityNode([object]$Node) {
    if ($null -ne $Node.matrix) {
        $identity = @(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
        for ($index = 0; $index -lt 16; ++$index) { if ([Math]::Abs([double]$Node.matrix[$index] - $identity[$index]) -gt 0.000001) { return $false } }
    }
    if ($null -ne $Node.translation) { for ($index = 0; $index -lt 3; ++$index) { if ([Math]::Abs([double]$Node.translation[$index]) -gt 0.000001) { return $false } } }
    if ($null -ne $Node.rotation) {
        $identityRotation = @(0,0,0,1)
        for ($index = 0; $index -lt 4; ++$index) { if ([Math]::Abs([double]$Node.rotation[$index] - $identityRotation[$index]) -gt 0.000001) { return $false } }
    }
    if ($null -ne $Node.scale) { for ($index = 0; $index -lt 3; ++$index) { if ([Math]::Abs([double]$Node.scale[$index] - 1) -gt 0.000001) { return $false } } }
    return $true
}

function Convert-InverseBindMatrix([float[]]$Value) {
    $rowMajor = [float[]]::new(16)
    for ($row = 0; $row -lt 4; ++$row) { for ($column = 0; $column -lt 4; ++$column) { $rowMajor[$row * 4 + $column] = $Value[$column * 4 + $row] } }
    $left = @($Scale, $Scale, -$Scale, 1.0)
    $right = @((1.0 / $Scale), (1.0 / $Scale), (-1.0 / $Scale), 1.0)
    $converted = [float[]]::new(16)
    for ($row = 0; $row -lt 4; ++$row) {
        for ($column = 0; $column -lt 4; ++$column) {
            $converted[$row * 4 + $column] = [float]($left[$row] * $rowMajor[$row * 4 + $column] * $right[$column])
            if (-not [float]::IsFinite($converted[$row * 4 + $column])) { throw 'Inverse bind matrix contains a non-finite value.' }
        }
    }
    return $converted
}

function Convert-Influences([float[]]$JointValues, [float[]]$WeightValues, [hashtable]$JointRemap, [int]$JointCount) {
    $pairs = [System.Collections.Generic.List[object]]::new(4)
    for ($slot = 0; $slot -lt 4; ++$slot) {
        $originalJoint = [int]$JointValues[$slot]
        if ($originalJoint -lt 0 -or $originalJoint -ge $JointCount -or [Math]::Abs($JointValues[$slot] - $originalJoint) -gt 0.0001) { throw 'Vertex joint index is invalid.' }
        $weight = [double]$WeightValues[$slot]
        if (-not [double]::IsFinite($weight) -or $weight -lt 0) { throw 'Vertex weight is invalid.' }
        $pairs.Add([pscustomobject]@{ Joint = [int]$JointRemap[$originalJoint]; Weight = $weight; Quantized = 0; Fraction = 0.0 })
    }
    $pairs = @($pairs | Sort-Object -Property @{ Expression = 'Weight'; Descending = $true }, @{ Expression = 'Joint'; Descending = $false })
    $total = ($pairs | Measure-Object -Property Weight -Sum).Sum
    if ($total -le 0) { throw 'Vertex weights must have a positive sum.' }
    $sum = 0
    foreach ($pair in $pairs) {
        $scaled = $pair.Weight / $total * 65535.0
        $pair.Quantized = [int][Math]::Floor($scaled)
        $pair.Fraction = $scaled - $pair.Quantized
        $sum += $pair.Quantized
    }
    $remaining = 65535 - $sum
    $fractionOrder = @($pairs | Sort-Object -Property @{ Expression = 'Fraction'; Descending = $true }, @{ Expression = 'Weight'; Descending = $true }, @{ Expression = 'Joint'; Descending = $false })
    for ($index = 0; $index -lt $remaining; ++$index) { ++$fractionOrder[$index].Quantized }
    $pairs = @($pairs | Sort-Object -Property @{ Expression = 'Quantized'; Descending = $true }, @{ Expression = 'Joint'; Descending = $false })
    for ($slot = 0; $slot -lt 4; ++$slot) { if ($pairs[$slot].Quantized -eq 0) { $pairs[$slot].Joint = 0 } }
    return $pairs
}

$primitive = @($document.meshes[0].primitives)
if ($primitive.Count -ne 1) { throw 'Exactly one mesh primitive is required.' }
$primitive = $primitive[0]
if ($null -ne $primitive.mode -and [int]$primitive.mode -ne 4) { throw 'Only triangle primitives are supported.' }
$requiredAttributes = @('POSITION', 'NORMAL', 'TEXCOORD_0', 'JOINTS_0', 'WEIGHTS_0')
foreach ($attribute in $requiredAttributes) { if ($null -eq $primitive.attributes.$attribute) { throw "Missing required glTF attribute: $attribute" } }
if ($null -eq $primitive.indices) { throw 'Indexed geometry is required.' }

$positions = Read-Accessor ([int]$primitive.attributes.POSITION)
$normals = Read-Accessor ([int]$primitive.attributes.NORMAL)
$textureCoordinates = Read-Accessor ([int]$primitive.attributes.TEXCOORD_0)
$jointValues = Read-Accessor ([int]$primitive.attributes.JOINTS_0)
$weightValues = Read-Accessor ([int]$primitive.attributes.WEIGHTS_0)
$indexValues = Read-Accessor ([int]$primitive.indices)
if ($positions.Components -ne 3 -or $normals.Components -ne 3 -or $textureCoordinates.Components -ne 2 -or $jointValues.Components -ne 4 -or $weightValues.Components -ne 4 -or $indexValues.Components -ne 1) { throw 'glTF accessor component counts are invalid.' }
if ($positions.Count -ne $normals.Count -or $positions.Count -ne $textureCoordinates.Count -or $positions.Count -ne $jointValues.Count -or $positions.Count -ne $weightValues.Count -or $indexValues.Count % 3) { throw 'glTF attribute counts are inconsistent.' }
if ($positions.Count -gt 100000 -or $indexValues.Count -gt 300000) { throw 'Skinned model exceeds the engine asset limits.' }

$meshNodes = @()
for ($nodeIndex = 0; $nodeIndex -lt @($document.nodes).Count; ++$nodeIndex) {
    $node = $document.nodes[$nodeIndex]
    if ($null -ne $node.mesh -and [int]$node.mesh -eq 0 -and $null -ne $node.skin -and [int]$node.skin -eq 0) { $meshNodes += $nodeIndex }
}
if ($meshNodes.Count -ne 1 -or -not (Test-IdentityNode $document.nodes[$meshNodes[0]])) { throw 'Exactly one identity-transform mesh node must reference the skin.' }

$skin = $document.skins[0]
$originalJointNodes = @($skin.joints | ForEach-Object { [int]$_ })
if ($originalJointNodes.Count -le 0 -or $originalJointNodes.Count -gt 128) { throw 'Skin joint count is invalid.' }
$jointNodeSet = @{}; $parentByNode = @{}
foreach ($nodeIndex in $originalJointNodes) {
    if ($nodeIndex -lt 0 -or $nodeIndex -ge @($document.nodes).Count -or $jointNodeSet.ContainsKey($nodeIndex)) { throw 'Skin joint node list is invalid.' }
    $jointNodeSet[$nodeIndex] = $true
}
for ($nodeIndex = 0; $nodeIndex -lt @($document.nodes).Count; ++$nodeIndex) {
    if ($null -eq $document.nodes[$nodeIndex].children) { continue }
    foreach ($child in @($document.nodes[$nodeIndex].children)) {
        $childIndex = [int]$child
        if ($parentByNode.ContainsKey($childIndex)) { throw 'A glTF node cannot have multiple parents.' }
        $parentByNode[$childIndex] = $nodeIndex
    }
}

function Get-JointParentNode([int]$NodeIndex) {
    $parent = if ($parentByNode.ContainsKey($NodeIndex)) { [int]$parentByNode[$NodeIndex] } else { -1 }
    while ($parent -ge 0 -and -not $jointNodeSet.ContainsKey($parent)) {
        if (-not (Test-IdentityNode $document.nodes[$parent])) { throw 'A transformed non-joint node exists inside the skin hierarchy.' }
        $parent = if ($parentByNode.ContainsKey($parent)) { [int]$parentByNode[$parent] } else { -1 }
    }
    return $parent
}

$orderedJointNodes = [System.Collections.Generic.List[int]]::new()
$addedJointNodes = @{}
while ($orderedJointNodes.Count -lt $originalJointNodes.Count) {
    $progress = $false
    foreach ($nodeIndex in $originalJointNodes) {
        if ($addedJointNodes.ContainsKey($nodeIndex)) { continue }
        $parent = Get-JointParentNode $nodeIndex
        if ($parent -lt 0 -or $addedJointNodes.ContainsKey($parent)) {
            $orderedJointNodes.Add($nodeIndex); $addedJointNodes[$nodeIndex] = $true; $progress = $true
        }
    }
    if (-not $progress) { throw 'Skin joint hierarchy contains a cycle.' }
}

$nodeToNewJoint = @{}; $originalOrdinalToNew = @{}
for ($newIndex = 0; $newIndex -lt $orderedJointNodes.Count; ++$newIndex) { $nodeToNewJoint[$orderedJointNodes[$newIndex]] = $newIndex }
for ($originalIndex = 0; $originalIndex -lt $originalJointNodes.Count; ++$originalIndex) { $originalOrdinalToNew[$originalIndex] = [int]$nodeToNewJoint[$originalJointNodes[$originalIndex]] }

$inverseBind = $null
if ($null -ne $skin.inverseBindMatrices) {
    $inverseBind = Read-Accessor ([int]$skin.inverseBindMatrices)
    if ($inverseBind.Count -ne $originalJointNodes.Count -or $inverseBind.Components -ne 16 -or $inverseBind.ComponentType -ne 5126) { throw 'Inverse bind matrix accessor is invalid.' }
}

$convertedInfluences = [System.Collections.Generic.List[object]]::new($positions.Count)
for ($vertex = 0; $vertex -lt $positions.Count; ++$vertex) { $convertedInfluences.Add((Convert-Influences $jointValues.Values[$vertex] $weightValues.Values[$vertex] $originalOrdinalToNew $originalJointNodes.Count)) }

$convertedInverseBind = [System.Collections.Generic.List[object]]::new($originalJointNodes.Count)
for ($newIndex = 0; $newIndex -lt $orderedJointNodes.Count; ++$newIndex) {
    $originalIndex = [Array]::IndexOf($originalJointNodes, $orderedJointNodes[$newIndex])
    if ($null -ne $inverseBind) { $convertedInverseBind.Add((Convert-InverseBindMatrix $inverseBind.Values[$originalIndex])) }
    else { $convertedInverseBind.Add([float[]]@(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)) }
}

$outputPath = if ([System.IO.Path]::IsPathRooted($Output)) {
    [System.IO.Path]::GetFullPath($Output)
} else {
    [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Output))
}
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($outputPath)) | Out-Null
$temporaryPath = "$outputPath.$([Guid]::NewGuid().ToString('N')).tmp"
$fileSize = [uint64](36L + [long]$positions.Count * 28L + [long]$indexValues.Count * 4L + [long]$orderedJointNodes.Count * 44L + [long]$positions.Count * 16L + [long]$orderedJointNodes.Count * 64L)
$stream = [System.IO.File]::Open($temporaryPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
$writer = [System.IO.BinaryWriter]::new($stream)
try {
    $writer.Write([uint32]0x4B534541); $writer.Write([uint32]1); $writer.Write([uint32]36); $writer.Write([uint32]0); $writer.Write($fileSize)
    $writer.Write([uint32]$positions.Count); $writer.Write([uint32]$indexValues.Count); $writer.Write([uint32]$orderedJointNodes.Count)
    for ($vertex = 0; $vertex -lt $positions.Count; ++$vertex) {
        $position = $positions.Values[$vertex]; $uv = $textureCoordinates.Values[$vertex]
        $writer.Write([float]($position[0] * $Scale)); $writer.Write([float]($position[1] * $Scale)); $writer.Write([float](-$position[2] * $Scale))
        $writer.Write((Convert-PackedNormal $normals.Values[$vertex])); $writer.Write([uint32]::MaxValue)
        $writer.Write([float]$uv[0]); $writer.Write([float]$uv[1])
    }
    for ($triangle = 0; $triangle -lt $indexValues.Count; $triangle += 3) {
        $a = [uint32]$indexValues.Values[$triangle][0]; $b = [uint32]$indexValues.Values[$triangle + 1][0]; $c = [uint32]$indexValues.Values[$triangle + 2][0]
        if ($a -ge $positions.Count -or $b -ge $positions.Count -or $c -ge $positions.Count) { throw 'glTF triangle index is out of range.' }
        $writer.Write($a); $writer.Write($c); $writer.Write($b)
    }
    foreach ($nodeIndex in $orderedJointNodes) {
        $parentNode = Get-JointParentNode $nodeIndex
        $parentJoint = if ($parentNode -ge 0) { [int]$nodeToNewJoint[$parentNode] } else { -1 }
        $transform = Get-NodeTransform $nodeIndex
        $writer.Write([int32]$parentJoint); foreach ($value in $transform) { $writer.Write([float]$value) }
    }
    foreach ($influences in $convertedInfluences) {
        foreach ($pair in $influences) { $writer.Write([uint16]$pair.Joint) }
        foreach ($pair in $influences) { $writer.Write([uint16]$pair.Quantized) }
    }
    foreach ($matrix in $convertedInverseBind) { foreach ($value in $matrix) { $writer.Write([float]$value) } }
} finally {
    $writer.Dispose()
}
if ([uint64](Get-Item -LiteralPath $temporaryPath).Length -ne $fileSize) { Remove-Item -LiteralPath $temporaryPath -Force; throw 'AESK output size is inconsistent.' }
[System.IO.File]::Move($temporaryPath, $outputPath, $true)
Write-Output "vertices=$($positions.Count) triangles=$($indexValues.Count / 3) joints=$($orderedJointNodes.Count) bytes=$fileSize"
