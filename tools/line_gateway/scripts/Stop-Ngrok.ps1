param([string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1');. (Join-Path $PSScriptRoot 'Funnel.Common.ps1');. (Join-Path $PSScriptRoot 'Ngrok.Common.ps1')
$paths=Get-GatewayPaths $InstallRoot;$lock=Enter-PublicTransportLock
try{$executable=Get-NgrokExecutableIdentity $PSScriptRoot;try{Resolve-NgrokPendingLaunch $paths $executable;if(-not(Test-Path $paths.NgrokStateFile)){Write-Output 'Ngrok state is absent.';exit 0};$state=Read-NgrokState $paths;$process=Assert-NgrokProcessIdentity $state $executable;$process.Kill($true);$process.WaitForExit()}finally{if($null-ne$executable){$executable.Stream.Dispose()}};Remove-Item $paths.NgrokStateFile -Force;Write-Output 'Ngrok endpoint stopped.'}finally{Exit-PublicTransportLock $lock}
