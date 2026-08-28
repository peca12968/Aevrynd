$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmakeExecutable = 'C:\Program Files\CMake\bin\cmake.exe'
$buildDirectory = Join-Path $projectRoot 'engine\build'
$buildParallelism = [Math]::Min(4, [Environment]::ProcessorCount)
& $cmakeExecutable -S $projectRoot -B $buildDirectory -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmakeExecutable --build $buildDirectory --config Release --target FrontierWorld --parallel $buildParallelism
if ($LASTEXITCODE -ne 0) { throw 'Frontier World build failed.' }
$distributionDirectory = Join-Path $projectRoot 'games\frontier_world\dist'
New-Item -ItemType Directory -Force -Path $distributionDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $buildDirectory 'bin\Release\Aevrynd-Frontier-World.exe') -Destination (Join-Path $distributionDirectory 'Aevrynd-Frontier-World.exe') -Force
$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'games\frontier_world\game.json') -Raw | ConvertFrom-Json
$versionedDemo = Join-Path $distributionDirectory "Aevrynd-Frontier-World-Demo-v$($manifest.version).exe"
Copy-Item -LiteralPath (Join-Path $buildDirectory 'bin\Release\Aevrynd-Frontier-World.exe') -Destination $versionedDemo -Force
Copy-Item -Path (Join-Path $buildDirectory 'bin\Release\*.dll') -Destination $distributionDirectory -Force
$distributionAssets = Join-Path $distributionDirectory 'assets'
New-Item -ItemType Directory -Force -Path $distributionAssets | Out-Null
Copy-Item -Path (Join-Path $buildDirectory 'bin\Release\assets\material-packs') -Destination $distributionAssets -Recurse -Force
Copy-Item -Path (Join-Path $buildDirectory 'bin\Release\assets\model-packs') -Destination $distributionAssets -Recurse -Force
$distributionModelPacks = Join-Path $distributionAssets 'model-packs'
Get-ChildItem -LiteralPath $distributionModelPacks -Directory -Filter 'source' -Recurse | Remove-Item -Recurse -Force
Copy-Item -Path (Join-Path $projectRoot 'games\frontier_world\assets\*') -Destination $distributionAssets -Recurse -Force
Write-Host "Built: $distributionDirectory\Aevrynd-Frontier-World.exe" -ForegroundColor Green
Write-Host "Versioned demo: $versionedDemo" -ForegroundColor Green
& (Join-Path $projectRoot 'publish-demos.ps1')
