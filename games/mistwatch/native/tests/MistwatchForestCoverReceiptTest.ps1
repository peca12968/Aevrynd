$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../tools/forest_cover_contract.ps1')
# 此為合成收據的負向契約單測，不是實機或畫質證據。
$valid=@{
    world_profile='legacy-core'
    ground_cover_instances='3600';forest_cover_mode='fixed_cluster_patches'
    forest_cover_seed='1129272901';forest_cover_authored_population='3600'
    forest_cover_maximum_patches='400';forest_cover_patch_minimum_plants='20'
    forest_cover_patch_maximum_plants='32';forest_cover_independent_anchors='900'
    forest_cover_patches='165';forest_cover_reached_budget='1'
        forest_cover_fingerprint='11564884043970395063';forest_cover_clearance_colliders='221'
        forest_cover_clearance_collider_fingerprint='5549580620105119187'
        forest_cover_variants='927,782,615,543,733'
        fern_colony_population='300';fern_colony_authored_population='300';fern_colonies='84'
    fern_colony_seed='1128681010';fern_colony_reached_budget='1'
        fern_colony_fingerprint='6509400757303582109'
        fern_colony_original_fingerprint='3085592665785500013'
        fern_colony_variants='11,23,183,83';understory_ferns='420';understory_saplings='42'
    submitted_grass_variants='1,1,1,1,1';submitted_grass='5';submitted_grass_triangles='7842'
    submitted_colony_variants='1,1,1,1';submitted_colony_ferns='4';submitted_colony_fern_triangles='6232'
    submitted_ferns='5';submitted_fern_triangles='8616'
    submitted_original_ferns='1';submitted_original_fern_triangles='2384';submitted_original_fern_variants='1,0,0,0'
    middle_layer_comparison='0';middle_layer_population='0';middle_layer_target='0'
    middle_layer_reached_budget='1';middle_layer_attempted='0';middle_layer_transform_fingerprint='0'
    submitted_middle_layer='0';submitted_middle_layer_triangles='0'
}
Assert-MistwatchForestCoverReceipt $valid
$script:rejected=0
function Assert-Rejected([hashtable]$Receipt,[string]$Case) {
    $failed=$false
    try {Assert-MistwatchForestCoverReceipt $Receipt}catch{$failed=$true}
    if(-not $failed){throw ('Invalid receipt accepted: '+$Case)}
    ++$script:rejected
}
foreach($field in @($valid.Keys)) {
    $bad=$valid.Clone();$bad.Remove($field)
    Assert-Rejected $bad ('missing '+$field)
}
foreach($change in @(
    @{field='ground_cover_instances';value='1363'},
    @{field='forest_cover_mode';value='original_scatter'},
    @{field='forest_cover_variants';value='940,745,629,563,723'},
    @{field='fern_colony_variants';value='75,75,75,75'},
    @{field='fern_colony_population';value='299'},
    @{field='fern_colony_reached_budget';value='0'},
    @{field='fern_colony_original_fingerprint';value='0'},
    @{field='understory_ferns';value='720'},
    @{field='submitted_grass_variants';value='1,1,1,1'},
    @{field='submitted_grass_variants';value='-1,1,1,1,1'},
    @{field='submitted_grass_variants';value='NaN,1,1,1,1'},
    @{field='submitted_grass_variants';value='01,1,1,1,1'},
    @{field='submitted_grass_variants';value='999999999999999999999,1,1,1,1'},
    @{field='submitted_grass_variants';value='942,1,1,1,1'},
    @{field='submitted_grass_variants';value='0,2,1,1,1'},
    @{field='submitted_colony_variants';value='1,1,1,1,0'},
    @{field='submitted_colony_variants';value='14,1,1,1'},
    @{field='submitted_colony_variants';value='0,2,1,1'},
    @{field='submitted_colony_fern_triangles';value='6231'},
    @{field='submitted_colony_ferns';value='3'},
    @{field='submitted_ferns';value='3'},
    @{field='submitted_ferns';value='425'},
    @{field='submitted_fern_triangles';value='6231'},
    @{field='submitted_fern_triangles';value='NaN'},
    @{field='submitted_fern_triangles';value='8617'}
    @{field='submitted_original_fern_variants';value='0,1,0,0'},
    @{field='submitted_original_fern_variants';value='106,0,0,0'}
)) {
    $bad=$valid.Clone();$bad[$change.field]=$change.value
    Assert-Rejected $bad ($change.field+'='+$change.value)
}
$distant=$valid.Clone();$distant.submitted_colony_variants='0,0,0,0'
$distant.submitted_colony_ferns='0';$distant.submitted_colony_fern_triangles='0'
$distant.submitted_ferns='1';$distant.submitted_fern_triangles='2384'
Assert-MistwatchForestCoverReceipt $distant
$omitted=$valid.Clone();$omitted.submitted_ferns='4';$omitted.submitted_fern_triangles='6232'
$omitted.submitted_original_ferns='0';$omitted.submitted_original_fern_triangles='0'
$omitted.submitted_original_fern_variants='0,0,0,0'
Assert-Rejected $omitted 'all original ferns omitted but colony kept'
foreach($scene in @('initial','river-overlook')) {
    $sceneFailed=$false
    try {Assert-MistwatchForestCoverReceipt $valid -Scene $scene}catch{$sceneFailed=$true}
    if(-not $sceneFailed){throw ('Fixed scene original submission drift accepted: '+$scene)}
    ++$script:rejected
}
'FOREST_COVER_RECEIPT_TEST_PASS; VALID=2; REJECTED='+$script:rejected

# 中層候選是不同世界配置；保留上方舊契約的全部65反例，不自動猜選版本。
$middle=$valid.Clone()
$middleFields=@{
    world_profile='legacy-middle'
            forest_cover_patches='154';forest_cover_fingerprint='8857932304976401347'
            forest_cover_clearance_colliders='245';forest_cover_clearance_collider_fingerprint='11739939804215987065'
            forest_cover_variants='871,751,625,568,785';fern_colonies='86'
            fern_colony_fingerprint='3313014129975770240';fern_colony_variants='6,37,170,87'
    middle_layer_comparison='1';middle_layer_population='24';middle_layer_target='24'
            middle_layer_reached_budget='1';middle_layer_attempted='19887'
            middle_layer_transform_fingerprint='11352586881239724297'
    submitted_middle_layer='24';submitted_middle_layer_triangles='1988496'
}
foreach($field in $middleFields.Keys){$middle[$field]=$middleFields[$field]}
Assert-MistwatchForestCoverReceipt $middle -MiddleLayer
# 各欄位及整組中層資料污染都必須在 baseline 模式拒絕，不讓舊指紋遮住多畫的樹。
foreach($field in @('middle_layer_comparison','middle_layer_population','middle_layer_target',
    'middle_layer_reached_budget','middle_layer_attempted','middle_layer_transform_fingerprint',
    'submitted_middle_layer','submitted_middle_layer_triangles')){
    $bad=$valid.Clone();$bad[$field]=$(if($field -eq 'middle_layer_reached_budget'){'0'}else{'1'})
    Assert-Rejected $bad ('baseline polluted '+$field)
}
$polluted=$valid.Clone()
foreach($field in @('middle_layer_comparison','middle_layer_population','middle_layer_target',
    'middle_layer_reached_budget','middle_layer_attempted','middle_layer_transform_fingerprint',
    'submitted_middle_layer','submitted_middle_layer_triangles')){$polluted[$field]=$middleFields[$field]}
Assert-Rejected $polluted 'baseline old grass/fern fingerprints plus all middle data'
foreach($value in @('00','-1','NaN','0.0','024')){
    $bad=$valid.Clone();$bad.middle_layer_population=$value;Assert-Rejected $bad ('noncanonical baseline population '+$value)
}
'BASELINE_MIDDLE_LAYER_REJECTION_PASS; EXTRA_NEGATIVE_CASES=14; TOTAL_BASELINE_REJECTED='+$script:rejected
$middleRejected=0
function Assert-MiddleRejected([hashtable]$Receipt,[string]$Case) {
    $failed=$false
    try {Assert-MistwatchForestCoverReceipt $Receipt -MiddleLayer}catch{$failed=$true}
    if(-not $failed){throw ('Invalid middle receipt accepted: '+$Case)}
    ++$script:middleRejected
}
foreach($field in @($middle.Keys)){
    $bad=$middle.Clone();$bad.Remove($field);Assert-MiddleRejected $bad ('missing '+$field)
}
foreach($field in $middleFields.Keys){
    $bad=$middle.Clone();$bad[$field]='0';Assert-MiddleRejected $bad ('zero '+$field)
}
Assert-MiddleRejected $valid 'old world passed as middle-layer candidate'
foreach($change in @(
    @{field='submitted_grass_variants';value='975,1,1,1,1'},
    @{field='submitted_colony_variants';value='11,1,1,1'},
    @{field='submitted_original_fern_variants';value='0,0,0,0'},
    @{field='submitted_middle_layer';value='23'},
    @{field='submitted_middle_layer_triangles';value='1988495'},
    @{field='middle_layer_population';value='024'},
    @{field='middle_layer_transform_fingerprint';value='9091870748330489573'},
    @{field='forest_cover_clearance_colliders';value='221'},
    @{field='forest_cover_fingerprint';value='18346272358334413870'},
    @{field='fern_colony_fingerprint';value='18387932991795706967'},
    @{field='submitted_colony_fern_triangles';value='6231'},
    @{field='submitted_fern_triangles';value='8617'}
)){
    $bad=$middle.Clone();$bad[$change.field]=$change.value;Assert-MiddleRejected $bad $change.field
}
$wrongMode=$false;try{Assert-MistwatchForestCoverReceipt $middle}catch{$wrongMode=$true}
if(-not $wrongMode){throw 'Middle world accepted without explicit mode'}
'MIDDLE_LAYER_RECEIPT_TEST_PASS; VALID=1; REJECTED='+$middleRejected+'; MODE_MIX_REJECTED=1'

# 同總數、同三角數的種類別名不得繞過固定景檢查；兩種世界皆驗正反例。
$fixedPositive=0;$aliasRejected=0
foreach($useMiddle in @($false,$true)){
    foreach($scene in @('initial','river-overlook')){
        $fixed=if($useMiddle){$middle.Clone()}else{$valid.Clone()}
        $count=if($scene -eq 'initial'){192}else{226}
        $triangles=if($scene -eq 'initial'){298968}else{350104}
        $fixed.submitted_original_ferns=[string]$count
        $fixed.submitted_original_fern_triangles=[string]$triangles
        $fixed.submitted_original_fern_variants=if($scene -eq 'initial'){'47,49,49,47'}else{'51,61,51,63'}
        $fixed.submitted_ferns=[string]($count+[long]$fixed.submitted_colony_ferns)
        $fixed.submitted_fern_triangles=[string]($triangles+[long]$fixed.submitted_colony_fern_triangles)
        Assert-MistwatchForestCoverReceipt $fixed -Scene $scene -MiddleLayer:$useMiddle
        ++$fixedPositive
        $alias=$fixed.Clone()
        $alias.submitted_original_fern_variants=if($scene -eq 'initial'){'51,45,66,30'}else{'55,57,68,46'}
        $identityRejected=$false
        try{Assert-MistwatchForestCoverReceipt $alias -Scene $scene -MiddleLayer:$useMiddle}catch{
            if($_.Exception.Message -eq 'Original fern fixed-scene variant identity changed'){$identityRejected=$true}
        }
        if(-not $identityRejected){throw 'Equal-count/equal-triangle variant alias accepted'}
        ++$aliasRejected
    }
}
'FIXED_SCENE_VARIANT_IDENTITY_PASS; POSITIVE='+$fixedPositive+'; ALIAS_REJECTED='+$aliasRejected

# 舊作者內容不能被新地形當作相同配置；每欄獨立拒絕，原反例全部保留。
foreach($change in @(
    @{field='forest_cover_fingerprint';value='18346272358334413870'},
    @{field='forest_cover_clearance_collider_fingerprint';value='522388356439590700'},
    @{field='forest_cover_variants';value='900,764,672,563,701'},
    @{field='fern_colonies';value='82'},
    @{field='fern_colony_fingerprint';value='18387932991795706967'},
    @{field='fern_colony_original_fingerprint';value='14418781708182382769'},
    @{field='fern_colony_variants';value='11,29,169,91'}
)){$bad=$valid.Clone();$bad[$change.field]=$change.value;Assert-Rejected $bad ('old authored '+$change.field)}
foreach($change in @(
    @{field='forest_cover_patches';value='162'},
    @{field='forest_cover_fingerprint';value='1048722341261312903'},
    @{field='forest_cover_clearance_collider_fingerprint';value='6744664032273563734'},
    @{field='forest_cover_variants';value='964,741,630,546,719'},
    @{field='fern_colonies';value='79'},
    @{field='fern_colony_fingerprint';value='3053818525289996684'},
    @{field='fern_colony_variants';value='10,28,173,89'},
    @{field='middle_layer_attempted';value='16999'},
    @{field='middle_layer_transform_fingerprint';value='12515088988881324841'}
)){$bad=$middle.Clone();$bad[$change.field]=$change.value;Assert-MiddleRejected $bad ('old authored '+$change.field)}
'EROSION_AUTHOR_REBASE_REJECTION_PASS; OLD_BASELINE=7; OLD_MIDDLE=9'

# NaN 的 abs 比較不會超出容差，須先明確拒絕非有限值；保留原有容差。
foreach($marker in @('-15.46875','-15.4688')){
    Assert-MistwatchTerrainCoverageMarker @{ground_normal_scale=$marker}
}
$markerRejected=0
foreach($marker in @('NaN','Infinity','-Infinity','1e999','-1e999','','bad','0','-15.5','-15.4685',$null)){
    $failed=$false
    try{Assert-MistwatchTerrainCoverageMarker @{ground_normal_scale=$marker}}catch{$failed=$true}
    if(-not $failed){throw 'Invalid terrain coverage marker accepted'}
    ++$markerRejected
}
$failed=$false
try{Assert-MistwatchTerrainCoverageMarker @{}}catch{$failed=$true}
if(-not $failed){throw 'Missing terrain coverage marker accepted'}
$markerRejected+=1
'TERRAIN_COVERAGE_MARKER_RECEIPT_PASS; POSITIVE=2; REJECTED='+$markerRejected

# 新世界的作者黃金值來自 V85 三世界封存與 V75 獨立 CPU；保留上方全部舊草蕨反例。
$canopy=$middle.Clone()
$canopyFields=@{
    world_profile='forest-canopy';forest_cover_patches='168';forest_cover_fingerprint='4989138190905410642'
    forest_cover_clearance_colliders='249';forest_cover_clearance_collider_fingerprint='16643660641049883501'
    forest_cover_variants='941,762,617,522,758';fern_colonies='85'
    fern_colony_fingerprint='5579852658837856591';fern_colony_variants='14,29,168,89'
}
foreach($field in $canopyFields.Keys){$canopy[$field]=$canopyFields[$field]}
Assert-MistwatchForestCoverReceipt $canopy -WorldProfile forest-canopy
$canopyRejected=0
function Assert-CanopyRejected([hashtable]$Receipt,[string]$Case){
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $Receipt -WorldProfile forest-canopy}catch{$failed=$true}
    if(-not $failed){throw ('Invalid canopy cover accepted: '+$Case)}
    ++$script:canopyRejected
}
Assert-CanopyRejected $valid 'legacy core passed as canopy'
Assert-CanopyRejected $middle 'legacy middle passed as canopy'
foreach($profile in @('legacy-core','legacy-middle')){
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $canopy -WorldProfile $profile}catch{$failed=$true}
    if(-not $failed){throw ('Canopy authored world accepted as '+$profile)}
    ++$canopyRejected
}
$failed=$false
try{Assert-MistwatchForestCoverReceipt $canopy -WorldProfile forest-canopy -MiddleLayer}catch{$failed=$true}
if(-not $failed){throw 'Conflicting world and middle selector accepted'}
++ $canopyRejected
foreach($field in @($canopy.Keys)){
    $bad=$canopy.Clone();$bad.Remove($field);Assert-CanopyRejected $bad ('missing '+$field)
}
foreach($field in $canopyFields.Keys){
    $bad=$canopy.Clone();$bad[$field]=$middle[$field];Assert-CanopyRejected $bad ('legacy middle '+$field)
}
foreach($change in @(
    @{field='submitted_grass_variants';value='942,1,1,1,1'},
    @{field='submitted_colony_variants';value='15,1,1,1'},
    @{field='submitted_original_fern_variants';value='0,0,0,0'},
    @{field='submitted_fern_triangles';value='8617'},
    @{field='middle_layer_population';value='23'},
    @{field='forest_cover_clearance_collider_fingerprint';value='15105317612767713774'}
)){$bad=$canopy.Clone();$bad[$change.field]=$change.value;Assert-CanopyRejected $bad $change.field}
foreach($scene in @('initial','river-overlook')){
    $fixed=$canopy.Clone()
    $count=if($scene -eq 'initial'){192}else{226}
    $triangles=if($scene -eq 'initial'){298968}else{350104}
    $fixed.submitted_original_ferns=[string]$count;$fixed.submitted_original_fern_triangles=[string]$triangles
    $fixed.submitted_original_fern_variants=if($scene -eq 'initial'){'47,49,49,47'}else{'51,61,51,63'}
    $fixed.submitted_ferns=[string]($count+[long]$fixed.submitted_colony_ferns)
    $fixed.submitted_fern_triangles=[string]($triangles+[long]$fixed.submitted_colony_fern_triangles)
    Assert-MistwatchForestCoverReceipt $fixed -WorldProfile forest-canopy -Scene $scene
    $fixed.submitted_original_fern_variants=if($scene -eq 'initial'){'51,45,66,30'}else{'55,57,68,46'}
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $fixed -WorldProfile forest-canopy -Scene $scene}catch{
        $failed=$_.Exception.Message -eq 'Original fern fixed-scene variant identity changed'
    }
    if(-not $failed){throw ('Canopy fixed-scene variant alias accepted: '+$scene)}
    ++$canopyRejected
}
'CANOPY_FOREST_COVER_RECEIPT_PASS; POSITIVE=3; REJECTED='+$canopyRejected

# 舊三世界的歷史收據可缺新增欄；若新收據提供欄位，必須是完整 Legacy 診斷且拒絕污染。
$legacyFirFields=@{
    fir_layout='legacy';fir_layout_population='100';fir_layout_reached_budget='1'
    fir_layout_fingerprint='14695981039346656037';fir_layout_patch_centres='0'
}
$legacyFirPositive=0;$legacyFirRejected=0
foreach($case in @(
    @{profile='legacy-core';receipt=$valid},@{profile='legacy-middle';receipt=$middle},
    @{profile='forest-canopy';receipt=$canopy})) {
    $annotated=$case.receipt.Clone()
    foreach($field in $legacyFirFields.Keys){$annotated[$field]=$legacyFirFields[$field]}
    Assert-MistwatchForestCoverReceipt $annotated -WorldProfile $case.profile
    ++$legacyFirPositive
    foreach($change in @(
        @{field='fir_layout';value='forest-wood-mature'},@{field='fir_layout_population';value='99'},
        @{field='fir_layout_reached_budget';value='0'},@{field='fir_layout_fingerprint';value='8236695966185867318'},
        @{field='fir_layout_patch_centres';value='24'})) {
        $bad=$annotated.Clone();$bad[$change.field]=$change.value;$failed=$false
        try{Assert-MistwatchForestCoverReceipt $bad -WorldProfile $case.profile}catch{$failed=$true}
        if(-not $failed){throw ('Legacy fir layout pollution accepted: '+$case.profile+' '+$change.field)}
        ++$legacyFirRejected
    }
}
'LEGACY_FIR_LAYOUT_RECEIPT_PASS; POSITIVE='+$legacyFirPositive+'; REJECTED='+$legacyFirRejected

# 每個世界逐欄拒絕缺漏、污染與只關繪圖；同時驗資源清理，不以畫面空白當證據。
$lifecycleRejected=0;$lifecyclePositive=0
function Assert-LifecycleRejected([hashtable]$Receipt,[hashtable]$Cleanup,[string]$Profile,[string]$Case,[string]$Scene=''){
    $failed=$false
    try{Assert-MistwatchWorldProfileReceipt $Receipt $Cleanup $Profile -Scene $Scene}catch{$failed=$true}
    if(-not $failed){throw ('Invalid profile lifecycle accepted: '+$Profile+' '+$Case)}
    ++$script:lifecycleRejected
}
foreach($profile in @('legacy-core','legacy-middle','forest-canopy')){
    $withCanopy=$profile -eq 'forest-canopy'
    $receipt=@{
        world_profile=$profile;canopy_preview_fixture='0'
        canopy_world_profile=$(if($withCanopy){'1'}else{'0'});canopy_enabled=$(if($withCanopy){'1'}else{'0'})
        canopy_plants=$(if($withCanopy){'4'}else{'0'});canopy_rooted_meshes=$(if($withCanopy){'4'}else{'0'})
        canopy_population_fingerprint=$(if($withCanopy){'13093722598475582899'}else{'0'})
        canopy_ground_checks=$(if($withCanopy){'12121992'}else{'0'})
        canopy_primitive_triangles=$(if($withCanopy){'606207'}else{'0'})
        canopy_materials=$(if($withCanopy){'3'}else{'0'});canopy_submitted_slots=$(if($withCanopy){'12'}else{'0'})
        forest_cover_clearance_colliders=$(if($withCanopy){'249'}elseif($profile -eq 'legacy-core'){'221'}else{'245'})
        shrubs_enabled='1';shrubs_plants='64';shrubs_submitted='64'
        shrubs_fingerprint=$(if($withCanopy){'5849093981174083644'}elseif($profile -eq 'legacy-core'){'4812794541849057446'}else{'18032575485754629455'})
    }
    $cleanup=@{cleanup='success';materials_remaining='0';mesh_destroy_calls=$(if($withCanopy){'7'}else{'0'})}
    Assert-MistwatchWorldProfileReceipt $receipt $cleanup $profile
    ++$lifecyclePositive
    foreach($wrongProfile in @('legacy-core','legacy-middle','forest-canopy') | Where-Object {$_ -ne $profile}){
        Assert-LifecycleRejected $receipt $cleanup $wrongProfile 'profile selection mismatch'
    }
    foreach($field in @($receipt.Keys)){
        $bad=$receipt.Clone();$bad.Remove($field);Assert-LifecycleRejected $bad $cleanup $profile ('missing '+$field)
        $bad=$receipt.Clone();$bad[$field]='invalid';Assert-LifecycleRejected $bad $cleanup $profile ('invalid '+$field)
    }
    foreach($field in @($cleanup.Keys)){
        $bad=$cleanup.Clone();$bad.Remove($field);Assert-LifecycleRejected $receipt $bad $profile ('missing cleanup '+$field)
        $bad=$cleanup.Clone();$bad[$field]='invalid';Assert-LifecycleRejected $receipt $bad $profile ('invalid cleanup '+$field)
    }
    foreach($value in @('00','-1','NaN','0.0','04','3','5')){
        $bad=$receipt.Clone();$bad.canopy_plants=$value;Assert-LifecycleRejected $bad $cleanup $profile ('population '+$value)
    }
    if($withCanopy){
        foreach($field in @('canopy_enabled','canopy_submitted_slots','canopy_materials','canopy_rooted_meshes')){
            $bad=$receipt.Clone();$bad[$field]='0';Assert-LifecycleRejected $bad $cleanup $profile ('draw/resource omitted '+$field)
        }
        foreach($scene in @('canopy-profile','canopy-root','canopy-detail')){
            $camera=$receipt.Clone();$camera.camera_source='diagnostic';$camera.camera_target='0,0,0'
            $camera.camera_position=if($scene -eq 'canopy-root'){'1.41421356,0.05,-1.41421356'}elseif($scene -eq 'canopy-detail'){'4.00010156,0.8,-4.00010156'}else{'0,0.5,-8.5'}
            Assert-MistwatchWorldProfileReceipt $camera $cleanup $profile -Scene $scene
            ++$lifecyclePositive
            foreach($change in @(
                @{field='camera_source';value='player'},@{field='camera_position';value='NaN,0.5,-8.5'},
                @{field='camera_position';value='Infinity,0.5,-8.5'},@{field='camera_target';value='0,0'},
                @{field='camera_position';value='0,0,0'}
            )){$bad=$camera.Clone();$bad[$change.field]=$change.value;Assert-LifecycleRejected $bad $cleanup $profile ('camera '+$change.field) $scene}
        }
    }else{
        # 只關畫面的新世界仍有四株、三材質與249碰撞，必須逐世界拒絕。
        $bad=$receipt.Clone();$bad.canopy_plants='4';$bad.canopy_materials='3';$bad.forest_cover_clearance_colliders='249'
        Assert-LifecycleRejected $bad $cleanup $profile 'canopy world with drawing disabled'
    }
}
'WORLD_PROFILE_LIFECYCLE_RECEIPT_PASS; POSITIVE='+$lifecyclePositive+'; REJECTED='+$lifecycleRejected

# V121/V128/V129 分別封存素材作者、完整世界與真蕨送件；上方三種舊世界及反例完全保留。
$wood=$middle.Clone()
$woodFields=@{
    world_profile='forest-wood';forest_cover_patches='158';forest_cover_fingerprint='7394606949570911368'
    forest_cover_clearance_colliders='4582';forest_cover_clearance_collider_fingerprint='876747342402009001'
    forest_cover_variants='956,738,635,540,731';fern_colonies='77'
    fern_colony_fingerprint='14103587995731364812';fern_colony_variants='14,28,171,87'
    fern_colony_original_fingerprint='8112783215925746322'
    middle_layer_attempted='11615';middle_layer_transform_fingerprint='10337542786750772359'
    fir_layout='forest-wood-mature';fir_layout_population='100';fir_layout_reached_budget='1'
    fir_layout_fingerprint='8236695966185867318';fir_layout_patch_centres='24'
}
$woodCollision=@{
    canopy_collision_mode='wood-grid-conservative';canopy_collision_parts='4335';canopy_collision_spacing_m='0.15'
    canopy_wood_source_triangles='304768';canopy_wood_surface_cells='18192';canopy_wood_enclosed_grid_cells='178'
}
foreach($field in $woodFields.Keys){$wood[$field]=$woodFields[$field]}
foreach($field in $woodCollision.Keys){$wood[$field]=$woodCollision[$field]}
Assert-MistwatchForestCoverReceipt $wood -WorldProfile forest-wood
$woodRejected=0;$woodPositive=1
function Assert-WoodRejected([hashtable]$Receipt,[string]$Case){
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $Receipt -WorldProfile forest-wood}catch{$failed=$true}
    if(-not $failed){throw ('Invalid wood cover accepted: '+$Case)}
    ++$script:woodRejected
}
foreach($field in @($wood.Keys)){
    $bad=$wood.Clone();$bad.Remove($field);Assert-WoodRejected $bad ('missing '+$field)
}
foreach($field in @($woodFields.Keys)+@($woodCollision.Keys)){
    $bad=$wood.Clone();$bad[$field]='invalid';Assert-WoodRejected $bad ('invalid '+$field)
}
foreach($old in @($valid,$middle,$canopy)){
    Assert-WoodRejected $old ('old world '+$old.world_profile)
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $wood -WorldProfile $old.world_profile}catch{$failed=$true}
    if(-not $failed){throw 'Wood authored world accepted as an old world'}
    ++$woodRejected
    foreach($field in $woodCollision.Keys){
        $bad=$old.Clone();$bad[$field]=$woodCollision[$field]
        $failed=$false
        try{Assert-MistwatchForestCoverReceipt $bad -WorldProfile $old.world_profile}catch{$failed=$true}
        if(-not $failed){throw ('Old world accepted wood collision contamination: '+$field)}
        ++$woodRejected
    }
}
foreach($field in $woodCollision.Keys){
    foreach($value in @('0','NaN','Infinity','-1','00','04267','4267.0','0.150')){
        $bad=$wood.Clone();$bad[$field]=$value;Assert-WoodRejected $bad ('noncanonical '+$field+'='+$value)
    }
}
foreach($change in @(
    @{field='forest_cover_fingerprint';value='4989138190905410642'},
    @{field='forest_cover_clearance_colliders';value='249'},
    @{field='forest_cover_clearance_collider_fingerprint';value='16643660641049883501'},
    @{field='forest_cover_variants';value='941,762,617,522,758'},
    @{field='fern_colonies';value='85'},@{field='fern_colony_fingerprint';value='5579852658837856591'},
    @{field='fern_colony_variants';value='14,29,168,89'},
    @{field='submitted_grass_variants';value='930,1,1,1,1'},
    @{field='submitted_colony_variants';value='12,1,1,1'},
    @{field='submitted_original_fern_variants';value='0,0,0,0'},
    @{field='fir_layout';value='legacy'},@{field='fir_layout_population';value='99'},
    @{field='fir_layout_reached_budget';value='0'},@{field='fir_layout_fingerprint';value='14695981039346656037'},
    @{field='fir_layout_patch_centres';value='23'},
    @{field='canopy_collision_parts';value='4266'},@{field='canopy_collision_spacing_m';value='0.2'},
    @{field='canopy_wood_source_triangles';value='304767'},@{field='canopy_wood_surface_cells';value='17910'},
    @{field='canopy_wood_enclosed_grid_cells';value='206'}
)){$bad=$wood.Clone();$bad[$change.field]=$change.value;Assert-WoodRejected $bad ('identity '+$change.field)}
$failed=$false
try{Assert-MistwatchForestCoverReceipt $wood -WorldProfile forest-wood -MiddleLayer}catch{$failed=$true}
if(-not $failed){throw 'Conflicting wood and middle selector accepted'}
++ $woodRejected
foreach($scene in @('initial','river-overlook')){
    $fixed=$wood.Clone();$count=if($scene -eq 'initial'){198}else{227}
    $triangles=if($scene -eq 'initial'){317224}else{359632}
    $fixed.submitted_original_ferns=[string]$count;$fixed.submitted_original_fern_triangles=[string]$triangles
    $fixed.submitted_original_fern_variants=if($scene -eq 'initial'){'50,55,47,46'}else{'65,52,62,48'}
    $fixed.submitted_ferns=[string]($count+[long]$fixed.submitted_colony_ferns)
    $fixed.submitted_fern_triangles=[string]($triangles+[long]$fixed.submitted_colony_fern_triangles)
    Assert-MistwatchForestCoverReceipt $fixed -WorldProfile forest-wood -Scene $scene
    ++$woodPositive
    # 此別名同時保留總株數與總三角形，只改變真素材 slot 身分。
    $fixed.submitted_original_fern_variants=if($scene -eq 'initial'){'46,59,30,63'}else{'61,56,45,65'}
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $fixed -WorldProfile forest-wood -Scene $scene}catch{
        $failed=$_.Exception.Message -eq 'Original fern fixed-scene variant identity changed'
    }
    if(-not $failed){throw ('Wood fixed-scene variant alias accepted: '+$scene)}
    ++$woodRejected
}
'WOOD_FOREST_COVER_RECEIPT_PASS; POSITIVE='+$woodPositive+'; REJECTED='+$woodRejected

$woodLifecycle=@{
    world_profile='forest-wood';canopy_preview_fixture='0';canopy_world_profile='1';canopy_enabled='1'
    canopy_plants='4';canopy_rooted_meshes='4';canopy_population_fingerprint='6584434718760703877'
    canopy_ground_checks='12121992';canopy_primitive_triangles='606207';canopy_materials='3';canopy_submitted_slots='12'
    forest_cover_clearance_colliders='4582';shrubs_enabled='1';shrubs_plants='64';shrubs_submitted='64'
    shrubs_fingerprint='14817549782697216774'
    fir_layout='forest-wood-mature';fir_layout_population='100';fir_layout_reached_budget='1'
    fir_layout_fingerprint='8236695966185867318';fir_layout_patch_centres='24'
}
foreach($field in $woodCollision.Keys){$woodLifecycle[$field]=$woodCollision[$field]}
$woodCleanup=@{cleanup='success';materials_remaining='0';mesh_destroy_calls='7'}
$woodLifecyclePositive=1;$beforeRejected=$lifecycleRejected
Assert-MistwatchWorldProfileReceipt $woodLifecycle $woodCleanup forest-wood
foreach($field in @($woodLifecycle.Keys)){
    $bad=$woodLifecycle.Clone();$bad.Remove($field);Assert-LifecycleRejected $bad $woodCleanup forest-wood ('missing '+$field)
    $bad=$woodLifecycle.Clone();$bad[$field]='invalid';Assert-LifecycleRejected $bad $woodCleanup forest-wood ('invalid '+$field)
}
foreach($field in @($woodCleanup.Keys)){
    $bad=$woodCleanup.Clone();$bad.Remove($field);Assert-LifecycleRejected $woodLifecycle $bad forest-wood ('missing cleanup '+$field)
    $bad=$woodCleanup.Clone();$bad[$field]='invalid';Assert-LifecycleRejected $woodLifecycle $bad forest-wood ('invalid cleanup '+$field)
}
foreach($oldProfile in @('legacy-core','legacy-middle','forest-canopy')){
    Assert-LifecycleRejected $woodLifecycle $woodCleanup $oldProfile 'wood profile mismatch'
}
foreach($field in @('canopy_enabled','canopy_submitted_slots','canopy_materials','canopy_rooted_meshes')){
    $bad=$woodLifecycle.Clone();$bad[$field]='0';Assert-LifecycleRejected $bad $woodCleanup forest-wood ('draw/resource omitted '+$field)
}
foreach($scene in @('canopy-profile','canopy-root','canopy-detail')){
    $camera=$woodLifecycle.Clone();$camera.camera_source='diagnostic';$camera.camera_target='0,0,0'
    $camera.camera_position=if($scene -eq 'canopy-root'){'1.41421356,0.05,-1.41421356'}elseif($scene -eq 'canopy-detail'){'4.00010156,0.8,-4.00010156'}else{'0,0.5,-8.5'}
    Assert-MistwatchWorldProfileReceipt $camera $woodCleanup forest-wood -Scene $scene
    ++$woodLifecyclePositive
    foreach($change in @(
        @{field='camera_source';value='player'},@{field='camera_position';value='NaN,0.5,-8.5'},
        @{field='camera_position';value='Infinity,0.5,-8.5'},@{field='camera_target';value='0,0'},
        @{field='camera_position';value='0,0,0'}
    )){$bad=$camera.Clone();$bad[$change.field]=$change.value;Assert-LifecycleRejected $bad $woodCleanup forest-wood ('camera '+$change.field) $scene}
}
'WOOD_PROFILE_LIFECYCLE_RECEIPT_PASS; POSITIVE='+$woodLifecyclePositive+'; REJECTED='+($lifecycleRejected-$beforeRejected)

# 只解析原生工具，不啟動 GPU；確認正常預設與明確舊比較的參數契約。
$tokens=$null;$parseErrors=$null
$validator=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../../tools/validate_native_capture.ps1'),[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Native validation tool failed parser contract'}
$parameter=@($validator.ParamBlock.Parameters | Where-Object {$_.Name.VariablePath.UserPath -eq 'WorldProfile'})
if($parameter.Count -ne 1 -or $parameter[0].DefaultValue.Value -cne 'forest-wood'){throw 'Native validation default no longer matches production world'}
$profileAttribute=@($parameter[0].Attributes | Where-Object {$_.TypeName.Name -eq 'ValidateSet'})
$profiles=@($profileAttribute[0].PositionalArguments | ForEach-Object {$_.Value})
if(($profiles -join ',') -cne 'legacy-core,legacy-middle,forest-canopy,forest-wood'){throw 'Native validation lost explicit old-world comparison'}
'NATIVE_VALIDATOR_PARAMETER_CONTRACT_PASS; DEFAULT=forest-wood; PROFILES=4'

# 新倒木的合成收據契約單測；下列常數是獨立 CPU 作者配置，不是本輪 GPU 黃金值。
# 真原生入口仍必須讀同一 fresh build 產生、來源與387runtime身分一致的 CPU certificate。
$scannedExpected=@{
    native_contract=@{
        canopy_collision_mode='wood-grid-conservative'
        canopy_collision_parts='4335'
        canopy_collision_spacing_m='0.15'
        canopy_enabled='1'
        canopy_ground_checks='12121992'
        canopy_materials='3'
        canopy_plants='4'
        canopy_population_fingerprint='6584434718760703877'
        canopy_primitive_triangles='606207'
        canopy_rooted_meshes='4'
        canopy_submitted_slots='12'
        canopy_wood_enclosed_grid_cells='178'
        canopy_wood_source_triangles='304768'
        canopy_wood_surface_cells='18192'
        canopy_world_profile='1'
        fern_colonies='84'
        fern_colony_authored_population='300'
        fern_colony_fingerprint='10788989389604024881'
        fern_colony_original_fingerprint='8112783215925746322'
        fern_colony_population='300'
        fern_colony_reached_budget='1'
        fern_colony_seed='1128681010'
        fern_colony_variants='13,24,184,79'
        fir_layout='forest-wood-mature'
        fir_layout_fingerprint='8236695966185867318'
        fir_layout_patch_centres='24'
        fir_layout_population='100'
        fir_layout_reached_budget='1'
        forest_cover_authored_population='3600'
        forest_cover_clearance_collider_fingerprint='9905927095422790687'
        forest_cover_clearance_colliders='4582'
        forest_cover_fingerprint='6588246535391004864'
        forest_cover_independent_anchors='900'
        forest_cover_maximum_patches='400'
        forest_cover_mode='fixed_cluster_patches'
        forest_cover_patch_maximum_plants='32'
        forest_cover_patch_minimum_plants='20'
        forest_cover_patches='156'
        forest_cover_reached_budget='1'
        forest_cover_seed='1129272901'
        forest_cover_variants='937,727,648,506,782'
        ground_cover_instances='3600'
        middle_layer_attempted='11615'
        middle_layer_comparison='1'
        middle_layer_population='24'
        middle_layer_reached_budget='1'
        middle_layer_target='24'
        middle_layer_transform_fingerprint='10337542786750772359'
        shrubs_author_instance_triangles='716137'
        shrubs_author_leaf_count='504'
        shrubs_enabled='1'
        shrubs_fingerprint='13203614399962699898'
        shrubs_plants='64'
        shrubs_render_instance_triangles='354006'
        shrubs_render_leaf_counts='224,224,224,224'
        shrubs_submitted='64'
        shrubs_uploaded_leaf_indices='13608,13599,13530,13551'
        shrubs_uploaded_meshes='8'
        shrubs_uploaded_stem_indices='3024,3024,3024,3024'
        submitted_middle_layer='24'
        submitted_middle_layer_triangles='1988496'
        trail_litter_branches='240'
        trail_litter_enabled='1'
        trail_litter_fingerprint='16254976898190631993'
        trail_litter_patches='48'
        trail_litter_stones='336'
        trail_litter_triangles='27391'
        understory_ferns='420'
        understory_saplings='42'
        world_profile='forest-wood'
    }
    fallen_pose_words=(Get-MistwatchFallenAuthoredPoseWords)
}
$scanned=$wood.Clone()
foreach($field in $scannedExpected.native_contract.Keys){$scanned[$field]=$scannedExpected.native_contract[$field]}
$scanned.canopy_preview_fixture='0'
Assert-MistwatchForestCoverReceipt $scanned -WorldProfile forest-wood -FallenLogProfile scanned-straight-v1 -IndependentExpected $scannedExpected
Assert-MistwatchWorldProfileReceipt $scanned $woodCleanup forest-wood -FallenLogProfile scanned-straight-v1 -IndependentExpected $scannedExpected
$scannedRejected=0
function Assert-ScannedRejected([hashtable]$Receipt,[string]$Case) {
    foreach($lifecycle in @($false,$true)){
        $failed=$false
        try{
            if($lifecycle){Assert-MistwatchWorldProfileReceipt $Receipt $woodCleanup forest-wood -FallenLogProfile scanned-straight-v1 -IndependentExpected $scannedExpected}
            else{Assert-MistwatchForestCoverReceipt $Receipt -WorldProfile forest-wood -FallenLogProfile scanned-straight-v1 -IndependentExpected $scannedExpected}
        }catch{$failed=$true}
        if(-not $failed){throw ('Invalid scanned independent receipt accepted: '+$Case+' lifecycle='+$lifecycle)}
        ++$script:scannedRejected
    }
}
foreach($field in $scannedExpected.native_contract.Keys){
    $bad=$scanned.Clone();$bad.Remove($field);Assert-ScannedRejected $bad ('missing native70 '+$field)
    $bad=$scanned.Clone();$bad[$field]='invalid';Assert-ScannedRejected $bad ('changed native70 '+$field)
}
Assert-ScannedRejected $wood 'legacy forestwood as scanned without independent fields'
foreach($profile in @('legacy-core','legacy-middle','forest-canopy')){
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $scanned -WorldProfile $profile -FallenLogProfile scanned-straight-v1 -IndependentExpected $scannedExpected}catch{$failed=$true}
    if(-not $failed){throw ('Scanned mode accepted nonwood profile: '+$profile)}
    ++$scannedRejected
}
$failed=$false
try{Assert-MistwatchForestCoverReceipt $scanned -WorldProfile forest-wood -FallenLogProfile scanned-straight-v1}catch{$failed=$true}
if(-not $failed){throw 'Scanned mode accepted missing independent CPU contract'}
++$scannedRejected
$failed=$false
try{Assert-MistwatchForestCoverReceipt $scanned -WorldProfile forest-wood -FallenLogProfile legacy-cylinder}catch{$failed=$true}
if(-not $failed){throw 'Scanned values silently accepted as legacy-cylinder'}
++$scannedRejected
foreach($kind in @('missing','extra','wrong-type','changed-pose')){
    $badExpected=@{native_contract=$scannedExpected.native_contract.Clone();fallen_pose_words=$scannedExpected.fallen_pose_words.Clone()}
    if($kind -ceq 'missing'){$badExpected.native_contract.Remove('forest_cover_fingerprint')}
    elseif($kind -ceq 'extra'){$badExpected.native_contract['fake_extra']='1'}
    elseif($kind -ceq 'wrong-type'){$badExpected.native_contract['ground_cover_instances']=3600}
    else{$badExpected.fallen_pose_words['prop_0']='0,'}
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $scanned -WorldProfile forest-wood -FallenLogProfile scanned-straight-v1 -IndependentExpected $badExpected}catch{$failed=$true}
    if(-not $failed){throw ('Malformed independent CPU contract accepted: '+$kind)}
    ++$scannedRejected
}
foreach($scene in @('initial','river-overlook')){
    $fixed=$scanned.Clone()
    $count=if($scene -ceq 'initial'){198}else{227}
    $triangles=if($scene -ceq 'initial'){317224}else{359632}
    $fixed.submitted_original_ferns=[string]$count;$fixed.submitted_original_fern_triangles=[string]$triangles
    $fixed.submitted_original_fern_variants=if($scene -ceq 'initial'){'50,55,47,46'}else{'65,52,62,48'}
    $fixed.submitted_ferns=[string]($count+[long]$fixed.submitted_colony_ferns)
    $fixed.submitted_fern_triangles=[string]($triangles+[long]$fixed.submitted_colony_fern_triangles)
    Assert-MistwatchForestCoverReceipt $fixed -WorldProfile forest-wood -Scene $scene -FallenLogProfile scanned-straight-v1 -IndependentExpected $scannedExpected
    $fixed.submitted_original_fern_variants=if($scene -ceq 'initial'){'46,59,30,63'}else{'61,56,45,65'}
    $failed=$false
    try{Assert-MistwatchForestCoverReceipt $fixed -WorldProfile forest-wood -Scene $scene -FallenLogProfile scanned-straight-v1 -IndependentExpected $scannedExpected}catch{$failed=$_.Exception.Message -ceq 'Original fern fixed-scene variant identity changed'}
    if(-not $failed){throw ('Scanned fixed-scene equal count/triangle alias accepted: '+$scene)}
    ++$scannedRejected
}
'SCANNED_FALLEN_NATIVE70_RECEIPT_PASS; POSITIVE=4; REJECTED='+$scannedRejected
$fallenParameter=@($validator.ParamBlock.Parameters|Where-Object{$_.Name.VariablePath.UserPath -ceq 'FallenLogProfile'})
if($fallenParameter.Count -ne 1 -or $fallenParameter[0].DefaultValue.Value -cne 'scanned-straight-v1'){throw 'Native validator default lost scanned production profile'}
$fallenAttribute=@($fallenParameter[0].Attributes|Where-Object{$_.TypeName.Name -ceq 'ValidateSet'})
if((@($fallenAttribute[0].PositionalArguments|ForEach-Object{$_.Value})-join ',') -cne 'legacy-cylinder,scanned-straight-v1'){throw 'Native validator lost explicit legacy fallen comparison'}
'NATIVE_FALLEN_PARAMETER_CONTRACT_PASS; DEFAULT=scanned-straight-v1; PROFILES=2'

# JSON 的整數寬度不能造成假失敗，也不能接受經隱式轉換的錯誤型別。
$toolchainValid=@{msc_full_ver=[int]194435228;cplusplus=[int]202002;
    configuration='Release-x64-C++20-/W4-/WX-/MP1-/fp:strict-MultiThreadedDLL';abi='formal-AevryndEngine'}
Assert-MistwatchFallenToolchain $toolchainValid
$toolchainLong=$toolchainValid.Clone()
$toolchainLong.msc_full_ver=[long]194435228;$toolchainLong.cplusplus=[long]202002
Assert-MistwatchFallenToolchain $toolchainLong
$toolchainJson=($toolchainValid|ConvertTo-Json -Compress)|ConvertFrom-Json -AsHashtable
if($toolchainJson.msc_full_ver -isnot [long] -or $toolchainJson.cplusplus -isnot [long]){throw 'Actual JSON integer representation changed'}
Assert-MistwatchFallenToolchain $toolchainJson
$toolchainRejected=0
function Assert-ToolchainRejected($Value,[string]$Case) {
    $failed=$false
    try{Assert-MistwatchFallenToolchain $Value}catch{$failed=$true}
    if(-not $failed){throw ('Invalid formal toolchain accepted: '+$Case)}
    ++$script:toolchainRejected
}
foreach($field in @($toolchainValid.Keys)){
    $bad=$toolchainValid.Clone();$bad.Remove($field);Assert-ToolchainRejected $bad ('missing '+$field)
    $bad=$toolchainValid.Clone();$bad[$field]=$null;Assert-ToolchainRejected $bad ('null '+$field)
}
foreach($value in @('194435228',[double]194435228,$true,$false,[int]0,[int]-1,[int]189999999,[long]2147483648)){
    $bad=$toolchainValid.Clone();$bad.msc_full_ver=$value;Assert-ToolchainRejected $bad ('compiler type/value '+[string]$value)
}
foreach($value in @('202002',[double]202002,$true,$false,[int]201703,[int]0)){
    $bad=$toolchainValid.Clone();$bad.cplusplus=$value;Assert-ToolchainRejected $bad ('language type/value '+[string]$value)
}
foreach($value in @('private-borrowed-engine','formal-aevryndengine','')){
    $bad=$toolchainValid.Clone();$bad.abi=$value;Assert-ToolchainRejected $bad ('ABI '+$value)
}
foreach($value in @('Debug-x64-C++20-/W4-/WX-/MP1-/fp:strict-MultiThreadedDLL',
    'Release-x64-C++20-/W4-/WX-/MP1-/fp:fast-MultiThreadedDLL',
    'Release-x64-C++20-/W4-/MP1-/fp:strict-MultiThreadedDLL',
    'Release-x64-C++20-/W4-/WX-/MP1-/fp:strict-MultiThreaded')){
    $bad=$toolchainValid.Clone();$bad.configuration=$value;Assert-ToolchainRejected $bad ('configuration '+$value)
}
$bad=$toolchainValid.Clone();$bad.extra='unexpected';Assert-ToolchainRejected $bad 'extra key'
'FORMAL_TOOLCHAIN_JSON_RECEIPT_PASS; POSITIVE=3; REJECTED='+$toolchainRejected
