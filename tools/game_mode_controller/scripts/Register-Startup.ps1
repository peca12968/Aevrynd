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

function Assert-TrustedPath([string]$Path,[bool]$AllowCurrentUserWrite) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Scheduled-task trust boundary contains a reparse point.' }
    $acl = Get-Acl -LiteralPath $Path
    $ownerSid = $acl.Owner | ForEach-Object { ([Security.Principal.NTAccount]$_).Translate([Security.Principal.SecurityIdentifier]).Value }
    if ($ownerSid -notin @('S-1-5-18','S-1-5-32-544')) { throw 'Scheduled-task trust boundary owner is invalid.' }
    $trustedWriters = @('S-1-5-18','S-1-5-32-544')
    $writeMask = [Security.AccessControl.FileSystemRights]::Write -bor [Security.AccessControl.FileSystemRights]::Modify -bor [Security.AccessControl.FileSystemRights]::FullControl
    foreach ($rule in $acl.Access) {
        $sid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
        if ($rule.AccessControlType -eq [Security.AccessControl.AccessControlType]::Allow -and $sid -notin $trustedWriters -and ($rule.FileSystemRights -band $writeMask)) { throw 'Scheduled-task trust boundary has an untrusted writer.' }
    }
}

$aevryndRoot = Join-Path $programDataRoot 'Aevrynd'
foreach ($path in @($aevryndRoot,$root,(Join-Path $root 'app'),(Join-Path $root 'scripts'),(Join-Path $root 'config'),(Join-Path $root 'manifests'))) { Assert-TrustedPath $path $false }
$launcher = Join-Path $root 'scripts\Launch-GameModeController.ps1'
$configurationPath = Join-Path $root 'config\node.json'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf) -or -not (Test-Path -LiteralPath $configurationPath -PathType Leaf)) { throw 'Verified controller installation is unavailable.' }
$installedManifestPath = Join-Path $root 'manifests\installation-manifest.json'
$installedManifestHashPath = Join-Path $root 'manifests\installation-manifest.sha256'
$installedManifest = Get-Content -Raw -LiteralPath $installedManifestPath | ConvertFrom-Json
$expectedInstalledManifestHash = (Get-Content -Raw -LiteralPath $installedManifestHashPath).Trim()
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $installedManifestPath).Hash.ToLowerInvariant() -cne $expectedInstalledManifestHash) { throw 'Installed manifest hash validation failed.' }
$launcherEntry = @($installedManifest.files | Where-Object path -ceq 'scripts/Launch-GameModeController.ps1')
if ($launcherEntry.Count -ne 1 -or (Get-FileHash -Algorithm SHA256 -LiteralPath $launcher).Hash.ToLowerInvariant() -cne [string]$launcherEntry[0].sha256) { throw 'Scheduled launcher hash validation failed.' }
$configuration = Get-Content -Raw -LiteralPath $configurationPath | ConvertFrom-Json
if ($configuration.schemaVersion -ne 2 -or $configuration.nodeId -cne 'main') { throw 'Only the deployed main runner can register startup.' }
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$taskName = 'Aevrynd Game Mode Controller'
$pwsh = (Get-Command pwsh.exe -ErrorAction Stop).Source
$action = New-ScheduledTaskAction -Execute $pwsh -Argument "-NoProfile -WindowStyle Hidden -File `"$launcher`" -InstallRoot `"$root`""
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $identity.Name
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Days 3650) -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)
if ($PSCmdlet.ShouldProcess($taskName, "register limited AtLogOn controller for $($identity.Name)")) {
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -User $identity.Name -RunLevel Limited -Force | Out-Null
}
