#requires -Version 7.0
# 新作者世界獨立契約；只讀 CPU 證書與實際捕捉欄位，不產生預期、不啟動遊戲。
. (Join-Path $PSScriptRoot 'FallenLogContract.ps1')

function Get-MistwatchAuthoredNativeFieldNames {
    # 固定153欄來自新版 CPU recipe，不能從捕捉結果自動擴充。
    return @((@'
canopy_collision_mode canopy_collision_parts canopy_collision_spacing_m canopy_enabled canopy_ground_checks canopy_materials canopy_plants canopy_population_fingerprint canopy_primitive_triangles canopy_rooted_meshes canopy_submitted_slots canopy_wood_enclosed_grid_cells canopy_wood_source_triangles canopy_wood_surface_cells canopy_world_profile entrance_author_applied entrance_author_fern_relocations entrance_author_fingerprint entrance_author_fir_target entrance_author_middle_target entrance_author_source_fir_fingerprint entrance_author_source_middle_fingerprint entrance_author_withdrawals entrance_canopy_appended entrance_canopy_exact_overlay_faces entrance_canopy_full_face_checks entrance_canopy_layout entrance_canopy_new_wood_parts entrance_canopy_original_fingerprint fern_colonies fern_colony_authored_population fern_colony_fingerprint fern_colony_original_fingerprint fern_colony_population fern_colony_reached_budget fern_colony_seed fern_colony_variants fir_layout fir_layout_fingerprint fir_layout_patch_centres fir_layout_population fir_layout_reached_budget forest_cover_authored_population forest_cover_clearance_collider_fingerprint forest_cover_clearance_colliders forest_cover_filled forest_cover_final_world_fingerprint forest_cover_fingerprint forest_cover_independent_anchors forest_cover_maximum_patches forest_cover_mode forest_cover_original_fingerprint forest_cover_patch_maximum_plants forest_cover_patch_minimum_plants forest_cover_patches forest_cover_provenance_fingerprint forest_cover_reached_budget forest_cover_rejected_original forest_cover_retained forest_cover_retention_revision forest_cover_seed forest_cover_source_world_fingerprint forest_cover_variants ground_art_base_color_path ground_art_normal_path ground_art_surface_encoding ground_art_surface_path ground_cover_instances ground_habitat_canopy_roots ground_habitat_fir_roots ground_habitat_landscape_changed ground_habitat_landscape_fingerprint ground_habitat_layer_base_path ground_habitat_layer_normal_path ground_habitat_layer_source ground_habitat_layer_surface_encoding ground_habitat_layer_surface_path ground_habitat_revision ground_habitat_terrain_changed ground_habitat_terrain_fingerprint ground_habitat_terrain_moss_dominant ground_habitat_terrain_soil_dominant ground_height_alpha ground_loaded_texture_identity ground_material_class ground_maximum_resolution ground_normal_scale ground_packed_orm ground_source ground_texture_layers ground_texture_scale ground_use_uv middle_layer_attempted middle_layer_comparison middle_layer_population middle_layer_reached_budget middle_layer_target middle_layer_transform_fingerprint planked_shelter_enabled planked_shelter_indices planked_shelter_roof_uv_aligned planked_shelter_uv_aligned planked_shelter_uv_fingerprint planked_shelter_vertices planked_shelter_wood_profile planked_workshop_enabled planked_workshop_indices planked_workshop_profile planked_workshop_uv_fingerprint planked_workshop_vertices shrubs_author_instance_triangles shrubs_author_leaf_count shrubs_enabled shrubs_fingerprint shrubs_plants shrubs_render_instance_triangles shrubs_render_leaf_counts shrubs_submitted shrubs_uploaded_leaf_indices shrubs_uploaded_meshes shrubs_uploaded_stem_indices submitted_middle_layer submitted_middle_layer_triangles terrain_triangles terrain_vertices trail_litter_branches trail_litter_enabled trail_litter_fingerprint trail_litter_patches trail_litter_stones trail_litter_triangles understory_ferns understory_saplings world_profile
'@).Trim() -split ' ') + @('grass_shape_profile','understory_mix_profile','ground_distance_normal_strength','ground_meso_partition') +
        @(foreach($variant in 0..4){foreach($suffix in @('identity','vertices','triangles')){'grass_render_mesh_'+$variant+'_'+$suffix}})
}
function Assert-MistwatchAuthoredNativeCanonicalMap($Value) {
    Assert-MistwatchFallenExactKeys $Value (Get-MistwatchAuthoredNativeFieldNames) 'authored native153 canonical projection'
    $tokens=@{world_profile='forest-wood';canopy_collision_mode='wood-grid-conservative';fir_layout='forest-wood-mature';
        forest_cover_mode='fixed_cluster_patches';forest_cover_retention_revision='original-safe-pose-v1';
        entrance_canopy_layout='entrance-authored-world-v1';ground_habitat_revision='root-soil-and-mission-spurs-v1';
        ground_source='ground037-moss-floor';ground_habitat_layer_source='forest-leaves-04-habitat';ground_height_alpha='shared_core_normalized';
        planked_shelter_wood_profile='weathered-bands-v1';planked_workshop_profile='planked-weathered-bands-v1';
        grass_shape_profile='source-geometry';understory_mix_profile='original'}
    $csv=@{forest_cover_variants=5;fern_colony_variants=4;shrubs_render_leaf_counts=4;shrubs_uploaded_leaf_indices=4;shrubs_uploaded_stem_indices=4}
    $paths=@('ground_art_base_color_path','ground_art_normal_path','ground_art_surface_path',
        'ground_habitat_layer_base_path','ground_habitat_layer_normal_path','ground_habitat_layer_surface_path')
    foreach($key in $Value.Keys){
        $v=$Value[$key]
        if($v -isnot [string] -or $v.Length -lt 1 -or $v.Length -gt 512 -or $v -match '[\x00-\x1F\x7F]'){throw ('Authored canonical field string required: '+$key)}
        if($tokens.ContainsKey($key)){if($v -cne $tokens[$key]){throw ('Authored native field profile differs: '+$key)}}
        elseif($csv.ContainsKey($key)){Assert-MistwatchAuthoredNativeCsv $v $csv[$key] $key}
        elseif($key -cin $paths){Assert-MistwatchAuthoredNativeRelative $v $key}
        elseif($key -cin @('ground_normal_scale','ground_texture_scale','canopy_collision_spacing_m','ground_distance_normal_strength')){
            [double]$number=0
            if($v -cnotmatch '^(0|-?[1-9][0-9]*)(\.[0-9]+)?$' -or
               -not [double]::TryParse($v,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$number) -or
               [double]::IsNaN($number) -or [double]::IsInfinity($number)){throw ('Authored canonical scalar differs: '+$key)}
        }else{
            Assert-MistwatchAuthoredNativeUnsigned $v $key
            if($key -cin @('canopy_world_profile','canopy_enabled','entrance_author_applied','fern_colony_reached_budget',
                'fir_layout_reached_budget','forest_cover_reached_budget','ground_packed_orm','ground_use_uv','ground_meso_partition',
                'middle_layer_comparison','middle_layer_reached_budget','planked_shelter_enabled','planked_shelter_uv_aligned',
                'planked_shelter_roof_uv_aligned','planked_workshop_enabled','shrubs_enabled','trail_litter_enabled') -and $v -cnotin @('0','1')){
                throw ('Authored boolean word differs: '+$key)
            }
        }
    }
}

function Assert-MistwatchAuthoredNativeUnsigned($Value,[string]$Label) {
    [ulong]$parsed=0
    if($Value -isnot [string] -or $Value -cnotmatch '^(0|[1-9][0-9]{0,19})$' -or
        -not [ulong]::TryParse($Value,[Globalization.NumberStyles]::None,[Globalization.CultureInfo]::InvariantCulture,[ref]$parsed)) {
        throw ('Authored canonical uint64 required: '+$Label)
    }
}
function Assert-MistwatchAuthoredNativeCsv($Value,[int]$Count,[string]$Label) {
    if($Value -isnot [string]){throw ('Authored CSV string required: '+$Label)}
    $parts=@($Value.Split(','))
    if($parts.Count -ne $Count){throw ('Authored CSV length differs: '+$Label)}
    foreach($part in $parts){Assert-MistwatchAuthoredNativeUnsigned $part $Label}
}
function Assert-MistwatchAuthoredNativeBoolean($Value,[bool]$Expected,[string]$Label) {
    if($Value -isnot [bool] -or $Value -ne $Expected){throw ('Authored exact boolean differs: '+$Label)}
}
function Get-MistwatchAuthoredNativeProfileSpec($Value) {
    if($Value -is [string]) {
        switch -CaseSensitive -Exact ($Value) {
            'habitat-full-v1' { return @{mask=2;count=402} }
            'authored-nettle-full-v1' { return @{mask=4;count=412} }
            'authored-mistwatch-only-v1' { return @{mask=8;count=154} }
            'authored-boulder-full-v2' { return @{mask=16;count=414} }
            'authored-boulder-mistwatch-only-v2' { return @{mask=32;count=156} }
            'authored-leaf-cluster-full-v3' { return @{mask=16;count=416;addition='leaf-cluster-v3'} }
            'authored-forest-surface-full-v4' { return @{mask=16;count=418;addition='forest-surface-v4'} }
            'authored-current-forest-full-v5' { return @{mask=16;count=418;addition='current-world-v5'} }
        }
    }
    throw 'Authored reviewed nonlegacy runtime profile required'
}
function Assert-MistwatchLeafClusterManifestBytes([byte[]]$Bytes) {
    # 固定來源審查的 sidecar 身分；不從待驗 runtime 自填任何預期。
    $sha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes))
    if($sha -cne 'DE5E956E5191F620301D8088471C5FCF4BCADF3071BA51AA808EA6832C49AC4A') {
        throw 'Authored leaf-cluster compiled manifest differs'
    }
}
function Get-MistwatchLeafClusterRuntimeAdditions([string]$SourceRoot) {
    $path=Assert-MistwatchFallenPhysicalPath (Join-Path $SourceRoot 'games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json')
    Assert-MistwatchLeafClusterManifestBytes ([IO.File]::ReadAllBytes($path))
    $manifest=Read-MistwatchFallenStrictJson $path
    return ,@($manifest.additions)
}
function Assert-MistwatchForestSurfaceManifestBytes([byte[]]$Bytes) {
    $sha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes))
    if($sha -cne '467BE56D0F3C5F1408E014DB50728DD1BCC9D28AB0366CE6B74957C74505F291'){
        throw 'Forest surface reviewed418 manifest differs'
    }
}
function Get-MistwatchForestSurfaceRuntimeAdditions([string]$SourceRoot) {
    $path=Assert-MistwatchFallenPhysicalPath (Join-Path $SourceRoot 'games/mistwatch/native/tests/qualification/runtime-forest-surface-full-v4.json')
    Assert-MistwatchForestSurfaceManifestBytes ([IO.File]::ReadAllBytes($path))
    $manifest=Read-MistwatchFallenStrictJson $path
    return ,@($manifest.additions)
}
function Assert-MistwatchCurrentForestManifestBytes([byte[]]$Bytes){
    if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)) -cne
        '8A2B65C6F27D9E7AC7CED21E69327D41514A4D72B22773B582D7D9CD6A57816B'){
        throw 'Current forest explicit418 manifest differs'
    }
}
function Assert-MistwatchAuthoredNativeProfile($Value) {
    $null=Get-MistwatchAuthoredNativeProfileSpec $Value
}
function Assert-MistwatchAuthoredNativeIdentity($Row,[string]$Label) {
    Assert-MistwatchFallenExactKeys $Row @('path','bytes','sha256') $Label
    if($Row.path -isnot [string] -or [string]::IsNullOrWhiteSpace($Row.path) -or $Row.path.Length -gt 32767 -or
        ($Row.bytes -isnot [long] -and $Row.bytes -isnot [int])){throw ('Authored raw identity type differs: '+$Label)}
    if($Row.bytes -lt 0 -or $Row.bytes -gt 2147483648){throw ('Authored raw identity size differs: '+$Label)}
    if($Row.sha256 -isnot [string]){throw ('Authored raw SHA string required: '+$Label)}
    Assert-MistwatchFallenSha $Row.sha256 $Label
}
function Assert-MistwatchAuthoredNativeRelative([string]$Path,[string]$Label) {
    if([string]::IsNullOrWhiteSpace($Path) -or [IO.Path]::IsPathRooted($Path) -or $Path.Contains('\') -or
       $Path.Contains(':') -or $Path.Contains([char]0) -or $Path.Split('/') -contains '.' -or
       $Path.Split('/') -contains '..' -or $Path.Split('/') -contains '' -or $Path.Contains("`r") -or $Path.Contains("`n")) {
        throw ('Authored unsafe relative path: '+$Label)
    }
}
function Assert-MistwatchAuthoredNativeRowsShape($Rows,[string]$Label) {
    if($Rows -isnot [array] -or $Rows.Count -lt 1 -or $Rows.Count -gt 4096){throw ('Authored row array required: '+$Label)}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    [long]$bytes=0
    foreach($row in $Rows){
        Assert-MistwatchAuthoredNativeIdentity $row $Label
        Assert-MistwatchAuthoredNativeRelative $row.path $Label
        if(-not $seen.Add($row.path)){throw ('Authored duplicate/shadow file row: '+$Label)}
        $bytes+=$row.bytes
    }
    if($bytes -gt 2147483648){throw ('Authored row byte budget exceeded: '+$Label)}
}
function Assert-MistwatchAuthoredNativeProvenanceShape($Value,[string]$Profile) {
    $keys=@('schema','world_cohort','authored_world_qualified','runtime_profile','generated_root_stem_override',
        'source_root','source_files','runtime_assets_root','runtime_assets','fallen_source_assets','raw_mesh','test_source',
        'cpu_binary','toolchain','source_input_unchanged','runtime_input_unchanged','outer_receipt_scope')
    Assert-MistwatchFallenExactKeys $Value $keys 'authored CPU provenance'
    if($Value.schema -isnot [string] -or $Value.schema -cne 'mistwatch.authored-world.cpu-provenance.1' -or
       $Value.world_cohort -isnot [string] -or $Value.world_cohort -cne 'entrance-authored-habitat-v1' -or
       $Value.runtime_profile -isnot [string] -or $Value.runtime_profile -cne $Profile){throw 'Authored provenance cohort/schema differs'}
    Assert-MistwatchAuthoredNativeBoolean $Value.authored_world_qualified $false 'provenance native qualification'
    foreach($key in @('source_input_unchanged','runtime_input_unchanged')){Assert-MistwatchAuthoredNativeBoolean $Value[$key] $true $key}
    foreach($key in @('source_root','runtime_assets_root')){
        if($Value[$key] -isnot [string] -or -not [IO.Path]::IsPathFullyQualified($Value[$key])){throw ('Authored absolute root required: '+$key)}
    }
    if($Value.generated_root_stem_override -isnot [string] -or
       ($Value.generated_root_stem_override -and -not [IO.Path]::IsPathFullyQualified($Value.generated_root_stem_override))){throw 'Authored generated root directory differs'}
    if($Value.outer_receipt_scope -isnot [string] -or $Value.outer_receipt_scope -cne
       'Before/after selected source and runtime raw identities only. Root independently binds fresh compile/link inputs, exits, Job drain and JUnit; no self-claimed outer process or GPU qualification.') {
        throw 'Authored provenance observation scope differs'
    }
    foreach($key in @('source_files','runtime_assets','fallen_source_assets')){Assert-MistwatchAuthoredNativeRowsShape $Value[$key] $key}
    foreach($key in @('raw_mesh','test_source')){Assert-MistwatchAuthoredNativeIdentity $Value[$key] $key;Assert-MistwatchAuthoredNativeRelative $Value[$key].path $key}
    Assert-MistwatchAuthoredNativeIdentity $Value.cpu_binary 'CPU binary'
    if(-not [IO.Path]::IsPathFullyQualified($Value.cpu_binary.path)){throw 'Authored absolute CPU binary required'}
    Assert-MistwatchFallenToolchain $Value.toolchain
}

function Assert-MistwatchAuthoredNativeReceiptContract($Value,[string]$Profile) {
    Assert-MistwatchFallenExactKeys $Value @('schema','world_cohort','runtime_profile','status','native_expected_author_passed',
        'source_input_unchanged','runtime_input_unchanged','expected_file','world_expected_file','proof_file','cpu_binary','scope') 'authored native expectation receipt'
    if($Value.schema -isnot [string] -or $Value.schema -cne 'mistwatch.authored-world.native-expected-receipt.1' -or
       $Value.world_cohort -isnot [string] -or $Value.world_cohort -cne 'entrance-authored-habitat-v1' -or
       $Value.status -isnot [string] -or $Value.status -cne 'authored-native-expectation-CPU-passed') {
        throw 'Authored native expectation receipt schema/status/cohort differs'
    }
    Assert-MistwatchAuthoredNativeProfile $Value.runtime_profile
    if($Profile -and $Value.runtime_profile -cne $Profile){throw 'Authored receipt/expected runtime profile differs'}
    foreach($key in @('native_expected_author_passed','source_input_unchanged','runtime_input_unchanged')){
        Assert-MistwatchAuthoredNativeBoolean $Value[$key] $true $key
    }
    $scope=@('native_qualified','actual_Main_route_qualified','art_accepted','full49_qualified','outer_process_qualified')
    Assert-MistwatchFallenExactKeys $Value.scope $scope 'authored receipt scope'
    foreach($key in $scope){Assert-MistwatchAuthoredNativeBoolean $Value.scope[$key] $false $key}
    foreach($pair in @(@('expected_file','NativeExpected.json'),@('world_expected_file','WorldExpected.json'),@('proof_file','world-seat.txt'))){
        Assert-MistwatchAuthoredNativeIdentity $Value[$pair[0]] $pair[0]
        if($Value[$pair[0]].path -cne $pair[1]){throw ('Authored certificate fixed output differs: '+$pair[0])}
    }
    Assert-MistwatchAuthoredNativeIdentity $Value.cpu_binary 'receipt CPU binary'
    if(-not [IO.Path]::IsPathFullyQualified($Value.cpu_binary.path)){throw 'Authored receipt absolute CPU binary required'}
}

function Assert-MistwatchAuthoredNativeExpectedContract($Value) {
    Assert-MistwatchFallenExactKeys $Value @('schema','world_cohort','runtime_profile','world_profile','fallen_log_profile',
        'production_full_solid_volumes','source','native_contract','fallen_pose_words','cpu_identity','provenance','scope') 'authored native expectation'
    foreach($pair in @(@('schema','mistwatch.authored-world.native-expected.1'),@('world_cohort','entrance-authored-habitat-v1'),
        @('world_profile','forest-wood'),@('fallen_log_profile','scanned-straight-v1'))){
        if($Value[$pair[0]] -isnot [string] -or $Value[$pair[0]] -cne $pair[1]){throw ('Authored expectation identity differs: '+$pair[0])}
    }
    Assert-MistwatchAuthoredNativeProfile $Value.runtime_profile
    # 新418部署不能借用舊153欄的世界資格；它有獨立地表證書與明確未通過的幾何範圍。
    if($Value.runtime_profile -cin @('authored-forest-surface-full-v4','authored-current-forest-full-v5')){
        throw 'Forest surface418 requires its surface-only certificate; legacy153 cannot qualify current geometry'
    }
    Assert-MistwatchAuthoredNativeBoolean $Value.production_full_solid_volumes $true 'production full volumes'
    $scope=@('authored_world_CPU_passed','native_expected_author_passed','native_executed','GPU_qualified','art_accepted','manual_play','published')
    Assert-MistwatchFallenExactKeys $Value.scope $scope 'authored expected scope'
    foreach($key in $scope){Assert-MistwatchAuthoredNativeBoolean $Value.scope[$key] ($key -cin @('authored_world_CPU_passed','native_expected_author_passed')) $key}
    Assert-MistwatchFallenExactKeys $Value.source @('sha256','bytes','vertices','indices','triangles') 'authored raw fallen source'
    $pins=@{sha256='b7f6a05bf928787082dea92a0051557496e45894a9c2b1a0af1e51110abd0d46';bytes='434012';vertices='8956';indices='45807';triangles='15269'}
    foreach($key in $pins.Keys){if($Value.source[$key] -isnot [string] -or $Value.source[$key] -cne $pins[$key]){throw ('Authored raw fallen pin differs: '+$key)}}
    Assert-MistwatchFallenExactKeys $Value.fallen_pose_words @('prop_0','prop_8','prop_34','prop_35') 'authored fallen source-ID words'
    $poses=Get-MistwatchFallenAuthoredPoseWords
    foreach($key in $poses.Keys){if($Value.fallen_pose_words[$key] -isnot [string] -or $Value.fallen_pose_words[$key] -cne $poses[$key]){throw ('Authored original fallen f32 words differ: '+$key)}}
    $cpu=$Value.cpu_identity
    Assert-MistwatchFallenExactKeys $cpu @('rich_cover_word_fnv','terrain_fingerprint','original_prop_fingerprint','source_original_nonlog_count',
        'retained_nonlog_count','initial_world','repaired_world','grass_source_vertices','grass_source_triangles','fern_source_triangles','fixed_scene_original_ferns') 'authored CPU identity'
    foreach($key in @('rich_cover_word_fnv','terrain_fingerprint','original_prop_fingerprint','source_original_nonlog_count','retained_nonlog_count')){
        Assert-MistwatchAuthoredNativeUnsigned $cpu[$key] $key
    }
    if($cpu.source_original_nonlog_count -cne '32' -or $cpu.retained_nonlog_count -cne '29'){throw 'Authored original/retained nonlog quota differs'}
    foreach($pair in @(@('initial_world','8853'),@('repaired_world','8854'))){
        $world=$cpu[$pair[0]]
        Assert-MistwatchFallenExactKeys $world @('count','bytewise_fnv','typed_fingerprint') $pair[0]
        foreach($key in @('count','bytewise_fnv','typed_fingerprint')){Assert-MistwatchAuthoredNativeUnsigned $world[$key] ($pair[0]+'.'+$key)}
        if($world.count -cne $pair[1]){throw ('Authored full world quota differs: '+$pair[0])}
    }
    Assert-MistwatchAuthoredNativeCsv $cpu.grass_source_vertices 5 'grass vertex variants'
    Assert-MistwatchAuthoredNativeCsv $cpu.grass_source_triangles 5 'grass triangle variants'
    Assert-MistwatchAuthoredNativeCsv $cpu.fern_source_triangles 4 'fern triangle variants'
    Assert-MistwatchFallenExactKeys $cpu.fixed_scene_original_ferns @('initial','river-overlook') 'authored fixed-scene fern'
    foreach($scene in @('initial','river-overlook')){
        $fern=$cpu.fixed_scene_original_ferns[$scene]
        Assert-MistwatchFallenExactKeys $fern @('plants','triangles','variants') $scene
        Assert-MistwatchAuthoredNativeUnsigned $fern.plants $scene
        Assert-MistwatchAuthoredNativeUnsigned $fern.triangles $scene
        Assert-MistwatchAuthoredNativeCsv $fern.variants 4 $scene
        if([ulong]$fern.plants -gt 420){throw 'Authored original fern exceeds420'}
        [ulong]$plants=0;[ulong]$triangles=0;$counts=$fern.variants.Split(',');$mesh=$cpu.fern_source_triangles.Split(',')
        for($i=0;$i -lt 4;$i++){
            if([ulong]$counts[$i] -gt 105 -or [ulong]$mesh[$i] -gt 600000){throw 'Authored fern variant domain differs'}
            $plants+=[ulong]$counts[$i];$triangles+=[ulong]$counts[$i]*[ulong]$mesh[$i]
        }
        if($plants -ne [ulong]$fern.plants -or $triangles -ne [ulong]$fern.triangles){throw 'Authored fixed-scene fern accounting differs'}
    }
    Assert-MistwatchAuthoredNativeProvenanceShape $Value.provenance $Value.runtime_profile
    Assert-MistwatchAuthoredNativeCanonicalMap $Value.native_contract
    if($Value.native_contract.forest_cover_clearance_colliders -cne $cpu.initial_world.count -or
       $Value.native_contract.forest_cover_clearance_collider_fingerprint -cne $cpu.initial_world.bytewise_fnv){throw 'Authored CPU/native world identity differs'}
}

function Assert-MistwatchAuthoredNativeFields {
    param([Parameter(Mandatory=$true)]$Receipt,[Parameter(Mandatory=$true)]$Expected)
    Assert-MistwatchAuthoredNativeExpectedContract $Expected
    Assert-MistwatchAuthoredNativeCanonicalMap $Receipt
    foreach($key in $Expected.native_contract.Keys){
        if($Receipt[$key] -cne $Expected.native_contract[$key]){throw ('Authored actual native field differs: '+$key)}
    }
}

function Assert-MistwatchAuthoredNativeSameTree($A,$B,[string]$Label) {
    # 不依 JSON 屬性順序判定；數字、字串、布林不得互相轉型。
    if($A -is [Collections.IDictionary]){
        Assert-MistwatchFallenExactKeys $B @($A.Keys) $Label
        foreach($key in $A.Keys){Assert-MistwatchAuthoredNativeSameTree $A[$key] $B[$key] ($Label+'.'+$key)}
    }elseif($A -is [array]){
        if($B -isnot [array] -or $A.Count -ne $B.Count){throw ('Authored sealed array differs: '+$Label)}
        for($i=0;$i -lt $A.Count;$i++){Assert-MistwatchAuthoredNativeSameTree $A[$i] $B[$i] ($Label+'['+$i+']')}
    }else{
        if($null -eq $A -or $null -eq $B -or $A.GetType() -ne $B.GetType() -or $A -cne $B){throw ('Authored sealed value/type differs: '+$Label)}
    }
}
function Assert-MistwatchAuthoredWorldNumber($Value,[string]$Expected,[string]$Label) {
    if($Value -isnot [int] -and $Value -isnot [long] -and $Value -isnot [bigint]){throw ('Authored WorldExpected integer required: '+$Label)}
    $canonical=$Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    Assert-MistwatchAuthoredNativeUnsigned $canonical $Label
    if($canonical -cne $Expected){throw ('Authored WorldExpected/native identity differs: '+$Label)}
}
function Assert-MistwatchAuthoredWorldExpectedContract($World,$Expected) {
    Assert-MistwatchAuthoredNativeExpectedContract $Expected
    Assert-MistwatchFallenExactKeys $World @('schema','world_cohort','runtime_profile','world','vegetation','provenance','scope') 'authored world certificate'
    foreach($pair in @(@('schema','mistwatch.authored-world.cpu-expected.1'),@('world_cohort',$Expected.world_cohort),@('runtime_profile',$Expected.runtime_profile))){
        if($World[$pair[0]] -isnot [string] -or $World[$pair[0]] -cne $pair[1]){throw 'Authored WorldExpected cannot cross cohort/profile'}
    }
    $scope=@('authored_world_CPU_passed','native_expected_author_passed','native_executed','GPU_qualified','art_accepted','manual_play','published')
    Assert-MistwatchFallenExactKeys $World.scope $scope 'authored world scope'
    foreach($key in $scope){Assert-MistwatchAuthoredNativeBoolean $World.scope[$key] ($key -ceq 'authored_world_CPU_passed') $key}
    Assert-MistwatchAuthoredNativeProvenanceShape $World.provenance $Expected.runtime_profile
    Assert-MistwatchAuthoredNativeSameTree $Expected.provenance $World.provenance 'world/native sealed provenance'
    $cpu=$Expected.cpu_identity;$n=$Expected.native_contract
    $worldValues=@{before_solids=$cpu.initial_world.count;after_solids=$cpu.repaired_world.count;
        canopy=$n.canopy_plants;wood=$n.canopy_collision_parts;before_fingerprint=$cpu.initial_world.bytewise_fnv;
        after_fingerprint=$cpu.repaired_world.bytewise_fnv;author_fingerprint=$n.entrance_author_fingerprint;
        source_fir_fingerprint=$n.entrance_author_source_fir_fingerprint;source_middle_fingerprint=$n.entrance_author_source_middle_fingerprint}
    Assert-MistwatchFallenExactKeys $World.world @($worldValues.Keys) 'authored full world values'
    foreach($key in $worldValues.Keys){Assert-MistwatchAuthoredWorldNumber $World.world[$key] $worldValues[$key] $key}
    $vegetationValues=@{firs=$n.fir_layout_population;middle=$n.middle_layer_population;saplings=$n.understory_saplings;
        props='33';original_ferns='420';cover=$n.forest_cover_authored_population;retained=$n.forest_cover_retained;
        rejected=$n.forest_cover_rejected_original;filled=$n.forest_cover_filled;cover_fingerprint=$n.forest_cover_fingerprint;
        provenance_fingerprint=$n.forest_cover_provenance_fingerprint;fern_colonies=$n.fern_colony_authored_population;
        shrubs=$n.shrubs_plants;litter_patches=$n.trail_litter_patches}
    Assert-MistwatchFallenExactKeys $World.vegetation @($vegetationValues.Keys) 'authored retained vegetation values'
    foreach($key in $vegetationValues.Keys){Assert-MistwatchAuthoredWorldNumber $World.vegetation[$key] $vegetationValues[$key] $key}
}

function Get-MistwatchAuthoredNativeRuntimeRows([string]$SourceRoot,[string]$AssetsRoot,[string]$Profile,[string]$GeneratedRoot) {
    Assert-MistwatchAuthoredNativeProfile $Profile
    $manifestPath=Join-Path $SourceRoot 'games/mistwatch/native/tests/qualification/runtime-profiles-v1.json'
    Assert-MistwatchFallenHash $manifestPath '1A0CC543A038BE1B98A5F32E27D9A37C11D53921536743B928FA36F712A8078A'
    # 與已版本化 runtime profile 相同的八個固定 producer 輸入；不是僅相信自述雜湊。
    $producerPins=@(
        @('games/mistwatch/tools/author_root_stem.py',9767,'BB9C019629A9CD1A66614DD1696EA1AA927741FFB31A4AD46FDCDFA7360AF176'),
        @('engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh',159612,'B964D1F7B16FB3602FF038AE111C79D0BFC029185896E95DA97CC1ACB4F59145'),
        @('games/mistwatch/native/tools/export_boar.cpp',638,'9397681D09D61448126D55326FC52179A7C18CD38D0DE5628991FA35CFA1DA1A'),
        @('games/mistwatch/native/include/MistwatchBoar.h',10869,'CF2C617A135447EB5A342532283AF7A35D3C402B805633BEEAEB38003BECCBD4'),
        @('engine/native/src/SkinnedModelAsset.cpp',39060,'9656F5CAE752C586F40D1F764D4409F80F2E17E1300A0CD69356422230102B51'),
        @('engine/native/include/Aevrynd/SkinnedModelAsset.h',2818,'9B07F44670F72CA4BA554E82A2C2066599CE4F70451F858680FB1B3C6B9824BA'),
        @('engine/native/include/Aevrynd/SkinnedModel3D.h',2882,'BA44F124955E8D7AF68B42C43A43497E7BD6C97E3E2DDBCB506ED5599C85C2C3'),
        @('engine/native/include/Aevrynd/Math.h',6625,'00E44D164ADBBA33D0E41D3019E3391905BE26775D096E74AF2B74BCD81AFDF5'))
    foreach($pin in $producerPins){Assert-MistwatchFallenRow @{path=(Join-Path $SourceRoot $pin[0]);bytes=$pin[1];sha256=$pin[2]}}
    $manifest=Read-MistwatchFallenStrictJson $manifestPath
    $profileSpec=Get-MistwatchAuthoredNativeProfileSpec $Profile
    $mask=$profileSpec.mask
    $count=$profileSpec.count
    $wanted=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    foreach($entry in $manifest.entries){if(([int]$entry[2] -band $mask) -ne 0){$wanted.Add([string]$entry[0],$entry)}}
    if($Profile -cin @('authored-leaf-cluster-full-v3','authored-forest-surface-full-v4','authored-current-forest-full-v5')) {
        if($wanted.Count -ne 414){throw 'Authored leaf parent membership differs'}
        $additions=Get-MistwatchLeafClusterRuntimeAdditions $SourceRoot
        foreach($entry in $additions){$wanted.Add([string]$entry[0],$entry)}
    }
    if($Profile -cin @('authored-forest-surface-full-v4','authored-current-forest-full-v5')) {
        if($wanted.Count -ne 416){throw 'Forest surface exact416 parent membership differs'}
        $additions=Get-MistwatchForestSurfaceRuntimeAdditions $SourceRoot
        foreach($entry in $additions){$wanted.Add([string]$entry[0],$entry)}
    }
    if($Profile -ceq 'authored-current-forest-full-v5'){
        $current=Assert-MistwatchFallenPhysicalPath (Join-Path $SourceRoot 'games/mistwatch/native/tests/qualification/runtime-current-forest-full-v5.json')
        Assert-MistwatchCurrentForestManifestBytes ([IO.File]::ReadAllBytes($current))
    }
    if($wanted.Count -ne $count){throw 'Authored reviewed runtime manifest quota differs'}
    $assets=Assert-MistwatchFallenPhysicalPath $AssetsRoot -Directory
    $generated=Assert-MistwatchFallenPhysicalPath $GeneratedRoot -Directory
    $rootMesh=Join-Path $generated 'island-tree-01-lod2-trunk-root-stem.aemesh'
    Assert-MistwatchFallenRow @{path=$rootMesh;bytes=165000;sha256='96EA5581E6A5D2A0C3260DEEBE614A4FA9BCCA0A77772E5EAE42E0C88730BC7C'}
    $rootReceiptPath=Assert-MistwatchFallenPhysicalPath (Join-Path $generated 'author-receipt.json')
    $null=Read-MistwatchFallenStrictJson $rootReceiptPath
    $rootRaw=[IO.File]::ReadAllText($rootReceiptPath,[Text.Encoding]::UTF8)
    $normalized=$rootRaw
    $rootSource=Join-Path $SourceRoot 'engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh'
    foreach($pair in @(@('source',$rootSource),@('output',$rootMesh))){
        $prefix='"'+$pair[0]+'": "'
        $start=$normalized.IndexOf($prefix,[StringComparison]::Ordinal)
        if($start -lt 0 -or $normalized.IndexOf($prefix,$start+$prefix.Length,[StringComparison]::Ordinal) -ge 0){throw 'Authored root metadata path field differs'}
        $escaped=([IO.Path]::GetFullPath($pair[1])|ConvertTo-Json -Compress)
        $escaped=$escaped.Substring(1,$escaped.Length-2)
        $offset=$start+$prefix.Length
        if($offset+$escaped.Length -ge $normalized.Length -or
           $normalized.Substring($offset,$escaped.Length) -cne $escaped -or $normalized[$offset+$escaped.Length] -ne '"'){
            throw 'Authored root metadata not current source/output'
        }
        $normalized=$normalized.Remove($offset,$escaped.Length).Insert($offset,('@'+$pair[0].ToUpperInvariant()+'@'))
    }
    $canonical=[Text.Encoding]::UTF8.GetBytes($normalized)
    $sha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($canonical))
    if($canonical.Length -ne 250893 -or $sha -cne '104607F9FA1E815A37583CCB8812A3982138B4F0F86230CDA91C494D50C649D6'){
        throw 'Authored complete root metadata differs'
    }
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $rows=@(foreach($file in Get-ChildItem -LiteralPath $assets -Recurse -Force){
        if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Authored runtime reparse rejected'}
        if($file -is [IO.DirectoryInfo]){continue}
        if($file -isnot [IO.FileInfo]){throw 'Authored runtime regular file required'}
        $relative=[IO.Path]::GetRelativePath($assets,$file.FullName).Replace('\','/')
        Assert-MistwatchAuthoredNativeRelative $relative 'runtime member'
        if(-not $seen.Add($relative) -or -not $wanted.ContainsKey($relative)){throw ('Authored runtime unexpected/shadow member: '+$relative)}
        $entry=$wanted[$relative]
        if($entry[3] -ceq 'root-receipt'){
            if((Get-FileHash -LiteralPath $file.FullName).Hash -cne (Get-FileHash -LiteralPath $rootReceiptPath).Hash -or
                $file.Length -ne (Get-Item -LiteralPath $rootReceiptPath).Length){throw 'Authored root metadata not current generated copy'}
        }else{
            Assert-MistwatchFallenRow @{path=$file.FullName;bytes=$entry[4];sha256=$entry[5]}
            if($entry[3] -ceq 'copy'){
                Assert-MistwatchAuthoredNativeRelative $entry[1] 'reviewed runtime source'
                Assert-MistwatchFallenRow @{path=(Join-Path $SourceRoot $entry[1]);bytes=$entry[4];sha256=$entry[5]}
            }elseif($entry[3] -ceq 'root-mesh'){
                if((Get-FileHash -LiteralPath $file.FullName).Hash -cne (Get-FileHash -LiteralPath $rootMesh).Hash){throw 'Authored root mesh not current generated copy'}
            }elseif($entry[3] -cne 'boar'){throw 'Authored unknown reviewed runtime producer'}
        }
        @{path=$relative;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
    })
    if($rows.Count -ne $count){throw 'Authored runtime exact membership differs'}
    return ,@($rows|Sort-Object path)
}

function Read-MistwatchAuthoredNativeCertificate {
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
    Assert-MistwatchAuthoredNativeExpectedContract $expected
    Assert-MistwatchAuthoredNativeReceiptContract $receipt $expected.runtime_profile
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
    $wanted=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    if($expected.runtime_profile -ceq 'authored-leaf-cluster-full-v3') {
        $required+='games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json'
        $additions=Get-MistwatchLeafClusterRuntimeAdditions $source
        foreach($entry in $additions){$required+=[string]$entry[1]}
    }
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
    foreach($row in @($p.cpu_binary,$receipt.cpu_binary)){Assert-MistwatchFallenRow $row}
    if([IO.Path]::GetFullPath($p.cpu_binary.path) -ine [IO.Path]::GetFullPath($receipt.cpu_binary.path) -or
       $p.cpu_binary.bytes -ne $receipt.cpu_binary.bytes -or $p.cpu_binary.sha256 -cne $receipt.cpu_binary.sha256 -or
       [IO.Path]::GetFileName($p.cpu_binary.path) -cne 'MistwatchFallenWorldQualificationTest.exe' -or
       -not [IO.Path]::GetFullPath($p.cpu_binary.path).StartsWith($build+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Authored CPU binary identity/build differs'}
    $generated=if($p.generated_root_stem_override){$p.generated_root_stem_override}else{Join-Path $build 'games/mistwatch/native/generated-root-stem'}
    $runtime=Get-MistwatchAuthoredNativeRuntimeRows $source $assets $expected.runtime_profile $generated
    Assert-MistwatchFallenRelativeRows $p.runtime_assets $assets 'authored runtime provenance'
    $declared=@($p.runtime_assets|Sort-Object path|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    $actual=@($runtime|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    if(($declared-join "`n") -cne ($actual-join "`n")){throw 'Authored current/provenance complete runtime differs'}
    $world=Read-MistwatchFallenStrictJson (Join-Path $certificate 'WorldExpected.json')
    Assert-MistwatchAuthoredWorldExpectedContract $world $expected
    # 回傳本身是 NativeExpected；未進行 native/GPU 資格判定。
    return ,$expected
}
