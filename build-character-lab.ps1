$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmakeExecutable = 'C:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmakeExecutable)) { throw 'CMake is not installed.' }
$buildDirectory = Join-Path $projectRoot 'engine\build'
$buildParallelism = [Math]::Min(8, [Environment]::ProcessorCount)
& $cmakeExecutable -S $projectRoot -B $buildDirectory -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmakeExecutable --build $buildDirectory --config Release --target CharacterLab --parallel $buildParallelism
if ($LASTEXITCODE -ne 0) { throw 'Character Lab build failed.' }
$distributionDirectory = Join-Path $projectRoot 'games\character_lab\dist'
New-Item -ItemType Directory -Force -Path $distributionDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $buildDirectory 'bin\Release\Aevrynd-Character-Lab.exe') -Destination (Join-Path $distributionDirectory 'Aevrynd-Character-Lab.exe') -Force
$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'games\character_lab\game.json') -Raw | ConvertFrom-Json
$versionedDemo = Join-Path $distributionDirectory "Aevrynd-Character-Lab-Demo-v$($manifest.version)-Engine-v$($manifest.engineVersion).exe"
Copy-Item -LiteralPath (Join-Path $buildDirectory 'bin\Release\Aevrynd-Character-Lab.exe') -Destination $versionedDemo -Force
Copy-Item -Path (Join-Path $buildDirectory 'bin\Release\*.dll') -Destination $distributionDirectory -Force
$assetSource = Join-Path $projectRoot 'games\character_lab\assets'
$assetDestination = Join-Path $distributionDirectory 'assets'
if (Test-Path -LiteralPath $assetDestination) { Remove-Item -LiteralPath $assetDestination -Recurse -Force }
Copy-Item -LiteralPath $assetSource -Destination $assetDestination -Recurse
Write-Host "Built: $distributionDirectory\Aevrynd-Character-Lab.exe" -ForegroundColor Green
Write-Host "Versioned demo: $versionedDemo" -ForegroundColor Green
& (Join-Path $projectRoot 'publish-demos.ps1')
