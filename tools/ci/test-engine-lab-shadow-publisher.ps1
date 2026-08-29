param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'engine-lab-shadow-publisher-test-fixture.ps1')
$publisher=Join-Path $PSScriptRoot 'Publish-EngineLabShadowRelease.ps1'
function Assert-ShadowTrue([bool]$Value,[string]$Message){if(-not$Value){throw $Message}}
function Invoke-ShadowPublisher([object]$Fixture,[switch]$DryRun,[string]$FailPoint=''){
    $arguments=@{ProjectRoot=$ProjectRoot;CandidateRoot=$Fixture.Candidate;DemoRoot=$Fixture.Demo;TestMode=$true;TestExpectedCandidateManifestSha256=$Fixture.ExpectedManifestSha256;TestExpectedCandidateFiles=$Fixture.ExpectedFiles;TestExpectedSourceCommit=$Fixture.ExpectedSourceCommit;TestExpectedEntrySha256=$Fixture.ExpectedEntrySha256;TestExpectedBuilderScriptSha256=$Fixture.ExpectedBuilderScriptSha256;TestExpectedCaptureRunnerSha256=$Fixture.ExpectedCaptureRunnerSha256;TestExpectedEngineLabSourceSha256=$Fixture.ExpectedEngineLabSourceSha256;TestExpectedGameManifestSha256=$Fixture.ExpectedGameManifestSha256;TestExpectedPerformanceSha256=$Fixture.ExpectedPerformanceSha256}
    if($DryRun){$arguments.DryRun=$true};if($FailPoint){$arguments.TestFailPoint=$FailPoint};&$publisher @arguments
}
function Get-ShadowPublisherArguments([object]$Fixture){
    @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$publisher,'-ProjectRoot',$ProjectRoot,'-CandidateRoot',$Fixture.Candidate,'-DemoRoot',$Fixture.Demo,'-TestMode','-TestExpectedCandidateManifestSha256',$Fixture.ExpectedManifestSha256,'-TestExpectedCandidateFiles',[string]$Fixture.ExpectedFiles,'-TestExpectedSourceCommit',$Fixture.ExpectedSourceCommit,'-TestExpectedEntrySha256',$Fixture.ExpectedEntrySha256,'-TestExpectedBuilderScriptSha256',$Fixture.ExpectedBuilderScriptSha256,'-TestExpectedCaptureRunnerSha256',$Fixture.ExpectedCaptureRunnerSha256,'-TestExpectedEngineLabSourceSha256',$Fixture.ExpectedEngineLabSourceSha256,'-TestExpectedGameManifestSha256',$Fixture.ExpectedGameManifestSha256,'-TestExpectedPerformanceSha256',$Fixture.ExpectedPerformanceSha256)
}
$passed=0

# Dry-run 執行完整候選驗證，但 Demo 必須逐位元不變。
$fixture=New-EngineLabShadowPublisherFixture $ProjectRoot
try{
    $before=Get-ShadowStructuralDigest $fixture.Demo;$result=Invoke-ShadowPublisher $fixture -DryRun|ConvertFrom-Json;$after=Get-ShadowStructuralDigest $fixture.Demo
    Assert-ShadowTrue ($result.dryRun-and$result.candidateFiles-eq$fixture.ExpectedFiles-and$result.roles.Count-eq3) 'Shadow dry-run result rejected.'
    Assert-ShadowTrue ($before-ceq$after) 'Shadow dry-run mutated Demo.';$passed++
}finally{Remove-EngineLabShadowPublisherFixture $fixture}

# 先放入合法 2.54 歷史，2.55 發布與重試都不得改動歷史位元。
$fixture=New-EngineLabShadowPublisherFixture $ProjectRoot
try{
    $seed=Add-ShadowSeedGeneration $fixture '2.54.0' '25425425';$historyPath=Join-Path $fixture.Demo 'current/engine-lab.generations'
    $oldReleasePath=Join-Path $fixture.Demo "releases/engine-lab/$($seed.ReleaseId)";$oldReleaseBefore=Get-ShadowStructuralDigest $oldReleasePath
    $result=Invoke-ShadowPublisher $fixture|ConvertFrom-Json;$release=Join-Path $fixture.Demo "releases/engine-lab/$($fixture.ReleaseId)"
    Assert-ShadowTrue ($result.published-and$result.files-eq$fixture.ExpectedFiles) 'Shadow publish result rejected.'
    Assert-ShadowTrue (Test-Path (Join-Path $release 'release.manifest.json')) 'Shadow immutable release missing.'
    $oldGeneration=@(Get-ChildItem $historyPath -Filter 'v2.54.0-*.json');Assert-ShadowTrue ($oldGeneration.Count-eq1) 'v2.54 history disappeared.'
    $oldHash=Get-ShadowTestSha256 $oldGeneration[0].FullName;$pointerBefore=Get-ShadowPointerSnapshot $fixture;$generationBefore=Get-ShadowGenerationSnapshot $fixture
    Invoke-ShadowPublisher $fixture|Out-Null
    Assert-ShadowTrue ((Get-ShadowTestSha256 $oldGeneration[0].FullName)-ceq$oldHash) 'v2.54 history bytes changed.'
    Assert-ShadowTrue ((Get-ShadowStructuralDigest $oldReleasePath)-ceq$oldReleaseBefore) 'v2.54 release tree changed.'
    Assert-ShadowTrue ((Get-ShadowPointerSnapshot $fixture)-ceq$pointerBefore-and(Get-ShadowGenerationSnapshot $fixture)-ceq$generationBefore) 'Idempotent shadow retry changed history.'
    Assert-ShadowLegacyHistory $fixture;Assert-ShadowNoPartial $fixture;$passed+=2
}finally{Remove-EngineLabShadowPublisherFixture $fixture}

# 同內容同版本並行發布必須收斂成一份 release 與 generation。
$fixture=New-EngineLabShadowPublisherFixture $ProjectRoot
try{
    $powerShell=[IO.Path]::GetFullPath((Get-Process -Id $PID).Path);$common=Get-ShadowPublisherArguments $fixture
    $out1=Join-Path $fixture.Root 'parallel-1.out';$err1=Join-Path $fixture.Root 'parallel-1.err';$out2=Join-Path $fixture.Root 'parallel-2.out';$err2=Join-Path $fixture.Root 'parallel-2.err'
    $one=Start-Process -FilePath $powerShell -ArgumentList $common -WorkingDirectory $ProjectRoot -RedirectStandardOutput $out1 -RedirectStandardError $err1 -PassThru
    $two=Start-Process -FilePath $powerShell -ArgumentList $common -WorkingDirectory $ProjectRoot -RedirectStandardOutput $out2 -RedirectStandardError $err2 -PassThru
    $one.WaitForExit();$two.WaitForExit();$errors=((Get-Content $err1 -Raw -ErrorAction SilentlyContinue)+(Get-Content $err2 -Raw -ErrorAction SilentlyContinue))
    Assert-ShadowTrue ($one.ExitCode-eq0-and$two.ExitCode-eq0) "Parallel shadow publish failed: $($one.ExitCode)/$($two.ExitCode) $errors"
    Assert-ShadowTrue (@(Get-ChildItem (Join-Path $fixture.Demo 'current/engine-lab.generations') -Filter 'v2.55.0-*.json').Count-eq1) 'Parallel shadow publish duplicated generation.'
    Assert-ShadowNoPartial $fixture;$passed++
}finally{Remove-EngineLabShadowPublisherFixture $fixture}

# 主要不可逆邊界皆需 fail-closed，且不得改 resolver/generation。
foreach($point in @('Swap','Copy','Hash','Verify','BeforeRename','Rename','BeforePointer','Pointer')){
    $fixture=New-EngineLabShadowPublisherFixture $ProjectRoot
    try{
        $pointer=Get-ShadowPointerSnapshot $fixture;$generations=Get-ShadowGenerationSnapshot $fixture;$failed=$false
        try{Invoke-ShadowPublisher $fixture -FailPoint $point|Out-Null}catch{$failed=$true}
        Assert-ShadowTrue $failed "Shadow failpoint accepted: $point"
        Assert-ShadowTrue ((Get-ShadowPointerSnapshot $fixture)-ceq$pointer-and(Get-ShadowGenerationSnapshot $fixture)-ceq$generations) "Shadow failpoint changed pointer history: $point"
        Assert-ShadowNoPartial $fixture;$passed++
    }finally{Remove-EngineLabShadowPublisherFixture $fixture}
}

[ordered]@{schemaVersion=1;passed=$true;cases=$passed;publisherSha256=Get-ShadowTestSha256 $publisher;completedUtc=[DateTimeOffset]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 4
