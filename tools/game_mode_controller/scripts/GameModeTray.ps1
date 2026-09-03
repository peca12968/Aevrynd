param([string]$InstallRoot = "$env:ProgramData\Aevrynd\GameModeController")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$root = [IO.Path]::GetFullPath($InstallRoot)
$expected = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\GameModeController")
if (-not [string]::Equals($root.TrimEnd('\'), $expected.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) { throw 'InstallRoot is not allowlisted.' }
$configuration = Get-Content -Raw -LiteralPath (Join-Path $root 'config\node.json') | ConvertFrom-Json
if ($configuration.schemaVersion -ne 2 -or $configuration.nodeId -cne 'main') { throw 'Node configuration is invalid.' }
$python = [IO.Path]::GetFullPath([string]$configuration.pythonExecutable)
if (-not (Test-Path -LiteralPath $python -PathType Leaf) -or ([IO.Path]::GetFileName($python) -notin @('python.exe', 'python3.exe'))) { throw 'Python executable is invalid.' }
$appRoot = Join-Path $root 'app'

$createdNew = $false
$mutex = [Threading.Mutex]::new($true, 'Local\AevryndGameModeTray', [ref]$createdNew)
if (-not $createdNew) { return }
$notify = [Windows.Forms.NotifyIcon]::new()
$notify.Icon = [Drawing.SystemIcons]::Application
$notify.Text = 'Aevrynd Game Mode'
$notify.Visible = $true
$menu = [Windows.Forms.ContextMenuStrip]::new()
$statusItems = @{}
$labels = [ordered]@{ main = 'Main PC'; taichung = 'Taichung PC'; taoyuan = 'Taoyuan PC' }
foreach ($nodeId in $labels.Keys) {
    $item = $menu.Items.Add("$($labels[$nodeId]): connecting")
    $item.Enabled = $false
    $statusItems[$nodeId] = $item
}
[void]$menu.Items.Add('-')

function Invoke-Controller([string[]]$Arguments) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new($python)
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.WorkingDirectory = $root
    $startInfo.Environment.Clear()
    foreach ($name in @('SystemRoot','WINDIR','TEMP','TMP','USERPROFILE','LOCALAPPDATA','ProgramData')) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if (-not [string]::IsNullOrEmpty($value)) { $startInfo.Environment[$name] = $value }
    }
    $startInfo.Environment['PYTHONPATH'] = $appRoot
    foreach ($argument in @('-B','-m','tools.game_mode_controller.runner') + $Arguments) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
    $process = $null
    try {
        $process = [Diagnostics.Process]::Start($startInfo)
        if ($null -eq $process) { throw 'Controller command failed.' }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(5000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw 'Controller command timed out.'
        }
        $output = $stdoutTask.GetAwaiter().GetResult()
        [void]$stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw 'Controller command failed.' }
        return $output.TrimEnd("`r", "`n")
    } finally {
        if ($null -ne $process) { $process.Dispose() }
    }
}

foreach ($nodeId in $labels.Keys) {
    $sub = [Windows.Forms.ToolStripMenuItem]::new("Control $($labels[$nodeId])")
    $enable = $sub.DropDownItems.Add('Enable game mode (stop Qwen)')
    $disable = $sub.DropDownItems.Add('Disable game mode (restore Qwen)')
    $capturedNode = $nodeId
    $enable.Add_Click({ try { [void](Invoke-Controller @('set','--target',$capturedNode,'--game-mode','on')) } catch { [Windows.Forms.MessageBox]::Show('The signed controller command failed.','Aevrynd Game Mode') | Out-Null } }.GetNewClosure())
    $disable.Add_Click({ try { [void](Invoke-Controller @('set','--target',$capturedNode,'--game-mode','off')) } catch { [Windows.Forms.MessageBox]::Show('The signed controller command failed.','Aevrynd Game Mode') | Out-Null } }.GetNewClosure())
    [void]$menu.Items.Add($sub)
}
[void]$menu.Items.Add('-')
$exitItem = $menu.Items.Add('Exit tray')
$exitItem.Add_Click({ [Windows.Forms.Application]::Exit() })
$notify.ContextMenuStrip = $menu

$timer = [Windows.Forms.Timer]::new()
$timer.Interval = 3000
$pollInProgress = $false
$retryMilliseconds = 3000
function Test-ControllerListener {
    $client = [Net.Sockets.TcpClient]::new()
    try {
        $connect = $client.ConnectAsync('127.0.0.1',8766)
        return $connect.Wait(250) -and $client.Connected
    } catch { return $false }
    finally { $client.Dispose() }
}
$timer.Add_Tick({
    if ($pollInProgress) { return }
    $pollInProgress = $true
    try {
        if (-not (Test-ControllerListener)) { throw 'Controller listener is unavailable.' }
        $snapshot = (Invoke-Controller @('status')) | ConvertFrom-Json
        foreach ($nodeId in $labels.Keys) {
            $entry = $snapshot.$nodeId
            if ($null -ne $entry) {
                $connection = if ($entry.online) { 'online' } else { 'offline' }
                $statusItems[$nodeId].Text = "$($labels[$nodeId]): gameMode $($entry.state.game_mode), Qwen $($entry.state.qwen_state), $connection"
            }
        }
        $retryMilliseconds = 3000
    } catch {
        foreach ($nodeId in $labels.Keys) { $statusItems[$nodeId].Text = "$($labels[$nodeId]): controller unavailable" }
        # listener 不可用時不啟動 Python，並有界退避到60秒。
        $retryMilliseconds = [Math]::Min(60000,[Math]::Max(3000,$retryMilliseconds * 2))
    } finally {
        $timer.Interval = $retryMilliseconds
        $pollInProgress = $false
    }
})
$timer.Start()
try { [Windows.Forms.Application]::Run() }
finally { $timer.Dispose(); $notify.Visible = $false; $notify.Dispose(); $menu.Dispose(); $mutex.ReleaseMutex(); $mutex.Dispose() }
