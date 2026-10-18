#requires -Version 7.0
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$BuildRoot,
    [Parameter(Mandatory=$true)][string]$ExecutablePath,
    [Parameter(Mandatory=$true)][string]$CertificateRoot,
    [Parameter(Mandatory=$true)][ValidateSet('normal','gbv')][string]$Mode,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$RunId='default',
    [switch]$InitialOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
# 目前森林的完整資格入口；舊版387部署、四樹證書及原32/17入口不改寫、不借用。
. (Join-Path $PSScriptRoot 'CurrentWorldContract.ps1')
. (Join-Path $PSScriptRoot 'CurrentWorldEvidence.ps1')
. (Join-Path $PSScriptRoot 'WorksiteContract.ps1')
$roots=Assert-MistwatchFallenBuildRoots $SourceRoot $BuildRoot
$source=$roots.source;$build=$roots.build
$exe=Assert-MistwatchFallenPhysicalPath $ExecutablePath
$certificate=Assert-MistwatchFallenPhysicalPath $CertificateRoot -Directory
if($exe -ine [IO.Path]::GetFullPath((Join-Path $build 'bin/Release/Aevrynd-Mistwatch.exe'))){
    throw 'Exact current CMake Mistwatch target required'
}
if($InitialOnly -and $Mode -cne 'normal'){throw 'Initial-only checkpoint is Normal only'}
$suffix=$Mode+$(if($InitialOnly){'-initial-only'}else{''})
if($RunId -cne 'default'){$suffix+='-'+$RunId}
$evidence=Join-Path $build ('test-artifacts/mistwatch-current-native/'+$suffix)
$workspace=Join-Path $build ('test-workspaces/mistwatch-current-native/'+$suffix)
if(Test-Path -LiteralPath $evidence){throw 'Preserve first current matrix evidence; no overwrite/restart'}
if(Test-Path -LiteralPath $workspace){throw 'Preserve first isolated current user data; no overwrite/restart'}
$validator=Join-Path $PSScriptRoot 'validate_current_native_capture.ps1'
$expected=if($Mode -ceq 'normal'){
    @('initial','repair-shelter','repair-player-camera','repair-ready','river','river-overlook','strike',
      'boar-recovery','boar-profile','settlement-clue','settlement-clue-read','fuse-lock-wrong',
      'fuse-lock-ready','panel','powered','relay-west','relay-aligned','relay-linked','complete',
      'audio-check','focus-pause','boulder-profile','grass-profile','fern-profile','supply-profile',
      'supply-collected','ranger-hut-profile','trail-profile','shrub-profile','canopy-profile','canopy-root','canopy-detail')
}else{
    @('river-overlook','strike','boar-recovery','repair-player-camera','fuse-lock-ready','relay-aligned',
      'focus-pause','boulder-profile','grass-profile','fern-profile','supply-profile','ranger-hut-profile',
      'trail-profile','shrub-profile','canopy-profile','canopy-root','canopy-detail')
}
if($InitialOnly){$expected=@('initial')}
function Get-CurrentRow([string]$Path,[string]$Label){
    $full=Assert-MistwatchFallenPhysicalPath $Path
    $item=Get-Item -LiteralPath $full -Force
    return @{path=$Label;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash}
}
function Get-CurrentTreeRows([string]$Root){
    $physical=Assert-MistwatchFallenPhysicalPath $Root -Directory
    $rows=@(foreach($item in Get-ChildItem -LiteralPath $physical -Force -Recurse){
        if($item.Attributes-band [IO.FileAttributes]::ReparsePoint){throw ('Physical tree reparse: '+$item.FullName)}
        if($item.PSIsContainer){continue}
        Get-CurrentRow $item.FullName ([IO.Path]::GetRelativePath($physical,$item.FullName).Replace('\','/'))
    })
    return @($rows|Sort-Object path)
}
function Get-CurrentSourceRows{
    $names=@('CMakeLists.txt','CMakePresets.json','engine/native/CMakeLists.txt','games/mistwatch/native/CMakeLists.txt')
    foreach($directory in @('engine/native','games/mistwatch/native','games/mistwatch/tools')){
        $root=Assert-MistwatchFallenPhysicalPath (Join-Path $source $directory) -Directory
        foreach($item in Get-ChildItem -LiteralPath $root -Force -Recurse){
            if($item.Attributes-band [IO.FileAttributes]::ReparsePoint){throw ('Current source reparse: '+$item.FullName)}
            if($item.PSIsContainer -or $item.Extension -ceq '.pyc'){continue}
            $names+=[IO.Path]::GetRelativePath($source,$item.FullName).Replace('\','/')
        }
    }
    return @(foreach($name in $names|Sort-Object -Unique){Get-CurrentRow (Join-Path $source $name) $name})
}
function Assert-CurrentRowsEqual($Before,$After,[string]$Label){
    $a=@($Before|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    $b=@($After|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    if(($a-join "`n") -cne ($b-join "`n")){throw ($Label+' membership/raw bytes drifted')}
}
function Write-CurrentRecord([string]$Path,$Value){$Value|ConvertTo-Json -Depth 20|Set-Content -LiteralPath $Path -Encoding utf8}
$savedEnvironment=@{}
$sourceRows=@();$runtimeRows=@();$certificateRows=@();$binaryRow=$null;$producerRows=@()
try {
    # 讀取器先驗418逐檔譜系、版本化 CPU 完整世界與來源；本層再凍結矩陣前後實體身分。
    $null=Read-MistwatchCurrentWorldCertificate -CertificateRoot $certificate -SourceRoot $source -BuildRoot $build -ExecutablePath $exe
    $sourceRows=@(Get-CurrentSourceRows)
    $assets=Join-Path (Split-Path $exe) 'assets'
    $runtimeRows=@(Get-CurrentTreeRows $assets)
    if($runtimeRows.Count -ne 418){throw 'Current complete runtime must have exact418 reviewed members'}
    $certificateRows=@(Get-CurrentTreeRows $certificate)
    $binaryRow=Get-CurrentRow $exe ([IO.Path]::GetRelativePath($build,$exe).Replace('\','/'))
    $generated=Join-Path $build 'games/mistwatch/native/generated-root-stem'
    foreach($pair in @(
        @('island-tree-01-lod2-trunk-root-stem.aemesh','meshes/island-tree-01-lod2-trunk-root-stem.aemesh'),
        @('author-receipt.json','root-stem-derivation.json'))){
        $from=Join-Path $generated $pair[0]
        $to=Join-Path $assets ('mistwatch-canopy-preview/'+$pair[1])
        $actual=Get-CurrentRow $from ([IO.Path]::GetRelativePath($build,$from).Replace('\','/'))
        $copy=Get-CurrentRow $to $pair[1]
        if($actual.bytes -ne $copy.bytes -or $actual.sha256 -cne $copy.sha256){throw 'Fresh generated root copy differs'}
        $producerRows+=$actual
    }
    foreach($name in @('MistwatchBoarAsset.exe','MistwatchFallenWorldQualificationTest.exe')){
        $from=Join-Path (Split-Path $exe) $name
        $producerRows+=Get-CurrentRow $from ([IO.Path]::GetRelativePath($build,$from).Replace('\','/'))
    }
    $null=New-Item -ItemType Directory -Path $workspace
    foreach($name in @('APPDATA','LOCALAPPDATA')+@(Get-ChildItem Env:|Where-Object Name -Like 'AEVRYND_*'|ForEach-Object Name)){
        if(-not $savedEnvironment.ContainsKey($name)){$savedEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
        [Environment]::SetEnvironmentVariable($name,$null,'Process')
    }
    foreach($name in @('APPDATA','LOCALAPPDATA')){
        $user=Join-Path $workspace $name;$null=New-Item -ItemType Directory -Path $user
        [Environment]::SetEnvironmentVariable($name,$user,'Process')
    }
    $parameters=@{ExecutablePath=$exe;EvidenceDirectory=$evidence;SourceRoot=$source;BuildRoot=$build;CertificateRoot=$certificate}
    if($Mode -ceq 'gbv'){$parameters.GpuValidation=$true}
    if($InitialOnly){$parameters.InitialOnly=$true}
    & $validator @parameters
    $report=Read-MistwatchFallenStrictJson (Join-Path $evidence 'native-validation.json')
    $status=if($InitialOnly){'INITIAL_ONLY_NOT_FULL49'}else{'CURRENT_MATRIX_MODE_PASSED'}
    if($report.schema -cne 'mistwatch.current-native-validation.1' -or $report.status -cne $status -or
       $report.world_cohort -cne 'entrance-current-forest-surface-v1' -or $report.runtime_profile -cne 'authored-current-forest-full-v5' -or
       $report.executable_sha256 -cne $binaryRow.sha256 -or
       $report.gpu_validation -isnot [bool] -or $report.gpu_validation -ne ($Mode -ceq 'gbv') -or
       $report.initial_only -isnot [bool] -or $report.initial_only -ne [bool]$InitialOnly -or
       $report.full49_qualified -isnot [bool] -or $report.full49_qualified -or
       (@($report.cases.case)-join "`n") -cne ($expected-join "`n")){
        throw 'Current native report does not contain exact ordered production32/17 or explicit initial-only checkpoint'
    }
    if(@($report.world_cases).Count -ne $expected.Count -or @($report.worksite_cases).Count -ne $expected.Count){throw 'Missing full current world/worksite cases'}
    $captureRows=@()
    foreach($case in $report.cases){
        if($case.exit_code -ne 0){throw 'Nonzero current native case result'}
        $capture=Join-Path $evidence ($case.case+'.png')
        if([IO.Path]::GetFullPath($case.capture) -cne [IO.Path]::GetFullPath($capture)){throw 'Current capture escaped fixed output'}
        $png=Get-CurrentRow $capture ($case.case+'.png')
        if($png.sha256 -cne $case.capture_sha256){throw 'Current actual PNG/report raw identity differs'}
        $null=Assert-MistwatchJointPngBytes ([IO.File]::ReadAllBytes($capture))
        if($case.outer_deadline_seconds -ne $(if($Mode -ceq 'gbv'){150}else{45}) -or
           $case.internal_deadline_seconds -ne $(if($Mode -ceq 'gbv'){120}else{30})){throw 'Current matrix deadline drift'}
        foreach($forbidden in @('AEVRYND_MISTWATCH_CAPTURE_COST_PROBE','AEVRYND_MISTWATCH_CAPTURE_PRESENT')){
            if($case.environment.Contains($forbidden)){throw 'Static cost/offscreen recipe cannot replace full production matrix'}
        }
        Assert-MistwatchCurrentFrameCameraEnvironment $case.environment $case.case (Read-MistwatchCurrentWorldReceipt ($capture+'.receipt.txt'))
        $captureRows+=@{case=$case.case;png=$png;receipt=(Get-CurrentRow ($capture+'.receipt.txt') ($case.case+'.png.receipt.txt'))}
    }
    $null=Read-MistwatchCurrentWorldCertificate -CertificateRoot $certificate -SourceRoot $source -BuildRoot $build -ExecutablePath $exe
    Assert-CurrentRowsEqual $sourceRows @(Get-CurrentSourceRows) 'Complete current source'
    Assert-CurrentRowsEqual $runtimeRows @(Get-CurrentTreeRows $assets) 'Complete current418 runtime'
    Assert-CurrentRowsEqual $certificateRows @(Get-CurrentTreeRows $certificate) 'Fresh complete current CPU certificate'
    Assert-MistwatchFallenRelativeRows (@($binaryRow)+$producerRows) $build 'Current game and producers'
    $eight=@('mistwatch-boulders','mistwatch-broadleaf','mistwatch-canopy-preview','mistwatch-ferns','mistwatch-forest','mistwatch-grass','mistwatch-materials','mistwatch-wildlife')
    $eightRows=@($runtimeRows|Where-Object{$_.path.Split('/')[0] -cin $eight})
    if($eightRows.Count -ne 142){throw 'Current original eight-group lineage must have exact142 reviewed files'}
    Write-CurrentRecord (Join-Path $evidence 'ctest-receipt.json') ([ordered]@{
        schema='mistwatch.current-native-ctest.1';status=$status;mode=$Mode;run_id=$RunId;
        world_cohort='entrance-current-forest-surface-v1';runtime_profile='authored-current-forest-full-v5';
        cases=$expected.Count;case_names=$expected;initial_only=[bool]$InitialOnly;full49_qualified=$false;
        per_case_seconds=$(if($Mode -ceq 'gbv'){150}else{45});
        internal_capture_seconds=$(if($Mode -ceq 'gbv'){120}else{30});
        executable=$binaryRow;producer_files=$producerRows;source_files=$sourceRows;
        runtime_files=$runtimeRows;full_runtime_members=$runtimeRows.Count;original_eight_groups_members=$eightRows.Count;
        certificate_root=$certificate;certificate_files=$certificateRows;capture_files=$captureRows;
        world_cases=$report.world_cases;worksite_cases=$report.worksite_cases;
        native_validation_sha256=(Get-FileHash -LiteralPath (Join-Path $evidence 'native-validation.json') -Algorithm SHA256).Hash;
        source_binding_scope='Complete game/native, game/tools and engine/native raw membership before/after plus independently qualified CPU source/material closure; root binds fresh compile/link/Job evidence';
        runtime_binding_scope='Exact reviewed current418 lineage, separate142 eight-group membership, fresh generated root and producer identity before/after';
        own_window_hidden=1;foreground_activation=0;global_input=0;manual_play_accepted=$false;realistic_art_accepted=$false;published=$false
    })
    Write-Output ('MISTWATCH_CURRENT_CTEST_MATRIX_PASS; MODE='+$Mode+'; CASES='+$expected.Count+'; INITIAL_ONLY='+[bool]$InitialOnly+'; FULL49=0')
} catch {
    if(-not(Test-Path -LiteralPath $evidence)){$null=New-Item -ItemType Directory -Path $evidence}
    Write-CurrentRecord (Join-Path $evidence 'ctest-failure.json') @{
        schema='mistwatch.current-native-ctest.1';status='failed';mode=$Mode;run_id=$RunId;first_divergence=$_.Exception.Message;
        initial_only=[bool]$InitialOnly;source_files=$sourceRows;runtime_files=$runtimeRows;certificate_files=$certificateRows;executable=$binaryRow;
        preserve_first_results=$true;no_kill_or_restart=$true
    }
    throw
} finally {
    foreach($name in $savedEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process')}
}
