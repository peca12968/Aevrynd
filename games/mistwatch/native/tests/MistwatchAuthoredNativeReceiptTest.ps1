#requires -Version 7.0
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../tools/ForestSurfaceContract.ps1')
# 僅使用明確的合成物件測拒絕規則，不讀 PNG/Main 收據，不宣稱實際來源資格。
$script:Rejected=0
function Copy-Tree($Value) {
    if($Value -is [Collections.IDictionary]){
        $copy=[Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
        foreach($key in $Value.Keys){$copy.Add($key,(Copy-Tree $Value[$key]))}
        return ,$copy
    }
    if($Value -is [array]){return ,@(foreach($item in $Value){Copy-Tree $item})}
    return ,$Value
}
function Reject([string]$Label,[scriptblock]$Action) {
    $failed=$false
    try{& $Action}catch{$failed=$true}
    if(-not $failed){throw ('AUTHORED_NATIVE_RECEIPT_NEGATIVE_ACCEPTED: '+$Label)}
    $script:Rejected++
}
function Identity([string]$Path){return @{path=$Path;bytes=1;sha256=('A'*64)}}
# 版本必須由名稱明確選擇，不得依目錄數量猜測；舊版映射保持可驗證。
foreach($spec in @(@('habitat-full-v1',2,402),@('authored-nettle-full-v1',4,412),
    @('authored-mistwatch-only-v1',8,154),@('authored-boulder-full-v2',16,414),
    @('authored-boulder-mistwatch-only-v2',32,156),@('authored-leaf-cluster-full-v3',16,416),
    @('authored-forest-surface-full-v4',16,418),@('authored-current-forest-full-v5',16,418))) {
    Assert-MistwatchAuthoredNativeProfile $spec[0]
    $actual=Get-MistwatchAuthoredNativeProfileSpec $spec[0]
    if($actual.mask -ne $spec[1] -or $actual.count -ne $spec[2]){throw 'AUTHORED_PROFILE_MAPPING_CHANGED'}
}
foreach($invalid in @($null,1,'','auto','legacy-full-v1','authored-boulder-full-v1',
    'AUTHORED-BOULDER-FULL-V2',' authored-boulder-full-v2','authored-boulder-mistwatch-only-v2 ',
    'authored-leaf-cluster-full-v2','AUTHORED-LEAF-CLUSTER-FULL-V3',' authored-leaf-cluster-full-v3',
    'authored-leaf-cluster-full-v3 ','authored-leaf-cluster-mistwatch-only-v3')) {
    Reject 'explicit runtime profile required' { Assert-MistwatchAuthoredNativeProfile $invalid }
}
$memberKeys=@('count','Keys','Values')
# 額外讀取已編譯身分固定的部署 sidecar；不讀引擎影像或自填 runtime 預期。
$leafPath=Join-Path $PSScriptRoot 'qualification/runtime-leaf-cluster-full-v3.json'
$leafBytes=[IO.File]::ReadAllBytes($leafPath)
Assert-MistwatchLeafClusterManifestBytes $leafBytes
$leafRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$leafAdditions=Get-MistwatchLeafClusterRuntimeAdditions $leafRoot
if($leafAdditions.Count -ne 2){throw 'LEAF_MANIFEST_ADDITIONS_COUNT'}
$leafSeen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($entry in $leafAdditions) {
    if($entry.Count -ne 6 -or $entry[0] -isnot [string] -or $entry[1] -isnot [string] -or
       $entry[2] -ne 64 -or $entry[3] -cne 'copy' -or -not $leafSeen.Add($entry[0])) {
        throw 'LEAF_MANIFEST_ROW_SHAPE'
    }
}
foreach($index in @(0,[int]($leafBytes.Length/2),($leafBytes.Length-1))) {
    Reject ('leaf manifest byte '+$index) {
        $changed=[byte[]]$leafBytes.Clone();$changed[$index]=$changed[$index] -bxor 1
        Assert-MistwatchLeafClusterManifestBytes $changed
    }
}
Reject 'leaf manifest extra data' { Assert-MistwatchLeafClusterManifestBytes ([byte[]]($leafBytes+32)) }
Reject 'leaf manifest truncated' { Assert-MistwatchLeafClusterManifestBytes ([byte[]]$leafBytes[0..($leafBytes.Length-2)]) }
$leafText=[Text.Encoding]::UTF8.GetString($leafBytes)
foreach($word in @('schema_version','scope','profile','parent','parent_mapping_sha256','count','additions',
    '6377336','1790','23E72E7C','839D5C2A','games/mistwatch/assets/leaf-clusters/leaves-v1.aemesh',
    'games/mistwatch/assets/leaf-clusters/PROVENANCE.txt','mistwatch-leaf-clusters/leaves-v1.aemesh',
    'mistwatch-leaf-clusters/PROVENANCE.txt')) {
    if(-not $leafText.Contains($word)){throw 'LEAF_MANIFEST_NEGATIVE_TARGET_MISSING'}
    Reject ('leaf manifest field '+$word) {
        Assert-MistwatchLeafClusterManifestBytes ([Text.Encoding]::UTF8.GetBytes($leafText.Replace($word,('X'+$word))))
    }
}
Assert-MistwatchFallenExactKeys @{count='8853';Keys='data';Values='data'} $memberKeys 'reserved data member keys'
Reject 'reserved member extra cannot forge count' {
    Assert-MistwatchFallenExactKeys @{count=3;Keys='data';Values='data';extra='data'} $memberKeys 'reserved member extra'
}
Reject 'reserved member missing cannot forge count' {
    Assert-MistwatchFallenExactKeys @{count=3;Keys='data'} $memberKeys 'reserved member missing'
}
Reject 'reserved member exact case' {
    Assert-MistwatchFallenExactKeys @{Count=3;Keys='data';Values='data'} $memberKeys 'reserved member case'
}
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
Assert-MistwatchAuthoredNativeExpectedContract $expected
Assert-MistwatchAuthoredNativeReceiptContract $receipt $profile
Assert-MistwatchAuthoredNativeFields -Receipt $native -Expected $expected
Assert-MistwatchAuthoredWorldExpectedContract $world $expected
foreach($key in $fields){
    Reject ($key+' missing') {$changed=Copy-Tree $native;$changed.Remove($key);Assert-MistwatchAuthoredNativeFields $changed $expected}
    Reject ($key+' change') {$changed=Copy-Tree $native;$changed[$key]+='0';Assert-MistwatchAuthoredNativeFields $changed $expected}
    Reject ($key+' extra replacement') {$changed=Copy-Tree $native;$changed.Remove($key);$changed.Add($key+'_extra','1');Assert-MistwatchAuthoredNativeFields $changed $expected}
    Reject ($key+' integer type') {$changed=Copy-Tree $native;$changed[$key]=1;Assert-MistwatchAuthoredNativeFields $changed $expected}
}
foreach($pair in @(@('expected',$expected),@('receipt',$receipt),@('world',$world))){
    foreach($key in $pair[1].Keys){
        Reject ($pair[0]+' top missing '+$key) {
            $changed=Copy-Tree $pair[1];$changed.Remove($key)
            switch($pair[0]){expected{Assert-MistwatchAuthoredNativeExpectedContract $changed} receipt{Assert-MistwatchAuthoredNativeReceiptContract $changed $profile} world{Assert-MistwatchAuthoredWorldExpectedContract $changed $expected}}
        }
    }
    Reject ($pair[0]+' top extra') {
        $changed=Copy-Tree $pair[1];$changed.Add('unknown',1)
        switch($pair[0]){expected{Assert-MistwatchAuthoredNativeExpectedContract $changed} receipt{Assert-MistwatchAuthoredNativeReceiptContract $changed $profile} world{Assert-MistwatchAuthoredWorldExpectedContract $changed $expected}}
    }
}
foreach($path in @(@('schema'),@('world_cohort'),@('world_profile'),@('fallen_log_profile'),@('runtime_profile'),
    @('source','sha256'),@('source','bytes'),@('source','vertices'),@('source','indices'),@('source','triangles'))){
    Reject ('expected changed '+($path-join '.')) {
        $changed=Copy-Tree $expected;$node=$changed
        for($i=0;$i -lt $path.Count-1;$i++){$node=$node[$path[$i]]};$node[$path[-1]]='invalid'
        Assert-MistwatchAuthoredNativeExpectedContract $changed
    }
}
foreach($key in $expected.scope.Keys){Reject ('expected falseclaim '+$key) {$changed=Copy-Tree $expected;$changed.scope[$key]=-not $changed.scope[$key];Assert-MistwatchAuthoredNativeExpectedContract $changed}}
foreach($key in $receipt.scope.Keys){Reject ('receipt falseclaim '+$key) {$changed=Copy-Tree $receipt;$changed.scope[$key]=$true;Assert-MistwatchAuthoredNativeReceiptContract $changed $profile}}
foreach($key in $world.scope.Keys){Reject ('world falseclaim '+$key) {$changed=Copy-Tree $world;$changed.scope[$key]=-not $changed.scope[$key];Assert-MistwatchAuthoredWorldExpectedContract $changed $expected}}
foreach($key in $expected.fallen_pose_words.Keys){Reject ('pose changed '+$key) {$changed=Copy-Tree $expected;$changed.fallen_pose_words[$key]='1,';Assert-MistwatchAuthoredNativeExpectedContract $changed}}
foreach($key in $expected.cpu_identity.Keys){Reject ('CPU missing '+$key) {$changed=Copy-Tree $expected;$changed.cpu_identity.Remove($key);Assert-MistwatchAuthoredNativeExpectedContract $changed}}
foreach($key in $provenance.Keys){Reject ('provenance missing '+$key) {$changed=Copy-Tree $expected;$changed.provenance.Remove($key);Assert-MistwatchAuthoredNativeExpectedContract $changed}}
foreach($section in @('world','vegetation')){foreach($key in $world[$section].Keys){
    Reject ('world numeric change '+$section+'.'+$key) {$changed=Copy-Tree $world;$changed[$section][$key]++;Assert-MistwatchAuthoredWorldExpectedContract $changed $expected}
    Reject ('world numeric string '+$section+'.'+$key) {$changed=Copy-Tree $world;$changed[$section][$key]=[string]$changed[$section][$key];Assert-MistwatchAuthoredWorldExpectedContract $changed $expected}
}}
Reject 'legacy profile' {$changed=Copy-Tree $expected;$changed.runtime_profile='legacy-full-v1';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'legacy wrapper schema' {$changed=Copy-Tree $expected;$changed.schema='mistwatch.formal-fallen-native-expected.1';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'CPU world is not native expectation' {Assert-MistwatchAuthoredNativeExpectedContract $world}
Reject 'full volumes string' {$changed=Copy-Tree $expected;$changed.production_full_solid_volumes='true';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'raw bytes integer' {$changed=Copy-Tree $expected;$changed.source.bytes=434012;Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'negative uint' {$changed=Copy-Tree $expected;$changed.cpu_identity.terrain_fingerprint='-1';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'overflow uint' {$changed=Copy-Tree $expected;$changed.cpu_identity.terrain_fingerprint='18446744073709551616';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'leading zero' {$changed=Copy-Tree $expected;$changed.cpu_identity.terrain_fingerprint='01';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'world count legacy387' {$changed=Copy-Tree $expected;$changed.cpu_identity.initial_world.count='387';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'fixed scene fern mismatch' {$changed=Copy-Tree $expected;$changed.cpu_identity.fixed_scene_original_ferns.initial.triangles='11';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'fern source extra variant' {$changed=Copy-Tree $expected;$changed.cpu_identity.fern_source_triangles='1,2,3,4,5';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'CPU-native collider fingerprint mismatch' {$changed=Copy-Tree $expected;$changed.native_contract.forest_cover_clearance_collider_fingerprint='2';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'provenance boolean string' {$changed=Copy-Tree $expected;$changed.provenance.source_input_unchanged='true';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'provenance native claim' {$changed=Copy-Tree $expected;$changed.provenance.authored_world_qualified=$true;Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'source case shadow' {$changed=Copy-Tree $expected;$changed.provenance.source_files=@((Identity 'source.cpp'),(Identity 'SOURCE.cpp'));Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'source traversal' {$changed=Copy-Tree $expected;$changed.provenance.source_files[0].path='../source.cpp';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'source hash lowercase' {$changed=Copy-Tree $expected;$changed.provenance.source_files[0].sha256=('a'*64);Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'toolchain config' {$changed=Copy-Tree $expected;$changed.provenance.toolchain.configuration='Debug';Assert-MistwatchAuthoredNativeExpectedContract $changed}
Reject 'world provenance changed' {$changed=Copy-Tree $world;$changed.provenance.source_files[0].sha256=('B'*64);Assert-MistwatchAuthoredWorldExpectedContract $changed $expected}
Reject 'world native expectation claim' {$changed=Copy-Tree $world;$changed.scope.native_expected_author_passed=$true;Assert-MistwatchAuthoredWorldExpectedContract $changed $expected}
Reject 'receipt output path' {$changed=Copy-Tree $receipt;$changed.expected_file.path='../NativeExpected.json';Assert-MistwatchAuthoredNativeReceiptContract $changed $profile}
Reject 'receipt bytes string' {$changed=Copy-Tree $receipt;$changed.expected_file.bytes='1';Assert-MistwatchAuthoredNativeReceiptContract $changed $profile}
Reject 'receipt profile cross cohort' {$changed=Copy-Tree $receipt;$changed.runtime_profile='habitat-full-v1';Assert-MistwatchAuthoredNativeReceiptContract $changed $profile}
Reject 'receipt status' {$changed=Copy-Tree $receipt;$changed.status='native-passed';Assert-MistwatchAuthoredNativeReceiptContract $changed $profile}
Reject 'receipt flag string' {$changed=Copy-Tree $receipt;$changed.native_expected_author_passed='true';Assert-MistwatchAuthoredNativeReceiptContract $changed $profile}
Reject 'native extra diagnostic not projection' {$changed=Copy-Tree $native;$changed.Add('gpu_handle','1');Assert-MistwatchAuthoredNativeFields $changed $expected}
Reject 'native exact key case' {$changed=Copy-Tree $native;$changed.Remove('world_profile');$changed.Add('World_Profile','forest-wood');Assert-MistwatchAuthoredNativeFields $changed $expected}
Reject 'native unsafe path' {$changed=Copy-Tree $native;$changed.ground_art_base_color_path='C:\test.dds';Assert-MistwatchAuthoredNativeCanonicalMap $changed}
Reject 'native NaN' {$changed=Copy-Tree $native;$changed.ground_distance_normal_strength='NaN';Assert-MistwatchAuthoredNativeCanonicalMap $changed}
Reject 'native bool word2' {$changed=Copy-Tree $native;$changed.ground_meso_partition='2';Assert-MistwatchAuthoredNativeCanonicalMap $changed}
foreach($raw in @('{"a":1,"a":2}','{"a":1,"A":2}','{"a":{"b":1,"b":2}}','{"a":1,}')){
    Reject ('strict JSON '+$raw) {
        $document=[Text.Json.JsonDocument]::Parse($raw)
        try{Assert-MistwatchFallenJsonNode $document.RootElement 'synthetic'}finally{$document.Dispose()}
    }
}
Write-Output ('AUTHORED_NATIVE_RECEIPT_CPU_PASS; FIELDS='+$fields.Count+'; REJECTED='+$script:Rejected+'; SYNTHETIC_ONLY=1; NATIVE49=0')
# 地表獨立格式的合成正負控；不把這些物件當成真正 CPU／GPU 證書。
$surfaceStart=$script:Rejected
$surface=@{profile='forest-moss-leaf-metric-v1';ground_art_surface_path='moss-height/Ground037_SurfaceOrmHeight_Level0.png';
    ground_art_surface_bytes='1531546';ground_art_surface_sha256='012d2378aa6e62cca05ad66d69b512966d5894694b7b24d39afd0ca7ca655f38';
    ground_art_surface_encoding='2';ground_habitat_layer_surface_path='leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png';
    ground_habitat_layer_surface_bytes='9102955';ground_habitat_layer_surface_sha256='9f3259f4772f294edca1de03fa4e8d7ed65c0924238b1a6d0b27b8fed7c0ef85';
    ground_habitat_layer_surface_encoding='2';ground_rock_surface_encoding='0';height_layer_mask='3';ground_soil_layer_scale='1.75';
    ground_loaded_texture_identity='1';parallax_enabled='1';parallax_depth_meters='0.015';parallax_settings_fingerprint='1'}
Assert-MistwatchForestSurfaceFields $surface
foreach($key in @($surface.Keys)){
    Reject ('surface missing '+$key) {$bad=Copy-Tree $surface;$bad.Remove($key);Assert-MistwatchForestSurfaceFields $bad}
    Reject ('surface number '+$key) {$bad=Copy-Tree $surface;$bad[$key]=1;Assert-MistwatchForestSurfaceFields $bad}
    Reject ('surface invalid '+$key) {$bad=Copy-Tree $surface;$bad[$key]='invalid';Assert-MistwatchForestSurfaceFields $bad}
}
Reject 'surface extra' {$bad=Copy-Tree $surface;$bad.extra='1';Assert-MistwatchForestSurfaceFields $bad}
foreach($key in @('ground_loaded_texture_identity','parallax_settings_fingerprint')){
    foreach($invalid in @('0','01','-1','18446744073709551616','NaN')){
        Reject ('surface identity '+$key+' '+$invalid) {$bad=Copy-Tree $surface;$bad[$key]=$invalid;Assert-MistwatchForestSurfaceFields $bad}
    }
}
$scope=@{runtime_identity_only=$true;surface_recipe_CPU_passed=$true;current_geometry_qualified=$false;native_qualified=$false;
    native_executed=$false;GPU_qualified=$false;full49_qualified=$false;actual_Main_route_qualified=$false;art_accepted=$false;manual_play=$false;published=$false}
Assert-MistwatchForestSurfaceScope $scope
foreach($key in @($scope.Keys)){
    Reject ('surface scope inversion '+$key) {$bad=Copy-Tree $scope;$bad[$key]=-not $bad[$key];Assert-MistwatchForestSurfaceScope $bad}
    Reject ('surface scope string '+$key) {$bad=Copy-Tree $scope;$bad[$key]='false';Assert-MistwatchForestSurfaceScope $bad}
}
$sp=Copy-Tree $provenance;$sp.schema='mistwatch.forest-surface.cpu-provenance.1';$sp.world_cohort='entrance-authored-surface-only-v1'
$sp.runtime_profile='authored-forest-surface-full-v4';$sp.generated_root_stem_override='C:\Synthetic\Build\generated-root-stem';$sp.current_geometry_qualified=$false
$se=@{schema='mistwatch.forest-surface.cpu-expected.1';world_cohort='entrance-authored-surface-only-v1';runtime_profile='authored-forest-surface-full-v4';
    surface_contract=$surface;provenance=$sp;scope=$scope}
Assert-MistwatchForestSurfaceExpected $se
foreach($key in @('schema','world_cohort','runtime_profile')){
    Reject ('surface expected identity '+$key) {$bad=Copy-Tree $se;$bad[$key]='wrong';Assert-MistwatchForestSurfaceExpected $bad}
    Reject ('surface provenance identity '+$key) {$bad=Copy-Tree $se;$bad.provenance[$key]='wrong';Assert-MistwatchForestSurfaceExpected $bad}
}
Reject 'surface geometry claim' {$bad=Copy-Tree $se;$bad.provenance.current_geometry_qualified=$true;Assert-MistwatchForestSurfaceExpected $bad}
Reject 'surface closure shadow' {$bad=Copy-Tree $se;$bad.provenance.source_files=@((Identity 'source.cpp'),(Identity 'SOURCE.cpp'));Assert-MistwatchForestSurfaceExpected $bad}
Reject 'surface cannot reuse old153' {$bad=Copy-Tree $expected;$bad.runtime_profile='authored-forest-surface-full-v4';Assert-MistwatchAuthoredNativeExpectedContract $bad}
$surfaceBytes=[IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'qualification/runtime-forest-surface-full-v4.json'))
Assert-MistwatchForestSurfaceManifestBytes $surfaceBytes
foreach($index in @(0,[int]($surfaceBytes.Length/2),($surfaceBytes.Length-1))){
    Reject ('surface manifest byte '+$index) {$bad=[byte[]]$surfaceBytes.Clone();$bad[$index]=$bad[$index] -bxor 1;Assert-MistwatchForestSurfaceManifestBytes $bad}
}
Reject 'surface manifest extra' {Assert-MistwatchForestSurfaceManifestBytes ([byte[]]($surfaceBytes+32))}
Reject 'surface manifest short' {Assert-MistwatchForestSurfaceManifestBytes ([byte[]]$surfaceBytes[0..($surfaceBytes.Length-2)])}
foreach($invalid in @('AUTHORED-FOREST-SURFACE-FULL-V4',' authored-forest-surface-full-v4','authored-forest-surface-full-v4 ',
    'authored-forest-surface-full-v3','authored-forest-surface-mistwatch-only-v4')){
    Reject 'surface profile exact' {Assert-MistwatchAuthoredNativeProfile $invalid}
}
Write-Output ('FOREST_SURFACE_RECEIPT_CPU_PASS; FIELDS=16; NEGATIVES='+($script:Rejected-$surfaceStart)+'; SYNTHETIC_ONLY=1; NATIVE49=0')
$currentStart=$script:Rejected
$currentBytes=[IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'qualification/runtime-current-forest-full-v5.json'))
Assert-MistwatchCurrentForestManifestBytes $currentBytes
foreach($index in @(0,[int]($currentBytes.Length/2),($currentBytes.Length-1))){
    Reject ('current manifest byte '+$index) {$bad=[byte[]]$currentBytes.Clone();$bad[$index]=$bad[$index] -bxor 1;Assert-MistwatchCurrentForestManifestBytes $bad}
}
Reject 'current manifest extra' {Assert-MistwatchCurrentForestManifestBytes ([byte[]]($currentBytes+32))}
Reject 'current manifest short' {Assert-MistwatchCurrentForestManifestBytes ([byte[]]$currentBytes[0..($currentBytes.Length-2)])}
foreach($invalid in @('AUTHORED-CURRENT-FOREST-FULL-V5',' authored-current-forest-full-v5','authored-current-forest-full-v5 ',
    'authored-current-forest-full-v4','authored-current-forest-only-v5')){
    Reject 'current profile exact' {Assert-MistwatchAuthoredNativeProfile $invalid}
}
Reject 'current runtime cannot borrow old153' {$bad=Copy-Tree $expected;$bad.runtime_profile='authored-current-forest-full-v5';Assert-MistwatchAuthoredNativeExpectedContract $bad}
Write-Output ('CURRENT_RUNTIME_IDENTITY_CPU_PASS; NEGATIVES='+($script:Rejected-$currentStart)+'; CURRENT_WORLD=0; NATIVE49=0')
