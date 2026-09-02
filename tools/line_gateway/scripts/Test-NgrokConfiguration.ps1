param([string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1');. (Join-Path $PSScriptRoot 'Funnel.Common.ps1');. (Join-Path $PSScriptRoot 'Ngrok.Common.ps1')
$paths=Get-GatewayPaths $InstallRoot;$config=Read-NgrokDomainConfiguration $paths
if(-not(Test-Path $paths.NgrokStateFile -PathType Leaf)){throw 'Ngrok state is absent.'};$state=Read-NgrokState $paths
if($state.domain-cne$config.Domain-or$state.origin-cne'http://127.0.0.1:8787'-or$state.path-cne'/webhook'){throw 'Ngrok state tuple is invalid.'}
$executable=Get-NgrokExecutableIdentity $PSScriptRoot
try{$process=Assert-NgrokProcessIdentity $state $executable}finally{if($null-ne$executable){$executable.Stream.Dispose()}}
$api=Invoke-RestMethod 'http://127.0.0.1:4040/api/tunnels' -TimeoutSec 3;$tunnels=@($api.tunnels)
if($tunnels.Count-ne1-or$tunnels[0].public_url-cne"https://$($config.Domain)"-or$tunnels[0].proto-cne'https'-or$tunnels[0].config.addr-cne'http://127.0.0.1:8787'){throw 'Ngrok authoritative local API tuple is invalid.'}
[pscustomobject]@{Mode='active';Domain=$config.Domain;WebhookUrl="https://$($config.Domain)/webhook";Origin='http://127.0.0.1:8787';Path='/webhook'}
