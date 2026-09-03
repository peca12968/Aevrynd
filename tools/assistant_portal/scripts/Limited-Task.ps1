param([Parameter(Mandatory)][ValidateSet('Preflight','Start','Stop','RestorePreview','ResumePortal')][string]$Operation)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-StandardUserProcess
$paths=Get-PortalPaths
$result=[ordered]@{schemaVersion=1;operation=$Operation;success=$false;safeCode='not_started';completedUtc=[DateTimeOffset]::UtcNow.ToString('o')}
try{
    switch($Operation){
        'Preflight'{$null=Invoke-PortalPreflight}
        'Start'{& (Join-Path $PSScriptRoot 'Switch-ToPortal.ps1')}
        'Stop'{& (Join-Path $PSScriptRoot 'Stop-Ngrok.ps1');& (Join-Path $PSScriptRoot 'Stop-Portal.ps1')}
        'RestorePreview'{$verified=Test-InstalledPortal;Start-RecordedPreview -Verified $verified}
        'ResumePortal'{
            $null=Invoke-PortalPreflight;$journal=Read-PortalRuntimeIntent
            try{& (Join-Path $PSScriptRoot 'Start-Portal.ps1');Write-PortalRuntimeIntent -State $journal -Stage portal_started;& (Join-Path $PSScriptRoot 'Start-Ngrok.ps1');Write-PortalRuntimeIntent -State $journal -Stage committed}
            catch{try{& (Join-Path $PSScriptRoot 'Stop-Ngrok.ps1')}catch{};try{& (Join-Path $PSScriptRoot 'Stop-Portal.ps1')}catch{};Write-PortalRuntimeIntent -State $journal -Stage portal_failed;throw}
        }
    }
    $result.success=$true;$result.safeCode='ok'
}catch{$result.safeCode='operation_failed'}
finally{$result.completedUtc=[DateTimeOffset]::UtcNow.ToString('o');Write-PortalJson -Path $paths.TaskResult -Value $result;Protect-PortalStateFile $paths.TaskResult}
if(-not$result.success){exit 1}
