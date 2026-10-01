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
$cases=if($GpuValidation){@('river-overlook','strike')}else{@('initial','river','river-overlook','strike','panel','powered','complete')}
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
