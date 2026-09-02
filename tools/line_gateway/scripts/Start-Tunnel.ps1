param(
    [string]$ConfigPath = "$env:ProgramData\Aevrynd\LineGateway\cloudflared\config.yml",
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Funnel.Common.ps1')
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$transportLock = Enter-PublicTransportLock
try {
$null = Test-InstallationManifest -Paths $paths
if (Test-Path -LiteralPath $paths.FunnelStateFile) { throw 'Tailscale Funnel state exists; public transports are mutually exclusive.' }
Test-FunnelTransportAbsent
$validated = & (Join-Path $PSScriptRoot 'Test-TunnelConfiguration.ps1') `
    -ConfigPath $ConfigPath -InstallRoot $InstallRoot
$currentConfigHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $validated.ConfigPath).Hash.ToLowerInvariant()
if ($currentConfigHash -cne $validated.ConfigSha256) { throw 'Named tunnel configuration changed during validation.' }

$state = Read-TunnelState -Paths $paths
if ($null -ne $state) {
    if ($state.status -eq 'active') {
        $existing = Test-TunnelProcess -Paths $paths -State $state
        if ($null -ne $existing) { throw "Named tunnel is already running with PID $($existing.Id)." }
        if ($null -ne (Get-Process -Id ([int]$state.pid) -ErrorAction SilentlyContinue)) {
            throw 'Tunnel state PID belongs to another process; refusing to replace state.'
        }
    } else {
        $recovered = Find-RecoverableTunnelProcess -Paths $paths -PendingState $state
        if ($null -ne $recovered) {
            Activate-TunnelState -Paths $paths -PendingState $state -Process $recovered
            throw "Recovered a tunnel from pending state with PID $($recovered.Id); refusing a duplicate start."
        }
    }
    Assert-ExactGatewayPath -Paths $paths -Path $paths.TunnelStateFile -AllowedRelativePaths @('state\tunnel-state.json')
    Remove-Item -LiteralPath $paths.TunnelStateFile -Force
}
$orphan = Find-RecoverableTunnelProcess -Paths $paths
if ($null -ne $orphan) { throw "An exact unmanaged tunnel already exists with PID $($orphan.Id); run Stop-Tunnel.ps1 first." }

Rotate-GatewayLogs -Paths $paths
$stdout = Join-Path $paths.Logs 'cloudflared.stdout.log'
$stderr = Join-Path $paths.Logs 'cloudflared.stderr.log'
$launchNonce = [Guid]::NewGuid().ToString('N')
Write-PendingTunnelState -Paths $paths -ExecutablePath $validated.CloudflaredExe `
    -ConfigPath $validated.ConfigPath -ConfigSha256 $validated.ConfigSha256 -LaunchNonce $launchNonce
$pending = Read-TunnelState -Paths $paths
if ($pending.status -ne 'pending' -or $pending.launchNonce -ne $launchNonce) {
    throw 'Pending tunnel launch handshake failed.'
}

$quotedConfig = '"' + $validated.ConfigPath + '"'
try {
    $process = Start-Process -FilePath $validated.CloudflaredExe `
        -ArgumentList @('--config', $quotedConfig, 'tunnel', 'run') `
        -WorkingDirectory $paths.Root -WindowStyle Hidden `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
} catch {
    $current = Read-TunnelState -Paths $paths
    if ($null -ne $current -and $current.status -eq 'pending' -and $current.launchNonce -eq $launchNonce) {
        Assert-ExactGatewayPath -Paths $paths -Path $paths.TunnelStateFile -AllowedRelativePaths @('state\tunnel-state.json')
        Remove-Item -LiteralPath $paths.TunnelStateFile -Force
    }
    throw
}

# pending 已在建立程序前落盤；此處即使宿主崩潰，Stop-Tunnel 仍可精確枚舉並收斂。
if ($process.WaitForExit(1500)) {
    $current = Read-TunnelState -Paths $paths
    if ($null -ne $current -and $current.status -eq 'pending' -and $current.launchNonce -eq $launchNonce) {
        Assert-ExactGatewayPath -Paths $paths -Path $paths.TunnelStateFile -AllowedRelativePaths @('state\tunnel-state.json')
        Remove-Item -LiteralPath $paths.TunnelStateFile -Force
    }
    throw 'cloudflared exited before tunnel activation completed.'
}
Activate-TunnelState -Paths $paths -PendingState $pending -Process $process
} finally {
    Exit-PublicTransportLock $transportLock
}
try {
    $process.WaitForExit()
    exit $process.ExitCode
} finally {
    try { $current = Read-TunnelState -Paths $paths } catch [System.Management.Automation.ItemNotFoundException] { $current = $null }
    if ($null -ne $current -and $current.status -eq 'active' -and [int]$current.pid -eq $process.Id) {
        Assert-ExactGatewayPath -Paths $paths -Path $paths.TunnelStateFile -AllowedRelativePaths @('state\tunnel-state.json')
        Remove-Item -LiteralPath $paths.TunnelStateFile -Force
    }
}
