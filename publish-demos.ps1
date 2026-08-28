$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$demoDirectory = Join-Path $projectRoot 'Demo'
$demoAssets = Join-Path $demoDirectory 'assets'
$publishMutex = [System.Threading.Mutex]::new($false, 'Local\AevryndDemoPublisher')
$publishLockTaken = $false

try {
    $publishLockTaken = $publishMutex.WaitOne([TimeSpan]::FromMinutes(5))
    if (-not $publishLockTaken) { throw 'Timed out waiting for the demo publisher lock.' }

    New-Item -ItemType Directory -Force -Path $demoDirectory | Out-Null
    New-Item -ItemType Directory -Force -Path $demoAssets | Out-Null

function Copy-DemoFile {
    param(
        [Parameter(Mandatory)] [System.IO.FileInfo] $Source,
        [Parameter(Mandatory)] [string] $DestinationDirectory,
        [switch] $Immutable
    )
    $destination = Join-Path $DestinationDirectory $Source.Name
    if (Test-Path -LiteralPath $destination) {
        $sourceHash = (Get-FileHash -LiteralPath $Source.FullName -Algorithm SHA256).Hash
        $destinationHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        if ($sourceHash -eq $destinationHash) { return }
        if ($Immutable) { throw "Versioned demo already exists with different content: $destination" }
    }
    try {
        Copy-Item -LiteralPath $Source.FullName -Destination $DestinationDirectory -Force
    } catch [System.IO.IOException] {
        Write-Warning "Skipped a currently running runtime file: $destination"
    }
}

$distributionDirectories = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'games') -Directory |
    Where-Object Name -ne 'character_lab' |
    ForEach-Object { Join-Path $_.FullName 'dist' } |
    Where-Object { Test-Path -LiteralPath $_ }

foreach ($distributionDirectory in $distributionDirectories) {
    Get-ChildItem -LiteralPath $distributionDirectory -File -Filter '*.exe' |
        ForEach-Object { Copy-DemoFile -Source $_ -DestinationDirectory $demoDirectory -Immutable:($_.BaseName -match 'Demo-v') }
    Get-ChildItem -LiteralPath $distributionDirectory -File -Filter '*.dll' |
        ForEach-Object { Copy-DemoFile -Source $_ -DestinationDirectory $demoDirectory }
    $assets = Join-Path $distributionDirectory 'assets'
    if (Test-Path -LiteralPath $assets) {
        Copy-Item -Path (Join-Path $assets '*') -Destination $demoAssets -Recurse -Force
    }
}

$demoModelPacks = Join-Path $demoAssets 'model-packs'
if (Test-Path -LiteralPath $demoModelPacks) {
    Get-ChildItem -LiteralPath $demoModelPacks -Directory -Filter 'source' -Recurse | Remove-Item -Recurse -Force
}

    $executables = Get-ChildItem -LiteralPath $demoDirectory -File -Filter '*.exe' | Sort-Object Name
    Write-Host "Published $($executables.Count) executable demos to $demoDirectory" -ForegroundColor Green
    $executables | ForEach-Object { Write-Host "  $($_.Name)" }
} finally {
    if ($publishLockTaken) { $publishMutex.ReleaseMutex() }
    $publishMutex.Dispose()
}
