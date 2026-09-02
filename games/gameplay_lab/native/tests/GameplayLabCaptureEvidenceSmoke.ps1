param(
  [ValidateSet('All','Temporal','Receipt')]
  [string]$Section = 'All',
  [string]$RealHdrValidator = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GameplayLabCaptureEvidence.ps1')

function Expect-Failure([scriptblock]$Action, [string]$Name) {
  try { & $Action; throw "Negative unexpectedly passed: $Name" } catch {
    if ($_.Exception.Message -eq "Negative unexpectedly passed: $Name") { throw }
  }
}

if((Get-GameplayCanonicalSingleRawBits ([BitConverter]::ToSingle([byte[]](0,0,0,128),0)))-ne0){throw 'Canonical negative zero raw bits were not normalized'}
Expect-Failure {Get-GameplayCanonicalSingleRawBits ([single]::NaN)} 'non-finite canonical single'

$renderRaw=[pscustomobject]@{frameSerial=1;sceneDrawRevision=11;worldRevision=21;submittedMeshInstances=3;submittedSkinnedInstances=2;submittedTerrainInstances=1;submittedWorldVertices=100;submittedWorldIndices=150}
$renderEffective=[pscustomobject]@{frameSerial=2;sceneDrawRevision=12;worldRevision=22;submittedMeshInstances=3;submittedSkinnedInstances=2;submittedTerrainInstances=1;submittedWorldVertices=100;submittedWorldIndices=150}
$sealed=[pscustomobject]@{source='GameplayLabDeterminismIntegrity';captures=@([pscustomobject]@{name='first-day.png';determinismFrameSerial=2;determinismIntegrityRequested='SealedCapture';determinismIntegrityActual='SealedCapture';determinismIntegrityReason='None';determinismIntegritySampled=2;determinismIntegrityVerified=2;determinismIntegrityRejected=0;determinismIntegrityAcceptedFrame=1;canonicalDrawFingerprint=[pscustomobject]@{schemaVersion=1;topology=1;material=2;transform=3;palette=4;identity=5;order=6};renderProvenance=$renderEffective})}
$null = Assert-GameplayDeterminismIntegrityEvidence -Receipt $sealed -ExpectedCaptures 1
$missing=[pscustomobject]@{source='GameplayLabDeterminismIntegrity';captures=@()};Expect-Failure {Assert-GameplayDeterminismIntegrityEvidence -Receipt $missing -ExpectedCaptures 1} 'missing integrity capture'
$runtime=$sealed|ConvertTo-Json -Depth 8|ConvertFrom-Json;$runtime.captures[0].determinismIntegrityRequested='Runtime';Expect-Failure {Assert-GameplayDeterminismIntegrityEvidence -Receipt $runtime -ExpectedCaptures 1} 'stale runtime integrity'
$rejected=$sealed|ConvertTo-Json -Depth 8|ConvertFrom-Json;$rejected.captures[0].determinismIntegrityRejected=1;Expect-Failure {Assert-GameplayDeterminismIntegrityEvidence -Receipt $rejected -ExpectedCaptures 1} 'rejected integrity material'
$rawIntegrityPhase=[pscustomobject]@{index=0;frameSerial=1;drawListRevision=11;determinismFrameSerial=1;determinismIntegrityRequested='SealedCapture';determinismIntegrityActual='SealedCapture';determinismIntegrityReason='None';determinismIntegritySampled=2;determinismIntegrityVerified=2;determinismIntegrityRejected=0;determinismIntegrityAcceptedFrame=1;canonicalDrawFingerprint=$sealed.captures[0].canonicalDrawFingerprint;renderProvenance=$renderRaw}
$effectiveIntegrityPhase=[pscustomobject]@{index=1;frameSerial=2;drawListRevision=12;determinismFrameSerial=2;determinismIntegrityRequested='SealedCapture';determinismIntegrityActual='SealedCapture';determinismIntegrityReason='None';determinismIntegritySampled=2;determinismIntegrityVerified=2;determinismIntegrityRejected=0;determinismIntegrityAcceptedFrame=1;canonicalDrawFingerprint=$sealed.captures[0].canonicalDrawFingerprint;renderProvenance=$renderEffective}
$frameBoundSequence=[pscustomobject]@{captures=@([pscustomobject]@{name='first-day.png';token=1;phases=@($rawIntegrityPhase,$effectiveIntegrityPhase)})}
$frameBoundCharacter=[pscustomobject]@{captures=@(($sealed.captures[0]|ConvertTo-Json -Depth 8|ConvertFrom-Json))}
$null = Assert-GameplayDeterminismIntegrityEvidence -Receipt $sealed -ExpectedCaptures 1 -SequenceReceipt $frameBoundSequence -CharacterReceipt $frameBoundCharacter
$brokenFrameBound=$frameBoundCharacter|ConvertTo-Json -Depth 12|ConvertFrom-Json;$brokenFrameBound.captures[0].determinismFrameSerial++
Expect-Failure {Assert-GameplayDeterminismIntegrityEvidence -Receipt $sealed -ExpectedCaptures 1 -SequenceReceipt $frameBoundSequence -CharacterReceipt $brokenFrameBound} 'cross-receipt frame serial drift'
$missingRender=$frameBoundSequence|ConvertTo-Json -Depth 12|ConvertFrom-Json;$missingRender.captures[0].phases[1].PSObject.Properties.Remove('renderProvenance')
Expect-Failure {Assert-GameplayDeterminismIntegrityEvidence -Receipt $sealed -ExpectedCaptures 1 -SequenceReceipt $missingRender -CharacterReceipt $frameBoundCharacter} 'missing frame-bound render provenance'
$wrongRenderFrame=$frameBoundSequence|ConvertTo-Json -Depth 12|ConvertFrom-Json;$wrongRenderFrame.captures[0].phases[1].renderProvenance.frameSerial++
Expect-Failure {Assert-GameplayDeterminismIntegrityEvidence -Receipt $sealed -ExpectedCaptures 1 -SequenceReceipt $wrongRenderFrame -CharacterReceipt $frameBoundCharacter} 'render provenance frame drift'
$wrongRenderRevision=$frameBoundSequence|ConvertTo-Json -Depth 12|ConvertFrom-Json;$wrongRenderRevision.captures[0].phases[1].renderProvenance.sceneDrawRevision++
Expect-Failure {Assert-GameplayDeterminismIntegrityEvidence -Receipt $sealed -ExpectedCaptures 1 -SequenceReceipt $wrongRenderRevision -CharacterReceipt $frameBoundCharacter} 'render provenance scene revision drift'
$wrongRenderCount=$frameBoundCharacter|ConvertTo-Json -Depth 12|ConvertFrom-Json;$wrongRenderCount.captures[0].renderProvenance.submittedMeshInstances++
Expect-Failure {Assert-GameplayDeterminismIntegrityEvidence -Receipt $sealed -ExpectedCaptures 1 -SequenceReceipt $frameBoundSequence -CharacterReceipt $wrongRenderCount} 'cross-receipt render count drift'

$scratch = Join-Path ([IO.Path]::GetTempPath()) ("aevrynd-gameplay-evidence-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $scratch | Out-Null
try {
  if ($Section -ne 'Temporal') {
    $hdrRoot=Join-Path $scratch 'hdr-evidence';New-Item -ItemType Directory -Force -Path $hdrRoot|Out-Null
    $stubValidator=Join-Path $hdrRoot 'hdr-validator-stub.ps1'
    [IO.File]::WriteAllText($stubValidator,@'
param([string]$SidecarPath)
if($env:AEVRYND_HDR_STUB_MODE-eq'nonzero'){[Console]::Error.WriteLine('rejected');exit 7}
if($env:AEVRYND_HDR_STUB_MODE-eq'invalid'){[Console]::Out.WriteLine('not-json');exit 0}
$bytes=[IO.File]::ReadAllBytes($SidecarPath);$sha=[Security.Cryptography.SHA256]::Create();try{$fileHash=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
$digit=[string]((([int]$bytes[0]-1)%9)+1)
$row=[ordered]@{schemaVersion=3;byteCount=[uint64]$bytes.Length;fileHash=$fileHash;stableContentHash=('d'*63)+$digit;payloadHash=('a'*63)+$digit;rootHash=('b'*63)+$digit}
switch($env:AEVRYND_HDR_STUB_MODE){'missingSchema'{$null=$row.Remove('schemaVersion')};'missingByteCount'{$null=$row.Remove('byteCount')};'missingFileHash'{$null=$row.Remove('fileHash')};'missingStableContentHash'{$null=$row.Remove('stableContentHash')};'missingPayloadHash'{$null=$row.Remove('payloadHash')};'missingRootHash'{$null=$row.Remove('rootHash')};'zeroFileHash'{$row.fileHash='0'*64};'zeroStableContentHash'{$row.stableContentHash='0'*64};'zeroPayloadHash'{$row.payloadHash='0'*64};'zeroRootHash'{$row.rootHash='0'*64};'schema'{$row.schemaVersion=1};'byteCount'{$row.byteCount++};'fileHash'{$row.fileHash='c'*64};'stableContentHash'{$row.stableContentHash='e'*64};'payloadHash'{$row.payloadHash='e'*64};'rootHash'{$row.rootHash='e'*64}}
$json=$row|ConvertTo-Json -Compress;[Console]::Out.WriteLine($json);if($env:AEVRYND_HDR_STUB_MODE-eq'multiple'){[Console]::Out.WriteLine($json)}
'@,[Text.UTF8Encoding]::new($false))
    $hdrNames=@('first-day.png','third-day.png','god-day.png','first-dusk.png','third-dusk.png','god-dusk.png','first-night.png','third-night.png','god-night.png','palette-building.png','melee-combat.png')
    $hdrRunId='01234567-89ab-4def-8123-456789abcdef'
    $hdrRows=@();$hdrSequences=@()
    for($index=0;$index-lt11;++$index){$relative="capture-$index.aevhdr";$path=Join-Path $hdrRoot $relative;[IO.File]::WriteAllBytes($path,[byte[]]@([byte]($index+1),0x41,0x45,0x56));$snapshot=Get-GameplayEvidenceFileSnapshot $path;$token=[uint64](700+$index);$frame=[uint64](900+$index);$digit=(($index%9)+1);$hdrRows+=[pscustomobject]@{name=$hdrNames[$index];path=$relative;state='Verified';byteCount=$snapshot.length;fileHash=$snapshot.sha256;payloadHash=('a' * 63)+$digit;rootHash=('b' * 63)+$digit;stableContentHash=('d' * 63)+$digit;frameSerial=$frame;sequenceToken=$token;sequenceIndex=1;verified=$true};$hdrSequences+=[pscustomobject]@{name=$hdrNames[$index];token=$token;phases=@([pscustomobject]@{index=0;sequenceIndex=0;sequenceToken=$token;frameSerial=$frame-1},[pscustomobject]@{index=1;sequenceIndex=1;sequenceToken=$token;frameSerial=$frame})}}
    $hdrReceipt=[pscustomobject]@{runId=$hdrRunId;source='GameplayLabProductionHdr';expectedCaptures=11;engineRawPeakBytes=44;maximumInFlight=1;captures=$hdrRows};$hdrSequence=[pscustomobject]@{runId=$hdrRunId;captures=$hdrSequences}
    $validated=@(Assert-GameplayHdrCaptureEvidence $hdrRoot $hdrReceipt $hdrSequence $hdrNames $stubValidator $hdrRunId);if($validated.Count-ne11){throw 'HDR evidence positive fixture count failed'}
    $copiedSidecar=Join-Path $hdrRoot 'copied-run-entry.aevhdr';[IO.File]::Copy((Join-Path $hdrRoot $validated[0].path),$copiedSidecar,$true);$null=Assert-GameplayHdrValidatorExactEvidence $stubValidator $copiedSidecar $validated[0] 'copied run entry'
    [IO.File]::AppendAllText($copiedSidecar,'x');Expect-Failure {Assert-GameplayHdrValidatorExactEvidence $stubValidator $copiedSidecar $validated[0] 'mutated copied run entry'|Out-Null} 'copied HDR sidecar mutation';Remove-Item -LiteralPath $copiedSidecar -Force
    $missingPath=Join-Path $hdrRoot $hdrRows[0].path;$savedBytes=[IO.File]::ReadAllBytes($missingPath);Remove-Item -LiteralPath $missingPath -Force;Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hdrReceipt $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR sidecar missing file';[IO.File]::WriteAllBytes($missingPath,$savedBytes)
    [IO.File]::AppendAllText($missingPath,'x');Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hdrReceipt $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR sidecar changed byte';[IO.File]::WriteAllBytes($missingPath,$savedBytes)
    Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hdrReceipt $hdrSequence $hdrNames (Join-Path $hdrRoot 'missing-validator.exe') $hdrRunId|Out-Null} 'HDR validator missing'
    foreach($mode in @('nonzero','invalid','multiple','missingSchema','missingByteCount','missingFileHash','missingStableContentHash','missingPayloadHash','missingRootHash','zeroFileHash','zeroStableContentHash','zeroPayloadHash','zeroRootHash','schema','byteCount','fileHash','stableContentHash','payloadHash','rootHash')){$env:AEVRYND_HDR_STUB_MODE=$mode;try{Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hdrReceipt $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} "HDR validator hostile $mode"}finally{Remove-Item Env:AEVRYND_HDR_STUB_MODE -ErrorAction SilentlyContinue}}
    foreach($mutation in @(@('source','SyntheticHdr'),@('runId',[Guid]::NewGuid().ToString('D')),@('expectedCaptures',10),@('engineRawPeakBytes',0),@('engineRawPeakBytes',16777217),@('maximumInFlight',2))){$hostile=$hdrReceipt|ConvertTo-Json -Depth 8|ConvertFrom-Json;$hostile.($mutation[0])=$mutation[1];Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hostile $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} "HDR header mutation $($mutation[0])"}
    $hostileSequence=$hdrSequence|ConvertTo-Json -Depth 8|ConvertFrom-Json;$hostileSequence.runId=[Guid]::NewGuid().ToString('D');Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hdrReceipt $hostileSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR sequence runId mutation'
    $hostileSequence=$hdrSequence|ConvertTo-Json -Depth 8|ConvertFrom-Json;$hostileSequence.captures[0].phases[1].frameSerial++;Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hdrReceipt $hostileSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR sequence phase mutation'
    $hostile=$hdrReceipt|ConvertTo-Json -Depth 8|ConvertFrom-Json;$swap=$hostile.captures[0];$hostile.captures[0]=$hostile.captures[1];$hostile.captures[1]=$swap;Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hostile $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR receipt order mutation'
    $hostile=$hdrReceipt|ConvertTo-Json -Depth 8|ConvertFrom-Json;$hostile.captures[1]=$hostile.captures[0];Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hostile $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR receipt duplicate mutation'
    $hostile=$hdrReceipt|ConvertTo-Json -Depth 8|ConvertFrom-Json;$hostile.captures[0].verified=$false;Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hostile $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR receipt verified false'
    $hostile=$hdrReceipt|ConvertTo-Json -Depth 8|ConvertFrom-Json;$hostile.captures[0].state='Published';Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hostile $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR receipt state mutation'
    $hostile=$hdrReceipt|ConvertTo-Json -Depth 8|ConvertFrom-Json;$hostile.captures[0].fileHash='c'*64;Expect-Failure {Assert-GameplayHdrCaptureEvidence $hdrRoot $hostile $hdrSequence $hdrNames $stubValidator $hdrRunId|Out-Null} 'HDR receipt hash mutation'
    $baseline=@(Assert-GameplayHdrCaptureEvidence $hdrRoot $hdrReceipt $hdrSequence $hdrNames $stubValidator $hdrRunId);$hostile=@($baseline|ForEach-Object{$_|ConvertTo-Json -Depth 8|ConvertFrom-Json});$hostile[0].stableContentHash=('e'*64);Expect-Failure {Assert-GameplayHdrStableContentDeterminism -Baseline $baseline -Candidate $hostile -ExpectedNames $hdrNames|Out-Null} 'HDR stable content hash cross-run mutation'
    if(-not[string]::IsNullOrWhiteSpace($RealHdrValidator)){$real=[IO.Path]::GetFullPath($RealHdrValidator);if(-not[IO.Path]::IsPathRooted($RealHdrValidator)-or-not(Test-Path -LiteralPath $real -PathType Leaf)){throw 'Optional real HDR validator path was invalid'};$invalidSidecar=Join-Path $hdrRoot 'arbitrary-four-byte.aevhdr';[IO.File]::WriteAllBytes($invalidSidecar,[byte[]](1,2,3,4));$probe=Invoke-GameplayHdrValidator $real $invalidSidecar;if([int]$probe.exitCode-eq0){throw 'Real HDR validator accepted an arbitrary four-byte sidecar'}}
  }
  if ($Section -ne 'Receipt') {
  Add-Type -AssemblyName System.Drawing
  $rawBeautyPath=Join-Path $scratch 'ghost-raw-beauty.png';$effectiveBeautyPath=Join-Path $scratch 'ghost-effective-beauty.png';$rawObjectPath=Join-Path $scratch 'ghost-raw-object.png';$effectiveObjectPath=Join-Path $scratch 'ghost-effective-object.png';$rawDepthPath=Join-Path $scratch 'ghost-raw-depth.png';$effectiveDepthPath=Join-Path $scratch 'ghost-effective-depth.png'
  $rawBeauty=[Drawing.Bitmap]::new(64,64);$effectiveBeauty=[Drawing.Bitmap]::new(64,64);$rawObject=[Drawing.Bitmap]::new(64,64);$effectiveObject=[Drawing.Bitmap]::new(64,64);$rawDepth=[Drawing.Bitmap]::new(64,64);$effectiveDepth=[Drawing.Bitmap]::new(64,64)
  try{for($y=0;$y-lt64;++$y){for($x=0;$x-lt64;++$x){$cloud=[int](80+24*[Math]::Sin($x*.21)+18*[Math]::Cos($y*.17));$color=[Drawing.Color]::FromArgb(255,$cloud,[Math]::Min(255,$cloud+35),[Math]::Min(255,$cloud+70));$rawBeauty.SetPixel($x,$y,$color);$effectiveBeauty.SetPixel($x,$y,$color);$rawObject.SetPixel($x,$y,[Drawing.Color]::Black);$effectiveObject.SetPixel($x,$y,[Drawing.Color]::Black);$rawDepth.SetPixel($x,$y,[Drawing.Color]::Black);$effectiveDepth.SetPixel($x,$y,[Drawing.Color]::Black)}};$rawBeauty.Save($rawBeautyPath);$effectiveBeauty.Save($effectiveBeautyPath);$rawObject.Save($rawObjectPath);$effectiveObject.Save($effectiveObjectPath);$rawDepth.Save($rawDepthPath);$effectiveDepth.Save($effectiveDepthPath)}finally{$rawBeauty.Dispose();$effectiveBeauty.Dispose();$rawObject.Dispose();$effectiveObject.Dispose();$rawDepth.Dispose();$effectiveDepth.Dispose()}
  $cleanGhost=Get-GameplayTemporalGhostEvidence $rawBeautyPath $effectiveBeautyPath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32
  if($cleanGhost.pass-ne1-or$cleanGhost.contract-ne'raw-effective-full-frame-v3'-or$cleanGhost.boundsX-ne16-or$cleanGhost.boundsY-ne8-or$cleanGhost.eligiblePixels-ne1024-or$cleanGhost.coveredPixels-ne1024-or$cleanGhost.excludedPixels-ne0-or$cleanGhost.stableEmptyPixels-ne1024-or$cleanGhost.objectAdjacentRingPixels-ne0-or$cleanGhost.meanAbsoluteChannelDifference-ne0-or$cleanGhost.maximumChannelDifference-ne0-or$cleanGhost.coverageSha256-notmatch'^[0-9a-f]{64}$'){throw 'Static textured cloud temporal pair was rejected'}
  if((Get-GameplayTemporalGhostEvidence $rawBeautyPath $effectiveBeautyPath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32).coverageSha256-ne$cleanGhost.coverageSha256){throw 'Temporal full-frame coverage was nondeterministic'}
  # 低幅度移動雲可通過；雲紋理本身不再與側邊中位數比較。 (moving cloud fixture).
  $movingPath=Join-Path $scratch 'ghost-moving-cloud.png';$moving=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{for($y=0;$y-lt64;++$y){for($x=0;$x-lt64;++$x){$c=$moving.GetPixel($x,$y);$delta=if((($x+$y)%3)-eq0){1}else{0};$moving.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,[Math]::Min(255,$c.R+$delta),[Math]::Min(255,$c.G+$delta),[Math]::Min(255,$c.B+$delta)))}};$moving.Save($movingPath)}finally{$moving.Dispose()}
  $movingGhost=Get-GameplayTemporalGhostEvidence $rawBeautyPath $movingPath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32
  if($movingGhost.pass-ne1-or$movingGhost.meanAbsoluteChannelDifference-le0-or$movingGhost.maximumChannelDifference-ne1){throw 'Legal low-amplitude moving cloud sequence was rejected'}
  $legacyPath=Join-Path $scratch 'ghost-legacy-stale.png';$legacy=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{for($y=16;$y-lt36;++$y){for($x=22;$x-lt42;++$x){$c=$legacy.GetPixel($x,$y);$legacy.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,[Math]::Min(255,$c.R+48),[Math]::Min(255,$c.G+48),[Math]::Min(255,$c.B+48)))}};$legacy.Save($legacyPath)}finally{$legacy.Dispose()}
  $legacyGhost=Get-GameplayTemporalGhostEvidence $rawBeautyPath $legacyPath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32
  if($legacyGhost.pass-ne0){throw 'Legacy stale-history mutation was not rejected'}
  $offsetPath=Join-Path $scratch 'ghost-offset.png';$offset=[Drawing.Bitmap]::new(64,64);$source=[Drawing.Bitmap]::FromFile($rawBeautyPath);try{for($y=0;$y-lt64;++$y){for($x=0;$x-lt64;++$x){$offset.SetPixel($x,$y,$source.GetPixel(($x+7)%64,$y))}};$offset.Save($offsetPath)}finally{$offset.Dispose();$source.Dispose()}
  $offsetGhost=Get-GameplayTemporalGhostEvidence $rawBeautyPath $offsetPath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32
  if($offsetGhost.pass-ne0){throw 'Offset/reprojected stale-history mutation was not rejected'}
  $driftPath=Join-Path $scratch 'ghost-diffuse-drift.png';$drift=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{for($y=0;$y-lt64;++$y){for($x=0;$x-lt64;++$x){$c=$drift.GetPixel($x,$y);$drift.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,[Math]::Min(255,$c.R+2),[Math]::Min(255,$c.G+2),[Math]::Min(255,$c.B+2)))}};$drift.Save($driftPath)}finally{$drift.Dispose()}
  $driftGhost=Get-GameplayTemporalGhostEvidence $rawBeautyPath $driftPath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32
  if($driftGhost.pass-ne0-or$driftGhost.maximumChannelDifference-gt8){throw 'Diffuse low-amplitude stale-history drift was not isolated by the mean gate'}
  $spikePath=Join-Path $scratch 'ghost-single-spike.png';$spike=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{$c=$spike.GetPixel(24,24);$spike.SetPixel(24,24,[Drawing.Color]::FromArgb(255,[Math]::Min(255,$c.R+9),[Math]::Min(255,$c.G+9),[Math]::Min(255,$c.B+9)));$spike.Save($spikePath)}finally{$spike.Dispose()}
  $spikeGhost=Get-GameplayTemporalGhostEvidence $rawBeautyPath $spikePath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32
  if($spikeGhost.pass-ne1-or$spikeGhost.stableLevels[0].pixels-ne1-or$spikeGhost.stableLevels[0].maximumConnectedPixels-ne1){throw "Visually negligible isolated +9 quantization pixel was not tolerated and measured: pass=$($spikeGhost.pass) mean=$($spikeGhost.meanAbsoluteChannelDifference) tileMean=$($spikeGhost.worstTileMeanAbsoluteChannelDifference) levels=$(($spikeGhost.stableLevels|ConvertTo-Json -Compress))"}
  $wideRawPath=Join-Path $scratch 'ghost-wide-raw.png';$wideEffectivePath=Join-Path $scratch 'ghost-wide-effective.png';$wideObjectPath=Join-Path $scratch 'ghost-wide-object.png';$wideDepthPath=Join-Path $scratch 'ghost-wide-depth.png';$wr=[Drawing.Bitmap]::new(192,128);$we=[Drawing.Bitmap]::new(192,128);$wo=[Drawing.Bitmap]::new(192,128);$wd=[Drawing.Bitmap]::new(192,128)
  try{for($y=0;$y-lt128;++$y){for($x=0;$x-lt192;++$x){$v=70+(($x*3+$y*5)%24);$c=[Drawing.Color]::FromArgb(255,$v,$v+24,$v+48);$wr.SetPixel($x,$y,$c);$we.SetPixel($x,$y,$c);$wo.SetPixel($x,$y,[Drawing.Color]::Black);$wd.SetPixel($x,$y,[Drawing.Color]::Black)}};for($y=104;$y-lt120;++$y){for($x=168;$x-lt184;++$x){$c=$we.GetPixel($x,$y);$we.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,$c.R+40,$c.G+40,$c.B+40))}};$wr.Save($wideRawPath);$we.Save($wideEffectivePath);$wo.Save($wideObjectPath);$wd.Save($wideDepthPath)}finally{$wr.Dispose();$we.Dispose();$wo.Dispose();$wd.Dispose()}
  # 每格偏差契約：正負交錯的合法 TAA 可通過，單格或兩格同方向擴散必須拒絕。
  $bias15File = (Join-Path $scratch 'ghost-tile-positive-1p5.png')
  $bias2File = (Join-Path $scratch 'ghost-tile-positive-2.png')
  $twoBias15File = (Join-Path $scratch 'ghost-two-tile-positive-1p5.png')
  $alternatingFile = (Join-Path $scratch 'ghost-tile-alternating-2.png')
  $p15=[Drawing.Bitmap]::FromFile($wideRawPath);$p2=[Drawing.Bitmap]::FromFile($wideRawPath);$two15=[Drawing.Bitmap]::FromFile($wideRawPath);$alternating=[Drawing.Bitmap]::FromFile($wideRawPath)
  try {
    for($y=16;$y-lt32;++$y){for($x=16;$x-lt32;++$x){
      $c=$p15.GetPixel($x,$y);$delta=if((($x+$y)-band 1)-eq0){1}else{2};$p15.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,$c.R+$delta,$c.G+$delta,$c.B+$delta))
      $c=$p2.GetPixel($x,$y);$p2.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,$c.R+2,$c.G+2,$c.B+2))
      $c=$two15.GetPixel($x,$y);$two15.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,$c.R+$delta,$c.G+$delta,$c.B+$delta))
      $c=$alternating.GetPixel($x,$y);$alternateDelta=if((($x+$y)-band 1)-eq0){-2}else{2};if(($x-eq16-and$y-eq16)-or($x-eq17-and$y-eq16)){$alternateDelta=0};$alternating.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,$c.R+$alternateDelta,$c.G+$alternateDelta,$c.B+$alternateDelta))
    }}
    for($y=16;$y-lt32;++$y){for($x=32;$x-lt48;++$x){$c=$two15.GetPixel($x,$y);$delta=if((($x+$y)-band 1)-eq0){1}else{2};$two15.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,$c.R+$delta,$c.G+$delta,$c.B+$delta))}}
    $p15.Save((Join-Path $scratch 'ghost-tile-positive-1p5.png'));$p2.Save((Join-Path $scratch 'ghost-tile-positive-2.png'));$two15.Save((Join-Path $scratch 'ghost-two-tile-positive-1p5.png'));$alternating.Save((Join-Path $scratch 'ghost-tile-alternating-2.png'))
  } finally {$p15.Dispose();$p2.Dispose();$two15.Dispose();$alternating.Dispose()}
  $tilePositive15=Get-GameplayTemporalGhostEvidence $wideRawPath (Join-Path $scratch 'ghost-tile-positive-1p5.png') $wideObjectPath $wideObjectPath $wideDepthPath $wideDepthPath 0 0 128 64;$tilePositive2=Get-GameplayTemporalGhostEvidence $wideRawPath (Join-Path $scratch 'ghost-tile-positive-2.png') $wideObjectPath $wideObjectPath $wideDepthPath $wideDepthPath 0 0 128 64;$twoTilePositive15=Get-GameplayTemporalGhostEvidence $wideRawPath (Join-Path $scratch 'ghost-two-tile-positive-1p5.png') $wideObjectPath $wideObjectPath $wideDepthPath $wideDepthPath 0 0 128 64;$tileAlternating=Get-GameplayTemporalGhostEvidence $wideRawPath (Join-Path $scratch 'ghost-tile-alternating-2.png') $wideObjectPath $wideObjectPath $wideDepthPath $wideDepthPath 0 0 128 64
  if($tilePositive15.pass-ne0-or$tilePositive15.tiles[9].signedBias-lt1.49-or$tilePositive15.tiles[9].coherence-lt.99){throw 'Uniform +1.5 single-tile bias was not rejected'}
  if($tilePositive2.pass-ne0-or$tilePositive2.tiles[9].signedBias-ne2-or$tilePositive2.tiles[9].signedMedianB-ne2){throw 'Uniform +2 single-tile bias was not rejected'}
  if($twoTilePositive15.pass-ne0-or$twoTilePositive15.tiles[9].pass-ne0-or$twoTilePositive15.tiles[10].pass-ne0){throw 'Uniform +1.5 two-tile bias was not rejected'}
  if($tileAlternating.pass-ne1-or[Math]::Abs($tileAlternating.tiles[9].signedBias)-gt.01-or$tileAlternating.tiles[9].medianBias-ne0){throw "Zero-mean alternating +/-2 legal TAA was rejected: pass=$($tileAlternating.pass) tile=$(($tileAlternating.tiles[9]|ConvertTo-Json -Compress)) stable=$(($tileAlternating.stableLevels|ConvertTo-Json -Compress))"}
  Write-Output ("TemporalGhostTileBias positive15(pass={0},mean={1},bias={2},median={3},variance={4},coherence={5}) positive2(pass={6},bias={7}) alternating(pass={8},bias={9},median={10},variance={11},coherence={12})" -f $tilePositive15.pass,$tilePositive15.tiles[9].meanAbsoluteChannelDifference,$tilePositive15.tiles[9].signedBias,$tilePositive15.tiles[9].medianBias,$tilePositive15.tiles[9].absoluteDifferenceVariance,$tilePositive15.tiles[9].coherence,$tilePositive2.pass,$tilePositive2.tiles[9].signedBias,$tileAlternating.pass,$tileAlternating.tiles[9].signedBias,$tileAlternating.tiles[9].medianBias,$tileAlternating.tiles[9].absoluteDifferenceVariance,$tileAlternating.tiles[9].coherence)
  # 129 是完整 tile gate 的最小樣本；低於此值且 coverage 足夠時不得靜默通過。
  $thinRawPath = (Join-Path $scratch 'ghost-thin-raw.png')
  $thinDepthPath = (Join-Path $scratch 'ghost-thin-depth.png')
  $thinRaw = New-Object Drawing.Bitmap 440,64
  $thinDepth = New-Object Drawing.Bitmap 440,64
  try{for($y=0;$y-lt64;++$y){for($x=0;$x-lt440;++$x){$v=80+(($x+$y)%16);$thinRaw.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,$v,$v+24,$v+48));$thinDepth.SetPixel($x,$y,[Drawing.Color]::Black)}};$thinRaw.Save((Join-Path $scratch 'ghost-thin-raw.png'));$thinDepth.Save((Join-Path $scratch 'ghost-thin-depth.png'))}finally{if($thinRaw){$thinRaw.Dispose()};if($thinDepth){$thinDepth.Dispose()}}
  function New-ThinTileObject([string]$ScratchPath,[int]$StablePixels){$path=Join-Path $ScratchPath "ghost-thin-object-$StablePixels.png";$bitmap=[Drawing.Bitmap]::new(440,64);try{for($y=16;$y-lt32;++$y){for($x=55;$x-lt110;++$x){$bitmap.SetPixel($x,$y,[Drawing.Color]::White)}};if($StablePixels-eq128){foreach($emptyY in @(18,25)){for($y=$emptyY;$y-lt$emptyY+4;++$y){for($x=60;$x-lt94;++$x){$bitmap.SetPixel($x,$y,[Drawing.Color]::Black)}}}}else{$emptyX=if($StablePixels-eq129){60}else{56};$emptyWidth=if($StablePixels-eq129){45}else{53};$emptyHeight=if($StablePixels-eq129){5}else{7};for($y=20;$y-lt20+$emptyHeight;++$y){for($x=$emptyX;$x-lt$emptyX+$emptyWidth;++$x){$bitmap.SetPixel($x,$y,[Drawing.Color]::Black)}}};$bitmap.Save($path)}finally{$bitmap.Dispose()};return $path}
  function New-ThinTileMutation([string]$ScratchPath,[string]$RawPath,[string]$Name,[int]$StablePixels,[double]$Amplitude,[switch]$Alternating){$path=Join-Path $ScratchPath $Name;$bitmap=[Drawing.Bitmap]::FromFile($RawPath);$innerX=if($StablePixels-eq129){61}else{57};$innerWidth=if($StablePixels-eq129){43}else{51};$innerHeight=if($StablePixels-eq129){3}else{5};try{$ordinal=0;for($y=21;$y-lt21+$innerHeight;++$y){for($x=$innerX;$x-lt$innerX+$innerWidth;++$x){$c=$bitmap.GetPixel($x,$y);if($Alternating){$delta=if(($ordinal-band 1)-eq0){-2}else{2};if($ordinal-eq$StablePixels-1){$delta=0}}else{$delta=if($Amplitude-eq1.5){if(($ordinal-band 1)-eq0){1}else{2}}else{[int]$Amplitude}};$bitmap.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,$c.R+$delta,$c.G+$delta,$c.B+$delta));++$ordinal}};$bitmap.Save($path)}finally{$bitmap.Dispose()};return $path}
  $thinObjects=@{128=(New-ThinTileObject $scratch 128);129=(New-ThinTileObject $scratch 129);255=(New-ThinTileObject $scratch 255)};$thinResults=@();foreach($sample in @(129,255)){foreach($amplitude in @(1.5,2,8)){$path=New-ThinTileMutation $scratch (Join-Path $scratch 'ghost-thin-raw.png') "ghost-thin-$sample-$amplitude.png" $sample $amplitude;$metric=Get-GameplayTemporalGhostEvidence (Join-Path $scratch 'ghost-thin-raw.png') $path $thinObjects[$sample] $thinObjects[$sample] (Join-Path $scratch 'ghost-thin-depth.png') (Join-Path $scratch 'ghost-thin-depth.png') 0 0 440 64;$thinResults+=[pscustomobject]@{stable=$sample;amplitude=$amplitude;pass=$metric.pass;tilePass=$metric.tiles[9].pass;gate=$metric.tiles[9].gateApplied;bias=$metric.tiles[9].signedBias;median=$metric.tiles[9].medianBias;coherence=$metric.tiles[9].coherence}}}
  if(@($thinResults|Where-Object{$_.pass-ne0-or$_.tilePass-ne0-or$_.gate-ne1}).Count){throw "129/255 coherent tile mutation was not fail-closed: $(($thinResults|ConvertTo-Json -Compress))"}
  $thinAlternatingPath=New-ThinTileMutation $scratch (Join-Path $scratch 'ghost-thin-raw.png') 'ghost-thin-129-alternating.png' 129 2 -Alternating;$thinAlternating=Get-GameplayTemporalGhostEvidence (Join-Path $scratch 'ghost-thin-raw.png') $thinAlternatingPath $thinObjects[129] $thinObjects[129] (Join-Path $scratch 'ghost-thin-depth.png') (Join-Path $scratch 'ghost-thin-depth.png') 0 0 440 64
  if($thinAlternating.pass-ne1-or$thinAlternating.tiles[9].stablePixels-ne129-or$thinAlternating.tiles[9].gateApplied-ne1-or[Math]::Abs($thinAlternating.tiles[9].signedBias)-gt.001-or$thinAlternating.tiles[9].medianBias-ne0){throw 'Legal 129-pixel zero-mean alternating tile was rejected'}
  $thinFallback=Get-GameplayTemporalGhostEvidence (Join-Path $scratch 'ghost-thin-raw.png') (Join-Path $scratch 'ghost-thin-raw.png') $thinObjects[128] $thinObjects[128] (Join-Path $scratch 'ghost-thin-depth.png') (Join-Path $scratch 'ghost-thin-depth.png') 0 0 440 64
  if($thinFallback.pass-ne0-or$thinFallback.tiles[9].stablePixels-ne128-or$thinFallback.tiles[9].eligiblePixels-lt256-or$thinFallback.tiles[9].gateApplied-ne0-or$thinFallback.tiles[9].coverageFallbackFailed-ne1-or$thinFallback.tiles[9].pass-ne0){throw 'Sub-129 stable tile with sufficient coverage did not fail closed'}
  Write-Output ("TemporalGhostMinimumTile results={0} alternating(pass={1},stable={2},bias={3},median={4},variance={5}) fallback(pass={6},stable={7},eligible={8},fallback={9})" -f ($thinResults|ConvertTo-Json -Compress),$thinAlternating.pass,$thinAlternating.tiles[9].stablePixels,$thinAlternating.tiles[9].signedBias,$thinAlternating.tiles[9].medianBias,$thinAlternating.tiles[9].absoluteDifferenceVariance,$thinFallback.pass,$thinFallback.tiles[9].stablePixels,$thinFallback.tiles[9].eligiblePixels,$thinFallback.tiles[9].coverageFallbackFailed)
  $cornerGhost=Get-GameplayTemporalGhostEvidence $wideRawPath $wideEffectivePath $wideObjectPath $wideObjectPath $wideDepthPath $wideDepthPath 0 0 192 128
  if($cornerGhost.pass-ne0-or$cornerGhost.stableLevels[2].pixels-ne256-or$cornerGhost.stableLevels[2].maximumConnectedPixels-ne256-or$cornerGhost.tiles[31].pass-ne0){throw 'Ghost outside the former best ROI/right-bottom tile was not rejected'}
  $ringRawObject=Join-Path $scratch 'ghost-ring-raw-object.png';$ringEffectiveObject=Join-Path $scratch 'ghost-ring-effective-object.png';$ringBeauty=Join-Path $scratch 'ghost-ring-beauty.png';$ro=[Drawing.Bitmap]::FromFile($rawObjectPath);$eo=[Drawing.Bitmap]::FromFile($effectiveObjectPath);$rb=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{for($y=20;$y-lt32;++$y){for($x=26;$x-lt38;++$x){$ro.SetPixel($x,$y,[Drawing.Color]::White);$eo.SetPixel($x,$y,[Drawing.Color]::White)}};for($y=19;$y-le32;++$y){foreach($x in @(25,38)){$c=$rb.GetPixel($x,$y);$rb.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,[Math]::Min(255,$c.R+40),[Math]::Min(255,$c.G+40),[Math]::Min(255,$c.B+40)))}};for($x=26;$x-lt38;++$x){foreach($y in @(19,32)){$c=$rb.GetPixel($x,$y);$rb.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,[Math]::Min(255,$c.R+40),[Math]::Min(255,$c.G+40),[Math]::Min(255,$c.B+40)))}};$ro.Save($ringRawObject);$eo.Save($ringEffectiveObject);$rb.Save($ringBeauty)}finally{$ro.Dispose();$eo.Dispose();$rb.Dispose()}
  $ringGhost=Get-GameplayTemporalGhostEvidence $rawBeautyPath $ringBeauty $ringRawObject $ringEffectiveObject $rawDepthPath $effectiveDepthPath 16 8 32 32
  if($ringGhost.pass-ne0-or$ringGhost.ringMaximumChannelDifference-le8-or$ringGhost.objectAdjacentRingPixels-lt1){throw 'Object-adjacent ring trail was excluded instead of rejected'}
  $movingRawObject=Join-Path $scratch 'ghost-moving-raw-object.png';$movingEffectiveObject=Join-Path $scratch 'ghost-moving-effective-object.png';$movingRawBeauty=Join-Path $scratch 'ghost-moving-raw-beauty.png';$movingEffectiveBeauty=Join-Path $scratch 'ghost-moving-effective-beauty.png';$ro=[Drawing.Bitmap]::FromFile($rawObjectPath);$eo=[Drawing.Bitmap]::FromFile($effectiveObjectPath);$rb=[Drawing.Bitmap]::FromFile($rawBeautyPath);$eb=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{for($y=20;$y-lt32;++$y){for($x=24;$x-lt34;++$x){$ro.SetPixel($x,$y,[Drawing.Color]::White);$rb.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,190,120,70));$eo.SetPixel($x+1,$y,[Drawing.Color]::White);$eb.SetPixel($x+1,$y,[Drawing.Color]::FromArgb(255,190,120,70))}};$ro.Save($movingRawObject);$eo.Save($movingEffectiveObject);$rb.Save($movingRawBeauty);$eb.Save($movingEffectiveBeauty)}finally{$ro.Dispose();$eo.Dispose();$rb.Dispose();$eb.Dispose()}
  $legalObjectMotion=Get-GameplayTemporalGhostEvidence $movingRawBeauty $movingEffectiveBeauty $movingRawObject $movingEffectiveObject $rawDepthPath $effectiveDepthPath 16 8 32 32
  if($legalObjectMotion.pass-ne1-or$legalObjectMotion.objectAdjacentRingPixels-lt1-or$legalObjectMotion.ringMaximumChannelDifference-gt8){throw 'Legal one-pixel moving object was rejected by the object-adjacent ring gate'}
  $originEmpty=Get-GameplayTemporalGhostEvidence $rawBeautyPath $effectiveBeautyPath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 0 0 64 64
  if($originEmpty.pass-ne1-or$originEmpty.eligiblePixels-ne4096-or$originEmpty.coveredPixels-ne4096-or$originEmpty.excludedPixels-ne0-or$originEmpty.stableEmptyPixels-ne4096-or$originEmpty.objectAdjacentRingPixels-ne0){throw 'Origin-aligned full empty coverage was incorrect'}
  $cornerOneObject=Join-Path $scratch 'ghost-corner-one-object.png';$o=[Drawing.Bitmap]::FromFile($rawObjectPath);try{$o.SetPixel(0,0,[Drawing.Color]::White);$o.Save($cornerOneObject)}finally{$o.Dispose()}
  $cornerOne=Get-GameplayTemporalGhostEvidence $rawBeautyPath $effectiveBeautyPath $cornerOneObject $cornerOneObject $rawDepthPath $effectiveDepthPath 0 0 64 64
  if($cornerOne.pass-ne1-or$cornerOne.eligiblePixels-ne4095-or$cornerOne.coveredPixels-ne4095-or$cornerOne.excludedPixels-ne1-or$cornerOne.stableEmptyPixels-ne4092-or$cornerOne.objectAdjacentRingPixels-ne3){throw 'Origin corner 1x1 object coverage/ring was incorrect'}
  $cornerTwoObject=Join-Path $scratch 'ghost-corner-two-object.png';$o=[Drawing.Bitmap]::FromFile($rawObjectPath);try{for($y=0;$y-lt2;++$y){for($x=0;$x-lt2;++$x){$o.SetPixel($x,$y,[Drawing.Color]::White)}};$o.Save($cornerTwoObject)}finally{$o.Dispose()}
  $cornerTwo=Get-GameplayTemporalGhostEvidence $rawBeautyPath $effectiveBeautyPath $cornerTwoObject $cornerTwoObject $rawDepthPath $effectiveDepthPath 0 0 64 64
  if($cornerTwo.pass-ne1-or$cornerTwo.eligiblePixels-ne4092-or$cornerTwo.coveredPixels-ne4092-or$cornerTwo.excludedPixels-ne4-or$cornerTwo.stableEmptyPixels-ne4087-or$cornerTwo.objectAdjacentRingPixels-ne5){throw 'Origin corner 2x2 object coverage/ring was incorrect'}
  $cornerTrailPath=Join-Path $scratch 'ghost-corner-ring-trail.png';$b=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{foreach($point in @(@(1,0),@(0,1),@(1,1))){$c=$b.GetPixel($point[0],$point[1]);$b.SetPixel($point[0],$point[1],[Drawing.Color]::FromArgb(255,[Math]::Min(255,$c.R+40),[Math]::Min(255,$c.G+40),[Math]::Min(255,$c.B+40)))};$b.Save($cornerTrailPath)}finally{$b.Dispose()}
  $cornerTrail=Get-GameplayTemporalGhostEvidence $rawBeautyPath $cornerTrailPath $cornerOneObject $cornerOneObject $rawDepthPath $effectiveDepthPath 0 0 64 64
  if($cornerTrail.pass-ne0-or$cornerTrail.objectAdjacentRingPixels-ne3-or$cornerTrail.ringLevels[2].pixels-lt1){throw 'Origin corner ring trail was not rejected'}
  $cornerMoveRawObject=Join-Path $scratch 'ghost-corner-move-raw-object.png';$cornerMoveEffectiveObject=Join-Path $scratch 'ghost-corner-move-effective-object.png';$cornerMoveRawBeauty=Join-Path $scratch 'ghost-corner-move-raw-beauty.png';$cornerMoveEffectiveBeauty=Join-Path $scratch 'ghost-corner-move-effective-beauty.png';$ro=[Drawing.Bitmap]::FromFile($rawObjectPath);$eo=[Drawing.Bitmap]::FromFile($effectiveObjectPath);$rb=[Drawing.Bitmap]::FromFile($rawBeautyPath);$eb=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{$ro.SetPixel(0,0,[Drawing.Color]::White);$eo.SetPixel(1,0,[Drawing.Color]::White);$rb.SetPixel(0,0,[Drawing.Color]::FromArgb(255,190,120,70));$eb.SetPixel(1,0,[Drawing.Color]::FromArgb(255,190,120,70));$ro.Save($cornerMoveRawObject);$eo.Save($cornerMoveEffectiveObject);$rb.Save($cornerMoveRawBeauty);$eb.Save($cornerMoveEffectiveBeauty)}finally{$ro.Dispose();$eo.Dispose();$rb.Dispose();$eb.Dispose()}
  $cornerMove=Get-GameplayTemporalGhostEvidence $cornerMoveRawBeauty $cornerMoveEffectiveBeauty $cornerMoveRawObject $cornerMoveEffectiveObject $rawDepthPath $effectiveDepthPath 0 0 64 64
  if($cornerMove.pass-ne1-or$cornerMove.excludedPixels-ne2-or$cornerMove.coveredPixels-ne$cornerMove.eligiblePixels-or$cornerMove.objectAdjacentRingPixels-lt1){throw 'Origin corner legal 1x1 object motion was rejected or miscounted'}
  $foregroundObject=Join-Path $scratch 'ghost-object-foreground.png';$foregroundDepth=Join-Path $scratch 'ghost-depth-foreground.png';$o=[Drawing.Bitmap]::FromFile($effectiveObjectPath);$d=[Drawing.Bitmap]::FromFile($effectiveDepthPath);try{for($y=18;$y-lt30;++$y){for($x=26;$x-lt38;++$x){$o.SetPixel($x,$y,[Drawing.Color]::White);$d.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,2,2,2))}};$o.Save($foregroundObject);$d.Save($foregroundDepth)}finally{$o.Dispose();$d.Dispose()}
  $motionMasked=Get-GameplayTemporalGhostEvidence $rawBeautyPath $legacyPath $rawObjectPath $foregroundObject $rawDepthPath $foregroundDepth 16 8 32 32
  if($motionMasked.excludedPixels-lt144-or$motionMasked.objectAdjacentRingPixels-lt1-or$motionMasked.coveredPixels-ne$motionMasked.eligiblePixels){throw 'Object-ID motion boundary ring was not covered by temporal comparison'}
  $noEmptyObject=Join-Path $scratch 'ghost-no-empty-object.png';$o=[Drawing.Bitmap]::FromFile($rawObjectPath);try{for($y=0;$y-lt64;++$y){for($x=0;$x-lt64;++$x){$o.SetPixel($x,$y,[Drawing.Color]::White)}};$o.Save($noEmptyObject)}finally{$o.Dispose()}
  Expect-Failure {Get-GameplayTemporalGhostEvidence $rawBeautyPath $effectiveBeautyPath $noEmptyObject $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32|Out-Null} 'temporal semantic-empty ROI completely unavailable'
  $wrongDepth=Join-Path $scratch 'ghost-depth-wrong-provenance.png';$d=[Drawing.Bitmap]::new(63,64);try{$d.Save($wrongDepth)}finally{$d.Dispose()}
  Expect-Failure {Get-GameplayTemporalGhostEvidence $rawBeautyPath $effectiveBeautyPath $rawObjectPath $effectiveObjectPath $wrongDepth $effectiveDepthPath 16 8 32 32|Out-Null} 'temporal depth raster dimensions mismatch'
  $expectedSha=@{};foreach($entry in $cleanGhost.artifactSha256.PSObject.Properties){$expectedSha[$entry.Name]=[string]$entry.Value};$expectedSha.effectiveBeauty='0'*64
  Expect-Failure {Get-GameplayTemporalGhostEvidence $rawBeautyPath $effectiveBeautyPath $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32 $expectedSha|Out-Null} 'temporal artifact SHA mismatch'
  $ghostDepthPath=$rawDepthPath
  $depthSha=Get-GameplayEvidenceSha256Hex $ghostDepthPath;$depthBytes=(Get-Item -LiteralPath $ghostDepthPath).Length;$pixelSha='a'*64;$phase0=[pscustomobject]@{index=0;frameSerial=41;viewProjectionHash=101;cameraHash=102;invariantHash=103;jitterIndex=0;jitterX=.05;jitterY=-.16;historyGeneration=7;historyIndex=1;tokenGeneration=8};$phase1=[pscustomobject]@{index=1;frameSerial=42;viewProjectionHash=101;cameraHash=102;invariantHash=103;jitterIndex=0;jitterX=.05;jitterY=-.16;historyGeneration=7;historyIndex=1;tokenGeneration=9};$typedCharacter=[pscustomobject]@{name='third-day.png';depthCaptureSupported=1;depthGatePass=1;depthGeneration=2;rawDepth='ghost-raw-depth.png';effectiveDepth='ghost-raw-depth.png';rawDepthSha256=$pixelSha;effectiveDepthSha256=$pixelSha;rawDepthPixelSha256=$pixelSha;effectiveDepthPixelSha256=$pixelSha;rawDepthFinalFileSha256=$depthSha;effectiveDepthFinalFileSha256=$depthSha;rawDepthFinalFileBytes=$depthBytes;effectiveDepthFinalFileBytes=$depthBytes};$typedPhotoreal=[pscustomobject]@{name='third-day.png';effectiveFrameSerial=42;semanticFrameSerial=42};$typedSequence=[pscustomobject]@{name='third-day.png';complete=1;phases=@($phase0,$phase1)}
  if(-not(Assert-GameplayTemporalGhostDepthProvenance $ghostDepthPath 'third-day.png' 1 $typedCharacter $typedPhotoreal $typedSequence)){throw 'Typed temporal depth provenance positive failed'}
  $badCharacter=$typedCharacter.PSObject.Copy();$badCharacter.effectiveDepthFinalFileSha256='0'*64;Expect-Failure {Assert-GameplayTemporalGhostDepthProvenance $ghostDepthPath 'third-day.png' 1 $badCharacter $typedPhotoreal $typedSequence|Out-Null} 'typed temporal depth final-file SHA mismatch'
  $badCharacter=$typedCharacter.PSObject.Copy();$badCharacter.effectiveDepthPixelSha256='b'*64;Expect-Failure {Assert-GameplayTemporalGhostDepthProvenance $ghostDepthPath 'third-day.png' 1 $badCharacter $typedPhotoreal $typedSequence|Out-Null} 'typed temporal depth pixel SHA mismatch'
  $badCharacter=$typedCharacter.PSObject.Copy();$badCharacter.effectiveDepthFinalFileBytes=[uint64]$depthBytes+1;Expect-Failure {Assert-GameplayTemporalGhostDepthProvenance $ghostDepthPath 'third-day.png' 1 $badCharacter $typedPhotoreal $typedSequence|Out-Null} 'typed temporal depth final length mismatch'
  $badPhase1=$phase1.PSObject.Copy();$badPhase1.viewProjectionHash=999;$badSequence=[pscustomobject]@{name='third-day.png';complete=1;phases=@($phase0,$badPhase1)};Expect-Failure {Assert-GameplayTemporalGhostDepthProvenance $ghostDepthPath 'third-day.png' 1 $typedCharacter $typedPhotoreal $badSequence|Out-Null} 'typed temporal depth VP mismatch'
  $badPhotoreal=$typedPhotoreal.PSObject.Copy();$badPhotoreal.semanticFrameSerial=43;Expect-Failure {Assert-GameplayTemporalGhostDepthProvenance $ghostDepthPath 'third-day.png' 1 $typedCharacter $badPhotoreal $typedSequence|Out-Null} 'typed temporal depth frame mismatch'
  $pairName='ghost-effective-beauty.png';$pairCharacter=[pscustomobject]@{name=$pairName;rawColor='ghost-raw-beauty.png';rawObjectIdMask='ghost-raw-object.png';objectIdMask='ghost-effective-object.png';rawDepth='ghost-raw-depth.png';effectiveDepth='ghost-effective-depth.png';rawDepthFinalFileSha256=$cleanGhost.artifactSha256.rawDepth;effectiveDepthFinalFileSha256=$cleanGhost.artifactSha256.effectiveDepth};$pairPhotoreal=[pscustomobject]@{name=$pairName;effectiveFrameSerial=42;semanticFrameSerial=42};$pairSequence=[pscustomobject]@{name=$pairName;complete=1;phases=@($phase0,$phase1)}
  $pairArtifacts=@();foreach($key in @('rawBeauty','effectiveBeauty','rawObjectId','effectiveObjectId','rawDepth','effectiveDepth')){$pairArtifacts+=[pscustomobject]@{key=$key;name=$cleanGhost.artifactName.$key;bytes=1;sha256=$cleanGhost.artifactSha256.$key}};$pairArtifactCapture=[pscustomobject]@{name=$pairName;rawFrameSerial=41;effectiveFrameSerial=42;rawTokenGeneration=8;effectiveTokenGeneration=9;artifacts=$pairArtifacts}
  if(-not(Assert-GameplayTemporalGhostPairProvenance $pairName $pairCharacter $pairPhotoreal $pairSequence $cleanGhost $pairArtifactCapture)){throw 'Temporal ghost pair provenance positive failed'}
  $reusedPhase=$phase1.PSObject.Copy();$reusedPhase.frameSerial=$phase0.frameSerial;$reusedSequence=[pscustomobject]@{name=$pairName;complete=1;phases=@($phase0,$reusedPhase)};Expect-Failure {Assert-GameplayTemporalGhostPairProvenance $pairName $pairCharacter $pairPhotoreal $reusedSequence $cleanGhost $pairArtifactCapture|Out-Null} 'reused temporal frame id'
  $wrongNameEvidence=$cleanGhost.PSObject.Copy();$wrongNameEvidence.artifactName=$cleanGhost.artifactName.PSObject.Copy();$wrongNameEvidence.artifactName.effectiveBeauty='reused.png';Expect-Failure {Assert-GameplayTemporalGhostPairProvenance $pairName $pairCharacter $pairPhotoreal $pairSequence $wrongNameEvidence $pairArtifactCapture|Out-Null} 'mismatched temporal artifact name'
  $emptyShaEvidence=$cleanGhost.PSObject.Copy();$emptyShaEvidence.artifactSha256=$cleanGhost.artifactSha256.PSObject.Copy();$emptyShaEvidence.artifactSha256.rawBeauty='';Expect-Failure {Assert-GameplayTemporalGhostPairProvenance $pairName $pairCharacter $pairPhotoreal $pairSequence $emptyShaEvidence $pairArtifactCapture|Out-Null} 'empty temporal artifact SHA'
  $movingPairName='ghost-moving-cloud.png';$movingPairCharacter=$pairCharacter.PSObject.Copy();$movingPairCharacter.name=$movingPairName;$movingPairPhotoreal=$pairPhotoreal.PSObject.Copy();$movingPairPhotoreal.name=$movingPairName;$movingPairSequence=$pairSequence.PSObject.Copy();$movingPairSequence.name=$movingPairName;$movingArtifacts=@();foreach($key in @('rawBeauty','effectiveBeauty','rawObjectId','effectiveObjectId','rawDepth','effectiveDepth')){$movingArtifacts+=[pscustomobject]@{key=$key;name=$movingGhost.artifactName.$key;bytes=1;sha256=$movingGhost.artifactSha256.$key}};$reusedShaCapture=[pscustomobject]@{name=$movingPairName;rawFrameSerial=41;effectiveFrameSerial=42;rawTokenGeneration=8;effectiveTokenGeneration=9;artifacts=$movingArtifacts};$reusedShaCapture.artifacts[1].sha256=$reusedShaCapture.artifacts[0].sha256;Expect-Failure {Assert-GameplayTemporalGhostPairProvenance $movingPairName $movingPairCharacter $movingPairPhotoreal $movingPairSequence $movingGhost $reusedShaCapture|Out-Null} 'reused receipt-owned SHA'
  $swappedCapture=$pairArtifactCapture.PSObject.Copy();$swappedCapture.artifacts=@($pairArtifacts|ForEach-Object{$_.PSObject.Copy()});$swap=$swappedCapture.artifacts[0].name;$swappedCapture.artifacts[0].name=$swappedCapture.artifacts[1].name;$swappedCapture.artifacts[1].name=$swap;Expect-Failure {Assert-GameplayTemporalGhostPairProvenance $pairName $pairCharacter $pairPhotoreal $pairSequence $cleanGhost $swappedCapture|Out-Null} 'swapped receipt-owned artifact'
  $reusedTokenCapture=$pairArtifactCapture.PSObject.Copy();$reusedTokenCapture.effectiveTokenGeneration=$reusedTokenCapture.rawTokenGeneration;Expect-Failure {Assert-GameplayTemporalGhostPairProvenance $pairName $pairCharacter $pairPhotoreal $pairSequence $cleanGhost $reusedTokenCapture|Out-Null} 'reused receipt-owned token generation'
  $sweep=@();foreach($amplitude in @(9,16,24,40)){foreach($area in @(1,4,16,64)){$side=[int][Math]::Sqrt($area);$path=Join-Path $scratch "ghost-sweep-$amplitude-$area.png";$bitmap=[Drawing.Bitmap]::FromFile($effectiveBeautyPath);try{for($y=20;$y-lt20+$side;++$y){for($x=20;$x-lt20+$side;++$x){$c=$bitmap.GetPixel($x,$y);$bitmap.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,[Math]::Min(255,$c.R+$amplitude),[Math]::Min(255,$c.G+$amplitude),[Math]::Min(255,$c.B+$amplitude)))}};$bitmap.Save($path)}finally{$bitmap.Dispose()};$metric=Get-GameplayTemporalGhostEvidence $rawBeautyPath $path $rawObjectPath $effectiveObjectPath $rawDepthPath $effectiveDepthPath 16 8 32 32;$sweep+=[pscustomobject]@{amplitude=$amplitude;area=$area;pass=$metric.pass;mean=$metric.meanAbsoluteChannelDifference;gt8=$metric.stableLevels[0].pixels;gt12=$metric.stableLevels[1].pixels;gt16=$metric.stableLevels[2].pixels;component8=$metric.stableLevels[0].maximumConnectedPixels;component16=$metric.stableLevels[2].maximumConnectedPixels}}}
  if(@($sweep|Where-Object{$_.amplitude-eq9-and$_.area-eq1-and$_.pass-ne1}).Count-or@($sweep|Where-Object{$_.area-ge16-and$_.amplitude-ge16-and$_.pass-ne0}).Count-or@($sweep|Where-Object{$_.area-ge4-and$_.amplitude-ge24-and$_.pass-ne0}).Count){throw 'Temporal ghost amplitude/area mutation sweep contract failed'}
  Write-Output ("TemporalGhostSweep="+($sweep|ConvertTo-Json -Compress))
  Write-Output ("TemporalGhostFixture static(mean={0},p99={1},max={2}) moving(mean={3},p99={4},max={5}) legacy(mean={6},p99={7},max={8},gt16={9},component16={10}) offset(mean={11},p99={12},max={13},gt16={14},component16={15}) drift(mean={16},max={17}) isolated9(pass={18},gt8={19})" -f $cleanGhost.meanAbsoluteChannelDifference,$cleanGhost.p99MaximumChannelDifference,$cleanGhost.maximumChannelDifference,$movingGhost.meanAbsoluteChannelDifference,$movingGhost.p99MaximumChannelDifference,$movingGhost.maximumChannelDifference,$legacyGhost.meanAbsoluteChannelDifference,$legacyGhost.p99MaximumChannelDifference,$legacyGhost.maximumChannelDifference,$legacyGhost.stableLevels[2].pixels,$legacyGhost.stableLevels[2].maximumConnectedPixels,$offsetGhost.meanAbsoluteChannelDifference,$offsetGhost.p99MaximumChannelDifference,$offsetGhost.maximumChannelDifference,$offsetGhost.stableLevels[2].pixels,$offsetGhost.stableLevels[2].maximumConnectedPixels,$driftGhost.meanAbsoluteChannelDifference,$driftGhost.maximumChannelDifference,$spikeGhost.pass,$spikeGhost.stableLevels[0].pixels)
  $receiptDepthPath=Join-Path $scratch 'depth-receipt.png';Copy-Item -LiteralPath $ghostDepthPath -Destination $receiptDepthPath;$receiptDepthSha=Get-GameplayEvidenceSha256Hex $receiptDepthPath;$receiptDepthBytes=(Get-Item -LiteralPath $receiptDepthPath).Length;$depthFileCapture=[pscustomobject]@{name='third-day.png';camera='third';rawDepth='depth-receipt.png';effectiveDepth='depth-receipt.png';referenceDepth='depth-receipt.png';rawDepthSha256=$pixelSha;effectiveDepthSha256=$pixelSha;referenceDepthSha256=$pixelSha;rawDepthPixelSha256=$pixelSha;effectiveDepthPixelSha256=$pixelSha;referenceDepthPixelSha256=$pixelSha;rawDepthFinalFileSha256=$receiptDepthSha;effectiveDepthFinalFileSha256=$receiptDepthSha;referenceDepthFinalFileSha256=$receiptDepthSha;rawDepthFinalFileBytes=$receiptDepthBytes;effectiveDepthFinalFileBytes=$receiptDepthBytes;referenceDepthFinalFileBytes=$receiptDepthBytes};$depthFileReceipt=[pscustomobject]@{captures=@($depthFileCapture)}
  if(-not(Assert-GameplayDepthFileReceipts $scratch $depthFileReceipt)){throw 'Depth final-file receipt positive failed'}
  [IO.File]::AppendAllText($receiptDepthPath,'tamper');Expect-Failure {Assert-GameplayDepthFileReceipts $scratch $depthFileReceipt|Out-Null} 'depth final-file tamper/replacement'
  }
  if ($Section -ne 'Temporal') {
  $packageRoot = Join-Path $scratch 'photoreal-cooked'
  New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
  $files = @()
  $payloadBytes = [int64]0
  function Add-Payload([string]$Relative, [string]$Content) {
    $path = Join-Path $packageRoot $Relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
    [IO.File]::WriteAllText($path, $Content, (New-Object Text.UTF8Encoding($false)))
    $item = Get-Item -LiteralPath $path
    $script:payloadBytes += $item.Length
    $script:files += [ordered]@{ path = $Relative.Replace('\', '/'); bytes = $item.Length; sha256 = Get-GameplayEvidenceSha256Hex $path }
  }

  $materials = @()
  for ($material = 0; $material -lt 11; ++$material) {
    $paths = @()
    foreach ($channel in @('base', 'surface', 'normal')) {
      $relative = "materials/m$material/$channel.bin"
      Add-Payload $relative "material-$material-$channel"
      $paths += $relative
    }
    $materials += [ordered]@{ id = "m$material"; baseColorPath = $paths[0]; surfacePath = $paths[1]; normalPath = $paths[2] }
  }
  $meshParts = @()
  for ($part = 0; $part -lt 6; ++$part) {
    $lods = @()
    for ($lod = 0; $lod -lt 3; ++$lod) {
      $relative = "meshes/p$part/lod$lod.bin"
      Add-Payload $relative "mesh-$part-$lod"
      $lods += $relative
    }
    $meshParts += [ordered]@{ id = "p$part"; lodPaths = $lods }
  }
  $iblPaths = @()
  foreach ($name in @('specular', 'irradiance', 'brdf')) {
    $relative = "ibl/$name.bin"
    Add-Payload $relative "ibl-$name"
    $iblPaths += $relative
  }
  if ($files.Count -ne 54) { throw 'Synthetic payload fixture count failed' }
  $assetIds = @(0..9 | ForEach-Object { "asset-$_" })
  $manifest = [ordered]@{
    schemaVersion = 1
    manifestType = 'aevrynd-runtime-render-package'
    exactAssetSet = [ordered]@{ id = 'synthetic'; count = 10; assetIds = $assetIds }
    files = $files
    materials = $materials
    meshParts = $meshParts
    ibl = [ordered]@{ specularPath = $iblPaths[0]; irradiancePath = $iblPaths[1]; brdfLutPath = $iblPaths[2] }
    assets = @(0..9 | ForEach-Object { [ordered]@{ id = "asset-$_" } })
    package = [ordered]@{ payloadFileCount = 54; payloadBytes = $payloadBytes; manifestPath = 'runtime-render-manifest.json'; manifestBytes = 0; packageFileCount = 55; packageBytes = $payloadBytes; manifestHashPolicy = 'external-envelope' }
  }
  $manifestPath = Join-Path $packageRoot 'runtime-render-manifest.json'
  for ($iteration = 0; $iteration -lt 10; ++$iteration) {
    $json = $manifest | ConvertTo-Json -Depth 10
    $bytes = (New-Object Text.UTF8Encoding($false)).GetBytes($json)
    if ($manifest.package.manifestBytes -eq $bytes.LongLength -and $manifest.package.packageBytes -eq $payloadBytes + $bytes.LongLength) { break }
    $manifest.package.manifestBytes = $bytes.LongLength
    $manifest.package.packageBytes = $payloadBytes + $bytes.LongLength
  }
  $json = $manifest | ConvertTo-Json -Depth 10
  [IO.File]::WriteAllText($manifestPath, $json, (New-Object Text.UTF8Encoding($false)))
  $package = Get-GameplayPhotorealPackageEvidence $manifestPath
  if ($package.packageFiles -ne 55 -or $package.payloadFiles -ne 54) { throw 'Synthetic package positive failed' }

  $gitSealRoot=Join-Path $scratch 'git-seal';New-Item -ItemType Directory -Path $gitSealRoot|Out-Null
  & git -C $gitSealRoot init -q;if($LASTEXITCODE-ne0){throw 'Synthetic Git seal init failed'}
  & git -C $gitSealRoot config user.email evidence@example.invalid;& git -C $gitSealRoot config user.name EvidenceFixture
  [IO.File]::WriteAllText((Join-Path $gitSealRoot 'tracked.txt'),'base');& git -C $gitSealRoot add tracked.txt;& git -C $gitSealRoot commit -q -m fixture
  if($LASTEXITCODE-ne0){throw 'Synthetic Git seal commit failed'}
  $cleanGitSeal=Get-GameplayGitSealEvidence $gitSealRoot
  if(@($cleanGitSeal.statusLines).Count-ne0-or$cleanGitSeal.statusText-ne''-or$cleanGitSeal.statusSha256-ne(Get-GameplayEvidenceTextSha256Hex '')){throw 'Clean Git seal empty status was unstable'}
  [IO.File]::WriteAllText((Join-Path $gitSealRoot 'tracked.txt'),'tracked-dirty-content');$trackedGitSeal=Get-GameplayGitSealEvidence $gitSealRoot;$trackedStatusLines=@($trackedGitSeal.statusLines)
  if($trackedStatusLines.Count-ne1-or$trackedStatusLines[0]-notmatch'^ M tracked\.txt$'-or$trackedGitSeal.statusSha256-eq$cleanGitSeal.statusSha256){throw "Tracked dirty Git seal was not recorded: count=$($trackedStatusLines.Count) lines=$($trackedStatusLines -join '|') clean=$($cleanGitSeal.statusSha256) dirty=$($trackedGitSeal.statusSha256)"}
  $untrackedTree=Join-Path $gitSealRoot 'untracked';New-Item -ItemType Directory -Path $untrackedTree|Out-Null;[IO.File]::WriteAllText((Join-Path $untrackedTree 'payload.bin'),'payload')
  $untrackedGitSeal=Get-GameplayGitSealEvidence $gitSealRoot;$untrackedStatusLines=@($untrackedGitSeal.statusLines)
  if($untrackedStatusLines.Count-ne2-or@($untrackedStatusLines|Where-Object{$_-eq'?? untracked/'}).Count-ne1-or$untrackedGitSeal.statusSha256-eq$trackedGitSeal.statusSha256){throw 'Untracked tree Git seal was not recorded'}
  $notGitRoot=Join-Path $scratch 'not-a-git-repository';New-Item -ItemType Directory -Path $notGitRoot|Out-Null
  Expect-Failure {Get-GameplayGitSealEvidence $notGitRoot|Out-Null} 'Git seal native command failure'

  $clearOcclusion=[pscustomobject]@{rawOccluderCoverage=.01;policyFadeOpacity=1.0;fadedStableLow=0;rawAcceptedIncludingOpacity=11;effectiveAcceptedIncludingOpacity=11}
  $clearPath=Get-GameplayThirdPersonOcclusionPathEvidence $clearOcclusion
  if($clearPath.rawPathOccluded-or$clearPath.clearPathOpacityInvalid){throw 'Synthetic clear occlusion-path classification failed'}
  $blockedOcclusion=[pscustomobject]@{rawOccluderCoverage=.12;policyFadeOpacity=.35;fadedStableLow=154;rawAcceptedIncludingOpacity=11;effectiveAcceptedIncludingOpacity=12}
  $blockedPath=Get-GameplayThirdPersonOcclusionPathEvidence $blockedOcclusion
  if(-not$blockedPath.rawPathOccluded-or-not$blockedPath.clearPathOpacityInvalid){throw 'Synthetic blocked occlusion-path classification failed'}
  $missingOcclusion=[pscustomobject]@{policyFadeOpacity=1.0;fadedStableLow=0;rawAcceptedIncludingOpacity=11;effectiveAcceptedIncludingOpacity=11}
  Expect-Failure { Get-GameplayThirdPersonOcclusionPathEvidence $missingOcclusion | Out-Null } 'missing raw occlusion path evidence'
  $emptyOcclusion=[pscustomobject]@{rawOccluderCoverage='';policyFadeOpacity=1.0;fadedStableLow=0;rawAcceptedIncludingOpacity=11;effectiveAcceptedIncludingOpacity=11}
  Expect-Failure { Get-GameplayThirdPersonOcclusionPathEvidence $emptyOcclusion | Out-Null } 'empty raw occlusion path evidence'

  $names = @('first-day.png','third-day.png','god-day.png','first-dusk.png','third-dusk.png','god-dusk.png','first-night.png','third-night.png','god-night.png','palette-building.png','melee-combat.png')
  $runId = '01234567-89ab-cdef-8123-456789abcdef'
  $depthHash = 'a' * 64
  $femaleArms=[pscustomobject]@{firstPersonArmSubmeshCount=4;firstPersonArmTriangleCount=5726;firstPersonLeftArmTriangleCount=2862;firstPersonRightArmTriangleCount=2864}
  $maleArms=[pscustomobject]@{firstPersonArmSubmeshCount=4;firstPersonArmTriangleCount=5979;firstPersonLeftArmTriangleCount=2990;firstPersonRightArmTriangleCount=2989}
  $characterCaptures=@();$viewExpectations=@(Get-GameplayCaptureViewExpectations)
  for($captureIndex=0;$captureIndex-lt$names.Count;++$captureIndex){$female=$captureIndex-lt5;$first=$viewExpectations[$captureIndex].camera-eq'first';$third=$viewExpectations[$captureIndex].camera-eq'third';$arms=if($female){$femaleArms}else{$maleArms};$characterCaptures += [pscustomobject]@{ name=$names[$captureIndex];character=if($female){'female-f2'}else{'male-m2'};camera=$viewExpectations[$captureIndex].camera;topCenterRayElevation=if($third){.05}else{0};minimumTopCenterRayElevation=if($third){.03}else{0};rejectedHorizon=if($third){100}else{0};bodyMaterialSlots=if($first){$arms.firstPersonArmSubmeshCount}else{9};weaponMaterialRanges=2;weaponAnchorError=.0005;firstPersonArmSubmeshCount=if($first){$arms.firstPersonArmSubmeshCount}else{0};firstPersonArmTriangleCount=if($first){$arms.firstPersonArmTriangleCount}else{0};firstPersonLeftArmTriangleCount=if($first){$arms.firstPersonLeftArmTriangleCount}else{0};firstPersonRightArmTriangleCount=if($first){$arms.firstPersonRightArmTriangleCount}else{0};objectIdOpaqueComplete=1;objectIdMaskedComplete=1;objectIdSkinnedComplete=1;depthCaptureSupported=1;depthGatePass=1;depthGeneration=1;rawDepthSha256=$depthHash;effectiveDepthSha256=$depthHash;referenceDepthSha256=$depthHash;feetContactPassed=1;groundSemantic='terrain';characterReferenceSupported=1;gpuFeetContactPassed=1;contactArtifactSha256=('b'*64);contactLeftFootPixels=4;contactRightFootPixels=4;contactGroundSupportPixels=8;contactShadowPixels=8;contactFrameSerial=2;contactPoseHash=4;rawFemalePoseHash=1;effectiveFemalePoseHash=1;referenceFemalePoseHash=1;rawMalePoseHash=2;effectiveMalePoseHash=2;referenceMalePoseHash=2;rawMonsterPoseHash=3;effectiveMonsterPoseHash=3;referenceMonsterPoseHash=3;rawFootPlacementHash=4;effectiveFootPlacementHash=4;referenceFootPlacementHash=4;requestDrawRevision=1;requestCameraHash=1;requestStateHash=1;rawAcceptedExcludingOpacity=1;effectiveAcceptedExcludingOpacity=1;referenceAcceptedExcludingOpacity=1;rawIdentityOrderHash=1;effectiveIdentityOrderHash=1;referenceIdentityOrderHash=1;referenceOpacityMatches=6;rawBuildingCoverage151To156=@(0,0,0,0,0,0);effectiveBuildingCoverage151To156=@(0,0,0,0,0,0);characterReferenceCoverage=.05;characterVisibility=1.0;characterSilhouetteIou=1.0}}
  foreach($characterCapture in $characterCaptures){Add-Member -InputObject $characterCapture -NotePropertyName objectIdGeneration -NotePropertyValue 1 -Force;foreach($prefix in @('raw','effective','reference')){Add-Member -InputObject $characterCapture -NotePropertyName "${prefix}DepthPixelSha256" -NotePropertyValue $depthHash -Force;Add-Member -InputObject $characterCapture -NotePropertyName "${prefix}DepthFinalFileSha256" -NotePropertyValue $depthHash -Force;Add-Member -InputObject $characterCapture -NotePropertyName "${prefix}DepthFinalFileBytes" -NotePropertyValue 128 -Force}}
  $fixturePosition=@([single]1,[single]2,[single]3);$fixturePositionBits=@($fixturePosition|ForEach-Object{[BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$_),0)});$fixtureRotation=[single].2;$fixtureRotationBits=[BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$fixtureRotation),0)
  foreach($characterCapture in $characterCaptures){foreach($field in @('captureStateCanonical','captureStateLegacy','captureStateEntities','captureStateTopology','captureStateTransform','captureStateController','captureStateYawPitch','captureStateRules','captureStateCamera','captureStatePalette','captureStateAnimation','captureStatePhysics')){Add-Member -InputObject $characterCapture -NotePropertyName $field -NotePropertyValue 1 -Force};Add-Member -InputObject $characterCapture -NotePropertyName captureStateSchemaVersion -NotePropertyValue 2 -Force;Add-Member -InputObject $characterCapture -NotePropertyName captureStateValid -NotePropertyValue 1 -Force;Add-Member -InputObject $characterCapture -NotePropertyName captureStateEntityCount -NotePropertyValue 1 -Force;Add-Member -InputObject $characterCapture -NotePropertyName captureStateEntityDiagnostics -NotePropertyValue @([pscustomobject]@{stableId=1;source='InitialSeed';category=1;dynamic=1;alive=1;position=$fixturePosition;positionRawBits=$fixturePositionBits;rotation=$fixtureRotation;rotationRawBits=$fixtureRotationBits;topologyHash=1;transformHash=1;bodyAvailable=1;bodyPosition=$fixturePosition;bodyVelocity=@(0,0,0);sceneAvailable=1;scenePosition=$fixturePosition}) -Force}
  $firstPersonViewmodels=@(
    [pscustomobject]@{name='first-day.png';armsIdentity=@('12592142912428528129',3,1);armsToken=31;armsPixels=1200;armsBounds=@(16,32,800,704);weaponIdentity=@('12592142912428528129',4,1);weaponToken=41;weaponPixels=700;weaponBounds=@(600,280,1264,704)},
    [pscustomobject]@{name='first-dusk.png';armsIdentity=@('12592142912428528129',3,1);armsToken=31;armsPixels=1180;armsBounds=@(18,34,802,700);weaponIdentity=@('12592142912428528129',4,1);weaponToken=41;weaponPixels=690;weaponBounds=@(602,282,1260,702)},
    [pscustomobject]@{name='first-night.png';armsIdentity=@('12592142912428528129',3,1);armsToken=31;armsPixels=1160;armsBounds=@(20,36,804,698);weaponIdentity=@('12592142912428528129',4,1);weaponToken=41;weaponPixels=680;weaponBounds=@(604,284,1258,700)}
  )
  $fixtureDelta=[single](1.0/60.0);$fixtureDeltaBits=[BitConverter]::ToUInt32([BitConverter]::GetBytes($fixtureDelta),0)
  $authorityCameraRaw=@(1..13);$paletteReplayDiagnostics=@(24..26|ForEach-Object{[pscustomobject]@{observed=1;inputSource='FixedTickReplayAuthoritativeGodCamera';liveSuppressed=1;inputBits=1;deltaSource='CaptureFixed60Hz';gameplayDelta=$fixtureDelta;gameplayDeltaRawBits=$fixtureDeltaBits;cameraHash=1;cameraAuthorityInputHash=2;cameraSubjectPoseHash=3;cameraProjectionMode=1;cameraProjectionModeName='Orthographic';cameraRawBits=$authorityCameraRaw;placementRaySource='ScreenPointToRayAuthoritativeGodCamera';placementPlaneSource='WorldYPlane';placementGridSource='ObjectPalette2mGrid';placementPlaneHeight=0.0;placementPlaneHeightRawBits=0;gridStep=2.0;gridStepRawBits=(Get-GameplayCanonicalSingleRawBits ([single]2.0));footPlacementCameraDistanceSource='PreviousFixedCamera';footPlacementCameraDistance=1.0;footPlacementCameraDistanceRawBits=(Get-GameplayCanonicalSingleRawBits ([single]1.0));tick=$_;normalizedPointer=@(.5,.5);rayOrigin=@(0,1,0);rayDirection=@(0,-1,0);hitValid=1;projectedHit=@(0,0,0);previewActive=1;previewValid=1;snappedTarget=@(0,.5,0);undoDepth=18;redoDepth=0}})
  $character = [pscustomobject]@{ runId=$runId;femaleFirstPersonArms=$femaleArms;maleFirstPersonArms=$maleArms;femaleCharacterMaterialCount=9;maleCharacterMaterialCount=9;firstPersonViewmodels=$firstPersonViewmodels;paletteReplayDiagnostics=$paletteReplayDiagnostics;captures=$characterCaptures }
  $godCoverageContract=Get-GameplayGodCoverageContract
  $cameraCaptures=@();for($captureIndex=0;$captureIndex-lt$names.Count;++$captureIndex){$god=$viewExpectations[$captureIndex].camera-eq'god';$third=$viewExpectations[$captureIndex].camera-eq'third';$cameraCaptures += [pscustomobject]@{name=$names[$captureIndex];topCenterRayElevation=if($third){.05}else{0};minimumTopCenterRayElevation=if($third){.03}else{0};rejectedHorizon=if($third){100}else{0};projectionMode=if($god){'Orthographic'}else{'Perspective'};projectionModeValue=if($god){1}else{0};verticalFovRadians=.85;orthographicHeight=if($god){26}else{0};godActualTriangleCoverage=if($god){$godCoverageContract.coverage}else{0};godVisibleWorldDescriptors=if($god){20}else{0};godBuildingUnion=if($god){.1}else{0};godMaximumBuildingPart=if($god){.05}else{0};godRoofCoverage=if($god){.02}else{0};godLodTransitions=if($god){1}else{0};builderValid=1;policyValid=1;matrixExact=1;candidateLow=1;expectedCameraHash=1;actualCameraHash=1}}
  for($captureIndex=0;$captureIndex-lt$names.Count;++$captureIndex){foreach($field in @('topCenterRayElevation','minimumTopCenterRayElevation')){$bits=[BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$characterCaptures[$captureIndex].$field),0);Add-Member -InputObject $characterCaptures[$captureIndex] -NotePropertyName "${field}RawBits" -NotePropertyValue $bits -Force;Add-Member -InputObject $cameraCaptures[$captureIndex] -NotePropertyName "${field}RawBits" -NotePropertyValue $bits -Force}}
  $camera = [pscustomobject]@{ runId=$runId;captures=$cameraCaptures }
  $sequenceCaptures = @()
  for ($captureIndex = 0; $captureIndex -lt $names.Count; ++$captureIndex) {
    $third = $names[$captureIndex] -like 'third-*' -or $names[$captureIndex] -in @('palette-building.png','melee-combat.png')
    $phaseCount = if ($third) { 3 } else { 2 }
    $token = $captureIndex + 1
    $phases = @()
    for ($phaseIndex = 0; $phaseIndex -lt $phaseCount; ++$phaseIndex) {
      $phases += [pscustomobject]@{ index=$phaseIndex;sequenceIndex=$phaseIndex;sequenceToken=$token;drawListRevision=1;cameraHash=1;stateHash=1;invariantHash=1;frameSerial=($phaseIndex+1);viewProjectionHash=1;tokenGeneration=($phaseIndex+1);historyGeneration=1;historyIndex=0;jitterIndex=0;jitterX=0.0;jitterY=0.0;excludingOpacity=1;includingOpacity=1;identityOrder=1 }
    }
    $sequenceCaptures += [pscustomobject]@{name=$names[$captureIndex];complete=1;expectedPhaseCount=$phaseCount;token=$token;jitterPolicy=1;fixedSampleIndex=0;phases=$phases}
  }
  $sequence = [pscustomobject]@{ runId=$runId; captures=$sequenceCaptures }
  foreach($capture in $character.captures){Add-Member -InputObject $capture -NotePropertyName effectiveBuildingUnion -NotePropertyValue 0.0 -Force}
  $referenceOpacityCaptures=@();foreach($capture in $character.captures){
    $third=$capture.camera-eq'third';$parts=@();for($partIndex=0;$partIndex-lt6;++$partIndex){$parts+=[pscustomobject]@{stableLow=151+$partIndex;requested=0;matched=0;missing=0;generationConflict=0;appliedOpacity=$null}}
    $referenceOpacityCaptures+=[pscustomobject]@{name=$capture.name;referencePhase=if($third){1}else{0};overrideMode='NoOverride';removalThreshold=.08;requestedReferenceCount=0;matchedReferenceCount=0;removedReferenceCount=0;removedCoverage=0.0;effectiveBuildingUnion=0.0;effectiveFrameSerial=2;referenceFrameSerial=3;effectiveSequenceIndex=1;referenceSequenceIndex=2;effectiveIncludingOpacity=1;referenceIncludingOpacity=1;effectiveExcludingOpacity=1;referenceExcludingOpacity=1;effectiveIdentityOrder=1;referenceIdentityOrder=1;parts=$parts}
  }
  $referenceOpacity=[pscustomobject]@{runId=$runId;source='GameplayLabReferenceOpacity';captures=$referenceOpacityCaptures}
  if(-not(Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence)){throw 'Reference opacity positive fixture failed'}
  $thirdReference=@($referenceOpacity.captures|Where-Object referencePhase -eq 1)[0];$thirdCharacter=@($character.captures|Where-Object camera -eq 'third')[0]
  $thirdSequence=@($sequence.captures|Where-Object name -eq $thirdReference.name)[0]
  $thirdReference.overrideMode='RemoveAll';$thirdReference.requestedReferenceCount=6;$thirdReference.matchedReferenceCount=6;$thirdReference.removedReferenceCount=6;$thirdReference.removedCoverage=.09;$thirdReference.effectiveBuildingUnion=.09;$thirdReference.referenceIncludingOpacity=3;$thirdCharacter.effectiveBuildingUnion=.09;$thirdSequence.phases[2].includingOpacity=3;foreach($part in $thirdReference.parts){$part.requested=1;$part.matched=1;$part.appliedOpacity=0.0};if(-not(Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence)){throw 'Removed reference opacity positive fixture failed'}
  $thirdReference.removalThreshold=.081;Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'reference removal threshold drift';$thirdReference.removalThreshold=.08
  $thirdReference.referenceIncludingOpacity=$thirdReference.effectiveIncludingOpacity;$thirdSequence.phases[2].includingOpacity=$thirdSequence.phases[1].includingOpacity;Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'removed count without fingerprint change';$thirdReference.referenceIncludingOpacity=3;$thirdSequence.phases[2].includingOpacity=3
  $thirdReference.parts[0].missing=1;Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'removed identity missing';$thirdReference.parts[0].missing=0
  $thirdReference.parts[0].generationConflict=1;Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'removed identity generation conflict';$thirdReference.parts[0].generationConflict=0
  $thirdReference.requestedReferenceCount=5;Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'removed requested count mismatch';$thirdReference.requestedReferenceCount=6
  $thirdReference.overrideMode='NoOverride';Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'removed override mode mismatch';$thirdReference.overrideMode='RemoveAll'
  $thirdReference.referenceFrameSerial=4;Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'reference sequence frame mismatch';$thirdReference.referenceFrameSerial=3
  $thirdReference.overrideMode='NoOverride';$thirdReference.requestedReferenceCount=0;$thirdReference.removedReferenceCount=0;$thirdReference.matchedReferenceCount=0;$thirdReference.removedCoverage=0;$thirdReference.effectiveBuildingUnion=0;$thirdCharacter.effectiveBuildingUnion=0;foreach($part in $thirdReference.parts){$part.requested=0;$part.matched=0;$part.appliedOpacity=$null};Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'retained count with fingerprint change';$thirdReference.referenceIncludingOpacity=$thirdReference.effectiveIncludingOpacity;$thirdSequence.phases[2].includingOpacity=$thirdSequence.phases[1].includingOpacity
  $savedThreshold=$thirdReference.removalThreshold;$thirdReference.PSObject.Properties.Remove('removalThreshold');Expect-Failure {Assert-GameplayReferenceOpacityEvidence $referenceOpacity $character $sequence|Out-Null} 'missing reference threshold';Add-Member -InputObject $thirdReference -NotePropertyName removalThreshold -NotePropertyValue $savedThreshold
  $receiptFemaleArms=[pscustomobject]@{firstPersonArmSubmeshCount=4;firstPersonArmTriangleCount=5726;firstPersonLeftArmTriangleCount=2862;firstPersonRightArmTriangleCount=2864}
  $receiptMaleArms=[pscustomobject]@{firstPersonArmSubmeshCount=4;firstPersonArmTriangleCount=5979;firstPersonLeftArmTriangleCount=2990;firstPersonRightArmTriangleCount=2989}
  $receipt = [pscustomobject]@{ runId=$runId;captures=11;entities=1;femaleCharacterMaterialCount=9;maleCharacterMaterialCount=9;characterMaterialCount=18;femaleFirstPersonArms=$receiptFemaleArms;maleFirstPersonArms=$receiptMaleArms;authoritativeTrajectoryHash='101';authoritativeAttackTrajectoryHashes=@('102','103','104');authoritativeTrajectoryCheckpointCount=9;authoritativeAttackCheckpointCounts=@(2,3,4) }
  $environment = [pscustomobject]@{ runId=$runId;source='GameplayShowcaseEnvironment3D';worldHash=1;terrainPatches=1;waterSegments=1;treeInstances=144;grassInstances=282;rockInstances=30;buildingComponents=6;descriptors=464;submittedDescriptors=464;executionObserved=1;counterDeltas=@(1,1,1,1,1,1,1,1,1,1);releasedDescriptors=464;retainedBytesAfterRelease=0 }
  $collision = [pscustomobject]@{ runId=$runId;source='EnvironmentCollisionStreaming3D';stateHash=1;descriptors=464;solidDescriptors=181;nonSolidDescriptors=283;activePartitions=1;activeColliders=181;activePhysicsBodies=181;activations=2;deactivations=1;failedActivations=0;rollbacks=0;stableIdEvidence=1;rayEvidence=1;meleeQueryEvidence=1;reloadStable=1;releasedDescriptors=464;releasedColliders=181;releasedPhysicsBodies=181;retainedBytesAfterRelease=0;activeCollidersAfterRelease=0;activePhysicsBodiesAfterRelease=0 }
  $palette = [pscustomobject]@{ runId=$runId;source='GameplayObjectPaletteUi3D';categories=6;drawnFrames=11;pointerDowns=1;drags=1;previews=1;commits=1;deterministicHash=1 }
  $photorealCaptures = @()
  $viewExpectations=@(Get-GameplayCaptureViewExpectations);$godCoverageContract=Get-GameplayGodCoverageContract
  for ($captureIndex = 0; $captureIndex -lt $names.Count; ++$captureIndex) {
    if($viewExpectations[$captureIndex].camera-eq'first'){$viewmodel=@($firstPersonViewmodels|Where-Object{$_.name-eq$names[$captureIndex]})[0];$required=@([pscustomobject]@{high='12592142912428528129';low=3;generation=1;nonzeroPixels=$viewmodel.armsPixels;coverage=.1},[pscustomobject]@{high='12592142912428528129';low=4;generation=1;nonzeroPixels=$viewmodel.weaponPixels;coverage=.2});$forbidden=@([pscustomobject]@{high='12592142912428528129';low=3;generation=2},[pscustomobject]@{high='12592142912428528129';low=4;generation=2})}else{$god=$viewExpectations[$captureIndex].camera-eq'god';$required=@([pscustomobject]@{high='12592142912428528129';low=1;generation=1;nonzeroPixels=if($god){$godCoverageContract.minimumPixels}else{100};coverage=if($god){$godCoverageContract.coverage}else{.1}},[pscustomobject]@{high='12592142921120481281';low=1;generation=1;nonzeroPixels=200;coverage=.2});$forbidden=@([pscustomobject]@{high='12592142912428528129';low=1;generation=2})}
    $god=$viewExpectations[$captureIndex].camera-eq'god';$lodTransition=if($viewExpectations[$captureIndex].requireLodTransition){1}else{0};$capturePath=[IO.Path]::GetFullPath((Join-Path $scratch $names[$captureIndex]));$photorealCaptures += [pscustomobject]@{ name=$names[$captureIndex];requestedName=$names[$captureIndex];committedName=$names[$captureIndex];requestedPath=$capturePath;committedPath=$capturePath;camera=$viewExpectations[$captureIndex].camera;lighting=$viewExpectations[$captureIndex].lighting;projectionMode=if($god){'Orthographic'}else{'Perspective'};projectionModeValue=if($god){1}else{0};verticalFovRadians=.85;orthographicHeight=if($god){26}else{0};godActualTriangleCoverage=if($god){$godCoverageContract.coverage}else{0};godVisibleWorldDescriptors=if($god){20}else{0};godBuildingUnion=if($god){.1}else{0};godMaximumBuildingPart=if($god){.05}else{0};godRoofCoverage=if($god){.02}else{0};godPredictedLodTransitions=if($god){1}else{0};godActualLodTransitions=if($god){1}else{0};godActualLodDualSubmissions=if($god){1}else{0};godLodPredictionMatched=1;godLodContractMissing=0;godLodContractExtra=0;godLodContractWrongLevel=0;godLodContractWrongBlend=0;requiredIdentityCount=$required.Count;requiredMatched=$required.Count;forbiddenIdentityCount=$forbidden.Count;forbiddenMatched=0;generationConflicts=0;semanticPass=1;effectiveFrameSerial=2;semanticFrameSerial=2;sceneSubmissionSerial=1;sceneDrawRevision=1;sceneIdentityOrderFingerprint=1;shadowFrameSerial=2;shadowResourceGeneration=1;shadowCasters=1;shadowDrawCalls=1;shadowCascadeDrawCalls=1;shadowCascadeRendered=1;shadowCascadeReused=0;shadowActiveCascades=1;shadowCascadeFingerprints=@(1,0,0,0);shadowPass=1;lodFrameSerial=2;lodInstances=1;lodTransitions=$lodTransition;lodDualSubmissions=$lodTransition;lodBudgetFallbacks=0;lodPass=1;requiredIdentities=$required;forbiddenIdentities=$forbidden }
  }
  $photoreal = [pscustomobject]@{ runId=$runId; source='GameplayPhotorealTyped'; expectedCaptures=11; actualCaptureIndex=11; completedCaptures=11; uniqueCaptureNames=11; manifestSha256=$package.manifestSha256; payloadFiles=54; validatedPayloadFiles=54; profileAssets=10; materials=11; validatedMaterials=11; meshParts=6; validatedMeshParts=6; validatedReferencedFiles=54; objectIdSupported=1; objectIdRequested=1; objectIdPass=1; semanticPass=1; depthCaptureSupported=1; depthPass=1; shadowPass=1; lodPass=1; shadowCasters=11; shadowDrawCalls=11; shadowCascadeDrawCalls=11; lodInstances=11; lodTransitions=3; lodDualSubmissions=3; lodBudgetFallbacks=0; captures=$photorealCaptures }
  foreach($capture in $photoreal.captures){if($capture.camera-ne'god'){$capture.godActualLodTransitions=$capture.lodTransitions;$capture.godActualLodDualSubmissions=$capture.lodDualSubmissions}}
  $photoreal.lodTransitions=11;$photoreal.lodDualSubmissions=11
  Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names
  $savedPaletteProjectionName=$character.paletteReplayDiagnostics[0].cameraProjectionModeName;$character.paletteReplayDiagnostics[0].cameraProjectionModeName='Perspective'
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'palette replay projection name/numeric mismatch'
  $character.paletteReplayDiagnostics[0].cameraProjectionModeName=$savedPaletteProjectionName
  $cameraPolicy=[pscustomobject]@{runId=$runId;source='GameplayLabCameraPolicy';captures=@($names|ForEach-Object{[pscustomobject]@{name=$_;paletteOpen=[uint64]($_-ceq'palette-building.png');minimumSubjectScreenArea=if($_-ceq'palette-building.png'){.08}else{.05};actualSubjectScreenArea=if($_-ceq'palette-building.png'){.08}else{.05}}})}
  Assert-GameplayCameraPolicyReceipt $cameraPolicy $runId $names
  $palettePolicyIndex=9;$cameraPolicy.captures[$palettePolicyIndex].paletteOpen=0;Expect-Failure { Assert-GameplayCameraPolicyReceipt $cameraPolicy $runId $names } 'palette camera omitted open';$cameraPolicy.captures[$palettePolicyIndex].paletteOpen=1
  $cameraPolicy.captures[0].paletteOpen=1;Expect-Failure { Assert-GameplayCameraPolicyReceipt $cameraPolicy $runId $names } 'non-palette camera leaked open';$cameraPolicy.captures[0].paletteOpen=0
  $cameraPolicy.captures[$palettePolicyIndex].minimumSubjectScreenArea=.05;Expect-Failure { Assert-GameplayCameraPolicyReceipt $cameraPolicy $runId $names } 'palette camera stale minimum area';$cameraPolicy.captures[$palettePolicyIndex].minimumSubjectScreenArea=.08
  $cameraPolicy.captures[$palettePolicyIndex].actualSubjectScreenArea=.075;Expect-Failure { Assert-GameplayCameraPolicyReceipt $cameraPolicy $runId $names } 'palette camera accepted undersized subject';$cameraPolicy.captures[$palettePolicyIndex].actualSubjectScreenArea=.08
  if($godCoverageContract.minimumPixels-ne[uint64][Math]::Ceiling($godCoverageContract.coverage*$godCoverageContract.width*$godCoverageContract.height)-or$godCoverageContract.coverage-ge.001){throw 'God boundary fixture cannot reject the legacy .001 mutation'}
  $coverageGodIndex=2;$belowCoverage=$godCoverageContract.coverage*.5
  $photoreal.captures[$coverageGodIndex].godActualTriangleCoverage=$belowCoverage;$camera.captures[$coverageGodIndex].godActualTriangleCoverage=$belowCoverage
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God actual coverage below source contract'
  $photoreal.captures[$coverageGodIndex].godActualTriangleCoverage=$godCoverageContract.coverage;$camera.captures[$coverageGodIndex].godActualTriangleCoverage=$godCoverageContract.coverage
  $godPrimary=$photoreal.captures[$coverageGodIndex].requiredIdentities[0];$godPrimary.nonzeroPixels=$godCoverageContract.minimumPixels-1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God primary pixels below derived boundary'
  $godPrimary.nonzeroPixels=$godCoverageContract.minimumPixels;$godPrimary.coverage=$belowCoverage
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God primary coverage below source contract'
  $godPrimary.coverage=$godCoverageContract.coverage
  foreach($field in @('builderValid','policyValid','matrixExact')){$saved=$camera.captures[$coverageGodIndex].$field;$camera.captures[$coverageGodIndex].$field=0;Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } "God camera diagnostic $field";$camera.captures[$coverageGodIndex].$field=$saved}
  $savedCandidate=$camera.captures[$coverageGodIndex].candidateLow;$camera.captures[$coverageGodIndex].candidateLow=0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God camera diagnostic candidate'
  $camera.captures[$coverageGodIndex].candidateLow=$savedCandidate;$savedExpectedHash=$camera.captures[$coverageGodIndex].expectedCameraHash;$camera.captures[$coverageGodIndex].expectedCameraHash=0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God camera diagnostic expected hash'
  $camera.captures[$coverageGodIndex].expectedCameraHash=$savedExpectedHash;$savedActualHash=$camera.captures[$coverageGodIndex].actualCameraHash;$camera.captures[$coverageGodIndex].actualCameraHash=$savedExpectedHash+1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God camera diagnostic hash mismatch'
  $camera.captures[$coverageGodIndex].actualCameraHash=$savedActualHash
  $character.captures[0].bodyMaterialSlots++
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'wrong first-person arms submesh/material count'
  $character.captures[0].bodyMaterialSlots--
  $savedArmTriangles=$character.captures[0].firstPersonArmTriangleCount;$character.captures[0].PSObject.Properties.Remove('firstPersonArmTriangleCount')
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing first-person arms triangle count'
  Add-Member -InputObject $character.captures[0] -NotePropertyName firstPersonArmTriangleCount -NotePropertyValue $savedArmTriangles
  $character.femaleFirstPersonArms.firstPersonLeftArmTriangleCount++
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'main/typed first-person arms count mismatch'
  $character.femaleFirstPersonArms.firstPersonLeftArmTriangleCount--
  $character.femaleCharacterMaterialCount++
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'female main/typed material count mismatch'
  $character.femaleCharacterMaterialCount--
  $receipt.characterMaterialCount++
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'female/male material sum mismatch'
  $receipt.characterMaterialCount--
  for($captureIndex=0;$captureIndex-lt$character.captures.Count;++$captureIndex){if($null-eq$character.captures[$captureIndex].PSObject.Properties['determinismFrameSerial']){Add-Member -InputObject $character.captures[$captureIndex] -NotePropertyName determinismFrameSerial -NotePropertyValue ([int64]$sequence.captures[$captureIndex].phases[1].frameSerial)};if($null-eq$character.captures[$captureIndex].PSObject.Properties['renderProvenance']){Add-Member -InputObject $character.captures[$captureIndex] -NotePropertyName renderProvenance -NotePropertyValue ([pscustomobject]@{frameSerial=[int64]$sequence.captures[$captureIndex].phases[1].frameSerial;sceneDrawRevision=1;worldRevision=1;submittedMeshInstances=1;submittedSkinnedInstances=1;submittedTerrainInstances=1;submittedWorldVertices=1;submittedWorldIndices=1})}}
  $canonicalState=Get-GameplayCanonicalStateSha $receipt $environment $collision $palette $character $camera $sequence $photoreal
  Add-Member -InputObject $receipt -NotePropertyName femaleGpuDraws -NotePropertyValue 100 -Force;Add-Member -InputObject $receipt -NotePropertyName maleGpuDraws -NotePropertyValue 200 -Force;Add-Member -InputObject $receipt -NotePropertyName weaponPbrDraws -NotePropertyValue 300 -Force
  $environment.counterDeltas=@($environment.counterDeltas|ForEach-Object{$_+100});$palette.drawnFrames+=100
  for($captureIndex=0;$captureIndex-lt$character.captures.Count;++$captureIndex){$characterCapture=$character.captures[$captureIndex];$characterCapture.objectIdGeneration+=100;$characterCapture.depthGeneration+=100;$characterCapture.contactFrameSerial+=1000;if($null-ne$characterCapture.PSObject.Properties['renderProvenance']){$characterCapture.renderProvenance.frameSerial+=1000};if($null-ne$characterCapture.PSObject.Properties['determinismFrameSerial']){$characterCapture.determinismFrameSerial+=1000}else{Add-Member -InputObject $characterCapture -NotePropertyName determinismFrameSerial -NotePropertyValue ([int64]$sequence.captures[$captureIndex].phases[1].frameSerial+1000)}}
  for($captureIndex=0;$captureIndex-lt$sequence.captures.Count;++$captureIndex){$sequenceCapture=$sequence.captures[$captureIndex];$sequenceCapture.token+=100;foreach($phase in $sequenceCapture.phases){$phase.sequenceToken+=100;$phase.frameSerial+=1000;$phase.tokenGeneration+=500;$phase.historyGeneration+=1000;$phase.historyIndex=1-$phase.historyIndex};$photorealCapture=$photoreal.captures[$captureIndex];$photorealCapture.effectiveFrameSerial+=1000;$photorealCapture.semanticFrameSerial+=1000;$photorealCapture.sceneSubmissionSerial+=1000;$photorealCapture.shadowFrameSerial+=1000;$photorealCapture.lodFrameSerial+=1000;$photorealCapture.shadowResourceGeneration+=9}
  if((Get-GameplayCanonicalStateSha $receipt $environment $collision $palette $character $camera $sequence $photoreal)-ne$canonicalState){throw 'Canonical state changed for process-local absolute provenance'}
  $assertCanonicalChanged={param([scriptblock]$Mutate,[scriptblock]$Restore,[string]$Name)&$Mutate;if((Get-GameplayCanonicalStateSha $receipt $environment $collision $palette $character $camera $sequence $photoreal)-eq$canonicalState){throw "Canonical semantic mutation was ignored: $Name"};&$Restore}
  $oldCoverage=$photoreal.captures[0].requiredIdentities[0].coverage;&$assertCanonicalChanged {$photoreal.captures[0].requiredIdentities[0].coverage=$oldCoverage+.01} {$photoreal.captures[0].requiredIdentities[0].coverage=$oldCoverage} 'coverage'
  $godCanonicalIndex=[Array]::IndexOf([object[]]@($viewExpectations|ForEach-Object{$_.name}),[object]'god-day.png');$oldLod=$photoreal.captures[$godCanonicalIndex].lodTransitions;&$assertCanonicalChanged {$photoreal.captures[$godCanonicalIndex].lodTransitions=$oldLod+1} {$photoreal.captures[$godCanonicalIndex].lodTransitions=$oldLod} 'LOD structure'
  $oldRendered=$photoreal.captures[0].shadowCascadeRendered;$oldReused=$photoreal.captures[0].shadowCascadeReused;&$assertCanonicalChanged {$photoreal.captures[0].shadowCascadeRendered=0;$photoreal.captures[0].shadowCascadeReused=1} {$photoreal.captures[0].shadowCascadeRendered=$oldRendered;$photoreal.captures[0].shadowCascadeReused=$oldReused} 'shadow cache structure'
  $oldPose=$character.captures[0].rawFemalePoseHash;&$assertCanonicalChanged {$character.captures[0].rawFemalePoseHash=$oldPose+1} {$character.captures[0].rawFemalePoseHash=$oldPose} 'pose'
  $oldCameraHash=$camera.captures[0].expectedCameraHash;&$assertCanonicalChanged {$camera.captures[0].expectedCameraHash=$oldCameraHash+1} {$camera.captures[0].expectedCameraHash=$oldCameraHash} 'camera'
  $oldName=$photoreal.captures[0].name;&$assertCanonicalChanged {$photoreal.captures[0].name='mutated.png'} {$photoreal.captures[0].name=$oldName} 'name'
  $oldRelativeFrame=$sequence.captures[0].phases[1].frameSerial;&$assertCanonicalChanged {$sequence.captures[0].phases[1].frameSerial=$oldRelativeFrame+1} {$sequence.captures[0].phases[1].frameSerial=$oldRelativeFrame} 'relative phase order'
  $oldRenderFrame=$character.captures[0].renderProvenance.frameSerial;&$assertCanonicalChanged {$character.captures[0].renderProvenance.frameSerial=$oldRenderFrame+1} {$character.captures[0].renderProvenance.frameSerial=$oldRenderFrame} 'render provenance effective-frame relation'
  $oldBaseFrame=$sequence.captures[0].phases[0].frameSerial;&$assertCanonicalChanged {$sequence.captures[0].phases[0].frameSerial=$oldRenderFrame+1} {$sequence.captures[0].phases[0].frameSerial=$oldBaseFrame} 'render provenance nonmonotonic phase relation'
  $oldObjectGeneration=$character.captures[1].objectIdGeneration;&$assertCanonicalChanged {$character.captures[1].objectIdGeneration=$oldObjectGeneration+1} {$character.captures[1].objectIdGeneration=$oldObjectGeneration} 'relative object-ID generation'
  $identity=$photoreal.captures[0].requiredIdentities[0];$oldIdentityHigh=$identity.high;&$assertCanonicalChanged {$identity.high='12592142912428528130'} {$identity.high=$oldIdentityHigh} 'identity stable ID'
  $oldIdentityGeneration=$identity.generation;&$assertCanonicalChanged {$identity.generation=$oldIdentityGeneration+1} {$identity.generation=$oldIdentityGeneration} 'identity generation'
  $oldIdentityPixels=$identity.nonzeroPixels;&$assertCanonicalChanged {$identity.nonzeroPixels=$oldIdentityPixels+1} {$identity.nonzeroPixels=$oldIdentityPixels} 'identity nonzero pixels'
  $oldShadowFingerprint=$photoreal.captures[0].shadowCascadeFingerprints[0];&$assertCanonicalChanged {$photoreal.captures[0].shadowCascadeFingerprints[0]=$oldShadowFingerprint+1} {$photoreal.captures[0].shadowCascadeFingerprints[0]=$oldShadowFingerprint} 'shadow fingerprint'
  $oldLodInstances=$photoreal.captures[0].lodInstances;&$assertCanonicalChanged {$photoreal.captures[0].lodInstances=$oldLodInstances+1} {$photoreal.captures[0].lodInstances=$oldLodInstances} 'LOD instances'
  $oldLodDual=$photoreal.captures[$godCanonicalIndex].lodDualSubmissions;&$assertCanonicalChanged {$photoreal.captures[$godCanonicalIndex].lodDualSubmissions=$oldLodDual+1} {$photoreal.captures[$godCanonicalIndex].lodDualSubmissions=$oldLodDual} 'LOD dual submissions'
  $oldLodFallback=$photoreal.captures[0].lodBudgetFallbacks;&$assertCanonicalChanged {$photoreal.captures[0].lodBudgetFallbacks=$oldLodFallback+1} {$photoreal.captures[0].lodBudgetFallbacks=$oldLodFallback} 'LOD fallback'
  $oldDepthSha=$character.captures[0].rawDepthSha256;&$assertCanonicalChanged {$character.captures[0].rawDepthSha256='b'*64} {$character.captures[0].rawDepthSha256=$oldDepthSha} 'depth'
  $oldContact=$character.captures[0].feetContactPassed;&$assertCanonicalChanged {$character.captures[0].feetContactPassed=0} {$character.captures[0].feetContactPassed=$oldContact} 'contact'
  $oldContactFrame=$character.captures[1].contactFrameSerial;&$assertCanonicalChanged {$character.captures[1].contactFrameSerial=$oldContactFrame+1} {$character.captures[1].contactFrameSerial=$oldContactFrame} 'relative contact frame'
  $oldRequestedPath=$photoreal.captures[0].requestedPath;&$assertCanonicalChanged {$photoreal.captures[0].requestedPath=$oldRequestedPath+'.mutated'} {$photoreal.captures[0].requestedPath=$oldRequestedPath} 'requested path'
  $oldCommittedPath=$photoreal.captures[0].committedPath;&$assertCanonicalChanged {$photoreal.captures[0].committedPath=$oldCommittedPath+'.mutated'} {$photoreal.captures[0].committedPath=$oldCommittedPath} 'committed path'
  $oldSceneRevision=$photoreal.captures[0].sceneDrawRevision;&$assertCanonicalChanged {$photoreal.captures[0].sceneDrawRevision=$oldSceneRevision+1} {$photoreal.captures[0].sceneDrawRevision=$oldSceneRevision} 'Scene revision'
  $oldSceneOrder=$photoreal.captures[0].sceneIdentityOrderFingerprint;&$assertCanonicalChanged {$photoreal.captures[0].sceneIdentityOrderFingerprint=$oldSceneOrder+1} {$photoreal.captures[0].sceneIdentityOrderFingerprint=$oldSceneOrder} 'Scene identity order'
  $phase=$sequence.captures[0].phases[0];$oldSequenceState=$phase.stateHash;&$assertCanonicalChanged {$phase.stateHash=$oldSequenceState+1} {$phase.stateHash=$oldSequenceState} 'sequence state'
  $oldSequenceInvariant=$phase.invariantHash;&$assertCanonicalChanged {$phase.invariantHash=$oldSequenceInvariant+1} {$phase.invariantHash=$oldSequenceInvariant} 'sequence invariant'
  $oldIncluding=$phase.includingOpacity;&$assertCanonicalChanged {$phase.includingOpacity=$oldIncluding+1} {$phase.includingOpacity=$oldIncluding} 'including-opacity fingerprint'
  $oldExcluding=$phase.excludingOpacity;&$assertCanonicalChanged {$phase.excludingOpacity=$oldExcluding+1} {$phase.excludingOpacity=$oldExcluding} 'excluding-opacity fingerprint'
  $oldAuthoritativeTrajectory=$receipt.authoritativeTrajectoryHash;&$assertCanonicalChanged {$receipt.authoritativeTrajectoryHash='201'} {$receipt.authoritativeTrajectoryHash=$oldAuthoritativeTrajectory} 'authoritative trajectory hash'
  $oldViewmodelPixels=$character.firstPersonViewmodels[0].armsPixels;&$assertCanonicalChanged {$character.firstPersonViewmodels[0].armsPixels=$oldViewmodelPixels+1} {$character.firstPersonViewmodels[0].armsPixels=$oldViewmodelPixels} 'first-person viewmodel GPU pixels'
  $oldAuthoritativeAttack=$receipt.authoritativeAttackTrajectoryHashes[1];&$assertCanonicalChanged {$receipt.authoritativeAttackTrajectoryHashes[1]='203'} {$receipt.authoritativeAttackTrajectoryHashes[1]=$oldAuthoritativeAttack} 'authoritative attack motion hash'
  $oldAttackCounts=@($receipt.authoritativeAttackCheckpointCounts);&$assertCanonicalChanged {$receipt.authoritativeAttackCheckpointCounts=@(3,2,4)} {$receipt.authoritativeAttackCheckpointCounts=$oldAttackCounts} 'authoritative per-attack checkpoint counts'
  $horizonCanonicalIndex=1;$oldHorizonActual=$character.captures[$horizonCanonicalIndex].topCenterRayElevation;&$assertCanonicalChanged {$character.captures[$horizonCanonicalIndex].topCenterRayElevation=$oldHorizonActual+.01} {$character.captures[$horizonCanonicalIndex].topCenterRayElevation=$oldHorizonActual} 'analytic horizon actual'
  $oldHorizonMinimum=$camera.captures[$horizonCanonicalIndex].minimumTopCenterRayElevation;&$assertCanonicalChanged {$camera.captures[$horizonCanonicalIndex].minimumTopCenterRayElevation=$oldHorizonMinimum+.01} {$camera.captures[$horizonCanonicalIndex].minimumTopCenterRayElevation=$oldHorizonMinimum} 'analytic horizon minimum'
  $oldRejectedHorizon=$character.captures[$horizonCanonicalIndex].rejectedHorizon;&$assertCanonicalChanged {$character.captures[$horizonCanonicalIndex].rejectedHorizon=$oldRejectedHorizon+1} {$character.captures[$horizonCanonicalIndex].rejectedHorizon=$oldRejectedHorizon} 'analytic horizon rejection count'
  $godLodCanonicalIndex=2;$oldGodActual=$photoreal.captures[$godLodCanonicalIndex].godActualLodTransitions;&$assertCanonicalChanged {$photoreal.captures[$godLodCanonicalIndex].godActualLodTransitions=$oldGodActual+1} {$photoreal.captures[$godLodCanonicalIndex].godActualLodTransitions=$oldGodActual} 'God actual LOD transitions'
  $oldGodPredicted=$photoreal.captures[$godLodCanonicalIndex].godPredictedLodTransitions;&$assertCanonicalChanged {$photoreal.captures[$godLodCanonicalIndex].godPredictedLodTransitions=$oldGodPredicted+1} {$photoreal.captures[$godLodCanonicalIndex].godPredictedLodTransitions=$oldGodPredicted} 'God predicted LOD transitions'

  $receipt.authoritativeAttackTrajectoryHashes[0]=$receipt.authoritativeTrajectoryHash
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'duplicate authoritative trajectory hash'
  $receipt.authoritativeAttackTrajectoryHashes[0]='102';$receipt.authoritativeAttackCheckpointCounts=@(2,3,3)
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'authoritative checkpoint count sum mismatch'
  $receipt.authoritativeAttackCheckpointCounts=$oldAttackCounts

  $photoreal.manifestSha256 = 'b' * 64
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'typed manifest SHA mismatch'
  $photoreal.manifestSha256 = $package.manifestSha256
  $camera.captures[1].name = $names[0]
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'duplicate ordered capture name'
  $camera.captures[1].name = $names[1]
  $character.captures[1].gpuFeetContactPassed = 0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing contact evidence'
  $character.captures[1].gpuFeetContactPassed = 1
  $character.captures[1].contactLeftFootPixels=0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'empty left-foot contact mask'
  $character.captures[1].contactLeftFootPixels=4;$character.captures[1].contactArtifactSha256='0'*64
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'zero contact artifact hash'
  $character.captures[1].contactArtifactSha256='b'*64;$character.captures[1].contactFrameSerial=3
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'contact raster provenance mismatch'
  $character.captures[1].contactFrameSerial=2
  $photoreal.lodBudgetFallbacks = 1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'LOD fallback'
  $photoreal.lodBudgetFallbacks = 0
  $godIndex=[Array]::IndexOf([object[]]@($viewExpectations|ForEach-Object{$_.name}),[object]'god-day.png');$photoreal.captures[$godIndex].lodTransitions=0;$photoreal.captures[$godIndex].lodDualSubmissions=0;$photoreal.lodTransitions=2;$photoreal.lodDualSubmissions=2
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'god view missing LOD transition'
  $photoreal.captures[$godIndex].lodTransitions=1;$photoreal.captures[$godIndex].lodDualSubmissions=1;$photoreal.lodTransitions=3;$photoreal.lodDualSubmissions=3
  $nonGodIndex=[Array]::IndexOf([object[]]@($viewExpectations|ForEach-Object{$_.name}),[object]'third-day.png');$photoreal.captures[$nonGodIndex].lodTransitions=1;$photoreal.captures[$nonGodIndex].lodDualSubmissions=1;$photoreal.lodTransitions=4;$photoreal.lodDualSubmissions=4
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'non-god view unexpected LOD transition'
  $photoreal.captures[$nonGodIndex].lodTransitions=0;$photoreal.captures[$nonGodIndex].lodDualSubmissions=0;$photoreal.lodTransitions=3;$photoreal.lodDualSubmissions=3
  $photoreal.captures[2].semanticFrameSerial = 3
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'per-view frame provenance mismatch'
  $photoreal.captures[2].semanticFrameSerial = 2
  $photoreal.captures[0].requiredIdentities[0].generation = 2
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'required identity mismatch'
  $photoreal.captures[0].requiredIdentities[0].generation = 1
  $character.firstPersonViewmodels[0].armsIdentity[2]=2
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'first-person arms wrong generation'
  $character.firstPersonViewmodels[0].armsIdentity[2]=1
  $savedViewmodelName=$character.firstPersonViewmodels[1].name;$character.firstPersonViewmodels[1].name=$character.firstPersonViewmodels[0].name
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'duplicate first-person viewmodel entry'
  $character.firstPersonViewmodels[1].name=$savedViewmodelName
  $oldWeaponToken=$character.firstPersonViewmodels[0].weaponToken;$character.firstPersonViewmodels[0].weaponToken=$character.firstPersonViewmodels[0].armsToken
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'first-person arms/weapon token conflict'
  $character.firstPersonViewmodels[0].weaponToken=$oldWeaponToken
  $savedViewmodels=@($character.firstPersonViewmodels);$character.firstPersonViewmodels=@($savedViewmodels|Select-Object -Skip 1)
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing first-person viewmodel entry'
  $character.firstPersonViewmodels=$savedViewmodels
  $oldMargin=$character.firstPersonViewmodels[0].armsBounds[0];$character.firstPersonViewmodels[0].armsBounds[0]=15
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'first-person viewmodel outside screen margin'
  $character.firstPersonViewmodels[0].armsBounds[0]=$oldMargin
  $savedProjectionMode=$camera.captures[0].projectionMode;$camera.captures[0].PSObject.Properties.Remove('projectionMode')
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing camera projection field'
  Add-Member -InputObject $camera.captures[0] -NotePropertyName projectionMode -NotePropertyValue $savedProjectionMode
  $savedFov=$photoreal.captures[0].verticalFovRadians;$photoreal.captures[0].verticalFovRadians='not-a-number';$camera.captures[0].verticalFovRadians='not-a-number'
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'non-numeric perspective FOV'
  $photoreal.captures[0].verticalFovRadians=$savedFov;$camera.captures[0].verticalFovRadians=$savedFov
  $projectionGodIndex=2;$savedGodUnion=$photoreal.captures[$projectionGodIndex].godBuildingUnion;$photoreal.captures[$projectionGodIndex].godBuildingUnion=.23;$camera.captures[$projectionGodIndex].godBuildingUnion=.23
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God building union out of bounds'
  $photoreal.captures[$projectionGodIndex].godBuildingUnion=$savedGodUnion;$camera.captures[$projectionGodIndex].godBuildingUnion=$savedGodUnion
  $camera.captures[0].verticalFovRadians=.9
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'camera/photoreal projection mismatch'
  $camera.captures[0].verticalFovRadians=$savedFov
  $savedGodMode=$photoreal.captures[$projectionGodIndex].projectionMode;$photoreal.captures[$projectionGodIndex].projectionMode='Perspective';$camera.captures[$projectionGodIndex].projectionMode='Perspective'
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God perspective projection'
  $photoreal.captures[$projectionGodIndex].projectionMode=$savedGodMode;$camera.captures[$projectionGodIndex].projectionMode=$savedGodMode
  foreach($wrongPerspectiveIndex in @(0,1)){$savedMode=$photoreal.captures[$wrongPerspectiveIndex].projectionMode;$savedHeight=$photoreal.captures[$wrongPerspectiveIndex].orthographicHeight;$photoreal.captures[$wrongPerspectiveIndex].projectionMode='Orthographic';$camera.captures[$wrongPerspectiveIndex].projectionMode='Orthographic';$photoreal.captures[$wrongPerspectiveIndex].orthographicHeight=26;$camera.captures[$wrongPerspectiveIndex].orthographicHeight=26
    Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } "first/third orthographic projection index=$wrongPerspectiveIndex"
    $photoreal.captures[$wrongPerspectiveIndex].projectionMode=$savedMode;$camera.captures[$wrongPerspectiveIndex].projectionMode=$savedMode;$photoreal.captures[$wrongPerspectiveIndex].orthographicHeight=$savedHeight;$camera.captures[$wrongPerspectiveIndex].orthographicHeight=$savedHeight
  }
  $savedGodHeight=$photoreal.captures[$projectionGodIndex].orthographicHeight;$photoreal.captures[$projectionGodIndex].orthographicHeight=0;$camera.captures[$projectionGodIndex].orthographicHeight=0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God invalid orthographic height'
  $photoreal.captures[$projectionGodIndex].orthographicHeight=$savedGodHeight;$camera.captures[$projectionGodIndex].orthographicHeight=$savedGodHeight
  $savedViewProjectionHash=$sequence.captures[$projectionGodIndex].phases[0].viewProjectionHash;$sequence.captures[$projectionGodIndex].phases[0].viewProjectionHash=0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God zero view-projection matrix hash'
  $sequence.captures[$projectionGodIndex].phases[0].viewProjectionHash=$savedViewProjectionHash
  $horizonIndex=1;$savedHorizonActual=$character.captures[$horizonIndex].topCenterRayElevation;$character.captures[$horizonIndex].PSObject.Properties.Remove('topCenterRayElevation')
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing character horizon field'
  Add-Member -InputObject $character.captures[$horizonIndex] -NotePropertyName topCenterRayElevation -NotePropertyValue $savedHorizonActual
  $savedHorizonMinimum=$camera.captures[$horizonIndex].minimumTopCenterRayElevation;$camera.captures[$horizonIndex].PSObject.Properties.Remove('minimumTopCenterRayElevation')
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing camera horizon field'
  Add-Member -InputObject $camera.captures[$horizonIndex] -NotePropertyName minimumTopCenterRayElevation -NotePropertyValue $savedHorizonMinimum
  $character.captures[$horizonIndex].topCenterRayElevation='0.05';$camera.captures[$horizonIndex].topCenterRayElevation='0.05'
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'non-numeric horizon actual'
  $character.captures[$horizonIndex].topCenterRayElevation=$savedHorizonActual;$camera.captures[$horizonIndex].topCenterRayElevation=$savedHorizonActual
  $character.captures[$horizonIndex].topCenterRayElevation=[double]::NaN;$camera.captures[$horizonIndex].topCenterRayElevation=[double]::NaN
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'NaN horizon actual'
  $character.captures[$horizonIndex].topCenterRayElevation=.02;$camera.captures[$horizonIndex].topCenterRayElevation=.02
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'horizon actual below minimum'
  $character.captures[$horizonIndex].topCenterRayElevation=$savedHorizonActual;$camera.captures[$horizonIndex].topCenterRayElevation=$savedHorizonActual
  $nonThirdHorizonIndex=0;$character.captures[$nonThirdHorizonIndex].rejectedHorizon=1;$camera.captures[$nonThirdHorizonIndex].rejectedHorizon=1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'non-third horizon evidence'
  $character.captures[$nonThirdHorizonIndex].rejectedHorizon=0;$camera.captures[$nonThirdHorizonIndex].rejectedHorizon=0
  $camera.captures[$horizonIndex].topCenterRayElevation=$savedHorizonActual+.01
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'character/camera horizon cross-receipt mismatch'
  $camera.captures[$horizonIndex].topCenterRayElevation=$savedHorizonActual
  $savedHorizonBits=$camera.captures[$horizonIndex].topCenterRayElevationRawBits;$camera.captures[$horizonIndex].topCenterRayElevationRawBits=[uint32]($savedHorizonBits+1)
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'character/camera horizon raw-bit mismatch'
  $camera.captures[$horizonIndex].topCenterRayElevationRawBits=$savedHorizonBits;$character.captures[$horizonIndex].topCenterRayElevationRawBits=[uint32]($savedHorizonBits+1);$camera.captures[$horizonIndex].topCenterRayElevationRawBits=[uint32]($savedHorizonBits+1)
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'horizon text/raw-bit round-trip mismatch'
  $character.captures[$horizonIndex].topCenterRayElevationRawBits=$savedHorizonBits;$camera.captures[$horizonIndex].topCenterRayElevationRawBits=$savedHorizonBits
  $godMappingIndex=2;$savedGodActual=$photoreal.captures[$godMappingIndex].godActualLodTransitions;$photoreal.captures[$godMappingIndex].PSObject.Properties.Remove('godActualLodTransitions');Add-Member -InputObject $photoreal.captures[$godMappingIndex] -NotePropertyName godLodTransitions -NotePropertyValue $savedGodActual
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing actual God LOD with obsolete same-name alias'
  $photoreal.captures[$godMappingIndex].PSObject.Properties.Remove('godLodTransitions');Add-Member -InputObject $photoreal.captures[$godMappingIndex] -NotePropertyName godActualLodTransitions -NotePropertyValue $savedGodActual
  $camera.captures[$godMappingIndex].godLodTransitions=$savedGodActual+1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'Camera actual/Photoreal actual God LOD mismatch'
  $camera.captures[$godMappingIndex].godLodTransitions=$savedGodActual
  $photoreal.captures[$godMappingIndex].godPredictedLodTransitions=$savedGodActual+1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'God predicted LOD misused as actual'
  $photoreal.captures[$godMappingIndex].godPredictedLodTransitions=$savedGodActual
  $nonGodMappingIndex=0;$camera.captures[$nonGodMappingIndex].godLodTransitions=1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'non-God Camera actual LOD nonzero'
  $camera.captures[$nonGodMappingIndex].godLodTransitions=0;$photoreal.captures[$nonGodMappingIndex].godPredictedLodTransitions=1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'non-God Photoreal predicted LOD nonzero'
  $photoreal.captures[$nonGodMappingIndex].godPredictedLodTransitions=0;$photoreal.captures[$nonGodMappingIndex].godActualLodTransitions=0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'non-God actual LOD counter mismatch'
  $photoreal.captures[$nonGodMappingIndex].godActualLodTransitions=$photoreal.captures[$nonGodMappingIndex].lodTransitions
  $photoreal.captures[0].requiredIdentities[0].nonzeroPixels = 0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'required identity zero pixels'
  $photoreal.captures[0].requiredIdentities[0].nonzeroPixels = 100
  $photoreal.captures[0].generationConflicts = 1
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'generation conflict'
  $photoreal.captures[0].generationConflicts = 0
  $photoreal.captures[0].sceneSubmissionSerial = 0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing Scene submission provenance'
  $photoreal.captures[0].sceneSubmissionSerial = 1
  $photoreal.captures[0].shadowCascadeFingerprints[0] = 0
  Expect-Failure { Assert-GameplayRunEvidence $receipt $environment $collision $palette $character $camera $sequence $photoreal $package $names } 'missing active shadow fingerprint'
  $photoreal.captures[0].shadowCascadeFingerprints[0] = 1

  $evidenceRoot = Join-Path $scratch 'capture-evidence'
  New-Item -ItemType Directory -Force -Path $evidenceRoot | Out-Null
  foreach ($name in $names) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($name)
    $artifacts = @($name, "$stem-object-id.png", "$stem-raw.png", "$stem-raw-object-id.png", "$stem-depth.png", "$stem-raw-depth.png")
    if ($name -like 'third-*' -or $name -in @('palette-building.png', 'melee-combat.png')) {
      $artifacts += @("$stem-reference.png", "$stem-reference-object-id.png", "$stem-reference-depth.png", "$stem-contact.png", "$stem-reference-contact.png")
    }
    foreach ($artifact in $artifacts) { [IO.File]::WriteAllText((Join-Path $evidenceRoot $artifact), "artifact-$artifact") }
  }
  $executable = Join-Path $evidenceRoot 'synthetic.exe'; [IO.File]::WriteAllText($executable, 'executable')
  Push-Location $evidenceRoot;try{$relativeResolved=Resolve-GameplayCaptureExecutablePath '.\synthetic.exe' $evidenceRoot}finally{Pop-Location};if($relativeResolved-cne[IO.Path]::GetFullPath($executable)){throw 'Relative executable positive path resolution failed'}
  Expect-Failure { Resolve-GameplayCaptureExecutablePath (Join-Path $evidenceRoot 'missing.exe') $evidenceRoot | Out-Null } 'missing executable path'
  Expect-Failure { Resolve-GameplayCaptureExecutablePath (Join-Path $evidenceRoot 'nested\..\synthetic.exe') $evidenceRoot | Out-Null } 'executable traversal path'
  $restrictedRoot=Join-Path $evidenceRoot 'restricted';New-Item -ItemType Directory -Force -Path $restrictedRoot|Out-Null
  Expect-Failure { Resolve-GameplayCaptureExecutablePath $executable $restrictedRoot | Out-Null } 'executable cross-root path'
  $receiptNames = @('gameplay-receipt.json','gameplay-character-capture-receipt.json','gameplay-camera-depth-diagnostic-receipt.json','gameplay-camera-policy-receipt.json','gameplay-determinism-integrity-receipt.json','gameplay-readback-sequence-receipt.json','gameplay-hdr-capture-receipt.json','gameplay-temporal-history-receipt.json','gameplay-temporal-ghost-artifact-receipt.json','gameplay-reference-opacity-receipt.json','gameplay-environment-receipt.json','gameplay-environment-collision-receipt.json','gameplay-palette-ui-receipt.json','gameplay-photoreal-receipt.json')
  $runDirectory = Join-Path $evidenceRoot 'determinism-runs/run-1'; New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
  foreach ($name in $receiptNames) { [IO.File]::WriteAllText((Join-Path $runDirectory $name), "receipt-$name") }
  $runArtifacts=@();foreach($artifact in @(Get-GameplayExpectedRunArtifactNames $names)){Copy-Item -LiteralPath (Join-Path $evidenceRoot $artifact) -Destination (Join-Path $runDirectory $artifact);$runArtifacts+=[pscustomobject]@{name=$artifact;sha256=Get-GameplayEvidenceSha256Hex (Join-Path $runDirectory $artifact)}}
  $run = [pscustomobject]@{ run=1; runId=$runId; stateSha256=('c' * 64); packageSnapshotEpochSha256=$package.snapshotEpochSha256; fatalAbsent=1; artifacts=$runArtifacts; receipts=Get-GameplayRunReceiptEvidence $runDirectory $receiptNames }
  $sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
  [IO.File]::WriteAllText((Join-Path $evidenceRoot 'gameplay-fatal-error.txt'), 'synthetic fatal')
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'SingleRun fatal marker before final receipt'
  Remove-Item -LiteralPath (Join-Path $evidenceRoot 'gameplay-fatal-error.txt') -Force
  $final = Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint'
  if ($final.evidencePayloadSha256 -notmatch '^[0-9a-f]{64}$' -or @($final.determinismRuns[0].receipts).Count -ne $receiptNames.Count) {
    throw 'Final evidence receipt positive/self-hash gate failed'
  }
  $savedArtifacts=@($run.artifacts);$run.artifacts=@($savedArtifacts|Select-Object -Skip 1)
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'missing archived artifact entry'
  $run.artifacts=@($savedArtifacts);$run.artifacts[-1]=$run.artifacts[0]
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'duplicate archived artifact entry'
  $run.artifacts=$savedArtifacts
  $savedReceipts=@($run.receipts);$run.receipts=@($savedReceipts|Select-Object -Skip 1)
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'missing archived receipt entry'
  $run.receipts=@($savedReceipts|Where-Object{$_.name-cne'gameplay-hdr-capture-receipt.json'})
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'missing archived HDR receipt entry'
  $run.receipts=@($savedReceipts);$run.receipts[-1]=$run.receipts[0]
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'duplicate archived receipt entry'
  $run.receipts=$savedReceipts
  [IO.File]::AppendAllText((Join-Path $runDirectory $names[0]), 'tamper')
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'archived run artifact tamper'
  Copy-Item -LiteralPath (Join-Path $evidenceRoot $names[0]) -Destination (Join-Path $runDirectory $names[0]) -Force
  $mutatedStateRun = [pscustomobject]@{ run=2; runId='11234567-89ab-cdef-8123-456789abcdef'; stateSha256=('d' * 64); packageSnapshotEpochSha256=$package.snapshotEpochSha256; fatalAbsent=1; artifacts=@(); receipts=@() }
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run,$mutatedStateRun) $names 1.0 'synthetic-fingerprint' | Out-Null } 'cross-run authoritative state mutation'
  $savedEpoch = $run.packageSnapshotEpochSha256; $run.packageSnapshotEpochSha256 = 'e' * 64
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'cross-run package snapshot mutation'
  $run.packageSnapshotEpochSha256 = $savedEpoch
  [IO.File]::AppendAllText((Join-Path $runDirectory $receiptNames[0]), 'tamper')
  Expect-Failure { Write-GameplayFinalEvidenceReceipt $evidenceRoot $executable $sourceRoot $package $receipt @($run) $names 1.0 'synthetic-fingerprint' | Out-Null } 'copied run receipt tamper'

  $extra = Join-Path $packageRoot 'unexpected.bin'; [IO.File]::WriteAllText($extra, 'extra')
  Expect-Failure { Get-GameplayPhotorealPackageEvidence $manifestPath | Out-Null } 'dirty staging package'
  Remove-Item -LiteralPath $extra -Force
  $firstPayload = Join-Path $packageRoot $files[0].path
  [IO.File]::AppendAllText($firstPayload, 'tamper')
  Expect-Failure { Get-GameplayPhotorealPackageEvidence $manifestPath | Out-Null } 'payload TOCTOU/content mismatch'

  }
  "GameplayLabCaptureEvidenceSmoke PASS section=$Section"
} finally {
  if (Test-Path -LiteralPath $scratch) { Remove-Item -LiteralPath $scratch -Recurse -Force }
}
