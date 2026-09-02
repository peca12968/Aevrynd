param([string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Funnel.Common.ps1')
$paths=Get-GatewayPaths -InstallRoot $InstallRoot
$executable=Get-FixedTailscaleExecutableIdentity
try{
$node=Get-TailscaleNodeIdentity -Executable $executable
if($node.BackendVersion-cne$executable.ExecutableVersion){throw 'Tailscale CLI and backend versions do not match.'}
$status=Get-FunnelStatusSnapshot -Executable $executable -DnsName $node.DnsName
[pscustomobject]@{Mode=$status.Mode;ExecutablePath=$executable.ExecutablePath;ExecutableSha256=$executable.ExecutableSha256;ExecutableVersion=$executable.ExecutableVersion;SignerThumbprint=$executable.SignerThumbprint;DnsName=$node.DnsName;NodeId=$node.NodeId;PublicKey=$node.PublicKey;TailscaleIPv4=$node.TailscaleIPv4;HttpsPort=443;Path=$script:AevryndFunnelPath;Target=$script:AevryndFunnelTarget}
}finally{$executable.Stream.Dispose()}
