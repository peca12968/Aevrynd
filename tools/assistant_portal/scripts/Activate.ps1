Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-Administrator
# Elevated 階段只管理受保護腳本、Credential Manager 與排程，不執行 AppData Python/import。
& (Join-Path $PSScriptRoot 'Admin-Broker.ps1') -Operation Recover
& (Join-Path $PSScriptRoot 'Register-Startup.ps1')
& (Join-Path $PSScriptRoot 'Set-GmailCredential.ps1')
& (Join-Path $PSScriptRoot 'Set-NgrokApiCredential.ps1')
Invoke-LimitedPortalTask -Operation Preflight
& (Join-Path $PSScriptRoot 'Admin-Broker.ps1') -Operation Prepare
try{Invoke-LimitedPortalTask -Operation Start;& (Join-Path $PSScriptRoot 'Admin-Broker.ps1') -Operation Commit}
catch{$cause=$_.Exception.Message;try{& (Join-Path $PSScriptRoot 'Admin-Broker.ps1') -Operation Restore}catch{throw "Activation failed ($cause); privileged rollback also failed: $($_.Exception.Message)"};throw "Activation failed and rollback completed: $cause"}
Write-Output "Activation complete: https://$script:PortalDomain/"
