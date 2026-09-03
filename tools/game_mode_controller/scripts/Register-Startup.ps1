[CmdletBinding(SupportsShouldProcess)]
param([string]$InstallRoot = "$env:ProgramData\Aevrynd\GameModeController")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($InstallRoot)
$programDataRoot = [IO.Path]::GetFullPath([Environment]::GetFolderPath('CommonApplicationData'))
$expected = [IO.Path]::GetFullPath((Join-Path $programDataRoot 'Aevrynd\GameModeController'))
if (-not [string]::Equals($root.TrimEnd('\'), $expected.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) { throw 'InstallRoot is not allowlisted.' }
$programDataItem = Get-Item -LiteralPath $programDataRoot -Force -ErrorAction Stop
if (-not $programDataItem.PSIsContainer -or ($programDataItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'ProgramData root is not a trusted physical directory.' }
$programDataOwner = (Get-Acl -LiteralPath $programDataRoot).Owner
$programDataOwnerSid = ([Security.Principal.NTAccount]$programDataOwner).Translate([Security.Principal.SecurityIdentifier]).Value
if ($programDataOwnerSid -notin @('S-1-5-18','S-1-5-32-544')) { throw 'ProgramData root owner is invalid.' }

function Assert-BootstrapTrust([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Scheduled-task trust boundary contains a reparse point or non-directory.' }
    $acl = Get-Acl -LiteralPath $Path
    $ownerSid = $acl.Owner | ForEach-Object { ([Security.Principal.NTAccount]$_).Translate([Security.Principal.SecurityIdentifier]).Value }
    if ($ownerSid -notin @('S-1-5-18','S-1-5-32-544')) { throw 'Scheduled-task trust boundary owner is invalid.' }
    # 此常數由 InstallTrust 的精確 mutation bits 產生；測試會鎖定兩者一致，避免 policy 漂移。
    $generatedMutationMask = 0x000D0156
    foreach ($rule in $acl.Access) {
        $sid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
        if ($rule.AccessControlType -eq [Security.AccessControl.AccessControlType]::Allow -and $sid -notin @('S-1-5-18','S-1-5-32-544') -and ([int64]$rule.FileSystemRights -band $generatedMutationMask)) { throw 'Scheduled-task trust boundary has an untrusted writer.' }
    }
}

$aevryndRoot = Join-Path $programDataRoot 'Aevrynd'
$scriptsRoot = Join-Path $root 'scripts'
$manifestsRoot = Join-Path $root 'manifests'
# 只先驗證載入 shared policy 所需的最小封閉邊界。
foreach ($path in @($aevryndRoot,$root,$scriptsRoot,$manifestsRoot)) { Assert-BootstrapTrust $path }
$launcher = Join-Path $root 'scripts\Launch-GameModeController.ps1'
$trustModule = Join-Path $scriptsRoot 'InstallTrust.psm1'
$configurationPath = Join-Path $root 'config\node.json'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf) -or -not (Test-Path -LiteralPath $configurationPath -PathType Leaf)) { throw 'Verified controller installation is unavailable.' }
$installedManifestPath = Join-Path $root 'manifests\installation-manifest.json'
$installedManifestHashPath = Join-Path $root 'manifests\installation-manifest.sha256'
$expectedInstalledManifestHash = (Get-Content -Raw -LiteralPath $installedManifestHashPath).Trim()
[byte[]]$installedManifestBytes = [IO.File]::ReadAllBytes($installedManifestPath)
$sha256 = [Security.Cryptography.SHA256]::Create()
try { $actualInstalledManifestHash = [Convert]::ToHexString($sha256.ComputeHash($installedManifestBytes)).ToLowerInvariant() } finally { $sha256.Dispose() }
if ($actualInstalledManifestHash -cne $expectedInstalledManifestHash) { throw 'Installed manifest hash validation failed.' }
$installedManifest = ([Text.UTF8Encoding]::new($false,$true)).GetString($installedManifestBytes) | ConvertFrom-Json
$trustEntry = @($installedManifest.files | Where-Object path -ceq 'scripts/InstallTrust.psm1')
if ($trustEntry.Count -ne 1 -or (Get-Item -LiteralPath $trustModule).Length -ne [int64]$trustEntry[0].size -or (Get-FileHash -Algorithm SHA256 -LiteralPath $trustModule).Hash.ToLowerInvariant() -cne [string]$trustEntry[0].sha256) { throw 'Installed trust policy hash validation failed.' }
Import-Module -Name $trustModule -Force -ErrorAction Stop
Assert-NoReparseAncestorChain -TrustedRoot $programDataRoot -Target $root
foreach ($path in @($aevryndRoot,$root,(Join-Path $root 'app'),$scriptsRoot,(Join-Path $root 'config'),$manifestsRoot)) { Assert-ClosedDirectoryTrust -Path $path }
$launcherEntry = @($installedManifest.files | Where-Object path -ceq 'scripts/Launch-GameModeController.ps1')
if ($launcherEntry.Count -ne 1 -or (Get-FileHash -Algorithm SHA256 -LiteralPath $launcher).Hash.ToLowerInvariant() -cne [string]$launcherEntry[0].sha256) { throw 'Scheduled launcher hash validation failed.' }
$configuration = Get-Content -Raw -LiteralPath $configurationPath | ConvertFrom-Json
if ($configuration.schemaVersion -ne 2 -or $configuration.nodeId -notin @('main','taichung','taoyuan')) { throw 'Deployed node configuration is invalid.' }
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator privileges are required to replace legacy startup.' }
$taskName = 'Aevrynd Game Mode Controller'
$pwsh = [IO.Path]::GetFullPath('C:\Program Files\PowerShell\7\pwsh.exe')
$pwshItem = Get-Item -LiteralPath $pwsh -Force -ErrorAction Stop
if ($pwshItem.PSIsContainer -or ($pwshItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Stable PowerShell 7 executable is unavailable.' }
$startupRoot = [Environment]::GetFolderPath([Environment+SpecialFolder]::Startup)
if ([string]::IsNullOrWhiteSpace($startupRoot)) { throw 'Per-user Startup folder is unavailable.' }
$startupItem = Get-Item -LiteralPath $startupRoot -Force -ErrorAction Stop
if (-not $startupItem.PSIsContainer -or ($startupItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Per-user Startup folder is invalid.' }
$startupShortcut = Join-Path $startupRoot 'Aevrynd Game Mode Controller.lnk'
$stagedShortcut = Join-Path $startupRoot ('.Aevrynd-GameMode-' + [Guid]::NewGuid().ToString('N') + '.lnk')
if ($PSCmdlet.ShouldProcess($startupShortcut, 'atomically register Explorer Startup controller and remove legacy scheduled task')) {
    try {
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut($stagedShortcut)
        $shortcut.TargetPath = $pwsh
        $shortcut.Arguments = "-NoProfile -NonInteractive -WindowStyle Hidden -File `"$launcher`" -InstallRoot `"$root`""
        $shortcut.WorkingDirectory = $root
        $shortcut.IconLocation = "$pwsh,0"
        $shortcut.WindowStyle = 7
        $shortcut.Description = 'Aevrynd supervised game mode controller startup'
        $shortcut.Save()
        if (-not (Test-Path -LiteralPath $stagedShortcut -PathType Leaf)) { throw 'Staged Startup shortcut is unavailable.' }
        [IO.File]::Move($stagedShortcut,$startupShortcut,$true)
        $legacyTask = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
        if ($null -ne $legacyTask) { Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction Stop }
    } finally {
        if (Test-Path -LiteralPath $stagedShortcut -PathType Leaf) { Remove-Item -LiteralPath $stagedShortcut -Force }
    }
}
