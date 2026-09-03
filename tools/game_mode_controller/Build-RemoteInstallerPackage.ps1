[CmdletBinding()]
param([string]$OutputPath = (Join-Path $PWD 'Aevrynd-GameMode-Remote-Installer-v6-reviewed.zip'))

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$output = [IO.Path]::GetFullPath($OutputPath)
if ([IO.Path]::GetExtension($output) -cne '.zip' -or (Test-Path -LiteralPath $output)) { throw 'OutputPath must be a new ZIP file.' }
$outputParent = [IO.Path]::GetDirectoryName($output)
if (-not (Test-Path -LiteralPath $outputParent -PathType Container)) { New-Item -ItemType Directory -Path $outputParent -Force | Out-Null }
$packageRoot = [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
$packageFiles = @(
    '__init__.py','acceptance.py','api.py','atomic_store.py','coordinator.py','credential_store.py',
    'deployment_transaction.py','installer_transaction.py','inventory.py','line_notifications.py','models.py',
    'process_guard.py','runner.py','security.py','state_machine.py','windows_runtime.py','source-manifest.json',
    'Build-RemoteInstallerPackage.ps1','REMOTE-INSTALL.md',
    'scripts/Capture-MainRuntimeManifest.ps1','scripts/Collect-Readiness.ps1','scripts/GameModeTray.ps1',
    'scripts/Install-RemoteGameMode.ps1','scripts/Install.ps1','scripts/InstallTrust.psm1',
    'scripts/Launch-GameModeController.ps1','scripts/Register-HubFirewall.ps1','scripts/Register-Startup.ps1',
    'scripts/Set-NodeCredentials.ps1'
)
if (@($packageFiles | Sort-Object -Unique).Count -ne $packageFiles.Count) { throw 'Closed package list contains duplicates.' }
$stage = Join-Path ([IO.Path]::GetTempPath()) ('AevryndGameModeRemote-' + [Guid]::NewGuid().ToString('N'))
try {
    $stagedPackage = Join-Path $stage 'tools\game_mode_controller'
    New-Item -ItemType Directory -Path $stagedPackage -Force | Out-Null
    foreach ($relative in $packageFiles) {
        if ($relative -notmatch '^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$') { throw 'Closed package path is invalid.' }
        $source = [IO.Path]::GetFullPath((Join-Path $packageRoot $relative.Replace('/','\')))
        if (-not $source.StartsWith($packageRoot + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Closed package path escaped source root.' }
        $item = Get-Item -LiteralPath $source -Force -ErrorAction Stop
        if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $item.Length -gt 1048576) { throw 'Closed package source is unsafe.' }
        $destination = Join-Path $stagedPackage $relative.Replace('/','\')
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        [IO.File]::Copy($source,$destination,$false)
    }
    $entries = @($packageFiles | Sort-Object | ForEach-Object {
        $relative = $_
        $path = Join-Path $stagedPackage $relative.Replace('/','\')
        [ordered]@{ path = $relative; size = (Get-Item -LiteralPath $path).Length; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() }
    })
    $manifestPath = Join-Path $stagedPackage 'package-manifest.json'
    $manifestHashPath = Join-Path $stagedPackage 'package-manifest.sha256'
    $manifestJson = [ordered]@{ schemaVersion = 1; files = $entries } | ConvertTo-Json -Depth 5
    [IO.File]::WriteAllText($manifestPath,$manifestJson,[Text.UTF8Encoding]::new($false))
    $manifestHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $manifestPath).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($manifestHashPath,$manifestHash,[Text.UTF8Encoding]::new($false))
    foreach ($entry in $entries) {
        $path = Join-Path $stagedPackage $entry.path.Replace('/','\')
        if ((Get-Item -LiteralPath $path).Length -ne [int64]$entry.size -or
            (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() -cne [string]$entry.sha256) {
            throw 'Staged package identity validation failed.'
        }
    }
    Compress-Archive -LiteralPath (Join-Path $stage 'tools') -DestinationPath $output -CompressionLevel Optimal
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
Write-Output $output
