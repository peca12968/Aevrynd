param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string]$SourceCandidateRoot,
    [Parameter(Mandatory)][string]$DestinationCandidateRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedPriorManifest = '2979ce4ac66e36aa1de6e58a56f624d37f15fd9dd9a0ff2f3614812777cf40e6'
$expectedEngineLabSource = '204047c476f1580ca071e16fb93a846b9b0d4e35db609c6a08c5142f57c45230'
$expectedGameManifest = 'fd483e89a16d93e9e3600be1a97cb144f359bdd624a43d8ab956b9acfc3ac69d'
$expectedRunner = '3b608a1f65c105858394e6050482c4678eebea9f9aba658677e72533b79430eb'
$expectedPerformance = '95d1cc62c95b914a6f2fb8166d0f0b4a68644ce139a9deffa7804209166ad77b'

function Get-Sha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Write-CanonicalJson([string]$Path,[object]$Value) {
    $text = ($Value | ConvertTo-Json -Depth 32).Replace("`r`n","`n") + "`n"
    [IO.File]::WriteAllText($Path,$text,[Text.UTF8Encoding]::new($false))
}
function Assert-RelativePath([string]$Path) {
    if($Path -cnotmatch '^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$') { throw "Relative path rejected: $Path" }
}

$project = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
$source = [IO.Path]::GetFullPath($SourceCandidateRoot).TrimEnd('\')
$destination = [IO.Path]::GetFullPath($DestinationCandidateRoot).TrimEnd('\')
if(-not(Test-Path -LiteralPath $source -PathType Container)) { throw 'Source candidate missing.' }
if(Test-Path -LiteralPath $destination) { throw 'Destination candidate must not exist.' }
$priorManifestPath = Join-Path $source 'candidate.manifest.json'
if((Get-Sha256 $priorManifestPath) -cne $expectedPriorManifest) { throw 'Prior candidate manifest rejected.' }

$sourceFiles = [ordered]@{
    'evidence/source/EngineLabGame.cpp' = Join-Path $project 'games/engine_lab/native/src/EngineLabGame.cpp'
    'evidence/source/game.json' = Join-Path $project 'games/engine_lab/game.json'
}
if((Get-Sha256 $sourceFiles['evidence/source/EngineLabGame.cpp']) -cne $expectedEngineLabSource -or
   (Get-Sha256 $sourceFiles['evidence/source/game.json']) -cne $expectedGameManifest -or
   (Get-Sha256 (Join-Path $project 'games/engine_lab/native/tests/EngineLabShadowCaptureSmoke.ps1')) -cne $expectedRunner) {
    throw 'Canonical source snapshot rejected.'
}

Copy-Item -LiteralPath $source -Destination $destination -Recurse
foreach($file in Get-ChildItem -LiteralPath $destination -File -Recurse) { $file.IsReadOnly = $false }
foreach($relative in $sourceFiles.Keys) {
    Assert-RelativePath $relative
    $target = Join-Path $destination $relative
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
    Copy-Item -LiteralPath $sourceFiles[$relative] -Destination $target
}
$finalizerRelative = 'evidence/Finalize-EngineLabShadowV255Candidate.ps1'
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $destination $finalizerRelative)
$finalizerHash = Get-Sha256 $PSCommandPath

$roleBindings = @()
foreach($role in @('normal','gbv','fallback')) {
    $captureRelative = "captures/$role/shadow-demo-capture-receipt.json"
    $capturePath = Join-Path $destination $captureRelative
    $priorCaptureHash = Get-Sha256 $capturePath
    $capture = Get-Content -LiteralPath $capturePath -Raw | ConvertFrom-Json -DateKind String
    $capture.source.root = '.'
    $capture.source.engineLabSource.path = 'evidence/source/EngineLabGame.cpp'
    $capture.source.gameManifest.path = 'evidence/source/game.json'
    $capture.source.runner.path = 'evidence/EngineLabShadowCaptureSmoke.ps1'
    $capture.executable.path = 'Aevrynd-Engine-Lab.exe'
    $capture.command.file = 'Aevrynd-Engine-Lab.exe'
    $capture.command.workingDirectory = '.'
    $capture.command.captureDirectory = "captures/$role"
    $capture.performance.receipt = 'evidence/four-cascade-performance-receipt.json'
    Write-CanonicalJson $capturePath $capture
    $roleBindings += ,([ordered]@{
        role = $role
        priorCaptureReceiptSha256 = $priorCaptureHash
        finalizedCaptureReceipt = $captureRelative
        finalizedCaptureReceiptSha256 = Get-Sha256 $capturePath
    })
}

$refinalizationRelative = 'evidence/candidate-refinalization-receipt.json'
$refinalization = [ordered]@{
    schemaVersion = 1
    operation = 'candidate-relative-shadow-provenance-v1'
    priorCandidateManifestSha256 = $expectedPriorManifest
    sourceCommit = 'c033de38beb574d5a355ab3d588ea70a5fc7b213'
    finalizer = [ordered]@{ path=$finalizerRelative; sha256=$finalizerHash }
    sourceArchive = @(
        [ordered]@{path='evidence/source/EngineLabGame.cpp';sha256=$expectedEngineLabSource},
        [ordered]@{path='evidence/source/game.json';sha256=$expectedGameManifest},
        [ordered]@{path='evidence/EngineLabShadowCaptureSmoke.ps1';sha256=$expectedRunner}
    )
    entry = [ordered]@{path='Aevrynd-Engine-Lab.exe';sha256='90727775f127851ee4ea94c452c77dbe4b46db5a7769bac61dac5e46929de59d'}
    performance = [ordered]@{path='evidence/four-cascade-performance-receipt.json';sha256=$expectedPerformance}
    roles = $roleBindings
}
Write-CanonicalJson (Join-Path $destination $refinalizationRelative) $refinalization

$manifestPath = Join-Path $destination 'candidate.manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json -DateKind String
$records = @(
    Get-ChildItem -LiteralPath $destination -File -Recurse |
        Where-Object FullName -ne $manifestPath |
        ForEach-Object {
            $relative = $_.FullName.Substring($destination.Length + 1).Replace('\','/')
            Assert-RelativePath $relative
            [ordered]@{path=$relative;size=[int64]$_.Length;sha256=Get-Sha256 $_.FullName}
        } |
        Sort-Object path
)
$manifest.files = $records
Write-CanonicalJson $manifestPath $manifest

foreach($file in Get-ChildItem -LiteralPath $destination -File -Recurse) { $file.IsReadOnly = $true }
[ordered]@{
    schemaVersion = 1
    candidateRoot = $destination
    priorManifestSha256 = $expectedPriorManifest
    manifestSha256 = Get-Sha256 $manifestPath
    records = $records.Count
    files = @(Get-ChildItem -LiteralPath $destination -File -Recurse).Count
    writableFiles = @(Get-ChildItem -LiteralPath $destination -File -Recurse | Where-Object {-not $_.IsReadOnly}).Count
    finalizerSha256 = $finalizerHash
} | ConvertTo-Json -Depth 6
