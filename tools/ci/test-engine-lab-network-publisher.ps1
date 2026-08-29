param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'engine-lab-publisher-test-fixture.ps1')
$publisher=Join-Path $PSScriptRoot 'Publish-EngineLabNetworkRelease.ps1'
function Assert-True([bool]$Value,[string]$Message){if(-not$Value){throw $Message}}
function Invoke-Publisher($Fixture,[switch]$DryRun,[string]$FailPoint=''){
    $arguments=@{ProjectRoot=$ProjectRoot;CandidateRoot=$Fixture.Candidate;VerifierBundleRoot=$Fixture.Bundle;VerificationSandboxRoot=$Fixture.Verification;DemoRoot=$Fixture.Demo;TestMode=$true;TestExpectedCandidateManifestSha256=$Fixture.ExpectedManifestSha256;TestExpectedCandidateFiles=$Fixture.ExpectedFiles;TestExpectedSourceCommit=$Fixture.ExpectedSourceCommit;TestExpectedEntrySha256=$Fixture.ExpectedEntrySha256;TestExpectedBuilderCommit=$Fixture.ExpectedBuilderCommit;TestExpectedBuilderScriptSha256=$Fixture.ExpectedBuilderScriptSha256}
    if($DryRun){$arguments.DryRun=$true};if($FailPoint){$arguments.TestFailPoint=$FailPoint}
    & $publisher @arguments
}

$passed=0
# Dry-run 必須完成完整信任鏈驗證，但不得改動 Demo。
$fixture=New-EngineLabPublisherFixture $ProjectRoot
try{
    $before=Get-TestStructuralDigest $fixture.Demo
    $result=Invoke-Publisher $fixture -DryRun|ConvertFrom-Json
    $after=Get-TestStructuralDigest $fixture.Demo
    Assert-True ($result.dryRun -and $result.candidateFiles-eq9 -and$result.releaseFiles-eq10) 'Dry-run inventory rejected.'
    Assert-True ($before-ceq$after) 'Dry-run changed Demo.';$passed++
}finally{Remove-EngineLabPublisherFixture $fixture}

# 成功發布只建立完整不可變版本與單一原子 current/latest 指標。
$fixture=New-EngineLabPublisherFixture $ProjectRoot
try{
    $result=Invoke-Publisher $fixture|ConvertFrom-Json;$release=Join-Path $fixture.Demo "releases/engine-lab/$($fixture.ReleaseId)"
    Assert-True ($result.published-and$result.files-eq9) 'Publisher success receipt rejected.'
    Assert-True (Test-Path -LiteralPath (Join-Path $release 'release.manifest.json') -PathType Leaf) 'Immutable release missing.'
    $pointer=Get-Content -LiteralPath (Join-Path $fixture.Demo 'current/engine-lab.current.json') -Raw|ConvertFrom-Json
    Assert-True ($pointer.role-ceq'current-and-latest-resolver'-and$pointer.mode-ceq'highest-semver-valid-generation') 'Resolver binding rejected.'
    Assert-True (@(Get-ChildItem -LiteralPath (Join-Path $fixture.Demo 'current/engine-lab.generations') -Filter 'v2.54.0-*.json').Count-eq1) 'Generation pointer missing.'
    Assert-TestNoPartial $fixture;$passed++
    $before=Get-PointerSnapshot $fixture;$generations=Get-GenerationSnapshot $fixture;Invoke-Publisher $fixture|Out-Null
    Assert-True ((Get-PointerSnapshot $fixture)-ceq$before-and(Get-GenerationSnapshot $fixture)-ceq$generations) 'Identical release retry changed immutable pointer history.';Assert-TestNoPartial $fixture;$passed++
}finally{Remove-EngineLabPublisherFixture $fixture}

# Resolver 永不改寫；較低與較高的有效 generation 只追加，resolver 以 semver 選最高。
$fixture=New-EngineLabPublisherFixture $ProjectRoot
try{
    Invoke-Publisher $fixture|Out-Null;$resolverBefore=Get-PointerSnapshot $fixture
    Add-TestGeneration $fixture '1.9.0' '11111119'|Out-Null;Invoke-Publisher $fixture|Out-Null
    Assert-True ((Get-PointerSnapshot $fixture)-ceq$resolverBefore-and@(Get-ChildItem (Join-Path $fixture.Demo 'current/engine-lab.generations') -File).Count-eq2) 'Lower generation changed resolver history.';$passed++
    Add-TestGeneration $fixture '3.0.0' '33333333'|Out-Null;Invoke-Publisher $fixture|Out-Null
    Assert-True ((Get-PointerSnapshot $fixture)-ceq$resolverBefore-and@(Get-ChildItem (Join-Path $fixture.Demo 'current/engine-lab.generations') -File).Count-eq3) 'Higher generation changed resolver bytes.';$passed++
}finally{Remove-EngineLabPublisherFixture $fixture}

# 兩個發布程序同時競爭時，只能得到同一份 immutable release、resolver 與 generation。
$fixture=New-EngineLabPublisherFixture $ProjectRoot
try{
    $powerShell=[IO.Path]::GetFullPath((Get-Process -Id $PID).Path)
    $common=@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$publisher,'-ProjectRoot',$ProjectRoot,'-CandidateRoot',$fixture.Candidate,'-VerifierBundleRoot',$fixture.Bundle,'-VerificationSandboxRoot',$fixture.Verification,'-DemoRoot',$fixture.Demo,'-TestMode','-TestExpectedCandidateManifestSha256',$fixture.ExpectedManifestSha256,'-TestExpectedCandidateFiles',[string]$fixture.ExpectedFiles,'-TestExpectedSourceCommit',$fixture.ExpectedSourceCommit,'-TestExpectedEntrySha256',$fixture.ExpectedEntrySha256,'-TestExpectedBuilderCommit',$fixture.ExpectedBuilderCommit,'-TestExpectedBuilderScriptSha256',$fixture.ExpectedBuilderScriptSha256)
    $out1=Join-Path $fixture.Root 'parallel-1.out';$err1=Join-Path $fixture.Root 'parallel-1.err';$out2=Join-Path $fixture.Root 'parallel-2.out';$err2=Join-Path $fixture.Root 'parallel-2.err'
    $one=Start-Process -FilePath $powerShell -ArgumentList $common -WorkingDirectory $ProjectRoot -RedirectStandardOutput $out1 -RedirectStandardError $err1 -PassThru
    $two=Start-Process -FilePath $powerShell -ArgumentList $common -WorkingDirectory $ProjectRoot -RedirectStandardOutput $out2 -RedirectStandardError $err2 -PassThru
    $one.WaitForExit();$two.WaitForExit();$parallelErrors=((Get-Content $err1 -Raw -ErrorAction SilentlyContinue)+(Get-Content $err2 -Raw -ErrorAction SilentlyContinue));Assert-True ($one.ExitCode-eq0-and$two.ExitCode-eq0) "Parallel publisher failed: $($one.ExitCode)/$($two.ExitCode) $parallelErrors"
    Assert-True (@(Get-ChildItem (Join-Path $fixture.Demo 'current/engine-lab.generations') -File).Count-eq1-and(Get-PointerSnapshot $fixture)-cne'<missing>') 'Parallel publisher duplicated immutable pointers.'
    Assert-TestNoPartial $fixture;$passed++
}finally{Remove-EngineLabPublisherFixture $fixture}

# rename 後、pointer 前崩潰留下的完整版本必須可安全採用重試。
$fixture=New-EngineLabPublisherFixture $ProjectRoot
try{
    $failed=$false;try{Invoke-Publisher $fixture -FailPoint 'BeforePointer'|Out-Null}catch{$failed=$true};Assert-True $failed 'Crash-after-rename fixture did not fail.'
    Assert-True ((Get-PointerSnapshot $fixture)-ceq'<missing>'-and(Get-GenerationSnapshot $fixture)-ceq'<missing>') 'Crash-after-rename changed pointer history.'
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture.Demo "releases/engine-lab/$($fixture.ReleaseId)/release.manifest.json")) 'Complete unpointed release missing.'
    Invoke-Publisher $fixture|Out-Null;Assert-True ((Get-PointerSnapshot $fixture)-cne'<missing>') 'Retry did not adopt identical release.';Assert-TestNoPartial $fixture;$passed++
}finally{Remove-EngineLabPublisherFixture $fixture}

# 每一個主要發布邊界都必須 fail-closed。
foreach($point in @('Swap','Copy','Hash','Verify','BeforeRename','Rename','BeforePointer','Pointer')){
    $fixture=New-EngineLabPublisherFixture $ProjectRoot
    try{$before=Get-PointerSnapshot $fixture;$generations=Get-GenerationSnapshot $fixture;$failed=$false;try{Invoke-Publisher $fixture -FailPoint $point|Out-Null}catch{$failed=$true};Assert-True $failed "FailPoint accepted: $point";Assert-True ((Get-PointerSnapshot $fixture)-ceq$before-and(Get-GenerationSnapshot $fixture)-ceq$generations) "FailPoint changed pointer history: $point";Assert-TestNoPartial $fixture;$passed++}finally{Remove-EngineLabPublisherFixture $fixture}
}

[ordered]@{schemaVersion=1;passed=$true;cases=$passed;publisherSha256=Get-TestSha256 $publisher;completedUtc=[DateTimeOffset]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 4
