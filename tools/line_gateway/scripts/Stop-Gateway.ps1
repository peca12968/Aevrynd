param([string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$state = Read-GatewayState -Paths $paths
if ($null -eq $state) { Write-Output 'Gateway state was not found; nothing to stop.'; exit 0 }
$process = Test-GatewayProcess -Paths $paths -State $state
if ($null -eq $process) {
    if ($null -ne (Get-Process -Id ([int]$state.pid) -ErrorAction SilentlyContinue)) {
        throw 'State PID belongs to a different process; refusing to stop it.'
    }
    Assert-ExactGatewayPath -Paths $paths -Path $paths.StateFile -AllowedRelativePaths @('state\gateway-state.json')
    Remove-Item -LiteralPath $paths.StateFile -Force
    Write-Output 'Removed stale gateway state.'
    exit 0
}
Stop-Process -Id $process.Id
if (-not $process.WaitForExit(10000)) { Stop-Process -Id $process.Id -Force }
Assert-ExactGatewayPath -Paths $paths -Path $paths.StateFile -AllowedRelativePaths @('state\gateway-state.json')
Remove-Item -LiteralPath $paths.StateFile -Force -ErrorAction SilentlyContinue
Write-Output 'Gateway stopped.'
