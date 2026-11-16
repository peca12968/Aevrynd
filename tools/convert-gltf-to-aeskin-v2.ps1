param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Output,
    [float]$Scale = 1,
    [int]$MaximumVertices = 1000000,
    [int]$MaximumIndices = 3000000,
    [int]$MaximumJoints = 256,
    [int]$MaximumPrimitives = 1024,
    [int]$MaximumAnimations = 128,
    [string[]]$PreserveConstantOverridesFor = @()
)

$ErrorActionPreference = 'Stop'
if (-not [float]::IsFinite($Scale) -or $Scale -le 0) { throw 'Scale must be finite and positive.' }
$sourcePath = (Resolve-Path -LiteralPath $Source).Path
$document = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json
if (@($document.buffers).Count -ne 1 -or @($document.meshes).Count -ne 1 -or @($document.skins).Count -ne 1) { throw 'Exactly one buffer, mesh, and skin are required.' }

$bufferUri = [string]$document.buffers[0].uri
if ($bufferUri.StartsWith('data:')) {
    $comma = $bufferUri.IndexOf(',')
    if ($comma -lt 0 -or -not $bufferUri.Substring(0, $comma).EndsWith(';base64')) { throw 'Invalid embedded buffer.' }
    $binary = [Convert]::FromBase64String($bufferUri.Substring($comma + 1))
} else {
    $binary = [IO.File]::ReadAllBytes((Join-Path (Split-Path -Parent $sourcePath) $bufferUri))
}
if ([long]$document.buffers[0].byteLength -ne $binary.LongLength) { throw 'Buffer length is inconsistent.' }

function Read-Accessor([int]$Index) {
    if ($Index -lt 0 -or $Index -ge @($document.accessors).Count) { throw 'Accessor index is invalid.' }
    $accessor = $document.accessors[$Index]
    if ($null -ne $accessor.sparse) { throw 'Sparse accessors are not supported.' }
    $viewIndex = [int]$accessor.bufferView
    if ($viewIndex -lt 0 -or $viewIndex -ge @($document.bufferViews).Count) { throw 'Buffer view index is invalid.' }
    $view = $document.bufferViews[$viewIndex]
    $components = @{ SCALAR=1; VEC2=2; VEC3=3; VEC4=4; MAT4=16 }[[string]$accessor.type]
    $bytes = @{ 5121=1; 5123=2; 5125=4; 5126=4 }[[int]$accessor.componentType]
    if (-not $components -or -not $bytes) { throw 'Accessor format is unsupported.' }
    $packed = $components * $bytes
    $stride = if ($null -ne $view.byteStride) { [int]$view.byteStride } else { $packed }
    $viewOffset = if ($null -ne $view.byteOffset) { [long]$view.byteOffset } else { 0L }
    $accessorOffset = if ($null -ne $accessor.byteOffset) { [long]$accessor.byteOffset } else { 0L }
    $offset = $viewOffset + $accessorOffset
    $count = [int]$accessor.count
    if ($count -lt 1 -or $stride -lt $packed -or $offset -lt 0) { throw 'Accessor range is invalid.' }
    $end = $offset + [long]($count - 1) * $stride + $packed
    if ($end -gt $binary.LongLength -or $end -gt $viewOffset + [long]$view.byteLength) { throw 'Accessor exceeds its buffer view.' }
    $stream = [IO.MemoryStream]::new($binary, $false); $reader = [IO.BinaryReader]::new($stream)
    $values = [Collections.Generic.List[object]]::new($count)
    try {
        for ($item = 0; $item -lt $count; ++$item) {
            $stream.Position = $offset + [long]$item * $stride
            $value = [double[]]::new($components)
            for ($component = 0; $component -lt $components; ++$component) {
                $number = switch ([int]$accessor.componentType) { 5121 {$reader.ReadByte()} 5123 {$reader.ReadUInt16()} 5125 {$reader.ReadUInt32()} 5126 {$reader.ReadSingle()} }
                if ($accessor.normalized -eq $true) { if ($accessor.componentType -eq 5121) {$number/=255.0} elseif ($accessor.componentType -eq 5123) {$number/=65535.0} }
                if (-not [double]::IsFinite([double]$number)) { throw 'Accessor contains a non-finite value.' }
                $value[$component] = [double]$number
            }
            $values.Add($value)
        }
    } finally { $reader.Dispose() }
    return [pscustomobject]@{Count=$count;Components=$components;ComponentType=[int]$accessor.componentType;Values=$values}
}

function New-Chunk([string]$Type, [scriptblock]$Body) {
    $stream = [IO.MemoryStream]::new(); $writer = [IO.BinaryWriter]::new($stream)
    try { & $Body $writer; $writer.Flush(); return [pscustomobject]@{Type=$Type;Bytes=$stream.ToArray();Count=0} }
    finally { $writer.Dispose() }
}
function Write-String([IO.BinaryWriter]$Writer,[string]$Value) {
    $bytes = [Text.Encoding]::UTF8.GetBytes($(if ($null -eq $Value) {''} else {$Value}))
    if ($bytes.Length -gt 65535) { throw 'UTF-8 string is too long.' }
    $Writer.Write([uint16]$bytes.Length); $Writer.Write($bytes)
}
function Write-Trs([IO.BinaryWriter]$Writer,[object]$Node) {
    if ($null -ne $Node.matrix) { throw 'Matrix-authored joints are not supported.' }
    $t=if($null-ne$Node.translation){@($Node.translation)}else{@(0,0,0)}; $r=if($null-ne$Node.rotation){@($Node.rotation)}else{@(0,0,0,1)}; $s=if($null-ne$Node.scale){@($Node.scale)}else{@(1,1,1)}
    if($t.Count-ne3-or$r.Count-ne4-or$s.Count-ne3){throw 'Invalid joint TRS.'}
    @(([float]$t[0]*$Scale),([float]$t[1]*$Scale),(-[float]$t[2]*$Scale),(-[float]$r[0]),(-[float]$r[1]),[float]$r[2],[float]$r[3],[float]$s[0],[float]$s[1],[float]$s[2])|ForEach-Object{$Writer.Write([float]$_)}
}

$skin=$document.skins[0]; $jointNodes=@($skin.joints|ForEach-Object{[int]$_})
if($jointNodes.Count-lt1-or$jointNodes.Count-gt$MaximumJoints){throw 'Joint limit exceeded.'}
$nodeToJoint=@{}; for($i=0;$i-lt$jointNodes.Count;$i++){if($nodeToJoint.ContainsKey($jointNodes[$i])){throw 'Duplicate joint node.'};$nodeToJoint[$jointNodes[$i]]=$i}
$parents=@{};for($i=0;$i-lt@($document.nodes).Count;$i++){if($null-eq$document.nodes[$i].children){continue};foreach($child in @($document.nodes[$i].children)){if($parents.ContainsKey([int]$child)){throw 'Node has multiple parents.'};$parents[[int]$child]=$i}}

$primitives=@($document.meshes[0].primitives)
if($primitives.Count-lt1-or$primitives.Count-gt$MaximumPrimitives){throw 'Primitive limit exceeded.'}
$decoded=[Collections.Generic.List[object]]::new();$totalVertices=0L;$totalIndices=0L
foreach($primitive in $primitives){
    if($null-ne$primitive.mode-and[int]$primitive.mode-ne4){throw 'Only triangle primitives are supported.'}
    foreach($name in @('POSITION','NORMAL','TEXCOORD_0','JOINTS_0','WEIGHTS_0')){if($null-eq$primitive.attributes.$name){throw "Missing attribute $name."}}
    if($null-eq$primitive.indices){throw 'Indexed geometry is required.'}
    $p=Read-Accessor $primitive.attributes.POSITION;$n=Read-Accessor $primitive.attributes.NORMAL;$uv=Read-Accessor $primitive.attributes.TEXCOORD_0;$j=Read-Accessor $primitive.attributes.JOINTS_0;$w=Read-Accessor $primitive.attributes.WEIGHTS_0;$ix=Read-Accessor $primitive.indices
    $t=if($null-ne$primitive.attributes.TANGENT){Read-Accessor $primitive.attributes.TANGENT}else{$null}
    $c=if($null-ne$primitive.attributes.COLOR_0){Read-Accessor $primitive.attributes.COLOR_0}else{$null}
    if($p.Components-ne3-or$n.Components-ne3-or$uv.Components-ne2-or$j.Components-ne4-or$w.Components-ne4-or$ix.Components-ne1-or$p.Count-ne$n.Count-or$p.Count-ne$uv.Count-or$p.Count-ne$j.Count-or$p.Count-ne$w.Count-or($null-ne$c-and$c.Count-ne$p.Count)-or($null-ne$t-and($t.Components-ne4-or$t.Count-ne$p.Count))-or$ix.Count%3){throw 'Primitive accessor shapes are inconsistent.'}
    $totalVertices+=$p.Count;$totalIndices+=$ix.Count;if($totalVertices-gt$MaximumVertices-or$totalIndices-gt$MaximumIndices){throw 'Aggregate mesh limit exceeded.'}
    $materialIndex=if($null-ne$primitive.material){[int]$primitive.material}else{-1};if($materialIndex-ge@($document.materials).Count){throw 'Material index is invalid.'}
    $decoded.Add([pscustomobject]@{P=$p;N=$n;Uv=$uv;J=$j;W=$w;C=$c;T=$t;I=$ix;Material=$materialIndex})
}

function Pack-Normal([double[]]$value){$length=[Math]::Sqrt($value[0]*$value[0]+$value[1]*$value[1]+$value[2]*$value[2]);if($length-lt1e-8){throw 'Normal is zero.'};$bytes=@();foreach($v in @(($value[0]/$length),($value[1]/$length),(-$value[2]/$length))){$n=[int][Math]::Round([Math]::Max(-1.0,[Math]::Min(1.0,[double]$v))*127);$bytes+=[byte]($n-band255)};return [uint32]([uint64]$bytes[0]-bor([uint64]$bytes[1]-shl8)-bor([uint64]$bytes[2]-shl16)-bor([uint64]127-shl24))}
function Pack-Tangent([double[]]$value){$length=[Math]::Sqrt($value[0]*$value[0]+$value[1]*$value[1]+$value[2]*$value[2]);if($length-lt1e-8-or[Math]::Abs([Math]::Abs($value[3])-1)-gt0.001){throw 'Tangent is invalid.'};$bytes=@();foreach($v in @(($value[0]/$length),($value[1]/$length),(-$value[2]/$length),(-$value[3]))){$n=[int][Math]::Round([Math]::Max(-1.0,[Math]::Min(1.0,[double]$v))*127);$bytes+=[byte]($n-band255)};return [uint32]([uint64]$bytes[0]-bor([uint64]$bytes[1]-shl8)-bor([uint64]$bytes[2]-shl16)-bor([uint64]$bytes[3]-shl24))}
function Pack-Color([double[]]$value){
    [double[]]$channels=@(1,1,1,1)
    if($null-ne$value){for($i=0;$i-lt$value.Length;$i++){$channels[$i]=$value[$i]}}
    [uint64]$result=0
    for($i=0;$i-lt4;$i++){
        [byte]$quantized=[Math]::Round([Math]::Max(0.0,[Math]::Min(1.0,$channels[$i]))*255.0)
        $result=$result-bor(([uint64]$quantized)-shl(8*$i))
    }
    return [uint32]$result
}
function Add-String([Collections.Generic.List[byte]]$table,[string]$value){if([string]::IsNullOrWhiteSpace($value)-or[Text.Encoding]::UTF8.GetByteCount($value)-gt255){throw 'Asset string is invalid.'};$bytes=[Text.Encoding]::UTF8.GetBytes($value);$result=@($table.Count,$bytes.Length);foreach($byte in $bytes){$table.Add($byte)};return,$result}
function Chunk([string]$type,[uint32]$count,[uint32]$stride,[scriptblock]$body){$chunk=New-Chunk $type $body;$chunk.Count=$count;$chunk|Add-Member Stride $stride;return,$chunk}

$strings=[Collections.Generic.List[byte]]::new();$jointRefs=@();$jointNames=@();foreach($nodeIndex in $jointNodes){$name=[string]$document.nodes[$nodeIndex].name;if($jointNames-contains$name){throw 'Joint names must be unique.'};$jointNames+=$name;$jointRefs+=,(Add-String $strings $name)}
$materials=@($document.materials);$materialNames=@();$materialRefs=@();for($i=0;$i-lt$materials.Count;$i++){$name=[string]$materials[$i].name;if([string]::IsNullOrWhiteSpace($name)){$name="Material_$i"};$materialNames+=$name;$materialRefs+=,(Add-String $strings $name)}
$vert=Chunk VERT $totalVertices 28 {param($w)foreach($part in $decoded){for($v=0;$v-lt$part.P.Count;$v++){$w.Write([float]($part.P.Values[$v][0]*$Scale));$w.Write([float]($part.P.Values[$v][1]*$Scale));$w.Write([float](-$part.P.Values[$v][2]*$Scale));$w.Write((Pack-Normal $part.N.Values[$v]));$vertexColor=if($null-ne$part.C){[double[]]$part.C.Values[$v]}else{[double[]]@(1,1,1,1)};$w.Write((Pack-Color $vertexColor));$w.Write([float]$part.Uv.Values[$v][0]);$w.Write([float]$part.Uv.Values[$v][1])}}}
$indx=Chunk INDX $totalIndices 4 {param($w)$base=0;foreach($part in $decoded){
    $triangles=[Collections.Generic.List[object]]::new()
    for($t=0;$t-lt$part.I.Count;$t+=3){
        [uint32]$a=$part.I.Values[$t][0];[uint32]$b=$part.I.Values[$t+1][0];[uint32]$c=$part.I.Values[$t+2][0]
        if($a-ge$part.P.Count-or$b-ge$part.P.Count-or$c-ge$part.P.Count){throw 'Index out of range.'}
        # 先轉為引擎左手座標繞向，再循環旋轉到字典序最小表示；旋轉不會翻轉三角形。
        [uint32[]]$oriented=@($a,$c,$b);$start=0
        for($candidate=1;$candidate-lt3;$candidate++){
            $better=$false
            for($component=0;$component-lt3;$component++){
                $left=$oriented[($candidate+$component)%3];$right=$oriented[($start+$component)%3]
                if($left-lt$right){$better=$true;break};if($left-gt$right){break}
            }
            if($better){$start=$candidate}
        }
        $triangles.Add([pscustomobject]@{A=$oriented[$start];B=$oriented[($start+1)%3];C=$oriented[($start+2)%3]})
    }
    foreach($triangle in @($triangles|Sort-Object A,B,C)){$w.Write([uint32]($base+$triangle.A));$w.Write([uint32]($base+$triangle.B));$w.Write([uint32]($base+$triangle.C))}
    $base+=$part.P.Count
}}
$join=Chunk JOIN $jointNodes.Count 44 {param($w)for($i=0;$i-lt$jointNodes.Count;$i++){$nodeIndex=$jointNodes[$i];$parentNode=if($parents.ContainsKey($nodeIndex)){[int]$parents[$nodeIndex]}else{-1};while($parentNode-ge0 -and -not $nodeToJoint.ContainsKey($parentNode)){$parentNode=if($parents.ContainsKey($parentNode)){[int]$parents[$parentNode]}else{-1}};$parent=if($parentNode-ge0){[int]$nodeToJoint[$parentNode]}else{-1};if($parent-ge$i){throw 'Joints must be parent-first.'};$w.Write([int32]$parent);Write-Trs $w $document.nodes[$nodeIndex]}}
$wght=Chunk WGHT $totalVertices 16 {param($w)foreach($part in $decoded){for($v=0;$v-lt$part.P.Count;$v++){for($k=0;$k-lt4;$k++){$j=[int]$part.J.Values[$v][$k];if($j-lt0-or$j-ge$jointNodes.Count){throw 'Joint influence invalid.'};$w.Write([uint16]$j)};$sum=($part.W.Values[$v]|Measure-Object -Sum).Sum;if($sum-le0){throw 'Weight sum is zero.'};$q=@(0,0,0,0);$largest=0;for($k=0;$k-lt4;$k++){if($part.W.Values[$v][$k]-gt$part.W.Values[$v][$largest]){$largest=$k};$q[$k]=[int][Math]::Floor($part.W.Values[$v][$k]/$sum*65535)};$q[$largest]+=65535-($q|Measure-Object -Sum).Sum;for($k=0;$k-lt4;$k++){$w.Write([uint16]$q[$k])}}}}
$allTangents=$true;foreach($part in $decoded){if($null-eq$part.T){$allTangents=$false}};if(-not$allTangents-and@($decoded|Where-Object{$null-ne$_.T}).Count){throw 'TANGENT must be present on every primitive or none.'}
$tang=if($allTangents){$chunkValue=Chunk TANG $totalVertices 4 {param($w)foreach($part in $decoded){for($v=0;$v-lt$part.T.Count;$v++){$w.Write((Pack-Tangent $part.T.Values[$v]))}}};$chunkValue|Add-Member Required $false;$chunkValue}else{$null}
$inverse=Read-Accessor ([int]$skin.inverseBindMatrices);if($inverse.Count-ne$jointNodes.Count-or$inverse.Components-ne16){throw 'Inverse bind matrices are invalid.'}
$inverseScale=1.0/[double]$Scale
$ibnd=Chunk IBND $jointNodes.Count 64 {param($w)foreach($m in $inverse.Values){for($row=0;$row-lt4;$row++){for($column=0;$column-lt4;$column++){$left=@($Scale,$Scale,-$Scale,1)[$row];$right=@($inverseScale,$inverseScale,-$inverseScale,1)[$column];$w.Write([float]($left*$m[$column*4+$row]*$right))}}}}
$jnam=Chunk JNAM $jointNodes.Count 8 {param($w)foreach($ref in $jointRefs){$w.Write([uint32]$ref[0]);$w.Write([uint32]$ref[1])}}
$subm=Chunk SUBM $decoded.Count 24 {param($w)$first=0;foreach($part in $decoded){$slot=if($part.Material-ge0){$part.Material}else{0};if($slot-ge$materialRefs.Count){throw 'Material slot invalid.'};$ref=$materialRefs[$slot];$w.Write([uint32]$first);$w.Write([uint32]$part.I.Count);$w.Write([uint32]$slot);$w.Write([uint32]$ref[0]);$w.Write([uint32]$ref[1]);$w.Write([uint32]0);$first+=$part.I.Count}}
$materialRecords=@();for($slot=0;$slot-lt$materials.Count;$slot++){$material=$materials[$slot];$pbr=$material.pbrMetallicRoughness;$base=if($null-ne$pbr.baseColorFactor){@($pbr.baseColorFactor)}else{@(1,1,1,1)};$emissive=if($null-ne$material.emissiveFactor){@($material.emissiveFactor)}else{@(0,0,0)};if($base.Count-ne4-or$emissive.Count-ne3){throw 'glTF material vector size is invalid.'};$roughness=if($null-ne$pbr.roughnessFactor){[double]$pbr.roughnessFactor}else{1.0};$metallic=if($null-ne$pbr.metallicFactor){[double]$pbr.metallicFactor}else{1.0};$alphaCutoff=if($null-ne$material.alphaCutoff){[double]$material.alphaCutoff}else{0.5};$normalScale=if($null-ne$material.normalTexture.scale){[double]$material.normalTexture.scale}else{1.0};$occlusion=if($null-ne$material.occlusionTexture.strength){[double]$material.occlusionTexture.strength}else{1.0};$emissiveStrength=if($null-ne$material.extensions.KHR_materials_emissive_strength.emissiveStrength){[double]$material.extensions.KHR_materials_emissive_strength.emissiveStrength}else{1.0};$alphaMode=if($null-ne$material.alphaMode){[string]$material.alphaMode}else{'OPAQUE'};$flags=if($alphaMode-eq'MASK'){1}elseif($alphaMode-eq'BLEND'){2}elseif($alphaMode-eq'OPAQUE'){0}else{throw 'glTF alpha mode is invalid.'};if($material.doubleSided-eq$true){$flags=$flags-bor4};if($null-ne$material.extensions.KHR_materials_unlit){$flags=$flags-bor8};$values=@($base)+@($emissive)+@($emissiveStrength,$roughness,$metallic,$alphaCutoff,$normalScale,$occlusion,0.0);foreach($value in $values){if(-not[double]::IsFinite([double]$value)){throw 'glTF material contains a non-finite value.'}};foreach($value in $base){if($value-lt0-or$value-gt1){throw 'glTF base color factor is outside range.'}};foreach($value in $emissive){if($value-lt0-or$value-gt1){throw 'glTF emissive factor is outside range.'}};if($roughness-lt0-or$roughness-gt1-or$metallic-lt0-or$metallic-gt1-or$alphaCutoff-lt0-or$alphaCutoff-gt1-or$normalScale-lt-16-or$normalScale-gt16-or$occlusion-lt0-or$occlusion-gt1-or$emissiveStrength-lt0-or$emissiveStrength-gt64){throw 'glTF material scalar is outside range.'};$record=@($slot,$flags)+$values;$materialRecords+=,$record}
$matl=Chunk MATL $materialRecords.Count 64 {param($w)foreach($record in $materialRecords){$w.Write([uint32]$record[0]);$w.Write([uint32]$record[1]);for($index=2;$index-lt$record.Count;$index++){$w.Write([float]$record[$index])}}};$matl|Add-Member Required $false

$clipRecords=@();$trackRecords=@();$keyRecords=@();$animations=@($document.animations|Where-Object{$null-ne$_});if($animations.Count-gt$MaximumAnimations){throw 'Animation limit exceeded.'}
foreach ($animation in $animations) {
    $tracks = @{}
    $preserveConstants = [string]$animation.name -cin $PreserveConstantOverridesFor
    foreach ($channel in @($animation.channels)) {
        $sampler = $animation.samplers[[int]$channel.sampler]
        $path = [string]$channel.target.path
        if ($path -notin @('translation','rotation','scale')) { throw 'Animation path unsupported.' }
        $node = [int]$channel.target.node
        if (-not $nodeToJoint.ContainsKey($node)) { throw 'Animation target is not a joint.' }
        $times = Read-Accessor ([int]$sampler.input)
        $values = Read-Accessor ([int]$sampler.output)
        $expected = if ($path -eq 'rotation') { 4 } else { 3 }
        if ($times.Components -ne 1 -or $values.Components -ne $expected -or $times.Count -ne $values.Count) {
            throw 'Animation sampler shape invalid.'
        }
        $constant = $true
        for ($key = 1; $key -lt $values.Count; ++$key) {
            for ($component = 0; $component -lt $expected; ++$component) {
                if ([Math]::Abs($values.Values[$key][$component] - $values.Values[0][$component]) -gt 1e-7) {
                    $constant = $false
                }
            }
        }
        if ($constant) {
            if (-not $preserveConstants) { continue }
            $rest = $document.nodes[$node].$path
            if ($null -eq $rest) {
                $rest = switch ($path) {
                    'translation' { @(0,0,0) }
                    'rotation' { @(0,0,0,1) }
                    'scale' { @(1,1,1) }
                }
            }
            $overridesRest = $false
            for ($component = 0; $component -lt $expected; ++$component) {
                if ([Math]::Abs($values.Values[0][$component] - $rest[$component]) -gt 1e-7) {
                    $overridesRest = $true
                }
            }
            if (-not $overridesRest) { continue }
        } elseif ($null -ne $sampler.interpolation -and $sampler.interpolation -ne 'LINEAR') {
            throw "Non-constant $($sampler.interpolation) animation is unsupported."
        }
        $joint = [int]$nodeToJoint[$node]
        if (-not $tracks.ContainsKey($joint)) { $tracks[$joint] = @{} }
        $tracks[$joint][$path] = @($times,$values)
    }
    $firstTrack=$trackRecords.Count;$duration=0.0;foreach($jointValue in @($tracks.Keys|Sort-Object)){$joint=[int]$jointValue;$paths=$tracks[$joint];$firstPath=[string]@($paths.Keys)[0];$timeline=$paths[$firstPath][0];foreach($pair in $paths.Values){if($pair[0].Count-ne$timeline.Count){throw 'Animated TRS timelines differ.'};for($k=0;$k-lt$timeline.Count;$k++){if([Math]::Abs($pair[0].Values[$k][0]-$timeline.Values[$k][0])-gt1e-7){throw 'Animated TRS timelines differ.'}}};$firstKey=$keyRecords.Count;$node=$document.nodes[$jointNodes[$joint]];$bt=if($null-ne$node.translation){@($node.translation)}else{@(0,0,0)};$br=if($null-ne$node.rotation){@($node.rotation)}else{@(0,0,0,1)};$bs=if($null-ne$node.scale){@($node.scale)}else{@(1,1,1)};for($k=0;$k-lt$timeline.Count;$k++){$t=if($paths.translation){$paths.translation[1].Values[$k]}else{$bt};$r=if($paths.rotation){$paths.rotation[1].Values[$k]}else{$br};$s=if($paths.scale){$paths.scale[1].Values[$k]}else{$bs};$time=[float]$timeline.Values[$k][0];$duration=[Math]::Max($duration,$time);$keyRecords+=,@($time,([double]$t[0]*$Scale),([double]$t[1]*$Scale),(-[double]$t[2]*$Scale),(-[double]$r[0]),(-[double]$r[1]),[double]$r[2],[double]$r[3],[double]$s[0],[double]$s[1],[double]$s[2])};$trackRecords+=,@([uint32]$joint,0,[uint32]$firstKey,[uint32]$timeline.Count,7,0)};$name=[string]$animation.name;if([string]::IsNullOrWhiteSpace($name)){$name="Animation_$($clipRecords.Count)"};$ref=Add-String $strings $name;$clipRecords+=,@([uint32]$ref[0],[uint32]$ref[1],1,[double]$duration,[uint32]$firstTrack,[uint32]($trackRecords.Count-$firstTrack),0,0)}
$clip=Chunk CLIP $clipRecords.Count 32 {param($w)foreach($r in $clipRecords){$w.Write([uint32]$r[0]);$w.Write([uint32]$r[1]);$w.Write([uint32]$r[2]);$w.Write([float]$r[3]);for($i=4;$i-lt8;$i++){$w.Write([uint32]$r[$i])}}}
$trak=Chunk TRAK $trackRecords.Count 24 {param($w)foreach($r in $trackRecords){foreach($v in $r){$w.Write([uint32]$v)}}}
$keyf=Chunk KEYF $keyRecords.Count 44 {param($w)foreach($r in $keyRecords){foreach($v in $r){$w.Write([float]$v)}}}
$strs=Chunk STRS $strings.Count 1 {param($w)$w.Write($strings.ToArray())}
$chunks=@($vert,$indx,$join,$wght)+@($(if($null-ne$tang){$tang}))+@($ibnd,$jnam,$subm,$matl,$clip,$trak,$keyf,$strs);$offset=64+$chunks.Count*32;$offset=($offset+7)-band(-bnot7);$entries=@();foreach($chunk in $chunks){$offset=($offset+7)-band(-bnot7);$entries+=,[pscustomobject]@{Chunk=$chunk;Offset=$offset};$offset+=$chunk.Bytes.Length};$fileSize=$offset
$outputPath=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($Output)){$Output}else{Join-Path (Get-Location) $Output}));[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath))|Out-Null;$temporary="$outputPath.$([Guid]::NewGuid().ToString('N')).tmp";$stream=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$w=[IO.BinaryWriter]::new($stream)
try{$w.Write([uint32]0x4B534541);$w.Write([uint32]2);$w.Write([uint32]64);$w.Write([uint32]0);$w.Write([uint64]$fileSize);$w.Write([uint64]64);$w.Write([uint32]$chunks.Count);$w.Write([uint32]$totalVertices);$w.Write([uint32]$totalIndices);$w.Write([uint32]$jointNodes.Count);$w.Write([uint32]$decoded.Count);$w.Write([uint32]$clipRecords.Count);$w.Write([uint32]$strings.Count);$w.Write([uint32]0);foreach($e in $entries){$w.Write([Text.Encoding]::ASCII.GetBytes($e.Chunk.Type));$w.Write([uint16]1);$w.Write([uint16]$(if($e.Chunk.Required-eq$false){0}else{1}));$w.Write([uint64]$e.Offset);$w.Write([uint64]$e.Chunk.Bytes.Length);$w.Write([uint32]$e.Chunk.Count);$w.Write([uint32]$e.Chunk.Stride)};foreach($e in $entries){while($w.BaseStream.Position-lt$e.Offset){$w.Write([byte]0)};$w.Write($e.Chunk.Bytes)}}finally{$w.Dispose()};if((Get-Item $temporary).Length-ne$fileSize){Remove-Item $temporary -Force;throw 'Output size mismatch.'};[IO.File]::Move($temporary,$outputPath,$true);Write-Output "AESK_V2 primitives=$($decoded.Count) materials=$($materialRecords.Count) vertices=$totalVertices indices=$totalIndices joints=$($jointNodes.Count) clips=$($clipRecords.Count) tracks=$($trackRecords.Count) bytes=$fileSize"
