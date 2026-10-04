function Assert-MistwatchForestCoverReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,[string]$Scene='')
    # 精確作者配置獨立於畫質評分；不接受舊 scatter、缺欄位或少株 fallback。
    $expected=@{
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
    }
    foreach($field in $expected.Keys) {
        if(-not $Receipt.ContainsKey($field)-or [string]$Receipt[$field] -cne $expected[$field]) {
            throw ('Forest cover author contract mismatch: '+$field)
        }
    }
    $groups=@(
        @{field='submitted_grass_variants';population=@(941,744,629,563,723);triangles=@(714,1760,1215,2489,1664);count='submitted_grass';total='submitted_grass_triangles'},
        @{field='submitted_colony_variants';population=@(13,32,165,90);triangles=@(2384,2248,784,816);count='submitted_colony_ferns';total='submitted_colony_fern_triangles'},
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
    if(($Scene -eq 'initial' -and ($originalCount -ne 201 -or $originalTriangles -ne 314224)) -or
       ($Scene -eq 'river-overlook' -and ($originalCount -ne 238 -or $originalTriangles -ne 379128))) {
        throw 'Original fern fixed-scene submission changed'
    }
}
