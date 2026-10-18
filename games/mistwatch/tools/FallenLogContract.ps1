#requires -Version 7.0
# 正式來源相對的獨立 CPU 契約；原生結果不可回寫預期值。
function Assert-MistwatchFallenPhysicalPath([string]$Path,[switch]$Directory) {
    $full=[IO.Path]::GetFullPath($Path)
    $item=Get-Item -LiteralPath $full -Force
    if($Directory -and $item -isnot [System.IO.DirectoryInfo]){throw ('Expected directory: '+$full)}
    if(-not $Directory -and $item -is [System.IO.DirectoryInfo]){throw ('Expected regular file: '+$full)}
    while($null -ne $item){
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw ('Reparse path rejected: '+$item.FullName)}
        $parent=if($item -is [System.IO.DirectoryInfo]){$item.Parent}else{$item.Directory}
        $item=$parent
    }
    return $full
}
function Assert-MistwatchFallenExactKeys($Value,[string[]]$Keys,[string]$Label) {
    if($Value -isnot [Collections.IDictionary]){throw ('Exact object inventory: '+$Label)}
    $inventory=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    # count/Keys/Values 可是資料欄位；列舉真實項目，避免 PowerShell 成員存取遮蔽。
    foreach($entry in $Value.GetEnumerator()){
        $actual=$entry.Key
        if($actual -isnot [string] -or -not $inventory.Add($actual)){throw ('Invalid or duplicate exact key: '+$Label)}
    }
    if($inventory.Count -ne $Keys.Count){throw ('Exact object inventory: '+$Label)}
    foreach($key in $Keys){if(-not $inventory.Contains($key)){throw ('Missing exact object key: '+$Label+' '+$key)}}
}
function Assert-MistwatchFallenJsonNode([System.Text.Json.JsonElement]$Node,[string]$Label) {
    if($Node.ValueKind -eq [System.Text.Json.JsonValueKind]::Object){
        $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach($property in $Node.EnumerateObject()){
            if(-not $seen.Add($property.Name)){throw ('Duplicate or shadow JSON key: '+$Label+' '+$property.Name)}
            Assert-MistwatchFallenJsonNode $property.Value ($Label+'.'+$property.Name)
        }
    }elseif($Node.ValueKind -eq [System.Text.Json.JsonValueKind]::Array){
        foreach($child in $Node.EnumerateArray()){Assert-MistwatchFallenJsonNode $child $Label}
    }
}
function Read-MistwatchFallenStrictJson([string]$Path) {
    $full=Assert-MistwatchFallenPhysicalPath $Path
    $raw=[IO.File]::ReadAllText($full,[Text.Encoding]::UTF8)
    $document=[System.Text.Json.JsonDocument]::Parse($raw)
    try{Assert-MistwatchFallenJsonNode $document.RootElement $full}finally{$document.Dispose()}
    return ,($raw|ConvertFrom-Json -AsHashtable)
}
function Assert-MistwatchFallenSha([string]$Sha,[string]$Label) {
    if($Sha -cnotmatch '^[0-9A-F]{64}$'){throw ('Exact uppercase sealed SHA required: '+$Label)}
}
function Assert-MistwatchFallenHash([string]$Path,[string]$Sha) {
    Assert-MistwatchFallenSha $Sha $Path
    $full=Assert-MistwatchFallenPhysicalPath $Path
    if((Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash -cne $Sha){throw ('Sealed raw hash mismatch: '+$full)}
}
function Assert-MistwatchFallenRow($Row,[string]$FixedPath='') {
    Assert-MistwatchFallenExactKeys $Row @('path','bytes','sha256') 'raw file row'
    if($Row.path -isnot [string] -or $Row.sha256 -isnot [string] -or
       $Row.sha256 -cnotmatch '^[0-9A-Fa-f]{64}$' -or ($Row.bytes -isnot [long] -and $Row.bytes -isnot [int]) -or
       [double]$Row.bytes -ne [long]$Row.bytes -or [long]$Row.bytes -lt 0){throw 'Malformed sealed file row'}
    $full=Assert-MistwatchFallenPhysicalPath $Row.path
    if($FixedPath -and $full -ine [IO.Path]::GetFullPath($FixedPath)){throw ('Unexpected sealed file path: '+$full)}
    $item=Get-Item -LiteralPath $full
    if($item.Length -ne [long]$Row.bytes -or
       (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash -cne $Row.sha256.ToUpperInvariant()){
        throw ('Actual raw file differs: '+$full)
    }
}
function Assert-MistwatchFallenRows($Rows) {
    if($Rows -isnot [array] -or $Rows.Count -lt 1){throw 'Sealed input rows missing'}
    $unique=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $Rows){
        Assert-MistwatchFallenExactKeys $row @('path','bytes','sha256') 'input row'
        $path=[IO.Path]::GetFullPath([string]$row.path)
        if($unique.ContainsKey($path)){
            if($unique[$path].bytes -ne $row.bytes -or
               [string]$unique[$path].sha256 -ine [string]$row.sha256){throw ('Conflicting duplicate input identity: '+$path)}
        }else{$unique.Add($path,$row)}
    }
    foreach($row in $unique.Values){Assert-MistwatchFallenRow $row}
}

function Assert-MistwatchIndependentExpectedContract($value) {
    if($value.native_contract -isnot [Collections.IDictionary]){throw 'Independent native contract missing'}
    foreach($entry in $value.native_contract.GetEnumerator()){
        if($entry.Key -cnotmatch '^[a-z][a-z0-9_]*$' -or $entry.Value -isnot [string]){throw 'Expected contract requires canonical string fields'}
    }
    $nativeKeys=@(
        'world_profile',
        'forest_cover_mode',
        'ground_cover_instances',
        'forest_cover_seed',
        'forest_cover_authored_population',
        'forest_cover_maximum_patches',
        'forest_cover_patch_minimum_plants',
        'forest_cover_patch_maximum_plants',
        'forest_cover_independent_anchors',
        'forest_cover_patches',
        'forest_cover_reached_budget',
        'forest_cover_fingerprint',
        'forest_cover_clearance_colliders',
        'forest_cover_clearance_collider_fingerprint',
        'forest_cover_variants',
        'fern_colony_population',
        'fern_colony_authored_population',
        'fern_colonies',
        'fern_colony_seed',
        'fern_colony_reached_budget',
        'fern_colony_fingerprint',
        'fern_colony_original_fingerprint',
        'fern_colony_variants',
        'understory_ferns',
        'understory_saplings',
        'shrubs_enabled',
        'shrubs_plants',
        'shrubs_fingerprint',
        'shrubs_author_leaf_count',
        'shrubs_render_leaf_counts',
        'shrubs_author_instance_triangles',
        'shrubs_render_instance_triangles',
        'shrubs_uploaded_meshes',
        'shrubs_uploaded_stem_indices',
        'shrubs_uploaded_leaf_indices',
        'shrubs_submitted',
        'middle_layer_comparison',
        'middle_layer_population',
        'middle_layer_target',
        'middle_layer_reached_budget',
        'middle_layer_attempted',
        'middle_layer_transform_fingerprint',
        'submitted_middle_layer',
        'submitted_middle_layer_triangles',
        'fir_layout',
        'fir_layout_population',
        'fir_layout_reached_budget',
        'fir_layout_fingerprint',
        'fir_layout_patch_centres',
        'canopy_collision_mode',
        'canopy_collision_parts',
        'canopy_collision_spacing_m',
        'canopy_wood_source_triangles',
        'canopy_wood_surface_cells',
        'canopy_wood_enclosed_grid_cells',
        'canopy_world_profile',
        'canopy_enabled',
        'canopy_plants',
        'canopy_population_fingerprint',
        'canopy_ground_checks',
        'canopy_rooted_meshes',
        'canopy_primitive_triangles',
        'canopy_materials',
        'canopy_submitted_slots',
        'trail_litter_enabled',
        'trail_litter_patches',
        'trail_litter_branches',
        'trail_litter_stones',
        'trail_litter_fingerprint',
        'trail_litter_triangles')
    Assert-MistwatchFallenExactKeys $value.native_contract $nativeKeys 'complete independent native70'
    Assert-MistwatchFallenExactKeys $value.fallen_pose_words @('prop_0','prop_8','prop_34','prop_35') 'independent pose words'
    $authored=Get-MistwatchFallenAuthoredPoseWords
    foreach($key in $authored.Keys){if($value.fallen_pose_words[$key] -cne $authored[$key]){throw ('Independent pose differs from pinned shared author: '+$key)}}
}

function Get-MistwatchFallenAuthoredPoseWords {
    # 15 詞原文來自 shared Profile 的 f32 位元，前三個 authoredAnchor 不在 sidecar。
    return @{
        prop_0='3238398341,1024796084,3241262266,0,3217625051,0,1068049213,1068049213,1068049213,3238442842,1044640524,3241281628,1044090176,1044640524,1073806224,'
        prop_8='1091813229,1023675301,3181330098,0,3216474037,0,1066778211,1066778211,1066778211,1091790106,1043168043,3182714096,1052291648,1043168043,1071812608,'
        prop_34='3237915803,1023799674,1118709672,0,1070404137,0,1066919253,1066919253,1066919253,3237833004,1043331442,1118711732,1045598432,1043331442,1072140480,'
        prop_35='3235919452,1026027809,1115807277,0,3225584543,0,1069446028,1069446028,1069446028,3235886607,1046258763,1115802673,1074829330,1046258763,1053373440,'
    }
}
function Assert-MistwatchIndependentNativeFields($Receipt,$Expected) {
    Assert-MistwatchIndependentExpectedContract $Expected
    foreach($entry in $Expected.native_contract.GetEnumerator()){
        if(-not $Receipt.ContainsKey($entry.Key) -or [string]$Receipt[$entry.Key] -cne $entry.Value){
            throw ('Independent complete-world native field mismatch: '+$entry.Key)
        }
    }
}
function Read-MistwatchFallenExactSidecar([string]$Path,[string[]]$Keys) {
    $full=Assert-MistwatchFallenPhysicalPath $Path
    $value=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    foreach($line in [IO.File]::ReadAllLines($full,[Text.Encoding]::UTF8)){
        if([string]::IsNullOrWhiteSpace($line)){continue}
        $pair=$line.Split('=',2)
        if($pair.Count -ne 2 -or $pair[0] -cnotmatch '^[a-z][a-z0-9_]*$' -or $value.ContainsKey($pair[0])){throw ('Malformed or duplicate fallen sidecar: '+$full)}
        $value.Add($pair[0],$pair[1])
    }
    if($value.Count -ne $Keys.Count){throw ('Fallen exact key count mismatch: '+$full)}
    foreach($key in $Keys){if(-not $value.ContainsKey($key)){throw ('Fallen exact key missing: '+$key)}}
    return ,$value
}
function Assert-MistwatchFallenLogEvidence {
    param([Parameter(Mandatory=$true)][string]$Capture,[Parameter(Mandatory=$true)]$Expected)
    $capturePath=Assert-MistwatchFallenPhysicalPath $Capture
    $receiptPath=$capturePath+'.fallen-log.txt'
    $cleanupPath=$capturePath+'.fallen-log-cleanup.txt'
    $allowed=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $null=$allowed.Add([IO.Path]::GetFileName($receiptPath));$null=$allowed.Add([IO.Path]::GetFileName($cleanupPath))
    $sidecars=@(Get-ChildItem -LiteralPath (Split-Path $capturePath) -File -Force |
        Where-Object{$_.Name.StartsWith([IO.Path]::GetFileName($capturePath),[StringComparison]::OrdinalIgnoreCase) -and $_.Name -imatch 'fallen[-_]log'})
    if($sidecars.Count -ne 2){throw 'Missing or shadow fallen evidence files'}
    foreach($file in $sidecars){if(-not $allowed.Contains($file.Name)){throw ('Shadow fallen sidecar filename: '+$file.Name)}}
    $keys=@('profile','source_sha256','mesh_handle','material_handle','vertices','indices','triangles','materials',
        'use_uv','texture_scale','submitted_instances','submitted_triangles','retained_beam_handle','detail_view','prop_0','prop_8','prop_34','prop_35')
    $value=Read-MistwatchFallenExactSidecar $receiptPath $keys
    $cleanup=Read-MistwatchFallenExactSidecar $cleanupPath @('materials','mesh_destroy_calls','destroyed_handle','cleanup')
    $fixed=@{profile='scanned-straight-v1';source_sha256='b7f6a05bf928787082dea92a0051557496e45894a9c2b1a0af1e51110abd0d46';
        vertices='8956';indices='45807';triangles='15269';materials='1';use_uv='1';texture_scale='1';
        submitted_instances='4';submitted_triangles='61076';detail_view='-1'}
    foreach($key in $fixed.Keys){if($value[$key] -cne $fixed[$key]){throw ('Scanned fallen resource field mismatch: '+$key)}}
    foreach($key in @('mesh_handle','material_handle','retained_beam_handle')){
        [ulong]$number=0
        if($value[$key] -cnotmatch '^[1-9][0-9]{0,19}$' -or
           -not [ulong]::TryParse($value[$key],[Globalization.NumberStyles]::None,[Globalization.CultureInfo]::InvariantCulture,[ref]$number)){
            throw ('Noncanonical uploaded resource handle: '+$key)
        }
    }
    # 材質與 mesh 使用不同 handle 域；只要求真正新倒木 mesh 不挪用舊木梁 mesh。
    if($value['mesh_handle'] -ceq $value['retained_beam_handle']){throw 'Scanned log aliases the retained beam mesh'}
    foreach($key in @('prop_0','prop_8','prop_34','prop_35')){
        $words=@($value[$key].Split(','))
        if($words.Count -ne 16 -or $words[-1] -cne ''){throw ('Fallen pose requires exactly15 words plus trailing comma: '+$key)}
        foreach($word in $words[0..14]){
            [uint32]$number=0
            if($word -cnotmatch '^(0|[1-9][0-9]{0,9})$' -or
               -not [uint32]::TryParse($word,[Globalization.NumberStyles]::None,[Globalization.CultureInfo]::InvariantCulture,[ref]$number)){
                throw ('Noncanonical fallen f32 pose word: '+$key)
            }
        }
        if($value[$key] -cne $Expected.fallen_pose_words[$key]){throw ('Actual uploaded fallen pose differs from independent author: '+$key)}
    }
    if($cleanup['materials'] -cne '0' -or $cleanup['mesh_destroy_calls'] -cne '1' -or
       $cleanup['destroyed_handle'] -cne $value['mesh_handle'] -or $cleanup['cleanup'] -cne 'success'){
        throw 'Actual uploaded fallen same-handle cleanup not proven'
    }
    return @{profile=$value['profile'];mesh_handle=$value['mesh_handle'];material_handle=$value['material_handle'];
        receipt_sha256=(Get-FileHash -LiteralPath $receiptPath -Algorithm SHA256).Hash;
        cleanup_sha256=(Get-FileHash -LiteralPath $cleanupPath -Algorithm SHA256).Hash}
}



function Get-MistwatchFallenPackagePins {
    return @(
        @('meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh',434012,'B7F6A05BF928787082DEA92A0051557496E45894A9C2B1A0AF1E51110ABD0D46'),
        @('materials/slot-0-base.dds',5592580,'DA9DB4EDE331A22F4E47BA2EAB34CFCFB2C6979F6F180007E28DFC61EA23410B'),
        @('materials/slot-0-arm.dds',5592580,'147D81E27AFEAA1D1AFC6E88D70D3814912A7D6235110CA4935EDF6FFB5838FE'),
        @('materials/slot-0-normal.dds',5592580,'F2F8EBD0A4C52C543047FB8131AD42391374313D595CD0901DDF5136C2B01354'),
        @('fallen-log-package-manifest.json',10663,'B8F6AEBD3986B3A674981F45A6A6BFA0D4262CCCA458818D518CA3F8140EB98B'),
        @('LICENSE.md',1998,'CE9483C1783A84DC63A77C36B376D23A44598F1ECFC34A7A689BDA5EBC405083')
    )
}
function Assert-MistwatchFallenBuildRoots([string]$SourceRoot,[string]$BuildRoot) {
    $source=Assert-MistwatchFallenPhysicalPath $SourceRoot -Directory
    $build=Assert-MistwatchFallenPhysicalPath $BuildRoot -Directory
    if($source -ieq $build -or $build -ieq [IO.Path]::GetPathRoot($build)){throw 'Dedicated CMake build root required'}
    $cache=Assert-MistwatchFallenPhysicalPath (Join-Path $build 'CMakeCache.txt')
    $matches=@([IO.File]::ReadAllLines($cache)|Where-Object{$_ -clike 'CMAKE_HOME_DIRECTORY:INTERNAL=*'})
    if($matches.Count -ne 1 -or [IO.Path]::GetFullPath($matches[0].Substring(30)) -ine $source){throw 'CMake source/build identity mismatch'}
    return @{source=$source;build=$build}
}
function Get-MistwatchFallenRuntimeRows {
    param([Parameter(Mandatory=$true)][string]$AssetsRoot,[string]$SourceRoot='')
    $root=Assert-MistwatchFallenPhysicalPath $AssetsRoot -Directory
    $members=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    $rows=@(foreach($item in Get-ChildItem -LiteralPath $root -Force -Recurse){
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw ('Runtime reparse: '+$item.FullName)}
        if($item -is [IO.DirectoryInfo]){continue}
        if($item -isnot [IO.FileInfo]){throw 'Nonregular runtime member'}
        $relative=[IO.Path]::GetRelativePath($root,$item.FullName).Replace('\','/')
        if($members.ContainsKey($relative)){throw ('Duplicate/shadow runtime member: '+$relative)}
        $row=@{path=$relative;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash}
        $members.Add($relative,$row);$row
    })
    if($rows.Count -ne 387){throw 'Formal runtime requires exact381 original plus6 fallen files'}
    $fallen=@($rows|Where-Object{$_.path.StartsWith('mistwatch-fallen-logs/',[StringComparison]::Ordinal)})
    if($fallen.Count -ne 6){throw 'Formal fallen runtime requires exact6 package members'}
    foreach($pin in Get-MistwatchFallenPackagePins){
        $relative='mistwatch-fallen-logs/'+$pin[0]
        if(-not $members.ContainsKey($relative)){throw ('Missing fixed fallen runtime member: '+$relative)}
        $row=$members[$relative]
        if($row.bytes -ne $pin[1] -or $row.sha256 -cne $pin[2]){throw ('Fixed fallen raw runtime mismatch: '+$relative)}
        if($SourceRoot){
            $file=Join-Path $SourceRoot ('engine/assets/'+$relative)
            Assert-MistwatchFallenRow @{path=$file;bytes=$pin[1];sha256=$pin[2]}
        }
    }
    return @($rows|Sort-Object path)
}
function Assert-MistwatchFallenRelativeRows($Rows,[string]$Root,[string]$Label) {
    if($Rows -isnot [array] -or $Rows.Count -lt 1){throw ('Missing relative rows: '+$Label)}
    $actualRoot=Assert-MistwatchFallenPhysicalPath $Root -Directory
    $unique=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $Rows){
        Assert-MistwatchFallenExactKeys $row @('path','bytes','sha256') ($Label+' row')
        Assert-MistwatchFallenSha $row.sha256 ($Label+' row')
        if($row.path -isnot [string] -or [IO.Path]::IsPathRooted($row.path) -or $row.path.Contains('\') -or
            $row.path.Contains(':') -or $row.path.Split('/') -contains '.' -or $row.path.Split('/') -contains '..' -or
            -not $unique.Add($row.path)){throw ('Invalid/shadow relative row: '+$Label)}
        $full=[IO.Path]::GetFullPath((Join-Path $actualRoot $row.path))
        if(-not $full.StartsWith($actualRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Relative identity escaped its root'}
        Assert-MistwatchFallenRow @{path=$full;bytes=$row.bytes;sha256=$row.sha256}
    }
}

function Assert-MistwatchFallenToolchain($Toolchain) {
    Assert-MistwatchFallenExactKeys $Toolchain @('msc_full_ver','cplusplus','configuration','abi') 'formal CPU toolchain'
    # JSON 整數由 PowerShell 讀成 Int64；只接受整數，不轉換字串、浮點數或布林值。
    $compilerInteger=$Toolchain.msc_full_ver -is [int] -or $Toolchain.msc_full_ver -is [long]
    $languageInteger=$Toolchain.cplusplus -is [int] -or $Toolchain.cplusplus -is [long]
    if(-not $compilerInteger -or -not $languageInteger -or
        $Toolchain.abi -cne 'formal-AevryndEngine' -or $Toolchain.cplusplus -ne 202002 -or
        $Toolchain.msc_full_ver -lt 190000000 -or $Toolchain.msc_full_ver -gt [int]::MaxValue -or
        $Toolchain.configuration -cne 'Release-x64-C++20-/W4-/WX-/MP1-/fp:strict-MultiThreadedDLL'){
        throw 'CPU test did not declare reviewed formal engine ABI/toolchain'
    }
}

function Get-MistwatchIndependentExpected {
    param([Parameter(Mandatory=$true)][string]$SourceRoot,
        [Parameter(Mandatory=$true)][string]$BuildRoot,[Parameter(Mandatory=$true)][string]$ExecutablePath)
    $roots=Assert-MistwatchFallenBuildRoots $SourceRoot $BuildRoot
    $source=$roots.source;$build=$roots.build
    $exe=Assert-MistwatchFallenPhysicalPath $ExecutablePath
    $fixedExe=Join-Path $build 'bin/Release/Aevrynd-Mistwatch.exe'
    if($exe -ine [IO.Path]::GetFullPath($fixedExe)){throw 'Exact current CMake Mistwatch target path required'}
    $assets=Assert-MistwatchFallenPhysicalPath (Join-Path (Split-Path $exe) 'assets') -Directory
    $certificate=Assert-MistwatchFallenPhysicalPath (Join-Path $build 'games/mistwatch/native/fallen-cpu-certificate') -Directory
    if(Test-Path -LiteralPath (Join-Path $certificate 'failure.txt')){throw 'CPU first-failure evidence cannot qualify a certificate'}
    $expectedPath=Join-Path $certificate 'Expected.json'
    $receiptPath=Join-Path $certificate 'receipt.json'
    $value=Read-MistwatchFallenStrictJson $expectedPath
    $receipt=Read-MistwatchFallenStrictJson $receiptPath
    Assert-MistwatchFallenExactKeys $receipt @('schema','status','source_root','runtime_assets_root','full_world_cpu_passed',
        'native_expected_author_passed','source_input_unchanged','runtime_input_unchanged','expected_file','proof_file','cpu_binary','scope') 'formal CPU receipt'
    if($receipt.schema -cne 'mistwatch.formal-fallen.cpu-receipt.1' -or $receipt.status -cne 'full-world-cpu-passed' -or
        [IO.Path]::GetFullPath($receipt.source_root) -ine $source -or [IO.Path]::GetFullPath($receipt.runtime_assets_root) -ine $assets){throw 'Formal CPU receipt profile/root mismatch'}
    foreach($key in @('full_world_cpu_passed','native_expected_author_passed','source_input_unchanged','runtime_input_unchanged')){
        if($receipt[$key] -isnot [bool] -or -not $receipt[$key]){throw ('Formal CPU completion missing: '+$key)}
    }
    $scopeKeys=@('native_qualified','actual_mesh_query_qualified','actual_Main_route_qualified','mass_stability_qualified',
        'lower_envelope_distribution_qualified','art_accepted','game_adopted','full49_qualified','outer_process_qualified')
    Assert-MistwatchFallenExactKeys $receipt.scope $scopeKeys 'formal CPU scope'
    foreach($key in $scopeKeys){if($receipt.scope[$key] -isnot [bool] -or $receipt.scope[$key]){throw 'CPU certificate made native/art/outer process claim'}}
    Assert-MistwatchFallenRelativeRows @($receipt.expected_file,$receipt.proof_file) $certificate 'CPU certificate outputs'
    if($receipt.expected_file.path -cne 'Expected.json' -or $receipt.proof_file.path -cne 'world-seat.txt'){throw 'CPU certificate output names differ'}
    Assert-MistwatchFallenExactKeys $value @('schema','world_profile','fallen_log_profile','production_full_solid_volumes',
        'source','native_contract','fallen_pose_words','cpu_identity','provenance','scope') 'independent expected schema'
    if($value.schema -cne 'mistwatch.straight-log.native-expected.1' -or $value.world_profile -cne 'forest-wood' -or
        $value.fallen_log_profile -cne 'scanned-straight-v1' -or $value.production_full_solid_volumes -isnot [bool] -or
        -not $value.production_full_solid_volumes){throw 'Independent expected profile/full-volume producer mismatch'}
    Assert-MistwatchFallenExactKeys $value.scope @('full_world_cpu_passed','native_expected_author_passed','native_executed',
        'gpu_resources_qualified','art_accepted','game_adopted','full49','submission_fields') 'independent CPU scope'
    foreach($key in @('full_world_cpu_passed','native_expected_author_passed')){
        if($value.scope[$key] -isnot [bool] -or -not $value.scope[$key]){throw ('Independent CPU completion missing: '+$key)}
    }
    foreach($key in @('native_executed','gpu_resources_qualified','art_accepted','game_adopted','full49')){
        if($value.scope[$key] -isnot [bool] -or $value.scope[$key]){throw 'Independent CPU package made native/art claim'}
    }
    Assert-MistwatchFallenExactKeys $value.source @('sha256','bytes','vertices','indices','triangles') 'independent raw source'
    if($value.source.sha256 -cne 'b7f6a05bf928787082dea92a0051557496e45894a9c2b1a0af1e51110abd0d46' -or
        $value.source.bytes -ne 434012 -or $value.source.vertices -ne 8956 -or
        $value.source.indices -ne 45807 -or $value.source.triangles -ne 15269){throw 'Independent raw source identity differs'}
    Assert-MistwatchFallenExactKeys $value.cpu_identity @('rich_cover_word_fnv','terrain_fingerprint','original_prop_fingerprint',
        'original_nonlog_count','initial_world','repaired_world','grass_source_vertices','grass_source_triangles',
        'fern_source_triangles','fixed_scene_original_ferns','negative_controls') 'independent CPU identity'
    if($value.cpu_identity.initial_world.count -ne 4582 -or $value.cpu_identity.repaired_world.count -ne 4583 -or
        $value.cpu_identity.original_nonlog_count -ne 32){throw 'Complete CPU world/retained cohort identity differs'}
    $p=$value.provenance
    Assert-MistwatchFallenExactKeys $p @('schema','source_root','source_files','runtime_assets_root','runtime_assets',
        'fallen_source_assets','raw_mesh','test_source','cpu_binary','toolchain','source_input_unchanged','runtime_input_unchanged','outer_receipt_scope') 'formal CPU provenance'
    if($p.schema -cne 'mistwatch.formal-fallen-native-provenance.1' -or
        [IO.Path]::GetFullPath($p.source_root) -ine $source -or [IO.Path]::GetFullPath($p.runtime_assets_root) -ine $assets -or
        $p.source_input_unchanged -isnot [bool] -or -not $p.source_input_unchanged -or
        $p.runtime_input_unchanged -isnot [bool] -or -not $p.runtime_input_unchanged){throw 'Formal CPU provenance roots/input stability differ'}
    Assert-MistwatchFallenRelativeRows $p.source_files $source 'formal CPU source closure'
    Assert-MistwatchFallenRelativeRows @($p.raw_mesh,$p.test_source) $source 'formal CPU raw mesh/test'
    Assert-MistwatchFallenRelativeRows $p.fallen_source_assets $source 'formal fallen source6'
    if(@($p.fallen_source_assets).Count -ne 6){throw 'Formal fallen source requires exact6 rows'}
    $sources=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $p.source_files){$sources.Add($row.path,$row)}
    foreach($row in @($p.raw_mesh,$p.test_source)+@($p.fallen_source_assets)){
        if(-not $sources.ContainsKey($row.path) -or $sources[$row.path].bytes -ne $row.bytes -or
            $sources[$row.path].sha256 -cne $row.sha256){throw 'CPU subset row not bound by same complete source closure'}
    }
    foreach($pin in Get-MistwatchFallenPackagePins){
        $path='engine/assets/mistwatch-fallen-logs/'+$pin[0]
        $matched=@($p.fallen_source_assets|Where-Object{$_.path -ceq $path})
        if($matched.Count -ne 1 -or $matched[0].bytes -ne $pin[1] -or $matched[0].sha256 -cne $pin[2]){throw 'Formal fallen source6 fixed membership/hash mismatch'}
    }
    $rawMesh='engine/assets/mistwatch-fallen-logs/meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh'
    if($p.raw_mesh.path -cne $rawMesh -or $p.test_source.path -cne 'games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp'){throw 'CPU raw mesh/test source escaped fixed formal paths'}
    $mandatory=@('CMakeLists.txt','engine/native/CMakeLists.txt','games/mistwatch/native/CMakeLists.txt',
        'games/mistwatch/native/src/main.cpp',
        'games/mistwatch/tools/author_root_stem.py','games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp',
        'games/mistwatch/native/tests/fallen/MistwatchFallenSeatSupport.h',
        'games/mistwatch/native/tests/fallen/MistwatchFallenSourceSupport.h')
    foreach($path in $mandatory){if(-not $sources.ContainsKey($path)){throw ('Missing CPU source closure member: '+$path)}}
    $completeSources=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($path in $mandatory){$null=$completeSources.Add($path)}
    foreach($row in $p.fallen_source_assets){$null=$completeSources.Add($row.path)}
    foreach($directory in @('games/mistwatch/native/include','engine/native/include','engine/native/src','engine/native/shaders')){
        $dir=Assert-MistwatchFallenPhysicalPath (Join-Path $source $directory) -Directory
        foreach($item in Get-ChildItem -LiteralPath $dir -Recurse -Force){
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'CPU source closure physical reparse'}
            if($item -is [IO.DirectoryInfo]){continue}
            $relative=[IO.Path]::GetRelativePath($source,$item.FullName).Replace('\','/')
            if(-not $sources.ContainsKey($relative)){throw ('Incomplete CPU source closure: '+$relative)}
            $null=$completeSources.Add($relative)
        }
    }
    if($sources.Count -ne $completeSources.Count){throw 'CPU source closure exact membership differs'}
    foreach($path in $sources.Keys){if(-not $completeSources.Contains($path)){throw 'CPU source closure extra/shadow member'}}
    Assert-MistwatchFallenRow $p.cpu_binary
    Assert-MistwatchFallenRow $receipt.cpu_binary
    Assert-MistwatchFallenSha $p.cpu_binary.sha256 'formal CPU executable'
    Assert-MistwatchFallenSha $receipt.cpu_binary.sha256 'formal CPU receipt executable'
    if([IO.Path]::GetFullPath($p.cpu_binary.path) -ine [IO.Path]::GetFullPath($receipt.cpu_binary.path) -or
        $p.cpu_binary.bytes -ne $receipt.cpu_binary.bytes -or $p.cpu_binary.sha256 -cne $receipt.cpu_binary.sha256 -or
        [IO.Path]::GetFileName($p.cpu_binary.path) -cne 'MistwatchFallenWorldQualificationTest.exe' -or
        -not [IO.Path]::GetFullPath($p.cpu_binary.path).StartsWith($build+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'CPU executable identity/build differs'}
    Assert-MistwatchFallenToolchain $p.toolchain
    $runtime=@(Get-MistwatchFallenRuntimeRows -AssetsRoot $assets -SourceRoot $source)
    Assert-MistwatchFallenRelativeRows $p.runtime_assets $assets 'CPU full387 runtime'
    if(@($p.runtime_assets).Count -ne 387){throw 'CPU provenance did not include exact387 runtime'}
    $before=@($p.runtime_assets|Sort-Object path|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    $now=@($runtime|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    if(($before-join "\n") -cne ($now-join "\n")){throw 'CPU/current native runtime387 membership or bytes differ'}
    Assert-MistwatchIndependentExpectedContract $value
    return ,$value
}
