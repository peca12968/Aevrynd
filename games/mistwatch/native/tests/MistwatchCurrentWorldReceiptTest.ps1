#requires -Version 7.0
param(
    [switch]$FootprintOnly,
    [string]$SourceRoot='', [string]$BuildRoot='', [string]$CertificateRoot='', [string]$ExecutablePath=''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../tools/CurrentWorldContract.ps1')
. (Join-Path $PSScriptRoot '../../tools/CurrentWorldEvidence.ps1')
# 合成物件只測拒絕規則；另可明確指定真 CPU 證書，實際呼叫完整讀取器，不產生或偽造資格。
$script:Rejected=0
function Copy-Tree($Value){
    if($Value -is [Collections.IDictionary]){
        $copy=[Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
        foreach($key in $Value.Keys){$copy.Add($key,(Copy-Tree $Value[$key]))}
        return ,$copy
    }
    if($Value -is [array]){return ,@(foreach($item in $Value){Copy-Tree $item})}
    return ,$Value
}
function Reject([string]$Label,[scriptblock]$Action){
    $failed=$false;try{& $Action}catch{$failed=$true}
    if(-not $failed){throw ('CURRENT_WORLD_RECEIPT_NEGATIVE_ACCEPTED: '+$Label)}
    $script:Rejected++
}
function Identity([string]$Path){return @{path=$Path;bytes=1;sha256=('A'*64)}}
$fields=@(Get-MistwatchAuthoredNativeFieldNames)
if($fields.Count -ne 153 -or @($fields|Sort-Object -Unique).Count -ne 153){throw 'AUTHORED_NATIVE_RECEIPT_INVENTORY_CHANGED'}
$native=[Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
foreach($key in $fields){$native.Add($key,'1')}
foreach($pair in @(@('world_profile','forest-wood'),@('canopy_collision_mode','wood-grid-conservative'),@('fir_layout','forest-wood-mature'),
    @('forest_cover_mode','fixed_cluster_patches'),@('forest_cover_retention_revision','original-safe-pose-v1'),
    @('entrance_canopy_layout','entrance-authored-world-v1'),@('ground_habitat_revision','root-soil-and-mission-spurs-v1'),
    @('ground_source','ground037-moss-floor'),@('ground_habitat_layer_source','forest-leaves-04-habitat'),
    @('ground_height_alpha','shared_core_normalized'),@('planked_shelter_wood_profile','weathered-bands-v1'),
    @('planked_workshop_profile','planked-weathered-bands-v1'),@('grass_shape_profile','source-geometry'),@('understory_mix_profile','original'),
    @('ground_normal_scale','0.5'),@('ground_texture_scale','0.75'),@('canopy_collision_spacing_m','0.15'),
    @('ground_distance_normal_strength','0.649999976'),@('ground_meso_partition','0'),
    @('forest_cover_variants','1,1,1,1,1'),@('fern_colony_variants','1,1,1,1'),
    @('shrubs_render_leaf_counts','1,1,1,1'),@('shrubs_uploaded_leaf_indices','1,1,1,1'),@('shrubs_uploaded_stem_indices','1,1,1,1'),
    @('forest_cover_clearance_colliders','8853'))){$native[$pair[0]]=$pair[1]}
foreach($key in @('ground_art_base_color_path','ground_art_normal_path','ground_art_surface_path',
    'ground_habitat_layer_base_path','ground_habitat_layer_normal_path','ground_habitat_layer_surface_path')){$native[$key]='test/'+$key+'.dds'}
$profile='authored-mistwatch-only-v1'
$provenance=@{schema='mistwatch.authored-world.cpu-provenance.1';world_cohort='entrance-authored-habitat-v1';authored_world_qualified=$false;
    runtime_profile=$profile;generated_root_stem_override='';source_root='C:\Synthetic\Source';runtime_assets_root='C:\Synthetic\Build\bin\Release\assets';
    source_files=@((Identity 'source.cpp'));runtime_assets=@((Identity 'test.dds'));fallen_source_assets=@((Identity 'mesh.aemesh'));
    raw_mesh=(Identity 'mesh.aemesh');test_source=(Identity 'source.cpp');cpu_binary=(Identity 'C:\Synthetic\Build\MistwatchFallenWorldQualificationTest.exe');
    toolchain=@{msc_full_ver=194435217;cplusplus=202002;configuration='Release-x64-C++20-/W4-/WX-/MP1-/fp:strict-MultiThreadedDLL';abi='formal-AevryndEngine'};
    source_input_unchanged=$true;runtime_input_unchanged=$true;
    outer_receipt_scope='Before/after selected source and runtime raw identities only. Root independently binds fresh compile/link inputs, exits, Job drain and JUnit; no self-claimed outer process or GPU qualification.'}
$expected=@{schema='mistwatch.authored-world.native-expected.1';world_cohort='entrance-authored-habitat-v1';runtime_profile=$profile;
    world_profile='forest-wood';fallen_log_profile='scanned-straight-v1';production_full_solid_volumes=$true;
    source=@{sha256='b7f6a05bf928787082dea92a0051557496e45894a9c2b1a0af1e51110abd0d46';bytes='434012';vertices='8956';indices='45807';triangles='15269'};
    native_contract=$native;fallen_pose_words=(Get-MistwatchFallenAuthoredPoseWords);
    cpu_identity=@{rich_cover_word_fnv='3';terrain_fingerprint='4';original_prop_fingerprint='5';source_original_nonlog_count='32';retained_nonlog_count='29';
        initial_world=@{count='8853';bytewise_fnv='1';typed_fingerprint='2'};repaired_world=@{count='8854';bytewise_fnv='6';typed_fingerprint='7'};
        grass_source_vertices='1,2,3,4,5';grass_source_triangles='1,2,3,4,5';fern_source_triangles='1,2,3,4';
        fixed_scene_original_ferns=@{initial=@{plants='4';triangles='10';variants='1,1,1,1'};'river-overlook'=@{plants='0';triangles='0';variants='0,0,0,0'}}};
    provenance=$provenance;scope=@{authored_world_CPU_passed=$true;native_expected_author_passed=$true;native_executed=$false;GPU_qualified=$false;art_accepted=$false;manual_play=$false;published=$false}}
$receipt=@{schema='mistwatch.authored-world.native-expected-receipt.1';world_cohort='entrance-authored-habitat-v1';runtime_profile=$profile;
    status='authored-native-expectation-CPU-passed';native_expected_author_passed=$true;source_input_unchanged=$true;runtime_input_unchanged=$true;
    expected_file=(Identity 'NativeExpected.json');world_expected_file=(Identity 'WorldExpected.json');proof_file=(Identity 'world-seat.txt');
    cpu_binary=(Copy-Tree $provenance.cpu_binary);scope=@{native_qualified=$false;actual_Main_route_qualified=$false;art_accepted=$false;full49_qualified=$false;outer_process_qualified=$false}}
$world=@{schema='mistwatch.authored-world.cpu-expected.1';world_cohort='entrance-authored-habitat-v1';runtime_profile=$profile;
    world=@{before_solids=8853;after_solids=8854;canopy=1;wood=1;before_fingerprint=1;after_fingerprint=6;author_fingerprint=1;source_fir_fingerprint=1;source_middle_fingerprint=1};
    vegetation=@{firs=1;middle=1;saplings=1;props=33;original_ferns=420;cover=1;retained=1;rejected=1;filled=1;cover_fingerprint=1;provenance_fingerprint=1;fern_colonies=1;shrubs=1;litter_patches=1};
    provenance=(Copy-Tree $provenance);scope=@{authored_world_CPU_passed=$true;native_expected_author_passed=$false;native_executed=$false;GPU_qualified=$false;art_accepted=$false;manual_play=$false;published=$false}}

$legacy=Copy-Tree $native
$legacy.shrubs_plants='64';$legacy.shrubs_submitted='64';$legacy.canopy_plants='8';$legacy.canopy_rooted_meshes='8'
$legacy.forest_cover_variants='720,720,720,720,720';$legacy.ground_cover_instances='3600'
$expected.native_contract=Copy-Tree $legacy;$expected.legacy_native_contract=$legacy
$expected.schema='mistwatch.current-world.native-expected.1';$expected.world_cohort='entrance-current-forest-surface-v1';$expected.runtime_profile='authored-current-forest-full-v5'
$p=$expected.provenance;$p.schema='mistwatch.current-world.cpu-provenance.1';$p.world_cohort=$expected.world_cohort;$p.runtime_profile=$expected.runtime_profile
$p.generated_root_stem_override='C:\Synthetic\Build\games\mistwatch\native\generated-root-stem'
$p.runtime_assets=@(foreach($i in 0..417){Identity ('runtime/item-'+$i+'.dds')})
$p.fallen_source_assets=@(foreach($i in 0..5){Identity ('fallen/item-'+$i+'.dds')})
$surface=@{profile='forest-moss-leaf-metric-v1';ground_art_surface_path='moss-height/Ground037_SurfaceOrmHeight_Level0.png';ground_art_surface_bytes='1531546';
    ground_art_surface_sha256='012d2378aa6e62cca05ad66d69b512966d5894694b7b24d39afd0ca7ca655f38';ground_art_surface_encoding='2';
    ground_habitat_layer_surface_path='leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png';ground_habitat_layer_surface_bytes='9102955';
    ground_habitat_layer_surface_sha256='9f3259f4772f294edca1de03fa4e8d7ed65c0924238b1a6d0b27b8fed7c0ef85';
    ground_habitat_layer_surface_encoding='2';ground_rock_surface_encoding='0';height_layer_mask='3';ground_soil_layer_scale='1.75';
    parallax_enabled='1';parallax_depth_meters='0.015';ground_loaded_texture_identity='9';parallax_settings_fingerprint='10'}
$expected.surface_contract=$surface
foreach($key in @('ground_art_surface_path','ground_art_surface_encoding','ground_habitat_layer_surface_path','ground_habitat_layer_surface_encoding','ground_loaded_texture_identity')){
    $expected.native_contract[$key]=$surface[$key]
}
foreach($pair in @(@('shrubs_plants','128'),@('shrubs_submitted','128'),@('planked_workshop_profile','framed-workshop-v1'),@('planked_workshop_vertices','12480'),
    @('planked_workshop_indices','17160'),@('ground_packed_orm','0'),@('ground_habitat_layer_source','unrecognized'))){$expected.native_contract[$pair[0]]=$pair[1]}
$expected.art_profiles=Get-MistwatchCurrentArtProfiles
$extras=[Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
foreach($key in Get-MistwatchCurrentExtraNames){$extras.Add($key,'1')}
foreach($entry in (Get-MistwatchCurrentExtraTokens).GetEnumerator()){$extras[$entry.Key]=$entry.Value}
foreach($key in @('entrance_road_base_path','entrance_road_surface_path','entrance_road_normal_path','settlement_surface_base_path','settlement_surface_orm_path','settlement_surface_normal_path')){$extras[$key]='test/'+$key+'.dds'}
foreach($i in 0..2){$extras['settlement_surface_channel_'+$i+'_sha256']='a'*64;$extras['settlement_surface_mesh_'+$i+'_triangles']='100'}
$extras.settlement_surface_material_occlusion='0.93'
foreach($pair in @(@('boulder_gpu_resources','1'),@('boulder_instances','29'),@('boulder_mesh_vertices','81373'),@('boulder_mesh_triangles','66122'),
    @('canopy_cleanup_mesh_destroy_calls','11'),@('entrance_road_materials','2'),@('entrance_road_original_terrain_drawn','0'),@('entrance_road_submitted','3'),
    @('entrance_understory_available_shared_meshes','8'),@('entrance_understory_clusters','6'),@('entrance_understory_population','24'),@('entrance_understory_left','12'),
    @('entrance_understory_right','12'),@('entrance_understory_submitted','24'),@('landscape_fir_cleanup_mesh_destroy_calls','5'),@('landscape_stand_added_population','96'),
    @('landscape_stand_base_population','520'),@('roadside_broadleaf_population','96'),@('roadside_broadleaf_left','48'),@('roadside_broadleaf_right','48'),
    @('roadside_broadleaf_materials','1'),@('settlement_surface_submitted','3'),@('shrubs_preserved_prefix','64'),@('shrubs_target','128'),@('workshop_frame_members','54'),
    @('shelter_detail_frame_members','57'),@('shelter_detail_corrugations','20'),@('entrance_road_source_triangles','300'),@('entrance_road_forest_triangles','200'),@('entrance_road_road_triangles','100'),
    @('entrance_understory_cluster_counts','4,4,4,4,4,4'),@('forest_cover_retained_render_variants','719,720,720,720,720'),@('roadside_broadleaf_retained_variants','47,48'),
    @('entrance_understory_suppressed_grass','1'),@('entrance_understory_suppressed_broadleaf','1'),@('entrance_understory_suppression_fingerprint','7454681923338364775'))){
    $extras[$pair[0]]=$pair[1]
}
$expected.current_extras=$extras;$expected.suppression_ids=@{grass=@(0);broadleaf=@(0)}
$receipt.schema='mistwatch.current-world.native-expected-receipt.1';$receipt.world_cohort=$expected.world_cohort;$receipt.runtime_profile=$expected.runtime_profile
$receipt.status='current-native-expectation-CPU-passed';$receipt.authored_world_CPU_passed=$true;$receipt.current_geometry_CPU_passed=$true
$world.schema='mistwatch.current-world.cpu-expected.1';$world.world_cohort=$expected.world_cohort;$world.runtime_profile=$expected.runtime_profile
$world.provenance=Copy-Tree $p;$world.world.canopy=8;$world.vegetation.legacy_shrubs=64;$world.vegetation.shrubs=128
$world.scope.current_geometry_CPU_passed=$true
Assert-MistwatchCurrentWorldExpected $expected
Assert-MistwatchCurrentWorldReceipt $receipt $true
Assert-MistwatchCurrentWorldCpuExpected $world $expected
if(-not $FootprintOnly){
foreach($key in @($expected.Keys)){
    Reject ('current top missing '+$key) {$changed=Copy-Tree $expected;$changed.Remove($key);Assert-MistwatchCurrentWorldExpected $changed}
}
Reject 'current top extra' {$changed=Copy-Tree $expected;$changed.extra='1';Assert-MistwatchCurrentWorldExpected $changed}
foreach($section in @('native_contract','legacy_native_contract','current_extras','surface_contract','art_profiles')){
    foreach($key in @($expected[$section].Keys)){
        Reject ($section+' missing '+$key) {$changed=Copy-Tree $expected;$changed[$section].Remove($key);Assert-MistwatchCurrentWorldExpected $changed}
        Reject ($section+' non-string '+$key) {$changed=Copy-Tree $expected;$changed[$section][$key]=1;Assert-MistwatchCurrentWorldExpected $changed}
    }
}
foreach($key in @(Get-MistwatchAuthoredNativeFieldNames|Where-Object{$_ -cnotin @(Get-MistwatchCurrentProjectionNames)})){
    Reject ('unreviewed153 delta '+$key) {$changed=Copy-Tree $expected;$changed.native_contract[$key]+='0';Assert-MistwatchCurrentWorldExpected $changed}
}
foreach($key in @($expected.scope.Keys)){
    Reject ('scope flag '+$key) {$changed=Copy-Tree $expected;$changed.scope[$key]=-not $changed.scope[$key];Assert-MistwatchCurrentWorldExpected $changed}
}
foreach($key in @('schema','world_cohort','runtime_profile')){
    Reject ('identity '+$key) {$changed=Copy-Tree $expected;$changed[$key]='wrong';Assert-MistwatchCurrentWorldExpected $changed}
}
foreach($ids in @(@(0,0),@(1,0),@(-1),@(3600),@('0'),@(0.0),@($true))){
    Reject 'suppression duplicate/order/type/domain' {$changed=Copy-Tree $expected;$changed.suppression_ids.grass=$ids;Assert-MistwatchCurrentWorldExpected $changed}
}
Reject 'suppression ID fingerprint changed' {$changed=Copy-Tree $expected;$changed.suppression_ids.grass=@(1);Assert-MistwatchCurrentWorldExpected $changed}
Reject 'suppression count mismatch' {$changed=Copy-Tree $expected;$changed.current_extras.entrance_understory_suppressed_grass='2';Assert-MistwatchCurrentWorldExpected $changed}
Reject 'surface semantic ID mismatch' {$changed=Copy-Tree $expected;$changed.native_contract.ground_loaded_texture_identity='10';Assert-MistwatchCurrentWorldExpected $changed}
Reject 'old64 currentshrubs' {$changed=Copy-Tree $expected;$changed.native_contract.shrubs_plants='64';Assert-MistwatchCurrentWorldExpected $changed}
Reject 'old4 canopy' {$changed=Copy-Tree $expected;$changed.legacy_native_contract.canopy_plants='4';$changed.native_contract.canopy_plants='4';Assert-MistwatchCurrentWorldExpected $changed}
Reject 'old387 runtime' {$changed=Copy-Tree $expected;$changed.provenance.runtime_assets=@($changed.provenance.runtime_assets|Select-Object -First 387);Assert-MistwatchCurrentWorldExpected $changed}
Reject 'old7 canopy cleanup' {$changed=Copy-Tree $expected;$changed.current_extras.canopy_cleanup_mesh_destroy_calls='7';Assert-MistwatchCurrentWorldExpected $changed}
Reject 'old26611 boulder' {$changed=Copy-Tree $expected;$changed.current_extras.boulder_mesh_triangles='26611';Assert-MistwatchCurrentWorldExpected $changed}
Reject 'changed world count' {$changed=Copy-Tree $world;$changed.world.before_solids=8852;Assert-MistwatchCurrentWorldCpuExpected $changed $expected}
Reject 'world128 legacyshrubs' {$changed=Copy-Tree $world;$changed.vegetation.legacy_shrubs=128;Assert-MistwatchCurrentWorldCpuExpected $changed $expected}
Reject 'world false currentCPU' {$changed=Copy-Tree $world;$changed.scope.current_geometry_CPU_passed=$false;Assert-MistwatchCurrentWorldCpuExpected $changed $expected}
foreach($key in @($receipt.Keys)){
    Reject ('receipt missing '+$key) {$changed=Copy-Tree $receipt;$changed.Remove($key);Assert-MistwatchCurrentWorldReceipt $changed $true}
}
foreach($key in @($receipt.scope.Keys)){
    Reject ('receipt scope claim '+$key) {$changed=Copy-Tree $receipt;$changed.scope[$key]=$true;Assert-MistwatchCurrentWorldReceipt $changed $true}
}
}
# 額外四份完整足跡是正式產生器輸出，不是任意可加的檔案；原始位元與兩個完整世界需同時相符。
function Hex-Word([uint32]$Word){
    [uint64]$bits=$Word;$sign=if($bits-band 2147483648){'-'}else{''}
    $exponent=[int](($bits-shr 23)-band 255);$fraction=($bits-band 8388607)-shl 29
    if($exponent -eq 0){if($fraction -ne 0){throw 'Synthetic fixture does not need subnormal hex writer'};return ($sign+'0x0.0000000000000p+0')}
    $e=$exponent-127;$eText=if($e -ge 0){'+'+$e}else{[string]$e}
    return ($sign+'0x1.'+$fraction.ToString('x13')+'p'+$eText)
}
$footprintText=(@(foreach($id in @('0','8','34','35')){
    'SEATED_PROP id='+$id+' complete_proof=footprint-prop-'+$id+'.txt bytes=1 sha256='+('a'*64)+' final_min=0x0/0x1 predecessor_min=-0x1/0x10000000'
})-join "`n")+"`n"
$footprintPins=Get-MistwatchCurrentFootprintPins $footprintText
$footprintHeaders=@{};$footprintTyped=@()
foreach($pin in $footprintPins){
    $words=@($expected.fallen_pose_words['prop_'+$pin.id].Split(',')[0..14]|ForEach-Object{[uint32]$_})
    $parts=@(foreach($i in 0..4){(@($words[($i*3)..($i*3+2)]|ForEach-Object{Hex-Word $_})-join ',')})
    $header='PROP '+$pin.id+' original_anchor='+$parts[0]+' position='+$parts[0]+' rotationYXZ='+$parts[1]+' uniform_scale='+$parts[2]+
        ' new_conservative_box='+$parts[3]+' extent='+$parts[4]+' predecessorY='+(Hex-Word ($words[1]-1))
    $complete='COMPLETE source_faces=15269 nonempty_pairs=1 constraints=1 rational_words=1 final_min=0x0/0x1 final_max_all_surface=0x1/0x1 final_negative=0 predecessor_min=-0x1/0x10000000 lattice_evaluations=2 art=false lower_envelope_distribution_qualified=false'
    $footprintHeaders[$pin.id]=@($header,$complete)
    foreach($state in 0..1){$footprintTyped+='SOLID state='+$state+' index=0 role=3 role_id='+$pin.id+' '+$parts[3]+' extent='+$parts[4]}
}
foreach($pin in $footprintPins){Assert-MistwatchCurrentFootprintHeader $footprintHeaders[$pin.id][0] $footprintHeaders[$pin.id][1] $pin $expected $footprintTyped}
Reject 'footprint missing proof' {Get-MistwatchCurrentFootprintPins (($footprintText-split "`n"|Select-Object -Skip 1)-join "`n")}
Reject 'footprint duplicate ID' {Get-MistwatchCurrentFootprintPins ($footprintText.Replace('id=8 complete_proof=footprint-prop-8','id=0 complete_proof=footprint-prop-0'))}
foreach($pair in @(@('id=8','id=9'),@('footprint-prop-8.txt','../footprint-prop-8.txt'),@('footprint-prop-8.txt','footprint-prop-0.txt'),
    @('bytes=1','bytes=0'),@('bytes=1','bytes=33554433'),@('sha256='+('a'*64),'sha256='+('a'*63)),
    @('final_min=0x0','final_min=-0x1'),@('predecessor_min=-0x1','predecessor_min=0x1'),@('SEATED_PROP','seated_prop'))){
    Reject ('footprint declaration '+$pair[0]) {Get-MistwatchCurrentFootprintPins ($footprintText.Replace($pair[0],$pair[1]))}
}
$pin=$footprintPins[0];$header=$footprintHeaders[$pin.id][0];$complete=$footprintHeaders[$pin.id][1]
Reject 'footprint pose mismatch' {Assert-MistwatchCurrentFootprintHeader ($header.Replace(' position=',' position=0x1.0000000000000p+0,')) $complete $pin $expected $footprintTyped}
Reject 'footprint summary final_min mismatch' {Assert-MistwatchCurrentFootprintHeader $header ($complete.Replace('final_min=0x0/0x1','final_min=0x1/0x1')) $pin $expected $footprintTyped}
Reject 'footprint wrong source faces' {Assert-MistwatchCurrentFootprintHeader $header ($complete.Replace('source_faces=15269','source_faces=15268')) $pin $expected $footprintTyped}
Reject 'footprint negative contact' {Assert-MistwatchCurrentFootprintHeader $header ($complete.Replace('final_negative=0','final_negative=1')) $pin $expected $footprintTyped}
Reject 'footprint not minimal lattice' {Assert-MistwatchCurrentFootprintHeader $header ($complete.Replace('lattice_evaluations=2','lattice_evaluations=1')) $pin $expected $footprintTyped}
Reject 'footprint missing typed state' {Assert-MistwatchCurrentFootprintHeader $header $complete $pin $expected @($footprintTyped|Where-Object{$_ -cnotmatch '^SOLID state=1 .* role_id=0 '})}
Reject 'footprint duplicate typed log' {Assert-MistwatchCurrentFootprintHeader $header $complete $pin $expected @($footprintTyped+$footprintTyped[0])}
Reject 'footprint wrong typed box' {Assert-MistwatchCurrentFootprintHeader $header $complete $pin $expected @($footprintTyped|ForEach-Object{$_+'0'})}
$members=@('NativeExpected.json','NativeReceipt.json','WorldExpected.json','receipt.json','world-seat.txt','typed-worlds.txt',
    'footprint-prop-0.txt','footprint-prop-8.txt','footprint-prop-34.txt','footprint-prop-35.txt')
Assert-MistwatchCurrentCertificateMemberNames $members
Reject 'old six-file certificate' {Assert-MistwatchCurrentCertificateMemberNames $members[0..5]}
Reject 'arbitrary extra certificate file' {Assert-MistwatchCurrentCertificateMemberNames @($members+'extra.txt')}
Reject 'samecount arbitrary replacement' {Assert-MistwatchCurrentCertificateMemberNames @($members[0..8]+'extra.txt')}
Reject 'samecount duplicate footprint replacement' {Assert-MistwatchCurrentCertificateMemberNames @($members[0..8]+'footprint-prop-0.txt')}
$normal=@{debug_layer_enabled='0';gpu_validation_requested='0';gpu_validation_executed='0';debug_warnings='0'}
Assert-MistwatchCurrentGpuMode $normal $false
$normal.debug_layer_enabled='1';Assert-MistwatchCurrentGpuMode $normal $false
$gbv=@{debug_layer_enabled='1';gpu_validation_requested='1';gpu_validation_available='1';gpu_validation_executed='1';gpu_validation_passed='1';debug_warnings='0'}
Assert-MistwatchCurrentGpuMode $gbv $true
foreach($flag in @('gpu_validation_requested','gpu_validation_executed')){
    Reject ('normal wrongly GBV '+$flag) {$changed=Copy-Tree $normal;$changed[$flag]='1';Assert-MistwatchCurrentGpuMode $changed $false}
    Reject ('normal missing '+$flag) {$changed=Copy-Tree $normal;$changed.Remove($flag);Assert-MistwatchCurrentGpuMode $changed $false}
}
foreach($flag in @('debug_layer_enabled','gpu_validation_requested','gpu_validation_available','gpu_validation_executed','gpu_validation_passed')){
    foreach($value in @('0','2')){
        Reject ('GBV invalid '+$flag+' '+$value) {$changed=Copy-Tree $gbv;$changed[$flag]=$value;Assert-MistwatchCurrentGpuMode $changed $true}
    }
    Reject ('GBV missing '+$flag) {$changed=Copy-Tree $gbv;$changed.Remove($flag);Assert-MistwatchCurrentGpuMode $changed $true}
}
Reject 'GBV warning' {$changed=Copy-Tree $gbv;$changed.debug_warnings='1';Assert-MistwatchCurrentGpuMode $changed $true}
Reject 'GBV missing warnings' {$changed=Copy-Tree $gbv;$changed.Remove('debug_warnings');Assert-MistwatchCurrentGpuMode $changed $true}
if(-not $FootprintOnly){
    # 與正式入口共用純收據會計；子範圍不得覆寫外層 CPU 證書 fixture 或其拒絕計數。
    $standParentExpected=$expected|ConvertTo-Json -Depth 20 -Compress
    $standParentRejected=$script:Rejected
    & {
        $expected=@{current_extras=@{landscape_stand_added_population='96'}}
        $script:StandAccountingPositive=0;$script:StandAccountingNegative=0
        function Stand([string]$o,[string]$s,[string]$n,[string]$f,[string]$p,[string]$d){
            return @{accepted_slots='384';post_cull_opaque_slots=$o;post_cull_shadow_only_slots=$s;actual_draws=$d;
                near_twig_draws=$n;far_twig_draws=$f;transition_pairs=$p}
        }
        function Positive([string]$Label,$Fixture){
            $before=$Fixture|ConvertTo-Json -Depth 5 -Compress
            $eBefore=$expected|ConvertTo-Json -Depth 5 -Compress
            $result=Assert-MistwatchCurrentWorldStandAccounting $Fixture $expected
            if(($Fixture|ConvertTo-Json -Depth 5 -Compress) -cne $before -or
                ($expected|ConvertTo-Json -Depth 5 -Compress) -cne $eBefore){throw ('STAND_MUTATION:'+ $Label)}
            if($result.physical_draws -ne $result.visible_slots+$result.transition_pairs -or
               $result.non_twig_slots+$result.twig_slots -ne $result.visible_slots){throw ('STAND_RESULT:'+ $Label)}
            $script:StandAccountingPositive++
        }
        function Negative([string]$Label,[string]$Reason,[scriptblock]$Action){
            $actual=$null
            try{$null=& $Action}catch{$actual=$_.Exception.Message}
            if($actual -cne $Reason){throw ('STAND_NEGATIVE_REASON:'+ $Label+' expected=['+$Reason+'] actual=['+$actual+']')}
            $script:StandAccountingNegative++
        }
        $fixtures=@(
            @{name='fully-culled';o='0';s='0';n='0';f='0';p='0';d='0'},
            @{name='near-only';o='1';s='0';n='1';f='0';p='0';d='1'},
            @{name='far-only';o='1';s='0';n='0';f='1';p='0';d='1'},
            @{name='opaque-only-pair';o='1';s='0';n='1';f='1';p='1';d='2'},
            @{name='shadow-only-pair';o='0';s='1';n='1';f='1';p='1';d='2'},
            @{name='all96-pairs';o='96';s='0';n='96';f='96';p='96';d='192'},
            @{name='initial-equation';o='137';s='1';n='0';f='32';p='0';d='138'},
            @{name='repair-zero-shadow-pairs';o='128';s='0';n='3';f='32';p='3';d='131'},
            @{name='maximum480-physical';o='384';s='0';n='96';f='96';p='96';d='480'},
            @{name='all288-non-twig';o='288';s='0';n='0';f='0';p='0';d='288'},
            @{name='maximum384-far';o='384';s='0';n='0';f='96';p='0';d='384'},
            @{name='maximum384-shadow-near';o='0';s='384';n='96';f='0';p='0';d='384'},
            @{name='mixed-layers';o='100';s='12';n='10';f='12';p='5';d='117'},
            @{name='exact96-logical-twig';o='96';s='0';n='47';f='54';p='5';d='101'},
            @{name='all96-shadow-pairs';o='0';s='96';n='96';f='96';p='96';d='192'}
        )
        foreach($f in $fixtures){Positive $f.name (Stand $f.o $f.s $f.n $f.f $f.p $f.d)}
        $dictionary=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
        foreach($entry in (Stand '1' '0' '1' '1' '1' '2').GetEnumerator()){$dictionary.Add($entry.Key,$entry.Value)}
        Positive 'ordinal-dictionary-positive' $dictionary
        $ordinaryExpected=$expected
        $expected=@{current_extras=[ordered]@{landscape_stand_added_population='96'}}
        Positive 'existing-ordered-extras-positive' (Stand '128' '0' '3' '32' '3' '131')
        $expected=$ordinaryExpected
        $basis=Stand '128' '0' '3' '32' '3' '131'
        $keys=@($basis.Keys)
        $badValues=@(
            @{name='int';value=1},@{name='long';value=[long]1},@{name='null';value=$null},
            @{name='bool';value=$true},@{name='double';value=[double]1},@{name='array';value=@('1','2')},
            @{name='map';value=@{n='1'}},@{name='empty';value=''},@{name='negative';value='-1'},
            @{name='negative-zero';value='-0'},@{name='plus';value='+1'},@{name='leading-zero';value='01'},
            @{name='decimal';value='1.0'},@{name='exponent';value='1e0'},@{name='leading-space';value=' 1'},
            @{name='trailing-space';value='1 '},@{name='NaN';value='NaN'},
            @{name='uint64-overflow';value='18446744073709551616'}
        )
        foreach($key in $keys){
            Negative ('missing-'+$key) ('CURRENT_STAND_MISSING:'+$key) {
                $changed=$basis.Clone();$changed.Remove($key)
                Assert-MistwatchCurrentWorldStandAccounting $changed $expected
            }
            foreach($bad in $badValues){
                Negative ($key+'-'+$bad.name) ('CURRENT_STAND_INTEGER:'+$key) {
                    $changed=$basis.Clone();$changed[$key]=$bad.value
                    Assert-MistwatchCurrentWorldStandAccounting $changed $expected
                }
            }
            $shadow=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
            foreach($entry in $basis.GetEnumerator()){$shadow.Add($entry.Key,$entry.Value)}
            $shadow.Add($key.ToUpperInvariant(),$basis[$key])
            Negative ('case-shadow-'+$key) ('CURRENT_STAND_KEY_CASE:'+$key) {
                Assert-MistwatchCurrentWorldStandAccounting $shadow $expected
            }
        }
        $mutations=@(
            @{name='accepted-minus1';key='accepted_slots';value='383';reason='CURRENT_STAND_ACCEPTED'},
            @{name='accepted-plus1';key='accepted_slots';value='385';reason='CURRENT_STAND_ACCEPTED'},
            @{name='opaque385';key='post_cull_opaque_slots';value='385';reason='CURRENT_STAND_OPAQUE_RANGE'},
            @{name='shadow385';key='post_cull_shadow_only_slots';value='385';reason='CURRENT_STAND_SHADOW_RANGE'},
            @{name='draw481';key='actual_draws';value='481';reason='CURRENT_STAND_DRAWS_RANGE'},
            @{name='near97';key='near_twig_draws';value='97';reason='CURRENT_STAND_NEAR_RANGE'},
            @{name='far97';key='far_twig_draws';value='97';reason='CURRENT_STAND_FAR_RANGE'},
            @{name='pairs97';key='transition_pairs';value='97';reason='CURRENT_STAND_PAIR_RANGE'},
            @{name='pairs-one-above-near';key='transition_pairs';value='4';reason='CURRENT_STAND_PAIR_ENDPOINTS'},
            @{name='draw-minus1';key='actual_draws';value='130';reason='CURRENT_STAND_DRAW_EQUATION'},
            @{name='draw-plus1';key='actual_draws';value='132';reason='CURRENT_STAND_DRAW_EQUATION'},
            @{name='uint64-max-accepted';key='accepted_slots';value='18446744073709551615';reason='CURRENT_STAND_ACCEPTED'},
            @{name='uint64-max-draws';key='actual_draws';value='18446744073709551615';reason='CURRENT_STAND_DRAWS_RANGE'},
            @{name='uint64-max-pairs';key='transition_pairs';value='18446744073709551615';reason='CURRENT_STAND_PAIR_RANGE'}
        )
        foreach($m in $mutations){
            Negative $m.name $m.reason {
                $changed=$basis.Clone();$changed[$m.key]=$m.value
                Assert-MistwatchCurrentWorldStandAccounting $changed $expected
            }
        }
        foreach($case in @(
            @{name='pair-without-near';v=(Stand '1' '0' '0' '1' '1' '2');reason='CURRENT_STAND_PAIR_ENDPOINTS'},
            @{name='pair-without-far';v=(Stand '1' '0' '1' '0' '1' '2');reason='CURRENT_STAND_PAIR_ENDPOINTS'},
            @{name='logical-twig97';v=(Stand '97' '0' '96' '1' '0' '97');reason='CURRENT_STAND_TWIG_POPULATION'},
            @{name='logical-twig-no-visible';v=(Stand '0' '0' '1' '0' '0' '0');reason='CURRENT_STAND_TWIG_VISIBLE'},
            @{name='non-twig289';v=(Stand '289' '0' '0' '0' '0' '289');reason='CURRENT_STAND_NON_TWIG_RANGE'},
            @{name='visible385';v=(Stand '200' '185' '96' '96' '96' '480');reason='CURRENT_STAND_VISIBLE_RANGE'},
            @{name='fake-pair-to-absorb-extra-draw';v=(Stand '3' '0' '2' '2' '1' '5');reason='CURRENT_STAND_DRAW_EQUATION'}
        )){
            Negative $case.name $case.reason {Assert-MistwatchCurrentWorldStandAccounting $case.v $expected}
        }
        Negative 'container-not-map' 'CURRENT_STAND_CONTAINER' {Assert-MistwatchCurrentWorldStandAccounting 'bad' $expected}
        foreach($population in @('0','95','97','18446744073709551615')){
            Negative ('population-'+$population) 'CURRENT_STAND_POPULATION' {
                Assert-MistwatchCurrentWorldStandAccounting $basis @{current_extras=@{landscape_stand_added_population=$population}}
            }
        }
        foreach($bad in $badValues){
            Negative ('population-'+$bad.name) 'CURRENT_STAND_POPULATION_INTEGER' {
                Assert-MistwatchCurrentWorldStandAccounting $basis @{current_extras=@{landscape_stand_added_population=$bad.value}}
            }
        }
        if($script:StandAccountingPositive -ne 17 -or $script:StandAccountingNegative -ne 184){throw 'STAND_SYNTHETIC_COUNTS'}
        Write-Output ('CURRENT_WORLD_STAND_ACCOUNTING_CPU_PASS synthetic_positive='+$script:StandAccountingPositive+
            ' exact_reason_negative='+$script:StandAccountingNegative+' native=0 GPU=0 full49=0')

    }
    if($script:Rejected -ne $standParentRejected -or
        ($expected|ConvertTo-Json -Depth 20 -Compress) -cne $standParentExpected){throw 'STAND_SCOPE_PARENT_MUTATION'}
}
if(-not $FootprintOnly){
    $frameParentExpected=$expected|ConvertTo-Json -Depth 20 -Compress
    $frameParentRejected=$script:Rejected
    & {
        $script:FramePositive=0;$script:FrameNegative=0
        function FramePositive($Environment,[string]$Scene,$Receipt){
            $before=@($Environment,$Receipt)|ConvertTo-Json -Depth 8 -Compress
            Assert-MistwatchCurrentFrameCameraEnvironment $Environment $Scene $Receipt
            if((@($Environment,$Receipt)|ConvertTo-Json -Depth 8 -Compress) -cne $before){throw 'FRAME_INPUT_MUTATION'}
            $script:FramePositive++
        }
        function FrameNegative([string]$Reason,$Environment,[string]$Scene,$Receipt){
            $actual=$null
            try{Assert-MistwatchCurrentFrameCameraEnvironment $Environment $Scene $Receipt}catch{$actual=$_.Exception.Message}
            if($actual -cne $Reason){throw ('FRAME_NEGATIVE_REASON expected=['+$Reason+'] actual=['+$actual+']')}
            $script:FrameNegative++
        }
        $key='AEVRYND_MISTWATCH_CAPTURE_FRAME_CAMERA'
        $cases=@('initial','repair-shelter','repair-player-camera','repair-ready','river','river-overlook','strike',
            'boar-recovery','boar-profile','settlement-clue','settlement-clue-read','fuse-lock-wrong','fuse-lock-ready',
            'panel','powered','relay-west','relay-aligned','relay-linked','complete','audio-check','focus-pause',
            'boulder-profile','grass-profile','fern-profile','supply-profile','supply-collected','ranger-hut-profile',
            'trail-profile','shrub-profile','canopy-profile','canopy-root','canopy-detail')
        foreach($scene in $cases){
            $canopy=$scene -cin @('canopy-profile','canopy-root','canopy-detail')
            $receipt=@{camera_motion_source=$(if($canopy){'diagnostic_render_step'}else{'fixed_update'})}
            if($canopy){
                FramePositive @{$key='1'} $scene $receipt
                FrameNegative 'CURRENT_FRAME_CAMERA_ENABLED_VALUE' @{} $scene $receipt
                foreach($value in @($null,'','0','2',' 1','1 ',1,$true,@('1','2'),@{value='1'})){
                    FrameNegative 'CURRENT_FRAME_CAMERA_ENABLED_VALUE' @{$key=$value} $scene $receipt
                }
                foreach($bad in @(@{value=@()},@{value=@('1')})){
                    FrameNegative 'CURRENT_FRAME_CAMERA_ENABLED_VALUE' @{$key=$bad.value} $scene $receipt
                }
            }else{
                FramePositive @{} $scene $receipt
                FramePositive @{$key=$null} $scene $receipt
                FramePositive @{$key=''} $scene $receipt
                foreach($value in @('1','0',' ','2',0,1,$false,@('1','2'),@{value=''})){
                    FrameNegative 'CURRENT_FRAME_CAMERA_DISABLED_VALUE' @{$key=$value} $scene $receipt
                }
                foreach($bad in @(@{value=@()},@{value=@('')})){
                    FrameNegative 'CURRENT_FRAME_CAMERA_DISABLED_VALUE' @{$key=$bad.value} $scene $receipt
                }
            }
            $valid=@{};if($canopy){$valid[$key]='1'}
            FrameNegative 'Actual current-world field differs: camera_motion_source' $valid $scene @{}
            FrameNegative 'Actual current-world field differs: camera_motion_source' $valid $scene @{
                camera_motion_source=$(if($canopy){'fixed_update'}else{'diagnostic_render_step'})}
            $shadow=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
            $shadow.Add($key.ToLowerInvariant(),'1')
            FrameNegative 'CURRENT_FRAME_CAMERA_KEY_CASE' $shadow $scene $receipt
            $shadow.Add($key,'1')
            FrameNegative 'CURRENT_FRAME_CAMERA_KEY_CASE' $shadow $scene $receipt
            foreach($member in @('Keys','Values','Count')){
                # 資料 key 不得遮蔽 PowerShell 的字典成員，進而藏掉真正的相機開關。
                $memberShadow=@{$member=@()}
                $memberShadow[$key]=$(if($canopy){''}else{'1'})
                FrameNegative $(if($canopy){'CURRENT_FRAME_CAMERA_ENABLED_VALUE'}else{'CURRENT_FRAME_CAMERA_DISABLED_VALUE'}) $memberShadow $scene $receipt
                $caseShadow=[Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
                $caseShadow.Add($member,@());$caseShadow.Add($key.ToLowerInvariant(),'1')
                FrameNegative 'CURRENT_FRAME_CAMERA_KEY_CASE' $caseShadow $scene $receipt
                $positive=@{$member=@()};$positive[$key]=$(if($canopy){'1'}else{''})
                FramePositive $positive $scene $receipt
            }
        }
        FrameNegative 'CURRENT_FRAME_CAMERA_CONTAINER' 'not-a-map' 'initial' @{camera_motion_source='fixed_update'}
        FrameNegative 'CURRENT_FRAME_CAMERA_SCENE' @{} 'Initial' @{camera_motion_source='fixed_update'}
        if($script:FramePositive -ne 186 -or $script:FrameNegative -ne 680){throw ('FRAME_SYNTHETIC_COUNTS '+$script:FramePositive+'/'+$script:FrameNegative)}
        Write-Output ('CURRENT_FRAME_CAMERA_ENVIRONMENT_CPU_PASS synthetic_positive='+$script:FramePositive+
            ' exact_reason_negative='+$script:FrameNegative+' native=0 GPU=0 full49=0')
    }
    if($script:Rejected -ne $frameParentRejected -or
        ($expected|ConvertTo-Json -Depth 20 -Compress) -cne $frameParentExpected){throw 'FRAME_SCOPE_PARENT_MUTATION'}
}
$actualReaderInvoked=$false
if($SourceRoot -or $BuildRoot -or $CertificateRoot -or $ExecutablePath){
    if(-not($SourceRoot -and $BuildRoot -and $CertificateRoot -and $ExecutablePath)){throw 'All four real-reader arguments required together'}
    $actual=Read-MistwatchCurrentWorldCertificate -SourceRoot $SourceRoot -BuildRoot $BuildRoot -CertificateRoot $CertificateRoot -ExecutablePath $ExecutablePath
    Assert-MistwatchCurrentWorldExpected $actual
    $actualReaderInvoked=$true
}
Write-Output ('CURRENT_WORLD_RECEIPT_CPU_PASS synthetic_positive=3 footprint_positive=5 negative='+$script:Rejected+' footprint_only='+[bool]$FootprintOnly+' actual_reader_invoked='+$actualReaderInvoked+' native=0 GPU=0 full49=0')
