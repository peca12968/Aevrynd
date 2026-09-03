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

$createdSupervisor = $false
$supervisorMutex = [Threading.Mutex]::new($true,'Local\AevryndGameModeControllerSupervisor',[ref]$createdSupervisor)
if (-not $createdSupervisor) { $supervisorMutex.Dispose(); return }

$startInfo = [Diagnostics.ProcessStartInfo]::new($python)
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.WorkingDirectory = $root
$minimalEnvironment = @('SystemRoot','WINDIR','TEMP','TMP','USERPROFILE','LOCALAPPDATA','ProgramData')
$capturedEnvironment = @{}
foreach ($name in $minimalEnvironment) { $value = [Environment]::GetEnvironmentVariable($name); if (-not [string]::IsNullOrEmpty($value)) { $capturedEnvironment[$name] = $value } }
$startInfo.Environment.Clear()
foreach ($name in $capturedEnvironment.Keys) { $startInfo.Environment[$name] = $capturedEnvironment[$name] }
$startInfo.ArgumentList.Add('-B'); $startInfo.ArgumentList.Add('-m'); $startInfo.ArgumentList.Add('tools.game_mode_controller.runner')
$startInfo.ArgumentList.Add('serve-main'); $startInfo.ArgumentList.Add('--install-root'); $startInfo.ArgumentList.Add($root)
$startInfo.Environment['PYTHONPATH'] = (Join-Path $root 'app')
$stateRoot = Join-Path $root 'state'
$stateItem = Get-Item -LiteralPath $stateRoot -Force -ErrorAction Stop
if (-not $stateItem.PSIsContainer -or ($stateItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Controller state directory is invalid.' }
$logRoot = Join-Path $stateRoot 'logs'
$logItem = Get-Item -LiteralPath $logRoot -Force -ErrorAction Stop
if (-not $logItem.PSIsContainer -or ($logItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Controller log directory is invalid.' }

function Assert-SecureLogAcl([string]$Path) {
    $acl = Get-Acl -LiteralPath $Path
    $ownerSid = ([Security.Principal.NTAccount]$acl.Owner).Translate([Security.Principal.SecurityIdentifier]).Value
    $currentSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    if (-not $acl.AreAccessRulesProtected -or $ownerSid -notin @('S-1-5-18','S-1-5-32-544')) { throw 'Controller log ACL is invalid.' }
    $writeMask = 0x000D0156
    foreach ($rule in $acl.Access) {
        $sid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
        if ($rule.AccessControlType -eq [Security.AccessControl.AccessControlType]::Allow -and $sid -notin @('S-1-5-18','S-1-5-32-544',$currentSid) -and ([int64]$rule.FileSystemRights -band $writeMask)) { throw 'Controller log has an untrusted writer.' }
    }
}
Assert-SecureLogAcl $logRoot

function Open-SecureLog([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $item.PSIsContainer) { throw 'Controller log path is invalid.' }
    Assert-SecureLogAcl $Path
    # 有限權限排程只開啟 installer 預先封閉的檔案，不要求 ChangePermissions。
    return [IO.FileStream]::new($Path,[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::Read)
}

$stdout = Open-SecureLog (Join-Path $logRoot 'hub.stdout.log')
$stderr = Open-SecureLog (Join-Path $logRoot 'hub.stderr.log')
$hub = $null
try {
    $hub = [Diagnostics.Process]::Start($startInfo)
    if ($null -eq $hub) { throw 'Controller hub failed to start.' }
    # 排程工作必須持續監督真正的長期程序，讓 Task Scheduler 的 restart policy 看見退出碼。
    $stdoutCopy = $hub.StandardOutput.BaseStream.CopyToAsync($stdout)
    $stderrCopy = $hub.StandardError.BaseStream.CopyToAsync($stderr)

    # tray 保持獨立單例；既有 mutex 會拒絕第二份。
    $pwsh = [IO.Path]::GetFullPath('C:\Program Files\PowerShell\7\pwsh.exe')
    $pwshItem = Get-Item -LiteralPath $pwsh -Force -ErrorAction Stop
    if ($pwshItem.PSIsContainer -or ($pwshItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Stable PowerShell 7 executable is unavailable.' }
    $trayInfo = [Diagnostics.ProcessStartInfo]::new($pwsh)
    $trayInfo.UseShellExecute = $false; $trayInfo.CreateNoWindow = $true; $trayInfo.WorkingDirectory = $root
    $trayInfo.Environment.Clear()
    foreach ($name in $capturedEnvironment.Keys) { $trayInfo.Environment[$name] = $capturedEnvironment[$name] }
    foreach ($argument in @('-NoProfile','-STA','-WindowStyle','Hidden','-File',$tray,'-InstallRoot',$root)) { $trayInfo.ArgumentList.Add($argument) }
    [void][Diagnostics.Process]::Start($trayInfo)

    $hub.WaitForExit()
    $stdoutCopy.GetAwaiter().GetResult()
    $stderrCopy.GetAwaiter().GetResult()
    $exitCode = $hub.ExitCode
} finally {
    if ($null -ne $hub) { $hub.Dispose() }
    $stdout.Dispose()
    $stderr.Dispose()
    $supervisorMutex.ReleaseMutex()
    $supervisorMutex.Dispose()
}
exit $exitCode
