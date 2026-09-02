param([string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$controlMutex = [Threading.Mutex]::new($false, 'Local\AevryndLineGatewayTunnelControl')
$mutexAcquired = $controlMutex.WaitOne(30000)
if (-not $mutexAcquired) { $controlMutex.Dispose(); throw 'Timed out waiting for tunnel control lock.' }
try {
$stateReadFailed = $false
try {
    $state = Read-TunnelState -Paths $paths
} catch {
    # state 可能在寫入中斷後成為截斷 JSON；停止流程改走固定命令的精確枚舉。
    $state = $null
    $stateReadFailed = $true
}
$process = $null

if ($null -eq $state) {
    # state 遺失時只接受唯一 cloudflared.exe 與固定 config 的完整精確命令。
    $process = Find-RecoverableTunnelProcess -Paths $paths
    if ($null -eq $process) {
        if ($stateReadFailed -and (Test-Path -LiteralPath $paths.TunnelStateFile -PathType Leaf)) {
            Assert-ExactGatewayPath -Paths $paths -Path $paths.TunnelStateFile -AllowedRelativePaths @('state\tunnel-state.json')
            Remove-Item -LiteralPath $paths.TunnelStateFile -Force
            Write-Output 'Removed corrupt tunnel state after exact emergency discovery found no process.'
        } else {
            Write-Output 'No exact Aevrynd named tunnel process was found.'
        }
        exit 0
    }
} elseif ($state.status -eq 'pending') {
    $process = Find-RecoverableTunnelProcess -Paths $paths -PendingState $state
    if ($null -eq $process) {
        Assert-ExactGatewayPath -Paths $paths -Path $paths.TunnelStateFile -AllowedRelativePaths @('state\tunnel-state.json')
        Remove-Item -LiteralPath $paths.TunnelStateFile -Force
        Write-Output 'Removed pending tunnel state with no matching process.'
        exit 0
    }
} else {
    $process = Test-TunnelProcess -Paths $paths -State $state
    if ($null -eq $process) {
        if ($null -ne (Get-Process -Id ([int]$state.pid) -ErrorAction SilentlyContinue)) {
            throw 'Tunnel state PID belongs to another process; refusing to stop it.'
        }
        $process = Find-RecoverableTunnelProcess -Paths $paths
        if ($null -eq $process) {
            Assert-ExactGatewayPath -Paths $paths -Path $paths.TunnelStateFile -AllowedRelativePaths @('state\tunnel-state.json')
            Remove-Item -LiteralPath $paths.TunnelStateFile -Force
            Write-Output 'Removed stale tunnel state.'
            exit 0
        }
    }
}

Stop-Process -Id $process.Id
if (-not $process.WaitForExit(10000)) { Stop-Process -Id $process.Id -Force }
if (Test-Path -LiteralPath $paths.TunnelStateFile -PathType Leaf) {
    Assert-ExactGatewayPath -Paths $paths -Path $paths.TunnelStateFile -AllowedRelativePaths @('state\tunnel-state.json')
    Remove-Item -LiteralPath $paths.TunnelStateFile -Force
}
Write-Output "Named tunnel stopped safely: PID=$($process.Id)"
} finally {
    if ($mutexAcquired) { $controlMutex.ReleaseMutex() }
    $controlMutex.Dispose()
}
