param(
    [Parameter(Mandatory=$true)][string]$ExecutablePath,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [switch]$GpuValidation,
    [switch]$MiddleLayer,
    [switch]$BaselineMiddleLayer
)
$ErrorActionPreference='Stop'
if($MiddleLayer -and $BaselineMiddleLayer){throw 'Select either production middle layer or explicit old baseline'}
$useMiddleLayer=-not $BaselineMiddleLayer
. (Join-Path $PSScriptRoot 'forest_cover_contract.ps1')
$executable=(Resolve-Path -LiteralPath $ExecutablePath).Path
if([IO.Path]::GetFileName($executable) -ne 'Aevrynd-Mistwatch.exe'){throw 'Unexpected executable'}
$evidence=[IO.Path]::GetFullPath($EvidenceDirectory)
if(Test-Path -LiteralPath $evidence){throw 'Evidence directory already exists; preserve previous results'}
New-Item -ItemType Directory -Path $evidence | Out-Null
$variables=@('AEVRYND_MISTWATCH_CAPTURE','AEVRYND_MISTWATCH_SCENE','AEVRYND_MISTWATCH_CAPTURE_SERIES',
    'AEVRYND_MISTWATCH_BENCHMARK','AEVRYND_MISTWATCH_TREE_LOD','AEVRYND_MISTWATCH_TWIG_MODE','AEVRYND_MISTWATCH_MIDDLE_LAYER',
    'AEVRYND_MISTWATCH_CROWN','AEVRYND_MISTWATCH_NO_AO','AEVRYND_GPU_VALIDATION',
    'AEVRYND_MISTWATCH_CAPTURE_FRAME','AEVRYND_MISTWATCH_NO_GROUND_NORMAL','AEVRYND_MISTWATCH_NO_MSAA',
    'AEVRYND_MISTWATCH_HDR_STAGE','AEVRYND_MISTWATCH_NO_SHADOWS','AEVRYND_MISTWATCH_NO_BLOOM',
    'AEVRYND_MISTWATCH_NO_GROUND_TEXTURES','AEVRYND_MISTWATCH_GROUND_NEUTRAL_BASE',
    'AEVRYND_MISTWATCH_GROUND_NEUTRAL_SURFACE','AEVRYND_MSAA_STAGE_DIAGNOSTIC_SAMPLE',
    'AEVRYND_MISTWATCH_LANDSCAPE_PHASE_PROBE','AEVRYND_MISTWATCH_GROUND_LEGACY','AEVRYND_MISTWATCH_GROUND_SINGLE_SURFACE','AEVRYND_MISTWATCH_GRASS_LEGACY',
    'AEVRYND_MISTWATCH_FERN_LEGACY','AEVRYND_MISTWATCH_GRASS_DIM','AEVRYND_MISTWATCH_GRASS_PROCEDURAL','AEVRYND_MISTWATCH_GRASS_SCATTER_BASELINE',
    'AEVRYND_MISTWATCH_GRASS_FIXTURE','AEVRYND_MISTWATCH_GRASS_FIXTURE_ENABLED','AEVRYND_MISTWATCH_LEGACY_RANGER_HUT')
$saved=@{}
foreach($name in $variables){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$null,'Process')}
$results=@()
$cases=if($GpuValidation){@('river-overlook','strike','boar-recovery','repair-player-camera','fuse-lock-ready','relay-aligned','focus-pause','boulder-profile','grass-profile','fern-profile','supply-profile','ranger-hut-profile')}else{@('initial','repair-shelter','repair-player-camera','repair-ready','river','river-overlook','strike','boar-recovery','boar-profile','settlement-clue','settlement-clue-read','fuse-lock-wrong','fuse-lock-ready','panel','powered','relay-west','relay-aligned','relay-linked','complete','audio-check','focus-pause','boulder-profile','grass-profile','fern-profile','supply-profile','supply-collected','ranger-hut-profile')}
try {
    # 正常驗收使用遊戲預設，不以環境變數代替 production 接入；舊版只在明確比較時啟用。
    if($BaselineMiddleLayer){[Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_MIDDLE_LAYER','0','Process')}
    if($GpuValidation){[Environment]::SetEnvironmentVariable('AEVRYND_GPU_VALIDATION','1','Process')}
    foreach($case in $cases) {
        Write-Progress -Activity '霧境守望原生驗證' -Status $case -PercentComplete ([int](100*$results.Count/$cases.Count))
        $capture=Join-Path $evidence ($case+'.png')
        [Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_CAPTURE',$capture,'Process')
        [Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_SCENE',$(if($case -eq 'initial'){$null}else{$case}),'Process')
        $benchmark=(-not $GpuValidation) -and $case -in @('initial','river-overlook')
        [Environment]::SetEnvironmentVariable('AEVRYND_MISTWATCH_BENCHMARK',$(if($benchmark){'1'}else{$null}),'Process')
        $start=[DateTime]::UtcNow
        $process=Start-Process -FilePath $executable -WorkingDirectory (Split-Path $executable) -WindowStyle Hidden -PassThru
        $deadline=[DateTime]::UtcNow.AddSeconds($(if($GpuValidation){150}else{45}))
        while(-not $process.WaitForExit(1000) -and [DateTime]::UtcNow -lt $deadline) {}
        if(-not $process.HasExited) {
            throw ('Native observation timed out; inspect same live PID='+$process.Id+' case='+$case)
        }
        if($process.ExitCode -ne 0){throw ('Native validation failed: '+$case+' exit='+$process.ExitCode)}
        $receiptPath=$capture+'.receipt.txt'
        if(-not (Test-Path -LiteralPath $capture) -or -not (Test-Path -LiteralPath $receiptPath)){throw ('Missing native evidence: '+$case)}
        $receipt=@{}
        foreach($line in Get-Content -LiteralPath $receiptPath){$pair=$line.Split('=',2);if($pair.Count -eq 2){$receipt[$pair[0]]=$pair[1]}}
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
            $receipt.ranger_hut_visual -ne 'planked_pitched_roof' -or $receipt.ranger_hut_triangles -ne '832' -or
            $receipt.ranger_hut_wood_textured -ne '1' -or $receipt.ranger_hut_camera_roof -ne '1'){
            throw ('Ranger hut production geometry/material/camera missing: '+$case)
        }
        if($case -eq 'ranger-hut-profile' -and $receipt.camera_source -ne 'diagnostic'){throw 'Hut profile not proven'}
        if($receipt.supply_visual -ne 'planked_wood_crate' -or $receipt.supply_mesh_resources -ne '2' -or
            $receipt.supply_wood_triangles -ne '360' -or $receipt.supply_metal_triangles -ne '236' -or
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
        if($receipt.boulder_instances -ne '32' -or $receipt.boulder_mesh_triangles -ne '26611' -or
           $receipt.boulder_materials -ne '1' -or $receipt.boulder_gpu_resources -ne '1' -or
           $receipt.boulder_use_uv -ne '1' -or $receipt.boulder_texture_scale -ne '1'){throw ('Scanned boulder geometry/material binding incorrect: '+$case)}
        Assert-MistwatchTerrainCoverageMarker $receipt
        if($receipt.ground_source -ne 'forest-ground-04' -or $receipt.ground_maximum_resolution -ne '2048' -or
           $receipt.ground_packed_orm -ne '1' -or $receipt.ground_texture_scale -ne '0.25' -or
           $receipt.ground_use_uv -ne '1'){throw ('Forest floor production coverage material route missing: '+$case)}
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
        Assert-MistwatchForestCoverReceipt -Receipt $receipt -Scene $case -MiddleLayer:$useMiddleLayer
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
        if($GpuValidation) {
            foreach($flag in @('debug_layer_enabled','gpu_validation_requested','gpu_validation_available','gpu_validation_executed','gpu_validation_passed')) {
                if($receipt[$flag] -ne '1'){throw ('GPU validation not proven '+$flag+': '+$case)}
            }
            if($receipt.debug_warnings -ne '0'){throw ('GPU debug warnings: '+$case)}
        }
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
        $results+= [pscustomobject]@{case=$case;exit_code=$process.ExitCode;started_utc=$start.ToString('o');
            completed_utc=[DateTime]::UtcNow.ToString('o');capture=$capture;capture_sha256=(Get-FileHash -LiteralPath $capture -Algorithm SHA256).Hash;receipt=$receipt}
    }
    $report=[pscustomobject]@{schema=1;executable=$executable;executable_sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash;
        acceptance='native initialization/capture and fixed-scene GPU timing only; not manual playthrough or realistic-art acceptance';cases=$results}
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'native-validation.json') -Encoding UTF8
    'MISTWATCH_NATIVE_VALIDATION_PASS; CASES='+$results.Count+'; GPU_VALIDATION='+$GpuValidation.IsPresent
} finally {
    foreach($name in $variables){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}
    Write-Progress -Activity '霧境守望原生驗證' -Completed
}
