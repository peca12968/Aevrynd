Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ShadowTestSha256([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-ShadowStringSha256([string]$Value) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
}
function Write-ShadowTestJson([string]$Path,[object]$Object) {
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($Path)) -Force | Out-Null
    [IO.File]::WriteAllText($Path,(($Object | ConvertTo-Json -Depth 32).Replace("`r`n","`n")+"`n"),[Text.UTF8Encoding]::new($false))
}
function New-ShadowTestFile([string]$Path,[string]$Content) {
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($Path)) -Force | Out-Null
    [IO.File]::WriteAllText($Path,$Content,[Text.UTF8Encoding]::new($false))
}
function Get-ShadowStructuralDigest([string]$Root) {
    if(-not (Test-Path -LiteralPath $Root)){ return '<missing>' }
    $full=[IO.Path]::GetFullPath($Root).TrimEnd('\');$rows=@()
    foreach($item in Get-ChildItem -LiteralPath $full -Force -Recurse | Sort-Object FullName){
        $relative=$item.FullName.Substring($full.Length+1).Replace('\','/')
        if($item.PSIsContainer){$rows+="D|$relative"}else{$rows+="F|$relative|$($item.Length)|$(Get-ShadowTestSha256 $item.FullName)"}
    }
    Get-ShadowStringSha256 (($rows -join "`n")+"`n")
}

function New-ShadowRuntime([string]$RunId,[int]$Cascades,[int]$Mask,[int64]$Bytes,[int[]]$Gpu) {
    $splits=if($Cascades-eq4){@(18.0,48.0,110.0,240.0)}else{@(30.0,120.0,120.0,120.0)}
    $rendered=if($Cascades-eq4){@(1,1,1,1)}else{@(1,1,0,0)}
    $reused=if($Cascades-eq4){@(0,1,1,1)}else{@(0,1,0,0)}
    [ordered]@{
        runId=$RunId;phase='shadow-v255';cascadeConfigured=$Cascades;cascadeCount=$Cascades;cascadeResolution=2048
        cascadeSplits=$splits;cascadeRendered=$rendered;cascadeReused=$reused;farCascadeMask=$Mask;farCascadeInstances=2
        manualMasks=1;contactRequested=1;contactExecuted=1;contactEvaluationPasses=1
        contactDisabledLuminance=0.8;contactEnabledLuminance=0.6;contactDarkenedLuminance=0.2
        shadowBytes=$Bytes;gpuFrameNanoseconds=1000000;effective=1
        gpuValidationRequested=$Gpu[0];gpuValidationAvailable=$Gpu[1];gpuValidationExecuted=$Gpu[2];gpuValidationPassed=$Gpu[4]
        debugLayerEnabled=$Gpu[3];gpuValidationErrors=$Gpu[5];gpuValidationWarnings=$Gpu[6];gpuValidationFirstErrorId=0
        gpuValidationSource='D3D12InfoQueue';gpuValidationEntry='EngineLabShadowCapture'
    }
}
function New-ShadowCaptureReceipt([string]$Candidate,[string]$Role,[string]$Mode,[object]$Runtime,[string]$EntryHash,[string]$RunnerHash,[string]$EngineLabSourceHash,[string]$GameManifestHash,[string]$PerformanceHash) {
    $captures=@()
    foreach($name in @('contact-shadow-disabled.png','contact-shadow-enabled.png','far-cascade-receiver.png','day-shadow.png','dusk-shadow.png','night-shadow.png')){
        $path=Join-Path $Candidate "captures/$Role/$name";New-ShadowTestFile $path "fake-png-$Role-$name"
        $captures+=,[ordered]@{file=$name;width=1280;height=720;sha256=Get-ShadowTestSha256 $path}
    }
    $gpu=[ordered]@{requested=[int]$Runtime.gpuValidationRequested;available=[int]$Runtime.gpuValidationAvailable;executed=[int]$Runtime.gpuValidationExecuted;debug=[int]$Runtime.debugLayerEnabled;passed=[int]$Runtime.gpuValidationPassed;errors=[int]$Runtime.gpuValidationErrors;warnings=[int]$Runtime.gpuValidationWarnings}
    $executable='Aevrynd-Engine-Lab.exe'
    [ordered]@{
        schemaVersion=1;runId=[string]$Runtime.runId;mode=$Mode;cascadeCount=[int]$Runtime.cascadeCount
        source=[ordered]@{root='.';workingTreeDigestBefore=('e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');workingTreeDigestAfter=('e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');trackedWipFiles=@();engineLabSource=[ordered]@{path='evidence/source/EngineLabGame.cpp';sha256Before=$EngineLabSourceHash;sha256After=$EngineLabSourceHash};gameManifest=[ordered]@{path='evidence/source/game.json';sha256Before=$GameManifestHash;sha256After=$GameManifestHash};runner=[ordered]@{path='evidence/EngineLabShadowCaptureSmoke.ps1';sha256Before=$RunnerHash;sha256After=$RunnerHash}}
        executable=[ordered]@{path=$executable;sha256Before=$EntryHash;sha256After=$EntryHash}
        command=[ordered]@{file=$executable;arguments=@('--capture-shadow-demo');workingDirectory='.';captureDirectory="captures/$Role";gpuValidation=$(if($Role-ceq'gbv'){'1'}else{''});configuredCascades=[int]$Runtime.cascadeCount}
        process=[ordered]@{exitCode=0};dimensions=[ordered]@{width=1280;height=720};captures=$captures
        semantics=[ordered]@{contact=[ordered]@{changed=10;darkened=8;brightened=2};farCascade=[ordered]@{mask=[int]$Runtime.farCascadeMask;instances=[int]$Runtime.farCascadeInstances}}
        runtime=$Runtime;performance=[ordered]@{receipt='evidence/four-cascade-performance-receipt.json';sha256=$PerformanceHash;fourCascadeRegression=0.01;maximumRegression=0.05;passed=$true};gpuValidation=$gpu
    }
}
function Sync-ShadowCandidateManifest([object]$Fixture) {
    $manifestPath=Join-Path $Fixture.Candidate 'candidate.manifest.json'
    $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $records=@(foreach($item in Get-ChildItem -LiteralPath $Fixture.Candidate -File -Recurse | Where-Object FullName -ne $manifestPath | Sort-Object FullName){
        [ordered]@{path=$item.FullName.Substring($Fixture.Candidate.Length+1).Replace('\','/');size=[int64]$item.Length;sha256=Get-ShadowTestSha256 $item.FullName}
    })
    $manifest.files=$records;Write-ShadowTestJson $manifestPath $manifest
    $Fixture.ExpectedManifestSha256=Get-ShadowTestSha256 $manifestPath;$Fixture.ExpectedFiles=$records.Count
}
function New-EngineLabShadowPublisherFixture([string]$ProjectRoot) {
    $root=Join-Path ([IO.Path]::GetPathRoot($ProjectRoot)) ('AevryndShadowPublisherTest-'+[Guid]::NewGuid().ToString('N'))
    $candidate=Join-Path $root 'engine-lab-v255-fixture';$demo=Join-Path $root 'Demo';New-Item -ItemType Directory -Path $candidate,$demo -Force|Out-Null
    New-ShadowTestFile (Join-Path $demo '.aevrynd-engine-lab-publisher-test-root') "schema=1`npurpose=engine-lab-publisher-test`n"
    New-ShadowTestFile (Join-Path $demo 'Aevrynd-Engine-Lab.exe') 'legacy-latest-must-remain-unchanged'
    New-ShadowTestFile (Join-Path $demo 'history/immutable.txt') 'immutable-history'
    $entry=Join-Path $candidate 'Aevrynd-Engine-Lab.exe';New-ShadowTestFile $entry 'MZ-shadow-fixture';$entryHash=Get-ShadowTestSha256 $entry
    $builderHash='3'*64;$baseBuilderHash='4'*64;$hostHash='6'*64;$commit='1'*40
    foreach($path in @('assets/engine_probe.obj','evidence/candidate-build-receipt.json','evidence/build-process-policy.json','evidence/ctest-full.command.json','evidence/ctest-full.out.log','evidence/pe-closure.json')){New-ShadowTestFile (Join-Path $candidate $path) "fixture-$path"}
    $engineLabSourcePath=Join-Path $candidate 'evidence/source/EngineLabGame.cpp';New-ShadowTestFile $engineLabSourcePath '// 測試用引擎實驗室來源快照'
    $gameManifestPath=Join-Path $candidate 'evidence/source/game.json';New-ShadowTestFile $gameManifestPath '{"name":"EngineLabFixture"}'
    $runnerPath=Join-Path $candidate 'evidence/EngineLabShadowCaptureSmoke.ps1';New-ShadowTestFile $runnerPath '# 測試用擷取腳本快照'
    $performancePath=Join-Path $candidate 'evidence/four-cascade-performance-receipt.json';Write-ShadowTestJson $performancePath ([ordered]@{schemaVersion=1;fourCascadeRegression=0.01;maximumRegression=0.05;passed=$true})
    $finalizerPath=Join-Path $candidate 'evidence/Finalize-EngineLabShadowV255Candidate.ps1';New-ShadowTestFile $finalizerPath '# 測試用候選封存腳本'
    $engineLabSourceHash=Get-ShadowTestSha256 $engineLabSourcePath;$gameManifestHash=Get-ShadowTestSha256 $gameManifestPath;$runnerHash=Get-ShadowTestSha256 $runnerPath;$performanceHash=Get-ShadowTestSha256 $performancePath;$finalizerHash=Get-ShadowTestSha256 $finalizerPath
    $roleSpecs=@(
        @('normal','normal',4,8,67108864,@(0,0,0,0,0,0,0),'11111111-1111-1111-1111-111111111111'),
        @('gbv','gbv',4,8,67108864,@(1,1,1,1,1,0,0),'22222222-2222-2222-2222-222222222222'),
        @('fallback','normal',2,2,33554432,@(0,0,0,0,0,0,0),'33333333-3333-3333-3333-333333333333')
    )
    $roleBindings=@()
    foreach($spec in $roleSpecs){
        $role=[string]$spec[0];$runtime=New-ShadowRuntime ([string]$spec[6]) ([int]$spec[2]) ([int]$spec[3]) ([int64]$spec[4]) ([int[]]$spec[5])
        $capture=New-ShadowCaptureReceipt $candidate $role ([string]$spec[1]) $runtime $entryHash $runnerHash $engineLabSourceHash $gameManifestHash $performanceHash
        Write-ShadowTestJson (Join-Path $candidate "captures/$role/shadow-demo-runtime-receipt.json") $runtime
        $capturePath=Join-Path $candidate "captures/$role/shadow-demo-capture-receipt.json";Write-ShadowTestJson $capturePath $capture
        $roleBindings+=,[ordered]@{role=$role;priorCaptureReceiptSha256=('a'*64);finalizedCaptureReceipt="captures/$role/shadow-demo-capture-receipt.json";finalizedCaptureReceiptSha256=Get-ShadowTestSha256 $capturePath}
    }
    $refinalization=[ordered]@{schemaVersion=1;operation='candidate-relative-shadow-provenance-v1';priorCandidateManifestSha256=('b'*64);sourceCommit=$commit;finalizer=[ordered]@{path='evidence/Finalize-EngineLabShadowV255Candidate.ps1';sha256=$finalizerHash};sourceArchive=@([ordered]@{path='evidence/source/EngineLabGame.cpp';sha256=$engineLabSourceHash},[ordered]@{path='evidence/source/game.json';sha256=$gameManifestHash},[ordered]@{path='evidence/EngineLabShadowCaptureSmoke.ps1';sha256=$runnerHash});entry=[ordered]@{path='Aevrynd-Engine-Lab.exe';sha256=$entryHash};performance=[ordered]@{path='evidence/four-cascade-performance-receipt.json';sha256=$performanceHash};roles=$roleBindings}
    Write-ShadowTestJson (Join-Path $candidate 'evidence/candidate-refinalization-receipt.json') $refinalization
    $manifest=[ordered]@{schemaVersion=1;releaseId='v2.55.0-engine-v2.86.0-c033de38';product='EngineLab';productVersion='2.55.0';engineVersion='2.86.0';commit=$commit;entry='Aevrynd-Engine-Lab.exe';featureStage='FourCascadeContactShadowValidated';maturity='validated-candidate';captureModes=@('normal-four-cascade','gbv-four-cascade','normal-two-cascade-fallback');fullTests=113;peClosure='evidence/pe-closure.json';source=[ordered]@{commit=$commit;origin=$commit;builder=[ordered]@{path='tools/build.ps1';sha256=$builderHash};baseBuilder=[ordered]@{path='tools/base.ps1';sha256=$baseBuilderHash};captureRunner=[ordered]@{path='tools/capture.ps1';sha256=$runnerHash};hostBinding=[ordered]@{path='evidence/host.json';sha256=$hostHash}};files=@()}
    Write-ShadowTestJson (Join-Path $candidate 'candidate.manifest.json') $manifest
    $fixture=[pscustomobject]@{Root=$root;Candidate=$candidate;Demo=$demo;ReleaseId=$manifest.releaseId;ExpectedManifestSha256='';ExpectedFiles=0;ExpectedSourceCommit=$commit;ExpectedEntrySha256=$entryHash;ExpectedBuilderScriptSha256=$builderHash;ExpectedCaptureRunnerSha256=$runnerHash;ExpectedEngineLabSourceSha256=$engineLabSourceHash;ExpectedGameManifestSha256=$gameManifestHash;ExpectedPerformanceSha256=$performanceHash}
    Sync-ShadowCandidateManifest $fixture
    $fixture
}
function Remove-EngineLabShadowPublisherFixture([object]$Fixture) {
    $full=[IO.Path]::GetFullPath([string]$Fixture.Root).TrimEnd('\');$leaf=[IO.Path]::GetFileName($full)
    if($leaf-cnotmatch'^AevryndShadowPublisherTest-[0-9a-f]{32}$'-or[IO.Path]::GetPathRoot($full)-ceq$full){throw 'Shadow fixture cleanup root rejected.'}
    if(Test-Path -LiteralPath $full){Remove-Item -LiteralPath $full -Recurse -Force}
}
function Get-ShadowPointerSnapshot([object]$Fixture) { $p=Join-Path $Fixture.Demo 'current/engine-lab.current.json';if(Test-Path $p){Get-ShadowTestSha256 $p}else{'<missing>'} }
function Get-ShadowGenerationSnapshot([object]$Fixture) {
    $root=Join-Path $Fixture.Demo 'current/engine-lab.generations'
    if(-not(Test-Path $root)-or-not@(Get-ChildItem $root -File -Force -ErrorAction SilentlyContinue).Count){return '<missing>'}
    Get-ShadowStructuralDigest $root
}
function Assert-ShadowNoPartial([object]$Fixture) {
    $staging=Join-Path $Fixture.Demo 'releases/engine-lab/.staging';if((Test-Path $staging)-and@(Get-ChildItem $staging -Force).Count){throw 'Shadow test found staging residue.'}
    if(@(Get-ChildItem (Join-Path $Fixture.Demo 'current') -Filter '*.pending' -File -Recurse -ErrorAction SilentlyContinue).Count){throw 'Shadow test found pending residue.'}
}
function Assert-ShadowLegacyHistory([object]$Fixture) {
    if((Get-ShadowTestSha256 (Join-Path $Fixture.Demo 'Aevrynd-Engine-Lab.exe'))-cne(Get-ShadowStringSha256 'legacy-latest-must-remain-unchanged')){throw 'Legacy executable changed.'}
    if((Get-ShadowTestSha256 (Join-Path $Fixture.Demo 'history/immutable.txt'))-cne(Get-ShadowStringSha256 'immutable-history')){throw 'Immutable history changed.'}
}
function Add-ShadowSeedGeneration([object]$Fixture,[string]$Version,[string]$Suffix) {
    $releaseId="v$Version-engine-v2.86.0-$Suffix";$release=Join-Path $Fixture.Demo "releases/engine-lab/$releaseId";New-Item -ItemType Directory -Path $release -Force|Out-Null
    $entry=Join-Path $release 'Aevrynd-Engine-Lab.exe';New-ShadowTestFile $entry "MZ-seed-$Version-$Suffix";$record=[ordered]@{path='Aevrynd-Engine-Lab.exe';size=[int64](Get-Item $entry).Length;sha256=Get-ShadowTestSha256 $entry}
    $manifest=[ordered]@{schemaVersion=1;product='EngineLab';productVersion=$Version;engineVersion='2.86.0';releaseId=$releaseId;entry='Aevrynd-Engine-Lab.exe';treeAlgorithm='candidate-files-plus-self-excluded-release-manifest-v1';releaseFileCount=2;pointerContract='immutable-resolver-append-only-semver-generations-v1';legacyLatestExecutableMode='preserved-unmanaged';source=[ordered]@{};builder=[ordered]@{};verifier=[ordered]@{};validation=[ordered]@{};peClosure=[ordered]@{};files=@($record)}
    $manifestPath=Join-Path $release 'release.manifest.json';Write-ShadowTestJson $manifestPath $manifest;$hash=Get-ShadowTestSha256 $manifestPath
    $generation=[ordered]@{schemaVersion=1;role='release-generation';product='EngineLab';productVersion=$Version;releaseId=$releaseId;releasePath="releases/engine-lab/$releaseId";entry='Aevrynd-Engine-Lab.exe';manifestSha256=$hash;legacyLatestExecutableMode='preserved-unmanaged'}
    $generationRoot=Join-Path $Fixture.Demo 'current/engine-lab.generations';Write-ShadowTestJson (Join-Path $generationRoot "v$Version-$hash.json") $generation
    [pscustomobject]@{ReleaseId=$releaseId;ManifestSha256=$hash}
}
