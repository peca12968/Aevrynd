Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-StandardUserProcess
$lock=Enter-PortalSwitchLock
try{
    # journal 是第一個讀取的 durable truth；Limited 程序絕不停止 elevated LINE。
    $journal=Read-PortalRuntimeIntent
    if($null-eq$journal){throw 'Administrator broker must prepare the public transport transaction first.'}
    if($journal.stage-in@('prepared','line_stopping','restore_failed','portal_failed')){throw 'Administrator recovery is required before portal startup.'}
    if($journal.stage-eq'rollback_complete'){throw 'The portal transaction has already been rolled back.'}
    if($journal.stage-notin@('line_stopped','preview_stopped','portal_started','ngrok_started','committed')){throw 'Portal switch journal stage is not startable.'}
    $preflight=Invoke-PortalPreflight
    try{
        if($journal.stage-eq'line_stopped'-and[int]$preflight.PreviewPid-gt0){
            $preview=Get-VerifiedPreviewProcess -Verified $preflight.Verified -ProcessId ([int]$preflight.PreviewPid)
            $preview.Kill($true);$preview.WaitForExit()
            Write-PortalRuntimeIntent -State $journal -Stage preview_stopped
        }
        & (Join-Path $PSScriptRoot 'Start-Portal.ps1')
        Write-PortalRuntimeIntent -State $journal -Stage portal_started
        & (Join-Path $PSScriptRoot 'Start-Ngrok.ps1')
        Write-PortalRuntimeIntent -State $journal -Stage ngrok_started
        Write-PortalRuntimeIntent -State $journal -Stage committed
        Write-Output "Portal switch committed: https://$script:PortalDomain/"
    }catch{
        try{& (Join-Path $PSScriptRoot 'Stop-Ngrok.ps1')}catch{}
        try{& (Join-Path $PSScriptRoot 'Stop-Portal.ps1')}catch{}
        if($journal.previewRunning){try{Start-RecordedPreview -Verified $preflight.Verified}catch{}}
        Write-PortalRuntimeIntent -State $journal -Stage portal_failed
        throw
    }
}finally{Exit-PortalSwitchLock $lock}
