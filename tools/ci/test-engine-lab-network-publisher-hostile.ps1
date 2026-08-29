param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'engine-lab-publisher-test-fixture.ps1')
$publisher=Join-Path $PSScriptRoot 'Publish-EngineLabNetworkRelease.ps1'
function Assert-True([bool]$Value,[string]$Message){if(-not$Value){throw $Message}}
function Invoke-Publisher($Fixture){&$publisher -ProjectRoot $ProjectRoot -CandidateRoot $Fixture.Candidate -VerifierBundleRoot $Fixture.Bundle -VerificationSandboxRoot $Fixture.Verification -DemoRoot $Fixture.Demo -TestMode -TestExpectedCandidateManifestSha256 $Fixture.ExpectedManifestSha256 -TestExpectedCandidateFiles $Fixture.ExpectedFiles -TestExpectedSourceCommit $Fixture.ExpectedSourceCommit -TestExpectedEntrySha256 $Fixture.ExpectedEntrySha256 -TestExpectedBuilderCommit $Fixture.ExpectedBuilderCommit -TestExpectedBuilderScriptSha256 $Fixture.ExpectedBuilderScriptSha256}
function Assert-Rejected([string]$Name,[scriptblock]$Mutate){
    $fixture=New-EngineLabPublisherFixture $ProjectRoot
    try{&$Mutate $fixture;$before=Get-PointerSnapshot $fixture;$generations=Get-GenerationSnapshot $fixture;$failed=$false;try{Invoke-Publisher $fixture|Out-Null}catch{$failed=$true};Assert-True $failed "Hostile case accepted: $Name";Assert-True ((Get-PointerSnapshot $fixture)-ceq$before-and(Get-GenerationSnapshot $fixture)-ceq$generations) "Hostile case changed pointer history: $Name";Assert-TestNoPartial $fixture;Assert-TestHistory $fixture;$script:passed++}finally{Remove-EngineLabPublisherFixture $fixture}
}
$script:passed=0

Assert-Rejected 'candidate-extra' {param($f)New-TestFile (Join-Path $f.Candidate 'extra.bin') 'extra'}
Assert-Rejected 'candidate-missing' {param($f)Remove-Item -LiteralPath (Join-Path $f.Candidate 'assets/fixture.bin')}
Assert-Rejected 'candidate-hash' {param($f)[IO.File]::AppendAllText((Join-Path $f.Candidate 'assets/fixture.bin'),'changed')}
Assert-Rejected 'candidate-manifest-duplicate-key' {param($f)$p=Join-Path $f.Candidate 'candidate.manifest.json';$raw=[IO.File]::ReadAllText($p);[IO.File]::WriteAllText($p,$raw.Insert($raw.IndexOf('{')+1,'"schemaVersion":1,'))}
Assert-Rejected 'candidate-path-traversal' {param($f)$p=Join-Path $f.Candidate 'candidate.manifest.json';$j=Get-Content $p -Raw|ConvertFrom-Json;$j.files[0].path='../escape';Write-TestJson $p $j}
Assert-Rejected 'candidate-reparse' {param($f)$real=Join-Path $f.Root 'real-assets';Move-Item (Join-Path $f.Candidate 'assets') $real;cmd /c mklink /J (Join-Path $f.Candidate 'assets') $real|Out-Null}
Assert-Rejected 'candidate-ads' {param($f)Set-Content -LiteralPath (Join-Path $f.Candidate 'assets/fixture.bin') -Stream hostile -Value x}
Assert-Rejected 'candidate-hardlink' {param($f)New-Item -ItemType HardLink -Path (Join-Path $f.Root 'linked.bin') -Target (Join-Path $f.Candidate 'assets/fixture.bin')|Out-Null}
Assert-Rejected 'candidate-declared-extra-approved-pin' {param($f)$extra=Join-Path $f.Candidate 'evidence/declared-extra.json';New-TestFile $extra '{}';$p=Join-Path $f.Candidate 'candidate.manifest.json';$j=Get-Content $p -Raw|ConvertFrom-Json;$item=Get-Item $extra;$j.files+=@([pscustomobject]@{path='evidence/declared-extra.json';size=[int64]$item.Length;sha256=Get-TestSha256 $extra});Write-TestJson $p $j}
Assert-Rejected 'approved-manifest-pin' {param($f)$f.ExpectedManifestSha256='0'*64}
Assert-Rejected 'approved-file-count-pin' {param($f)$f.ExpectedFiles++}
Assert-Rejected 'approved-source-commit-pin' {param($f)$f.ExpectedSourceCommit='4'*40}
Assert-Rejected 'approved-entry-hash-pin' {param($f)$f.ExpectedEntrySha256='5'*64}
Assert-Rejected 'approved-builder-commit-pin' {param($f)$f.ExpectedBuilderCommit='6'*40}
Assert-Rejected 'approved-builder-script-pin' {param($f)$f.ExpectedBuilderScriptSha256='7'*64}
Assert-Rejected 'bundle-extra' {param($f)New-TestFile (Join-Path $f.Bundle 'extra.ps1') '# extra'}
Assert-Rejected 'bundle-missing' {param($f)Remove-Item (Join-Path $f.Bundle 'test-engine-lab-network-release-candidate-verifier.ps1')}
Assert-Rejected 'bundle-script-swap' {param($f)[IO.File]::AppendAllText((Join-Path $f.Bundle 'Test-EngineLabNetworkReleaseCandidate.ps1'),'# swap')}
Assert-Rejected 'bundle-manifest-hash' {param($f)$p=Join-Path $f.Bundle 'BUNDLE.MANIFEST.json';$j=Get-Content $p -Raw|ConvertFrom-Json;$j.bundleSha256='0'*64;Write-TestJson $p $j}
Assert-Rejected 'bundle-root-name' {param($f)$bad=Join-Path $f.Root 'phaseb-BAD';[IO.Directory]::Move($f.Bundle,$bad);$f.Bundle=$bad}
Assert-Rejected 'bundle-ads' {param($f)Set-Content -LiteralPath (Join-Path $f.Bundle 'Test-EngineLabNetworkReleaseCandidate.ps1') -Stream hostile -Value x}
Assert-Rejected 'bundle-hardlink' {param($f)New-Item -ItemType HardLink -Path (Join-Path $f.Root 'bundle-link.ps1') -Target (Join-Path $f.Bundle 'Test-EngineLabNetworkReleaseCandidate.ps1')|Out-Null}
Assert-Rejected 'demo-reparse' {param($f)$real=Join-Path $f.Root 'RealDemo';Move-Item $f.Demo $real;cmd /c mklink /J $f.Demo $real|Out-Null}
Assert-Rejected 'verification-reparse' {param($f)$real=Join-Path $f.Root 'RealVerification';Move-Item $f.Verification $real;cmd /c mklink /J $f.Verification $real|Out-Null}

# 既有版本與不可變 resolver/generation 歷史只接受逐位元相同內容。
Assert-Rejected 'existing-release-different' {param($f)$r=Join-Path $f.Demo "releases/engine-lab/$($f.ReleaseId)";New-TestFile (Join-Path $r 'foreign.bin') 'foreign'}
Assert-Rejected 'invalid-existing-pointer-format' {param($f)New-TestFile (Join-Path $f.Demo 'current/engine-lab.current.json') "schema=0`nreleaseId=legacy`n"}
Assert-Rejected 'resolver-tamper-after-publish' {param($f)Invoke-Publisher $f|Out-Null;[IO.File]::AppendAllText((Join-Path $f.Demo 'current/engine-lab.current.json')," `n")}
Assert-Rejected 'generation-tamper-after-publish' {param($f)Invoke-Publisher $f|Out-Null;$g=@(Get-ChildItem (Join-Path $f.Demo 'current/engine-lab.generations') -File)[0];[IO.File]::AppendAllText($g.FullName," `n")}
Assert-Rejected 'generation-extra-file' {param($f)Invoke-Publisher $f|Out-Null;New-TestFile (Join-Path $f.Demo 'current/engine-lab.generations/foreign.txt') 'foreign'}
Assert-Rejected 'same-semver-generation-collision-high' {param($f)Invoke-Publisher $f|Out-Null;Add-TestGeneration $f '2.54.0' '22222222'|Out-Null}
Assert-Rejected 'same-semver-generation-collision-middle' {param($f)Invoke-Publisher $f|Out-Null;Add-TestGeneration $f '3.0.0' '33333331'|Out-Null;Add-TestGeneration $f '2.54.0' '22222223'|Out-Null}
Assert-Rejected 'same-semver-generation-collision-low' {param($f)Invoke-Publisher $f|Out-Null;Add-TestGeneration $f '1.9.0' '11111118'|Out-Null;Add-TestGeneration $f '1.9.0' '11111119'|Out-Null}
Assert-Rejected 'higher-invalid-generation-no-downgrade' {param($f)Invoke-Publisher $f|Out-Null;New-TestFile (Join-Path $f.Demo ('current/engine-lab.generations/v9.0.0-'+('9'*64)+'.json')) '{}'}
$fixture=New-EngineLabPublisherFixture $ProjectRoot
try{
    $residue=Join-Path $fixture.Demo 'current/engine-lab.current.hostile.pending';New-TestFile $residue 'unowned';$before=Get-PointerSnapshot $fixture;$failed=$false;try{Invoke-Publisher $fixture|Out-Null}catch{$failed=$true}
    Assert-True $failed 'Pointer temp residue was accepted.';Assert-True((Get-PointerSnapshot $fixture)-ceq$before)'Pointer residue changed pointer.';Assert-True((Get-TestSha256 $residue)-ceq(Get-StringFixtureHash 'unowned'))'Pointer residue was mutated.';Assert-TestHistory $fixture;$script:passed++
}finally{Remove-EngineLabPublisherFixture $fixture}
# 跨磁碟只在本機有第二個固定磁碟時執行；無條件仍驗證不同 root token 的拒絕邏輯由 publisher parser/selftest涵蓋。
$otherDrive=@(Get-PSDrive -PSProvider FileSystem|Where-Object{$_.Root-and$_.Root-cne[IO.Path]::GetPathRoot($ProjectRoot)-and(Test-Path $_.Root)}|Select-Object -First 1)
if($otherDrive.Count){
    $fixture=New-EngineLabPublisherFixture $ProjectRoot
    $crossRoot=Join-Path $otherDrive[0].Root ('AevryndPublisherCross-'+[Guid]::NewGuid().ToString('N'))
    try{New-Item -ItemType Directory -Path $crossRoot|Out-Null;New-TestFile (Join-Path $crossRoot '.aevrynd-engine-lab-publisher-test-root') "schema=1`npurpose=engine-lab-publisher-test`n";$fixture.Demo=$crossRoot;$before=Get-PointerSnapshot $fixture;$failed=$false;try{Invoke-Publisher $fixture|Out-Null}catch{$failed=$true};Assert-True $failed 'Cross-volume accepted.';Assert-True((Get-PointerSnapshot $fixture)-ceq$before)'Cross-volume changed pointer.';$script:passed++}finally{if(Test-Path $crossRoot){Remove-Item $crossRoot -Recurse -Force};Remove-EngineLabPublisherFixture $fixture}
}

[ordered]@{schemaVersion=1;passed=$true;cases=$script:passed;publisherSha256=Get-TestSha256 $publisher;completedUtc=[DateTimeOffset]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 4
