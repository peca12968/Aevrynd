param(
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway",
    [switch]$Wait
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$null = Test-InstallationManifest -Paths $paths
$python = Get-InstalledPython -Paths $paths
Import-GatewaySettings -Paths $paths

$existingState = Read-GatewayState -Paths $paths
if ($null -ne $existingState) {
    $existingProcess = Test-GatewayProcess -Paths $paths -State $existingState
    if ($null -ne $existingProcess) { throw "Gateway is already running with PID $($existingProcess.Id)." }
    Assert-ExactGatewayPath -Paths $paths -Path $paths.StateFile -AllowedRelativePaths @('state\gateway-state.json')
    Remove-Item -LiteralPath $paths.StateFile -Force
}
if (-not (Test-Path -LiteralPath $paths.Runner -PathType Leaf)) { throw 'Gateway runner is missing.' }

# 先由正式 loader 驗證 allowlist 與 Credential Manager，輸出只允許固定字串。
$probe = 'from tools.line_gateway.config import load_config; load_config(); print("configuration_ok")'
$previousPythonPath = $env:PYTHONPATH
$previousDontWriteBytecode = $env:PYTHONDONTWRITEBYTECODE
try {
    $env:PYTHONPATH = $paths.App
    $env:PYTHONDONTWRITEBYTECODE = '1'
    $probeOutput = & $python -B -c $probe 2>$null
    if ($LASTEXITCODE -ne 0 -or $probeOutput -ne 'configuration_ok') {
        throw 'Gateway configuration or credentials failed validation.'
    }
} finally {
    if ($null -eq $previousPythonPath) { Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue }
    else { $env:PYTHONPATH = $previousPythonPath }
    if ($null -eq $previousDontWriteBytecode) { Remove-Item Env:PYTHONDONTWRITEBYTECODE -ErrorAction SilentlyContinue }
    else { $env:PYTHONDONTWRITEBYTECODE = $previousDontWriteBytecode }
}

Rotate-GatewayLogs -Paths $paths
$stdout = Join-Path $paths.Logs 'gateway.stdout.log'
$stderr = Join-Path $paths.Logs 'gateway.stderr.log'
$previousDontWriteBytecode = $env:PYTHONDONTWRITEBYTECODE
try {
    $env:PYTHONDONTWRITEBYTECODE = '1'
    $process = Start-Process -FilePath $python -ArgumentList @('-B', $paths.Runner) `
        -WorkingDirectory $paths.Root -WindowStyle Hidden `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
} finally {
    if ($null -eq $previousDontWriteBytecode) { Remove-Item Env:PYTHONDONTWRITEBYTECODE -ErrorAction SilentlyContinue }
    else { $env:PYTHONDONTWRITEBYTECODE = $previousDontWriteBytecode }
}
try { $process.PriorityClass = [Diagnostics.ProcessPriorityClass]::BelowNormal } catch { }
Write-GatewayState -Paths $paths -Process $process

$healthy = $false
for ($attempt = 0; $attempt -lt 20; $attempt++) {
    if ($process.HasExited) { break }
    try {
        $response = Invoke-RestMethod -Method Get -Uri 'http://127.0.0.1:8787/health' -TimeoutSec 2
        if ($response.status -eq 'ok') { $healthy = $true; break }
    } catch { }
    Start-Sleep -Milliseconds 500
}
if (-not $healthy) {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    Assert-ExactGatewayPath -Paths $paths -Path $paths.StateFile -AllowedRelativePaths @('state\gateway-state.json')
    Remove-Item -LiteralPath $paths.StateFile -Force -ErrorAction SilentlyContinue
    throw 'Gateway failed its loopback health check. See the secret-safe error log.'
}
Write-Output "Gateway started on 127.0.0.1:8787 with PID $($process.Id)."
if ($Wait) {
    $process.WaitForExit()
    $currentState = Read-GatewayState -Paths $paths
    if ($null -ne $currentState -and [int]$currentState.pid -eq $process.Id) {
        Assert-ExactGatewayPath -Paths $paths -Path $paths.StateFile -AllowedRelativePaths @('state\gateway-state.json')
        Remove-Item -LiteralPath $paths.StateFile -Force
    }
    exit $process.ExitCode
}
