param(
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway",
    [string]$CloudflaredConfigPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Assert-Administrator
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$pwsh = Get-Command pwsh.exe -ErrorAction SilentlyContinue
if ($null -eq $pwsh) { throw 'PowerShell 7 executable was not found.' }
if (-not (Test-Path -LiteralPath (Join-Path $paths.Scripts 'Start-Gateway.ps1') -PathType Leaf)) {
    throw 'Installed gateway start script is missing.'
}

# 在改動排程前完成全部 tunnel 驗證，避免留下半套公開入口。
$tunnel = $null
if (-not [string]::IsNullOrWhiteSpace($CloudflaredConfigPath)) {
    $tunnel = & (Join-Path $paths.Scripts 'Test-TunnelConfiguration.ps1') -ConfigPath $CloudflaredConfigPath
}

$user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -RestartCount 3 `
    -RestartInterval (New-TimeSpan -Minutes 1) -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
$startScript = Join-Path $paths.Scripts 'Start-Gateway.ps1'
$gatewayArguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$startScript`" -Wait"
$gatewayAction = New-ScheduledTaskAction -Execute $pwsh.Source -Argument $gatewayArguments -WorkingDirectory $paths.Root
$gatewayTask = New-ScheduledTask -Action $gatewayAction -Trigger $trigger -Principal $principal -Settings $settings
Register-ScheduledTask -TaskName 'Aevrynd-Line-Gateway' -InputObject $gatewayTask -Force | Out-Null

$existingTunnel = Get-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Tunnel' -ErrorAction SilentlyContinue
if ($null -eq $tunnel) {
    if ($null -ne $existingTunnel) { Unregister-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Tunnel' -Confirm:$false }
    Write-Output 'Gateway startup registered for the current user; public tunnel startup is disabled.'
    exit 0
}
$tunnelScript = Join-Path $paths.Scripts 'Start-Tunnel.ps1'
$tunnelArguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$tunnelScript`" -ConfigPath `"$($tunnel.ConfigPath)`""
$tunnelAction = New-ScheduledTaskAction -Execute $pwsh.Source -Argument $tunnelArguments -WorkingDirectory $paths.Root
$tunnelTask = New-ScheduledTask -Action $tunnelAction -Trigger $trigger -Principal $principal -Settings $settings
Register-ScheduledTask -TaskName 'Aevrynd-Line-Gateway-Tunnel' -InputObject $tunnelTask -Force | Out-Null
Write-Output 'Gateway and validated named tunnel startup tasks registered for the current user.'
