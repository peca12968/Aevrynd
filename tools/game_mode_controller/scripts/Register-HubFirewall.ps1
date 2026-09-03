[CmdletBinding(SupportsShouldProcess)]
param([string]$InstallRoot = "$env:ProgramData\Aevrynd\GameModeController")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator privileges are required.' }
$root = [IO.Path]::GetFullPath($InstallRoot)
$expected = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\GameModeController")
if (-not [string]::Equals($root.TrimEnd('\'),$expected.TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase)) { throw 'InstallRoot is not allowlisted.' }
$configurationPath = Join-Path $root 'config\node.json'
$configurationItem = Get-Item -LiteralPath $configurationPath -Force -ErrorAction Stop
if ($configurationItem.PSIsContainer -or ($configurationItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $configurationItem.Length -lt 2 -or $configurationItem.Length -gt 8192) { throw 'Hub configuration is invalid.' }
$configuration = Get-Content -Raw -LiteralPath $configurationPath | ConvertFrom-Json
$configurationFields = @($configuration.PSObject.Properties.Name | Sort-Object)
if (($configurationFields -join ',') -cne 'nodeId,pythonExecutable,runtimeManifest,runtimeManifestSha256,schemaVersion' -or
    $configuration.schemaVersion -isnot [int64] -or $configuration.schemaVersion -ne 2 -or $configuration.nodeId -cne 'main') { throw 'Only the main node may register the hub firewall rule.' }
$python = [IO.Path]::GetFullPath([string]$configuration.pythonExecutable)
if (-not (Test-Path -LiteralPath $python -PathType Leaf) -or
    (((Get-Item -LiteralPath $python -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) -or
    [IO.Path]::GetFileName($python) -notin @('python.exe','python3.exe')) { throw 'Pinned Python executable is unavailable.' }
$contracts = @(
    [ordered]@{ Name = 'Aevrynd Game Mode Hub Taichung'; Local = '192.168.50.18'; Remote = '192.168.50.98' },
    [ordered]@{ Name = 'Aevrynd Game Mode Hub Taoyuan'; Local = '100.115.174.48'; Remote = '100.89.42.36' }
)
foreach ($contract in $contracts) {
    $existing = @(Get-NetFirewallRule -DisplayName $contract.Name -ErrorAction SilentlyContinue)
    if ($existing.Count -gt 1) { throw 'Hub firewall rule identity is ambiguous.' }
    if ($existing.Count -eq 1) {
        $port = $existing[0] | Get-NetFirewallPortFilter
        $address = $existing[0] | Get-NetFirewallAddressFilter
        $application = $existing[0] | Get-NetFirewallApplicationFilter
        if ($existing[0].Direction -ne 'Inbound' -or $existing[0].Action -ne 'Allow' -or $existing[0].Enabled -ne 'True' -or
            $existing[0].Profile -ne 'Any' -or $existing[0].EdgeTraversalPolicy -ne 'Block' -or
            $port.Protocol -ne 'TCP' -or [string]$port.LocalPort -ne '8766' -or
            @($address.LocalAddress).Count -ne 1 -or [string]$address.LocalAddress -cne $contract.Local -or
            @($address.RemoteAddress).Count -ne 1 -or [string]$address.RemoteAddress -cne $contract.Remote -or
            -not [string]::Equals([string]$application.Program,$python,[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Existing hub firewall rule differs from the fixed contract.'
        }
        continue
    }
    if ($PSCmdlet.ShouldProcess($contract.Name,'register fixed-node inbound firewall rule')) {
        New-NetFirewallRule -DisplayName $contract.Name -Direction Inbound -Action Allow -Enabled True -Profile Any -EdgeTraversalPolicy Block -Protocol TCP -LocalPort 8766 -LocalAddress $contract.Local -RemoteAddress $contract.Remote -Program $python | Out-Null
    }
}
$legacyName = 'Aevrynd Game Mode Hub Fixed Nodes'
$legacy = @(Get-NetFirewallRule -DisplayName $legacyName -ErrorAction SilentlyContinue)
if ($legacy.Count -gt 1) { throw 'Legacy hub firewall rule identity is ambiguous.' }
if ($legacy.Count -eq 1) {
    $legacyPort = $legacy[0] | Get-NetFirewallPortFilter
    $legacyAddress = $legacy[0] | Get-NetFirewallAddressFilter
    $legacyApplication = $legacy[0] | Get-NetFirewallApplicationFilter
    $legacyLocal = @($legacyAddress.LocalAddress | ForEach-Object { [string]$_ } | Sort-Object -Unique)
    $legacyRemote = @($legacyAddress.RemoteAddress | ForEach-Object { [string]$_ } | Sort-Object -Unique)
    if ($legacy[0].Direction -ne 'Inbound' -or $legacy[0].Action -ne 'Allow' -or
        $legacyPort.Protocol -ne 'TCP' -or [string]$legacyPort.LocalPort -ne '8766' -or
        ($legacyLocal -join ',') -cne '100.115.174.48,192.168.50.18' -or
        ($legacyRemote -join ',') -cne '100.89.42.36,192.168.50.98' -or
        -not [string]::Equals([string]$legacyApplication.Program,$python,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Legacy hub firewall rule differs from the removable contract.'
    }
    if ($PSCmdlet.ShouldProcess($legacyName,'remove superseded Cartesian-address firewall rule')) {
        Remove-NetFirewallRule -DisplayName $legacyName -ErrorAction Stop
    }
}
Write-Output 'Fixed pairwise hub firewall rules are registered.'
