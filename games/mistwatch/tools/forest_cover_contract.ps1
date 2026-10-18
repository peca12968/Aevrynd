. (Join-Path $PSScriptRoot 'FallenLogContract.ps1')
function Assert-MistwatchForestCoverReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,[string]$Scene='',[switch]$MiddleLayer,
        [ValidateSet('legacy-core','legacy-middle','forest-canopy','forest-wood')][string]$WorldProfile='legacy-core',
        [ValidateSet('legacy-cylinder','scanned-straight-v1')][string]$FallenLogProfile='legacy-cylinder',
        [hashtable]$IndependentExpected=$null)
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
            # V121 全素材獨立作者與 V128 完整世界重建；只更新新杉樹布局，舊三世界不變。
            $woodExpected=@{
                forest_cover_patches='158';forest_cover_fingerprint='7394606949570911368'
                forest_cover_clearance_colliders='4582';forest_cover_clearance_collider_fingerprint='876747342402009001'
                forest_cover_variants='956,738,635,540,731';fern_colonies='77'
                fern_colony_fingerprint='14103587995731364812';fern_colony_variants='14,28,171,87'
                fern_colony_original_fingerprint='8112783215925746322'
                middle_layer_attempted='11615';middle_layer_transform_fingerprint='10337542786750772359'
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
    if($FallenLogProfile -ceq 'scanned-straight-v1'){
        if($WorldProfile -cne 'forest-wood'){throw 'Scanned independent branch is only current forest-wood'}
        if($null -eq $IndependentExpected){throw 'Scanned branch requires independent CPU expected contract'}
        $independent=$IndependentExpected
        Assert-MistwatchIndependentExpectedContract $independent
        foreach($field in @('forest_cover_patches','forest_cover_fingerprint','forest_cover_clearance_collider_fingerprint',
            'forest_cover_variants','fern_colonies','fern_colony_fingerprint','fern_colony_variants')){
            $expected[$field]=$independent.native_contract[$field]
        }
        Assert-MistwatchIndependentNativeFields $Receipt $independent
    }
    foreach($field in ($expected.Keys | Sort-Object)) {
        if(-not $Receipt.ContainsKey($field)-or [string]$Receipt[$field] -cne $expected[$field]) {
            throw ('Forest cover author contract mismatch: '+$field)
        }
    }
    Assert-MistwatchCanopyCollisionReceipt $Receipt $WorldProfile
    Assert-MistwatchFirLayoutReceipt $Receipt $WorldProfile
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
    # V129 以真四素材重建距離送件；新根布局有自己的精確值，舊三世界仍保留原值。
    $wood=$WorldProfile -eq 'forest-wood'
    $initialCount=if($wood){198}else{192};$initialTriangles=if($wood){317224}else{298968}
    $riverCount=if($wood){227}else{226};$riverTriangles=if($wood){359632}else{350104}
    if(($Scene -eq 'initial' -and ($originalCount -ne $initialCount -or $originalTriangles -ne $initialTriangles)) -or
       ($Scene -eq 'river-overlook' -and ($originalCount -ne $riverCount -or $originalTriangles -ne $riverTriangles))) {
        throw 'Original fern fixed-scene submission changed'
    }
    # 相同總株數與三角數仍可由錯誤種類分布湊出；固定景需逐種類保留身分。
    $originalVariants=if($Scene -eq 'initial'){if($wood){'50,55,47,46'}else{'47,49,49,47'}}
        elseif($Scene -eq 'river-overlook'){if($wood){'65,52,62,48'}else{'51,61,51,63'}}else{$null}
    if($null -ne $originalVariants -and [string]$Receipt.submitted_original_fern_variants -cne $originalVariants){
        throw 'Original fern fixed-scene variant identity changed'
    }
}

function Assert-MistwatchWorldProfileReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,
        [Parameter(Mandatory=$true)][hashtable]$Cleanup,
        [Parameter(Mandatory=$true)][ValidateSet('legacy-core','legacy-middle','forest-canopy','forest-wood')][string]$WorldProfile,
        [string]$Scene='',
        [ValidateSet('legacy-cylinder','scanned-straight-v1')][string]$FallenLogProfile='legacy-cylinder',
        [hashtable]$IndependentExpected=$null)
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
            $expected.forest_cover_clearance_colliders='4582'
            $expected.shrubs_fingerprint='14817549782697216774'
            $expected.canopy_population_fingerprint='6584434718760703877'
        }
    }
    if($FallenLogProfile -ceq 'scanned-straight-v1'){
        if($WorldProfile -cne 'forest-wood'){throw 'Scanned independent branch is only current forest-wood'}
        if($null -eq $IndependentExpected){throw 'Scanned branch requires independent CPU expected contract'}
        $independent=$IndependentExpected
        Assert-MistwatchIndependentExpectedContract $independent
        $expected.shrubs_fingerprint=$independent.native_contract.shrubs_fingerprint
        Assert-MistwatchIndependentNativeFields $Receipt $independent
    }
    foreach($field in ($expected.Keys | Sort-Object)){
        if(-not $Receipt.ContainsKey($field) -or [string]$Receipt[$field] -cne $expected[$field]){
            throw ('World profile lifecycle mismatch: '+$field)
        }
    }
    Assert-MistwatchCanopyCollisionReceipt $Receipt $WorldProfile
    Assert-MistwatchFirLayoutReceipt $Receipt $WorldProfile
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
    # V121/V128 真素材與新四 pose 的木件作者結果；舊冠盒世界保持原黃金值。
    $wood=$WorldProfile -eq 'forest-wood'
    $expected=@{
        canopy_collision_mode=$(if($wood){'wood-grid-conservative'}else{'full-render-envelope'})
        canopy_collision_parts=$(if($wood){'4335'}elseif($WorldProfile -eq 'forest-canopy'){'4'}else{'0'})
        canopy_collision_spacing_m=$(if($wood){'0.15'}else{'0'})
        canopy_wood_source_triangles=$(if($wood){'304768'}else{'0'})
        canopy_wood_surface_cells=$(if($wood){'18192'}else{'0'})
        canopy_wood_enclosed_grid_cells=$(if($wood){'178'}else{'0'})
    }
    foreach($field in $expected.Keys){
        # 舊封存收據可能沒有新診斷欄位；存在時仍逐欄拒絕木質配置污染。
        if(($wood -or $Receipt.ContainsKey($field)) -and
            (-not $Receipt.ContainsKey($field) -or [string]$Receipt[$field] -cne $expected[$field])){
            throw ('Canopy collision contract mismatch: '+$field)
        }
    }
}

function Assert-MistwatchFirLayoutReceipt {
    param([Parameter(Mandatory=$true)][hashtable]$Receipt,
        [Parameter(Mandatory=$true)][ValidateSet('legacy-core','legacy-middle','forest-canopy','forest-wood')][string]$WorldProfile)
    $wood=$WorldProfile -eq 'forest-wood'
    $expected=@{
        fir_layout=$(if($wood){'forest-wood-mature'}else{'legacy'})
        fir_layout_population='100';fir_layout_reached_budget='1'
        fir_layout_fingerprint=$(if($wood){'8236695966185867318'}else{'14695981039346656037'})
        fir_layout_patch_centres=$(if($wood){'24'}else{'0'})
    }
    foreach($field in $expected.Keys){
        # 舊歷史收據可缺新增診斷欄；新木質布局必須完整提供，不能省略或少株。
        if(($wood -or $Receipt.ContainsKey($field)) -and
            (-not $Receipt.ContainsKey($field) -or [string]$Receipt[$field] -cne $expected[$field])){
            throw ('Mature fir layout contract mismatch: '+$field)
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
