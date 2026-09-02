[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateSet('main', 'taichung', 'taoyuan')]
    [string]$NodeId,
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\GameModeController",
    [string]$PythonExe = '',
    [string]$RuntimeManifestPath = '',
    [ValidatePattern('^[0-9a-f]{64}$')]
    [string]$RuntimeManifestSha256 = '',
    [switch]$RegisterStartup
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$programDataRoot = [IO.Path]::GetFullPath([Environment]::GetFolderPath('CommonApplicationData')).TrimEnd('\')
$expectedRoot = [IO.Path]::GetFullPath((Join-Path $programDataRoot 'Aevrynd\GameModeController')).TrimEnd('\')
$resolvedRoot = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
if (-not [string]::Equals($expectedRoot, $resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'InstallRoot must be the fixed ProgramData Aevrynd GameModeController directory.'
}
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$trustModule = Join-Path $PSScriptRoot 'InstallTrust.psm1'
$sourceManifest = Join-Path $sourceRoot 'source-manifest.json'
$expectedSourceManifestSha256 = 'bbe70cb1681197228e14a6ab9b9f6b275bf0004987501ac157351bfe14f6cb90'
if (-not (Test-Path -LiteralPath $sourceManifest -PathType Leaf)) { throw 'Closed source manifest is missing.' }
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    # 工作區可由非系統管理員修改；雜湊與解析必須使用同一份不可變快照。
    [byte[]]$sourceManifestBytes = [IO.File]::ReadAllBytes($sourceManifest)
    $actualSourceManifestSha256 = [Convert]::ToHexString($sha256.ComputeHash($sourceManifestBytes)).ToLowerInvariant()
} finally { $sha256.Dispose() }
if ($actualSourceManifestSha256 -cne $expectedSourceManifestSha256) { throw 'Closed source manifest hash validation failed.' }
$strictUtf8 = [Text.UTF8Encoding]::new($false,$true)
$closedManifest = $strictUtf8.GetString($sourceManifestBytes) | ConvertFrom-Json
$trustEntry = @($closedManifest.files | Where-Object source -ceq 'scripts/InstallTrust.psm1')
[byte[]]$trustModuleBytes = [IO.File]::ReadAllBytes($trustModule)
$sha256 = [Security.Cryptography.SHA256]::Create()
try { $trustModuleHash = [Convert]::ToHexString($sha256.ComputeHash($trustModuleBytes)).ToLowerInvariant() }
finally { $sha256.Dispose() }
if ($trustEntry.Count -ne 1 -or $trustModuleBytes.LongLength -ne [int64]$trustEntry[0].size -or $trustModuleHash -cne [string]$trustEntry[0].sha256) { throw 'Install trust helper validation failed.' }

if (-not $PSCmdlet.ShouldProcess($resolvedRoot, "transactionally install Game Mode Controller for node $NodeId")) { return }
if ($NodeId -cne 'main') { throw 'This deployable phase installs only the main node.' }

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator privileges are required.' }

# 驗證後的模組快照只寫入隨機且封閉的 ProgramData 目錄，再從該副本載入。
$programDataItem = Get-Item -LiteralPath $programDataRoot -Force -ErrorAction Stop
if (-not $programDataItem.PSIsContainer -or ($programDataItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'ProgramData root is not a trusted physical directory.' }
$programDataOwner = (Get-Acl -LiteralPath $programDataRoot).Owner
$programDataOwnerSid = ([Security.Principal.NTAccount]$programDataOwner).Translate([Security.Principal.SecurityIdentifier]).Value
if ($programDataOwnerSid -notin @('S-1-5-18','S-1-5-32-544')) { throw 'ProgramData root owner is invalid.' }
$bootstrapRoot = Join-Path $programDataRoot ('.AevryndGameModeBootstrap-' + [Guid]::NewGuid().ToString('N'))
try {
New-Item -ItemType Directory -Path $bootstrapRoot -ErrorAction Stop | Out-Null
$bootstrapAcl = Get-Acl -LiteralPath $bootstrapRoot
$bootstrapAcl.SetAccessRuleProtection($true,$false)
$administratorsSid = [Security.Principal.SecurityIdentifier]::new('S-1-5-32-544')
$bootstrapAcl.SetOwner($administratorsSid)
foreach ($existingSid in @($bootstrapAcl.Access | ForEach-Object { $_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]) } | Select-Object -Unique)) { $bootstrapAcl.PurgeAccessRules($existingSid) }
foreach ($sid in @([Security.Principal.SecurityIdentifier]::new('S-1-5-18'),$administratorsSid)) {
    $bootstrapAcl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,[Security.AccessControl.FileSystemRights]::FullControl,[Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit',[Security.AccessControl.PropagationFlags]::None,[Security.AccessControl.AccessControlType]::Allow)) | Out-Null
}
Set-Acl -LiteralPath $bootstrapRoot -AclObject $bootstrapAcl
$stagedTrustModule = Join-Path $bootstrapRoot 'InstallTrust.psm1'
$stream = [IO.FileStream]::new($stagedTrustModule,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try { $stream.Write($trustModuleBytes,0,$trustModuleBytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
[byte[]]$stagedTrustBytes = [IO.File]::ReadAllBytes($stagedTrustModule)
$sha256 = [Security.Cryptography.SHA256]::Create()
try { $stagedTrustHash = [Convert]::ToHexString($sha256.ComputeHash($stagedTrustBytes)).ToLowerInvariant() } finally { $sha256.Dispose() }
if ($stagedTrustBytes.LongLength -ne $trustModuleBytes.LongLength -or $stagedTrustHash -cne $trustModuleHash) { throw 'Staged install trust helper identity validation failed.' }
$stagedItem = Get-Item -LiteralPath $stagedTrustModule -Force -ErrorAction Stop
if (($stagedItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $stagedItem.Length -ne $trustModuleBytes.LongLength) { throw 'Staged install trust helper is not a physical immutable file.' }
$verifiedBootstrapAcl = Get-Acl -LiteralPath $bootstrapRoot
$verifiedBootstrapOwner = ([Security.Principal.NTAccount]$verifiedBootstrapAcl.Owner).Translate([Security.Principal.SecurityIdentifier]).Value
if ($verifiedBootstrapOwner -notin @('S-1-5-18','S-1-5-32-544')) { throw 'Bootstrap directory owner validation failed.' }
$writeMask = [Security.AccessControl.FileSystemRights]::WriteData `
    -bor [Security.AccessControl.FileSystemRights]::AppendData `
    -bor [Security.AccessControl.FileSystemRights]::WriteExtendedAttributes `
    -bor [Security.AccessControl.FileSystemRights]::WriteAttributes `
    -bor [Security.AccessControl.FileSystemRights]::DeleteSubdirectoriesAndFiles `
    -bor [Security.AccessControl.FileSystemRights]::Delete `
    -bor [Security.AccessControl.FileSystemRights]::ChangePermissions `
    -bor [Security.AccessControl.FileSystemRights]::TakeOwnership
foreach ($rule in $verifiedBootstrapAcl.Access) {
    $ruleSid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
    if ($rule.AccessControlType -eq [Security.AccessControl.AccessControlType]::Allow -and $ruleSid -notin @('S-1-5-18','S-1-5-32-544') -and ($rule.FileSystemRights -band $writeMask)) { throw 'Bootstrap directory has an untrusted writer.' }
}
Import-Module -Name $stagedTrustModule -Force -ErrorAction Stop

$resolvedRoot = Initialize-ControllerTrustRoot -ProgramDataRoot $programDataRoot -InstallRoot $resolvedRoot -CurrentUserSid $identity.User
foreach ($child in @('app','scripts','config','manifests','state','staging','transactions')) {
    $candidate = Join-Path $resolvedRoot $child
    Assert-NoReparseAncestorChain -TrustedRoot $programDataRoot -Target $candidate
    if (Test-Path -LiteralPath $candidate) { Assert-ClosedDirectoryTrust -Path $candidate -WritableUserSid $(if($child -eq 'state'){$identity.User}else{$null}) }
}
if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) { throw 'Python 3.11 or newer was not found.' }
    $PythonExe = $pythonCommand.Source
}
$PythonExe = [IO.Path]::GetFullPath($PythonExe)
if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf)) { throw 'Python executable was not found.' }
$versionText = (& $PythonExe -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>$null)
if ($LASTEXITCODE -ne 0 -or [version]$versionText -lt [version]'3.11') { throw 'Python 3.11 or newer is required.' }
if ([string]::IsNullOrWhiteSpace($RuntimeManifestPath) -or [string]::IsNullOrWhiteSpace($RuntimeManifestSha256)) { throw 'A pinned runtime manifest path and SHA-256 are required.' }
$RuntimeManifestPath = [IO.Path]::GetFullPath($RuntimeManifestPath)
if (-not (Test-Path -LiteralPath $RuntimeManifestPath -PathType Leaf)) { throw 'Runtime manifest was not found.' }
New-Item -ItemType Directory -Force -Path $resolvedRoot,(Join-Path $resolvedRoot 'state'),(Join-Path $resolvedRoot 'transactions') | Out-Null
Set-ClosedDirectoryTrust -Path (Join-Path $resolvedRoot 'state') -CurrentUserSid $identity.User -AllowCurrentUserWrite
Set-ClosedDirectoryTrust -Path (Join-Path $resolvedRoot 'transactions') -CurrentUserSid $identity.User
$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutPath = Join-Path $desktop 'Aevrynd Game Mode.lnk'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $sourceRoot '..\..'))
$previousPythonPath = $env:PYTHONPATH
try {
    $env:PYTHONPATH = $repositoryRoot
    & $PythonExe -B -m tools.game_mode_controller.deployment_transaction recover $sourceRoot $resolvedRoot $expectedSourceManifestSha256 $shortcutPath
    if ($LASTEXITCODE -ne 0) { throw 'Previous deployment recovery failed.' }
    & $PythonExe -B -c "from tools.game_mode_controller.process_guard import load_runtime_manifest; import sys; load_runtime_manifest(sys.argv[1], expected_node_id='main', expected_sha256=sys.argv[2])" $RuntimeManifestPath $RuntimeManifestSha256
    if ($LASTEXITCODE -ne 0) { throw 'Pinned runtime manifest validation failed.' }
} finally {
    if ($null -eq $previousPythonPath) { Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue } else { $env:PYTHONPATH = $previousPythonPath }
}

# 先產生 config/shortcut stage；任何 COM 或路徑錯誤都發生在 core swap 之前。
$postId = [Guid]::NewGuid().ToString('N')
$postStage = Join-Path $resolvedRoot "staging\post-$postId"
$postPrefix = [IO.Path]::GetFullPath((Join-Path $resolvedRoot 'staging')).TrimEnd('\') + '\'
if (-not ([IO.Path]::GetFullPath($postStage)).StartsWith($postPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Post-install staging path escaped the fixed root.' }
New-Item -ItemType Directory -Force -Path $postStage | Out-Null
$stagedConfig = Join-Path $postStage 'node.json'
$stagedRuntimeManifest = Join-Path $postStage 'runtime-manifest.json'
Copy-Item -LiteralPath $RuntimeManifestPath -Destination $stagedRuntimeManifest
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $stagedRuntimeManifest).Hash.ToLowerInvariant() -cne $RuntimeManifestSha256) { throw 'Staged runtime manifest validation failed.' }
$nodeConfig = [ordered]@{ schemaVersion = 2; nodeId = $NodeId; runtimeManifest = 'runtime-manifest.json'; runtimeManifestSha256 = $RuntimeManifestSha256; pythonExecutable = $PythonExe }
[IO.File]::WriteAllText($stagedConfig, ($nodeConfig | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
$stagedShortcut = Join-Path $postStage 'Aevrynd Game Mode.lnk'
$launcher = Join-Path $resolvedRoot 'scripts\Launch-GameModeController.ps1'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($stagedShortcut)
$shortcut.TargetPath = (Get-Command pwsh.exe -ErrorAction Stop).Source
$shortcut.Arguments = "-NoProfile -STA -WindowStyle Hidden -File `"$launcher`""
$shortcut.WorkingDirectory = $resolvedRoot
$shortcut.Description = 'Aevrynd three-node game mode controller'
$shortcut.Save()
if (-not (Test-Path -LiteralPath $stagedShortcut -PathType Leaf)) { throw 'Staged desktop shortcut validation failed.' }

# 單一 durable commit intent 同時涵蓋 core、node config、runtime manifest 與 desktop shortcut。
$previousPythonPath = $env:PYTHONPATH
try {
    $env:PYTHONPATH = $repositoryRoot
    & $PythonExe -B -m tools.game_mode_controller.deployment_transaction prepare $sourceRoot $resolvedRoot $expectedSourceManifestSha256 $stagedConfig $stagedRuntimeManifest $stagedShortcut $shortcutPath
    if ($LASTEXITCODE -ne 0) { throw 'Transactional deployment preparation failed.' }
    if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) { throw 'Verified launcher is missing after core preparation.' }
    foreach ($component in @('app','scripts','config','manifests','transactions')) { Set-ClosedDirectoryTrust -Path (Join-Path $resolvedRoot $component) -CurrentUserSid $identity.User }
    & $PythonExe -B -m tools.game_mode_controller.deployment_transaction commit $sourceRoot $resolvedRoot $expectedSourceManifestSha256 $shortcutPath
    if ($LASTEXITCODE -ne 0) { throw 'Transactional deployment commit failed.' }
} catch {
    & $PythonExe -B -m tools.game_mode_controller.deployment_transaction rollback $sourceRoot $resolvedRoot $expectedSourceManifestSha256 $shortcutPath 2>$null
    throw
} finally {
    if ($null -eq $previousPythonPath) { Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue }
    else { $env:PYTHONPATH = $previousPythonPath }
    if (Test-Path -LiteralPath $postStage -PathType Container) { Remove-Item -LiteralPath $postStage -Recurse -Force }
}

Write-Output "Transactionally installed the authenticated controller and pinned runtime for $NodeId."
if ($RegisterStartup) {
    & (Join-Path $resolvedRoot 'scripts\Register-Startup.ps1') -InstallRoot $resolvedRoot
    Write-Output 'Registered limited AtLogOn startup.'
} else {
    Write-Output 'Startup was not registered; run Register-Startup.ps1 or reinstall with -RegisterStartup.'
}
} finally {
    if ($null -ne $bootstrapRoot -and (Test-Path -LiteralPath $bootstrapRoot -PathType Container)) {
        Remove-Item -LiteralPath $bootstrapRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
