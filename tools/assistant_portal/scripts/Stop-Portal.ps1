Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
$verified=Test-InstalledPortal -RecoveryOnly;$paths=$verified.Paths;$install=$verified.Install
$state=Read-PortalRuntimeState -Path $paths.PortalState -Kind portal;if($null-eq$state){Write-Output 'Portal is not running.';return}
$process=Get-OwnedProcess -State $state -ExpectedExecutable $install.pythonExe -ExpectedCommandLine (Get-PortalCommandLine $verified)
if($null-ne$process){$process.Kill($true);$process.WaitForExit()}
Remove-Item -LiteralPath $paths.PortalState -Force
Write-Output 'Portal stopped.'
