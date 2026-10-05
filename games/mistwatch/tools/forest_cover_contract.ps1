function Assert-MistwatchForestCoverReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,[string]$Scene='',[switch]$MiddleLayer)
    # 精確作者配置獨立於畫質評分；不接受舊 scatter、缺欄位或少株 fallback。
    $expected=@{
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
    }
    if($MiddleLayer) {
        # 新幹使低植被重新避讓，另驗精確新作者配置；不把舊FNV投影成假通過。
        $middleExpected=@{
            forest_cover_patches='162';forest_cover_fingerprint='1048722341261312903'
            forest_cover_clearance_colliders='245';forest_cover_clearance_collider_fingerprint='6744664032273563734'
            forest_cover_variants='964,741,630,546,719';fern_colonies='79'
            fern_colony_fingerprint='3053818525289996684';fern_colony_variants='10,28,173,89'
            middle_layer_comparison='1';middle_layer_population='24';middle_layer_target='24'
            middle_layer_reached_budget='1';middle_layer_attempted='16999'
            middle_layer_transform_fingerprint='12515088988881324841'
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
    if(($Scene -eq 'initial' -and ($originalCount -ne 188 -or $originalTriangles -ne 286568)) -or
       ($Scene -eq 'river-overlook' -and ($originalCount -ne 224 -or $originalTriangles -ne 346056))) {
        throw 'Original fern fixed-scene submission changed'
    }
    # 相同總株數與三角數仍可由錯誤種類分布湊出；固定景需逐種類保留身分。
    $originalVariants=if($Scene -eq 'initial'){'43,47,49,49'}elseif($Scene -eq 'river-overlook'){'55,55,54,60'}else{$null}
    if($null -ne $originalVariants -and [string]$Receipt.submitted_original_fern_variants -cne $originalVariants){
        throw 'Original fern fixed-scene variant identity changed'
    }
}
