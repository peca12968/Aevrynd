param(
    [Parameter(Mandatory)][string]$ConfigPath,
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$fullConfigPath = [IO.Path]::GetFullPath($ConfigPath)
Assert-ExactGatewayPath -Paths $paths -Path $fullConfigPath -AllowedRelativePaths @('cloudflared\config.yml')
$commands = @(Get-Command cloudflared.exe -All -ErrorAction SilentlyContinue)
$resolvedPaths = @($commands | ForEach-Object { [IO.Path]::GetFullPath($_.Source) } | Sort-Object -Unique)
if ($resolvedPaths.Count -ne 1) {
    throw 'A unique cloudflared executable is unavailable; public webhook ingress remains disabled.'
}
$cloudflaredExe = $resolvedPaths[0]
$configuration = Read-NamedTunnelConfiguration -ConfigPath $fullConfigPath
& $cloudflaredExe tunnel --config $configuration.ConfigPath ingress validate *> $null
if ($LASTEXITCODE -ne 0) { throw 'cloudflared rejected the named tunnel ingress configuration.' }
[pscustomobject]@{
    CloudflaredExe = $cloudflaredExe
    ConfigPath = $configuration.ConfigPath
    ConfigSha256 = $configuration.ConfigSha256
    TunnelId = $configuration.TunnelId
    Hostname = $configuration.Hostname
}
