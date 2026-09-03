#requires -RunAsAdministrator
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('taichung', 'taoyuan')]
    [string]$NodeId,

    [Parameter(Mandatory)]
    [string]$ReadinessManifest
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$installRoot = 'C:\ProgramData\Aevrynd\RemoteTestWorker'
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$manifestPath = Join-Path $sourceRoot 'package-manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'package_manifest_missing' }
if (-not (Test-Path -LiteralPath $ReadinessManifest -PathType Leaf)) { throw 'readiness_manifest_missing' }
$readiness = Get-Content -LiteralPath $ReadinessManifest -Raw | ConvertFrom-Json
$readinessFields = @($readiness.PSObject.Properties.Name | Sort-Object)
if (($readinessFields -join ',') -ne 'capabilities,nodeId,policy,ready,schemaVersion' -or
    $readiness.schemaVersion -isnot [int] -or $readiness.schemaVersion -ne 1 -or
    $readiness.nodeId -isnot [string] -or $readiness.nodeId -cne $NodeId -or
    $readiness.ready -isnot [bool] -or -not $readiness.ready) {
    throw 'readiness_rejected'
}
$entries = @(Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json)
$seen = @{}
foreach ($entry in $entries) {
    $fields = @($entry.PSObject.Properties.Name | Sort-Object)
    if (($fields -join ',') -ne 'file,sha256' -or $entry.file -isnot [string] -or
        $entry.sha256 -isnot [string] -or $entry.sha256 -cnotmatch '^[0-9a-f]{64}$' -or
        $entry.file -notmatch '^[A-Za-z0-9_.\-/]+$' -or $entry.file -match '(^|/)\.\.(/|$)' -or
        $seen.ContainsKey($entry.file)) {
        throw 'unsafe_manifest_entry'
    }
    $seen[$entry.file] = $true
    $candidate = Join-Path $sourceRoot ([string]$entry.file).Replace('/', '\')
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw 'payload_missing' }
    $actual = (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne [string]$entry.sha256) { throw 'payload_hash_mismatch' }
}
$actualFiles = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File | ForEach-Object {
    $_.FullName.Substring($sourceRoot.Length + 1).Replace('\', '/')
} | Where-Object { $_ -ne 'package-manifest.json' } | Sort-Object)
$manifestFiles = @($entries | ForEach-Object { [string]$_.file } | Sort-Object)
if (($actualFiles -join "`n") -cne ($manifestFiles -join "`n")) { throw 'payload_file_set_mismatch' }
# Phase 1 只安裝已驗證 payload；不開防火牆、不建立網路 listener，也不攜帶金鑰。
$staging = "$installRoot.staging"
if (Test-Path -LiteralPath $staging) {
    $stagingItem = Get-Item -LiteralPath $staging -Force
    if (($stagingItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'unsafe_staging_reparse' }
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Path $staging -Force | Out-Null
foreach ($entry in $entries) {
    $relative = ([string]$entry.file).Replace('/', '\')
    $destination = Join-Path $staging $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRoot $relative) -Destination $destination
}
Set-Content -LiteralPath (Join-Path $staging 'node-id.txt') -Value $NodeId -Encoding ASCII
Write-Output 'staged_verified_no_service_no_firewall_no_credentials'
