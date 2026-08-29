Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-TestSha256([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Write-TestJson([string]$Path,[object]$Object) { [IO.File]::WriteAllText($Path,(($Object|ConvertTo-Json -Depth 24).Replace("`r`n","`n")+"`n"),[Text.UTF8Encoding]::new($false)) }
function New-TestFile([string]$Path,[string]$Content) { New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($Path)) -Force|Out-Null;[IO.File]::WriteAllText($Path,$Content,[Text.UTF8Encoding]::new($false)) }
function Get-BundleDigest([object[]]$Files) { $rows=@($Files|ForEach-Object{"$($_.path)|$($_.size)|$($_.sha256)"});$sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($rows-join"`n")+"`n")))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()} }

function New-EngineLabPublisherFixture([string]$ProjectRoot) {
    $root=Join-Path ([IO.Path]::GetPathRoot($ProjectRoot)) ('AevryndPublisherTest-'+[Guid]::NewGuid().ToString('N'))
    $candidate=Join-Path $root 'engine-lab-v254-fixture';$demo=Join-Path $root 'Demo';$verification=Join-Path $root 'VerificationSandbox'
    New-Item -ItemType Directory -Path $candidate,$demo,$verification -Force|Out-Null
    New-TestFile (Join-Path $demo '.aevrynd-engine-lab-publisher-test-root') "schema=1`npurpose=engine-lab-publisher-test`n"
    New-TestFile (Join-Path $demo 'Aevrynd-Engine-Lab.exe') 'legacy-latest-must-remain-unchanged'
    New-TestFile (Join-Path $demo 'history/immutable.txt') 'immutable-history'
    $paths=@(
        'Aevrynd-Engine-Lab.exe',
        'assets/fixture.bin',
        'evidence/candidate-build-receipt.json',
        'evidence/toolchain.json',
        'evidence/pe-tool.json',
        'evidence/normal-network-capture-receipt.json',
        'evidence/normal-network-runtime-receipt.json',
        'evidence/network-capture-receipt.json',
        'evidence/network-runtime-receipt.json'
    )
    foreach($relative in $paths){$content=if($relative.EndsWith('.exe')){'MZ-fixture'}elseif($relative.EndsWith('.bin')){'fixture-asset'}else{'{}'};New-TestFile (Join-Path $candidate $relative) $content}
    $records=@(foreach($relative in $paths|Sort-Object){$item=Get-Item -LiteralPath (Join-Path $candidate $relative);[ordered]@{path=$relative;size=[int64]$item.Length;sha256=Get-TestSha256 $item.FullName}})
    $manifest=[ordered]@{schemaVersion=1;releaseId='v2.54.0-engine-v2.86.0-11111111';product='EngineLab';productVersion='2.54.0';engineVersion='2.86.0';commit=('1'*40);entry='Aevrynd-Engine-Lab.exe';featureStage='NetworkReplicationPhase1ValidatedWip';maturity='validated-wip';transportScope='deterministic-loopback';derivedCaptures=@();files=$records}
    Write-TestJson (Join-Path $candidate 'candidate.manifest.json') $manifest

    $bundleSource=Join-Path $root 'bundle-source';New-Item -ItemType Directory -Path $bundleSource|Out-Null
    $verifier=@'
param([string]$ProjectRoot,[string]$CandidateRoot,[string]$VerificationRoot)
$ErrorActionPreference='Stop';New-Item -ItemType Directory -Path $VerificationRoot|Out-Null
$manifest=Get-Content -LiteralPath (Join-Path $CandidateRoot 'candidate.manifest.json') -Raw|ConvertFrom-Json
$candidateManifestPath=Join-Path $CandidateRoot 'candidate.manifest.json';$stream=[IO.File]::OpenRead($candidateManifestPath);try{$sha=[Security.Cryptography.SHA256]::Create();try{$hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}}finally{$stream.Dispose()}
$receipt=[ordered]@{schemaVersion=1;verified=$true;candidateRoot=[IO.Path]::GetFullPath($CandidateRoot).TrimEnd('\');sourceCommit=$manifest.commit;builderCommit=('2'*40);builderScriptSha256=('3'*64);manifestSha256=$hash;manifestFiles=$manifest.files.Count;executables=1;captures=0;bundledDependencies=0;ctestPassed=1;networkGates=1;verifiedUtc=[DateTimeOffset]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path $VerificationRoot 'candidate-verification.json'),(($receipt|ConvertTo-Json -Depth 8).Replace("`r`n","`n")+"`n"),[Text.UTF8Encoding]::new($false))
'@
    New-TestFile (Join-Path $bundleSource 'Test-EngineLabNetworkReleaseCandidate.ps1') $verifier
    New-TestFile (Join-Path $bundleSource 'test-engine-lab-network-release-candidate-verifier-hostile.ps1') "# fixture hostile`n"
    New-TestFile (Join-Path $bundleSource 'test-engine-lab-network-release-candidate-verifier.ps1') "# fixture selftest`n"
    $bundleFiles=@(foreach($name in @('Test-EngineLabNetworkReleaseCandidate.ps1','test-engine-lab-network-release-candidate-verifier-hostile.ps1','test-engine-lab-network-release-candidate-verifier.ps1')){$item=Get-Item (Join-Path $bundleSource $name);[ordered]@{path=$name;size=[int64]$item.Length;sha256=Get-TestSha256 $item.FullName}})
    $digest=Get-BundleDigest $bundleFiles;$bundle=Join-Path $root ('phaseb-'+$digest.ToUpperInvariant());[IO.Directory]::Move($bundleSource,$bundle)
    Write-TestJson (Join-Path $bundle 'BUNDLE.MANIFEST.json') ([ordered]@{schemaVersion=1;algorithm='sha256-utf8-ordinal-path-size-hash-lf-v1';files=$bundleFiles;bundleSha256=$digest})
    $manifestPath=Join-Path $candidate 'candidate.manifest.json'
    [pscustomobject]@{Root=$root;Candidate=$candidate;Demo=$demo;Verification=$verification;Bundle=$bundle;ReleaseId=$manifest.releaseId;ExpectedManifestSha256=(Get-TestSha256 $manifestPath);ExpectedFiles=$records.Count;ExpectedSourceCommit=[string]$manifest.commit;ExpectedEntrySha256=(Get-TestSha256 (Join-Path $candidate 'Aevrynd-Engine-Lab.exe'));ExpectedBuilderCommit=('2'*40);ExpectedBuilderScriptSha256=('3'*64)}
}
function Remove-EngineLabPublisherFixture($Fixture) {
    $full=[IO.Path]::GetFullPath([string]$Fixture.Root).TrimEnd('\');$leaf=[IO.Path]::GetFileName($full)
    if($leaf -notmatch '^AevryndPublisherTest-[0-9a-f]{32}$' -or [IO.Path]::GetPathRoot($full)-eq$full){throw 'Fixture cleanup root rejected.'}
    if(Test-Path -LiteralPath $full){Remove-Item -LiteralPath $full -Recurse -Force}
}
function Get-PointerSnapshot($Fixture) {
    $path=Join-Path $Fixture.Demo 'current/engine-lab.current.json'
    if(Test-Path -LiteralPath $path){return (Get-TestSha256 $path)}
    return '<missing>'
}
function Get-GenerationSnapshot($Fixture){
    $root=Join-Path $Fixture.Demo 'current/engine-lab.generations';if(-not(Test-Path -LiteralPath $root)-or-not@(Get-ChildItem -LiteralPath $root -Force -File -ErrorAction SilentlyContinue).Count){return '<missing>'};return Get-TestStructuralDigest $root
}
function Get-TestStructuralDigest([string]$Root) {
    $full=[IO.Path]::GetFullPath($Root).TrimEnd('\');$rows=@()
    foreach($item in Get-ChildItem -LiteralPath $full -Force -Recurse|Sort-Object FullName){$relative=$item.FullName.Substring($full.Length+1).Replace('\','/');if($item.PSIsContainer){$rows+="D|$relative"}else{$rows+="F|$relative|$($item.Length)|$(Get-TestSha256 $item.FullName)"}}
    $sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($rows-join"`n")+"`n")))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
}
function Assert-TestNoPartial($Fixture) {
    $staging=Join-Path $Fixture.Demo 'releases/engine-lab/.staging'
    if(Test-Path -LiteralPath $staging){if(@(Get-ChildItem -LiteralPath $staging -Force).Count){throw 'Test found staging residue.'}}
    if(@(Get-ChildItem -LiteralPath (Join-Path $Fixture.Demo 'current') -Filter '*.pending' -ErrorAction SilentlyContinue).Count){throw 'Test found pointer pending residue.'}
}
function Assert-TestHistory([object]$Fixture) {
    if((Get-TestSha256 (Join-Path $Fixture.Demo 'Aevrynd-Engine-Lab.exe'))-cne(Get-StringFixtureHash 'legacy-latest-must-remain-unchanged')){throw 'Legacy latest history changed.'}
    if((Get-TestSha256 (Join-Path $Fixture.Demo 'history/immutable.txt'))-cne(Get-StringFixtureHash 'immutable-history')){throw 'Immutable history changed.'}
}
function Get-StringFixtureHash([string]$Value){$sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}}
function Add-TestGeneration([object]$Fixture,[string]$Version,[string]$Suffix){
    $source=Join-Path $Fixture.Demo "releases/engine-lab/$($Fixture.ReleaseId)";if(-not(Test-Path -LiteralPath $source)){throw 'Source test release missing.'}
    $releaseId="v$Version-engine-v2.86.0-$Suffix";$destination=Join-Path $Fixture.Demo "releases/engine-lab/$releaseId";Copy-Item -LiteralPath $source -Destination $destination -Recurse
    $manifestPath=Join-Path $destination 'release.manifest.json';$manifest=Get-Content $manifestPath -Raw|ConvertFrom-Json;$manifest.productVersion=$Version;$manifest.releaseId=$releaseId;Write-TestJson $manifestPath $manifest;$hash=Get-TestSha256 $manifestPath
    $generation=[ordered]@{schemaVersion=1;role='release-generation';product='EngineLab';productVersion=$Version;releaseId=$releaseId;releasePath="releases/engine-lab/$releaseId";entry='Aevrynd-Engine-Lab.exe';manifestSha256=$hash;legacyLatestExecutableMode='preserved-unmanaged'}
    $generationRoot=Join-Path $Fixture.Demo 'current/engine-lab.generations';New-Item -ItemType Directory -Path $generationRoot -Force|Out-Null;Write-TestJson (Join-Path $generationRoot "v$Version-$hash.json") $generation
    [pscustomobject]@{ReleaseId=$releaseId;ManifestSha256=$hash}
}
