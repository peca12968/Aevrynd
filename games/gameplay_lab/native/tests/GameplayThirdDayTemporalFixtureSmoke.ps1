param(
  [Parameter(Mandatory=$true)][string]$Executable,
  [Parameter(Mandatory=$true)][string]$EvidenceHelper,
  [ValidateSet(0,3,4,5)][int]$Warmup=0)
$ErrorActionPreference='Stop'

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
  # Fixture 可刻意覆寫 3/4/5 幀以驗證排程不變性；正式 capture baseline 仍固定為 5。
  if($receipt.source-cne'GameplayThirdDayProductionTemporalFixture'-or$receipt.capture-cne'third-day.png'-or$receipt.warmupFrames-ne$warmup-or$receipt.baselineWarmupFrames-ne5-or$receipt.baselineContract-cne'typed-third-day-warmup-v1'-or$receipt.recordedWarmupFrames-ne$warmup-or$receipt.sequenceComplete-ne1-or$receipt.phaseProvenance.Count-ne3){throw "Temporal fixture receipt failed: warmup=$warmup"}
  if($receipt.phaseProvenance[0].atmosphere.dispatch-ne1-or$receipt.phaseProvenance[0].temporal.dispatch-ne1-or$receipt.phaseProvenance[1].atmosphere.reuse-ne1-or$receipt.phaseProvenance[1].temporal.reuse-ne1-or$receipt.phaseProvenance[2].atmosphere.reuse-ne1-or$receipt.phaseProvenance[2].temporal.reuse-ne1){throw "Temporal fixture production provenance failed: warmup=$warmup"}
  $first=$receipt.phaseProvenance[0]
  $firstAtmosphereIdentity=Assert-RenderResourceIdentity $first.atmosphere.outputIdentity "warmup=$warmup phase=0 atmosphere";$firstTemporalIdentity=Assert-RenderResourceIdentity $first.temporal.outputIdentity "warmup=$warmup phase=0 temporal"
  foreach($phase in $receipt.phaseProvenance){$atmosphereIdentity=Assert-RenderResourceIdentity $phase.atmosphere.outputIdentity "warmup=$warmup phase=$($phase.phase) atmosphere";$temporalIdentity=Assert-RenderResourceIdentity $phase.temporal.outputIdentity "warmup=$warmup phase=$($phase.phase) temporal";if($phase.atmosphere.fingerprint-ne$first.atmosphere.fingerprint-or$atmosphereIdentity-cne$firstAtmosphereIdentity-or$phase.atmosphere.fence-ne$first.atmosphere.fence-or$phase.atmosphere.reject-ne0-or$phase.temporal.fingerprint-ne$first.temporal.fingerprint-or$temporalIdentity-cne$firstTemporalIdentity-or$phase.temporal.fence-ne$first.temporal.fence-or$phase.temporal.reject-ne0-or$phase.atmosphere.fence-eq0-or$phase.temporal.fence-eq0){throw "Temporal fixture production identity/fence drifted: warmup=$warmup phase=$($phase.phase)"}}
  $presentationReceipt=Get-Content -Raw -LiteralPath (Join-Path $directory 'temporal-fixture-presentation-receipt.json')|ConvertFrom-Json
  if($presentationReceipt.source-cne'GameplayThirdDayPresentationParity'-or$presentationReceipt.capture-cne'third-day.png'-or$presentationReceipt.diagnosticCapture-cne'third-day-diagnostic.png'-or$presentationReceipt.logicalToken-lt1-or$presentationReceipt.frameSerial-eq$presentationReceipt.diagnosticFrameSerial-or$presentationReceipt.cameraHash-lt1-or$presentationReceipt.stateHash-lt1-or$presentationReceipt.invariantHash-lt1-or$presentationReceipt.paired-ne1-or$presentationReceipt.msaaEnabled-ne1-or$presentationReceipt.taaEnabled-ne1-or$presentationReceipt.historyUsed-ne1-or$presentationReceipt.warmupFrames-ne$warmup){throw "Temporal fixture presentation parity failed: warmup=$warmup validation=$validationLabel"}
  if((Get-Sha256 (Join-Path $directory 'third-day-raw.png'))-ceq(Get-Sha256 (Join-Path $directory 'third-day.png'))-or(Get-Sha256 (Join-Path $directory 'third-day-diagnostic.png'))-ceq(Get-Sha256 (Join-Path $directory 'third-day.png'))){throw "Temporal fixture presentation reused a diagnostic raster: warmup=$warmup validation=$validationLabel"}
  # 診斷 ghost gate 必須比較同類型 semantic raster；presentation 另以 AA/TAA receipt 驗證。
  # 程序結束後影像已封存；在乾淨的 Windows PowerShell 程序分析一次，
  # 避免大型布林陣列受本測試程序先前的子程序狀態影響，也不以輪詢掩蓋缺失證據。
  # 維持左右安全邊界並涵蓋完整垂直畫面，避免茂密樹冠讓局部格只剩物件鄰接環而缺少穩定背景樣本。
  if([string]::IsNullOrWhiteSpace($EvidenceHelper)-or-not(Test-Path -LiteralPath $EvidenceHelper)){throw 'Temporal ghost evidence helper path was lost'}
  $evidencePaths=@('third-day-raw.png','third-day-diagnostic.png','third-day-raw-object-id.png','third-day-object-id.png','third-day-raw-depth.png','third-day-depth.png')|ForEach-Object{(Join-Path $directory $_).Replace("'","''")}
  $analysis=". '$EvidenceHelper'; Get-GameplayTemporalGhostEvidence '$($evidencePaths[0])' '$($evidencePaths[1])' '$($evidencePaths[2])' '$($evidencePaths[3])' '$($evidencePaths[4])' '$($evidencePaths[5])' 64 0 1152 720 | ConvertTo-Json -Depth 8 -Compress"
  $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($analysis))
  $ghostJson=& (Join-Path $PSHOME 'powershell.exe') -NoProfile -ExecutionPolicy Bypass -EncodedCommand $encoded
  if($LASTEXITCODE-ne0-or[string]::IsNullOrWhiteSpace($ghostJson)){throw "Temporal ghost evidence process failed: warmup=$warmup"}
  $ghost=$ghostJson|ConvertFrom-Json
  if($null-eq$ghost){throw "Temporal ghost evidence did not produce a result: warmup=$warmup"}
  if($ghost.pass-ne1){throw "Temporal fixture threshold8 gate failed: warmup=$warmup"}
  $results+=[ordered]@{warmup=$warmup;mean=$ghost.meanAbsoluteChannelDifference;p99=$ghost.p99MaximumChannelDifference;maximum=$ghost.maximumChannelDifference;pixelsOver8=$ghost.stableLevels[0].pixels;connectedOver8=$ghost.stableLevels[0].maximumConnectedPixels;excessOver8=$ghost.stableLevels[0].normalizedExcessEnergy}
}
$results|ConvertTo-Json -Compress
