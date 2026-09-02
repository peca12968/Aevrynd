param([string]$InstallRoot = "$env:ProgramData\Aevrynd\GameModeController")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($InstallRoot)
$expected = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\GameModeController")
if (-not [string]::Equals($root.TrimEnd('\'), $expected.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) {
    throw 'InstallRoot is not allowlisted.'
}
$tray = Join-Path $root 'scripts\GameModeTray.ps1'
if (-not (Test-Path -LiteralPath $tray -PathType Leaf)) { throw 'Tray script is unavailable.' }

# 啟動器只負責重新開啟系統匣；不直接操作任何程序。
Start-Process -FilePath (Get-Command pwsh.exe -ErrorAction Stop).Source `
    -ArgumentList @('-NoProfile', '-STA', '-WindowStyle', 'Hidden', '-File', $tray) `
    -WorkingDirectory $root -WindowStyle Hidden
