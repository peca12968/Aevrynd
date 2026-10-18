#requires -Version 7.0
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$BuildRoot,
    [Parameter(Mandatory=$true)][string]$ExecutablePath,
    [Parameter(Mandatory=$true)][ValidateSet('normal','gbv')][string]$Mode,
    [ValidateSet('legacy-cylinder','scanned-straight-v1')][string]$FallenLogProfile='scanned-straight-v1'
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$source=[IO.Path]::GetFullPath($SourceRoot)
$build=[IO.Path]::GetFullPath($BuildRoot)
$fallenContract=Join-Path $PSScriptRoot 'FallenLogContract.ps1'
. $fallenContract
$null=Assert-MistwatchFallenBuildRoots $source $build
$exe=(Resolve-Path -LiteralPath $ExecutablePath).Path
if([IO.Path]::GetFileName($exe) -cne 'Aevrynd-Mistwatch.exe' -or
    -not $exe.StartsWith($build+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw 'Mistwatch target must be inside this fresh build'
}
$evidence=Join-Path $build ('test-artifacts/mistwatch-native/'+$Mode)
if(Test-Path -LiteralPath $evidence){throw 'Preserve first matrix evidence; no overwrite/restart'}
$workspace=Join-Path $build ('test-workspaces/mistwatch-native/'+$Mode)
if(Test-Path -LiteralPath $workspace){throw 'Preserve first isolated user data; no overwrite/restart'}
$validator=Join-Path $PSScriptRoot 'validate_native_capture.ps1'
$contract=Join-Path $PSScriptRoot 'forest_cover_contract.ps1'
$worksiteContract=Join-Path $PSScriptRoot 'WorksiteContract.ps1'
$worksiteHeader=Join-Path $source 'games/mistwatch/native/include/MistwatchBridgeWorksite.h'
$groups=@('mistwatch-boulders','mistwatch-broadleaf','mistwatch-canopy-preview','mistwatch-ferns',
    'mistwatch-forest','mistwatch-grass','mistwatch-materials','mistwatch-wildlife')
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
function Get-Row([string]$Path,[string]$Label){
    $item=Get-Item -LiteralPath $Path -Force
    if($item.PSIsContainer -or ($item.Attributes-band [IO.FileAttributes]::ReparsePoint)){throw ('Invalid file: '+$Label)}
    @{path=$Label;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
}
function Assert-Rows([string]$Root,$Rows){
    foreach($row in $Rows){
        $actual=Get-Row (Join-Path $Root $row.path) $row.path
        if($actual.bytes -ne $row.bytes -or $actual.sha256 -cne $row.sha256){throw ('Bytes drift: '+$row.path)}
    }
}
function Get-RuntimeRows {
    $root=Join-Path (Split-Path $exe) 'assets'
    $rows=@(foreach($group in $groups){
        $dir=Join-Path $root $group
        if(-not (Test-Path -LiteralPath $dir -PathType Container)){throw ('Missing Mistwatch asset group: '+$group)}
        $items=@(Get-Item -LiteralPath $dir -Force)+@(Get-ChildItem -LiteralPath $dir -Recurse -Force)
        foreach($item in $items){
            if($item.Attributes-band [IO.FileAttributes]::ReparsePoint){throw ('Asset reparse: '+$item.FullName)}
            if(-not $item.PSIsContainer){Get-Row $item.FullName ([IO.Path]::GetRelativePath($root,$item.FullName).Replace('\','/'))}
        }
    })
    if($rows.Count -ne 133){throw 'Mistwatch target must stage exact133 files in its eight groups'}
    # 原130檔之外只新增這三張原始木板材質；不接受任意三檔補足數量。
    foreach($expectedMap in @(
        @('base-color.jpg',943056,'698C09B337375A9AAE12A76AD09FF2FDA11D735E6C1BBB18D72875BD61988DB6'),
        @('roughness.jpg',807719,'C79F640DC2D7EF81D69DE463EF6676B7C540D598FB0E462A6EB39EB5462770A9'),
        @('normal-dx.png',4886175,'743411ACF7332D8176D4E402237BB4B2D28A89FDC9BA160BB1EEE2DE6FBFB74B'))){
        $actualMap=Get-Row (Join-Path $root ('mistwatch-materials/poly-haven/worn_planks/1k/'+$expectedMap[0])) $expectedMap[0]
        if($actualMap.bytes -ne $expectedMap[1] -or $actualMap.sha256 -cne $expectedMap[2]){throw 'Weathered wood original runtime map differs'}
    }
    return @($rows | Sort-Object path)
}
function Write-Record([string]$Path,$Value){
    $Value|ConvertTo-Json -Depth 18|Set-Content -LiteralPath $Path -Encoding utf8
}
$savedEnvironment=@{}
$sourceRows=@();$runtimeRows=@();$fullRuntimeRows=@();$fallenRuntimeRows=@();$binaryRow=$null;$producerRows=@()
try {
    # 原藝術與工位門保留，新增真工作桌提交及唯一資源釋放證據。
    if((Get-FileHash -LiteralPath $validator).Hash -cne '3AFFD472F73E4B33DB9529AF884FD17A44B6D858B737D66FF2EDBCB7E1842A05' -or
       (Get-FileHash -LiteralPath $contract).Hash -cne '31468D7C13F4806B00B93AB07E84BCDA1D0E48B1FA2DA5874816ED9C8241D644'){
        throw 'Reviewed desk/art validator or original forestwood gates changed; review a new cohort instead'
    }
    if((Get-FileHash -LiteralPath $fallenContract).Hash -cne '474B890A71EAF2F0B0247A9A95E489B33088C2194CD1EAAA1D878F20E7B4415D'){throw 'Reviewed formal fallen contract changed; review new cohort'}
    # 聯合 cohort 額外固定原作者 header；缺少／LF 改寫／hash 不同皆拒絕。
    if((Get-FileHash -LiteralPath $worksiteContract).Hash -cne '1EF2DCACB9AB7F8D1BAFAE7DA3B8FD4638937FBAD1753F960A48BD27B682FDD8'){throw 'Joint worksite contract changed; review a new cohort'}
    $worksiteHeaderRow=Get-Row $worksiteHeader 'games/mistwatch/native/include/MistwatchBridgeWorksite.h'
    if($worksiteHeaderRow.bytes -ne 4761 -or $worksiteHeaderRow.sha256 -cne '99482F5351F068A4224944FD027EEBAB9328A20ECF12084C421E8F708E11CD43'){throw 'Joint worksite author header raw source mismatch'}
    . $worksiteContract
    $sourceNames=@('CMakePresets.json','games/mistwatch/native/CMakeLists.txt',
        'games/mistwatch/native/src/main.cpp','games/mistwatch/tools/run_native_ctest.ps1',
        'games/mistwatch/tools/validate_native_capture.ps1','games/mistwatch/tools/forest_cover_contract.ps1',
        'games/mistwatch/tools/author_root_stem.py','games/mistwatch/tools/WorksiteContract.ps1',
        'games/mistwatch/tools/FallenLogContract.ps1',
        'games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp',
        'games/mistwatch/native/tests/fallen/MistwatchFallenSeatSupport.h',
        'games/mistwatch/native/tests/fallen/MistwatchFallenSourceSupport.h',
        'games/mistwatch/native/include/MistwatchBridgeWorksite.h',
        'games/mistwatch/native/tests/art/RangerDeskCpuTest.cpp',
        'engine/assets/material-packs/material-packs.json','engine/assets/material-packs/LICENSE.md',
        'engine/assets/material-packs/poly-haven/worn_planks/1k/base-color.jpg',
        'engine/assets/material-packs/poly-haven/worn_planks/1k/roughness.jpg',
        'engine/assets/material-packs/poly-haven/worn_planks/1k/normal-dx.png')
    $sourceNames+=@(Get-ChildItem -LiteralPath (Join-Path $source 'games/mistwatch/native/include') -File|
        ForEach-Object {[IO.Path]::GetRelativePath($source,$_.FullName).Replace('\','/')})
    $sourceNames+=@(Get-MistwatchFallenPackagePins|ForEach-Object{'engine/assets/mistwatch-fallen-logs/'+$_[0]})
    $sourceRows=@(foreach($name in $sourceNames | Sort-Object -Unique){Get-Row (Join-Path $source $name) $name})
    $runtimeRows=@(Get-RuntimeRows)
    $fullRuntimeRows=@(Get-MistwatchFallenRuntimeRows -AssetsRoot (Join-Path (Split-Path $exe) 'assets') -SourceRoot $source)
    $fallenRuntimeRows=@($fullRuntimeRows|Where-Object{$_.path.StartsWith('mistwatch-fallen-logs/',[StringComparison]::Ordinal)})
    $binaryRow=Get-Row $exe ([IO.Path]::GetRelativePath($build,$exe).Replace('\','/'))
    # 比對真正本輪 CMake 派生物；不依賴主機 V150 或舊 asset receipts。
    $generated=Join-Path $build 'games/mistwatch/native/generated-root-stem'
    foreach($pair in @(
        @('island-tree-01-lod2-trunk-root-stem.aemesh','meshes/island-tree-01-lod2-trunk-root-stem.aemesh'),
        @('author-receipt.json','root-stem-derivation.json'))){
        $from=Join-Path $generated $pair[0]
        $to=Join-Path (Split-Path $exe) ('assets/mistwatch-canopy-preview/'+$pair[1])
        if((Get-FileHash -LiteralPath $from).Hash -cne (Get-FileHash -LiteralPath $to).Hash){throw 'Fresh derived root copy mismatch'}
        $producerRows+=Get-Row $from ([IO.Path]::GetRelativePath($build,$from).Replace('\','/'))
    }
    $boarProducer=Join-Path (Split-Path $exe) 'MistwatchBoarAsset.exe'
    $producerRows+=Get-Row $boarProducer ([IO.Path]::GetRelativePath($build,$boarProducer).Replace('\','/'))
    $null=New-Item -ItemType Directory -Path $workspace
    foreach($name in @('APPDATA','LOCALAPPDATA')+@(Get-ChildItem Env:|Where-Object Name -Like 'AEVRYND_*'|ForEach-Object Name)){
        if(-not $savedEnvironment.ContainsKey($name)){$savedEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
        [Environment]::SetEnvironmentVariable($name,$null,'Process')
    }
    foreach($name in @('APPDATA','LOCALAPPDATA')){
        $user=Join-Path $workspace $name;$null=New-Item -ItemType Directory -Path $user
        [Environment]::SetEnvironmentVariable($name,$user,'Process')
    }
    $parameters=@{ExecutablePath=$exe;EvidenceDirectory=$evidence;WorldProfile='forest-wood';FallenLogProfile=$FallenLogProfile;SourceRoot=$source;BuildRoot=$build}
    if($Mode -ceq 'gbv'){$parameters.GpuValidation=$true}
    # 原入口完整跑32/17病例；自己遊戲視窗Hidden，沒有SetForegroundWindow或全域input。
    & $validator @parameters
    $report=Get-Content -LiteralPath (Join-Path $evidence 'native-validation.json') -Raw|ConvertFrom-Json
    if($report.schema -ne 1 -or $report.world_profile -cne 'forest-wood' -or $report.fallen_log_profile -cne $FallenLogProfile -or
       $report.executable_sha256 -cne $binaryRow.sha256 -or
       (@($report.cases.case)-join "`n") -cne ($expected-join "`n")){
        throw 'Native report does not contain exact ordered production32/17 matrix'
    }
    foreach($case in $report.cases){if($case.exit_code -ne 0){throw 'Nonzero native result'}}
    if($FallenLogProfile -ceq 'scanned-straight-v1' -and @($report.fallen_cases).Count -ne $expected.Count){throw 'Missing actual scanned resource/cleanup case'}
    # 原完整 validator 成功後逐 scene 讀真 PNG receipt／cleanup，再加嚴工位 gate。
    $worksiteResults=@()
    foreach($case in $report.cases){
        $capture=Join-Path $evidence ($case.case+'.png')
        if([IO.Path]::GetFullPath($case.capture) -cne [IO.Path]::GetFullPath($capture)){throw 'Joint worksite capture escaped fixed scene output'}
        $pngRow=Get-Row $capture ($case.case+'.png')
        if($pngRow.sha256 -cne $case.capture_sha256){throw 'Joint worksite actual PNG/report raw hash mismatch'}
        $worksiteResult=Assert-MistwatchJointWorksiteEvidence $capture $case.case ($Mode -ceq 'gbv')
        if(@($worksiteResult).Count -ne 1 -or $worksiteResult -isnot [Collections.IDictionary]){throw 'Joint contract must return one dictionary'}
        $worksiteResults+=@{case=$case.case;contract=$worksiteResult;png=$pngRow;
            receipt=(Get-Row ($capture+'.receipt.txt') ($case.case+'.png.receipt.txt'));
            cleanup=(Get-Row ($capture+'.bridge-worksite-cleanup.txt') ($case.case+'.png.bridge-worksite-cleanup.txt'))}
    }
    Assert-Rows $source $sourceRows
    Assert-Rows $build (@($binaryRow)+$producerRows)
    $runtimeAfter=@(Get-RuntimeRows)
    $runtimeKeysBefore=@($runtimeRows|ForEach-Object {$_.path+'|'+$_.bytes+'|'+$_.sha256})
    $runtimeKeysAfter=@($runtimeAfter|ForEach-Object {$_.path+'|'+$_.bytes+'|'+$_.sha256})
    if(($runtimeKeysAfter-join "`n") -cne ($runtimeKeysBefore-join "`n")){throw 'Runtime membership/bytes changed during matrix'}
    $fullAfter=@(Get-MistwatchFallenRuntimeRows -AssetsRoot (Join-Path (Split-Path $exe) 'assets') -SourceRoot $source)
    $fullKeysBefore=@($fullRuntimeRows|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    $fullKeysAfter=@($fullAfter|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    if(($fullKeysBefore-join "`n") -cne ($fullKeysAfter-join "`n")){throw 'Full runtime387 membership/bytes changed during matrix'}
    Write-Record (Join-Path $evidence 'ctest-receipt.json') @{
        schema='mistwatch-native-ctest-1';art_cohort='mistwatch-art-v297';hut_cohort='mistwatch-hut-whole-v557';status='passed';mode=$Mode;world_profile='forest-wood';fallen_log_profile=$FallenLogProfile;
        cases=$expected.Count;case_names=$expected;per_case_seconds=$(if($Mode -ceq 'gbv'){150}else{45});
        original_validator_sha256=(Get-FileHash -LiteralPath $validator).Hash;
        original_contract_sha256=(Get-FileHash -LiteralPath $contract).Hash;
        worksite_contract_sha256=(Get-FileHash -LiteralPath $worksiteContract).Hash;
        worksite_author_header=$worksiteHeaderRow;worksite_cases=$worksiteResults;
        fallen_contract_sha256=(Get-FileHash -LiteralPath $fallenContract).Hash;
        fallen_cases=$report.fallen_cases;independent_expected_sha256=$report.independent_expected_sha256;
        executable=$binaryRow;producer_files=$producerRows;source_files=$sourceRows;runtime_files=$runtimeRows;
        original_eight_groups_members=$runtimeRows.Count;fallen_runtime_files=$fallenRuntimeRows;fallen_members=$fallenRuntimeRows.Count;
        full_runtime_files=$fullRuntimeRows;full_runtime_members=$fullRuntimeRows.Count;
        source_binding_scope='Limited game/tools raw snapshot; job base/artifact must separately bind complete fresh source/toolchain';
        runtime_binding_scope='Original eightgroup133 retained separately; exact new fallen6 raw pins; full formal387 before/after raw identity; root fresh-copy exact; not all387 semantic or art acceptance';
        native_validation_sha256=(Get-FileHash -LiteralPath (Join-Path $evidence 'native-validation.json')).Hash;
        own_window_hidden=1;foreground_activation=0;global_input=0;manual_play_accepted=$false;realistic_art_accepted=$false
    }
    Write-Output ('MISTWATCH_CTEST_MATRIX_PASS; MODE='+$Mode+'; CASES='+$expected.Count)
} catch {
    if(-not(Test-Path -LiteralPath $evidence)){$null=New-Item -ItemType Directory -Path $evidence -Force}
    Write-Record (Join-Path $evidence 'ctest-failure.json') @{
        schema='mistwatch-native-ctest-1';status='failed';mode=$Mode;first_divergence=$_.Exception.Message;
        source_files=$sourceRows;runtime_files=$runtimeRows;full_runtime_files=$fullRuntimeRows;fallen_runtime_files=$fallenRuntimeRows;executable=$binaryRow;
        preserve_first_results=$true;no_kill_or_restart=$true
    }
    throw
} finally {
    foreach($name in $savedEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process')}
}
