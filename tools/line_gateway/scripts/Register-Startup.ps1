param(
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway",
    [string]$CloudflaredConfigPath = '',
    [ValidateSet('None','Cloudflare','Funnel','Ngrok')][string]$PublicTransport = 'None'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Assert-Administrator
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$transportLock = Enter-PublicTransportLock
try {
$pwsh = Get-TrustedSystemPowerShell
if (-not (Test-Path -LiteralPath (Join-Path $paths.Scripts 'Start-Gateway.ps1') -PathType Leaf)) {
    throw 'Installed gateway start script is missing.'
}

# 在改動排程前完成全部 transport 驗證，避免留下半套公開入口。
$tunnel = $null
if (-not [string]::IsNullOrWhiteSpace($CloudflaredConfigPath)) {
    if ($PublicTransport -in @('Funnel','Ngrok')) { throw 'Cloudflare config and the selected public transport are mutually exclusive.' }
    $PublicTransport = 'Cloudflare'
    $tunnel = & (Join-Path $paths.Scripts 'Test-TunnelConfiguration.ps1') -ConfigPath $CloudflaredConfigPath
}
if ($PublicTransport -eq 'Cloudflare' -and $null -eq $tunnel) { throw 'Cloudflare startup requires an explicit validated config path.' }
$funnel = $null
if ($PublicTransport -eq 'Funnel') {
    $funnel = & (Join-Path $paths.Scripts 'Test-FunnelConfiguration.ps1') -InstallRoot $InstallRoot
    if ($funnel.Mode -ne 'active') { throw 'Register Funnel startup only after authoritative active status is verified.' }
}
$ngrok=$null
if($PublicTransport-eq'Ngrok'){$ngrok=& (Join-Path $paths.Scripts 'Test-NgrokConfiguration.ps1') -InstallRoot $InstallRoot;if($ngrok.Mode-ne'active'){throw 'Register ngrok startup only after its exact endpoint is active and verified.'}}

$user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -RestartCount 3 `
    -RestartInterval (New-TimeSpan -Minutes 1) -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
$bootstrapScript = Join-Path $paths.Scripts 'Bootstrap.ps1'
$gatewayArguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$bootstrapScript`" -Entry Gateway -InstallRoot `"$InstallRoot`" -Wait"
$gatewayAction = New-ScheduledTaskAction -Execute $pwsh -Argument $gatewayArguments -WorkingDirectory $paths.Root
$gatewayTask = New-ScheduledTask -Action $gatewayAction -Trigger $trigger -Principal $principal -Settings $settings
Register-ScheduledTask -TaskName 'Aevrynd-Line-Gateway' -InputObject $gatewayTask -Force | Out-Null

$existingTunnel = Get-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Tunnel' -ErrorAction SilentlyContinue
$existingNgrok = Get-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Ngrok' -ErrorAction SilentlyContinue
if ($PublicTransport -eq 'None') {
    if ($null -ne $existingTunnel) { Unregister-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Tunnel' -Confirm:$false }
    if ($null -ne $existingNgrok) { Unregister-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Ngrok' -Confirm:$false }
    Write-Output 'Gateway startup registered for the current user; public tunnel startup is disabled.'
    exit 0
}
if ($PublicTransport -eq 'Funnel') {
    if ($null -ne $existingTunnel) { Unregister-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Tunnel' -Confirm:$false }
    if ($null -ne $existingNgrok) { Unregister-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Ngrok' -Confirm:$false }
    Write-Output 'Gateway startup registered; verified background Funnel persists through the Tailscale service.'
    exit 0
}
if($PublicTransport-eq'Ngrok'-and$null-ne$existingTunnel){Unregister-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Tunnel' -Confirm:$false}
if($PublicTransport-eq'Cloudflare'-and$null-ne$existingNgrok){Unregister-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Ngrok' -Confirm:$false}
$tunnelArguments = if($PublicTransport-eq'Ngrok'){"-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$bootstrapScript`" -Entry Ngrok -InstallRoot `"$InstallRoot`""}else{"-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$bootstrapScript`" -Entry Cloudflare -InstallRoot `"$InstallRoot`" -ConfigPath `"$($tunnel.ConfigPath)`""}
$tunnelAction = New-ScheduledTaskAction -Execute $pwsh -Argument $tunnelArguments -WorkingDirectory $paths.Root
$tunnelTask = New-ScheduledTask -Action $tunnelAction -Trigger $trigger -Principal $principal -Settings $settings
Register-ScheduledTask -TaskName $(if($PublicTransport-eq'Ngrok'){'Aevrynd-Line-Gateway-Ngrok'}else{'Aevrynd-Line-Gateway-Tunnel'}) -InputObject $tunnelTask -Force | Out-Null
Write-Output 'Gateway and validated named tunnel startup tasks registered for the current user.'
} finally { Exit-PublicTransportLock $transportLock }
