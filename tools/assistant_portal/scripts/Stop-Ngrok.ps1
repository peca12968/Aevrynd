Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
$verified=Test-InstalledPortal -RecoveryOnly;$paths=$verified.Paths;$install=$verified.Install
$transportLock=Enter-PublicTransportLock
try{
$state=Read-PortalRuntimeState -Path $paths.NgrokState -Kind ngrok;if($null-eq$state){Write-Output 'Portal ngrok is not running.';return}
$process=Get-OwnedProcess -State $state -ExpectedExecutable $install.ngrokExe -ExpectedCommandLine (Get-NgrokCommandLine $verified)
if($null-ne$process){$process.Kill($true);$process.WaitForExit()}
Remove-Item -LiteralPath $paths.NgrokState -Force
Write-Output 'Portal ngrok stopped.'
}finally{Exit-PortalSwitchLock $transportLock}
