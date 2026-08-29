param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string]$CandidateRoot,
    [string]$DemoRoot = '',
    [switch]$DryRun,
    [switch]$TestMode,
    [string]$TestExpectedCandidateManifestSha256 = '',
    [int]$TestExpectedCandidateFiles = 0,
    [string]$TestExpectedSourceCommit = '',
    [string]$TestExpectedEntrySha256 = '',
    [string]$TestExpectedBuilderScriptSha256 = '',
    [string]$TestExpectedCaptureRunnerSha256 = '',
    [string]$TestExpectedEngineLabSourceSha256 = '',
    [string]$TestExpectedGameManifestSha256 = '',
    [string]$TestExpectedPerformanceSha256 = '',
    [ValidateSet('', 'Swap', 'Copy', 'Hash', 'Verify', 'BeforeRename', 'Rename', 'BeforePointer', 'Pointer')]
    [string]$TestFailPoint = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:ExpectedCandidateManifestSha256 = '362b396a992cd1245c449be628991459c76ca0aa520cb6ea33c7255c79ff9d6b'
$script:ExpectedCandidateFiles = 133
$script:ExpectedSourceCommit = 'c033de38beb574d5a355ab3d588ea70a5fc7b213'
$script:ExpectedEntrySha256 = '90727775f127851ee4ea94c452c77dbe4b46db5a7769bac61dac5e46929de59d'
$script:ExpectedBuilderScriptSha256 = '5496d2d278392eeead9c5b0dc096c6bb00ea383fe992a4e1ffd12b7d56ce63c5'
$script:ExpectedBaseBuilderScriptSha256 = '017923050ced6c87ddf099364f5e8d62a1ab6bbf59632773f2161e9d91f85971'
$script:ExpectedCaptureRunnerSha256 = '3b608a1f65c105858394e6050482c4678eebea9f9aba658677e72533b79430eb'
$script:ExpectedPeClosureSha256 = '7619048a3a84ad4cf89d43986e0532525af3395835cb6739cbcd08d0caf6ced0'
$script:ExpectedReceipts = [ordered]@{
    normalRuntime='7f98354f2e154bb06f26707780c698eb0b8594ca9c1e4aad5dc2509d7e99ca77'
    normalCapture='62926d58be563fe95586b982da67c62035a6b302b5b7110949067186a9805957'
    gbvRuntime='b98a5d05b586ae46633a1eb8485a9b1c888c482f8ebea64efaeeb5b5d8322eef'
    gbvCapture='d2e933f0aa4089b5ad21555074566f8aa16d859fd1f993fb7e8a062015d18b39'
    fallbackRuntime='968f3daebc970c6ef9519f5ca9697336f244e063299f681e9e01f65442869558'
    fallbackCapture='f8cf6aeedab3576694c442c0bbfddd176656671e53ca415ee6e09508a29b5755'
}
$script:ExpectedRunIds = [ordered]@{normal='0d904654-6b2c-4d5d-a7ed-fea0a271b540';gbv='66865dd3-11fe-4fea-ae3e-c4b7fb9809a8';fallback='61b15216-f83d-4366-adfb-0d045b522755'}
$script:ExpectedEngineLabSourceSha256 = '204047c476f1580ca071e16fb93a846b9b0d4e35db609c6a08c5142f57c45230'
$script:ExpectedGameManifestSha256 = 'fd483e89a16d93e9e3600be1a97cb144f359bdd624a43d8ab956b9acfc3ac69d'
$script:ExpectedPerformanceSha256 = '95d1cc62c95b914a6f2fb8166d0f0b4a68644ce139a9deffa7804209166ad77b'
$script:ExpectedRefinalizationSha256 = '977cdd2d4cabe5047154db91e200e698f2c979c8bf65e0f871faa517cde6e1de'
$script:ExpectedFinalizerSha256 = '955e6b9c14d3d25059848246484633b0afdd7e6162cd095c0ee5811922860a71'

if (-not ('Aevrynd.EngineLabPublisherNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
namespace Aevrynd {
  [StructLayout(LayoutKind.Sequential)] public struct ByHandleFileInformation {
    public uint FileAttributes; public System.Runtime.InteropServices.ComTypes.FILETIME CreationTime;
    public System.Runtime.InteropServices.ComTypes.FILETIME LastAccessTime; public System.Runtime.InteropServices.ComTypes.FILETIME LastWriteTime;
    public uint VolumeSerialNumber; public uint FileSizeHigh; public uint FileSizeLow; public uint NumberOfLinks;
    public uint FileIndexHigh; public uint FileIndexLow;
  }
  public static class EngineLabPublisherNative {
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool GetFileInformationByHandle(SafeFileHandle hFile, out ByHandleFileInformation info);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern bool MoveFileEx(string existingName, string newName, uint flags);
  }
}
'@
}

function Get-FullPath([string]$Path) { [IO.Path]::GetFullPath($Path).TrimEnd('\') }
function Get-Sha256([string]$Path) {
    $stream = [IO.File]::Open($Path, 'Open', 'Read', 'Read')
    try { $sha = [Security.Cryptography.SHA256]::Create(); try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() } finally { $sha.Dispose() } } finally { $stream.Dispose() }
}
function Get-StringSha256([string]$Value) {
    $sha = [Security.Cryptography.SHA256]::Create(); try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace('-', '').ToLowerInvariant() } finally { $sha.Dispose() }
}
function Get-FileIdentity([IO.FileStream]$Stream) {
    $info = [Aevrynd.ByHandleFileInformation]::new()
    if (-not [Aevrynd.EngineLabPublisherNative]::GetFileInformationByHandle($Stream.SafeFileHandle, [ref]$info)) { throw "File identity query failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
    [ordered]@{ volume = $info.VolumeSerialNumber.ToString('x8'); file = ('{0:x8}{1:x8}' -f $info.FileIndexHigh, $info.FileIndexLow); links = [uint64]$info.NumberOfLinks }
}
function Assert-NoReparseParents([string]$Path) {
    $cursor = Get-FullPath $Path
    while ($cursor -and [IO.Path]::GetPathRoot($cursor) -ne $cursor) {
        if (Test-Path -LiteralPath $cursor) { if (((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse path rejected: $cursor" } }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
}
function Assert-SafeFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing file: $Path" }
    $item = Get-Item -LiteralPath $Path -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse file rejected: $Path" }
    if (@(Get-Item -LiteralPath $Path -Stream * | Where-Object Stream -ne ':$DATA').Count) { throw "ADS rejected: $Path" }
    $stream = [IO.File]::Open($Path, 'Open', 'Read', 'Read')
    try { $identity = Get-FileIdentity $stream; if ($identity.links -ne 1) { throw "Hardlink rejected: $Path" }; return $identity } finally { $stream.Dispose() }
}
function Assert-ExactProperties([object]$Object, [string[]]$Names, [string]$Label) {
    if ($null -eq $Object) { throw "$Label is null." }
    $actual = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($property in $Object.PSObject.Properties) { if (-not $actual.Add($property.Name)) { throw "$Label duplicate property." } }
    $expected = [Collections.Generic.HashSet[string]]::new($Names, [StringComparer]::Ordinal)
    if (-not $expected.SetEquals($actual)) { throw "$Label schema rejected." }
}
function Read-StrictJson([string]$Path, [string]$Label) {
    Assert-SafeFile $Path | Out-Null
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xef -and $bytes[1] -eq 0xbb -and $bytes[2] -eq 0xbf) { throw "$Label BOM rejected." }
    $raw = [Text.Encoding]::UTF8.GetString($bytes)
    $document = [Text.Json.JsonDocument]::Parse($raw)
    try {
        $walk = $null
        $walk = { param([Text.Json.JsonElement]$Element, [string]$At)
            if ($Element.ValueKind -eq [Text.Json.JsonValueKind]::Object) {
                $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
                foreach ($property in $Element.EnumerateObject()) { if (-not $names.Add($property.Name)) { throw "$Label duplicate JSON key: $At/$($property.Name)" }; & $walk $property.Value ($At + '/' + $property.Name) }
            } elseif ($Element.ValueKind -eq [Text.Json.JsonValueKind]::Array) { $i = 0; foreach ($value in $Element.EnumerateArray()) { & $walk $value ($At + '/' + $i); $i++ } }
        }
        & $walk $document.RootElement '$'
    } finally { $document.Dispose() }
    $raw | ConvertFrom-Json -DateKind String
}
function Write-CanonicalJson([string]$Path, [object]$Object) {
    $text = Get-CanonicalJsonText $Object
    [IO.File]::WriteAllText($Path, $text, [Text.UTF8Encoding]::new($false))
}
function Get-CanonicalJsonText([object]$Object){($Object|ConvertTo-Json -Depth 32).Replace("`r`n","`n")+"`n"}
function Test-RelativePath([string]$Path) {
    $Path -cmatch '^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$' -and -not [IO.Path]::IsPathRooted($Path) -and -not $Path.Contains(':') -and @($Path.Split('/') | Where-Object { $_ -in @('', '.', '..') }).Count -eq 0
}
function Get-ApprovedCandidatePolicy {
    if (-not $TestMode) {
        return [ordered]@{manifestSha256=$script:ExpectedCandidateManifestSha256;files=$script:ExpectedCandidateFiles;sourceCommit=$script:ExpectedSourceCommit;entrySha256=$script:ExpectedEntrySha256;builderScriptSha256=$script:ExpectedBuilderScriptSha256;captureRunnerSha256=$script:ExpectedCaptureRunnerSha256;engineLabSourceSha256=$script:ExpectedEngineLabSourceSha256;gameManifestSha256=$script:ExpectedGameManifestSha256;performanceSha256=$script:ExpectedPerformanceSha256}
    }
    $hashes=@($TestExpectedCandidateManifestSha256,$TestExpectedEntrySha256,$TestExpectedBuilderScriptSha256,$TestExpectedCaptureRunnerSha256,$TestExpectedEngineLabSourceSha256,$TestExpectedGameManifestSha256,$TestExpectedPerformanceSha256)
    if($TestExpectedCandidateFiles -lt 1 -or @($hashes | Where-Object { $_ -cnotmatch '^[0-9a-f]{64}$' }).Count -or $TestExpectedSourceCommit -cnotmatch '^[0-9a-f]{40}$') { throw 'Test approved candidate policy rejected.' }
    [ordered]@{manifestSha256=$TestExpectedCandidateManifestSha256;files=$TestExpectedCandidateFiles;sourceCommit=$TestExpectedSourceCommit;entrySha256=$TestExpectedEntrySha256;builderScriptSha256=$TestExpectedBuilderScriptSha256;captureRunnerSha256=$TestExpectedCaptureRunnerSha256;engineLabSourceSha256=$TestExpectedEngineLabSourceSha256;gameManifestSha256=$TestExpectedGameManifestSha256;performanceSha256=$TestExpectedPerformanceSha256}
}
function Assert-Integer([object]$Value,[string]$Label) {
    if($Value -isnot [int] -and $Value -isnot [long]) { throw "$Label integer rejected." }
}
function Assert-Number([object]$Value,[string]$Label) {
    if($Value -isnot [int] -and $Value -isnot [long] -and $Value -isnot [double] -and $Value -isnot [decimal]) { throw "$Label number rejected." }
    if(-not [double]::IsFinite([double]$Value)) { throw "$Label finite number rejected." }
}
function Assert-ShadowRuntime([object]$Runtime,[string]$Role,[int]$ExpectedCascades,[int]$ExpectedMask,[int64]$ExpectedBytes,[int[]]$Gpu) {
    $names=@('runId','phase','cascadeConfigured','cascadeCount','cascadeResolution','cascadeSplits','cascadeRendered','cascadeReused','farCascadeMask','farCascadeInstances','manualMasks','contactRequested','contactExecuted','contactEvaluationPasses','contactDisabledLuminance','contactEnabledLuminance','contactDarkenedLuminance','shadowBytes','gpuFrameNanoseconds','effective','gpuValidationRequested','gpuValidationAvailable','gpuValidationExecuted','gpuValidationPassed','debugLayerEnabled','gpuValidationErrors','gpuValidationWarnings','gpuValidationFirstErrorId','gpuValidationSource','gpuValidationEntry')
    Assert-ExactProperties $Runtime $names "$Role runtime"
    if([string]$Runtime.runId -cnotmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$' -or $Runtime.phase -cne 'shadow-v255') { throw "$Role runtime identity rejected." }
    foreach($name in @('cascadeConfigured','cascadeCount','cascadeResolution','farCascadeMask','farCascadeInstances','manualMasks','contactRequested','contactExecuted','contactEvaluationPasses','shadowBytes','gpuFrameNanoseconds','effective','gpuValidationRequested','gpuValidationAvailable','gpuValidationExecuted','gpuValidationPassed','debugLayerEnabled','gpuValidationErrors','gpuValidationWarnings','gpuValidationFirstErrorId')) { Assert-Integer $Runtime.$name "$Role runtime/$name" }
    foreach($name in @('contactDisabledLuminance','contactEnabledLuminance','contactDarkenedLuminance')) { Assert-Number $Runtime.$name "$Role runtime/$name" }
    if([int]$Runtime.cascadeConfigured -ne $ExpectedCascades -or [int]$Runtime.cascadeCount -ne $ExpectedCascades -or [int]$Runtime.cascadeResolution -ne 2048 -or [int]$Runtime.farCascadeMask -ne $ExpectedMask -or [int]$Runtime.farCascadeInstances -lt 1 -or [int]$Runtime.manualMasks -ne 1 -or [int]$Runtime.contactRequested -ne 1 -or [int]$Runtime.contactExecuted -ne 1 -or [int]$Runtime.contactEvaluationPasses -lt 1 -or [double]$Runtime.contactDarkenedLuminance -le 0 -or [int64]$Runtime.shadowBytes -ne $ExpectedBytes -or [int64]$Runtime.gpuFrameNanoseconds -lt 1 -or [int]$Runtime.effective -ne 1) { throw "$Role effective shadow gate rejected." }
    if($Runtime.gpuValidationSource -cne 'D3D12InfoQueue' -or $Runtime.gpuValidationEntry -cne 'EngineLabShadowCapture' -or [int]$Runtime.gpuValidationFirstErrorId -ne 0) { throw "$Role GPU source gate rejected." }
    $gpuActual=@([int]$Runtime.gpuValidationRequested,[int]$Runtime.gpuValidationAvailable,[int]$Runtime.gpuValidationExecuted,[int]$Runtime.debugLayerEnabled,[int]$Runtime.gpuValidationPassed,[int]$Runtime.gpuValidationErrors,[int]$Runtime.gpuValidationWarnings)
    if(@(Compare-Object $Gpu $gpuActual -SyncWindow 0).Count) { throw "$Role GPU counters rejected." }
    foreach($name in @('cascadeSplits','cascadeRendered','cascadeReused')) { if($Runtime.$name -isnot [array] -or @($Runtime.$name).Count -ne 4) { throw "$Role runtime/$name shape rejected." } }
    $previous=0.0
    for($i=0;$i -lt $ExpectedCascades;$i++) { Assert-Number $Runtime.cascadeSplits[$i] "$Role split"; if([double]$Runtime.cascadeSplits[$i] -le $previous -or [int]$Runtime.cascadeRendered[$i] -lt 1 -or ($i -gt 0 -and [int]$Runtime.cascadeReused[$i] -lt 1)) { throw "$Role active cascade rejected." }; $previous=[double]$Runtime.cascadeSplits[$i] }
    if($ExpectedCascades -eq 2) { for($i=2;$i -lt 4;$i++) { if([double]$Runtime.cascadeSplits[$i] -ne [double]$Runtime.cascadeSplits[1] -or [int]$Runtime.cascadeRendered[$i] -ne 0 -or [int]$Runtime.cascadeReused[$i] -ne 0) { throw 'Fallback inactive cascade rejected.' } } }
}
function Assert-CaptureReceipt([object]$Receipt,[object]$Runtime,[string]$Role,[string]$Mode,[int]$Cascades,[string]$Root,[object]$Declared,[object]$Policy) {
    Assert-ExactProperties $Receipt @('schemaVersion','runId','mode','cascadeCount','source','executable','command','process','dimensions','captures','semantics','runtime','performance','gpuValidation') "$Role capture"
    if($Receipt.schemaVersion -ne 1 -or $Receipt.runId -cne $Runtime.runId -or $Receipt.mode -cne $Mode -or [int]$Receipt.cascadeCount -ne $Cascades) { throw "$Role capture identity rejected." }
    if((Get-CanonicalJsonText $Receipt.runtime) -cne (Get-CanonicalJsonText $Runtime)) { throw "$Role nested runtime mismatch." }
    Assert-ExactProperties $Receipt.executable @('path','sha256Before','sha256After') "$Role executable"
    if($Receipt.executable.path -cne 'Aevrynd-Engine-Lab.exe' -or $Receipt.executable.sha256Before -cne $Policy.entrySha256 -or $Receipt.executable.sha256After -cne $Policy.entrySha256 -or (Get-Sha256 (Join-Path $Root $Receipt.executable.path)) -cne $Policy.entrySha256) { throw "$Role executable binding rejected." }
    Assert-ExactProperties $Receipt.command @('file','arguments','workingDirectory','captureDirectory','gpuValidation','configuredCascades') "$Role command"
    if($Receipt.command.file -cne 'Aevrynd-Engine-Lab.exe' -or $Receipt.command.workingDirectory -cne '.' -or $Receipt.command.captureDirectory -cne "captures/$Role" -or $Receipt.command.arguments -isnot [array] -or @($Receipt.command.arguments).Count -ne 1 -or $Receipt.command.arguments[0] -cne '--capture-shadow-demo' -or [int]$Receipt.command.configuredCascades -ne $Cascades -or ($Role -eq 'gbv' -and $Receipt.command.gpuValidation -cne '1') -or ($Role -ne 'gbv' -and $Receipt.command.gpuValidation -cne '')) { throw "$Role command binding rejected." }
    Assert-ExactProperties $Receipt.source @('root','workingTreeDigestBefore','workingTreeDigestAfter','trackedWipFiles','engineLabSource','gameManifest','runner') "$Role source"
    if($Receipt.source.root -cne '.' -or $Receipt.source.workingTreeDigestBefore -cne ('e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855') -or $Receipt.source.workingTreeDigestAfter -cne $Receipt.source.workingTreeDigestBefore -or $Receipt.source.trackedWipFiles -isnot [array] -or @($Receipt.source.trackedWipFiles).Count -ne 0) { throw "$Role clean source binding rejected." }
    foreach($name in @('engineLabSource','gameManifest','runner')) { Assert-ExactProperties $Receipt.source.$name @('path','sha256Before','sha256After') "$Role source/$name"; if([string]$Receipt.source.$name.sha256Before -cnotmatch '^[0-9a-f]{64}$' -or $Receipt.source.$name.sha256After -cne $Receipt.source.$name.sha256Before) { throw "$Role source/$name hash binding rejected." } }
    $sourceContract=[ordered]@{engineLabSource=@('evidence/source/EngineLabGame.cpp',$Policy.engineLabSourceSha256);gameManifest=@('evidence/source/game.json',$Policy.gameManifestSha256);runner=@('evidence/EngineLabShadowCaptureSmoke.ps1',$Policy.captureRunnerSha256)}
    foreach($name in $sourceContract.Keys) { $expectedPath=[string]$sourceContract[$name][0];$expectedHash=[string]$sourceContract[$name][1];if($Receipt.source.$name.path -cne $expectedPath -or $Receipt.source.$name.sha256Before -cne $expectedHash -or (Get-Sha256 (Join-Path $Root $expectedPath)) -cne $expectedHash) { throw "$Role source/$name canonical binding rejected." } }
    Assert-ExactProperties $Receipt.dimensions @('width','height') "$Role dimensions"; if([int]$Receipt.dimensions.width -ne 1280 -or [int]$Receipt.dimensions.height -ne 720) { throw "$Role dimensions rejected." }
    Assert-ExactProperties $Receipt.process @('exitCode') "$Role process"; if([int]$Receipt.process.exitCode -ne 0) { throw "$Role process rejected." }
    Assert-ExactProperties $Receipt.performance @('receipt','sha256','fourCascadeRegression','maximumRegression','passed') "$Role performance"
    Assert-Number $Receipt.performance.fourCascadeRegression "$Role regression"; Assert-Number $Receipt.performance.maximumRegression "$Role maximum regression"
    if($Receipt.performance.receipt -cne 'evidence/four-cascade-performance-receipt.json' -or $Receipt.performance.sha256 -cne $Policy.performanceSha256 -or (Get-Sha256 (Join-Path $Root $Receipt.performance.receipt)) -cne $Policy.performanceSha256 -or $Receipt.performance.passed -isnot [bool] -or -not $Receipt.performance.passed -or [double]$Receipt.performance.fourCascadeRegression -gt .05 -or [double]$Receipt.performance.maximumRegression -ne .05) { throw "$Role performance rejected." }
    Assert-ExactProperties $Receipt.semantics @('contact','farCascade') "$Role semantics"; Assert-ExactProperties $Receipt.semantics.contact @('changed','darkened','brightened') "$Role contact semantics"; Assert-ExactProperties $Receipt.semantics.farCascade @('mask','instances') "$Role far semantics"
    if([int64]$Receipt.semantics.contact.darkened -lt 1 -or [int]$Receipt.semantics.farCascade.mask -ne [int]$Runtime.farCascadeMask -or [int]$Receipt.semantics.farCascade.instances -ne [int]$Runtime.farCascadeInstances) { throw "$Role semantic counters rejected." }
    Assert-ExactProperties $Receipt.gpuValidation @('requested','available','executed','debug','passed','errors','warnings') "$Role GPU receipt"
    $gpuReceipt=@([int]$Receipt.gpuValidation.requested,[int]$Receipt.gpuValidation.available,[int]$Receipt.gpuValidation.executed,[int]$Receipt.gpuValidation.debug,[int]$Receipt.gpuValidation.passed,[int]$Receipt.gpuValidation.errors,[int]$Receipt.gpuValidation.warnings)
    $gpuRuntime=@([int]$Runtime.gpuValidationRequested,[int]$Runtime.gpuValidationAvailable,[int]$Runtime.gpuValidationExecuted,[int]$Runtime.debugLayerEnabled,[int]$Runtime.gpuValidationPassed,[int]$Runtime.gpuValidationErrors,[int]$Runtime.gpuValidationWarnings)
    if(@(Compare-Object $gpuRuntime $gpuReceipt -SyncWindow 0).Count) { throw "$Role GPU wrapper mismatch." }
    $expectedNames=@('contact-shadow-disabled.png','contact-shadow-enabled.png','far-cascade-receiver.png','day-shadow.png','dusk-shadow.png','night-shadow.png')
    if($Receipt.captures -isnot [array] -or @($Receipt.captures).Count -ne 6) { throw "$Role capture count rejected." }
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($capture in $Receipt.captures) { Assert-ExactProperties $capture @('file','width','height','sha256') "$Role PNG"; if(-not $seen.Add([string]$capture.file) -or [string]$capture.file -cnotin $expectedNames -or [int]$capture.width -ne 1280 -or [int]$capture.height -ne 720 -or [string]$capture.sha256 -cnotmatch '^[0-9a-f]{64}$') { throw "$Role PNG record rejected." }; $relative="captures/$Role/$($capture.file)"; $record=$Declared[$relative]; if($null -eq $record -or [string]$record.sha256 -cne [string]$capture.sha256 -or (Get-Sha256 (Join-Path $Root $relative)) -cne [string]$capture.sha256) { throw "$Role PNG binding rejected: $relative" } }
    if($seen.Count -ne 6) { throw "$Role PNG exact set rejected." }
}
function Get-CandidateIdentity([string]$Root) {
    $policy=Get-ApprovedCandidatePolicy; $rootFull=Get-FullPath $Root; Assert-NoReparseParents $rootFull
    foreach($item in Get-ChildItem -LiteralPath $rootFull -Force -Recurse) { if(($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Candidate reparse rejected: $($item.FullName)" }; if(-not $TestMode -and -not $item.PSIsContainer -and -not $item.IsReadOnly) { throw "Candidate freeze rejected: $($item.FullName)" } }
    $manifestPath=Join-Path $rootFull 'candidate.manifest.json'; $manifest=Read-StrictJson $manifestPath 'Candidate manifest'
    Assert-ExactProperties $manifest @('schemaVersion','releaseId','product','productVersion','engineVersion','commit','entry','featureStage','maturity','captureModes','fullTests','peClosure','source','files') 'Candidate manifest'
    if($manifest.schemaVersion -ne 1 -or $manifest.product -cne 'EngineLab' -or $manifest.productVersion -cne '2.55.0' -or $manifest.engineVersion -cne '2.86.0' -or $manifest.releaseId -cne 'v2.55.0-engine-v2.86.0-c033de38' -or $manifest.commit -cne $policy.sourceCommit -or $manifest.entry -cne 'Aevrynd-Engine-Lab.exe' -or $manifest.featureStage -cne 'FourCascadeContactShadowValidated' -or $manifest.maturity -cne 'validated-candidate' -or $manifest.captureModes -isnot [array] -or @($manifest.captureModes).Count -ne 3 -or [int64]$manifest.fullTests -ne 113 -or $manifest.peClosure -cne 'evidence/pe-closure.json' -or $manifest.files -isnot [array]) { throw 'Candidate identity rejected.' }
    if(@(Compare-Object @('normal-four-cascade','gbv-four-cascade','normal-two-cascade-fallback') @($manifest.captureModes) -SyncWindow 0).Count) { throw 'Candidate capture modes rejected.' }
    Assert-ExactProperties $manifest.source @('commit','origin','builder','baseBuilder','captureRunner','hostBinding') 'Candidate source'
    foreach($name in @('builder','baseBuilder','captureRunner','hostBinding')) { Assert-ExactProperties $manifest.source.$name @('path','sha256') "Candidate source/$name" }
    if($manifest.source.commit -cne $policy.sourceCommit -or $manifest.source.origin -cne $policy.sourceCommit -or $manifest.source.builder.sha256 -cne $policy.builderScriptSha256 -or $manifest.source.captureRunner.sha256 -cne $policy.captureRunnerSha256) { throw 'Candidate source binding rejected.' }
    if(-not $TestMode -and $manifest.source.baseBuilder.sha256 -cne $script:ExpectedBaseBuilderScriptSha256) { throw 'Candidate base builder binding rejected.' }
    $declared=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($record in $manifest.files) { Assert-ExactProperties $record @('path','size','sha256') 'Candidate file'; $relative=[string]$record.path; if(-not(Test-RelativePath $relative) -or ($record.size -isnot [int] -and $record.size -isnot [long]) -or [string]$record.sha256 -cnotmatch '^[0-9a-f]{64}$' -or $declared.ContainsKey($relative)) { throw "Candidate file record rejected: $relative" }; $declared.Add($relative,$record) }
    $actual=@(Get-ChildItem -LiteralPath $rootFull -File -Recurse | Where-Object FullName -ne $manifestPath)
    if($actual.Count -ne $declared.Count) { throw "Candidate exact file count rejected: actual=$($actual.Count) declared=$($declared.Count)" }
    foreach($file in $actual) { $relative=$file.FullName.Substring($rootFull.Length+1).Replace('\','/'); if(-not $declared.ContainsKey($relative) -or [string]$declared[$relative].path -cne $relative) { throw "Candidate extra/case mismatch: $relative" }; if($file.Length -ne [int64]$declared[$relative].size -or (Get-Sha256 $file.FullName) -cne [string]$declared[$relative].sha256) { throw "Candidate content mismatch: $relative" } }
    $manifestHash=Get-Sha256 $manifestPath; $entryRecord=@($manifest.files | Where-Object path -CEQ 'Aevrynd-Engine-Lab.exe')
    if($manifestHash -cne $policy.manifestSha256 -or $manifest.files.Count -ne $policy.files -or $entryRecord.Count -ne 1 -or [string]$entryRecord[0].sha256 -cne $policy.entrySha256) { throw 'Candidate is not the uniquely approved shadow artifact.' }
    if(-not $TestMode) {
        if((Get-Sha256 (Join-Path $rootFull 'evidence/pe-closure.json')) -cne $script:ExpectedPeClosureSha256 -or (Get-Sha256 (Join-Path $rootFull 'evidence/four-cascade-performance-receipt.json')) -cne $script:ExpectedPerformanceSha256) { throw 'PE closure or performance evidence binding rejected.' }
        $refinalizationPath=Join-Path $rootFull 'evidence/candidate-refinalization-receipt.json'
        if((Get-Sha256 $refinalizationPath) -cne $script:ExpectedRefinalizationSha256 -or (Get-Sha256 (Join-Path $rootFull 'evidence/Finalize-EngineLabShadowV255Candidate.ps1')) -cne $script:ExpectedFinalizerSha256) { throw 'Candidate refinalization evidence hash rejected.' }
        $refinalization=Read-StrictJson $refinalizationPath 'Candidate refinalization receipt'
        Assert-ExactProperties $refinalization @('schemaVersion','operation','priorCandidateManifestSha256','sourceCommit','finalizer','sourceArchive','entry','performance','roles') 'Candidate refinalization receipt'
        if($refinalization.schemaVersion -ne 1 -or $refinalization.operation -cne 'candidate-relative-shadow-provenance-v1' -or $refinalization.priorCandidateManifestSha256 -cne '2979ce4ac66e36aa1de6e58a56f624d37f15fd9dd9a0ff2f3614812777cf40e6' -or $refinalization.sourceCommit -cne $policy.sourceCommit -or $refinalization.sourceArchive -isnot [array] -or @($refinalization.sourceArchive).Count -ne 3 -or $refinalization.roles -isnot [array] -or @($refinalization.roles).Count -ne 3) { throw 'Candidate refinalization identity rejected.' }
        foreach($name in @('finalizer','entry','performance')) { Assert-ExactProperties $refinalization.$name @('path','sha256') "Candidate refinalization/$name" }
        if($refinalization.finalizer.path -cne 'evidence/Finalize-EngineLabShadowV255Candidate.ps1' -or $refinalization.finalizer.sha256 -cne $script:ExpectedFinalizerSha256 -or $refinalization.entry.path -cne 'Aevrynd-Engine-Lab.exe' -or $refinalization.entry.sha256 -cne $policy.entrySha256 -or $refinalization.performance.path -cne 'evidence/four-cascade-performance-receipt.json' -or $refinalization.performance.sha256 -cne $policy.performanceSha256) { throw 'Candidate refinalization primary binding rejected.' }
        $expectedSources=[ordered]@{'evidence/source/EngineLabGame.cpp'=$policy.engineLabSourceSha256;'evidence/source/game.json'=$policy.gameManifestSha256;'evidence/EngineLabShadowCaptureSmoke.ps1'=$policy.captureRunnerSha256}
        foreach($record in $refinalization.sourceArchive) { Assert-ExactProperties $record @('path','sha256') 'Candidate refinalization source';if(-not $expectedSources.Contains([string]$record.path) -or $expectedSources[[string]$record.path] -cne [string]$record.sha256) { throw 'Candidate refinalization source binding rejected.' };[void]$expectedSources.Remove([string]$record.path) }
        if($expectedSources.Count) { throw 'Candidate refinalization source set rejected.' }
        $expectedRoles=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($expectedRole in @('normal','gbv','fallback')){[void]$expectedRoles.Add($expectedRole)}
        foreach($record in $refinalization.roles) { Assert-ExactProperties $record @('role','priorCaptureReceiptSha256','finalizedCaptureReceipt','finalizedCaptureReceiptSha256') 'Candidate refinalization role';$role=[string]$record.role;if(-not $expectedRoles.Remove($role) -or $record.finalizedCaptureReceipt -cne "captures/$role/shadow-demo-capture-receipt.json" -or $record.finalizedCaptureReceiptSha256 -cne $script:ExpectedReceipts[($role+'Capture')] -or [string]$record.priorCaptureReceiptSha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Candidate refinalization role binding rejected.' } }
        if($expectedRoles.Count) { throw 'Candidate refinalization role set rejected.' }
    }
    $roles=[ordered]@{}; $runIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($spec in @(@('normal','normal',4,8,67108864,@(0,0,0,0,0,0,0)),@('gbv','gbv',4,8,67108864,@(1,1,1,1,1,0,0)),@('fallback','normal',2,2,33554432,@(0,0,0,0,0,0,0)))) {
        $role=[string]$spec[0]; $runtimePath=Join-Path $rootFull "captures/$role/shadow-demo-runtime-receipt.json"; $capturePath=Join-Path $rootFull "captures/$role/shadow-demo-capture-receipt.json"
        $runtime=Read-StrictJson $runtimePath "$role runtime"; $capture=Read-StrictJson $capturePath "$role capture"
        Assert-ShadowRuntime $runtime $role ([int]$spec[2]) ([int]$spec[3]) ([int64]$spec[4]) ([int[]]$spec[5]); Assert-CaptureReceipt $capture $runtime $role ([string]$spec[1]) ([int]$spec[2]) $rootFull $declared $policy
        if(-not $runIds.Add([string]$runtime.runId)) { throw 'Shadow run IDs must be unique.' }
        if(-not $TestMode) { if((Get-Sha256 $runtimePath) -cne $script:ExpectedReceipts[($role+'Runtime')] -or (Get-Sha256 $capturePath) -cne $script:ExpectedReceipts[($role+'Capture')] -or $runtime.runId -cne $script:ExpectedRunIds[$role]) { throw "$role approved receipt binding rejected." } }
        $roles[$role]=[ordered]@{role=$role;runId=[string]$runtime.runId;runtimeSha256=Get-Sha256 $runtimePath;captureSha256=Get-Sha256 $capturePath;cascades=[int]$runtime.cascadeCount;contactExecuted=[int]$runtime.contactExecuted;shadowBytes=[int64]$runtime.shadowBytes}
    }
    [ordered]@{root=$rootFull;manifestPath=$manifestPath;manifest=$manifest;manifestSha256=$manifestHash;files=@($manifest.files);policy=$policy;receipts=$roles}
}
function Copy-VerifiedFile([string]$Source, [string]$Destination, [object]$Record) {
    $sourceStream = [IO.File]::Open($Source, 'Open', 'Read', 'Read')
    try {
        $sourceIdentityBefore = Get-FileIdentity $sourceStream
        if ($sourceIdentityBefore.links -ne 1) { throw "Source hardlink rejected: $Source" }
        if (@(Get-Item -LiteralPath $Source -Stream * | Where-Object Stream -ne ':$DATA').Count) { throw "Source ADS rejected: $Source" }
        $destinationStream = [IO.File]::Open($Destination, 'CreateNew', 'ReadWrite', 'None')
        try {
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $buffer = New-Object byte[] 1048576; [int64]$total = 0; while (($read = $sourceStream.Read($buffer,0,$buffer.Length)) -gt 0) { $destinationStream.Write($buffer,0,$read); [void]$sha.TransformBlock($buffer,0,$read,$null,0); $total += $read }; [void]$sha.TransformFinalBlock([byte[]]::new(0),0,0); $hash = ([BitConverter]::ToString($sha.Hash)).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
            $destinationStream.Flush($true); $destinationIdentity = Get-FileIdentity $destinationStream
        } finally { $destinationStream.Dispose() }
        $sourceIdentityAfter = Get-FileIdentity $sourceStream
        if ($sourceIdentityBefore.volume -cne $sourceIdentityAfter.volume -or $sourceIdentityBefore.file -cne $sourceIdentityAfter.file -or $sourceIdentityAfter.links -ne 1 -or $destinationIdentity.links -ne 1 -or ($sourceIdentityBefore.volume -ceq $destinationIdentity.volume -and $sourceIdentityBefore.file -ceq $destinationIdentity.file)) { throw "Copy file identity rejected: $Source" }
        if ($total -ne [int64]$Record.size -or $hash -cne [string]$Record.sha256) { throw "Copy hash/size rejected: $Source" }
        [ordered]@{ path = [string]$Record.path; size = $total; sha256 = $hash; sourceFileId = "$($sourceIdentityBefore.volume):$($sourceIdentityBefore.file)"; releaseFileId = "$($destinationIdentity.volume):$($destinationIdentity.file)" }
    } finally { $sourceStream.Dispose() }
}
function Assert-ReleaseTree([string]$Root, [object[]]$Files, [string]$ManifestHash) {
    Assert-NoReparseParents $Root; $manifestPath = Join-Path $Root 'release.manifest.json'; $all=@(Get-ChildItem -LiteralPath $Root -Force -Recurse);foreach($item in $all){if(($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw "Release reparse rejected: $($item.FullName)"}};$actual = @($all|Where-Object{-not$_.PSIsContainer})
    if ($actual.Count -ne $Files.Count + 1) { throw 'Release exact tree count rejected.' }
    $known = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($record in $Files) { [void]$known.Add([string]$record.path) }
    foreach ($file in $actual) { $relative = $file.FullName.Substring($Root.Length + 1).Replace('\','/'); if ($relative -ceq 'release.manifest.json') { continue }; if (-not $known.Remove($relative)) { throw "Release extra/case mismatch: $relative" }; Assert-SafeFile $file.FullName | Out-Null; $record = @($Files | Where-Object path -CEQ $relative)[0]; if ($file.Length -ne [int64]$record.size -or (Get-Sha256 $file.FullName) -cne [string]$record.sha256) { throw "Release content mismatch: $relative" } }
    if ($known.Count -or (Get-Sha256 $manifestPath) -cne $ManifestHash) { throw 'Release missing file or manifest mismatch.' }
    $expectedDirectories=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($record in $Files){$parts=([string]$record.path).Split('/');for($i=1;$i-lt$parts.Count;$i++){[void]$expectedDirectories.Add(($parts[0..($i-1)]-join'/'))}}
    $actualDirectories=@($all|Where-Object PSIsContainer|ForEach-Object{$_.FullName.Substring($Root.Length+1).Replace('\','/')}|Sort-Object);if($actualDirectories.Count-ne$expectedDirectories.Count){throw 'Release exact directory count rejected.'};foreach($relative in $actualDirectories){if(-not$expectedDirectories.Contains($relative)){throw "Release extra directory rejected: $relative"}}
}
function New-ResolverObject {
    [ordered]@{schemaVersion=1;role='current-and-latest-resolver';product='EngineLab';mode='highest-semver-valid-generation';generationDirectory='current/engine-lab.generations';releaseRoot='releases/engine-lab';entry='Aevrynd-Engine-Lab.exe';legacyLatestExecutableMode='preserved-unmanaged'}
}
function Assert-Resolver([string]$Path) {
    if(-not(Test-Path -LiteralPath $Path -PathType Leaf)){return $false}
    $resolver=Read-StrictJson $Path 'Engine Lab resolver';Assert-ExactProperties $resolver @('schemaVersion','role','product','mode','generationDirectory','releaseRoot','entry','legacyLatestExecutableMode') 'Engine Lab resolver'
    $expected=Get-CanonicalJsonText (New-ResolverObject);if((Get-CanonicalJsonText $resolver)-cne$expected-or[IO.File]::ReadAllText($Path)-cne$expected){throw 'Engine Lab immutable resolver contract rejected.'};return $true
}
function Read-GenerationSet([string]$GenerationRoot,[string]$DemoRoot) {
    $result=@();if(-not(Test-Path -LiteralPath $GenerationRoot)){return @()}
    foreach($item in @(Get-ChildItem -LiteralPath $GenerationRoot -Force|Sort-Object Name)){
        if($item.PSIsContainer-or($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0-or$item.Name-cnotmatch'^v([0-9]+)\.([0-9]+)\.([0-9]+)-([0-9a-f]{64})\.json$'){throw "Generation exact tree rejected: $($item.Name)"}
        $major=[uint64]$Matches[1];$minor=[uint64]$Matches[2];$patch=[uint64]$Matches[3];$nameHash=$Matches[4]
        $generation=Read-StrictJson $item.FullName 'Engine Lab generation';Assert-ExactProperties $generation @('schemaVersion','role','product','productVersion','releaseId','releasePath','entry','manifestSha256','legacyLatestExecutableMode') 'Engine Lab generation'
        if([IO.File]::ReadAllText($item.FullName)-cne(Get-CanonicalJsonText $generation)){throw "Generation canonical bytes rejected: $($item.Name)"}
        $version="$major.$minor.$patch"
        if($generation.schemaVersion-ne1-or$generation.role-cne'release-generation'-or$generation.product-cne'EngineLab'-or$generation.productVersion-cne$version-or[string]$generation.releaseId-cnotmatch('^v'+[regex]::Escape($version)+'-engine-v[0-9]+\.[0-9]+\.[0-9]+-[0-9a-f]{8}$')-or$generation.releasePath-cne('releases/engine-lab/'+$generation.releaseId)-or$generation.entry-cne'Aevrynd-Engine-Lab.exe'-or$generation.manifestSha256-cne$nameHash-or$generation.legacyLatestExecutableMode-cne'preserved-unmanaged'){throw "Generation contract rejected: $($item.Name)"}
        $release=Join-Path $DemoRoot ([string]$generation.releasePath);$manifestPath=Join-Path $release 'release.manifest.json';if(-not(Test-Path -LiteralPath $manifestPath -PathType Leaf)-or(Get-Sha256 $manifestPath)-cne$nameHash){throw "Generation target rejected: $($item.Name)"}
        $manifest=Read-StrictJson $manifestPath 'Generation release manifest';Assert-ExactProperties $manifest @('schemaVersion','product','productVersion','engineVersion','releaseId','entry','treeAlgorithm','releaseFileCount','pointerContract','legacyLatestExecutableMode','source','builder','verifier','validation','peClosure','files') 'Generation release manifest'
        if($manifest.schemaVersion-ne1-or$manifest.product-cne'EngineLab'-or$manifest.productVersion-cne$version-or$manifest.releaseId-cne$generation.releaseId-or$manifest.entry-cne$generation.entry-or$manifest.pointerContract-cne'immutable-resolver-append-only-semver-generations-v1'-or$manifest.legacyLatestExecutableMode-cne'preserved-unmanaged'-or$manifest.files-isnot[array]-or[int64]$manifest.releaseFileCount-ne($manifest.files.Count+1)){throw 'Generation release manifest inventory rejected.'};Assert-ReleaseTree $release @($manifest.files) $nameHash
        $result+=,[pscustomobject]@{path=$item.FullName;major=$major;minor=$minor;patch=$patch;manifestSha256=$nameHash;generation=$generation}
    }
    # 所有歷代版本都必須只有一個 generation，不能只檢查目前最高版本。
    foreach($generationRecord in $result){
        if(@($result|Where-Object{$_.major-eq$generationRecord.major-and$_.minor-eq$generationRecord.minor-and$_.patch-eq$generationRecord.patch}).Count-ne1){throw 'Ambiguous same-semver generations rejected.'}
    }
    return @($result)
}
function Publish-CreateOnlyFile([string]$Pending,[string]$Destination,[string]$Label){
    $pendingHash=Get-Sha256 $Pending
    if(Test-Path -LiteralPath $Destination){if((Get-Sha256 $Destination)-cne$pendingHash){throw "$Label immutable collision rejected."};Remove-Item -LiteralPath $Pending -Force;return}
    if(-not [Aevrynd.EngineLabPublisherNative]::MoveFileEx($Pending,$Destination,[uint32]0x8)){
        $error=[Runtime.InteropServices.Marshal]::GetLastWin32Error();if(-not(Test-Path -LiteralPath $Destination)-or(Get-Sha256 $Destination)-cne$pendingHash){throw "$Label create-only commit failed: $error"};Remove-Item -LiteralPath $Pending -Force
    }
}
function Open-PublisherLock([string]$Path){
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.ElapsedMilliseconds-lt30000){
        try { return [IO.File]::Open($Path,'OpenOrCreate','ReadWrite','None') }
        catch [IO.IOException] { Start-Sleep -Milliseconds 50 }
    }
    throw 'Publisher lock acquisition timed out.'
}
function Invoke-Failure([string]$Point) { if ($TestFailPoint -ceq $Point) { throw "Injected publisher failure: $Point" } }

$project=Get-FullPath $ProjectRoot;$candidateRootFull=Get-FullPath $CandidateRoot;if(-not$DemoRoot){$DemoRoot=Join-Path $project 'Demo'};$demo=Get-FullPath $DemoRoot
foreach($root in @($project,$candidateRootFull)){if(-not(Test-Path -LiteralPath $root -PathType Container)){throw "Required root missing: $root"};Assert-NoReparseParents $root}
if($TestFailPoint-and-not$TestMode){throw 'Failure injection requires TestMode.'}
if($TestMode){$marker=Join-Path $demo '.aevrynd-engine-lab-publisher-test-root';if(-not(Test-Path -LiteralPath $marker -PathType Leaf)-or[IO.File]::ReadAllText($marker)-cne"schema=1`npurpose=engine-lab-publisher-test`n"){throw 'TestMode Demo marker rejected.'}}
if([IO.Path]::GetPathRoot($candidateRootFull)-cne[IO.Path]::GetPathRoot($demo)){throw 'Publisher roots must share one volume.'}
$candidate=Get-CandidateIdentity $candidateRootFull;$candidateManifestBefore=$candidate.manifestSha256
if($TestFailPoint-ceq'Swap'){[IO.File]::AppendAllText((Join-Path $candidate.root 'assets/engine_probe.obj'),'swap');[void](Get-CandidateIdentity $candidate.root)}
$evidenceNames=@('evidence/candidate-build-receipt.json','evidence/build-process-policy.json','evidence/ctest-full.command.json','evidence/ctest-full.out.log','evidence/pe-closure.json','evidence/four-cascade-performance-receipt.json','evidence/candidate-refinalization-receipt.json','evidence/Finalize-EngineLabShadowV255Candidate.ps1','captures/normal/shadow-demo-runtime-receipt.json','captures/normal/shadow-demo-capture-receipt.json','captures/gbv/shadow-demo-runtime-receipt.json','captures/gbv/shadow-demo-capture-receipt.json','captures/fallback/shadow-demo-runtime-receipt.json','captures/fallback/shadow-demo-capture-receipt.json')
$evidence=@();$evidenceByPath=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal);foreach($relative in $evidenceNames){$record=@($candidate.files|Where-Object path -CEQ $relative)[0];if($null-eq$record){throw "Required evidence missing: $relative"};$value=[ordered]@{path=$relative;size=[int64]$record.size;sha256=[string]$record.sha256};$evidence+=,$value;$evidenceByPath.Add($relative,$value)}
$peFiles=@($candidate.files|Where-Object{([string]$_.path).EndsWith('.exe',[StringComparison]::OrdinalIgnoreCase)-or([string]$_.path).EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase)}|ForEach-Object{[string]$_.path}|Sort-Object);$dllFiles=@($peFiles|Where-Object{$_.EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase)});if($peFiles.Count-ne1-or$dllFiles.Count){throw 'Candidate PE exact-set rejected.'}
$releaseId=[string]$candidate.manifest.releaseId;$publisherSha=Get-Sha256 $PSCommandPath
$plan=[ordered]@{schemaVersion=1;dryRun=[bool]$DryRun;releaseId=$releaseId;candidateFiles=$candidate.files.Count;releaseFiles=($candidate.files.Count+1);peFiles=$peFiles.Count;bundledDlls=$dllFiles.Count;candidateManifestSha256=$candidate.manifestSha256;sourceCommit=$candidate.manifest.commit;publisherSha256=$publisherSha;roles=@($candidate.receipts.Keys)}
if($DryRun){$plan|ConvertTo-Json -Depth 8;exit 0}
New-Item -ItemType Directory -Path $demo -Force|Out-Null;Assert-NoReparseParents $demo
$releaseBase=Join-Path $demo 'releases\engine-lab';$stagingBase=Join-Path $releaseBase '.staging';$currentRoot=Join-Path $demo 'current';$generationRoot=Join-Path $currentRoot 'engine-lab.generations';New-Item -ItemType Directory -Path $releaseBase,$stagingBase,$currentRoot,$generationRoot -Force|Out-Null
foreach($directory in @($demo,$releaseBase,$stagingBase,$currentRoot,$generationRoot)){if(((Get-Item -LiteralPath $directory -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw "Publisher directory reparse rejected: $directory"}}
$resolver=Join-Path $currentRoot 'engine-lab.current.json';$lockPath=Join-Path $demo '.engine-lab-publish.lock';$lock=Open-PublisherLock $lockPath;$staging=Join-Path $stagingBase ($releaseId+'-'+[Guid]::NewGuid().ToString('N'));$release=Join-Path $releaseBase $releaseId;$resolverPending=Join-Path $currentRoot ('engine-lab.resolver.'+[Guid]::NewGuid().ToString('N')+'.pending');$generationPending=Join-Path $generationRoot ('generation.'+[Guid]::NewGuid().ToString('N')+'.pending');$generation=''
try{
    if(@(Get-ChildItem -LiteralPath $currentRoot -Filter 'engine-lab.*.pending' -Force -ErrorAction SilentlyContinue).Count-or@(Get-ChildItem -LiteralPath $generationRoot -Filter '*.pending' -Force -ErrorAction SilentlyContinue).Count){throw 'Unowned engine-lab pointer pending residue rejected.'}
    [void](Assert-Resolver $resolver);$generationSet=@(Read-GenerationSet $generationRoot $demo);$historyBefore=@(Get-ChildItem -LiteralPath $generationRoot -File -ErrorAction SilentlyContinue|ForEach-Object{[ordered]@{name=$_.Name;sha256=Get-Sha256 $_.FullName}})
    New-Item -ItemType Directory -Path $staging|Out-Null;$copied=@();$index=0;foreach($record in @($candidate.files|Sort-Object path)){$relative=[string]$record.path;$source=Join-Path $candidate.root $relative;Assert-SafeFile $source|Out-Null;$destination=Join-Path $staging $relative;New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force|Out-Null;$copied+=,(Copy-VerifiedFile $source $destination $record);if($index++-eq0){Invoke-Failure 'Copy'}}
    Invoke-Failure 'Hash';if((Get-Sha256 $candidate.manifestPath)-cne$candidateManifestBefore){throw 'Candidate manifest changed after copy.'}
    $receiptRoles=@($candidate.receipts.Keys|ForEach-Object{$candidate.receipts[$_]})
    $manifestObject=[ordered]@{schemaVersion=1;product='EngineLab';productVersion='2.55.0';engineVersion='2.86.0';releaseId=$releaseId;entry=[string]$candidate.manifest.entry;treeAlgorithm='candidate-files-plus-self-excluded-release-manifest-v1';releaseFileCount=($candidate.files.Count+1);pointerContract='immutable-resolver-append-only-semver-generations-v1';legacyLatestExecutableMode='preserved-unmanaged';source=[ordered]@{commit=[string]$candidate.manifest.commit;candidateManifestSha256=$candidate.manifestSha256;candidateFileCount=$candidate.files.Count};builder=[ordered]@{scriptSha256=[string]$candidate.manifest.source.builder.sha256;baseBuilderScriptSha256=[string]$candidate.manifest.source.baseBuilder.sha256;captureRunnerSha256=[string]$candidate.manifest.source.captureRunner.sha256;evidence=@($evidenceByPath['evidence/candidate-build-receipt.json'],$evidenceByPath['evidence/build-process-policy.json'],$evidenceByPath['evidence/ctest-full.command.json'],$evidenceByPath['evidence/ctest-full.out.log'])};verifier=[ordered]@{policy='shadow-v255-exact-pins-v1';publisherSha256=$publisherSha;candidateManifestSha256=$candidate.manifestSha256;refinalization=@($evidenceByPath['evidence/candidate-refinalization-receipt.json'],$evidenceByPath['evidence/Finalize-EngineLabShadowV255Candidate.ps1']);receiptRoles=$receiptRoles};validation=[ordered]@{performanceEvidence=$evidenceByPath['evidence/four-cascade-performance-receipt.json'];normalReceipts=@($evidenceByPath['captures/normal/shadow-demo-runtime-receipt.json'],$evidenceByPath['captures/normal/shadow-demo-capture-receipt.json']);gbvReceipts=@($evidenceByPath['captures/gbv/shadow-demo-runtime-receipt.json'],$evidenceByPath['captures/gbv/shadow-demo-capture-receipt.json']);fallbackReceipts=@($evidenceByPath['captures/fallback/shadow-demo-runtime-receipt.json'],$evidenceByPath['captures/fallback/shadow-demo-capture-receipt.json']);ctestPassed=113;captures=18};peClosure=[ordered]@{recursive=$true;peFiles=$peFiles;bundledDlls=$dllFiles;evidence=$evidenceByPath['evidence/pe-closure.json']};files=@($copied|ForEach-Object{[ordered]@{path=$_.path;size=$_.size;sha256=$_.sha256}})}
    $manifestPath=Join-Path $staging 'release.manifest.json';Write-CanonicalJson $manifestPath $manifestObject;$manifestHash=Get-Sha256 $manifestPath;Assert-ReleaseTree $staging $manifestObject.files $manifestHash
    $sameSemver=@($generationSet|Where-Object{$_.major-eq2-and$_.minor-eq55-and$_.patch-eq0})
    if($sameSemver.Count -gt 0 -and ($sameSemver.Count -ne 1 -or $sameSemver[0].manifestSha256 -cne $manifestHash -or $sameSemver[0].generation.releaseId -cne $releaseId)){throw 'Same-semver different release rejected before immutable commit.'}
    Invoke-Failure 'Verify';Invoke-Failure 'BeforeRename';Invoke-Failure 'Rename'
    if(Test-Path -LiteralPath $release){Assert-ReleaseTree $release $manifestObject.files $manifestHash;Remove-Item -LiteralPath $staging -Recurse -Force}else{[IO.Directory]::Move($staging,$release)};Assert-ReleaseTree $release $manifestObject.files $manifestHash
    $generation=Join-Path $generationRoot ("v2.55.0-$manifestHash.json");$generationObject=[ordered]@{schemaVersion=1;role='release-generation';product='EngineLab';productVersion='2.55.0';releaseId=$releaseId;releasePath=('releases/engine-lab/'+$releaseId);entry=([string]$candidate.manifest.entry);manifestSha256=$manifestHash;legacyLatestExecutableMode='preserved-unmanaged'}
    Write-CanonicalJson $generationPending $generationObject;Write-CanonicalJson $resolverPending (New-ResolverObject);Invoke-Failure 'BeforePointer';Invoke-Failure 'Pointer';Publish-CreateOnlyFile $generationPending $generation 'Generation pointer';Publish-CreateOnlyFile $resolverPending $resolver 'Current/latest resolver'
    if(-not(Assert-Resolver $resolver)){throw 'Resolver commit missing.'};[void](Read-GenerationSet $generationRoot $demo);foreach($record in $historyBefore){$path=Join-Path $generationRoot $record.name;if(-not(Test-Path -LiteralPath $path)-or(Get-Sha256 $path)-cne$record.sha256){throw 'Existing generation history changed.'}}
    [ordered]@{schemaVersion=1;published=$true;releaseId=$releaseId;releaseRoot=$release;manifestSha256=$manifestHash;files=$manifestObject.files.Count;peFiles=$peFiles.Count;bundledDlls=$dllFiles.Count;generation=$generation}|ConvertTo-Json -Depth 6
}finally{if(Test-Path -LiteralPath $staging){Remove-Item -LiteralPath $staging -Recurse -Force};foreach($pending in @($resolverPending,$generationPending)){if(Test-Path -LiteralPath $pending){Remove-Item -LiteralPath $pending -Force}};$lock.Dispose()}
