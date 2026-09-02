param([string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$state = Read-GatewayState -Paths $paths
$process = Test-GatewayProcess -Paths $paths -State $state
if ($null -eq $process) { throw 'Gateway process validation failed.' }
$listeners = @(Get-NetTCPConnection -State Listen -LocalPort 8787 -ErrorAction Stop)
if ($listeners.Count -ne 1 -or $listeners[0].LocalAddress -ne '127.0.0.1' -or $listeners[0].OwningProcess -ne $process.Id) {
    throw 'Gateway listener is not restricted to loopback.'
}
$response = Invoke-RestMethod -Method Get -Uri 'http://127.0.0.1:8787/health' -TimeoutSec 5
if ($response.status -ne 'ok' -or $response.workersHealthy -ne $true -or
    [int]$response.workerCount -ne [int]$response.expectedWorkerCount) {
    throw 'Gateway health response is invalid.'
}
Write-Output "Gateway health passed: PID=$($process.Id) listener=127.0.0.1:8787 workers=$($response.workerCount)"
