param(
    [string]$Blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe',
    [string]$RuntimeManifest = (Join-Path $PSScriptRoot '../engine/assets/realistic-cc0-first-batch/runtime-asset-manifest.json')
)
$ErrorActionPreference = 'Stop'
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-first-batch-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temporary) | Out-Null
try {
    $inputRoot = Join-Path $temporary 'input'
    $outputRoot = Join-Path $temporary 'output'
    [IO.Directory]::CreateDirectory($inputRoot) | Out-Null
    $text = & (Join-Path $PSScriptRoot 'import-first-batch-assets.ps1') -InputRoot $inputRoot -OutputRoot $outputRoot -Blender $Blender -PlanOnly 2>&1
    if ($LASTEXITCODE -ne 2) { throw 'Missing-source plan did not fail closed.' }
    $plan = ($text -join "`n") | ConvertFrom-Json
    if (@($plan).Count -ne 10 -or @($plan | Where-Object status -ne 'missing-source').Count) { throw 'First-batch plan contract failed.' }
    $names = @($plan.assetName)
    if (@($names | Where-Object { $_ -cnotmatch '^[a-z0-9]+(?:-[a-z0-9]+)*$' }).Count -or (@($names | Sort-Object -Unique).Count -ne 10)) {
        throw 'English asset naming contract failed.'
    }
    if (-not (Test-Path -LiteralPath $Blender -PathType Leaf)) { throw 'Configured Blender executable is missing.' }
    $runtime = Get-Content -LiteralPath $RuntimeManifest -Raw | ConvertFrom-Json
    if ($runtime.memoryGate -ne 'pass' -or $runtime.memoryGateMethod -ne 'gltfpack-process-tree' -or $runtime.peakProcessTreeBytes -le 0 -or $runtime.peakProcessTreeBytes -ge $runtime.peakWorkingSetLimitBytes) {
        throw 'Runtime manifest process-tree memory admission contract failed.'
    }
    if ($runtime.assetCount -ne 12 -or @($runtime.assets).Count -ne 12 -or @($runtime.assets | Where-Object memoryGate -ne 'pass').Count) { throw 'Runtime per-asset admission contract failed.' }
    $assetRoot = Split-Path -Parent $RuntimeManifest
    $computedBytes = 0L
    foreach ($asset in $runtime.assets) {
        if ($asset.license -ne 'CC0-1.0' -or -not $asset.commercialUse -or $asset.officialPage -notmatch '^https://(?:polyhaven\.com/a/|ambientcg\.com/view\?id=)') { throw "Asset $($asset.assetName) source or license receipt failed." }
        foreach ($file in $asset.files) {
            $path = Join-Path $assetRoot $file.path
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256.ToLowerInvariant()) { throw "Runtime file receipt failed: $($file.path)" }
            if ([IO.Path]::GetExtension($path) -match '(?i)^\.(?:jpe?g|png|tiff?|hdr)$' -and ($file.width -le 0 -or $file.height -le 0)) { throw "Runtime image dimension receipt failed: $($file.path)" }
            if ([int64]$file.bytes -gt [int64]::MaxValue - $computedBytes) { throw 'Runtime byte accounting overflow.' }
            $computedBytes += [int64]$file.bytes
        }
        if ($asset.kind -ne 'hdri') {
            $paths = @($asset.files.path)
            if (-not @($paths | Where-Object { $_ -match '(?i)(?:diff|color)' }).Count -or -not @($paths | Where-Object { $_ -match '(?i)(?:nor_gl|nor_dx|normal)' }).Count -or -not @($paths | Where-Object { $_ -match '(?i)(?:rough|_arm|_orm)' }).Count) { throw "Asset $($asset.assetName) PBR channels are incomplete." }
        }
        if ($asset.kind -eq 'mesh-pbr') {
            $levels = @($asset.lodLevels)
            if ($levels.Count -ne 3 -or $levels[1].triangles -ge $levels[0].triangles -or $levels[2].triangles -ge $levels[1].triangles) { throw "Asset $($asset.assetName) LOD receipt failed." }
            if ($asset.materialBinding -eq 'runtime-sidecar-integration-required') {
                $sidecarFile = @($asset.files | Where-Object path -match '\.materials\.json$')
                if ($sidecarFile.Count -ne 1) { throw "Asset $($asset.assetName) material sidecar receipt is missing." }
                $sidecar = Get-Content -LiteralPath (Join-Path $assetRoot $sidecarFile[0].path) -Raw | ConvertFrom-Json
                if ($sidecar.binding -ne 'runtime-sidecar' -or $sidecar.meshMaterialBinding -ne 'integration-required') { throw "Asset $($asset.assetName) material binding disclosure failed." }
                $sidecarDirectory = Split-Path -Parent $sidecarFile[0].path
                foreach ($material in $sidecar.materials) {
                    foreach ($channel in @('baseColor','normalGl','packedArm','roughness','surface')) {
                        $relative = $material.$channel
                        if (-not $relative) { continue }
                        $boundPath = (Join-Path $sidecarDirectory $relative).Replace('\','/')
                        if ($boundPath -match '(^|/)\.\.(/|$)' -or @($asset.files.path | Where-Object { $_ -ceq $boundPath }).Count -ne 1) { throw "Asset $($asset.assetName) material sidecar path is unsafe or unreceipted: $relative" }
                    }
                }
            }
        }
    }
    if ($runtime.runtimeBytes -lt $computedBytes -or $runtime.runtimeBytes - $computedBytes -gt 16MB) { throw 'Runtime byte accounting failed.' }

    $hostileRoot = Join-Path $temporary 'hostile-source'
    $hostileOutput = Join-Path $temporary 'hostile-output'
    [IO.Directory]::CreateDirectory($hostileRoot) | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $temporary 'outside.bin'), [byte[]](0,1,2,3))
    [IO.File]::WriteAllText((Join-Path $hostileRoot 'escape.gltf'), '{"asset":{"version":"2.0"},"buffers":[{"uri":"../outside.bin","byteLength":4}]}', [Text.UTF8Encoding]::new($false))
    $hostileCommand = Join-Path $temporary 'run-hostile.cmd'
    $hostileStdoutPath = Join-Path $temporary 'hostile-stdout.txt'
    $hostileStderrPath = Join-Path $temporary 'hostile-stderr.txt'
    $hostileCommandText = '@echo off' + "`r`n" + '"' + $Blender + '" --background --python-exit-code 1 --python "' + (Join-Path $PSScriptRoot 'blender/build-static-mesh-lods.py') + '" -- --source "' + (Join-Path $hostileRoot 'escape.gltf') + '" --output "' + $hostileOutput + '" --asset-name hostile-escape --ratios 1 >"' + $hostileStdoutPath + '" 2>"' + $hostileStderrPath + '"' + "`r`n" + 'exit /b %ERRORLEVEL%' + "`r`n"
    [IO.File]::WriteAllText($hostileCommand, $hostileCommandText, [Text.Encoding]::ASCII)
    & cmd.exe /d /c $hostileCommand
    $hostileExitCode = $LASTEXITCODE
    $hostileOutputText = (Get-Content -LiteralPath $hostileStdoutPath -Raw) + "`n" + (Get-Content -LiteralPath $hostileStderrPath -Raw)
    if ($hostileExitCode -eq 0 -or $hostileOutputText -notmatch 'glTF dependency escapes source directory' -or (Test-Path -LiteralPath (Join-Path $hostileOutput 'hostile-escape-lod0.glb'))) {
        throw 'glTF dependency path escape did not fail closed with the expected diagnostic.'
    }
    Write-Output "FIRST_BATCH_IMPORT_TEST=PASS assets=10 missingFailClosed=true pathEscapeRejected=true memoryGate=$($runtime.memoryGate) blender=$Blender"
} finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}
exit 0
