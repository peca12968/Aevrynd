param([string]$InstallRoot = "$env:ProgramData\Aevrynd\GameModeController")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$root = [IO.Path]::GetFullPath($InstallRoot)
$expected = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\GameModeController")
if (-not [string]::Equals($root.TrimEnd('\'), $expected.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) {
    throw 'InstallRoot is not allowlisted.'
}

$createdNew = $false
$mutex = [Threading.Mutex]::new($true, 'Local\AevryndGameModeTray', [ref]$createdNew)
if (-not $createdNew) { return }
$notify = [Windows.Forms.NotifyIcon]::new()
$notify.Icon = [Drawing.SystemIcons]::Application
$notify.Text = 'Aevrynd Game Mode'
$notify.Visible = $true
$menu = [Windows.Forms.ContextMenuStrip]::new()
$statusItems = @{}
foreach ($node in @('Main PC', 'Taichung PC', 'Taoyuan PC')) {
    $item = $menu.Items.Add("$node`: waiting for signed agent")
    $item.Enabled = $false
    $statusItems[$node] = $item
}
[void]$menu.Items.Add('-')
$blocked = $menu.Items.Add('Controls unavailable: agent adapter not installed')
$blocked.Enabled = $false
[void]$menu.Items.Add('-')
$exitItem = $menu.Items.Add('Exit tray')
$exitItem.Add_Click({ [Windows.Forms.Application]::Exit() })
$notify.ContextMenuStrip = $menu

# 第一工作包只提供安全 UI 外殼。沒有認證 agent 時不提供可點擊控制，避免 loopback 偽造請求。
$timer = [Windows.Forms.Timer]::new()
$timer.Interval = 5000
$timer.Add_Tick({
    $cache = Join-Path $root 'state\cluster-cache.json'
    if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) { return }
    try {
        $document = Get-Content -Raw -LiteralPath $cache | ConvertFrom-Json
        if ($document.schemaVersion -ne 1) { return }
        foreach ($entry in @($document.nodes)) {
            $name = [string]$entry.displayName
            if ($statusItems.ContainsKey($name)) {
                $statusItems[$name].Text = "$name`: gameMode $($entry.gameMode), Qwen $($entry.qwenState), $($entry.connection)"
            }
        }
    } catch { }
})
$timer.Start()
try { [Windows.Forms.Application]::Run() }
finally {
    $timer.Dispose()
    $notify.Visible = $false
    $notify.Dispose()
    $menu.Dispose()
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
