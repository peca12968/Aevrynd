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
$sourceExecutable = Join-Path $build 'bin\Release\Riftwarden-DX12.exe'
Copy-Item -LiteralPath $sourceExecutable -Destination (Join-Path $dist 'Riftwarden-DX12.exe') -Force
$manifest = Get-Content -LiteralPath (Join-Path $root 'games\riftwarden\game.json') -Raw | ConvertFrom-Json
$versionedDemo = Join-Path $dist "Riftwarden-Demo-v$($manifest.version)-Engine-v$($manifest.engineVersion).exe"
if (Test-Path -LiteralPath $versionedDemo) {
    $sourceHash = (Get-FileHash -LiteralPath $sourceExecutable -Algorithm SHA256).Hash
    $versionedHash = (Get-FileHash -LiteralPath $versionedDemo -Algorithm SHA256).Hash
    if ($sourceHash -ne $versionedHash) { throw "Versioned demo already exists with different content: $versionedDemo" }
} else {
    Copy-Item -LiteralPath $sourceExecutable -Destination $versionedDemo
}
Copy-Item -Path (Join-Path $build 'bin\Release\*.dll') -Destination $dist -Force
Write-Host "Built: $dist\Riftwarden-DX12.exe" -ForegroundColor Green
Write-Host "Versioned demo: $versionedDemo" -ForegroundColor Green
& (Join-Path $root 'publish-demos.ps1')
