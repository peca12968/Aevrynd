Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-StandardUserProcess
$verified=Test-InstalledPortal;$paths=$verified.Paths;$install=$verified.Install
$transportLock=Enter-PublicTransportLock
try{
$portalState=Read-PortalRuntimeState -Path $paths.PortalState -Kind portal
if($null-eq$portalState){throw 'A verified managed production portal must be running before ngrok.'}
$null=Assert-PortalRuntime -Verified $verified -State $portalState
if(Test-Path -LiteralPath $paths.NgrokState){$old=Read-PortalRuntimeState -Path $paths.NgrokState -Kind ngrok;$owned=Get-OwnedProcess -State $old -ExpectedExecutable $install.ngrokExe -ExpectedCommandLine (Get-NgrokCommandLine $verified);if($null-ne$owned){$null=Assert-NgrokRuntime -Verified $verified -State $old -InstanceNonce ([string]$portalState.instanceNonce);Write-Output 'Portal ngrok is already running.';return};Remove-Item -LiteralPath $paths.NgrokState -Force}
if(@(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue).Count-ne0){throw 'Another ngrok process exists; refusing ambiguous public routing.'}
Add-PortalCredentialInterop
$arguments=@('http',"http://127.0.0.1:$script:PortalPort","--domain=$script:PortalDomain",'--scheme=https','--inspect=false','--log=false')
$notBefore=[DateTimeOffset]::UtcNow
$launchIntent=[ordered]@{schemaVersion=1;kind='ngrok';notBeforeUtc=$notBefore.ToString('o');expectedExecutable=[string]$install.ngrokExe;expectedCommandLine=(Get-NgrokCommandLine $verified)};Write-PortalJson -Path $paths.LaunchIntent -Value $launchIntent;Protect-PortalStateFile $paths.LaunchIntent
$process=$null
try{$process=[AevryndPortalCredential]::StartNgrok([string]$install.ngrokExe,$arguments,$script:NgrokCredentialTarget)
Invoke-ManagedProcessLaunchGuard -Process $process -Operation {
Assert-StartedOwnedProcess -Process $process -ExpectedExecutable $install.ngrokExe -ExpectedCommandLine (Get-NgrokCommandLine $verified) -NotBefore $notBefore
Write-PortalJson -Path $paths.NgrokState -Value ([ordered]@{schemaVersion=1;pid=$process.Id;startedUtc=([DateTimeOffset]::new($process.StartTime.ToUniversalTime())).ToString('o')});Protect-PortalStateFile $paths.NgrokState
$healthy=$false
for($attempt=0;$attempt-lt30;$attempt++){if($process.HasExited){break};try{
    $currentState=Read-PortalRuntimeState -Path $paths.NgrokState -Kind ngrok
    $null=Assert-NgrokRuntime -Verified $verified -State $currentState -InstanceNonce ([string]$portalState.instanceNonce)
    $healthy=$true;break
}catch{};Start-Sleep -Milliseconds 500}
if(-not$healthy){throw 'Public portal ngrok API, URL, origin, or instance marker verification failed.'}
} -Cleanup {Remove-Item -LiteralPath $paths.NgrokState -Force -ErrorAction SilentlyContinue}
Remove-Item -LiteralPath $paths.LaunchIntent -Force -ErrorAction SilentlyContinue
}catch{if($null-eq$process){Stop-ProtectedPortalLaunchTree -Verified $verified -NotBefore $notBefore};Remove-Item -LiteralPath $paths.LaunchIntent -Force -ErrorAction SilentlyContinue;throw}
Write-Output "Public portal verified: https://$script:PortalDomain/"
}finally{Exit-PortalSwitchLock $transportLock}
