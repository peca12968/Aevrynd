param(
    [Parameter(Mandatory=$true)][string]$ExecutablePath,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [switch]$GpuValidation
)
$ErrorActionPreference='Stop'
$executable=(Resolve-Path -LiteralPath $ExecutablePath).Path
if([IO.Path]::GetFileName($executable) -ne 'Aevrynd-Mistwatch.exe'){throw 'Unexpected executable'}
$evidence=[IO.Path]::GetFullPath($EvidenceDirectory)
if(Test-Path -LiteralPath $evidence){throw 'Evidence directory already exists; preserve previous results'}
New-Item -ItemType Directory -Path $evidence | Out-Null
$variables=@('AEVRYND_MISTWATCH_CAPTURE','AEVRYND_MISTWATCH_SCENE','AEVRYND_MISTWATCH_CAPTURE_SERIES',
    'AEVRYND_MISTWATCH_BENCHMARK','AEVRYND_MISTWATCH_TREE_LOD','AEVRYND_MISTWATCH_TWIG_MODE',
    'AEVRYND_MISTWATCH_CROWN','AEVRYND_MISTWATCH_NO_AO','AEVRYND_GPU_VALIDATION')
$saved=@{}
foreach($name in $variables){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$null,'Process')}
$results=@()
$cases=if($GpuValidation){@('river-overlook','strike','boar-recovery','repair-player-camera','fuse-lock-ready','relay-aligned')}else{@('initial','repair-shelter','repair-player-camera','repair-ready','river','river-overlook','strike','boar-recovery','boar-profile','settlement-clue','settlement-clue-read','fuse-lock-wrong','fuse-lock-ready','panel','powered','relay-west','relay-aligned','relay-linked','complete')}
try {
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
            $process.Kill();$process.WaitForExit()
            throw ('Native validation timed out: '+$case)
        }
        if($process.ExitCode -ne 0){throw ('Native validation failed: '+$case+' exit='+$process.ExitCode)}
        $receiptPath=$capture+'.receipt.txt'
        if(-not (Test-Path -LiteralPath $capture) -or -not (Test-Path -LiteralPath $receiptPath)){throw ('Missing native evidence: '+$case)}
        $receipt=@{}
        foreach($line in Get-Content -LiteralPath $receiptPath){$pair=$line.Split('=',2);if($pair.Count -eq 2){$receipt[$pair[0]]=$pair[1]}}
        if($receipt.capture -ne 'success' -or $receipt.output_width -ne '1280' -or $receipt.output_height -ne '720') {throw ('Invalid capture receipt: '+$case)}
        if($receipt.debug_errors -ne '0'){throw ('GPU debug errors: '+$case)}
        if($receipt.landscape_vertices -ne '25480' -or $receipt.landscape_triangles -ne '49000' -or
           $receipt.landscape_trees -ne '520'){throw ('Missing landscape geography or distant forest: '+$case)}
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
