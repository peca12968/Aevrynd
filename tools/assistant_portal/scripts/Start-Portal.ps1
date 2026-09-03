Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-StandardUserProcess
$verified = Test-InstalledPortal; $paths=$verified.Paths; $install=$verified.Install
$expectedCommandLine=Get-PortalCommandLine $verified
if (Test-Path -LiteralPath $paths.PortalState) {
    $old=Read-PortalRuntimeState -Path $paths.PortalState -Kind portal
    $owned=Get-OwnedProcess -State $old -ExpectedExecutable $install.pythonExe -ExpectedCommandLine $expectedCommandLine
    if($null-ne$owned){$null=Assert-PortalRuntime -Verified $verified -State $old;Write-Output 'Portal is already running.';return}
    Remove-Item -LiteralPath $paths.PortalState -Force
}
if ((Get-NetTCPConnection -LocalAddress '127.0.0.1' -LocalPort $script:PortalPort -State Listen -ErrorAction SilentlyContinue)) { throw 'Portal loopback port is already in use.' }
Add-PortalCredentialInterop
$nonce=([Guid]::NewGuid().ToString('N'))
$arguments = @('-B',(Join-Path $paths.App 'server.py'),'--host','127.0.0.1','--port',[string]$script:PortalPort,'--production','--public-host',$script:PortalDomain,'--trusted-proxy-source','127.0.0.1')
$notBefore=[DateTimeOffset]::UtcNow
$launchIntent=[ordered]@{schemaVersion=1;kind='portal';notBeforeUtc=$notBefore.ToString('o');expectedExecutable=[string]$install.pythonExe;expectedCommandLine=$expectedCommandLine};Write-PortalJson -Path $paths.LaunchIntent -Value $launchIntent;Protect-PortalStateFile $paths.LaunchIntent
$process=$null
try{$process = [AevryndPortalCredential]::StartPortal([string]$install.pythonExe,$arguments,$script:GmailCredentialTarget,$nonce,$paths.SmtpDiagnostic)
try { $process.PriorityClass=[Diagnostics.ProcessPriorityClass]::BelowNormal } catch { }
Invoke-ManagedProcessLaunchGuard -Process $process -Operation {
    Assert-StartedOwnedProcess -Process $process -ExpectedExecutable $install.pythonExe -ExpectedCommandLine $expectedCommandLine -NotBefore $notBefore
    $state=[ordered]@{schemaVersion=1;pid=$process.Id;startedUtc=([DateTimeOffset]::new($process.StartTime.ToUniversalTime())).ToString('o');instanceNonce=$nonce}
    Write-PortalJson -Path $paths.PortalState -Value $state;Protect-PortalStateFile $paths.PortalState
    $healthy=$false;$safeStage='listener_pending'
    for($attempt=0;$attempt-lt40;$attempt++){
        if($process.HasExited){$safeStage="process_exited_$($process.ExitCode)";break}
        try{$current=Read-PortalRuntimeState -Path $paths.PortalState -Kind portal;$null=Assert-PortalRuntime -Verified $verified -State $current;$healthy=$true;break}
        catch{
            $message=$_.Exception.Message
            $safeStage=if($message-match'listener'){'listener_pending'}elseif($message-match'marker|security response'){'http_marker_pending'}elseif($message-match'command|identity|start time'){'process_identity_failed'}else{'runtime_check_pending'}
        }
        Start-Sleep -Milliseconds 250
    }
    if(-not$healthy){throw "Portal production health failed at safe stage: $safeStage"}
} -Cleanup {Remove-Item -LiteralPath $paths.PortalState -Force -ErrorAction SilentlyContinue}
Remove-Item -LiteralPath $paths.LaunchIntent -Force -ErrorAction SilentlyContinue
}catch{if($null-eq$process){Stop-ProtectedPortalLaunchTree -Verified $verified -NotBefore $notBefore};Remove-Item -LiteralPath $paths.LaunchIntent -Force -ErrorAction SilentlyContinue;throw}
Write-Output "Portal started on 127.0.0.1:$script:PortalPort."
