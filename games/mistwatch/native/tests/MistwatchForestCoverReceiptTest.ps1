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
