[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('taichung','taoyuan')][string]$NodeId,
    [Parameter(Mandatory)][ValidateLength(1,512)][string]$ModelId,
    [string]$OutputDirectory = (Join-Path $PWD 'Aevrynd-GameMode-Readiness'),
    [string]$PythonExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand -or $pythonCommand.Source -like '*\WindowsApps\python.exe') { throw 'A real Python 3.11 or newer executable is required.' }
    $PythonExe = $pythonCommand.Source
}
$PythonExe = [IO.Path]::GetFullPath($PythonExe)
if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf) -or
    (((Get-Item -LiteralPath $PythonExe -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) -or
    [IO.Path]::GetFileName($PythonExe) -notin @('python.exe','python3.exe')) { throw 'Python executable is unavailable.' }
$listeners = @(Get-NetTCPConnection -State Listen -LocalPort 8080 -ErrorAction Stop | Select-Object -ExpandProperty OwningProcess -Unique)
if ($listeners.Count -ne 1 -or [int]$listeners[0] -le 0) { throw 'Exactly one Qwen listener must own TCP 8080.' }
$processId = [int]$listeners[0]
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $outputRoot) { throw 'OutputDirectory must not already exist.' }
$outputParent = [IO.DirectoryInfo]::new([IO.Path]::GetDirectoryName($outputRoot))
if (-not $outputParent.Exists -or ($outputParent.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'OutputDirectory parent is unsafe.' }
$stagingRoot = Join-Path $outputParent.FullName ('.Aevrynd-GameMode-Readiness-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stagingRoot | Out-Null
$manifest = Join-Path $stagingRoot "$NodeId-runtime-manifest.json"
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$previous = $env:PYTHONPATH
try {
    $env:PYTHONPATH = $sourceRoot
    & $PythonExe -B -m tools.game_mode_controller.runner capture-remote-manifest --output $manifest --node $NodeId --model-id $ModelId --pid $processId
    if ($LASTEXITCODE -ne 0) { throw 'Remote runtime capture failed closed.' }
    $digest = (Get-FileHash -Algorithm SHA256 -LiteralPath $manifest).Hash.ToLowerInvariant()
    $summary = [ordered]@{ schemaVersion = 1; nodeId = $NodeId; managedPid = $processId; runtimeManifestFile = [IO.Path]::GetFileName($manifest); runtimeManifestSha256 = $digest; ready = $true }
    [IO.File]::WriteAllText((Join-Path $stagingRoot 'readiness.json'),($summary | ConvertTo-Json -Compress),[Text.UTF8Encoding]::new($false))
    [IO.Directory]::Move($stagingRoot,$outputRoot)
} finally {
    if ($null -eq $previous) { Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue } else { $env:PYTHONPATH = $previous }
    $cleanup = [IO.Path]::GetFullPath($stagingRoot)
    $cleanupPrefix = $outputParent.FullName.TrimEnd('\') + '\.Aevrynd-GameMode-Readiness-'
    if (-not $cleanup.StartsWith($cleanupPrefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Readiness cleanup target escaped its fixed staging prefix.' }
    if (Test-Path -LiteralPath $cleanup -PathType Container) { Remove-Item -LiteralPath $cleanup -Recurse -Force }
}
Write-Output "Readiness captured for $NodeId. Copy only the readiness directory back to the trusted administrator."
