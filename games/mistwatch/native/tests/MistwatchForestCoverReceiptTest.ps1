$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../tools/forest_cover_contract.ps1')
# 此為合成收據的負向契約單測，不是實機或畫質證據。
$valid=@{
    ground_cover_instances='3600';forest_cover_mode='fixed_cluster_patches'
    forest_cover_seed='1129272901';forest_cover_authored_population='3600'
    forest_cover_maximum_patches='400';forest_cover_patch_minimum_plants='20'
    forest_cover_patch_maximum_plants='32';forest_cover_independent_anchors='900'
    forest_cover_patches='169';forest_cover_reached_budget='1'
    forest_cover_fingerprint='5098415506555036174';forest_cover_clearance_colliders='221'
    forest_cover_clearance_collider_fingerprint='6196035014821778326'
    forest_cover_variants='941,744,629,563,723'
    fern_colony_population='300';fern_colony_authored_population='300';fern_colonies='82'
    fern_colony_seed='1128681010';fern_colony_reached_budget='1'
    fern_colony_fingerprint='3770887003960151305'
    fern_colony_original_fingerprint='14330624253385469047'
    fern_colony_variants='13,32,165,90';understory_ferns='420';understory_saplings='42'
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
    forest_cover_patches='166';forest_cover_fingerprint='13112970783824743828'
    forest_cover_clearance_colliders='245';forest_cover_clearance_collider_fingerprint='11331479475778127843'
    forest_cover_variants='974,737,624,535,730';fern_colonies='86'
    fern_colony_fingerprint='5859764795355075831';fern_colony_variants='9,37,168,86'
    middle_layer_comparison='1';middle_layer_population='24';middle_layer_target='24'
    middle_layer_reached_budget='1';middle_layer_attempted='11395'
    middle_layer_transform_fingerprint='9091870748330489572'
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
    @{field='submitted_colony_variants';value='10,1,1,1'},
    @{field='submitted_original_fern_variants';value='0,0,0,0'},
    @{field='submitted_middle_layer';value='23'},
    @{field='submitted_middle_layer_triangles';value='1988495'},
    @{field='middle_layer_population';value='024'},
    @{field='middle_layer_transform_fingerprint';value='9091870748330489573'},
    @{field='forest_cover_clearance_colliders';value='221'},
    @{field='forest_cover_fingerprint';value='5098415506555036174'},
    @{field='fern_colony_fingerprint';value='3770887003960151305'},
    @{field='submitted_colony_fern_triangles';value='6231'},
    @{field='submitted_fern_triangles';value='8617'}
)){
    $bad=$middle.Clone();$bad[$change.field]=$change.value;Assert-MiddleRejected $bad $change.field
}
$wrongMode=$false;try{Assert-MistwatchForestCoverReceipt $middle}catch{$wrongMode=$true}
if(-not $wrongMode){throw 'Middle world accepted without explicit mode'}
'MIDDLE_LAYER_RECEIPT_TEST_PASS; VALID=1; REJECTED='+$middleRejected+'; MODE_MIX_REJECTED=1'
