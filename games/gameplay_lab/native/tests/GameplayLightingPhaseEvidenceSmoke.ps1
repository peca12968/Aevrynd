Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'GameplayLightingPhaseEvidence.ps1')
Add-Type -AssemblyName System.Drawing

function Write-SolidCapture {
    param([string]$Path, [int]$Value)
    $bitmap = New-Object System.Drawing.Bitmap 64, 64
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb($Value, $Value, $Value))
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$root = Join-Path ([IO.Path]::GetTempPath()) ("aevrynd-lighting-phase-" + [Guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    foreach ($view in @('first', 'third', 'god')) {
        Write-SolidCapture (Join-Path $root "$view-day.png") 220
        Write-SolidCapture (Join-Path $root "$view-dusk.png") 150
        Write-SolidCapture (Join-Path $root "$view-night.png") 100
    }

    $result = @(Assert-GameplayLightingPhaseLuminance $root 'passing-fixture')
    if ($result.Count -ne 3 -or @($result | Where-Object { $_.duskNightRatio -lt 1.05 }).Count -ne 0) {
        throw 'Passing lighting phase fixture did not return complete evidence'
    }

    Write-SolidCapture (Join-Path $root 'god-dusk.png') 104
    $rejected = $false
    try {
        $null = Assert-GameplayLightingPhaseLuminance $root 'later-run-hostile-fixture'
    }
    catch {
        if ($_.Exception.Message -notmatch 'label=later-run-hostile-fixture view=god') { throw }
        $rejected = $true
    }
    if (-not $rejected) {
        throw 'Later-run hostile lighting phase fixture unexpectedly passed'
    }

    Write-Output 'GameplayLightingPhaseEvidenceSmoke PASS passingViews=3 hostileLaterRunRejected=1 minimumRatio=1.05'
}
finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
