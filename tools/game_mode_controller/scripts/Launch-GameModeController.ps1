param([string]$InstallRoot = "$env:ProgramData\Aevrynd\GameModeController")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($InstallRoot)
$expected = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\GameModeController")
if (-not [string]::Equals($root.TrimEnd('\'), $expected.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) { throw 'InstallRoot is not allowlisted.' }
$configuration = Get-Content -Raw -LiteralPath (Join-Path $root 'config\node.json') | ConvertFrom-Json
if ($configuration.schemaVersion -ne 2 -or $configuration.nodeId -cne 'main') { throw 'Node configuration is invalid.' }
$python = [IO.Path]::GetFullPath([string]$configuration.pythonExecutable)
$tray = Join-Path $root 'scripts\GameModeTray.ps1'
if (-not (Test-Path -LiteralPath $python -PathType Leaf) -or -not (Test-Path -LiteralPath $tray -PathType Leaf)) { throw 'Controller runtime is unavailable.' }

$startInfo = [Diagnostics.ProcessStartInfo]::new($python)
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.WorkingDirectory = $root
$minimalEnvironment = @('SystemRoot','WINDIR','TEMP','TMP','USERPROFILE','LOCALAPPDATA','ProgramData')
$capturedEnvironment = @{}
foreach ($name in $minimalEnvironment) { $value = [Environment]::GetEnvironmentVariable($name); if (-not [string]::IsNullOrEmpty($value)) { $capturedEnvironment[$name] = $value } }
$startInfo.Environment.Clear()
foreach ($name in $capturedEnvironment.Keys) { $startInfo.Environment[$name] = $capturedEnvironment[$name] }
$startInfo.ArgumentList.Add('-B'); $startInfo.ArgumentList.Add('-m'); $startInfo.ArgumentList.Add('tools.game_mode_controller.runner')
$startInfo.ArgumentList.Add('serve-main'); $startInfo.ArgumentList.Add('--install-root'); $startInfo.ArgumentList.Add($root)
$startInfo.Environment['PYTHONPATH'] = (Join-Path $root 'app')
[void][Diagnostics.Process]::Start($startInfo)

# runner 與 tray 都只繼承明確 allowlist，不帶入 controller、LINE 或其他程序秘密。
$pwsh = (Get-Command pwsh.exe -ErrorAction Stop).Source
$trayInfo = [Diagnostics.ProcessStartInfo]::new($pwsh)
$trayInfo.UseShellExecute = $false; $trayInfo.CreateNoWindow = $true; $trayInfo.WorkingDirectory = $root
$trayInfo.Environment.Clear()
foreach ($name in $capturedEnvironment.Keys) { $trayInfo.Environment[$name] = $capturedEnvironment[$name] }
foreach ($argument in @('-NoProfile','-STA','-WindowStyle','Hidden','-File',$tray,'-InstallRoot',$root)) { $trayInfo.ArgumentList.Add($argument) }
[void][Diagnostics.Process]::Start($trayInfo)
