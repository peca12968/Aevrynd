param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'GameplayLabCaptureEvidence.ps1')
$d=[ordered]@{schemaVersion=2}
foreach($n in @('epoch','downgradeCount','requested','actual','reason','phase','firstFalseReason','gpu_epoch','gpu_observationIndex','gpu_submittedFrameSerial','gpu_completedFrameSerial','gpu_timingSampleSerial','gpu_lastGpuFrameNanoseconds','gpu_telemetryAgeNanoseconds','gpu_dedicatedVideoMemoryBytes','gpu_currentVideoMemoryUsageBytes','gpu_rendererAdapterLuidLow','gpu_rendererAdapterLuidHigh','gpu_telemetryAdapterLuidLow','gpu_telemetryAdapterLuidHigh','gpu_hardVramBudgetBytes','gpu_pendingGpuReleaseBytes','gpu_debugErrorCount','gpu_debugWarningCount','gpu_stableFrames','gpu_requiredStableFrames','gpu_timingProvenance','terminalOutcome','terminalReason','terminalEpoch')){$d[$n]=0}
foreach($n in @('requestActive','noOp','applyAttempted','applySucceeded','postApplyPassed','rollbackAttempted','rollbackSucceeded','rendererApplied','transitionPolicyApplied','shadowResourcesApplied','volumetricResolutionApplied','cloudSamplesApplied','cloudShadowSamplesApplied','lightShaftSamplesApplied','vegetationDensityApplied','textureBudgetApplied','meshBudgetApplied','gpu_observed','gpu_resourcesTrue','gpu_telemetryTrue','gpu_rendererAdapterIdentityValid','gpu_adapterIdentityTrue','gpu_validationTrue','gpu_timingRecorded','gpu_timingSerialAligned','gpu_productionFrameBudgetApplied','gpu_frameBudgetTrue','gpu_fresh','gpu_framePassed','gpu_committed')){$d[$n]=$false}
foreach($n in @('requestedFingerprint','normalizedFingerprint','actualFingerprint')){$d[$n]='0'}
$value=($d|ConvertTo-Json -Compress|ConvertFrom-Json)
foreach($n in @('renderer_completedFrameSerial','renderer_totalNanoseconds','renderer_scalarSerial','renderer_lastVolumetricAtmosphereGpuNanoseconds','renderer_lastHzbPrepassGpuNanoseconds','renderer_lastHzbBuildGpuNanoseconds','renderer_lastHzbCullGpuNanoseconds','renderer_lastHzbColorGpuNanoseconds','renderer_shadow_serial','renderer_shadow_beginTick','renderer_shadow_endTick','renderer_shadow_frequency','renderer_shadow_nanoseconds','renderer_captureCopy_serial','renderer_captureCopy_beginTick','renderer_captureCopy_endTick','renderer_captureCopy_frequency','renderer_captureCopy_nanoseconds')){$d[$n]=0}
foreach($n in @('renderer_scalarAlignedValid','renderer_shadow_recorded','renderer_shadow_valid','renderer_shadow_alignedValid','renderer_captureCopy_recorded','renderer_captureCopy_valid','renderer_captureCopy_alignedValid')){$d[$n]=$false}
$value=($d|ConvertTo-Json -Compress|ConvertFrom-Json)
$null=Assert-GameplayUltraDiagnosticSchema $value
$value.gpu_lastGpuFrameNanoseconds=20000000;$value.firstFalseReason=8
$null=Assert-GameplayUltraDiagnosticSchema $value
# 診斷必須保留失敗數值，不能將效能失敗轉成 schema 失敗。 (raw truth).
foreach($case in @('missing','string-bool','fraction','negative','fingerprint-overflow','extra')){
  $bad=$value|ConvertTo-Json -Compress|ConvertFrom-Json
  switch($case){
    'missing'{$bad.PSObject.Properties.Remove('gpu_frameBudgetTrue')}
    'string-bool'{$bad.gpu_frameBudgetTrue='false'}
    'fraction'{$bad.gpu_lastGpuFrameNanoseconds=1.5}
    'negative'{$bad.gpu_pendingGpuReleaseBytes=-1}
    'fingerprint-overflow'{$bad.actualFingerprint='18446744073709551616'}
    'extra'{$bad|Add-Member NoteProperty unexpected 0}
  }
  $rejected=$false;try{$null=Assert-GameplayUltraDiagnosticSchema $bad}catch{$rejected=$true}
  if(-not$rejected){throw "Ultra diagnostic hostile schema accepted: $case"}
}
$value.renderer_completedFrameSerial=157;$value.renderer_scalarSerial=157;$value.renderer_scalarAlignedValid=$true
$value.renderer_shadow_serial=157;$value.renderer_shadow_beginTick=100;$value.renderer_shadow_endTick=200;$value.renderer_shadow_frequency=1000000;$value.renderer_shadow_nanoseconds=100000;$value.renderer_shadow_recorded=$true;$value.renderer_shadow_valid=$true;$value.renderer_shadow_alignedValid=$true
$null=Assert-GameplayUltraDiagnosticSchema $value
# 原始有效但屬於舊幀的資料可記錄，不能標成已對齊。 (stale diagnostics).
$value.renderer_shadow_serial=156;$value.renderer_shadow_alignedValid=$false
$null=Assert-GameplayUltraDiagnosticSchema $value
foreach($case in @('stale-aligned','scalar-stale','zero-frequency','reversed-ticks','unrecorded-time','false-raw-valid')){
  $bad=$value|ConvertTo-Json -Compress|ConvertFrom-Json
  switch($case){
    'stale-aligned'{$bad.renderer_shadow_alignedValid=$true}
    'scalar-stale'{$bad.renderer_scalarSerial=156}
    'zero-frequency'{$bad.renderer_shadow_frequency=0}
    'reversed-ticks'{$bad.renderer_shadow_endTick=99}
    'unrecorded-time'{$bad.renderer_shadow_recorded=$false}
    'false-raw-valid'{$bad.renderer_shadow_serial=157;$bad.renderer_shadow_valid=$false;$bad.renderer_shadow_alignedValid=$true}
  }
  $rejected=$false;try{$null=Assert-GameplayUltraDiagnosticSchema $bad}catch{$rejected=$true}
  if(-not$rejected){throw "Ultra timing hostile schema accepted: $case"}
}
Write-Output 'GameplayUltraDiagnosticSchema PASS raw-failure-preserved=1 stale-preserved=1 hostile=12'
