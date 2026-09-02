[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway",
    [switch]$RemoveData
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Assert-Administrator
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$transportLock = Enter-PublicTransportLock
try {

foreach ($taskName in @('Aevrynd-Line-Gateway', 'Aevrynd-Line-Gateway-Tunnel', 'Aevrynd-Line-Gateway-Funnel', 'Aevrynd-Line-Gateway-Ngrok')) {
    if ($null -ne (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue)) {
        Disable-ScheduledTask -TaskName $taskName | Out-Null
    }
}
$stopFailures = @(Invoke-GatewayEmergencyStops -ScriptsRoot $PSScriptRoot -InstallRoot $InstallRoot)
if ($stopFailures.Count -gt 0) {
    throw "Gateway emergency stop failed; uninstall preserved tasks, credentials, and files. Components: $($stopFailures -join ',')."
}
foreach ($taskName in @('Aevrynd-Line-Gateway', 'Aevrynd-Line-Gateway-Tunnel', 'Aevrynd-Line-Gateway-Funnel', 'Aevrynd-Line-Gateway-Ngrok')) {
    if ($null -ne (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue)) {
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
    }
}

if (-not ('AevryndCredentialRemover' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class AevryndCredentialRemover {
    [DllImport("advapi32.dll", EntryPoint = "CredDeleteW", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CredDelete(string target, UInt32 type, UInt32 flags);

    public static void DeleteIfPresent(string target) {
        if (CredDelete(target, 1, 0)) return;
        int error = Marshal.GetLastWin32Error();
        if (error != 1168) throw new System.ComponentModel.Win32Exception(error);
    }
}
'@
}
foreach ($target in @(
    'Aevrynd/LineGateway/ChannelSecret',
    'Aevrynd/LineGateway/ChannelAccessToken',
    'Aevrynd/LineGateway/QwenApiKey'
    'Aevrynd/LineGateway/SecondaryQwenApiKey',
    'Aevrynd/LineGateway/NgrokAuthtoken'
)) {
    [AevryndCredentialRemover]::DeleteIfPresent($target)
}

# 緊急停止與外部排程/credential 收斂不依賴 manifest；此 gate 只保護後續檔案刪除。
$null = Test-InstallationManifest -Paths $paths
if ($RemoveData) {
    if ($PSCmdlet.ShouldProcess($paths.Root, 'Remove all LINE gateway programs, queue, handoff and logs')) {
        $expected = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\LineGateway")
        if (-not [string]::Equals($paths.Root.TrimEnd('\'), $expected.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Refusing to remove an unexpected path.'
        }
        Remove-Item -LiteralPath $paths.Root -Recurse -Force
        Write-Output 'LINE gateway programs and data removed.'
    }
    exit 0
}

# 預設保留 queue、handoff 與 logs，只移除可重建程式和本機秘密設定。
foreach ($path in @($paths.App, $paths.Scripts, $paths.Manifests, $paths.Config, (Join-Path $paths.Root 'cloudflared'), (Join-Path $paths.Root 'staging'))) {
    Assert-ExactGatewayPath -Paths $paths -Path $path -AllowedRelativePaths @('app', 'scripts', 'manifests', 'config', 'cloudflared', 'staging')
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
}
Assert-ExactGatewayPath -Paths $paths -Path $paths.StateFile -AllowedRelativePaths @('state\gateway-state.json')
if (Test-Path -LiteralPath $paths.StateFile) { Remove-Item -LiteralPath $paths.StateFile -Force }
Assert-ExactGatewayPath -Paths $paths -Path $paths.FunnelStateFile -AllowedRelativePaths @('state\funnel-state.json')
if (Test-Path -LiteralPath $paths.FunnelStateFile) { Remove-Item -LiteralPath $paths.FunnelStateFile -Force }
Assert-ExactGatewayPath -Paths $paths -Path $paths.NgrokStateFile -AllowedRelativePaths @('state\ngrok-state.json')
if (Test-Path -LiteralPath $paths.NgrokStateFile) { Remove-Item -LiteralPath $paths.NgrokStateFile -Force }
Write-Output "Programs and credentials removed; data, handoff and logs preserved under $($paths.Root)."
} finally { Exit-PublicTransportLock $transportLock }
