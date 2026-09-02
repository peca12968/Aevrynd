param([string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Funnel.Common.ps1')
$paths=Get-GatewayPaths -InstallRoot $InstallRoot
$transportLock=Enter-PublicTransportLock
try{
    $state=Read-FunnelState -Paths $paths
    if($null-eq$state){
        Test-FunnelTransportAbsent
        Write-Output 'Tailscale Funnel is absent or disabled.';exit 0
    }
    $handle=Get-FixedTailscaleExecutableIdentity;$node=Get-TailscaleNodeIdentity -Executable $handle;if($node.BackendVersion-cne$handle.ExecutableVersion){throw 'Tailscale CLI and backend versions do not match.'};$status=Get-FunnelStatusSnapshot -Executable $handle -DnsName $node.DnsName;$validated=[pscustomobject]@{Mode=$status.Mode;ExecutablePath=$handle.ExecutablePath;ExecutableSha256=$handle.ExecutableSha256;ExecutableVersion=$handle.ExecutableVersion;DnsName=$node.DnsName;NodeId=$node.NodeId;PublicKey=$node.PublicKey;TailscaleIPv4=$node.TailscaleIPv4}
    Assert-FunnelIdentityMatchesState $state $validated $validated
    if($validated.Mode-eq'disabled'){
        Remove-Item -LiteralPath $paths.FunnelStateFile -Force
        Write-Output 'Removed stale Funnel state after authoritative disabled status.';exit 0
    }
    $command=Invoke-TailscaleCli $handle @('funnel','--yes','--https=443','--set-path=/webhook','http://127.0.0.1:8787','off')
    $afterNode=Get-TailscaleNodeIdentity -Executable $handle;$afterStatus=Get-FunnelStatusSnapshot -Executable $handle -DnsName $afterNode.DnsName;$after=[pscustomobject]@{Mode=$afterStatus.Mode}
    if($command.ExitCode-ne0-and$after.Mode-ne'disabled'){throw 'Tailscale Funnel disable command failed and status remains active.'}
    if($after.Mode-ne'disabled'){throw 'Tailscale Funnel disable was not confirmed by authoritative status JSON.'}
    Remove-Item -LiteralPath $paths.FunnelStateFile -Force
    Write-Output 'Tailscale Funnel disabled and state converged.'
}finally{if($null-ne(Get-Variable handle -ErrorAction SilentlyContinue)-and$null-ne$handle){$handle.Stream.Dispose()};Exit-PublicTransportLock $transportLock}
