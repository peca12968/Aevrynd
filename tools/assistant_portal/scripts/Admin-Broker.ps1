param([Parameter(Mandatory)][ValidateSet('Prepare','Commit','Recover','Restore')][string]$Operation)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-Administrator

function Restore-BaselineElevated([object]$Journal){
    try{
        Set-LineBaseline -GatewayRunning ([bool]$Journal.baseline.lineGatewayRunning) -NgrokRunning ([bool]$Journal.baseline.lineNgrokRunning) -RecoveryMode
        Write-PortalSwitchState -State $Journal -Stage rollback_complete
        $intent=[pscustomobject]@{schemaVersion=1;transactionId=[string]$Journal.transactionId;stage='rollback_complete';previewRunning=[bool]$Journal.baseline.previewRunning;updatedUtc=[DateTimeOffset]::UtcNow.ToString('o')};Write-PortalRuntimeIntent -State $intent -Stage rollback_complete
    }catch{
        try{Set-LineBaseline -GatewayRunning $false -NgrokRunning $false -RecoveryMode}catch{}
        Write-PortalSwitchState -State $Journal -Stage restore_failed
        throw
    }
}

$lock=Enter-PortalSwitchLock
try{
    # crash recovery 永遠先讀 journal；不依賴 Python、SMTP、ngrok 或 portal 健康。
    $journal=Read-PortalSwitchState
    if($Operation-eq'Commit'){
        if($null-eq$journal-or$journal.stage-ne'line_stopped'){throw 'Privileged transaction is not ready to commit.'}
        $intent=Read-PortalRuntimeIntent
        if($null-eq$intent-or$intent.stage-ne'committed'-or$intent.transactionId-cne$journal.transactionId){throw 'Limited runtime intent does not match the privileged transaction.'}
        $verified=Test-InstalledPortal;$portalState=Read-PortalRuntimeState -Path $verified.Paths.PortalState -Kind portal;$ngrokState=Read-PortalRuntimeState -Path $verified.Paths.NgrokState -Kind ngrok
        $null=Assert-PortalRuntime -Verified $verified -State $portalState;$null=Assert-NgrokRuntime -Verified $verified -State $ngrokState -InstanceNonce ([string]$portalState.instanceNonce)
        Write-PortalSwitchState -State $journal -Stage committed
        return
    }
    if($Operation-eq'Recover'){
        if($null-eq$journal-or$journal.stage-in@('committed','rollback_complete')){return}
        if($journal.stage-in@('portal_started','ngrok_started','portal_failed')){Invoke-LimitedPortalTask -Operation Stop -AllowFailure;Assert-PortalStackStoppedForRecovery}
        Restore-BaselineElevated $journal
        return
    }
    if($Operation-eq'Prepare'){
        if($null-ne$journal-and$journal.stage-notin@('committed','rollback_complete')){Restore-BaselineElevated $journal}
        if($null-ne$journal-and$journal.stage-eq'committed'){Write-Output 'Portal transaction is already committed.';return}
        $truth=Get-LineServiceTruth
        $previewRunning=@(Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort -ErrorAction SilentlyContinue).Count-gt0
        $journal=[pscustomobject]@{schemaVersion=1;transactionId=[Guid]::NewGuid().ToString('N');stage='prepared';updatedUtc=[DateTimeOffset]::UtcNow.ToString('o');baseline=[pscustomobject]@{lineGatewayRunning=[bool]$truth.GatewayRunning;lineNgrokRunning=[bool]$truth.NgrokRunning;previewRunning=$previewRunning}}
        Write-PortalSwitchState -State $journal -Stage prepared
        try{
            Write-PortalSwitchState -State $journal -Stage line_stopping
            Set-LineBaseline -GatewayRunning $false -NgrokRunning $false
            Write-PortalSwitchState -State $journal -Stage line_stopped
            $intent=[pscustomobject]@{schemaVersion=1;transactionId=[string]$journal.transactionId;stage='line_stopped';previewRunning=[bool]$journal.baseline.previewRunning;updatedUtc=[DateTimeOffset]::UtcNow.ToString('o')};Write-PortalRuntimeIntent -State $intent -Stage line_stopped
        }catch{Restore-BaselineElevated $journal;throw}
        return
    }
    if($null-eq$journal){throw 'No portal transaction journal exists.'}
    if($journal.stage-eq'rollback_complete'){Disable-PortalStartupTask;return}
    Invoke-LimitedPortalTask -Operation Stop -AllowFailure
    Assert-PortalStackStoppedForRecovery
    try{Restore-BaselineElevated $journal}
    catch{
        # Restore 部分失敗時先回到「LINE 全停」，再 best-effort 恢復原 portal；兩邊不並存。
        try{
            Set-LineBaseline -GatewayRunning $false -NgrokRunning $false -RecoveryMode
            Write-PortalSwitchState -State $journal -Stage line_stopped
            $result=Invoke-LimitedPortalTask -Operation ResumePortal -AllowFailure
            if($null-eq$result-or-not$result.success){Write-PortalSwitchState -State $journal -Stage restore_failed}
        }catch{Write-PortalSwitchState -State $journal -Stage restore_failed}
        throw
    }
    Disable-PortalStartupTask
    if($journal.baseline.previewRunning){Invoke-LimitedPortalTask -Operation RestorePreview -AllowFailure}
}finally{Exit-PortalSwitchLock $lock}
