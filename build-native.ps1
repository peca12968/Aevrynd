$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake is not installed.' }
$build = Join-Path $root 'engine\build'
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $build --config Release --parallel 1
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
$dist = Join-Path $root 'games\riftwarden\dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'bin\Release\Riftwarden-DX12.exe') -Destination (Join-Path $dist 'Riftwarden-DX12.exe') -Force
$manifest = Get-Content -LiteralPath (Join-Path $root 'games\riftwarden\game.json') -Raw | ConvertFrom-Json
$versionedDemo = Join-Path $dist "Riftwarden-Demo-v$($manifest.version)-Engine-v$($manifest.engineVersion).exe"
Copy-Item -LiteralPath (Join-Path $build 'bin\Release\Riftwarden-DX12.exe') -Destination $versionedDemo -Force
Copy-Item -Path (Join-Path $build 'bin\Release\*.dll') -Destination $dist -Force
Write-Host "Built: $dist\Riftwarden-DX12.exe" -ForegroundColor Green
Write-Host "Versioned demo: $versionedDemo" -ForegroundColor Green
