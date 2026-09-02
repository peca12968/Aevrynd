param([Parameter(Mandatory=$true)][string]$Executable,[ValidateSet(0,3,4,5)][int]$Warmup=0)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'GameplayLabCaptureEvidence.ps1')

function Invoke-Fixture([string]$Arguments,[hashtable]$Environment=@{}) {
  foreach($entry in $Environment.GetEnumerator()){[Environment]::SetEnvironmentVariable($entry.Key,$entry.Value,'Process')}
  try {
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=[IO.Path]::GetFullPath($Executable);$start.Arguments=$Arguments;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $process=[Diagnostics.Process]::Start($start);$process.WaitForExit();$exitCode=[int]$process.ExitCode;$process.Dispose()
  }
  finally {foreach($entry in $Environment.GetEnumerator()){[Environment]::SetEnvironmentVariable($entry.Key,$null,'Process')}}
  return $exitCode
}

function Get-Sha256([string]$Path) {
  $stream=[IO.File]::OpenRead($Path)
  try {$sha=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','')}finally{$sha.Dispose()}}
  finally {$stream.Dispose()}
}

function Assert-RenderResourceIdentity($Identity,[string]$Context) {
  $fields=@('generation','width','height','format','sampleCount','state')
  if($null-eq$Identity){throw "Render resource identity missing: $Context"}
  foreach($field in $fields){if($null-eq$Identity.PSObject.Properties[$field]){throw "Render resource identity field missing: $Context field=$field"}}
  if([uint64]$Identity.generation-lt1-or[uint32]$Identity.width-lt1-or[uint32]$Identity.height-lt1-or[uint32]$Identity.format-lt1-or[uint32]$Identity.sampleCount-lt1){throw "Render resource identity incomplete: $Context"}
  return ($fields|ForEach-Object{"$_=$($Identity.$_)"})-join';'
}

$identityFixture=[pscustomobject]@{generation=1;width=2;height=3;format=4;sampleCount=1;state=5}
$identityFixtureKey=Assert-RenderResourceIdentity $identityFixture 'fixture'
$identityMissing=$identityFixture|ConvertTo-Json|ConvertFrom-Json;$identityMissing.PSObject.Properties.Remove('format')
try{$null = Assert-RenderResourceIdentity $identityMissing 'missing mutation';throw 'Missing identity mutation unexpectedly passed'}catch{if($_.Exception.Message-eq'Missing identity mutation unexpectedly passed'){throw}}
$identityDrift=$identityFixture|ConvertTo-Json|ConvertFrom-Json;$identityDrift.generation++
if((Assert-RenderResourceIdentity $identityDrift 'drift mutation')-ceq$identityFixtureKey){throw 'Identity drift mutation unexpectedly passed'}

# CLI 必須 fail-closed；未知場景、缺少或超出 typed warmup 都不能啟動 GPU fixture。
$invalidArguments=@('--temporal-fixture unknown --warmup 3','--temporal-fixture third-day','--temporal-fixture third-day --warmup 2','--temporal-fixture third-day --warmup 6')
foreach($invalid in $invalidArguments){
  $exitCode=Invoke-Fixture $invalid
  if($exitCode -eq 0){throw "Temporal fixture accepted invalid CLI: $invalid"}
}

$root=Split-Path -Parent (Resolve-Path -LiteralPath $Executable)
$gpuValidation=[bool][Environment]::GetEnvironmentVariable('AEVRYND_GPU_VALIDATION','Process')
$validationLabel=if($gpuValidation){'gbv'}else{'normal'}
$results=@()
$warmups=if($Warmup){@($Warmup)}else{@(3,4,5)}
foreach($warmup in $warmups){
  $exitCode=Invoke-Fixture "--temporal-fixture third-day --warmup $warmup"
  if($exitCode -ne 0){throw "Temporal fixture failed: warmup=$warmup"}
  $directory=Join-Path $root "temporal-fixture-third-day-warmup-$warmup-$validationLabel"
  $receipt=Get-Content -Raw -LiteralPath (Join-Path $directory 'temporal-fixture-receipt.json')|ConvertFrom-Json
  if($receipt.source-cne'GameplayThirdDayProductionTemporalFixture'-or$receipt.capture-cne'third-day.png'-or$receipt.warmupFrames-ne$warmup-or$receipt.baselineWarmupFrames-ne3-or$receipt.baselineContract-cne'typed-third-day-warmup-v1'-or$receipt.recordedWarmupFrames-ne$warmup-or$receipt.sequenceComplete-ne1-or$receipt.phaseProvenance.Count-ne3){throw "Temporal fixture receipt failed: warmup=$warmup"}
  if($receipt.phaseProvenance[0].atmosphere.dispatch-ne1-or$receipt.phaseProvenance[0].temporal.dispatch-ne1-or$receipt.phaseProvenance[1].atmosphere.reuse-ne1-or$receipt.phaseProvenance[1].temporal.reuse-ne1-or$receipt.phaseProvenance[2].atmosphere.reuse-ne1-or$receipt.phaseProvenance[2].temporal.reuse-ne1){throw "Temporal fixture production provenance failed: warmup=$warmup"}
  $first=$receipt.phaseProvenance[0]
  $firstAtmosphereIdentity=Assert-RenderResourceIdentity $first.atmosphere.outputIdentity "warmup=$warmup phase=0 atmosphere";$firstTemporalIdentity=Assert-RenderResourceIdentity $first.temporal.outputIdentity "warmup=$warmup phase=0 temporal"
  foreach($phase in $receipt.phaseProvenance){$atmosphereIdentity=Assert-RenderResourceIdentity $phase.atmosphere.outputIdentity "warmup=$warmup phase=$($phase.phase) atmosphere";$temporalIdentity=Assert-RenderResourceIdentity $phase.temporal.outputIdentity "warmup=$warmup phase=$($phase.phase) temporal";if($phase.atmosphere.fingerprint-ne$first.atmosphere.fingerprint-or$atmosphereIdentity-cne$firstAtmosphereIdentity-or$phase.atmosphere.fence-ne$first.atmosphere.fence-or$phase.atmosphere.reject-ne0-or$phase.temporal.fingerprint-ne$first.temporal.fingerprint-or$temporalIdentity-cne$firstTemporalIdentity-or$phase.temporal.fence-ne$first.temporal.fence-or$phase.temporal.reject-ne0-or$phase.atmosphere.fence-eq0-or$phase.temporal.fence-eq0){throw "Temporal fixture production identity/fence drifted: warmup=$warmup phase=$($phase.phase)"}}
  if((Get-Sha256 (Join-Path $directory 'third-day-raw.png'))-cne(Get-Sha256 (Join-Path $directory 'third-day.png'))){throw "Temporal fixture phase beauty was not exact: warmup=$warmup validation=$validationLabel"}
  $ghost=Get-GameplayTemporalGhostEvidence (Join-Path $directory 'third-day-raw.png') (Join-Path $directory 'third-day.png') (Join-Path $directory 'third-day-raw-object-id.png') (Join-Path $directory 'third-day-object-id.png') (Join-Path $directory 'third-day-raw-depth.png') (Join-Path $directory 'third-day-depth.png') 64 64 1152 592
  if($ghost.pass-ne1){throw "Temporal fixture threshold8 gate failed: warmup=$warmup"}
  $results+=[ordered]@{warmup=$warmup;mean=$ghost.meanAbsoluteChannelDifference;p99=$ghost.p99MaximumChannelDifference;maximum=$ghost.maximumChannelDifference;pixelsOver8=$ghost.stableLevels[0].pixels;connectedOver8=$ghost.stableLevels[0].maximumConnectedPixels;excessOver8=$ghost.stableLevels[0].normalizedExcessEnergy}
}
$results|ConvertTo-Json -Compress
