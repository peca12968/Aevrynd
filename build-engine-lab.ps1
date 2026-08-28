$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake is not installed.' }
$build = Join-Path $root 'engine\build'
$buildParallelism = [Math]::Min(4, [Environment]::ProcessorCount)
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $build --config Release --target EngineLab --parallel $buildParallelism
if ($LASTEXITCODE -ne 0) { throw 'Engine Lab build failed.' }
$dist = Join-Path $root 'games\engine_lab\dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'bin\Release\Aevrynd-Engine-Lab.exe') -Destination (Join-Path $dist 'Aevrynd-Engine-Lab.exe') -Force
$manifest = Get-Content -LiteralPath (Join-Path $root 'games\engine_lab\game.json') -Raw | ConvertFrom-Json
$versionedDemo = Join-Path $dist "Aevrynd-Engine-Demo-v$($manifest.engineVersion)-Lab-v$($manifest.version).exe"
Copy-Item -LiteralPath (Join-Path $build 'bin\Release\Aevrynd-Engine-Lab.exe') -Destination $versionedDemo -Force
Copy-Item -Path (Join-Path $build 'bin\Release\*.dll') -Destination $dist -Force
$distAssets = Join-Path $dist 'assets'
New-Item -ItemType Directory -Force -Path $distAssets | Out-Null
Copy-Item -Path (Join-Path $root 'games\engine_lab\assets\*') -Destination $distAssets -Recurse -Force
$materialPacks = Join-Path $root 'engine\assets\material-packs'
if (Test-Path -LiteralPath $materialPacks) {
    Copy-Item -LiteralPath $materialPacks -Destination $distAssets -Recurse -Force
}
Write-Host "Built: $dist\Aevrynd-Engine-Lab.exe" -ForegroundColor Green
Write-Host "Versioned demo: $versionedDemo" -ForegroundColor Green
& (Join-Path $root 'publish-demos.ps1')
