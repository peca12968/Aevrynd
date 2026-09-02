param([string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Funnel.Common.ps1')
$paths=Get-GatewayPaths -InstallRoot $InstallRoot
$transportLock=Enter-PublicTransportLock
try{
    $null=Test-InstallationManifest -Paths $paths
    if(Test-Path -LiteralPath $paths.TunnelStateFile){throw 'Cloudflare tunnel state exists; public transports are mutually exclusive.'}
    $cloudflared=@(Get-CimInstance Win32_Process -Filter "Name = 'cloudflared.exe'" -ErrorAction SilentlyContinue)
    if($cloudflared.Count-ne0){throw 'A cloudflared process exists; refusing simultaneous public transports.'}
    $handle=Get-FixedTailscaleExecutableIdentity
    $node=Get-TailscaleNodeIdentity -Executable $handle
    if($node.BackendVersion-cne$handle.ExecutableVersion){throw 'Tailscale CLI and backend versions do not match.'}
    $status=Get-FunnelStatusSnapshot -Executable $handle -DnsName $node.DnsName
    $validated=[pscustomobject]@{Mode=$status.Mode;ExecutablePath=$handle.ExecutablePath;ExecutableSha256=$handle.ExecutableSha256;ExecutableVersion=$handle.ExecutableVersion;DnsName=$node.DnsName;NodeId=$node.NodeId;PublicKey=$node.PublicKey;TailscaleIPv4=$node.TailscaleIPv4}
    if($validated.Mode-ne'disabled'){throw 'Tailscale Funnel is already configured.'}
    $existing=Read-FunnelState -Paths $paths
    if($null-ne$existing){throw 'Funnel state already exists; run Stop-Funnel.ps1 first.'}
    $launchNonce=[Guid]::NewGuid().ToString('N');$created=[DateTimeOffset]::UtcNow.ToString('o')
    Write-FunnelState -Paths $paths -Status pending -Executable $validated -Node $validated -LaunchNonce $launchNonce -CreatedUtc $created
    $command=Invoke-TailscaleCli $handle @('funnel','--bg','--yes','--https=443','--set-path=/webhook','http://127.0.0.1:8787')
    $afterNode=Get-TailscaleNodeIdentity -Executable $handle;if($afterNode.BackendVersion-cne$handle.ExecutableVersion){throw 'Tailscale backend version changed during activation.'};$afterStatus=Get-FunnelStatusSnapshot -Executable $handle -DnsName $afterNode.DnsName;$after=[pscustomobject]@{Mode=$afterStatus.Mode;ExecutablePath=$handle.ExecutablePath;ExecutableSha256=$handle.ExecutableSha256;ExecutableVersion=$handle.ExecutableVersion;DnsName=$afterNode.DnsName;NodeId=$afterNode.NodeId;PublicKey=$afterNode.PublicKey;TailscaleIPv4=$afterNode.TailscaleIPv4}
    if($command.ExitCode-ne0-or$after.Mode-ne'active'){
        if($after.Mode-eq'disabled'){Remove-Item -LiteralPath $paths.FunnelStateFile -Force}
        throw 'Tailscale Funnel activation was not confirmed by authoritative status JSON.'
    }
    $pending=Read-FunnelState -Paths $paths
    if($pending.status-ne'pending'-or$pending.launchNonce-cne$launchNonce){throw 'Pending Funnel activation handshake failed.'}
    Assert-FunnelIdentityMatchesState $pending $after $after
    Write-FunnelState -Paths $paths -Status active -Executable $after -Node $after -LaunchNonce $launchNonce -CreatedUtc $created -ActivatedUtc ([DateTimeOffset]::UtcNow.ToString('o'))
    Write-Output "Tailscale Funnel verified: https://$($after.DnsName)/webhook"
}finally{if($null-ne(Get-Variable handle -ErrorAction SilentlyContinue)-and$null-ne$handle){$handle.Stream.Dispose()};Exit-PublicTransportLock $transportLock}
