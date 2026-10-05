function Assert-MistwatchForestCoverReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,[string]$Scene='',[switch]$MiddleLayer)
    # 凹灣地形實機重封存作者配置；獨立於畫質評分，不接受舊 scatter、缺欄位或少株 fallback。
    $expected=@{
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
    }
    if($MiddleLayer) {
        # 新幹使低植被重新避讓，另驗精確新作者配置；不把舊FNV投影成假通過。
        $middleExpected=@{
            forest_cover_patches='154';forest_cover_fingerprint='8857932304976401347'
            forest_cover_clearance_colliders='245';forest_cover_clearance_collider_fingerprint='11739939804215987065'
            forest_cover_variants='871,751,625,568,785';fern_colonies='86'
            fern_colony_fingerprint='3313014129975770240';fern_colony_variants='6,37,170,87'
            middle_layer_comparison='1';middle_layer_population='24';middle_layer_target='24'
            middle_layer_reached_budget='1';middle_layer_attempted='19887'
            middle_layer_transform_fingerprint='11352586881239724297'
            submitted_middle_layer='24';submitted_middle_layer_triangles='1988496'
        }
        foreach($field in $middleExpected.Keys){$expected[$field]=$middleExpected[$field]}
    } else {
        # 舊世界對照必須明確沒有中層樹；舊草蕨指紋不能掩蓋額外繪製。
        $baselineMiddle=@{
            middle_layer_comparison='0';middle_layer_population='0';middle_layer_target='0'
            middle_layer_reached_budget='1';middle_layer_attempted='0';middle_layer_transform_fingerprint='0'
            submitted_middle_layer='0';submitted_middle_layer_triangles='0'
        }
        foreach($field in $baselineMiddle.Keys){$expected[$field]=$baselineMiddle[$field]}
    }
    foreach($field in $expected.Keys) {
        if(-not $Receipt.ContainsKey($field)-or [string]$Receipt[$field] -cne $expected[$field]) {
            throw ('Forest cover author contract mismatch: '+$field)
        }
    }
    $groups=@(
        @{field='submitted_grass_variants';population=@($expected.forest_cover_variants.Split(',')|ForEach-Object{[int]$_});triangles=@(714,1760,1215,2489,1664);count='submitted_grass';total='submitted_grass_triangles'},
        @{field='submitted_colony_variants';population=@($expected.fern_colony_variants.Split(',')|ForEach-Object{[int]$_});triangles=@(2384,2248,784,816);count='submitted_colony_ferns';total='submitted_colony_fern_triangles'},
        @{field='submitted_original_fern_variants';population=@(105,105,105,105);triangles=@(2384,2248,784,816);count='submitted_original_ferns';total='submitted_original_fern_triangles'}
    )
    foreach($group in $groups) {
        $counts=@([string]$Receipt[$group.field] -split ',')
        if($counts.Count -ne $group.population.Count){throw ('Invalid submitted variant count: '+$group.field)}
        [long]$sum=0;[long]$triangles=0
        for($variant=0;$variant -lt $counts.Count;++$variant) {
            if($counts[$variant] -cnotmatch '^(0|[1-9][0-9]{0,4})$'){throw ('Noncanonical submitted variant: '+$group.field)}
            [long]$value=$counts[$variant]
            if($value -gt $group.population[$variant]){throw ('Submitted variant exceeds authored population: '+$group.field)}
            $sum+=$value;$triangles+=$value*$group.triangles[$variant]
        }
        if([string]$Receipt[$group.count] -cne [string]$sum -or [string]$Receipt[$group.total] -cne [string]$triangles) {
            throw ('Submitted variant accounting mismatch: '+$group.field)
        }
    }
    foreach($field in @('submitted_ferns','submitted_fern_triangles')) {
        if([string]$Receipt[$field] -cnotmatch '^(0|[1-9][0-9]{0,7})$'){throw ('Invalid original plus colony count: '+$field)}
    }
    [long]$originalCount=[long]$Receipt.submitted_ferns-[long]$Receipt.submitted_colony_ferns
    [long]$originalTriangles=[long]$Receipt.submitted_fern_triangles-[long]$Receipt.submitted_colony_fern_triangles
    if($originalCount -lt 1 -or $originalCount -gt 420 -or
       [string]$Receipt.submitted_original_ferns -cne [string]$originalCount -or
       [string]$Receipt.submitted_original_fern_triangles -cne [string]$originalTriangles) {
        throw 'Original fern accounting was replaced or merged incorrectly'
    }
    # 固定兩景另保留原版實機提交數，不讓新蕨的總數掩蓋原林下層漏畫。
    if(($Scene -eq 'initial' -and ($originalCount -ne 192 -or $originalTriangles -ne 298968)) -or
       ($Scene -eq 'river-overlook' -and ($originalCount -ne 226 -or $originalTriangles -ne 350104))) {
        throw 'Original fern fixed-scene submission changed'
    }
    # 相同總株數與三角數仍可由錯誤種類分布湊出；固定景需逐種類保留身分。
    $originalVariants=if($Scene -eq 'initial'){'47,49,49,47'}elseif($Scene -eq 'river-overlook'){'51,61,51,63'}else{$null}
    if($null -ne $originalVariants -and [string]$Receipt.submitted_original_fern_variants -cne $originalVariants){
        throw 'Original fern fixed-scene variant identity changed'
    }
}

function Assert-MistwatchTerrainCoverageMarker {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt)
    [double]$marker=0
    if(-not $Receipt.ContainsKey('ground_normal_scale') -or
       -not [double]::TryParse([string]$Receipt.ground_normal_scale,[Globalization.NumberStyles]::Float,
           [Globalization.CultureInfo]::InvariantCulture,[ref]$marker) -or
       [double]::IsNaN($marker) -or [double]::IsInfinity($marker) -or
       [Math]::Abs($marker+15.46875) -gt .0001) {
        throw 'Forest floor production coverage marker missing or invalid'
    }
}
