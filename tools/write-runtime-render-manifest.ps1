param(
    [Parameter(Mandatory = $true)][string]$Python,
    [string]$AssetRoot = "",
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"
$toolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $toolRoot
if ([string]::IsNullOrWhiteSpace($AssetRoot)) {
    $AssetRoot = Join-Path $repoRoot "engine\assets\realistic-cc0-first-batch"
}
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $AssetRoot "cooked\runtime-render-manifest.json"
}

& $Python (Join-Path $toolRoot "write-runtime-render-manifest.py") --asset-root $AssetRoot --output $Output
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
