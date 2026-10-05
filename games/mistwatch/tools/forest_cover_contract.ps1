function Assert-MistwatchForestCoverReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,[string]$Scene='',[switch]$MiddleLayer,
        [ValidateSet('legacy-core','legacy-middle','forest-canopy','forest-wood')][string]$WorldProfile='legacy-core')
    if($MiddleLayer){
        if($PSBoundParameters.ContainsKey('WorldProfile') -and $WorldProfile -ne 'legacy-middle'){throw 'Middle layer conflicts with world profile'}
        $WorldProfile='legacy-middle'
    }
    # 凹灣地形實機重封存作者配置；獨立於畫質評分，不接受舊 scatter、缺欄位或少株 fallback。
    $expected=@{
        world_profile=$WorldProfile;ground_cover_instances='3600';forest_cover_mode='fixed_cluster_patches'
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
    if($WorldProfile -ne 'legacy-core') {
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
        if($WorldProfile -eq 'forest-canopy'){
            # V85 三世界封存與 V75 獨立 CPU 作者證據；不從本輪收據更新黃金值。
            $canopyExpected=@{
                forest_cover_patches='168';forest_cover_fingerprint='4989138190905410642'
                forest_cover_clearance_colliders='249';forest_cover_clearance_collider_fingerprint='16643660641049883501'
                forest_cover_variants='941,762,617,522,758';fern_colonies='85'
                fern_colony_fingerprint='5579852658837856591';fern_colony_variants='14,29,168,89'
            }
            foreach($field in $canopyExpected.Keys){$expected[$field]=$canopyExpected[$field]}
        }
        elseif($WorldProfile -eq 'forest-wood'){
            # V108 保留三個舊配置並以完整網格／木質盒獨立重現的新作者身分。
            $woodExpected=@{
                forest_cover_patches='168';forest_cover_fingerprint='6471420486779392182'
                forest_cover_clearance_colliders='4512';forest_cover_clearance_collider_fingerprint='230929787695590120'
                forest_cover_variants='929,749,614,528,780';fern_colonies='78'
                fern_colony_fingerprint='9573416696332517026';fern_colony_variants='11,22,165,102'
            }
            foreach($field in $woodExpected.Keys){$expected[$field]=$woodExpected[$field]}
        }
    } else {
        # 舊世界對照必須明確沒有中層樹；舊草蕨指紋不能掩蓋額外繪製。
        $baselineMiddle=@{
            middle_layer_comparison='0';middle_layer_population='0';middle_layer_target='0'
            middle_layer_reached_budget='1';middle_layer_attempted='0';middle_layer_transform_fingerprint='0'
            submitted_middle_layer='0';submitted_middle_layer_triangles='0'
        }
        foreach($field in $baselineMiddle.Keys){$expected[$field]=$baselineMiddle[$field]}
    }
    foreach($field in ($expected.Keys | Sort-Object)) {
        if(-not $Receipt.ContainsKey($field)-or [string]$Receipt[$field] -cne $expected[$field]) {
            throw ('Forest cover author contract mismatch: '+$field)
        }
    }
    Assert-MistwatchCanopyCollisionReceipt $Receipt $WorldProfile
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

function Assert-MistwatchWorldProfileReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,
        [Parameter(Mandatory=$true)][hashtable]$Cleanup,
        [Parameter(Mandatory=$true)][ValidateSet('legacy-core','legacy-middle','forest-canopy','forest-wood')][string]$WorldProfile,
        [string]$Scene='')
    # 舊世界必須未載入新樹；關閉繪製卻留下資源或碰撞不能當作舊世界。
    $expected=@{
        world_profile=$WorldProfile;canopy_preview_fixture='0';canopy_world_profile='0';canopy_enabled='0'
        canopy_plants='0';canopy_population_fingerprint='0';canopy_ground_checks='0'
        canopy_rooted_meshes='0';canopy_primitive_triangles='0';canopy_materials='0';canopy_submitted_slots='0'
        forest_cover_clearance_colliders=$(if($WorldProfile -eq 'legacy-core'){'221'}else{'245'})
        shrubs_enabled='1';shrubs_plants='64';shrubs_submitted='64'
        shrubs_fingerprint=$(if($WorldProfile -eq 'legacy-core'){'4812794541849057446'}else{'18032575485754629455'})
    }
    $meshDestroyCalls='0'
    if($WorldProfile -in @('forest-canopy','forest-wood')){
        $canopyExpected=@{
            canopy_world_profile='1';canopy_enabled='1';canopy_plants='4'
            canopy_population_fingerprint='13093722598475582899';canopy_ground_checks='12121992'
            canopy_rooted_meshes='4';canopy_primitive_triangles='606207';canopy_materials='3'
            canopy_submitted_slots='12';forest_cover_clearance_colliders='249';shrubs_fingerprint='5849093981174083644'
        }
        foreach($field in $canopyExpected.Keys){$expected[$field]=$canopyExpected[$field]}
        $meshDestroyCalls='7'
        if($WorldProfile -eq 'forest-wood'){
            $expected.forest_cover_clearance_colliders='4512'
            $expected.shrubs_fingerprint='6244520233766686875'
        }
    }
    foreach($field in ($expected.Keys | Sort-Object)){
        if(-not $Receipt.ContainsKey($field) -or [string]$Receipt[$field] -cne $expected[$field]){
            throw ('World profile lifecycle mismatch: '+$field)
        }
    }
    Assert-MistwatchCanopyCollisionReceipt $Receipt $WorldProfile
    foreach($field in @('cleanup','materials_remaining','mesh_destroy_calls')){
        $value=if($field -eq 'cleanup'){'success'}elseif($field -eq 'materials_remaining'){'0'}else{$meshDestroyCalls}
        if(-not $Cleanup.ContainsKey($field) -or [string]$Cleanup[$field] -cne $value){throw ('Canopy cleanup mismatch: '+$field)}
    }
    if($Scene -in @('canopy-profile','canopy-root','canopy-detail')){
        if($WorldProfile -notin @('forest-canopy','forest-wood') -or $Receipt.camera_source -cne 'diagnostic'){throw 'Canopy profile camera missing'}
        $position=@([string]$Receipt.camera_position -split ',')
        $target=@([string]$Receipt.camera_target -split ',')
        if($position.Count -ne 3 -or $target.Count -ne 3){throw 'Canopy camera receipt invalid'}
        $offset=if($Scene -eq 'canopy-root'){@(([Math]::Sqrt(2)),.05,(-[Math]::Sqrt(2)))}elseif($Scene -eq 'canopy-detail'){@((5.657/[Math]::Sqrt(2)),.8,(-5.657/[Math]::Sqrt(2)))}else{@(0,.5,-8.5)}
        for($axis=0;$axis -lt 3;++$axis){
            [double]$p=0;[double]$t=0
            if(-not [double]::TryParse($position[$axis],[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$p) -or
               -not [double]::TryParse($target[$axis],[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$t) -or
               [double]::IsNaN($p) -or [double]::IsInfinity($p) -or [double]::IsNaN($t) -or [double]::IsInfinity($t) -or
               [Math]::Abs($p-$t-$offset[$axis]) -gt .001){throw 'Canopy profile camera offset mismatch'}
        }
    }
}

function Assert-MistwatchCanopyCollisionReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,
        [Parameter(Mandatory=$true)][ValidateSet('legacy-core','legacy-middle','forest-canopy','forest-wood')][string]$WorldProfile)
    # V108 真資產 CPU 作者證據；只把新木質配置加為第四個世界，不重寫舊黃金值。
    $wood=$WorldProfile -eq 'forest-wood'
    $expected=@{
        canopy_collision_mode=$(if($wood){'wood-grid-conservative'}else{'full-render-envelope'})
        canopy_collision_parts=$(if($wood){'4267'}elseif($WorldProfile -eq 'forest-canopy'){'4'}else{'0'})
        canopy_collision_spacing_m=$(if($wood){'0.15'}else{'0'})
        canopy_wood_source_triangles=$(if($wood){'304768'}else{'0'})
        canopy_wood_surface_cells=$(if($wood){'17911'}else{'0'})
        canopy_wood_enclosed_grid_cells=$(if($wood){'207'}else{'0'})
    }
    foreach($field in $expected.Keys){
        # 舊封存收據可能沒有新診斷欄位；存在時仍逐欄拒絕木質配置污染。
        if(($wood -or $Receipt.ContainsKey($field)) -and
            (-not $Receipt.ContainsKey($field) -or [string]$Receipt[$field] -cne $expected[$field])){
            throw ('Canopy collision contract mismatch: '+$field)
        }
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
