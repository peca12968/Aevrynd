#requires -Version 7.0
# 新版完整森林的純 CPU 契約與實體讀取；GPU、實機流程、人工遊玩與美術仍須各自驗收。
. (Join-Path $PSScriptRoot 'ForestSurfaceContract.ps1')
function Copy-MistwatchCurrentObject($Value){
    $copy=[Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
    foreach($entry in $Value.GetEnumerator()){$copy.Add($entry.Key,$entry.Value)}
    return ,$copy
}

function Read-MistwatchCurrentWorldCertificate {
    param([Parameter(Mandatory=$true)][string]$CertificateRoot,[Parameter(Mandatory=$true)][string]$SourceRoot,
        [Parameter(Mandatory=$true)][string]$BuildRoot,[Parameter(Mandatory=$true)][string]$ExecutablePath)
    $roots=Assert-MistwatchFallenBuildRoots $SourceRoot $BuildRoot
    $source=$roots.source;$build=$roots.build
    $exe=Assert-MistwatchFallenPhysicalPath $ExecutablePath
    if($exe -ine [IO.Path]::GetFullPath((Join-Path $build 'bin/Release/Aevrynd-Mistwatch.exe'))){throw 'Authored fixed current target path required'}
    $assets=Assert-MistwatchFallenPhysicalPath (Join-Path (Split-Path $exe) 'assets') -Directory
    $certificate=Assert-MistwatchFallenPhysicalPath $CertificateRoot -Directory
    if(Test-Path -LiteralPath (Join-Path $certificate 'failure.txt')){throw 'Authored first-failure evidence cannot qualify certificate'}
    $expected=Read-MistwatchFallenStrictJson (Join-Path $certificate 'NativeExpected.json')
    $receipt=Read-MistwatchFallenStrictJson (Join-Path $certificate 'NativeReceipt.json')
    Assert-MistwatchCurrentWorldExpected $expected
    Assert-MistwatchCurrentWorldReceipt $receipt $true
    Assert-MistwatchFallenRelativeRows @($receipt.expected_file,$receipt.world_expected_file,$receipt.proof_file) $certificate 'authored fixed outputs'
    $p=$expected.provenance
    if([IO.Path]::GetFullPath($p.source_root) -ine $source -or [IO.Path]::GetFullPath($p.runtime_assets_root) -ine $assets){throw 'Authored provenance current source/runtime roots differ'}
    Assert-MistwatchFallenRelativeRows $p.source_files $source 'authored exact source closure'
    $required=@('CMakeLists.txt','engine/native/CMakeLists.txt','games/mistwatch/native/CMakeLists.txt','games/mistwatch/native/src/main.cpp',
        'games/mistwatch/tools/author_root_stem.py','games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp',
        'games/mistwatch/native/tests/fallen/MistwatchFallenSeatSupport.h','games/mistwatch/native/tests/fallen/MistwatchFallenSourceSupport.h',
        'games/mistwatch/native/tests/fallen/MistwatchQualificationRuntime.h','games/mistwatch/native/tests/qualification/MistwatchQualificationRuntimeTest.cpp',
        'games/mistwatch/native/tests/qualification/runtime-profiles-v1.json','games/mistwatch/native/tests/fallen/MistwatchAuthoredWorldQualification.h',
        'games/mistwatch/native/tests/qualification/MistwatchAuthoredWorldQualificationTest.cpp','games/mistwatch/native/tests/fallen/MistwatchAuthoredNativeExpected.h',
        'games/mistwatch/native/tests/qualification/MistwatchAuthoredNativeExpectedTest.cpp','games/mistwatch/tools/AuthoredNativeContract.ps1',
        'games/mistwatch/native/tests/MistwatchAuthoredNativeReceiptTest.ps1')
    $required+=@('games/mistwatch/tools/ForestSurfaceContract.ps1',
        'games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json',
        'games/mistwatch/native/tests/qualification/runtime-forest-surface-full-v4.json',
        'games/mistwatch/native/tests/qualification/runtime-current-forest-full-v5.json',
        'games/mistwatch/native/tests/fallen/MistwatchCurrentWorldExpected.h',
        'games/mistwatch/native/tests/qualification/MistwatchCurrentWorldExpectedTest.cpp',
        'games/mistwatch/tools/CurrentWorldContract.ps1','games/mistwatch/tools/CurrentWorldEvidence.ps1',
        'games/mistwatch/tools/run_current_native_ctest.ps1','games/mistwatch/tools/validate_current_native_capture.ps1',
        'games/mistwatch/tools/stage_current_runtime.py','games/mistwatch/native/tests/MistwatchCurrentWorldReceiptTest.ps1')
    foreach($entry in (Get-MistwatchLeafClusterRuntimeAdditions $source)+(Get-MistwatchForestSurfaceRuntimeAdditions $source)){$required+=[string]$entry[1]}
    $wanted=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($path in $required){$null=$wanted.Add($path)}
    foreach($pin in Get-MistwatchFallenPackagePins){$null=$wanted.Add('engine/assets/mistwatch-fallen-logs/'+$pin[0])}
    foreach($directory in @('games/mistwatch/native/include','engine/native/include','engine/native/src','engine/native/shaders',
        'engine/assets/mistwatch-nettle-plant','engine/assets/material-packs/poly-haven/forest_leaves_04')){
        $dir=Assert-MistwatchFallenPhysicalPath (Join-Path $source $directory) -Directory
        foreach($file in Get-ChildItem -LiteralPath $dir -Recurse -Force){
            if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Authored source closure reparse'}
            if($file -is [IO.DirectoryInfo]){continue}
            if($file -isnot [IO.FileInfo]){throw 'Authored source regular file required'}
            $null=$wanted.Add([IO.Path]::GetRelativePath($source,$file.FullName).Replace('\','/'))
        }
    }
    if($p.source_files.Count -ne $wanted.Count){throw 'Authored complete source closure membership differs'}
    $sources=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    foreach($row in $p.source_files){if(-not $wanted.Contains($row.path)){throw 'Authored extra source closure member'};$sources.Add($row.path,$row)}
    Assert-MistwatchFallenRelativeRows $p.fallen_source_assets $source 'authored fallen6'
    if($p.fallen_source_assets.Count -ne 6){throw 'Authored fallen6 quota differs'}
    foreach($pin in Get-MistwatchFallenPackagePins){
        $path='engine/assets/mistwatch-fallen-logs/'+$pin[0]
        $row=@($p.fallen_source_assets|Where-Object{$_.path -ceq $path})
        if($row.Count -ne 1 -or $row[0].bytes -ne $pin[1] -or $row[0].sha256 -cne $pin[2]){throw 'Authored fixed fallen6 pin differs'}
    }
    $raw='engine/assets/mistwatch-fallen-logs/meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh'
    if($p.raw_mesh.path -cne $raw -or $p.test_source.path -cne 'games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp'){throw 'Authored raw/test fixed path differs'}
    foreach($row in @($p.raw_mesh,$p.test_source)+@($p.fallen_source_assets)){
        if(-not $sources.ContainsKey($row.path) -or $sources[$row.path].bytes -ne $row.bytes -or $sources[$row.path].sha256 -cne $row.sha256){throw 'Authored source subset not bound by full closure'}
    }
    foreach($row in @($p.cpu_binary,$receipt.cpu_binary)){
        Assert-MistwatchFallenRow $row (Join-Path $build 'bin/Release/MistwatchFallenWorldQualificationTest.exe')
    }
    Assert-MistwatchAuthoredNativeSameTree $p.cpu_binary $receipt.cpu_binary 'Current exact same CPU binary'
    $generated=[IO.Path]::GetFullPath((Join-Path $build 'games/mistwatch/native/generated-root-stem'))
    if([IO.Path]::GetFullPath($p.generated_root_stem_override) -ine $generated){throw 'Current generated root must be from this exact build'}
    $runtime=Get-MistwatchAuthoredNativeRuntimeRows $source $assets $expected.runtime_profile $generated
    Assert-MistwatchFallenRelativeRows $p.runtime_assets $assets 'authored runtime provenance'
    $declared=@($p.runtime_assets|Sort-Object path|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    $actual=@($runtime|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    if(($declared-join "`n") -cne ($actual-join "`n")){throw 'Authored current/provenance complete runtime differs'}
    $world=Read-MistwatchFallenStrictJson (Join-Path $certificate 'WorldExpected.json')
    Assert-MistwatchCurrentWorldCpuExpected $world $expected
    $worldReceipt=Read-MistwatchFallenStrictJson (Join-Path $certificate 'receipt.json')
    Assert-MistwatchCurrentWorldReceipt $worldReceipt $false
    Assert-MistwatchFallenRelativeRows @($worldReceipt.expected_file,$worldReceipt.proof_file) $certificate 'Current world receipt sealed outputs'
    Assert-MistwatchAuthoredNativeSameTree $worldReceipt.expected_file $receipt.world_expected_file 'Current world output cross-receipt identity'
    Assert-MistwatchAuthoredNativeSameTree $worldReceipt.proof_file $receipt.proof_file 'Current proof cross-receipt identity'
    Assert-MistwatchAuthoredNativeSameTree $worldReceipt.cpu_binary $p.cpu_binary 'Current world receipt CPU binary identity'
    $proof=Assert-MistwatchFallenPhysicalPath (Join-Path $certificate 'world-seat.txt')
    $text=[IO.File]::ReadAllText($proof,[Text.Encoding]::UTF8)
    if($text -cmatch 'FIRST_DIVERGENCE' -or $text -cnotmatch '(?m)^CURRENT_WORLD_EXPECTATION_CPU_PASS fields=153 legacy_fields=153 extras=105 surface=16 profiles=10 shrubs=128 original_shrub_prefix=64 canopy=8 wood=8617 before=8853 after=8854 runtime=418 native=false GPU=false full49=false\r?$'){
        throw 'Current complete CPU proof absent or contains first failure'
    }
    $typed=Assert-MistwatchFallenPhysicalPath (Join-Path $certificate 'typed-worlds.txt')
    $matches=[regex]::Matches($text,'(?m)^AUTHORED_TYPED_WORLDS bytes=([0-9]+) sha256=([0-9a-f]{64}) before=8853 after=8854 source_ids_preserved=true\r?$')
    if($matches.Count -ne 1 -or (Get-Item -LiteralPath $typed).Length -cne [long]$matches[0].Groups[1].Value -or
       (Get-FileHash -LiteralPath $typed -Algorithm SHA256).Hash -cne $matches[0].Groups[2].Value.ToUpperInvariant()){
        throw 'Current typed world raw identity differs from sealed CPU proof'
    }
    $lines=[IO.File]::ReadAllLines($typed,[Text.Encoding]::UTF8)
    if($lines.Count -ne 17707){throw 'Current typed world requires exact8853+8854 rows'}
    $offset=0
    foreach($state in 0..1){
        $count=8853+$state
        for($i=0;$i -lt $count;$i++){
            if(-not $lines[$offset++].StartsWith(('SOLID state='+$state+' index='+$i+' role='),[StringComparison]::Ordinal)){throw 'Current typed world state/index identity differs'}
        }
    }
    Assert-MistwatchCurrentFootprintEvidence $certificate $text $expected $lines
    $allowed=@('NativeExpected.json','NativeReceipt.json','WorldExpected.json','receipt.json','world-seat.txt','typed-worlds.txt',
        'footprint-prop-0.txt','footprint-prop-8.txt','footprint-prop-34.txt','footprint-prop-35.txt')
    $members=@(Get-ChildItem -LiteralPath $certificate -Force -Recurse)
    Assert-MistwatchCurrentCertificateMemberNames @($members.Name)
    foreach($file in $members){
        if($file -isnot [IO.FileInfo] -or $file.Name -cnotin $allowed){throw 'Current CPU certificate shadow/extra member'}
        $null=Assert-MistwatchFallenPhysicalPath $file.FullName
    }
    # 回傳完整新版 CPU 預期；不將讀取成功宣稱成 GPU、49項或人工遊玩通過。
    return ,$expected
}


function Get-MistwatchCurrentFootprintPins([string]$ProofText){
    $lines=@([regex]::Matches($ProofText,'(?mi)^SEATED_PROP\b[^\r\n]*')|ForEach-Object{$_.Value})
    if($lines.Count -ne 4){throw 'Current complete world requires exact four seated footprint proof declarations'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $pins=@(foreach($line in $lines){
        $m=[regex]::Match($line,'^SEATED_PROP id=(0|8|34|35) complete_proof=(footprint-prop-\1\.txt) bytes=([1-9][0-9]{0,7}) sha256=([0-9a-f]{64}) final_min=(0x[0-9a-f]+/0x[1-9a-f][0-9a-f]*) predecessor_min=(-0x[1-9a-f][0-9a-f]*/0x[1-9a-f][0-9a-f]*)$')
        if(-not $m.Success -or -not $seen.Add($m.Groups[1].Value)){throw 'Current footprint declaration has wrong identity, duplicate ID, path, hash or seating sign'}
        [long]$bytes=$m.Groups[3].Value
        if($bytes -gt 33554432){throw 'Current footprint proof exceeds bounded32MiB'}
        @{id=$m.Groups[1].Value;path=$m.Groups[2].Value;bytes=$bytes;sha256=$m.Groups[4].Value.ToUpperInvariant();
            final_min=$m.Groups[5].Value;predecessor_min=$m.Groups[6].Value}
    })
    foreach($id in @('0','8','34','35')){if(-not $seen.Contains($id)){throw 'Current footprint proof role missing'}}
    return ,$pins
}
function Assert-MistwatchCurrentCertificateMemberNames([string[]]$Names){
    $expected=@('NativeExpected.json','NativeReceipt.json','WorldExpected.json','receipt.json','world-seat.txt','typed-worlds.txt',
        'footprint-prop-0.txt','footprint-prop-8.txt','footprint-prop-34.txt','footprint-prop-35.txt')
    if($Names.Count -ne 10){throw 'Current CPU certificate exact ten-file membership differs'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($name in $Names){if($name -cnotin $expected -or -not $seen.Add($name)){throw 'Current CPU certificate duplicate/shadow/extra member'}}
}
function Convert-MistwatchCurrentHexFloatWord([string]$Text){
    $m=[regex]::Match($Text,'^(-?)0x([01])\.([0-9a-f]{13})p([+-](?:0|[1-9][0-9]{0,2}))$')
    if(-not $m.Success){throw 'Current footprint requires canonical C++ finite hexfloat'}
    $mantissa=[Convert]::ToUInt64($m.Groups[2].Value+$m.Groups[3].Value,16)
    [int]$exponent=$m.Groups[4].Value
    if($exponent -lt -149 -or $exponent -gt 127){throw 'Current footprint f32 exponent outside finite domain'}
    [double]$number=[double]$mantissa*[Math]::Pow(2.,$exponent-52)
    if($m.Groups[1].Value -ceq '-'){$number=-$number}
    [single]$f32=$number
    if(-not [double]::IsFinite($number) -or -not [single]::IsFinite($f32) -or [double]$f32 -ne $number){throw 'Current footprint hexfloat was not exact finite f32'}
    return [BitConverter]::ToUInt32([BitConverter]::GetBytes($f32),0)
}
function Assert-MistwatchCurrentFootprintHeader([string]$Header,[string]$Complete,$Pin,$Expected,[string[]]$TypedLines){
    $id=$Pin.id
    $m=[regex]::Match($Header,('^PROP '+$id+' original_anchor=(\S+) position=(\S+) rotationYXZ=(\S+) uniform_scale=(\S+) new_conservative_box=(\S+) extent=(\S+) predecessorY=(\S+)$'))
    if(-not $m.Success){throw 'Current footprint header role/schema differs'}
    foreach($word in $m.Groups[1].Value.Split(',')){$null=Convert-MistwatchCurrentHexFloatWord $word}
    if($m.Groups[1].Value.Split(',').Count -ne 3){throw 'Current original footprint anchor must have three components'}
    $words=@(foreach($group in 2..6){
        $parts=$m.Groups[$group].Value.Split(',')
        if($parts.Count -ne 3){throw 'Current footprint pose/box must have three components'}
        foreach($word in $parts){Convert-MistwatchCurrentHexFloatWord $word}
    })
    if(($words-join ',')+',' -cne $Expected.fallen_pose_words['prop_'+$id]){throw 'Current footprint f32 pose/box differs from fixed independent fallen author'}
    $predecessor=Convert-MistwatchCurrentHexFloatWord $m.Groups[7].Value
    if($words[1] -eq 0 -or $words[1] -ge 2147483648 -or $predecessor -ne $words[1]-1){throw 'Current footprint predecessor must be one f32 below positive fixed seat'}
    $completeMatch=[regex]::Match($Complete,'^COMPLETE source_faces=15269 nonempty_pairs=([1-9][0-9]*) constraints=([1-9][0-9]*) rational_words=([1-9][0-9]*) final_min=(\S+) final_max_all_surface=(0x[0-9a-f]+/0x[1-9a-f][0-9a-f]*) final_negative=0 predecessor_min=(\S+) lattice_evaluations=2 art=false lower_envelope_distribution_qualified=false$')
    if(-not $completeMatch.Success -or $completeMatch.Groups[4].Value -cne $Pin.final_min -or
       $completeMatch.Groups[6].Value -cne $Pin.predecessor_min){throw 'Current footprint complete overlay/seating proof differs from sealed summary'}
    $box=$m.Groups[5].Value+' extent='+$m.Groups[6].Value
    foreach($state in 0..1){
        $matching=@($TypedLines|Where-Object{$_ -cmatch ('^SOLID state='+$state+' index=(0|[1-9][0-9]*) role=3 role_id='+$id+' ')})
        if($matching.Count -ne 1 -or -not $matching[0].EndsWith((' role=3 role_id='+$id+' '+$box),[StringComparison]::Ordinal)){
            throw 'Current footprint collider not bound to same fixed log role in both typed worlds'
        }
    }
}
function Assert-MistwatchCurrentFootprintEvidence([string]$CertificateRoot,[string]$ProofText,$Expected,[string[]]$TypedLines){
    $pins=Get-MistwatchCurrentFootprintPins $ProofText
    foreach($pin in $pins){
        $path=Join-Path $CertificateRoot $pin.path
        Assert-MistwatchFallenRow @{path=$path;bytes=$pin.bytes;sha256=$pin.sha256}
        # 封存摘要已綁完整約10MB原始證據；只額外串接其15個f32與兩個完整世界，不重跑精確幾何解算。
        $reader=[IO.StreamReader]::new($path,[Text.Encoding]::UTF8)
        try{$header=$reader.ReadLine();$complete=$reader.ReadLine()}finally{$reader.Dispose()}
        Assert-MistwatchCurrentFootprintHeader $header $complete $pin $Expected $TypedLines
    }
}

function Get-MistwatchCurrentProjectionNames{
    return @('shrubs_plants','shrubs_fingerprint','shrubs_author_instance_triangles','shrubs_render_instance_triangles','shrubs_submitted',
        'planked_workshop_profile','planked_workshop_vertices','planked_workshop_indices','planked_workshop_uv_fingerprint',
        'ground_art_surface_path','ground_art_surface_encoding','ground_packed_orm','ground_habitat_layer_surface_path',
        'ground_habitat_layer_surface_encoding','ground_loaded_texture_identity','ground_habitat_layer_source')
}
function Get-MistwatchCurrentExtraNames{
    return @((@'
boulder_base_path boulder_base_sha256 boulder_color_profile boulder_geometry_profile boulder_gpu_resources boulder_instances boulder_mesh_triangles boulder_mesh_vertices canopy_cleanup_mesh_destroy_calls entrance_road_base_path entrance_road_forest_fingerprint entrance_road_forest_triangles entrance_road_materials entrance_road_normal_path entrance_road_original_terrain_drawn entrance_road_profile entrance_road_receipt_scope entrance_road_road_fingerprint entrance_road_road_triangles entrance_road_shared_vertices entrance_road_source_fingerprint entrance_road_source_triangles entrance_road_submitted entrance_road_surface_encoding entrance_road_surface_path entrance_road_texture_identity entrance_understory_attempted entrance_understory_available_shared_meshes entrance_understory_cluster_counts entrance_understory_clusters entrance_understory_fingerprint entrance_understory_left entrance_understory_population entrance_understory_profile entrance_understory_right entrance_understory_submitted entrance_understory_suppressed_broadleaf entrance_understory_suppressed_grass entrance_understory_suppression_fingerprint forest_cover_retained_render_variants landscape_fir_cleanup_mesh_destroy_calls landscape_stand_added_population landscape_stand_base_population landscape_stand_pose_fingerprint landscape_stand_profile landscape_stand_source_fingerprint landscape_twig_far_fingerprint landscape_twig_far_triangles landscape_twig_near_fingerprint landscape_twig_near_triangles landscape_twig_profile roadside_broadleaf_attempted roadside_broadleaf_fingerprint roadside_broadleaf_left roadside_broadleaf_materials roadside_broadleaf_population roadside_broadleaf_profile roadside_broadleaf_retained_variants roadside_broadleaf_right settlement_ground_boundary_vertices settlement_ground_changed_forest settlement_ground_changed_road settlement_ground_moved_triangles settlement_ground_original_forest_fingerprint settlement_ground_original_road_fingerprint settlement_ground_profile settlement_surface_base_path settlement_surface_material_identity settlement_surface_material_occlusion settlement_surface_normal_path settlement_surface_orm_path settlement_surface_profile settlement_surface_submitted shelter_detail_corrugations shelter_detail_frame_members shelter_detail_profile shelter_detail_roof_fingerprint shelter_detail_roof_indices shelter_detail_roof_material shelter_detail_roof_vertices shelter_detail_wood_fingerprint shelter_detail_wood_indices shelter_detail_wood_vertices shrubs_preserved_prefix shrubs_profile shrubs_target workshop_frame_fingerprint workshop_frame_members workshop_frame_profile
'@).Trim()-split ' ')+
        @(foreach($i in 0..1){foreach($name in @('identity','triangles')){'roadside_broadleaf_mesh_'+$i+'_'+$name}})+
        @(foreach($i in 0..2){foreach($name in @('fingerprint','triangles')){'settlement_surface_mesh_'+$i+'_'+$name}})+
        @(foreach($i in 0..2){foreach($name in @('bytes','sha256')){'settlement_surface_channel_'+$i+'_'+$name}})
}
function Get-MistwatchCurrentExtraTokens{
    return @{
        boulder_base_path='materials/slot-0-base-source-srgb-v1.dds';
        boulder_base_sha256='d874d5ac2e2c01ca7bd87970ba23415ef285e051b0a501c74e383e6219758b79';
        boulder_color_profile='source-srgb-v1';boulder_geometry_profile='lod0-source-v1';
        entrance_road_profile='entrance-road-material-domain-v1';entrance_road_receipt_scope='combined-source-before-material-split';
        entrance_understory_profile='entrance-understory-v2';roadside_broadleaf_profile='roadside-broadleaf-v1';
        settlement_ground_profile='workshop-ground-domain-v1';settlement_surface_profile='workshop-gravel-surface-candidate-v1';
        shelter_detail_profile='framed-corrugated-v1';shelter_detail_roof_material='procedural-weathered-metal';
        shrubs_profile='woodland-feasible-arc-v3';workshop_frame_profile='framed-workshop-v1';
        landscape_twig_profile='crown-distance-lod-v1';landscape_stand_profile='landscape-stand-fill-v2'
    }
}
function Get-MistwatchCurrentArtProfiles{
    return @{Shelter='framed-corrugated-v1';Workshop='framed-workshop-v1';Road='entrance-road-material-domain-v1';
        Roadside='roadside-broadleaf-v1';Understory='entrance-understory-v2';BoulderColor='source-srgb-v1';
        BoulderGeometry='lod0-source-v1';Shrubs='woodland-feasible-arc-v3';LandscapeTwig='crown-distance-lod-v1';LandscapeStand='landscape-stand-fill-v2'}
}
function Assert-MistwatchCurrentProvenance($Value){
    Assert-MistwatchFallenExactKeys $Value @('schema','world_cohort','authored_world_qualified','runtime_profile','generated_root_stem_override',
        'source_root','source_files','runtime_assets_root','runtime_assets','fallen_source_assets','raw_mesh','test_source',
        'cpu_binary','toolchain','source_input_unchanged','runtime_input_unchanged','outer_receipt_scope') 'current provenance'
    foreach($pair in @(@('schema','mistwatch.current-world.cpu-provenance.1'),@('world_cohort','entrance-current-forest-surface-v1'),
        @('runtime_profile','authored-current-forest-full-v5'))){
        if($Value[$pair[0]] -isnot [string] -or $Value[$pair[0]] -cne $pair[1]){throw ('Current provenance differs: '+$pair[0])}
    }
    if($Value.generated_root_stem_override -isnot [string] -or -not [IO.Path]::IsPathFullyQualified($Value.generated_root_stem_override)){
        throw 'Current generated root must be explicit absolute path'
    }
    # 僅重用既有純形狀、型別與範圍檢查；此暫存投影從不傳給實體讀取器或成為舊版資格。
    $shape=Copy-MistwatchCurrentObject $Value
    $shape.schema='mistwatch.authored-world.cpu-provenance.1';$shape.world_cohort='entrance-authored-habitat-v1';$shape.runtime_profile='authored-leaf-cluster-full-v3'
    Assert-MistwatchAuthoredNativeProvenanceShape $shape 'authored-leaf-cluster-full-v3'
    if(@($Value.runtime_assets).Count -ne 418 -or @($Value.fallen_source_assets).Count -ne 6){throw 'Current provenance requires exact418 runtime and six fallen source rows'}
}
function Get-MistwatchCurrentLegacyShape($Value){
    $shape=[Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
    foreach($key in @('schema','world_cohort','runtime_profile','world_profile','fallen_log_profile','production_full_solid_volumes',
        'source','native_contract','fallen_pose_words','cpu_identity','provenance','scope')){$shape.Add($key,$Value[$key])}
    $shape.schema='mistwatch.authored-world.native-expected.1';$shape.world_cohort='entrance-authored-habitat-v1'
    $shape.runtime_profile='authored-leaf-cluster-full-v3';$shape.native_contract=$Value.legacy_native_contract
    $shape.provenance=Copy-MistwatchCurrentObject $Value.provenance
    $shape.provenance.schema='mistwatch.authored-world.cpu-provenance.1';$shape.provenance.world_cohort='entrance-authored-habitat-v1'
    $shape.provenance.runtime_profile='authored-leaf-cluster-full-v3'
    return ,$shape
}
function Assert-MistwatchCurrentExtras($Value){
    Assert-MistwatchFallenExactKeys $Value (Get-MistwatchCurrentExtraNames) 'current exact105 extras'
    $tokens=Get-MistwatchCurrentExtraTokens
    $csv=@{entrance_understory_cluster_counts=6;forest_cover_retained_render_variants=5;roadside_broadleaf_retained_variants=2}
    $paths=@('entrance_road_base_path','entrance_road_surface_path','entrance_road_normal_path',
        'settlement_surface_base_path','settlement_surface_orm_path','settlement_surface_normal_path')
    foreach($key in $Value.Keys){
        $v=$Value[$key]
        if($v -isnot [string] -or $v.Length -lt 1 -or $v.Length -gt 512 -or $v -match '[\x00-\x1F\x7F]'){throw ('Current extra string required: '+$key)}
        if($tokens.ContainsKey($key)){if($v -cne $tokens[$key]){throw ('Current extra fixed profile differs: '+$key)}}
        elseif($csv.ContainsKey($key)){Assert-MistwatchAuthoredNativeCsv $v $csv[$key] $key}
        elseif($key -cin $paths){Assert-MistwatchAuthoredNativeRelative $v $key}
        elseif($key -cmatch '^settlement_surface_channel_[0-2]_sha256$'){
            if($v -cnotmatch '^[0-9a-f]{64}$'){throw 'Current source channel SHA must be lowercase64'}
        }elseif($key -ceq 'settlement_surface_material_occlusion'){
            [double]$number=0
            if($v -cnotmatch '^(0|1)(\.[0-9]+)?$' -or -not [double]::TryParse($v,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$number) -or
               -not [double]::IsFinite($number) -or $number -lt 0 -or $number -gt 1){throw 'Current material occlusion scalar differs'}
        }else{Assert-MistwatchAuthoredNativeUnsigned $v $key}
    }
    $fixed=@{boulder_gpu_resources='1';boulder_instances='29';boulder_mesh_vertices='81373';boulder_mesh_triangles='66122';
        canopy_cleanup_mesh_destroy_calls='11';entrance_road_materials='2';entrance_road_original_terrain_drawn='0';entrance_road_submitted='3';
        entrance_understory_available_shared_meshes='8';entrance_understory_clusters='6';entrance_understory_population='24';
        entrance_understory_left='12';entrance_understory_right='12';entrance_understory_submitted='24';
        landscape_fir_cleanup_mesh_destroy_calls='5';landscape_stand_added_population='96';landscape_stand_base_population='520';
        roadside_broadleaf_population='96';roadside_broadleaf_left='48';roadside_broadleaf_right='48';roadside_broadleaf_materials='1';
        settlement_surface_submitted='3';shrubs_preserved_prefix='64';shrubs_target='128';workshop_frame_members='54';
        shelter_detail_frame_members='57';shelter_detail_corrugations='20'}
    foreach($key in $fixed.Keys){if($Value[$key] -cne $fixed[$key]){throw ('Current authored quota differs: '+$key)}}
    if(@($Value.entrance_understory_cluster_counts.Split(',')|Where-Object{$_ -cne '4'}).Count){throw 'Current six understory clusters must have four plants each'}
    [bigint]$parts=0;foreach($i in 0..2){$parts+=[bigint]$Value['settlement_surface_mesh_'+$i+'_triangles']}
    if($parts -ne [bigint]$Value.entrance_road_source_triangles -or
       [bigint]$Value.entrance_road_forest_triangles+[bigint]$Value.entrance_road_road_triangles -ne $parts){throw 'Current terrain partition did not conserve original triangles'}
}
function Assert-MistwatchCurrentSuppression($Value,$Extras,$Native){
    Assert-MistwatchFallenExactKeys $Value @('grass','broadleaf') 'current suppression ID domains'
    foreach($pair in @(@('grass',3600),@('broadleaf',96))){
        $ids=$Value[$pair[0]];[long]$prior=-1
        if($ids -isnot [array] -or $ids.Count -lt 1 -or $ids.Count -gt $pair[1]){throw 'Current suppression array size differs'}
        foreach($id in $ids){if(($id -isnot [int] -and $id -isnot [long]) -or $id -le $prior -or $id -ge $pair[1]){throw 'Current suppression IDs must be canonical sorted unique in range'};$prior=$id}
        if([string]$ids.Count -cne $Extras['entrance_understory_suppressed_'+$pair[0]]){throw 'Current suppression count differs'}
    }
    [bigint]$hash=[bigint]::Parse('14695981039346656037');[bigint]$mask=[bigint]::Parse('18446744073709551615')
    foreach($word in @([uint32]0x47524153,$Value.grass.Count)+@($Value.grass)+@([uint32]0x42524f41,$Value.broadleaf.Count)+@($Value.broadleaf)){
        $hash=(($hash -bxor [bigint]$word)*[bigint]1099511628211)-band $mask
    }
    if($hash.ToString([Globalization.CultureInfo]::InvariantCulture) -cne $Extras.entrance_understory_suppression_fingerprint){throw 'Current exact suppression IDs/fingerprint differ'}
    $retained=@($Extras.forest_cover_retained_render_variants.Split(','));$author=@($Native.forest_cover_variants.Split(','))
    [long]$sum=0;for($i=0;$i -lt 5;$i++){if([long]$retained[$i] -gt [long]$author[$i]){throw 'Current retained grass variant exceeds original population'};$sum+=[long]$retained[$i]}
    if($sum -ne 3600-$Value.grass.Count){throw 'Current retained grass/suppression identity differs'}
    $retained=@($Extras.roadside_broadleaf_retained_variants.Split(','));[long]$sum=0
    foreach($count in $retained){if([long]$count -gt 96){throw 'Current retained broadleaf variant exceeds population'};$sum+=[long]$count}
    if($sum -ne 96-$Value.broadleaf.Count){throw 'Current retained broadleaf/suppression identity differs'}
}
function Assert-MistwatchCurrentWorldExpected($Value){
    Assert-MistwatchFallenExactKeys $Value @('schema','world_cohort','runtime_profile','world_profile','fallen_log_profile','production_full_solid_volumes',
        'source','native_contract','legacy_native_contract','current_extras','surface_contract','art_profiles','suppression_ids',
        'fallen_pose_words','cpu_identity','provenance','scope') 'current world expectation'
    foreach($pair in @(@('schema','mistwatch.current-world.native-expected.1'),@('world_cohort','entrance-current-forest-surface-v1'),
        @('runtime_profile','authored-current-forest-full-v5'))){
        if($Value[$pair[0]] -isnot [string] -or $Value[$pair[0]] -cne $pair[1]){throw ('Current expectation differs: '+$pair[0])}
    }
    Assert-MistwatchCurrentProvenance $Value.provenance
    $legacy=Get-MistwatchCurrentLegacyShape $Value
    Assert-MistwatchAuthoredNativeExpectedContract $legacy
    Assert-MistwatchFallenExactKeys $Value.native_contract (Get-MistwatchAuthoredNativeFieldNames) 'current153 projection'
    $changed=@(Get-MistwatchCurrentProjectionNames)
    foreach($key in $Value.legacy_native_contract.Keys){
        $v=$Value.native_contract[$key]
        if($v -isnot [string] -or $v.Length -lt 1 -or $v.Length -gt 512 -or $v -match '[\x00-\x1F\x7F]'){throw ('Current native canonical string differs: '+$key)}
        if($key -cnotin $changed -and $v -cne $Value.legacy_native_contract[$key]){throw ('Current unreviewed projection differs: '+$key)}
    }
    $fixed=@{shrubs_plants='128';shrubs_submitted='128';planked_workshop_profile='framed-workshop-v1';
        planked_workshop_vertices='12480';planked_workshop_indices='17160';ground_art_surface_path='moss-height/Ground037_SurfaceOrmHeight_Level0.png';
        ground_art_surface_encoding='2';ground_packed_orm='0';ground_habitat_layer_surface_path='leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png';
        ground_habitat_layer_surface_encoding='2';ground_habitat_layer_source='unrecognized'}
    foreach($key in $fixed.Keys){if($Value.native_contract[$key] -cne $fixed[$key]){throw ('Current exact projected value differs: '+$key)}}
    foreach($key in @('shrubs_fingerprint','shrubs_author_instance_triangles','shrubs_render_instance_triangles','planked_workshop_uv_fingerprint','ground_loaded_texture_identity')){
        Assert-MistwatchAuthoredNativeUnsigned $Value.native_contract[$key] $key
        if($Value.native_contract[$key] -ceq '0'){throw ('Current projected identity missing: '+$key)}
    }
    foreach($pair in @(@('shrubs_plants','64'),@('shrubs_submitted','64'),@('canopy_plants','8'),@('canopy_rooted_meshes','8'))){
        if($Value.legacy_native_contract[$pair[0]] -cne $pair[1]){throw 'Current original64 shrubs/eight canopy CPU context differs'}
    }
    Assert-MistwatchForestSurfaceFields $Value.surface_contract
    foreach($key in @('ground_art_surface_path','ground_art_surface_encoding','ground_habitat_layer_surface_path','ground_habitat_layer_surface_encoding','ground_loaded_texture_identity')){
        if($Value.native_contract[$key] -cne $Value.surface_contract[$key]){throw 'Current complete world/surface16 identity differs'}
    }
    $profiles=Get-MistwatchCurrentArtProfiles
    Assert-MistwatchFallenExactKeys $Value.art_profiles @($profiles.Keys) 'current exact10 art profiles'
    foreach($key in $profiles.Keys){if($Value.art_profiles[$key] -isnot [string] -or $Value.art_profiles[$key] -cne $profiles[$key]){throw 'Current art profile differs'}}
    Assert-MistwatchCurrentExtras $Value.current_extras
    Assert-MistwatchCurrentSuppression $Value.suppression_ids $Value.current_extras $Value.native_contract
}
function Assert-MistwatchCurrentWorldReceipt($Value,[bool]$Native){
    $keys=@('schema','world_cohort','runtime_profile','status','authored_world_CPU_passed','current_geometry_CPU_passed',
        'native_expected_author_passed','source_input_unchanged','runtime_input_unchanged','expected_file','proof_file','cpu_binary','scope')
    if($Native){$keys+='world_expected_file'}
    Assert-MistwatchFallenExactKeys $Value $keys 'current CPU receipt'
    $schema=if($Native){'mistwatch.current-world.native-expected-receipt.1'}else{'mistwatch.current-world.cpu-receipt.1'}
    $status=if($Native){'current-native-expectation-CPU-passed'}else{'current-world-route-load-CPU-passed'}
    foreach($pair in @(@('schema',$schema),@('world_cohort','entrance-current-forest-surface-v1'),@('runtime_profile','authored-current-forest-full-v5'),@('status',$status))){
        if($Value[$pair[0]] -isnot [string] -or $Value[$pair[0]] -cne $pair[1]){throw 'Current CPU receipt schema/status/cohort differs'}
    }
    foreach($key in @('authored_world_CPU_passed','current_geometry_CPU_passed','source_input_unchanged','runtime_input_unchanged')){
        Assert-MistwatchAuthoredNativeBoolean $Value[$key] $true $key
    }
    Assert-MistwatchAuthoredNativeBoolean $Value.native_expected_author_passed $Native 'current native CPU author flag'
    $scope=@('native_qualified','actual_Main_route_qualified','art_accepted','full49_qualified','outer_process_qualified')
    Assert-MistwatchFallenExactKeys $Value.scope $scope 'current CPU receipt exact scope'
    foreach($key in $scope){Assert-MistwatchAuthoredNativeBoolean $Value.scope[$key] $false $key}
    $outputs=@(@('expected_file',$(if($Native){'NativeExpected.json'}else{'WorldExpected.json'})),@('proof_file','world-seat.txt'))
    if($Native){$outputs+=,@('world_expected_file','WorldExpected.json')}
    foreach($pair in $outputs){Assert-MistwatchAuthoredNativeIdentity $Value[$pair[0]] $pair[0];if($Value[$pair[0]].path -cne $pair[1]){throw 'Current fixed output filename differs'}}
    Assert-MistwatchAuthoredNativeIdentity $Value.cpu_binary 'current receipt CPU binary'
    if(-not [IO.Path]::IsPathFullyQualified($Value.cpu_binary.path)){throw 'Current receipt CPU binary must be absolute'}
}
function Assert-MistwatchCurrentWorldCpuExpected($World,$Expected){
    Assert-MistwatchCurrentWorldExpected $Expected
    Assert-MistwatchFallenExactKeys $World @('schema','world_cohort','runtime_profile','world','vegetation','provenance','scope') 'current complete CPU world'
    foreach($pair in @(@('schema','mistwatch.current-world.cpu-expected.1'),@('world_cohort','entrance-current-forest-surface-v1'),@('runtime_profile','authored-current-forest-full-v5'))){
        if($World[$pair[0]] -isnot [string] -or $World[$pair[0]] -cne $pair[1]){throw 'Current WorldExpected schema/cohort/profile differs'}
    }
    $scope=@('authored_world_CPU_passed','current_geometry_CPU_passed','native_expected_author_passed','native_executed','GPU_qualified','art_accepted','manual_play','published')
    Assert-MistwatchFallenExactKeys $World.scope $scope 'current WorldExpected scope'
    foreach($key in $scope){Assert-MistwatchAuthoredNativeBoolean $World.scope[$key] ($key -cin @('authored_world_CPU_passed','current_geometry_CPU_passed')) $key}
    Assert-MistwatchCurrentProvenance $World.provenance
    Assert-MistwatchAuthoredNativeSameTree $Expected.provenance $World.provenance 'current world/native sealed provenance'
    Assert-MistwatchAuthoredWorldNumber $World.vegetation.shrubs '128' 'current shrubs'
    Assert-MistwatchAuthoredWorldNumber $World.vegetation.legacy_shrubs '64' 'legacy shrubs'
    # 純結構交叉核對重用原 world/vegetation 逐數值規則，從未用此暫存物件讀取舊部署。
    $shape=Copy-MistwatchCurrentObject $World
    $shape.schema='mistwatch.authored-world.cpu-expected.1';$shape.world_cohort='entrance-authored-habitat-v1';$shape.runtime_profile='authored-leaf-cluster-full-v3'
    $shape.provenance=(Get-MistwatchCurrentLegacyShape $Expected).provenance
    $shape.scope=Copy-MistwatchCurrentObject $World.scope;$shape.scope.Remove('current_geometry_CPU_passed')
    $shape.vegetation=Copy-MistwatchCurrentObject $World.vegetation;$shape.vegetation.Remove('legacy_shrubs');$shape.vegetation.shrubs=$World.vegetation.legacy_shrubs
    Assert-MistwatchAuthoredWorldExpectedContract $shape (Get-MistwatchCurrentLegacyShape $Expected)
}
