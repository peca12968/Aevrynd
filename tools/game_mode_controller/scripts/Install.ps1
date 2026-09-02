[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateSet('main', 'taichung', 'taoyuan')]
    [string]$NodeId,
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\GameModeController",
    [string]$PythonExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedRoot = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\GameModeController").TrimEnd('\')
$resolvedRoot = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
if (-not [string]::Equals($expectedRoot, $resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'InstallRoot must be the fixed ProgramData Aevrynd GameModeController directory.'
}
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sourceManifest = Join-Path $sourceRoot 'source-manifest.json'
$expectedSourceManifestSha256 = '82feba111a663f788cbf6cbf58c1bc6d99571eb2c3f1d58489b41d1641d5a6db'
if (-not (Test-Path -LiteralPath $sourceManifest -PathType Leaf)) { throw 'Closed source manifest is missing.' }
$actualSourceManifestSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceManifest).Hash.ToLowerInvariant()
if ($actualSourceManifestSha256 -cne $expectedSourceManifestSha256) { throw 'Closed source manifest hash validation failed.' }

if (-not $PSCmdlet.ShouldProcess($resolvedRoot, "transactionally install Game Mode Controller for node $NodeId")) { return }

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator privileges are required.' }
if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) { throw 'Python 3.11 or newer was not found.' }
    $PythonExe = $pythonCommand.Source
}
$PythonExe = [IO.Path]::GetFullPath($PythonExe)
if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf)) { throw 'Python executable was not found.' }
$versionText = (& $PythonExe -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>$null)
if ($LASTEXITCODE -ne 0 -or [version]$versionText -lt [version]'3.11') { throw 'Python 3.11 or newer is required.' }

# 先產生 config/shortcut stage；任何 COM 或路徑錯誤都發生在 core swap 之前。
$postId = [Guid]::NewGuid().ToString('N')
$postStage = Join-Path $resolvedRoot "staging\post-$postId"
$postPrefix = [IO.Path]::GetFullPath((Join-Path $resolvedRoot 'staging')).TrimEnd('\') + '\'
if (-not ([IO.Path]::GetFullPath($postStage)).StartsWith($postPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Post-install staging path escaped the fixed root.' }
New-Item -ItemType Directory -Force -Path $postStage | Out-Null
$stagedConfig = Join-Path $postStage 'node.json'
$nodeConfig = [ordered]@{ schemaVersion = 1; nodeId = $NodeId }
[IO.File]::WriteAllText($stagedConfig, ($nodeConfig | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutPath = Join-Path $desktop 'Aevrynd Game Mode.lnk'
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

# Python transaction 會再次驗 source，copy 後驗 stage，並保持 backup 直到 config/shortcut 一起成功。
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $sourceRoot '..\..'))
$previousPythonPath = $env:PYTHONPATH
$config = Join-Path $resolvedRoot 'config'
$state = Join-Path $resolvedRoot 'state'
$nodeConfigPath = Join-Path $config 'node.json'
$configExisted = Test-Path -LiteralPath $nodeConfigPath -PathType Leaf
$shortcutExisted = Test-Path -LiteralPath $shortcutPath -PathType Leaf
if ($configExisted) { Copy-Item -LiteralPath $nodeConfigPath -Destination (Join-Path $postStage 'node.backup') }
if ($shortcutExisted) { Copy-Item -LiteralPath $shortcutPath -Destination (Join-Path $postStage 'shortcut.backup') }
try {
    $env:PYTHONPATH = $repositoryRoot
    & $PythonExe -B -m tools.game_mode_controller.installer_transaction prepare $sourceRoot $resolvedRoot $expectedSourceManifestSha256
    if ($LASTEXITCODE -ne 0) { throw 'Transactional core preparation failed.' }
    if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) { throw 'Verified launcher is missing after core preparation.' }
    New-Item -ItemType Directory -Force -Path $config,$state | Out-Null
    [IO.File]::Move($stagedConfig, $nodeConfigPath, $true)
    [IO.File]::Move($stagedShortcut, $shortcutPath, $true)
    & $PythonExe -B -m tools.game_mode_controller.installer_transaction finalize $sourceRoot $resolvedRoot $expectedSourceManifestSha256
    if ($LASTEXITCODE -ne 0) { throw 'Transactional installation finalize failed.' }
} catch {
    & $PythonExe -B -m tools.game_mode_controller.installer_transaction rollback $sourceRoot $resolvedRoot $expectedSourceManifestSha256 2>$null
    if ($configExisted) { Copy-Item -LiteralPath (Join-Path $postStage 'node.backup') -Destination $nodeConfigPath -Force }
    elseif (Test-Path -LiteralPath $nodeConfigPath -PathType Leaf) { Remove-Item -LiteralPath $nodeConfigPath -Force }
    if ($shortcutExisted) { Copy-Item -LiteralPath (Join-Path $postStage 'shortcut.backup') -Destination $shortcutPath -Force }
    elseif (Test-Path -LiteralPath $shortcutPath -PathType Leaf) { Remove-Item -LiteralPath $shortcutPath -Force }
    throw
} finally {
    if ($null -eq $previousPythonPath) { Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue }
    else { $env:PYTHONPATH = $previousPythonPath }
    if (Test-Path -LiteralPath $postStage -PathType Container) { Remove-Item -LiteralPath $postStage -Recurse -Force }
}

# authenticated agent/runtime adapter 尚未交付，因此保持 controls 與登入排程 fail closed。
Write-Output "Transactionally installed controller core and desktop launcher for $NodeId."
Write-Output 'Startup registration remains blocked until the signed runtime manifest and agent adapter are installed.'
