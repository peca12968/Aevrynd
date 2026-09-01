param(
    [Parameter(Mandatory = $true)]
    [string]$Python,
    [string]$ExpectedPythonVersion = '3.12.13',
    [string]$ExpectedPillowVersion = '12.3.0',
    [string]$ReceiptDirectory = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$generator = Join-Path $PSScriptRoot 'generate-character-materials.py'
$committedRoot = Join-Path $projectRoot 'games\character_lab\assets\material-packs\original'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("aevrynd-character-material-determinism-" + [Guid]::NewGuid().ToString('N'))
$materialIds = @('skin', 'lips', 'sclera', 'iris', 'cloth', 'leather', 'hair', 'eye', 'metal')
$channelIds = @('base-color', 'roughness', 'normal-dx')

function Get-FileSha256([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

function Assert-ExactFile([string]$Expected, [string]$Actual, [string]$Label) {
    $expectedBytes = [IO.File]::ReadAllBytes($Expected)
    $actualBytes = [IO.File]::ReadAllBytes($Actual)
    if ($expectedBytes.Length -ne $actualBytes.Length) {
        throw "$Label byte count differs: $($expectedBytes.Length) != $($actualBytes.Length)"
    }
    if (-not [Linq.Enumerable]::SequenceEqual($expectedBytes, $actualBytes)) {
        throw "$Label bytes differ: $(Get-FileSha256 $Expected) != $(Get-FileSha256 $Actual)"
    }
}

function Get-ManifestContract([string]$Root) {
    $manifestPath = Join-Path $Root 'manifest.json'
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    if ($manifest.schemaVersion -ne 1 -or $manifest.seed -ne 11432473 -or $manifest.license -ne 'CC0-1.0') {
        throw 'Character material manifest identity is invalid.'
    }
    $actualIds = @($manifest.materials | ForEach-Object { [string]$_.id })
    if (($actualIds -join '|') -cne ($materialIds -join '|')) {
        throw "Character material exact-set differs: $($actualIds -join ',')"
    }

    $files = [Collections.Generic.List[string]]::new()
    foreach ($material in $manifest.materials) {
        $channelNames = @($material.channels.PSObject.Properties.Name)
        if (($channelNames -join '|') -cne ($channelIds -join '|')) {
            throw "Material '$($material.id)' channel exact-set differs: $($channelNames -join ',')"
        }
        foreach ($channelId in $channelIds) {
            $channel = $material.channels.$channelId
            $relativePath = [string]$channel.path
            $expectedPath = "$($material.id)/$channelId.png"
            if ($relativePath -cne $expectedPath) { throw "Unexpected material path: $relativePath" }
            $absolutePath = Join-Path $Root $relativePath
            if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) { throw "Missing material payload: $relativePath" }
            $actualLength = (Get-Item -LiteralPath $absolutePath).Length
            $actualSha = Get-FileSha256 $absolutePath
            if ($actualLength -ne [int64]$channel.bytes -or $actualSha -cne ([string]$channel.sha256).ToLowerInvariant()) {
                throw "Material payload receipt mismatch: $relativePath"
            }
            $files.Add($relativePath)
        }
    }
    if ($files.Count -ne 27 -or (@($files | Sort-Object -Unique).Count -ne 27)) {
        throw "Character material payload exact-set must contain 27 unique PNG files, received $($files.Count)."
    }
    return [pscustomobject]@{ ManifestPath = $manifestPath; Files = @($files) }
}

try {
    foreach ($required in @($Python, $generator, (Join-Path $committedRoot 'manifest.json'))) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file is missing: $required" }
    }
    $runtimeScript = "import json, platform, sys, PIL; print(json.dumps({'executable':sys.executable,'python':platform.python_version(),'pillow':PIL.__version__}, sort_keys=True))"
    $runtime = & $Python -c $runtimeScript
    if ($LASTEXITCODE -ne 0 -or @($runtime).Count -ne 1) { throw 'Python runtime identity query failed.' }
    $runtimeIdentity = $runtime | ConvertFrom-Json
    if ($runtimeIdentity.python -cne $ExpectedPythonVersion -or $runtimeIdentity.pillow -cne $ExpectedPillowVersion) {
        throw "Python dependency drift: expected Python $ExpectedPythonVersion / Pillow $ExpectedPillowVersion, received Python $($runtimeIdentity.python) / Pillow $($runtimeIdentity.pillow)."
    }

    $committedContract = Get-ManifestContract $committedRoot
    [IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
    $runs = [Collections.Generic.List[object]]::new()
    foreach ($runName in @('run-a', 'run-b', 'run-c')) {
        $runRoot = Join-Path $temporaryRoot $runName
        # 每次從相同已封存輸入建立獨立目錄，避免前一次程序狀態影響下一次。
        [IO.Directory]::CreateDirectory($runRoot) | Out-Null
        foreach ($child in Get-ChildItem -LiteralPath $committedRoot -Force) {
            Copy-Item -LiteralPath $child.FullName -Destination (Join-Path $runRoot $child.Name) -Recurse
        }
        $process = Start-Process -FilePath $Python -ArgumentList @($generator, $runRoot) -Wait -PassThru -NoNewWindow
        if ($process.ExitCode -ne 0) { throw "Character material generation failed for $runName with exit $($process.ExitCode)." }
        $contract = Get-ManifestContract $runRoot
        $runs.Add([pscustomobject]@{ Name = $runName; Root = $runRoot; Contract = $contract })
    }

    foreach ($run in $runs) {
        Assert-ExactFile $committedContract.ManifestPath $run.Contract.ManifestPath "$($run.Name) committed manifest"
        foreach ($relativePath in $committedContract.Files) {
            Assert-ExactFile (Join-Path $committedRoot $relativePath) (Join-Path $run.Root $relativePath) "$($run.Name) committed $relativePath"
        }
    }
    foreach ($relativePath in @('manifest.json') + $committedContract.Files) {
        Assert-ExactFile (Join-Path $runs[0].Root $relativePath) (Join-Path $runs[1].Root $relativePath) "run-a/run-b $relativePath"
        Assert-ExactFile (Join-Path $runs[0].Root $relativePath) (Join-Path $runs[2].Root $relativePath) "run-a/run-c $relativePath"
    }

    $receipt = [ordered]@{
        schemaVersion = 1
        result = 'PASS'
        generator = 'tools/generate-character-materials.py'
        pythonExecutable = [string]$runtimeIdentity.executable
        pythonVersion = [string]$runtimeIdentity.python
        pillowVersion = [string]$runtimeIdentity.pillow
        manifestSha256 = Get-FileSha256 $committedContract.ManifestPath
        materialCount = 9
        pngCount = 27
        independentProcessCount = 3
        committedOutputsExact = $true
        seed = 11432473
    }
    if ($ReceiptDirectory) {
        [IO.Directory]::CreateDirectory($ReceiptDirectory) | Out-Null
        $receiptPath = Join-Path $ReceiptDirectory 'character-material-determinism-receipt.json'
        [IO.File]::WriteAllText($receiptPath, (($receipt | ConvertTo-Json -Depth 4) + "`n"), [Text.UTF8Encoding]::new($false))
    }
    Write-Output "CHARACTER_MATERIAL_DETERMINISM=PASS python=$($runtimeIdentity.python) pillow=$($runtimeIdentity.pillow) runs=3 materials=9 png=27 manifest=$(Get-FileSha256 $committedContract.ManifestPath)"
} finally {
    # 僅清理由本次測試建立的唯一暫存目錄，正式資產永遠保持唯讀。
    if (Test-Path -LiteralPath $temporaryRoot) { [IO.Directory]::Delete($temporaryRoot, $true) }
}
