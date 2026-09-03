param(
    [Parameter(Mandatory)][string]$SourceFile,
    [ValidateRange(60,86400)][int]$TtlSeconds = 3600,
    [ValidateRange(1,5)][int]$MaxDownloads = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$paths = Get-GatewayPaths
$null = Test-InstallationManifest -Paths $paths
$install = Get-Content -LiteralPath $paths.InstallFile -Raw | ConvertFrom-Json
$python = [IO.Path]::GetFullPath([string]$install.pythonExe)
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) { throw 'Installed Python is unavailable.' }
$source = [IO.Path]::GetFullPath($SourceFile)
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw 'Artifact source is unavailable.' }
$original = Get-Location
try {
    Set-Location -LiteralPath $paths.App
    & $python -B -m tools.line_gateway.artifact_store publish --source $source --ttl-seconds $TtlSeconds --max-downloads $MaxDownloads
    if ($LASTEXITCODE -ne 0) { throw 'Artifact publication failed.' }
} finally {
    Set-Location -LiteralPath $original
}
