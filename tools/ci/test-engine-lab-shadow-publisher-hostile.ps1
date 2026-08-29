param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'engine-lab-shadow-publisher-test-fixture.ps1')
$publisher=Join-Path $PSScriptRoot 'Publish-EngineLabShadowRelease.ps1'
function Assert-ShadowHostileTrue([bool]$Value,[string]$Message){if(-not$Value){throw $Message}}
function Invoke-ShadowHostilePublisher([object]$Fixture){
    &$publisher -ProjectRoot $ProjectRoot -CandidateRoot $Fixture.Candidate -DemoRoot $Fixture.Demo -TestMode -TestExpectedCandidateManifestSha256 $Fixture.ExpectedManifestSha256 -TestExpectedCandidateFiles $Fixture.ExpectedFiles -TestExpectedSourceCommit $Fixture.ExpectedSourceCommit -TestExpectedEntrySha256 $Fixture.ExpectedEntrySha256 -TestExpectedBuilderScriptSha256 $Fixture.ExpectedBuilderScriptSha256 -TestExpectedCaptureRunnerSha256 $Fixture.ExpectedCaptureRunnerSha256 -TestExpectedEngineLabSourceSha256 $Fixture.ExpectedEngineLabSourceSha256 -TestExpectedGameManifestSha256 $Fixture.ExpectedGameManifestSha256 -TestExpectedPerformanceSha256 $Fixture.ExpectedPerformanceSha256
}
function Update-ShadowManifestPin([object]$Fixture){$Fixture.ExpectedManifestSha256=Get-ShadowTestSha256 (Join-Path $Fixture.Candidate 'candidate.manifest.json')}
function Get-ShadowReceipt([object]$Fixture,[string]$Role,[string]$Kind){$p=Join-Path $Fixture.Candidate "captures/$Role/shadow-demo-$Kind-receipt.json";[pscustomobject]@{Path=$p;Json=(Get-Content $p -Raw|ConvertFrom-Json)}}
function Assert-ShadowRejected([string]$Name,[scriptblock]$Mutate){
    $fixture=New-EngineLabShadowPublisherFixture $ProjectRoot
    try{
        &$Mutate $fixture;$pointer=Get-ShadowPointerSnapshot $fixture;$generations=Get-ShadowGenerationSnapshot $fixture;$failed=$false
        try{Invoke-ShadowHostilePublisher $fixture|Out-Null}catch{$failed=$true}
        Assert-ShadowHostileTrue $failed "Hostile shadow case accepted: $Name"
        Assert-ShadowHostileTrue ((Get-ShadowPointerSnapshot $fixture)-ceq$pointer-and(Get-ShadowGenerationSnapshot $fixture)-ceq$generations) "Hostile shadow case changed pointer history: $Name"
        if($Name-ceq'pending-residue'){
            $pending=Join-Path $fixture.Demo 'current/engine-lab.hostile.pending';Assert-ShadowHostileTrue ((Get-ShadowTestSha256 $pending)-ceq$fixture.PendingHash) 'Pending residue was mutated.'
        }else{Assert-ShadowNoPartial $fixture}
        Assert-ShadowLegacyHistory $fixture;$script:passed++
    }finally{Remove-EngineLabShadowPublisherFixture $fixture}
}
$script:passed=0

Assert-ShadowRejected 'candidate-extra' {param($f)New-ShadowTestFile (Join-Path $f.Candidate 'extra.bin') 'extra'}
Assert-ShadowRejected 'candidate-missing' {param($f)Remove-Item (Join-Path $f.Candidate 'assets/engine_probe.obj')}
Assert-ShadowRejected 'candidate-content-tamper' {param($f)[IO.File]::AppendAllText((Join-Path $f.Candidate 'assets/engine_probe.obj'),'tamper')}
Assert-ShadowRejected 'candidate-manifest-duplicate-key' {param($f)$p=Join-Path $f.Candidate 'candidate.manifest.json';$raw=[IO.File]::ReadAllText($p);[IO.File]::WriteAllText($p,$raw.Insert($raw.IndexOf('{')+1,'"schemaVersion":1,'));Update-ShadowManifestPin $f}
Assert-ShadowRejected 'candidate-manifest-path-traversal' {param($f)$p=Join-Path $f.Candidate 'candidate.manifest.json';$j=Get-Content $p -Raw|ConvertFrom-Json;$j.files[0].path='../escape';Write-ShadowTestJson $p $j;Update-ShadowManifestPin $f}
Assert-ShadowRejected 'candidate-manifest-duplicate-path-case' {param($f)$p=Join-Path $f.Candidate 'candidate.manifest.json';$j=Get-Content $p -Raw|ConvertFrom-Json;$copy=$j.files[0].PSObject.Copy();$copy.path=([string]$copy.path).ToUpperInvariant();$j.files+=@($copy);Write-ShadowTestJson $p $j;$f.ExpectedFiles++;Update-ShadowManifestPin $f}
Assert-ShadowRejected 'candidate-manifest-wrong-size-type' {param($f)$p=Join-Path $f.Candidate 'candidate.manifest.json';$j=Get-Content $p -Raw|ConvertFrom-Json;$j.files[0].size='16';Write-ShadowTestJson $p $j;Update-ShadowManifestPin $f}
Assert-ShadowRejected 'approved-manifest-pin' {param($f)$f.ExpectedManifestSha256='0'*64}
Assert-ShadowRejected 'approved-file-count-pin' {param($f)$f.ExpectedFiles++}
Assert-ShadowRejected 'approved-source-pin' {param($f)$f.ExpectedSourceCommit='2'*40}
Assert-ShadowRejected 'approved-entry-pin' {param($f)$f.ExpectedEntrySha256='7'*64}
Assert-ShadowRejected 'approved-builder-pin' {param($f)$f.ExpectedBuilderScriptSha256='7'*64}
Assert-ShadowRejected 'approved-runner-pin' {param($f)$f.ExpectedCaptureRunnerSha256='7'*64}

# r2 憑證只能指向候選內的封存來源與相對執行位置；三種角色都要各自驗證，避免單一角色成為繞過點。
$sourceMembers=[ordered]@{normal='engineLabSource';gbv='gameManifest';fallback='runner'}
foreach($role in @('normal','gbv','fallback')){
    $member=[string]$sourceMembers[$role]
    Assert-ShadowRejected "r2-$role-source-path" {param($f)$capture=Get-ShadowReceipt $f $role 'capture';$capture.Json.source.$member.path='../fake-source';Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
    Assert-ShadowRejected "r2-$role-source-hash" {param($f)$capture=Get-ShadowReceipt $f $role 'capture';$capture.Json.source.$member.sha256Before='7'*64;$capture.Json.source.$member.sha256After='7'*64;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
    Assert-ShadowRejected "r2-$role-entry-path" {param($f)$capture=Get-ShadowReceipt $f $role 'capture';$capture.Json.executable.path='evidence/fake.exe';$capture.Json.command.file='evidence/fake.exe';Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
    Assert-ShadowRejected "r2-$role-entry-hash" {param($f)$capture=Get-ShadowReceipt $f $role 'capture';$capture.Json.executable.sha256Before='7'*64;$capture.Json.executable.sha256After='7'*64;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
    Assert-ShadowRejected "r2-$role-performance-path" {param($f)$capture=Get-ShadowReceipt $f $role 'capture';$capture.Json.performance.receipt='evidence/fake-performance.json';Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
    Assert-ShadowRejected "r2-$role-performance-hash" {param($f)$capture=Get-ShadowReceipt $f $role 'capture';$capture.Json.performance.sha256='7'*64;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
    Assert-ShadowRejected "r2-$role-working-directory-traversal" {param($f)$capture=Get-ShadowReceipt $f $role 'capture';$capture.Json.command.workingDirectory='../escape';Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
    Assert-ShadowRejected "r2-$role-capture-directory-noncanonical" {param($f)$capture=Get-ShadowReceipt $f $role 'capture';$capture.Json.command.captureDirectory="captures/$role/../$role";Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
}

# 交換角色憑證，即使重新簽 candidate manifest 也必須因角色語意拒絕。
Assert-ShadowRejected 'receipt-role-swap' {param($f)
    foreach($kind in @('runtime','capture')){$a=Join-Path $f.Candidate "captures/normal/shadow-demo-$kind-receipt.json";$b=Join-Path $f.Candidate "captures/fallback/shadow-demo-$kind-receipt.json";$tmp="$a.swap";Move-Item $a $tmp;Move-Item $b $a;Move-Item $tmp $b};Sync-ShadowCandidateManifest $f
}
Assert-ShadowRejected 'gbv-downgrade' {param($f)
    $runtime=Get-ShadowReceipt $f 'gbv' 'runtime';foreach($n in @('gpuValidationRequested','gpuValidationAvailable','gpuValidationExecuted','gpuValidationPassed','debugLayerEnabled')){$runtime.Json.$n=0};Write-ShadowTestJson $runtime.Path $runtime.Json
    $capture=Get-ShadowReceipt $f 'gbv' 'capture';$capture.Json.runtime=$runtime.Json;foreach($n in @('requested','available','executed','passed','debug')){$capture.Json.gpuValidation.$n=0};Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f
}
Assert-ShadowRejected 'duplicate-run-id' {param($f)
    $normal=(Get-ShadowReceipt $f 'normal' 'runtime').Json.runId;$runtime=Get-ShadowReceipt $f 'fallback' 'runtime';$runtime.Json.runId=$normal;Write-ShadowTestJson $runtime.Path $runtime.Json
    $capture=Get-ShadowReceipt $f 'fallback' 'capture';$capture.Json.runId=$normal;$capture.Json.runtime.runId=$normal;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f
}
Assert-ShadowRejected 'nested-runtime-mismatch' {param($f)$capture=Get-ShadowReceipt $f 'normal' 'capture';$capture.Json.runtime.gpuFrameNanoseconds=1000001;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
Assert-ShadowRejected 'png-role-substitution' {param($f)$capture=Get-ShadowReceipt $f 'normal' 'capture';$other=Join-Path $f.Candidate 'captures/gbv/day-shadow.png';$capture.Json.captures[3].sha256=Get-ShadowTestSha256 $other;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
Assert-ShadowRejected 'png-duplicate-name' {param($f)$capture=Get-ShadowReceipt $f 'normal' 'capture';$capture.Json.captures[1].file=$capture.Json.captures[0].file;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
Assert-ShadowRejected 'performance-over-five-percent' {param($f)$capture=Get-ShadowReceipt $f 'normal' 'capture';$capture.Json.performance.fourCascadeRegression=0.051;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
Assert-ShadowRejected 'performance-missing-field' {param($f)$capture=Get-ShadowReceipt $f 'normal' 'capture';$capture.Json.performance.PSObject.Properties.Remove('passed');Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}
Assert-ShadowRejected 'contact-not-executed' {param($f)$runtime=Get-ShadowReceipt $f 'normal' 'runtime';$runtime.Json.contactExecuted=0;Write-ShadowTestJson $runtime.Path $runtime.Json;$capture=Get-ShadowReceipt $f 'normal' 'capture';$capture.Json.runtime=$runtime.Json;Write-ShadowTestJson $capture.Path $capture.Json;Sync-ShadowCandidateManifest $f}

# 既有 resolver/generation 與同 semver 衝突必須在不可變 commit 前拒絕。
Assert-ShadowRejected 'pending-residue' {param($f)$p=Join-Path $f.Demo 'current/engine-lab.hostile.pending';New-ShadowTestFile $p 'unowned';$f|Add-Member -NotePropertyName PendingHash -NotePropertyValue (Get-ShadowTestSha256 $p)}
Assert-ShadowRejected 'same-semver-different-generation' {param($f)Add-ShadowSeedGeneration $f '2.55.0' 'deadbeef'|Out-Null}
Assert-ShadowRejected 'invalid-higher-generation' {param($f)Add-ShadowSeedGeneration $f '2.54.0' '25425425'|Out-Null;New-ShadowTestFile (Join-Path $f.Demo ('current/engine-lab.generations/v9.0.0-'+('9'*64)+'.json')) '{}'}
Assert-ShadowRejected 'generation-tamper-after-publish' {param($f)Invoke-ShadowHostilePublisher $f|Out-Null;$g=@(Get-ChildItem (Join-Path $f.Demo 'current/engine-lab.generations') -Filter 'v2.55.0-*.json')[0];[IO.File]::AppendAllText($g.FullName," `n")}
Assert-ShadowRejected 'resolver-tamper-after-publish' {param($f)Invoke-ShadowHostilePublisher $f|Out-Null;[IO.File]::AppendAllText((Join-Path $f.Demo 'current/engine-lab.current.json')," `n")}

# 兩個不同內容的同版本候選同時競爭時，只能有一方成功。
$fixture=New-EngineLabShadowPublisherFixture $ProjectRoot
try{
    $candidateTwo=Join-Path $fixture.Root 'engine-lab-v255-fixture-two';Copy-Item $fixture.Candidate $candidateTwo -Recurse
    $fixtureTwo=[pscustomobject]@{Root=$fixture.Root;Candidate=$candidateTwo;Demo=$fixture.Demo;ReleaseId=$fixture.ReleaseId;ExpectedManifestSha256=$fixture.ExpectedManifestSha256;ExpectedFiles=$fixture.ExpectedFiles;ExpectedSourceCommit=$fixture.ExpectedSourceCommit;ExpectedEntrySha256=$fixture.ExpectedEntrySha256;ExpectedBuilderScriptSha256=$fixture.ExpectedBuilderScriptSha256;ExpectedCaptureRunnerSha256=$fixture.ExpectedCaptureRunnerSha256;ExpectedEngineLabSourceSha256=$fixture.ExpectedEngineLabSourceSha256;ExpectedGameManifestSha256=$fixture.ExpectedGameManifestSha256;ExpectedPerformanceSha256=$fixture.ExpectedPerformanceSha256}
    [IO.File]::AppendAllText((Join-Path $candidateTwo 'assets/engine_probe.obj'),'different');Sync-ShadowCandidateManifest $fixtureTwo
    $shell=[IO.Path]::GetFullPath((Get-Process -Id $PID).Path)
    function Get-Args($f){@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$publisher,'-ProjectRoot',$ProjectRoot,'-CandidateRoot',$f.Candidate,'-DemoRoot',$f.Demo,'-TestMode','-TestExpectedCandidateManifestSha256',$f.ExpectedManifestSha256,'-TestExpectedCandidateFiles',[string]$f.ExpectedFiles,'-TestExpectedSourceCommit',$f.ExpectedSourceCommit,'-TestExpectedEntrySha256',$f.ExpectedEntrySha256,'-TestExpectedBuilderScriptSha256',$f.ExpectedBuilderScriptSha256,'-TestExpectedCaptureRunnerSha256',$f.ExpectedCaptureRunnerSha256,'-TestExpectedEngineLabSourceSha256',$f.ExpectedEngineLabSourceSha256,'-TestExpectedGameManifestSha256',$f.ExpectedGameManifestSha256,'-TestExpectedPerformanceSha256',$f.ExpectedPerformanceSha256)}
    $o1=Join-Path $fixture.Root 'race1.out';$e1=Join-Path $fixture.Root 'race1.err';$o2=Join-Path $fixture.Root 'race2.out';$e2=Join-Path $fixture.Root 'race2.err'
    $p1=Start-Process $shell -ArgumentList (Get-Args $fixture) -RedirectStandardOutput $o1 -RedirectStandardError $e1 -PassThru;$p2=Start-Process $shell -ArgumentList (Get-Args $fixtureTwo) -RedirectStandardOutput $o2 -RedirectStandardError $e2 -PassThru;$p1.WaitForExit();$p2.WaitForExit()
    Assert-ShadowHostileTrue ((@($p1.ExitCode,$p2.ExitCode|Where-Object{$_-eq0}).Count)-eq1) "Different same-semver race did not select exactly one winner: $($p1.ExitCode)/$($p2.ExitCode)"
    Assert-ShadowHostileTrue (@(Get-ChildItem (Join-Path $fixture.Demo 'current/engine-lab.generations') -Filter 'v2.55.0-*.json').Count-eq1) 'Different same-semver race created ambiguous generations.'
    Assert-ShadowNoPartial $fixture;$script:passed++
}finally{Remove-EngineLabShadowPublisherFixture $fixture}

[ordered]@{schemaVersion=1;passed=$true;cases=$script:passed;publisherSha256=Get-ShadowTestSha256 $publisher;completedUtc=[DateTimeOffset]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 4
