param(
    [string]$Configuration = "Release",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$testRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$nativeRoot = Split-Path -Parent (Split-Path -Parent $testRoot)
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $nativeRoot))
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $env:TEMP "aevrynd-gameplay-ux-replay"
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installation)) {
    throw "Visual C++ Build Tools were not found"
}
$developerShell = Join-Path $installation "Common7\Tools\Launch-VsDevShell.ps1"
& $developerShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

$executable = Join-Path $OutputDirectory "GameplayUxReplayWin32Test.exe"
$sources = @(
    (Join-Path $testRoot "GameplayUxReplayWin32.cpp"),
    (Join-Path $testRoot "GameplayUxReplayWin32Smoke.cpp")
)
$arguments = @(
    "/nologo", "/std:c++20", "/W4", "/WX", "/permissive-", "/utf-8",
    "/EHsc", "/O2", "/DNDEBUG",
    "/I$($repoRoot)\engine\native\include",
    "/I$nativeRoot\include",
    "/I$testRoot",
    "/Fe:$executable"
) + $sources + @("user32.lib")
& cl.exe @arguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $executable
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
