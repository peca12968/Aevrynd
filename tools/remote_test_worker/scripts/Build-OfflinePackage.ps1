[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $PSScriptRoot '..\artifacts'
}
$moduleRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$stageRoot = Join-Path $env:TEMP ('AevryndRemoteTestWorker-' + [guid]::NewGuid().ToString('N'))
$payloadRoot = Join-Path $stageRoot 'remote_test_worker'
$zipPath = Join-Path $OutputDirectory 'Aevrynd-Remote-Test-Worker-Phase1-reviewed.zip'
try {
    New-Item -ItemType Directory -Path $payloadRoot -Force | Out-Null
    # 測試、快取、artifact 與任何金鑰都不得進入離線包。
    Get-ChildItem -LiteralPath $moduleRoot -File | Where-Object Name -NotIn @('run_tests.py') | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $payloadRoot
    }
    New-Item -ItemType Directory -Path (Join-Path $payloadRoot 'scripts') | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $moduleRoot 'scripts') -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $payloadRoot 'scripts')
    }
    Get-ChildItem -LiteralPath $payloadRoot -Recurse -File | Where-Object {
        $_.Name -notmatch '\.(zip|db|key)$' -and $_.FullName -notmatch '[\\/]artifacts[\\/]'
    } | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{
            file = $_.FullName.Substring($payloadRoot.Length + 1).Replace('\', '/')
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payloadRoot 'package-manifest.json') -Encoding UTF8
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    Compress-Archive -LiteralPath $payloadRoot -DestinationPath $zipPath -Force
    Write-Output $zipPath
} finally {
    if (Test-Path -LiteralPath $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force }
}
