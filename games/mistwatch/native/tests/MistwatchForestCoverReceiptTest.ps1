$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../tools/forest_cover_contract.ps1')
# 此為合成收據的負向契約單測，不是實機或畫質證據。
$valid=@{
    ground_cover_instances='3600';forest_cover_mode='fixed_cluster_patches'
    forest_cover_seed='1129272901';forest_cover_authored_population='3600'
    forest_cover_maximum_patches='400';forest_cover_patch_minimum_plants='20'
    forest_cover_patch_maximum_plants='32';forest_cover_independent_anchors='900'
    forest_cover_patches='165';forest_cover_reached_budget='1'
    forest_cover_fingerprint='18346272358334413870';forest_cover_clearance_colliders='221'
    forest_cover_clearance_collider_fingerprint='522388356439590700'
    forest_cover_variants='900,764,672,563,701'
    fern_colony_population='300';fern_colony_authored_population='300';fern_colonies='82'
    fern_colony_seed='1128681010';fern_colony_reached_budget='1'
    fern_colony_fingerprint='18387932991795706967'
    fern_colony_original_fingerprint='14418781708182382769'
    fern_colony_variants='11,29,169,91';understory_ferns='420';understory_saplings='42'
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
    forest_cover_patches='162';forest_cover_fingerprint='1048722341261312903'
    forest_cover_clearance_colliders='245';forest_cover_clearance_collider_fingerprint='6744664032273563734'
    forest_cover_variants='964,741,630,546,719';fern_colonies='79'
    fern_colony_fingerprint='3053818525289996684';fern_colony_variants='10,28,173,89'
    middle_layer_comparison='1';middle_layer_population='24';middle_layer_target='24'
    middle_layer_reached_budget='1';middle_layer_attempted='16999'
    middle_layer_transform_fingerprint='12515088988881324841'
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
        $count=if($scene -eq 'initial'){188}else{224}
        $triangles=if($scene -eq 'initial'){286568}else{346056}
        $fixed.submitted_original_ferns=[string]$count
        $fixed.submitted_original_fern_triangles=[string]$triangles
        $fixed.submitted_original_fern_variants=if($scene -eq 'initial'){'43,47,49,49'}else{'55,55,54,60'}
        $fixed.submitted_ferns=[string]($count+[long]$fixed.submitted_colony_ferns)
        $fixed.submitted_fern_triangles=[string]($triangles+[long]$fixed.submitted_colony_fern_triangles)
        Assert-MistwatchForestCoverReceipt $fixed -Scene $scene -MiddleLayer:$useMiddle
        ++$fixedPositive
        $alias=$fixed.Clone()
        $alias.submitted_original_fern_variants=if($scene -eq 'initial'){'47,43,66,32'}else{'59,51,71,43'}
        $identityRejected=$false
        try{Assert-MistwatchForestCoverReceipt $alias -Scene $scene -MiddleLayer:$useMiddle}catch{
            if($_.Exception.Message -eq 'Original fern fixed-scene variant identity changed'){$identityRejected=$true}
        }
        if(-not $identityRejected){throw 'Equal-count/equal-triangle variant alias accepted'}
        ++$aliasRejected
    }
}
'FIXED_SCENE_VARIANT_IDENTITY_PASS; POSITIVE='+$fixedPositive+'; ALIAS_REJECTED='+$aliasRejected
