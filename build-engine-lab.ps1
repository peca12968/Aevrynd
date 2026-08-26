$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake is not installed.' }
$build = Join-Path $root 'engine\build'
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $build --config Release --target EngineLab --parallel
if ($LASTEXITCODE -ne 0) { throw 'Engine Lab build failed.' }
$dist = Join-Path $root 'games\engine_lab\dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'bin\Release\Aevrynd-Engine-Lab.exe') -Destination (Join-Path $dist 'Aevrynd-Engine-Lab.exe') -Force
Copy-Item -Path (Join-Path $build 'bin\Release\*.dll') -Destination $dist -Force
Write-Host "Built: $dist\Aevrynd-Engine-Lab.exe" -ForegroundColor Green
