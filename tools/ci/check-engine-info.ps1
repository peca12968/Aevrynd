param(
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$InfoRoot = '',
    [switch]$RequireGeneratedInventory
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
if (-not $InfoRoot) { $InfoRoot = Join-Path (Split-Path -Parent $ProjectRoot) 'Info' }
$InfoRoot = [IO.Path]::GetFullPath($InfoRoot)

$cmakeText = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'CMakeLists.txt'))
if ($cmakeText -notmatch 'project\(Aevrynd\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Unable to read the engine version from CMakeLists.txt.'
}
$engineVersion = $Matches[1]
$documents = @('README.md', 'EngineFeatureCatalog.md', 'EngineTypeInventory.md', 'EngineChangeLog.md')
foreach ($name in $documents) {
    $path = Join-Path $InfoRoot $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing engine information document: $name" }
    $text = [IO.File]::ReadAllText($path)
    if ($text -notmatch "DocumentedEngineVersion:\s*$([regex]::Escape($engineVersion))") {
        throw "Engine information version is stale in $name; expected $engineVersion."
    }
}

$catalogText = [IO.File]::ReadAllText((Join-Path $InfoRoot 'EngineFeatureCatalog.md'))
$catalogRows = [regex]::Matches($catalogText, '(?m)^\|\s*(?:[1-9]|1[0-5])\s*\|').Count
if ($catalogRows -ne 15) { throw "Feature catalog must contain exactly 15 numbered subsystem rows; found $catalogRows." }

if ($RequireGeneratedInventory) {
    $generatedPath = Join-Path $InfoRoot 'EngineTypeInventory.generated.json'
    if (-not (Test-Path -LiteralPath $generatedPath -PathType Leaf)) { throw 'Generated engine inventory is missing.' }
    $inventory = Get-Content -LiteralPath $generatedPath -Raw | ConvertFrom-Json
    if ($inventory.schemaVersion -ne 1 -or $inventory.curatedBaselineDefinitionCount -ne 427) {
        throw 'Generated engine inventory has an unsupported schema or baseline.'
    }
    $temporaryInventory = Join-Path ([IO.Path]::GetTempPath()) ("aevrynd-engine-inventory-" + [Guid]::NewGuid().ToString('N') + '.json')
    try {
        & (Join-Path $InfoRoot 'Update-EngineInventory.ps1') -ProjectRoot $ProjectRoot -OutputPath $temporaryInventory | Out-Null
        $fresh = Get-Content -LiteralPath $temporaryInventory -Raw | ConvertFrom-Json
        $storedRows = @($inventory.byHeader | ForEach-Object { "$($_.header)|$($_.typeDefinitions)|$($_.aliases)|$($_.total)" })
        $freshRows = @($fresh.byHeader | ForEach-Object { "$($_.header)|$($_.typeDefinitions)|$($_.aliases)|$($_.total)" })
        if ($inventory.headerCount -ne $fresh.headerCount -or
            $inventory.typeDefinitionCount -ne $fresh.typeDefinitionCount -or
            $inventory.aliasDefinitionCount -ne $fresh.aliasDefinitionCount -or
            (Compare-Object $storedRows $freshRows)) {
            throw 'Generated engine inventory is stale; run Update-EngineInventory.ps1.'
        }
    } finally {
        Remove-Item -LiteralPath $temporaryInventory -Force -ErrorAction SilentlyContinue
    }
}

Write-Output "Engine information check passed for version $engineVersion."
