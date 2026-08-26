$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake is not installed.' }
& $cmake -S $root -B (Join-Path $root 'build') -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build (Join-Path $root 'build') --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
$dist = Join-Path $root 'games\riftwarden\dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'build\bin\Release\Riftwarden-DX12.exe') -Destination (Join-Path $dist 'Riftwarden-DX12.exe') -Force
Copy-Item -Path (Join-Path $root 'build\bin\Release\*.dll') -Destination $dist -Force
Write-Host "Built: $dist\Riftwarden-DX12.exe" -ForegroundColor Green
