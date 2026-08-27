$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake is not installed.' }
$build = Join-Path $root 'engine\build'
$buildParallelism = [Math]::Min(28, [Environment]::ProcessorCount)
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $build --config Release --parallel $buildParallelism
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
$dist = Join-Path $root 'games\riftwarden\dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$sourceExecutable = Join-Path $build 'bin\Release\Riftwarden-DX12.exe'
$unsignedSourceHash = (Get-FileHash -LiteralPath $sourceExecutable -Algorithm SHA256).Hash
$manifest = Get-Content -LiteralPath (Join-Path $root 'games\riftwarden\game.json') -Raw | ConvertFrom-Json
$versionedDemo = Join-Path $dist "Riftwarden-Demo-v$($manifest.version)-Engine-v$($manifest.engineVersion).exe"
$unsignedHashRecord = "$versionedDemo.unsigned.sha256"
if (Test-Path -LiteralPath $versionedDemo) {
    if (Test-Path -LiteralPath $unsignedHashRecord) {
        $versionedUnsignedHash = (Get-Content -LiteralPath $unsignedHashRecord -Raw).Trim()
    } elseif ((Get-AuthenticodeSignature -LiteralPath $versionedDemo).Status -eq 'NotSigned') {
        $versionedUnsignedHash = (Get-FileHash -LiteralPath $versionedDemo -Algorithm SHA256).Hash
    } else {
        throw "Signed versioned demo has no unsigned hash record: $versionedDemo"
    }
    if ($unsignedSourceHash -ne $versionedUnsignedHash) { throw "Versioned demo already exists with different content: $versionedDemo" }
} else {
    Copy-Item -LiteralPath $sourceExecutable -Destination $versionedDemo
}
Set-Content -LiteralPath $unsignedHashRecord -Value $unsignedSourceHash -Encoding ascii
if ($env:AEVRYND_SIGNING_THUMBPRINT) {
    & (Join-Path $root 'sign-windows-release.ps1') -Path @($sourceExecutable, $versionedDemo)
    if ($LASTEXITCODE -ne 0) { throw 'Release signing failed.' }
} else {
    Write-Warning 'Release is unsigned. Smart App Control can block this executable. Configure AEVRYND_SIGNING_THUMBPRINT before publishing.'
}
Copy-Item -LiteralPath $sourceExecutable -Destination (Join-Path $dist 'Riftwarden-DX12.exe') -Force
Copy-Item -Path (Join-Path $build 'bin\Release\*.dll') -Destination $dist -Force
Write-Host "Built: $dist\Riftwarden-DX12.exe" -ForegroundColor Green
Write-Host "Versioned demo: $versionedDemo" -ForegroundColor Green
& (Join-Path $root 'publish-demos.ps1')
