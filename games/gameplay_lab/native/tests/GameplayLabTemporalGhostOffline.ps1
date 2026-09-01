param(
  [Parameter(Mandatory=$true)][string]$CaptureDirectory,
  [Parameter(Mandatory=$true)][string]$OutputLog
)

$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'GameplayLabCaptureEvidence.ps1')

$directory=(Resolve-Path -LiteralPath $CaptureDirectory).Path
$outputPath=[IO.Path]::GetFullPath($OutputLog)
$receiptPath=Join-Path $directory 'gameplay-temporal-ghost-artifact-receipt.json'
$gameplay=Get-Content -Raw -LiteralPath (Join-Path $directory 'gameplay-receipt.json')|ConvertFrom-Json
$character=Get-Content -Raw -LiteralPath (Join-Path $directory 'gameplay-character-capture-receipt.json')|ConvertFrom-Json
$photoreal=Get-Content -Raw -LiteralPath (Join-Path $directory 'gameplay-photoreal-receipt.json')|ConvertFrom-Json
$sequence=Get-Content -Raw -LiteralPath (Join-Path $directory 'gameplay-readback-sequence-receipt.json')|ConvertFrom-Json
$artifactReceipt=Get-Content -Raw -LiteralPath $receiptPath|ConvertFrom-Json
$receiptPayload=[ordered]@{source=$artifactReceipt.source;runId=$artifactReceipt.runId;captures=$artifactReceipt.captures}
$receiptPayloadSha=Get-GameplayEvidenceTextSha256Hex ($receiptPayload|ConvertTo-Json -Depth 8 -Compress)
if($gameplay.runId-cne$artifactReceipt.runId-or$artifactReceipt.payloadSha256-cne$receiptPayloadSha){throw 'Offline temporal receipt runId/self-hash mismatch'}
$receiptSnapshot=Get-GameplayEvidenceFileSnapshot $receiptPath

$captureEvidence=@()
foreach($stem in @('third-day','third-dusk')){
  $name="$stem.png";$characterCapture=@($character.captures|Where-Object name -eq $name);$photorealCapture=@($photoreal.captures|Where-Object name -eq $name);$sequenceCapture=@($sequence.captures|Where-Object name -eq $name);$artifactCapture=@($artifactReceipt.captures|Where-Object name -eq $name)
  if($characterCapture.Count-ne1-or$photorealCapture.Count-ne1-or$sequenceCapture.Count-ne1-or$artifactCapture.Count-ne1){throw "Offline temporal capture provenance missing: $name"}
  $expected=Get-GameplayTemporalGhostExpectedSha $artifactCapture[0]
  $ghost=Get-GameplayTemporalGhostEvidence (Join-Path $directory "$stem-raw.png") (Join-Path $directory "$stem.png") (Join-Path $directory "$stem-raw-object-id.png") (Join-Path $directory "$stem-object-id.png") (Join-Path $directory "$stem-raw-depth.png") (Join-Path $directory "$stem-depth.png") 64 64 1152 592 $expected
  if($ghost.pass-ne1){throw "Offline temporal ghost gate failed: $name"}
  $receiptProvenance=Assert-GameplayTemporalGhostPairProvenance $name $characterCapture[0] $photorealCapture[0] $sequenceCapture[0] $ghost $artifactCapture[0]
  $artifacts=@();foreach($owned in @($artifactCapture[0].artifacts)){$path=Join-Path $directory $owned.name;$actual=Get-GameplayEvidenceFileSnapshot $path;$artifacts+=[ordered]@{key=$owned.key;name=$owned.name;bytes=[uint64]$owned.bytes;actualBytes=[uint64]$actual.length;expectedSha256=$owned.sha256;actualSha256=$actual.sha256;match=if($owned.sha256-ceq$actual.sha256-and[uint64]$owned.bytes-eq[uint64]$actual.length){$true}else{$false}}}
  if(@($artifacts|Where-Object{-not$_.match}).Count){throw "Offline temporal artifact ownership failed: $name"}
  $raw=@($sequenceCapture[0].phases)[0];$effective=@($sequenceCapture[0].phases)[1]
  $captureEvidence+=[ordered]@{
    name=$name;receiptProvenance=[bool]$receiptProvenance
    raw=[ordered]@{frameSerial=[uint64]$raw.frameSerial;tokenGeneration=[uint64]$raw.tokenGeneration;viewProjectionHash=$raw.viewProjectionHash;cameraHash=$raw.cameraHash;invariantHash=$raw.invariantHash;jitterIndex=$raw.jitterIndex;jitterX=$raw.jitterX;jitterY=$raw.jitterY;historyGeneration=$raw.historyGeneration;historyIndex=$raw.historyIndex}
    effective=[ordered]@{frameSerial=[uint64]$effective.frameSerial;tokenGeneration=[uint64]$effective.tokenGeneration;viewProjectionHash=$effective.viewProjectionHash;cameraHash=$effective.cameraHash;invariantHash=$effective.invariantHash;jitterIndex=$effective.jitterIndex;jitterX=$effective.jitterX;jitterY=$effective.jitterY;historyGeneration=$effective.historyGeneration;historyIndex=$effective.historyIndex}
    metrics=[ordered]@{pass=$ghost.pass;stablePass=$ghost.stablePass;ringPass=$ghost.ringPass;tilePass=$ghost.tilePass;eligiblePixels=$ghost.eligiblePixels;coveredPixels=$ghost.coveredPixels;excludedPixels=$ghost.excludedPixels;worstTileMean=$ghost.worstTileMeanAbsoluteChannelDifference;worstTileP99=$ghost.worstTileP99MaximumChannelDifference;worstTileMaximum=$ghost.worstTileMaximumChannelDifference;worstTileSignedBias=$ghost.worstTileSignedBias;worstTileMedianBias=$ghost.worstTileMedianBias;worstTileCoherence=$ghost.worstTileCoherence;tileMinimumStablePixels=$ghost.tileMinimumStablePixels;tileCoherenceThreshold=$ghost.tileCoherenceThreshold;tile9=$ghost.tiles[9]}
    artifacts=$artifacts
  }
}

$log=[ordered]@{source='GameplayTemporalGhostOfflineEvidence';runId=$gameplay.runId;artifactReceipt=[ordered]@{absolutePath=[IO.Path]::GetFullPath($receiptPath);bytes=[uint64]$receiptSnapshot.length;sha256=$receiptSnapshot.sha256;payloadSha256=$artifactReceipt.payloadSha256;payloadVerified=$true};captures=$captureEvidence}
$parent=Split-Path -Parent $outputPath;if(-not(Test-Path -LiteralPath $parent)){New-Item -ItemType Directory -Force -Path $parent|Out-Null}
[IO.File]::WriteAllText($outputPath,($log|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
$logSnapshot=Get-GameplayEvidenceFileSnapshot $outputPath
$sidecarPath="$outputPath.receipt.json";$sidecarPayload=[ordered]@{source='GameplayTemporalGhostOfflineLogReceipt';runId=$gameplay.runId;logAbsolutePath=$outputPath;logBytes=[uint64]$logSnapshot.length;logSha256=$logSnapshot.sha256};$sidecarPayloadJson=$sidecarPayload|ConvertTo-Json -Depth 4 -Compress;$sidecar=[ordered]@{source=$sidecarPayload.source;runId=$sidecarPayload.runId;logAbsolutePath=$sidecarPayload.logAbsolutePath;logBytes=$sidecarPayload.logBytes;logSha256=$sidecarPayload.logSha256;payloadSha256=Get-GameplayEvidenceTextSha256Hex $sidecarPayloadJson}
[IO.File]::WriteAllText($sidecarPath,($sidecar|ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
Write-Output "GameplayTemporalGhostOffline PASS runId=$($gameplay.runId) log=$outputPath logBytes=$($logSnapshot.length) logSha256=$($logSnapshot.sha256) sidecar=$sidecarPath"
