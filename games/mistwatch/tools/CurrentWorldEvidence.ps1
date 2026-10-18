#requires -Version 7.0
# 目前森林的實機契約；預期只來自獨立 CPU 證書，不從捕捉結果回填。
. (Join-Path $PSScriptRoot 'CurrentWorldContract.ps1')
. (Join-Path $PSScriptRoot 'WorksiteContract.ps1')

function Read-MistwatchCurrentWorldReceipt([string]$Path) {
    $full=Assert-MistwatchFallenPhysicalPath $Path
    if((Get-Item -LiteralPath $full).Length -gt 524288){throw 'Current world receipt byte budget exceeded'}
    $values=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($line in [IO.File]::ReadAllLines($full,[Text.Encoding]::UTF8)){
        if([string]::IsNullOrWhiteSpace($line)){continue}
        $pair=$line.Split('=',2)
        if($pair.Count -ne 2 -or $pair[0] -cnotmatch '^[a-z][a-z0-9_]*$' -or
            $pair[1].Length -gt 8192 -or $pair[1] -match '[\x00-\x1F\x7F]' -or -not $seen.Add($pair[0])){
            throw ('Malformed, duplicate or shadow current-world field: '+$full)
        }
        $values.Add($pair[0],$pair[1])
        if($values.Count -gt 2048){throw 'Current world receipt field budget exceeded'}
    }
    return ,$values
}
function Assert-MistwatchCurrentWorldField($Values,[string]$Key,[string]$Expected) {
    if(-not $Values.ContainsKey($Key) -or $Values[$Key] -isnot [string] -or $Values[$Key] -cne $Expected){
        throw ('Actual current-world field differs: '+$Key)
    }
}
function Assert-MistwatchCurrentGpuMode($Receipt,[bool]$GpuValidation) {
    # Normal 沿用原矩陣，不冒充 GBV；只有 GBV 必須證明完整五項驗證旗標。
    if($GpuValidation){
        foreach($flag in @('debug_layer_enabled','gpu_validation_requested','gpu_validation_available','gpu_validation_executed','gpu_validation_passed')){
            Assert-MistwatchCurrentWorldField $Receipt $flag '1'
        }
        Assert-MistwatchCurrentWorldField $Receipt 'debug_warnings' '0'
    }else{
        foreach($flag in @('gpu_validation_requested','gpu_validation_executed')){
            Assert-MistwatchCurrentWorldField $Receipt $flag '0'
        }
    }
}
function Get-MistwatchCurrentWorldUnsigned($Values,[string]$Key) {
    if(-not $Values.ContainsKey($Key)){throw ('Missing current-world integer: '+$Key)}
    Assert-MistwatchAuthoredNativeUnsigned $Values[$Key] $Key
    return [ulong]$Values[$Key]
}
function Assert-MistwatchCurrentWorldExactSidecar([string]$Path,$Expected) {
    $actual=Read-MistwatchCurrentWorldReceipt $Path
    Assert-MistwatchFallenExactKeys $actual @($Expected.Keys) 'current-world sidecar'
    foreach($key in $Expected.Keys){Assert-MistwatchCurrentWorldField $actual $key $Expected[$key]}
    return ,$actual
}
function Assert-MistwatchCurrentWorldFloat32([string]$Value,[string]$Expected,[string]$Label) {
    [single]$actual=0;[single]$wanted=0
    $style=[Globalization.NumberStyles]::Float;$culture=[Globalization.CultureInfo]::InvariantCulture
    if($Value -cnotmatch '^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$' -or
        -not [single]::TryParse($Value,$style,$culture,[ref]$actual) -or
        -not [single]::TryParse($Expected,$style,$culture,[ref]$wanted) -or
        -not [single]::IsFinite($actual) -or -not [single]::IsFinite($wanted) -or
        [BitConverter]::SingleToUInt32Bits($actual) -ne [BitConverter]::SingleToUInt32Bits($wanted)){
        throw ('Actual current-world float32 differs: '+$Label)
    }
}
function Assert-MistwatchCurrentWorldVariantAccounting($Receipt,[string]$Variants,[string]$Count,[string]$Triangles,
    [string]$Population,[string]$SourceTriangles,[int]$VariantCount) {
    if(-not $Receipt.ContainsKey($Variants)){throw ('Missing current-world submitted variants: '+$Variants)}
    Assert-MistwatchAuthoredNativeCsv $Receipt[$Variants] $VariantCount $Variants
    Assert-MistwatchAuthoredNativeCsv $Population $VariantCount ($Variants+' population')
    Assert-MistwatchAuthoredNativeCsv $SourceTriangles $VariantCount ($Variants+' topology')
    $submitted=$Receipt[$Variants].Split(',');$limits=$Population.Split(',');$mesh=$SourceTriangles.Split(',')
    [ulong]$counted=0;[ulong]$trianglesCounted=0
    for($i=0;$i -lt $VariantCount;$i++){
        [ulong]$n=$submitted[$i];[ulong]$limit=$limits[$i];[ulong]$faces=$mesh[$i]
        if($n -gt $limit -or $limit -gt 100000 -or $faces -lt 1 -or $faces -gt 1000000){
            throw ('Current-world submitted variants exceed independent population/topology: '+$Variants)
        }
        $counted+=$n;$trianglesCounted+=$n*$faces
    }
    if((Get-MistwatchCurrentWorldUnsigned $Receipt $Count) -ne $counted -or
        (Get-MistwatchCurrentWorldUnsigned $Receipt $Triangles) -ne $trianglesCounted){
        throw ('Current-world submitted variant accounting differs: '+$Variants)
    }
    return @{count=$counted;triangles=$trianglesCounted}
}
function Assert-MistwatchCurrentFrameCameraEnvironment($Environment,[string]$Scene,$Receipt) {
    if($Environment -isnot [Collections.IDictionary]){throw 'CURRENT_FRAME_CAMERA_CONTAINER'}
    $known=@('initial','repair-shelter','repair-player-camera','repair-ready','river','river-overlook','strike',
        'boar-recovery','boar-profile','settlement-clue','settlement-clue-read','fuse-lock-wrong','fuse-lock-ready',
        'panel','powered','relay-west','relay-aligned','relay-linked','complete','audio-check','focus-pause',
        'boulder-profile','grass-profile','fern-profile','supply-profile','supply-collected','ranger-hut-profile',
        'trail-profile','shrub-profile','canopy-profile','canopy-root','canopy-detail')
    if($Scene -cnotin $known){throw 'CURRENT_FRAME_CAMERA_SCENE'}
    $key='AEVRYND_MISTWATCH_CAPTURE_FRAME_CAMERA'
    $matches=@(foreach($entry in $Environment.GetEnumerator()){
        if($entry.Key -is [string] -and $entry.Key -ieq $key){$entry.Key}
    })
    if($matches.Count -gt 1 -or ($matches.Count -eq 1 -and $matches[0] -cne $key)){
        throw 'CURRENT_FRAME_CAMERA_KEY_CASE'
    }
    $value=$null
    if($matches.Count -eq 1){$value=$Environment[$key]}
    $canopy=$Scene -cin @('canopy-profile','canopy-root','canopy-detail')
    # Windows 的空值環境項目仍可被列舉；GetEnvironmentVariableW 回傳 0，並未啟用相機開關。
    # 只接受未設、null 或真正空字串為關閉；不把 '0'、空白或其他型別當成關閉。
    if($canopy){
        if($value -isnot [string] -or $value -cne '1'){throw 'CURRENT_FRAME_CAMERA_ENABLED_VALUE'}
    }elseif($null -ne $value -and ($value -isnot [string] -or $value.Length -ne 0)){
        throw 'CURRENT_FRAME_CAMERA_DISABLED_VALUE'
    }
    Assert-MistwatchCurrentWorldField $Receipt 'camera_motion_source' $(if($canopy){'diagnostic_render_step'}else{'fixed_update'})
}
function Assert-MistwatchCurrentWorldCanopyCamera($Receipt,[string]$Scene) {
    if($Scene -cnotin @('canopy-profile','canopy-root','canopy-detail')){return}
    Assert-MistwatchCurrentWorldField $Receipt 'camera_source' 'diagnostic'
    $position=@($Receipt['camera_position'].Split(','));$target=@($Receipt['camera_target'].Split(','))
    if($position.Count -ne 3 -or $target.Count -ne 3){throw 'Current-world canopy camera requires3 coordinates'}
    $offset=if($Scene -ceq 'canopy-root'){@([Math]::Sqrt(2),.05,(-[Math]::Sqrt(2)))}
        elseif($Scene -ceq 'canopy-detail'){@((5.657/[Math]::Sqrt(2)),.8,(-5.657/[Math]::Sqrt(2)))}else{@(0,.5,-8.5)}
    for($axis=0;$axis -lt 3;$axis++){
        [double]$p=0;[double]$t=0
        if(-not [double]::TryParse($position[$axis],[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$p) -or
            -not [double]::TryParse($target[$axis],[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$t) -or
            -not [double]::IsFinite($p) -or -not [double]::IsFinite($t) -or
            [Math]::Abs($p-$t-$offset[$axis]) -gt .001){throw 'Current-world canopy camera offset differs'}
    }
}
function Assert-MistwatchCurrentWorldSurface([string]$Capture,$Receipt,$Expected) {
    $cpu=$Expected.surface_contract
    Assert-MistwatchForestSurfaceFields $cpu
    foreach($key in @('ground_art_surface_path','ground_art_surface_encoding','ground_habitat_layer_surface_path',
        'ground_habitat_layer_surface_encoding','ground_loaded_texture_identity')){
        Assert-MistwatchCurrentWorldField $Receipt $key $cpu[$key]
    }
    # last-bind 是實際觀測範圍，不把它誇大為每個 draw 的完整紀錄。
    $fixed=@{schema='mistwatch.forest-surface.1';mode='capture';manual_cli='0';profile=$cpu.profile;
        actual_setting_enabled=$cpu.parallax_enabled;height_mask_last_bind=$cpu.height_layer_mask;height_material_binds='2';
        last_height_semantic=$cpu.ground_loaded_texture_identity;terrain_semantic=$cpu.ground_loaded_texture_identity;
        render_observed='1';settings_fingerprint=$cpu.parallax_settings_fingerprint;
        texture_scale=$Expected.native_contract.ground_texture_scale;soil_layer_scale=$cpu.ground_soil_layer_scale;
        soil_effective_scale=$cpu.ground_soil_layer_scale;source_base_encoding=$cpu.ground_art_surface_encoding;
        source_leaf_encoding=$cpu.ground_habitat_layer_surface_encoding;canonical_base_encoding='0';canonical_leaf_encoding='0';
        geometry_displaced='0';all_draw_masks_recorded='0';art_accepted='0'}
    $actual=Read-MistwatchCurrentWorldReceipt ($Capture+'.forest-surface.txt')
    Assert-MistwatchFallenExactKeys $actual (@($fixed.Keys)+@('submitted_frame','depth_meters')) 'current surface actual23'
    foreach($key in $fixed.Keys){Assert-MistwatchCurrentWorldField $actual $key $fixed[$key]}
    Assert-MistwatchCurrentWorldFloat32 $actual['depth_meters'] $cpu.parallax_depth_meters 'POM depth'
    if((Get-MistwatchCurrentWorldUnsigned $actual 'submitted_frame') -lt 1){throw 'Current surface has no submitted render frame'}
    return ,$actual
}
# 純收據會計；logical slot 與過渡期雙 draw 分開，不變更場景或資格門檻。
function Assert-MistwatchCurrentWorldStandAccounting($Stand,$Expected) {
    if($Stand -isnot [Collections.IDictionary]){throw 'CURRENT_STAND_CONTAINER'}
    $keys=@('accepted_slots','post_cull_opaque_slots','post_cull_shadow_only_slots','actual_draws',
        'near_twig_draws','far_twig_draws','transition_pairs')
    $values=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    foreach($key in $keys){
        $exact=@($Stand.Keys|Where-Object{$_ -is [string] -and $_ -ceq $key})
        if($exact.Count -ne 1){throw ('CURRENT_STAND_MISSING:'+ $key)}
        if(@($Stand.Keys|Where-Object{$_ -is [string] -and $_ -ieq $key -and $_ -cne $key}).Count){
            throw ('CURRENT_STAND_KEY_CASE:'+ $key)
        }
        $values.Add($key,$Stand[$key])
    }
    $counts=@{}
    foreach($key in $keys){
        try{$counts[$key]=Get-MistwatchCurrentWorldUnsigned $values $key}
        catch{throw ('CURRENT_STAND_INTEGER:'+ $key)}
    }
    try{
        $population=@{population=$Expected.current_extras.landscape_stand_added_population}
        [ulong]$n=Get-MistwatchCurrentWorldUnsigned $population 'population'
    }catch{throw 'CURRENT_STAND_POPULATION_INTEGER'}
    # 目前完整森林 v5 的獨立 CPU 契約已固定新增 96 株；此處不得由實機反推容量。
    if($n -ne 96){throw 'CURRENT_STAND_POPULATION'}
    [ulong]$four=4*$n;[ulong]$three=3*$n;[ulong]$five=5*$n
    [ulong]$accepted=$counts.accepted_slots
    [ulong]$opaque=$counts.post_cull_opaque_slots
    [ulong]$shadow=$counts.post_cull_shadow_only_slots
    [ulong]$draws=$counts.actual_draws
    [ulong]$near=$counts.near_twig_draws
    [ulong]$far=$counts.far_twig_draws
    [ulong]$pairs=$counts.transition_pairs
    # 先限制各數值，再做加減，避免 UInt64 上界被動態數值提升或溢位掩蓋。
    if($accepted -ne $four){throw 'CURRENT_STAND_ACCEPTED'}
    if($opaque -gt $four){throw 'CURRENT_STAND_OPAQUE_RANGE'}
    if($shadow -gt $four){throw 'CURRENT_STAND_SHADOW_RANGE'}
    if($draws -gt $five){throw 'CURRENT_STAND_DRAWS_RANGE'}
    if($near -gt $n){throw 'CURRENT_STAND_NEAR_RANGE'}
    if($far -gt $n){throw 'CURRENT_STAND_FAR_RANGE'}
    if($pairs -gt $n){throw 'CURRENT_STAND_PAIR_RANGE'}
    if($pairs -gt $near -or $pairs -gt $far){throw 'CURRENT_STAND_PAIR_ENDPOINTS'}
    [ulong]$twig=$near+$far-$pairs
    if($twig -gt $n){throw 'CURRENT_STAND_TWIG_POPULATION'}
    [ulong]$visible=$opaque+$shadow
    if($visible -gt $four){throw 'CURRENT_STAND_VISIBLE_RANGE'}
    if($twig -gt $visible){throw 'CURRENT_STAND_TWIG_VISIBLE'}
    [ulong]$nonTwig=$visible-$twig
    if($nonTwig -gt $three){throw 'CURRENT_STAND_NON_TWIG_RANGE'}
    # 每一個完整 near/far pair 是同一個 logical slot，但增加一個 physical draw。
    if($draws -ne $visible+$pairs){throw 'CURRENT_STAND_DRAW_EQUATION'}
    return @{population=$n;accepted=$accepted;visible_slots=$visible;twig_slots=$twig;non_twig_slots=$nonTwig;
        near_draws=$near;far_draws=$far;transition_pairs=$pairs;physical_draws=$draws}
}
function Assert-MistwatchCurrentWorldLandscape([string]$Capture,$Expected) {
    $cpu=$Expected.current_extras
    $stand=Read-MistwatchCurrentWorldReceipt ($Capture+'.landscape-stand.txt')
    $planting=Read-MistwatchCurrentWorldReceipt ($Capture+'.landscape-planting.txt')
    $standFixed=@{schema='mistwatch.landscape-stand-native.1';profile=$cpu.landscape_stand_profile;
        capture_only='1';base_population=$cpu.landscape_stand_base_population;added_population=$cpu.landscape_stand_added_population;
        complete='1';pose_fingerprint=$cpu.landscape_stand_pose_fingerprint;source_fingerprint=$cpu.landscape_stand_source_fingerprint;
        created_mesh_acquisitions='0';created_material_acquisitions='0';accepted_slots='384';art_accepted='0';playable_outer_ring='0'}
    foreach($key in $standFixed.Keys){Assert-MistwatchCurrentWorldField $stand $key $standFixed[$key]}
    $plantingFixed=@{schema='mistwatch.authored-landscape-native.1';profile='original';capture_only='1';
        world_profile='forest-wood';habitat_profile='original';source_population=$cpu.landscape_stand_base_population;
        replacement_trees='0';created_acquisitions='0';accepted_fir_slots='2080';accepted_broadleaf_slots='0';
        art_accepted='0';playable_outer_ring='0';twig_profile=$cpu.landscape_twig_profile;twig_alpha_mode='1';twig_alpha_cutoff='0.25';
        twig_double_sided='1';twig_material_class='2';twig_range_profile='original';twig_switch_distance='60';twig_transition_width='16';
        twig_lod_acquisitions='1';twig_near_mesh_triangles=$cpu.landscape_twig_near_triangles;
        twig_near_mesh_fingerprint=$cpu.landscape_twig_near_fingerprint;
        fir_mesh_1_triangles=$cpu.landscape_twig_far_triangles;fir_mesh_1_fingerprint=$cpu.landscape_twig_far_fingerprint}
    foreach($key in $plantingFixed.Keys){Assert-MistwatchCurrentWorldField $planting $key $plantingFixed[$key]}
    foreach($key in @('twig_near_mesh_handle','fir_mesh_1_handle','twig_texture_identity')){
        if((Get-MistwatchCurrentWorldUnsigned $planting $key) -lt 1){throw ('Missing current uploaded landscape handle: '+$key)}
    }
    if($planting['twig_near_mesh_handle'] -ceq $planting['fir_mesh_1_handle']){throw 'Current near/far twig meshes alias one handle'}
    foreach($key in @('scene_serial','camera_hash')){
        if((Get-MistwatchCurrentWorldUnsigned $stand $key) -lt 1 -or
            $stand[$key] -cne $planting[$key]){throw ('Current landscape sidecars are not from one submitted view: '+$key)}
    }
    $null=Assert-MistwatchCurrentWorldStandAccounting $stand $Expected
    [ulong]$accepted=0;[ulong]$proposed=0
    foreach($band in 0..5){
        Assert-MistwatchCurrentWorldField $stand ('band_'+$band+'_accepted') '16'
        $accepted+=Get-MistwatchCurrentWorldUnsigned $stand ('band_'+$band+'_accepted')
        $proposed+=Get-MistwatchCurrentWorldUnsigned $stand ('band_'+$band+'_proposals')
    }
    if($accepted -ne (Get-MistwatchCurrentWorldUnsigned $stand 'added_population') -or
        $proposed -ne (Get-MistwatchCurrentWorldUnsigned $stand 'proposals')){throw 'Current stand band accounting differs'}
    return @{stand=$stand;planting=$planting;scope='Independent stand poses and uploaded near/far topology; not independent per-pixel visibility or art approval'}
}
function Assert-MistwatchCurrentWorldRoadsideAccounting($Receipt,$Expected) {
    $cpu=$Expected.current_extras
    Assert-MistwatchAuthoredNativeCsv $cpu.roadside_broadleaf_retained_variants 2 'current retained roadside variants'
    $limits=$cpu.roadside_broadleaf_retained_variants.Split(',')
    [long]$n=Get-MistwatchCurrentWorldUnsigned $Receipt 'roadside_broadleaf_submitted'
    [long]$triangles=Get-MistwatchCurrentWorldUnsigned $Receipt 'roadside_broadleaf_submitted_triangles'
    [long]$first=$cpu.roadside_broadleaf_mesh_0_triangles;[long]$second=$cpu.roadside_broadleaf_mesh_1_triangles
    if($n -gt 96 -or $first -le $second -or $first -gt 1000000 -or $second -lt 1 -or
        $triangles -lt $n*$second -or $triangles -gt $n*$first){throw 'Current roadside submission exceeds exact topology domain'}
    [long]$numerator=$triangles-$n*$second;[long]$divisor=$first-$second
    if($numerator % $divisor -ne 0){throw 'Current roadside submitted triangles do not represent its two uploaded meshes'}
    [long]$variant0=$numerator/$divisor;[long]$variant1=$n-$variant0
    if($variant0 -lt 0 -or $variant1 -lt 0 -or $variant0 -gt [long]$limits[0] -or $variant1 -gt [long]$limits[1]){
        throw 'Current roadside submission uses a suppressed or unauthored variant'
    }
    return @{count=$n;triangles=$triangles;variant_0=$variant0;variant_1=$variant1}
}
function Assert-MistwatchCurrentWorldCleanups([string]$Capture,$Receipt,$Expected) {
    # 目前 recipe 的29份收據；不接受同前綴的額外、缺失或大小寫影子。
    $fixed=@{
        'audio'=@{sounds='0';bytes='0';voices='0';cleanup='success'};
        'boulder'=@{materials='0';mesh_destroy_calls='2';cleanup='success'};
        'bridge-worksite'=@{mesh_destroy_calls='2';cleanup='success'};
        'broadleaf'=@{materials='0';mesh_destroy_calls='3';cleanup='success'};
        'canopy'=@{mesh_destroy_calls=$Expected.current_extras.canopy_cleanup_mesh_destroy_calls;materials_remaining='0';cleanup='success'};
        'core-herbs'=@{acquisitions='0';release_calls='0';remaining_owned='0';cleanup='success'};
        'crown-growth'=@{acquisitions='0';release_calls='0';remaining_owned='0';cleanup='success'};
        'desk'=@{mesh_destroy_calls='1';destroyed_handle=$Receipt['ranger_desk_resource_handle'];cleared_resources='1';cleanup='success'};
        'entrance-road'=@{materials='0';mesh_destroy_calls='3';cleanup='success'};
        'fern'=@{materials='0';mesh_destroy_calls='4';cleanup='success'};
        'generator'=@{mesh_destroy_calls='3';cleared_resources='3';cleanup='success'};
        'grass'=@{materials='0';mesh_destroy_calls='5';canonical_mesh_destroy_calls='5';candidate_mesh_destroy_calls='0';cleanup='success'};
        'hut'=@{mesh_destroy_calls='1';cleanup='success'};
        'landscape-fir'=@{mesh_destroy_calls=$Expected.current_extras.landscape_fir_cleanup_mesh_destroy_calls;remaining_owned='0';cleanup='success'};
        'nettle'=@{materials='0';mesh_destroy_calls='0';cleanup='success'};
        'planked-shelter'=@{destroyed_handle=$Receipt['planked_shelter_handle'];destroy_calls='1';cleared='1'};
        'repair-sign'=@{wood_handle=$Receipt['repair_sign_wood_handle'];paint_handle=$Receipt['repair_sign_paint_handle'];
            steel_handle=$Receipt['repair_sign_steel_handle'];destroy_calls='3';cleared='1'};
        'repair-tools'=@{metal_handle=$Receipt['repair_tools_metal_handle'];grip_handle=$Receipt['repair_tools_handle_handle'];destroy_calls='2';cleared='1'};
        'roadside-broadleaf'=@{materials='0';mesh_destroy_calls='2';cleanup='success'};
        'route-grass'=@{acquisitions='0';release_calls='0';remaining_owned='0';cleanup='success'};
        'shelter-detail'=@{mesh_destroy_calls='2';remaining_owned='0';cleanup='success'};
        'shrub'=@{mesh_destroy_calls='8';cleanup='success'};
        'supply'=@{mesh_destroy_calls='2';cleanup='success'};
        'trail'=@{mesh_destroy_calls='2';cleanup='success'};
        'workshop'=@{destroyed_handle=$Receipt['planked_workshop_handle'];destroy_calls='1';cleared='1'}
    }
    foreach($key in @('ranger_desk_resource_handle','planked_shelter_handle','repair_sign_wood_handle','repair_sign_paint_handle',
        'repair_sign_steel_handle','repair_tools_metal_handle','repair_tools_handle_handle','planked_workshop_handle')){
        if((Get-MistwatchCurrentWorldUnsigned $Receipt $key) -lt 1){throw ('Current cleanup handle is empty: '+$key)}
    }
    $dynamic=@('landscape-planting','shoulder','shoulder-groundcover')
    $allowed=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $prefix=[IO.Path]::GetFileName($Capture)
    foreach($name in @($fixed.Keys)+$dynamic+@('fallen-log')){$null=$allowed.Add($prefix+'.'+$name+'-cleanup.txt')}
    $files=@(Get-ChildItem -LiteralPath (Split-Path $Capture) -File -Force|Where-Object{
        $_.Name.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -and $_.Name -imatch 'cleanup'})
    if($files.Count -ne 29 -or $allowed.Count -ne 29){throw 'Current world requires exact29 cleanup sidecars'}
    foreach($file in $files){if(-not $allowed.Contains($file.Name)){throw ('Shadow current cleanup filename: '+$file.Name)}}
    $results=@{}
    foreach($name in $fixed.Keys){$results[$name]=Assert-MistwatchCurrentWorldExactSidecar ($Capture+'.'+$name+'-cleanup.txt') $fixed[$name]}
    foreach($name in $dynamic){
        $actual=Read-MistwatchCurrentWorldReceipt ($Capture+'.'+$name+'-cleanup.txt')
        $static=@{created_acquisitions='0';release_calls='0';remaining_owned='0';pending_release_bytes_after_idle='0';cleanup='success'}
        if($name -cne 'landscape-planting'){$static.release_failures='0'}
        Assert-MistwatchFallenExactKeys $actual (@($static.Keys)+@('references_before','references_after')) 'current zero-acquisition cleanup'
        foreach($key in $static.Keys){Assert-MistwatchCurrentWorldField $actual $key $static[$key]}
        if((Get-MistwatchCurrentWorldUnsigned $actual 'references_before') -ne
            (Get-MistwatchCurrentWorldUnsigned $actual 'references_after')){throw 'Current resource reference counts did not return to baseline'}
        $results[$name]=$actual
    }
    # FallenLogContract 另驗 source/pose 與同一 mesh handle；這裡仍保留其原始清理欄位。
    $results['fallen-log']=Read-MistwatchCurrentWorldReceipt ($Capture+'.fallen-log-cleanup.txt')
    return ,$results
}
function Assert-MistwatchCurrentWorldEvidence {
    param([Parameter(Mandatory=$true)][string]$Capture,[Parameter(Mandatory=$true)][string]$Scene,
        [Parameter(Mandatory=$true)][bool]$GpuValidation,[Parameter(Mandatory=$true)]$Expected)
    $known=@('initial','repair-shelter','repair-player-camera','repair-ready','river','river-overlook','strike',
        'boar-recovery','boar-profile','settlement-clue','settlement-clue-read','fuse-lock-wrong','fuse-lock-ready',
        'panel','powered','relay-west','relay-aligned','relay-linked','complete','audio-check','focus-pause',
        'boulder-profile','grass-profile','fern-profile','supply-profile','supply-collected','ranger-hut-profile',
        'trail-profile','shrub-profile','canopy-profile','canopy-root','canopy-detail')
    if($Scene -cnotin $known){throw 'Unknown current-world capture scene'}
    Assert-MistwatchCurrentWorldExpected $Expected
    if(-not [IO.Path]::IsPathFullyQualified($Capture)){throw 'Current world requires absolute capture path'}
    $capturePath=Assert-MistwatchFallenPhysicalPath $Capture
    $null=Assert-MistwatchJointPngBytes ([IO.File]::ReadAllBytes($capturePath))
    $receipt=Read-MistwatchCurrentWorldReceipt ($capturePath+'.receipt.txt')
    $projected=@{}
    foreach($key in Get-MistwatchAuthoredNativeFieldNames){
        if(-not $receipt.ContainsKey($key)){throw ('Missing actual current153 field: '+$key)}
        $projected[$key]=$receipt[$key]
    }
    Assert-MistwatchFallenExactKeys $projected (Get-MistwatchAuthoredNativeFieldNames) 'actual current153 whitelist projection'
    foreach($key in $projected.Keys){Assert-MistwatchCurrentWorldField $projected $key $Expected.native_contract[$key]}
    $cpuOnly=@('canopy_cleanup_mesh_destroy_calls','landscape_fir_cleanup_mesh_destroy_calls','shrubs_preserved_prefix',
        'entrance_understory_cluster_counts','landscape_twig_profile','landscape_stand_profile',
        'landscape_stand_base_population','landscape_stand_added_population','landscape_stand_pose_fingerprint',
        'landscape_stand_source_fingerprint','landscape_twig_near_triangles','landscape_twig_far_triangles',
        'landscape_twig_near_fingerprint','landscape_twig_far_fingerprint',
        'roadside_broadleaf_retained_variants','forest_cover_retained_render_variants')
    foreach($key in $Expected.current_extras.Keys){
        if($key -cin $cpuOnly){continue}
        Assert-MistwatchCurrentWorldField $receipt $key $Expected.current_extras[$key]
    }
    $boulderBinding=@{boulder_materials='1';boulder_use_uv='1';boulder_texture_scale='1';boulder_gpu_resources='1';
        boulder_diagnostic_profile='original';boulder_normal_scale='1';boulder_has_textures='1'}
    foreach($key in $boulderBinding.Keys){Assert-MistwatchCurrentWorldField $receipt $key $boulderBinding[$key]}
    Assert-MistwatchCurrentWorldField $receipt 'canopy_preview_fixture' '0'
    Assert-MistwatchCurrentWorldField $receipt 'gpu_validation_requested' $(if($GpuValidation){'1'}else{'0'})
    Assert-MistwatchCurrentWorldField $receipt 'gpu_validation_executed' $(if($GpuValidation){'1'}else{'0'})
    $cpu=$Expected.cpu_identity
    $grass=Assert-MistwatchCurrentWorldVariantAccounting $receipt 'submitted_grass_variants' 'submitted_grass' 'submitted_grass_triangles' `
        $Expected.current_extras.forest_cover_retained_render_variants $cpu.grass_source_triangles 5
    $colony=Assert-MistwatchCurrentWorldVariantAccounting $receipt 'submitted_colony_variants' 'submitted_colony_ferns' 'submitted_colony_fern_triangles' `
        $Expected.native_contract.fern_colony_variants $cpu.fern_source_triangles 4
    $original=Assert-MistwatchCurrentWorldVariantAccounting $receipt 'submitted_original_fern_variants' 'submitted_original_ferns' 'submitted_original_fern_triangles' `
        '105,105,105,105' $cpu.fern_source_triangles 4
    if($grass.count -lt 1 -or $original.count -lt 1 -or $original.count -gt 420 -or
        (Get-MistwatchCurrentWorldUnsigned $receipt 'submitted_ferns') -ne ($original.count+$colony.count) -or
        (Get-MistwatchCurrentWorldUnsigned $receipt 'submitted_fern_triangles') -ne ($original.triangles+$colony.triangles)){
        throw 'Current original420 plus colony300 submission accounting differs'
    }
    if($Scene -cin @('initial','river-overlook')){
        $fixed=$cpu.fixed_scene_original_ferns[$Scene]
        Assert-MistwatchCurrentWorldField $receipt 'submitted_original_ferns' $fixed.plants
        Assert-MistwatchCurrentWorldField $receipt 'submitted_original_fern_triangles' $fixed.triangles
        Assert-MistwatchCurrentWorldField $receipt 'submitted_original_fern_variants' $fixed.variants
    }
    Assert-MistwatchCurrentWorldCanopyCamera $receipt $Scene
    $fallen=Assert-MistwatchFallenLogEvidence -Capture $capturePath -Expected $Expected
    $surface=Assert-MistwatchCurrentWorldSurface $capturePath $receipt $Expected
    $landscape=Assert-MistwatchCurrentWorldLandscape $capturePath $Expected
    $roadside=Assert-MistwatchCurrentWorldRoadsideAccounting $receipt $Expected
    $cleanups=Assert-MistwatchCurrentWorldCleanups $capturePath $receipt $Expected
    return @{scene=$Scene;native_fields=153;current_extra_fields=$Expected.current_extras.Count;surface_fields=16;
        world_profile=$receipt['world_profile'];runtime_profile=$Expected.runtime_profile;
        fallen=$fallen;surface=$surface;landscape=$landscape;roadside=$roadside;cleanups=$cleanups;actual_current_world_fields_verified=$true;
        scope='Current-world projection, submitted topology accounting, actual material last-bind and exact resource receipts only; outer process, full49 and manual play remain separately verified'}
}
