$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake is not installed.' }
$engineName = -join ([char[]](0x5F15,0x64CE))
$gamesName = -join ([char[]](0x904A,0x6232))
$gameName = -join ([char[]](0x88C2,0x9699,0x5B88,0x671B,0x8005))
$build = Join-Path (Join-Path $root $engineName) 'build'
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
$dist = Join-Path (Join-Path (Join-Path $root $gamesName) $gameName) 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'bin\Release\Riftwarden-DX12.exe') -Destination (Join-Path $dist 'Riftwarden-DX12.exe') -Force
Copy-Item -Path (Join-Path $build 'bin\Release\*.dll') -Destination $dist -Force
Write-Host "Built: $dist\Riftwarden-DX12.exe" -ForegroundColor Green
