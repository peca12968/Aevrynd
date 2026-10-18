#requires -Version 7.0
param(
    [Parameter(Mandatory=$true)][string]$ExecutablePath,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$BuildRoot,
    [Parameter(Mandatory=$true)][string]$CertificateRoot,
    [switch]$GpuValidation,
    [switch]$InitialOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# 完整新版世界使用獨立版本入口；原始四樹／387部署與正式舊49項入口完全保留。
. (Join-Path $PSScriptRoot 'CurrentWorldContract.ps1')
. (Join-Path $PSScriptRoot 'CurrentWorldEvidence.ps1')
. (Join-Path $PSScriptRoot 'WorksiteContract.ps1')
function Read-MistwatchExactReceipt([string]$Path){
    $values=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    foreach($line in [IO.File]::ReadAllLines((Assert-MistwatchFallenPhysicalPath $Path),[Text.Encoding]::UTF8)){
        if([string]::IsNullOrWhiteSpace($line)){continue}
        $pair=$line.Split('=',2)
        if($pair.Count -ne 2 -or $pair[0] -cnotmatch '^[a-z][a-z0-9_]*$' -or $values.ContainsKey($pair[0])){
            throw ('Malformed or duplicate native receipt field: '+$Path)
        }
        $values.Add($pair[0],$pair[1])
    }
    return ,$values
}
function Assert-MistwatchTerrainCoverageMarker($Receipt){
    [double]$marker=0
    if(-not $Receipt.ContainsKey('ground_normal_scale') -or
       -not [double]::TryParse([string]$Receipt.ground_normal_scale,[Globalization.NumberStyles]::Float,
           [Globalization.CultureInfo]::InvariantCulture,[ref]$marker) -or
       -not [double]::IsFinite($marker) -or [Math]::Abs($marker+15.46875) -gt .0001){
        throw 'Forest floor production coverage marker missing or invalid'
    }
}
if($InitialOnly -and $GpuValidation){throw 'Initial-only qualification checkpoint is Normal only, never full49'}
$WorldProfile='forest-wood'
$FallenLogProfile='scanned-straight-v1'
$executable=Assert-MistwatchFallenPhysicalPath $ExecutablePath
$independent=Read-MistwatchCurrentWorldCertificate -CertificateRoot $CertificateRoot -SourceRoot $SourceRoot -BuildRoot $BuildRoot -ExecutablePath $executable
$certificateFiles=@(Get-ChildItem -LiteralPath $CertificateRoot -Force -Recurse -File|Sort-Object FullName|ForEach-Object{
    $path=Assert-MistwatchFallenPhysicalPath $_.FullName
    @{path=[IO.Path]::GetRelativePath($CertificateRoot,$path).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
})
$binarySha=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$evidence=[IO.Path]::GetFullPath($EvidenceDirectory)
if(Test-Path -LiteralPath $evidence){throw 'Evidence directory already exists; preserve previous results'}
$null=New-Item -ItemType Directory -Path $evidence
$variables=@(Get-ChildItem Env:|Where-Object Name -Like 'AEVRYND_*'|ForEach-Object Name)
$sharedEnvironment=@{
    AEVRYND_MISTWATCH_WORLD_PROFILE='forest-wood';AEVRYND_MISTWATCH_FALLEN_LOGS='1';
    AEVRYND_MISTWATCH_HABITAT_PROFILE='original';
    AEVRYND_MISTWATCH_SHELTER_DETAIL='framed-corrugated-v1';AEVRYND_MISTWATCH_WORKSHOP_FRAME='framed-workshop-v1';
    AEVRYND_MISTWATCH_ENTRANCE_ROAD='entrance-road-material-domain-v1';AEVRYND_MISTWATCH_ROADSIDE_COVER='roadside-broadleaf-v1';
    AEVRYND_MISTWATCH_SETTLEMENT_GROUND='workshop-ground-domain-v1';
    AEVRYND_MISTWATCH_SETTLEMENT_SURFACE='workshop-gravel-surface-candidate-v1';
    AEVRYND_MISTWATCH_ENTRANCE_UNDERSTORY='entrance-understory-v2';AEVRYND_MISTWATCH_BOULDER_COLOR='source-srgb-v1';
    AEVRYND_MISTWATCH_BOULDER_GEOMETRY='lod0-source-v1';AEVRYND_MISTWATCH_SHRUB_PROFILE='woodland-feasible-arc-v3';
    AEVRYND_MISTWATCH_LANDSCAPE_TWIG_PROFILE='crown-distance-lod-v1';
    AEVRYND_MISTWATCH_LANDSCAPE_STAND_PROFILE='landscape-stand-fill-v2';
    AEVRYND_MISTWATCH_FOREST_SURFACE_PROFILE='forest-moss-leaf-metric-v1';
    AEVRYND_MISTWATCH_FOREST_LIGHTING='forest-ambient-contrast-v1'
}
$variables=@($variables+@($sharedEnvironment.Keys)+@('AEVRYND_MISTWATCH_CAPTURE','AEVRYND_MISTWATCH_SCENE',
    'AEVRYND_MISTWATCH_CANOPY_VIEW','AEVRYND_MISTWATCH_CAPTURE_FRAME_CAMERA','AEVRYND_MISTWATCH_BENCHMARK','AEVRYND_GPU_VALIDATION')|Sort-Object -Unique)
$saved=@{}
foreach($name in $variables){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$null,'Process')}
$results=@();$worldResults=@();$worksiteResults=@();$process=$null;$activeCase=''
$cases=if($GpuValidation){
    @('river-overlook','strike','boar-recovery','repair-player-camera','fuse-lock-ready','relay-aligned','focus-pause',
      'boulder-profile','grass-profile','fern-profile','supply-profile','ranger-hut-profile','trail-profile','shrub-profile',
      'canopy-profile','canopy-root','canopy-detail')
}else{
    @('initial','repair-shelter','repair-player-camera','repair-ready','river','river-overlook','strike','boar-recovery',
      'boar-profile','settlement-clue','settlement-clue-read','fuse-lock-wrong','fuse-lock-ready','panel','powered',
      'relay-west','relay-aligned','relay-linked','complete','audio-check','focus-pause','boulder-profile','grass-profile',
      'fern-profile','supply-profile','supply-collected','ranger-hut-profile','trail-profile','shrub-profile',
      'canopy-profile','canopy-root','canopy-detail')
}
if($InitialOnly){$cases=@('initial')}
try {
    foreach($name in $sharedEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$sharedEnvironment[$name],'Process')}
    if($GpuValidation){[Environment]::SetEnvironmentVariable('AEVRYND_GPU_VALIDATION','1','Process')}
    foreach($case in $cases) {
        $activeCase=$case;$process=$null
        Write-Progress -Activity '霧境守望原生驗證' -Status $case -PercentComplete ([int](100*$results.Count/$cases.Count))
        $capture=Join-Path $evidence ($case+'.png')
        [Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_CAPTURE',$capture,'Process')
        # 根部與細節重用同一原生新樹鏡頭，僅選取已驗證的固定視角。
        $canopyCase=$case -in @('canopy-profile','canopy-root','canopy-detail')
        [Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_SCENE',$(if($case -eq 'initial'){$null}elseif($canopyCase){'canopy-profile'}else{$case}),'Process')
        [Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_CANOPY_VIEW',$(if($case -eq 'canopy-root'){'root'}elseif($case -eq 'canopy-detail'){'detail'}else{$null}),'Process')
        [Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_CAPTURE_FRAME_CAMERA',$(if($canopyCase){'1'}else{$null}),'Process')
        $benchmark=(-not $GpuValidation) -and $case -in @('initial','river-overlook')
        [Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_BENCHMARK',$(if($benchmark){'1'}else{$null}),'Process')
        $caseEnvironment=@{}
        foreach($entry in Get-ChildItem Env:|Where-Object Name -Like 'AEVRYND_*'){$caseEnvironment[$entry.Name]=$entry.Value}
        $start=[DateTime]::UtcNow
        $process=Start-Process -FilePath $executable -WorkingDirectory (Split-Path $executable) -WindowStyle Hidden -PassThru
        $deadline=[DateTime]::UtcNow.AddSeconds($(if($GpuValidation){150}else{45}))
        while(-not $process.WaitForExit(1000) -and [DateTime]::UtcNow -lt $deadline) {}
        if(-not $process.HasExited) {
            @{schema='mistwatch.current-native.live-process.1';case=$case;pid=$process.Id;started_utc=$start.ToString('o');
                observation_deadline_utc=$deadline.ToString('o');terminal=$false;do_not_restart=$true}|
                ConvertTo-Json|Set-Content -LiteralPath (Join-Path $evidence 'live-process.json') -Encoding utf8
            throw ('Native observation timed out; inspect same live PID='+$process.Id+' case='+$case)
        }
        if($process.ExitCode -ne 0){throw ('Native validation failed: '+$case+' exit='+$process.ExitCode)}
        $receiptPath=$capture+'.receipt.txt'
        if(-not (Test-Path -LiteralPath $capture) -or -not (Test-Path -LiteralPath $receiptPath)){throw ('Missing native evidence: '+$case)}
        $receipt=Read-MistwatchExactReceipt $receiptPath
        $worldResult=Assert-MistwatchCurrentWorldEvidence -Capture $capture -Scene $case -GpuValidation ([bool]$GpuValidation) -Expected $independent
        if(@($worldResult).Count -ne 1 -or $worldResult -isnot [Collections.IDictionary]){throw 'Current world evidence must return one dictionary'}
        $worldResults+=@{case=$case;contract=$worldResult}
        $worksiteResults+=@{case=$case;contract=(Assert-MistwatchJointWorksiteEvidence $capture $case ([bool]$GpuValidation))}
        if($case -eq 'shrub-profile' -and $receipt.camera_source -ne 'diagnostic'){throw 'Shrub profile not proven'}
        $shrubCleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.shrub-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$shrubCleanup[$pair[0]]=$pair[1]}}
        if($shrubCleanup.cleanup -ne 'success' -or $shrubCleanup.mesh_destroy_calls -ne '8'){throw ('Shrub cleanup incomplete: '+$case)}
        $expectedTrailFingerprint=$independent.native_contract.trail_litter_fingerprint
        $expectedTrailTriangles=$independent.native_contract.trail_litter_triangles
        if($receipt.trail_litter_enabled -ne '1' -or $receipt.trail_litter_patches -ne '48' -or
            $receipt.trail_litter_branches -ne '240' -or $receipt.trail_litter_stones -ne '336' -or
            $receipt.trail_litter_triangles -ne $expectedTrailTriangles -or $receipt.trail_litter_fingerprint -ne $expectedTrailFingerprint -or
            $receipt.submitted_trail_litter -ne '2'){throw ('Trail litter production contract missing: '+$case)}
        if($case -eq 'trail-profile' -and $receipt.camera_source -ne 'diagnostic'){throw 'Trail profile not proven'}
        $trailCleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.trail-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$trailCleanup[$pair[0]]=$pair[1]}}
        if($trailCleanup.cleanup -ne 'success' -or $trailCleanup.mesh_destroy_calls -ne '2'){throw ('Trail cleanup incomplete: '+$case)}
        $cleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.audio-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$cleanup[$pair[0]]=$pair[1]}}
        if($cleanup.cleanup -ne 'success' -or $cleanup.sounds -ne '0' -or $cleanup.bytes -ne '0' -or $cleanup.voices -ne '0'){throw ('Audio resources leaked: '+$case)}
        $treeCleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.broadleaf-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$treeCleanup[$pair[0]]=$pair[1]}}
        if($treeCleanup.cleanup -ne 'success' -or $treeCleanup.materials -ne '0' -or $treeCleanup.mesh_destroy_calls -ne '3'){throw ('Broadleaf cleanup incomplete: '+$case)}
        $boulderCleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.boulder-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$boulderCleanup[$pair[0]]=$pair[1]}}
        if($boulderCleanup.cleanup -ne 'success' -or $boulderCleanup.materials -ne '0' -or $boulderCleanup.mesh_destroy_calls -ne '2'){throw ('Boulder/log cleanup incomplete: '+$case)}
        $fernCleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.fern-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$fernCleanup[$pair[0]]=$pair[1]}}
        if($fernCleanup.cleanup -ne 'success' -or $fernCleanup.materials -ne '0' -or $fernCleanup.mesh_destroy_calls -ne '4'){throw ('Fern cleanup incomplete: '+$case)}
        $grassCleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.grass-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$grassCleanup[$pair[0]]=$pair[1]}}
        if($grassCleanup.cleanup -ne 'success' -or $grassCleanup.materials -ne '0' -or $grassCleanup.mesh_destroy_calls -ne '5'){throw ('Grass cleanup incomplete: '+$case)}
        $supplyCleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.supply-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$supplyCleanup[$pair[0]]=$pair[1]}}
        if($supplyCleanup.cleanup -ne 'success' -or $supplyCleanup.mesh_destroy_calls -ne '2'){throw ('Supply cleanup incomplete: '+$case)}
        $hutCleanup=@{}
        foreach($line in Get-Content -LiteralPath ($capture+'.hut-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count -eq 2){$hutCleanup[$pair[0]]=$pair[1]}}
        if($hutCleanup.cleanup -ne 'success' -or $hutCleanup.mesh_destroy_calls -ne '1' -or
            $receipt.ranger_hut_visual -ne 'planked_pitched_roof' -or $receipt.ranger_hut_triangles -ne '3200' -or
            $receipt.ranger_hut_wood_textured -ne '1' -or $receipt.ranger_hut_camera_roof -ne '1'){
            throw ('Ranger hut production geometry/material/camera missing: '+$case)
        }
        if($case -eq 'ranger-hut-profile' -and $receipt.camera_source -ne 'diagnostic'){throw 'Hut profile not proven'}
        # 原版非世界美術逐項精確驗收；32/17場景、品質與資源門檻保持。
        $artExpected=@{
            art_contract='mistwatch-art-v297';ranger_hut_mesh_resources='1';ranger_hut_indices='9600';
            ranger_hut_authored_uv='1';ranger_hut_visible_trim='1';ranger_hut_use_uv='1';
            ranger_hut_texture_scale='1';ranger_hut_normal_scale='0.22';
            explorer_vertices='9281';explorer_triangles='12344';explorer_surface_windows='9';
            explorer_surface_skin_windows='3';explorer_surface_eye_windows='2';explorer_surface_textured_windows='0';
            explorer_lens_drawlist_accepted='2';explorer_lens_indices='1404';explorer_surface_routes_match='1';
            renderer_accepted_skin_instances='3';renderer_accepted_eye_instances='2';
            renderer_accepted_hair_instances='1';renderer_accepted_cloth_instances='1';renderer_accepted_textured_skinned_instances='0';
            generator_static_triangles='1228';generator_indicator_triangles='24'
        }
        foreach($key in $artExpected.Keys){
            if(-not $receipt.ContainsKey($key) -or $receipt[$key] -cne $artExpected[$key]){
                throw ('New art production contract mismatch '+$key+': '+$case)
            }
        }
        if($receipt.generator_circuit -cnotmatch '^[0-7]$'){throw ('Invalid generator circuit: '+$case)}
        $generatorCleanup=Read-MistwatchExactReceipt ($capture+'.generator-cleanup.txt')
        if($generatorCleanup.Count -ne 3 -or $generatorCleanup['cleanup'] -cne 'success' -or
            $generatorCleanup['mesh_destroy_calls'] -cne '3' -or $generatorCleanup['cleared_resources'] -cne '3'){
            throw ('Generator resources leaked: '+$case)
        }
        if($receipt.supply_visual -ne 'planked_wood_crate' -or $receipt.supply_mesh_resources -ne '2' -or
            $receipt.supply_wood_triangles -ne '664' -or $receipt.supply_metal_triangles -ne '236' -or
            $receipt.supply_wood_textured -ne '1'){throw ('Supply production geometry/material missing: '+$case)}
        $mask=[int]$receipt.supply_collected_mask
        if($mask -lt 0 -or $mask -gt 7){throw ('Invalid supply mask: '+$case)}
        $expectedSupplies=3-[int][bool]($mask-band 1)-[int][bool]($mask-band 2)-[int][bool]($mask-band 4)
        if([int]$receipt.submitted_supply_crates -ne $expectedSupplies){throw ('Collected supply rendering mismatch: '+$case)}
        if($case -in @('supply-profile','supply-collected')) {
            $expectedMask=if($case -eq 'supply-collected'){1}else{0}
            if($mask -ne $expectedMask -or $receipt.camera_source -ne 'diagnostic'){throw ('Supply fixture not proven: '+$case)}
        }
        if($receipt.capture -ne 'success' -or $receipt.output_width -ne '1280' -or $receipt.output_height -ne '720') {throw ('Invalid capture receipt: '+$case)}
        if($receipt.debug_errors -ne '0' -or $receipt.debug_warnings -ne '0'){throw ('GPU debug messages: '+$case)}
        if($receipt.multisample_count -ne '4'){throw ('Production MSAA missing: '+$case)}
        foreach($flag in @('quality_shadows','quality_ao','quality_post_aa','ground_textured')){
            if($receipt[$flag] -ne '1'){throw ('Production quality missing '+$flag+': '+$case)}
        }
        foreach($flag in @('diagnostic_no_shadows','diagnostic_no_msaa','diagnostic_no_bloom','diagnostic_hdr_stage','ground_neutral_base','ground_neutral_surface')){
            if($receipt[$flag] -ne '0'){throw ('Diagnostic override remains '+$flag+': '+$case)}
        }
        if($case -eq 'focus-pause' -and ($receipt.focus_pause_verified -ne '1' -or
            $receipt.focus_explicit_resume_verified -ne '1' -or $receipt.focus_frozen_ticks -ne '17' -or
            $receipt.paused -ne '1' -or $receipt.camera_source -ne 'player')){throw 'Actual game focus-pause/resume path not proven'}
        if($receipt.audio_sounds -ne '14' -or [long]$receipt.audio_bytes -le 0 -or [long]$receipt.audio_bytes -gt 1000000 -or $receipt.audio_master -ne '0'){throw ('Audio bank missing or automated capture not muted: '+$case)}
        if($case -eq 'audio-check' -and ($receipt.audio_device -ne '1' -or [long]$receipt.audio_plays -lt 10 -or
            [long]$receipt.audio_step_requests -lt 8 -or [long]$receipt.audio_interact_requests -lt 1 -or
            $receipt.audio_wind_requests -ne '2' -or [long]$receipt.audio_probe_ticks -lt 180 -or
            $receipt.audio_dropped -ne '0' -or [long]$receipt.audio_peak_voices -gt 8 -or $receipt.camera_source -ne 'player')){throw 'Native audio walk/collect/pause-resume playback not proven'}
        if($receipt.landscape_vertices -ne '25480' -or $receipt.landscape_triangles -ne '49000' -or
           $receipt.landscape_trees -ne '520'){throw ('Missing landscape geography or distant forest: '+$case)}
        if($receipt.broadleaf_trees -ne '12' -or $receipt.broadleaf_mesh_triangles -ne '82854' -or
           $receipt.broadleaf_materials -ne '3'){throw ('Missing rooted broadleaf trees or verified materials: '+$case)}
        # 地形、LOD0 岩石與153欄新版完整世界由獨立 CPU 證書及同 handle 清理契約驗證。
        Assert-MistwatchTerrainCoverageMarker $receipt
        if($receipt.ground_material_class -ne '3' -or $receipt.ground_texture_layers -ne '3' -or
           $receipt.ground_single_surface -ne '0' -or $receipt.ground_height_alpha -ne 'shared_core_normalized' -or
           $receipt.ground_distance_normal_strength -ne '0.65' -or $receipt.ground_meso_partition -ne '0' -or
           [long]$receipt.submitted_terrain_instances -lt 1){throw ('Three-layer terrain production submission missing: '+$case)}
        if($receipt.grass_style -ne 'scanned' -or $receipt.grass_vertices -ne '7031' -or $receipt.grass_triangles -ne '7842' -or
           $receipt.grass_variants -ne '5' -or $receipt.grass_source -ne 'poly-haven-grass-medium-02' -or
           $receipt.grass_materials -ne '1' -or $receipt.grass_use_uv -ne '1' -or $receipt.grass_alpha_cutoff -ne '0.5' -or
           $receipt.grass_tint_scale -ne '1' -or $receipt.ground_cover_instances -ne '3600' -or
           [int]$receipt.submitted_grass -lt 1 -or [int]$receipt.submitted_grass_triangles -lt 714){throw ('Scanned grass production binding missing: '+$case)}
        if($receipt.fern_source -ne 'poly-haven-fern-02' -or $receipt.fern_variants -ne '4' -or
           $receipt.fern_asset_scale -ne '1.6' -or $receipt.fern_materials -ne '1' -or
           $receipt.fern_use_uv -ne '1' -or $receipt.fern_alpha_cutoff -ne '0.5' -or
           $receipt.understory_ferns -ne '420' -or [int]$receipt.submitted_ferns -lt 1 -or
           [int]$receipt.submitted_fern_triangles -lt 1){throw ('Scanned fern production binding missing: '+$case)}
        if($case -in @('boulder-profile','grass-profile','fern-profile')) {
            if($receipt.camera_source -ne 'diagnostic'){throw 'Detail fixture missing diagnostic camera'}
            $position=@($receipt.camera_position.Split(',')|ForEach-Object{[double]::Parse($_,[Globalization.CultureInfo]::InvariantCulture)})
            $target=@($receipt.camera_target.Split(',')|ForEach-Object{[double]::Parse($_,[Globalization.CultureInfo]::InvariantCulture)})
            if($position.Count -ne 3 -or $target.Count -ne 3){throw 'Boulder camera receipt invalid'}
            $offset=if($case -eq 'grass-profile'){@(1.15,.55,-1.15)}elseif($case -eq 'fern-profile'){@(.95,.55,-1.2)}else{@(2,1.1,-2)}
            for($axis=0;$axis -lt 3;++$axis){
                if([double]::IsNaN($position[$axis]) -or [double]::IsInfinity($position[$axis]) -or
                    [double]::IsNaN($target[$axis]) -or [double]::IsInfinity($target[$axis]) -or
                    [Math]::Abs($position[$axis]-$target[$axis]-$offset[$axis]) -gt .001){throw 'Boulder detail camera offset mismatch'}
            }
        }
        if($receipt.enemy_visual -ne 'skinned_boar' -or $receipt.enemy_joints -ne '16'){throw ('Missing skinned wildlife: '+$case)}
        if($case -eq 'strike' -and ($receipt.enemy_phase -ne '1' -or [double]$receipt.enemy_brace -lt .4)){throw 'Missing windup posture fixture'}
        if($case -eq 'boar-recovery' -and ($receipt.enemy_phase -ne '2' -or [double]$receipt.enemy_brace -ne 0)){throw 'Missing recovery posture fixture'}
        if($case -in @('relay-west','relay-aligned','relay-linked')) {
            $expectedLinked=if($case -eq 'relay-linked'){'1'}else{'0'}
            $expectedSectors=if($case -eq 'relay-west'){'0'}else{$receipt.relay_target_west}
            if($receipt.mission_stage -ne '6' -or $receipt.relay_linked -ne $expectedLinked -or
               $receipt.relay_sectors -ne $expectedSectors -or $receipt.camera_source -ne 'player') {
                throw ('Relay fixture state or player camera mismatch: '+$case)
            }
            if($case -ne 'relay-west' -and $receipt.relay_signal_west -ne '100'){throw 'Aligned relay missing full directional signal'}
        }
        if([int]$receipt.understory_ferns -lt 220 -or [int]$receipt.understory_ferns -gt 420 -or
           [int]$receipt.understory_saplings -lt 30 -or [int]$receipt.understory_saplings -gt 42) {
            throw ('Missing layered forest population: '+$case)
        }
        if($case -eq 'initial' -and ([int]$receipt.submitted_ferns -lt 30 -or
           [int]$receipt.submitted_saplings -ne [int]$receipt.understory_saplings)) {
            throw 'Initial player view did not submit expected understory geometry'
        }
        if($case -in @('repair-shelter','repair-player-camera') -and ($receipt.character_height -ne '1.4' -or $receipt.mission_stage -ne '1' -or $receipt.repair_materials -ne '0')){throw 'Shelter capture does not prove expected crouched mission fixture'}
        if($case -eq 'repair-player-camera') {
            if($receipt.camera_source -ne 'player'){throw 'Shelter camera was replaced by diagnostic view'}
            $distance=[double]::Parse($receipt.camera_distance,[Globalization.CultureInfo]::InvariantCulture)
            $height=[double]::Parse($receipt.camera_height,[Globalization.CultureInfo]::InvariantCulture)
            $anchor=[double]::Parse($receipt.camera_anchor_above_feet,[Globalization.CultureInfo]::InvariantCulture)
            if([double]::IsNaN($distance) -or [double]::IsInfinity($distance) -or $distance -lt 1.5 -or
                [double]::IsNaN($height) -or [double]::IsInfinity($height) -or $height -gt 1.25001 -or $height -lt .2 -or
                [double]::IsNaN($anchor) -or [double]::IsInfinity($anchor) -or [Math]::Abs($anchor-.85) -gt .00001){throw 'Invalid shelter player-camera evidence'}
        }
        if($case -eq 'repair-ready' -and ($receipt.mission_stage -ne '2' -or $receipt.repair_materials -ne '7')){throw 'Bridge-ready capture does not prove repair inventory'}
        if($case -in @('settlement-clue','settlement-clue-read','fuse-lock-wrong','fuse-lock-ready')) {
            $expectedClue=if($case -eq 'settlement-clue'){'0'}else{'1'}
            $expectedDials=if($case -eq 'fuse-lock-ready'){'50'}else{'0'}
            if($receipt.mission_stage -ne '4' -or $receipt.fuse_collected -ne '0' -or
                $receipt.fuse_clue_read -ne $expectedClue -or $receipt.fuse_dials -ne $expectedDials -or
                $receipt.camera_source -ne 'player'){throw 'Invalid settlement investigation fixture or substituted camera'}
        }
        Assert-MistwatchCurrentGpuMode $receipt ([bool]$GpuValidation)
        if($case -in @('river','river-overlook')) {
            foreach($flag in @('water_single_composite','refraction','screen_space_reflections')) {
                if($receipt[$flag] -ne '1'){throw ('Missing water pipeline '+$flag+': '+$case)}
            }
            foreach($counter in @('water_composite_draws','refraction_passes','reflection_passes')) {
                if([long]$receipt[$counter] -le 0){throw ('Water pipeline did not execute '+$counter+': '+$case)}
            }
        }
        if($benchmark) {
            if($receipt.gpu_samples -ne '120' -or $receipt.benchmark_mode -ne 'fixed_scene_gpu_only'){throw ('Missing timing evidence: '+$case)}
            foreach($metric in @('gpu_mean_ms','gpu_p50_ms','gpu_p95_ms','gpu_max_ms')) {
                $value=[double]::Parse($receipt[$metric],[Globalization.CultureInfo]::InvariantCulture)
                if([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -le 0){throw ('Invalid GPU timing: '+$case)}
            }
        }
        # 桌子的真網格與回收必須獨立成立，不能只看到木屋就算道具通過。
        foreach($pair in @(@('ranger_desk_mesh_resources','1'),@('ranger_desk_vertices','216'),
            @('ranger_desk_triangles','108'),@('ranger_desk_indices','324'),
            @('ranger_desk_scene_submissions','1'),@('ranger_desk_wood_textured','1'))){
            if(-not $receipt.ContainsKey($pair[0]) -or $receipt[$pair[0]] -cne $pair[1]){throw ('Invalid ranger desk resource: '+$case+' '+$pair[0])}
        }
        [ulong]$deskHandle=0
        if(-not $receipt.ContainsKey('ranger_desk_resource_handle') -or
           -not [ulong]::TryParse($receipt['ranger_desk_resource_handle'],[Globalization.NumberStyles]::None,
               [Globalization.CultureInfo]::InvariantCulture,[ref]$deskHandle) -or $deskHandle -eq 0){throw 'Missing true ranger desk handle'}
        $deskCleanup=Read-MistwatchExactReceipt ($capture+'.desk-cleanup.txt')
        if($deskCleanup.Count -ne 4 -or $deskCleanup['mesh_destroy_calls'] -cne '1' -or
           $deskCleanup['destroyed_handle'] -cne $receipt['ranger_desk_resource_handle'] -or
           $deskCleanup['cleared_resources'] -cne '1' -or $deskCleanup['cleanup'] -cne 'success'){
            throw ('Ranger desk cleanup not proven: '+$case)
        }
        $results+= [pscustomobject]@{case=$case;exit_code=$process.ExitCode;started_utc=$start.ToString('o');
            completed_utc=[DateTime]::UtcNow.ToString('o');pid=$process.Id;environment=$caseEnvironment;outer_deadline_seconds=$(if($GpuValidation){150}else{45});internal_deadline_seconds=$(if($GpuValidation){120}else{30});capture=$capture;capture_sha256=(Get-FileHash -LiteralPath $capture -Algorithm SHA256).Hash;receipt=$receipt}
    }
    $null=Read-MistwatchCurrentWorldCertificate -CertificateRoot $CertificateRoot -SourceRoot $SourceRoot -BuildRoot $BuildRoot -ExecutablePath $executable
    Assert-MistwatchFallenRelativeRows $certificateFiles $CertificateRoot 'Current matrix frozen CPU certificate'
    $certificateAfter=@(Get-ChildItem -LiteralPath $CertificateRoot -Force -Recurse -File|ForEach-Object{[IO.Path]::GetRelativePath($CertificateRoot,$_.FullName).Replace('\','/')}|Sort-Object)
    if(($certificateAfter-join "`n") -cne (@($certificateFiles.path|Sort-Object)-join "`n")){throw 'CPU certificate membership drifted during matrix'}
    if((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -cne $binarySha){throw 'Game executable drifted during matrix'}
    if($worldResults.Count -ne $cases.Count -or $worksiteResults.Count -ne $cases.Count){throw 'Missing actual current world/worksite cases'}
    $report=[ordered]@{
        schema='mistwatch.current-native-validation.1';
        status=$(if($InitialOnly){'INITIAL_ONLY_NOT_FULL49'}else{'CURRENT_MATRIX_MODE_PASSED'});
        world_cohort='entrance-current-forest-surface-v1';runtime_profile='authored-current-forest-full-v5';
        world_profile=$WorldProfile;fallen_log_profile=$FallenLogProfile;gpu_validation=[bool]$GpuValidation;
        initial_only=[bool]$InitialOnly;full49_qualified=$false;
        executable=$executable;executable_sha256=$binarySha;certificate_root=[IO.Path]::GetFullPath($CertificateRoot);
        certificate_files=$certificateFiles;shared_environment=$sharedEnvironment;world_cases=$worldResults;worksite_cases=$worksiteResults;
        acceptance='Current native initialization/capture, gameplay fixtures and fixed-scene GPU timing only; not manual playthrough or realistic-art acceptance';
        cases=$results
    }
    $report|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $evidence 'native-validation.json') -Encoding utf8
    Write-Output ('MISTWATCH_CURRENT_NATIVE_VALIDATION_PASS; CASES='+$results.Count+'; GPU_VALIDATION='+[bool]$GpuValidation+'; INITIAL_ONLY='+[bool]$InitialOnly+'; FULL49=0')
} catch {
    $originalError=$_
    $nativeProcess=@{status='not_started';pid=$null;has_exited=$null;exit_code=$null;observed_utc=[DateTime]::UtcNow.ToString('o')}
    try {
        if($null -ne $process){
            $nativeProcess.pid=$process.Id
            $nativeProcess.has_exited=$process.HasExited
            if($nativeProcess.has_exited -isnot [bool]){throw 'Native process exit observation unavailable'}
            $nativeProcess.status=if($nativeProcess.has_exited){'exited'}else{'running'}
            if($nativeProcess.has_exited){$nativeProcess.exit_code=$process.ExitCode}
        }
    } catch {$nativeProcess.status='observation_unavailable';$nativeProcess.observation_error=$_.Exception.Message}
    # 空矩陣也必須保留原始第一個分歧；診斷輸出失敗不能掩蓋真正錯誤或啟動第二個程序。
    try {
        @{schema='mistwatch.current-native-validation.1';status='failed';first_divergence=$originalError.Exception.Message;
            exception_type=$originalError.Exception.GetType().FullName;exception_text=$originalError.Exception.ToString();
            script_stack_trace=[string]$originalError.ScriptStackTrace;
            position_message=$(if($null -ne $originalError.InvocationInfo){[string]$originalError.InvocationInfo.PositionMessage}else{''});
            initial_only=[bool]$InitialOnly;gpu_validation=[bool]$GpuValidation;
            completed_case_names=@($results|ForEach-Object{$_.case});active_case=$activeCase;native_process=$nativeProcess;
            live_process_record_path=(Join-Path $evidence 'live-process.json');
            preserve_first_results=$true;no_kill_or_restart=$true}|
            ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $evidence 'native-failure.json') -Encoding utf8
    } catch {
        try{[Console]::Error.WriteLine('Native failure diagnostic write rejected: '+$_.Exception.Message)}catch{}
    }
    throw $originalError
} finally {
    foreach($name in $variables){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}
    Write-Progress -Activity '霧境守望原生驗證' -Completed
}
