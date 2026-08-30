param([Parameter(Mandatory = $true)][string]$Python)

$ErrorActionPreference = "Stop"
$toolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
& $Python (Join-Path $toolRoot "test-runtime-render-manifest.py")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
