param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string]$CandidateRoot,
    [Parameter(Mandatory)][string]$VerifierBundleRoot,
    [Parameter(Mandatory)][string]$VerificationSandboxRoot,
    [string]$DemoRoot = '',
    [switch]$DryRun,
    [switch]$TestMode,
    [string]$TestExpectedCandidateManifestSha256 = '',
    [int]$TestExpectedCandidateFiles = 0,
    [string]$TestExpectedSourceCommit = '',
    [string]$TestExpectedEntrySha256 = '',
    [string]$TestExpectedBuilderCommit = '',
    [string]$TestExpectedBuilderScriptSha256 = '',
    [ValidateSet('', 'Swap', 'Copy', 'Hash', 'Verify', 'BeforeRename', 'Rename', 'BeforePointer', 'Pointer')]
    [string]$TestFailPoint = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:ExpectedProductionBundleSha256 = '472134687a6d53fd1724ac84494cccd19de3842bfa21506110966ab96cc053dc'
$script:ExpectedCandidateManifestSha256 = 'ddafc5583488a7d29295912525ca25a26e499d0d10deff47993aa14a6581e99e'
$script:ExpectedCandidateFiles = 172
$script:ExpectedSourceCommit = 'b141122b3a987c16cb7e842efc1bfe32ab5a2d4d'
$script:ExpectedEntrySha256 = '0f83cfef642f535867c4e62dc5e4695b041362dc312f55bfc88d0c89a9d94dbe'
$script:ExpectedBuilderCommit = '1ce01425323b47fe39f18a77efb63a13bd8851c9'
$script:ExpectedBuilderScriptSha256 = '017923050ced6c87ddf099364f5e8d62a1ab6bbf59632773f2161e9d91f85971'
$script:ExpectedBundleFiles = @(
    'Test-EngineLabNetworkReleaseCandidate.ps1',
    'test-engine-lab-network-release-candidate-verifier-hostile.ps1',
    'test-engine-lab-network-release-candidate-verifier.ps1'
)

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
        return [ordered]@{ manifestSha256=$script:ExpectedCandidateManifestSha256; files=$script:ExpectedCandidateFiles; sourceCommit=$script:ExpectedSourceCommit; entrySha256=$script:ExpectedEntrySha256; builderCommit=$script:ExpectedBuilderCommit; builderScriptSha256=$script:ExpectedBuilderScriptSha256 }
    }
    $hashes=@($TestExpectedCandidateManifestSha256,$TestExpectedEntrySha256,$TestExpectedBuilderScriptSha256)
    if($TestExpectedCandidateFiles-lt1-or@($hashes|Where-Object{$_ -cnotmatch '^[0-9a-f]{64}$'}).Count-or$TestExpectedSourceCommit-cnotmatch'^[0-9a-f]{40}$'-or$TestExpectedBuilderCommit-cnotmatch'^[0-9a-f]{40}$'){throw 'Test approved candidate policy rejected.'}
    [ordered]@{manifestSha256=$TestExpectedCandidateManifestSha256;files=$TestExpectedCandidateFiles;sourceCommit=$TestExpectedSourceCommit;entrySha256=$TestExpectedEntrySha256;builderCommit=$TestExpectedBuilderCommit;builderScriptSha256=$TestExpectedBuilderScriptSha256}
}
function Get-BundleIdentity([string]$Root) {
    $rootFull = Get-FullPath $Root; Assert-NoReparseParents $rootFull
    $manifestPath = Join-Path $rootFull 'BUNDLE.MANIFEST.json'
    if ([IO.File]::ReadAllText($manifestPath).Contains("`r")) { throw 'Verifier bundle canonical LF rejected.' }
    $manifest = Read-StrictJson $manifestPath 'Verifier bundle manifest'
    Assert-ExactProperties $manifest @('schemaVersion', 'algorithm', 'files', 'bundleSha256') 'Verifier bundle manifest'
    if ($manifest.schemaVersion -ne 1 -or $manifest.algorithm -cne 'sha256-utf8-ordinal-path-size-hash-lf-v1' -or $manifest.files -isnot [array] -or $manifest.files.Count -ne 3 -or [string]$manifest.bundleSha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Verifier bundle identity rejected.' }
    $actualTop = @(Get-ChildItem -LiteralPath $rootFull -Force)
    if ($actualTop.Count -ne 4 -or @($actualTop | Where-Object PSIsContainer).Count -or @(Compare-Object -CaseSensitive (@('BUNDLE.MANIFEST.json') + $script:ExpectedBundleFiles | Sort-Object) @($actualTop.Name | Sort-Object)).Count) { throw 'Verifier bundle exact tree rejected.' }
    $rows = [Collections.Generic.List[string]]::new(); $files = @()
    for ($i = 0; $i -lt 3; $i++) {
        $record = $manifest.files[$i]; Assert-ExactProperties $record @('path', 'size', 'sha256') 'Verifier bundle file'
        $relative = [string]$record.path
        if ($relative -cne $script:ExpectedBundleFiles[$i] -or ($record.size -isnot [int] -and $record.size -isnot [long]) -or [string]$record.sha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Verifier bundle file record rejected.' }
        $path = Join-Path $rootFull $relative; Assert-SafeFile $path | Out-Null
        $size = [int64](Get-Item -LiteralPath $path).Length; $hash = Get-Sha256 $path
        if ($size -ne [int64]$record.size -or $hash -cne [string]$record.sha256) { throw 'Verifier bundle file mismatch.' }
        $rows.Add("$relative|$size|$hash"); $files += ,([ordered]@{ path = $relative; size = $size; sha256 = $hash })
    }
    $digest = Get-StringSha256 (($rows -join "`n") + "`n")
    if ($digest -cne [string]$manifest.bundleSha256 -or [IO.Path]::GetFileName($rootFull) -cne ('phaseb-' + $digest.ToUpperInvariant())) { throw 'Verifier bundle digest/root binding rejected.' }
    if (-not $TestMode -and $digest -cne $script:ExpectedProductionBundleSha256) { throw 'Verifier bundle is not the approved Phase B bundle.' }
    [ordered]@{ root = $rootFull; sha256 = $digest; manifestSha256 = Get-Sha256 $manifestPath; files = $files }
}
function Get-CandidateIdentity([string]$Root) {
    $policy=Get-ApprovedCandidatePolicy
    $rootFull = Get-FullPath $Root; Assert-NoReparseParents $rootFull
    foreach($item in Get-ChildItem -LiteralPath $rootFull -Force -Recurse){if(($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw "Candidate reparse rejected: $($item.FullName)"}}
    $manifestPath = Join-Path $rootFull 'candidate.manifest.json'; $manifest = Read-StrictJson $manifestPath 'Candidate manifest'
    Assert-ExactProperties $manifest @('schemaVersion','releaseId','product','productVersion','engineVersion','commit','entry','featureStage','maturity','transportScope','derivedCaptures','files') 'Candidate manifest'
    if ($manifest.schemaVersion -ne 1 -or $manifest.product -cne 'EngineLab' -or $manifest.productVersion -cne '2.54.0' -or $manifest.engineVersion -cne '2.86.0' -or [string]$manifest.commit -cnotmatch '^[0-9a-f]{40}$' -or $manifest.entry -cne 'Aevrynd-Engine-Lab.exe' -or [string]$manifest.releaseId -cnotmatch '^v2\.54\.0-engine-v2\.86\.0-[0-9a-f]{8}$' -or $manifest.files -isnot [array] -or $manifest.files.Count -lt 1) { throw 'Candidate identity rejected.' }
    $declared = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($record in $manifest.files) {
        Assert-ExactProperties $record @('path','size','sha256') 'Candidate file'; $relative = [string]$record.path
        if (-not (Test-RelativePath $relative) -or ($record.size -isnot [int] -and $record.size -isnot [long]) -or [string]$record.sha256 -cnotmatch '^[0-9a-f]{64}$' -or $declared.ContainsKey($relative)) { throw "Candidate file record rejected: $relative" }
        $declared.Add($relative, $record)
    }
    $actual = @(Get-ChildItem -LiteralPath $rootFull -File -Recurse | Where-Object FullName -ne $manifestPath)
    if ($actual.Count -ne $declared.Count) { throw "Candidate exact file count rejected: actual=$($actual.Count) declared=$($declared.Count)" }
    foreach ($file in $actual) { $relative = $file.FullName.Substring($rootFull.Length + 1).Replace('\','/'); if (-not $declared.ContainsKey($relative) -or [string]$declared[$relative].path -cne $relative) { throw "Candidate extra/case mismatch: $relative" } }
    $manifestHash=Get-Sha256 $manifestPath;$entryRecord=@($manifest.files|Where-Object path -CEQ 'Aevrynd-Engine-Lab.exe')
    if($manifestHash-cne$policy.manifestSha256-or$manifest.files.Count-ne$policy.files-or$manifest.commit-cne$policy.sourceCommit-or$entryRecord.Count-ne1-or[string]$entryRecord[0].sha256-cne$policy.entrySha256){throw 'Candidate is not the uniquely approved Phase A artifact.'}
    [ordered]@{ root = $rootFull; manifestPath = $manifestPath; manifest = $manifest; manifestSha256 = $manifestHash; files = @($manifest.files); policy=$policy }
}
function Invoke-PhaseBVerifier([object]$Bundle, [object]$Candidate, [string]$Project, [string]$Sandbox) {
    $verificationParent = Join-Path $Sandbox 'AevryndVerification'; New-Item -ItemType Directory -Path $verificationParent -Force | Out-Null
    $marker = Join-Path $verificationParent '.aevrynd-verification-sandbox'
    if (-not (Test-Path -LiteralPath $marker)) {
        try {
            $stream=[IO.File]::Open($marker,'CreateNew','Write','Read')
            try{$bytes=[Text.UTF8Encoding]::new($false).GetBytes("schema=1`npurpose=engine-lab-network-candidate-verification`n");$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
        } catch [IO.IOException] {}
    }
    $markerExpected="schema=1`npurpose=engine-lab-network-candidate-verification`n";$markerValid=$false
    for($attempt=0;$attempt-lt20-and-not$markerValid;$attempt++){
        try { $markerValid=[IO.File]::ReadAllText($marker)-ceq$markerExpected } catch [IO.IOException] {}
        if(-not$markerValid){Start-Sleep -Milliseconds 25}
    }
    if(-not$markerValid){throw 'Verification sandbox marker rejected.'}
    $root = Join-Path $verificationParent ('verify-engine-lab-v254-publish-' + [Guid]::NewGuid().ToString('N'))
    $powerShell = [IO.Path]::GetFullPath((Get-Process -Id $PID).Path); $scriptPath = Join-Path $Bundle.root 'Test-EngineLabNetworkReleaseCandidate.ps1'
    $arguments = @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$scriptPath,'-ProjectRoot',$Project,'-CandidateRoot',$Candidate.root,'-VerificationRoot',$root)
    $process = Start-Process -FilePath $powerShell -ArgumentList $arguments -WorkingDirectory $Bundle.root -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) { throw "Phase B verifier failed: exit=$($process.ExitCode)" }
    $receiptPath = Join-Path $root 'candidate-verification.json'; $receipt = Read-StrictJson $receiptPath 'Phase B receipt'
    Assert-ExactProperties $receipt @('schemaVersion','verified','candidateRoot','sourceCommit','builderCommit','builderScriptSha256','manifestSha256','manifestFiles','executables','captures','bundledDependencies','ctestPassed','networkGates','verifiedUtc') 'Phase B receipt'
    if ($receipt.schemaVersion -ne 1 -or $receipt.verified -isnot [bool] -or -not $receipt.verified -or (Get-FullPath ([string]$receipt.candidateRoot)) -cne $Candidate.root -or $receipt.sourceCommit -cne $Candidate.manifest.commit -or $receipt.manifestSha256 -cne $Candidate.manifestSha256 -or [int64]$receipt.manifestFiles -ne $Candidate.files.Count-or$receipt.builderCommit-cne$Candidate.policy.builderCommit-or$receipt.builderScriptSha256-cne$Candidate.policy.builderScriptSha256) { throw 'Phase B receipt binding rejected.' }
    [ordered]@{ root = $root; receipt = $receipt; receiptSha256 = Get-Sha256 $receiptPath; powerShellPath = $powerShell; powerShellSha256 = Get-Sha256 $powerShell }
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

$project = Get-FullPath $ProjectRoot; $candidateRootFull = Get-FullPath $CandidateRoot; $bundleRootFull = Get-FullPath $VerifierBundleRoot; $verificationSandbox = Get-FullPath $VerificationSandboxRoot
if (-not $DemoRoot) { $DemoRoot = Join-Path $project 'Demo' }; $demo = Get-FullPath $DemoRoot
foreach ($root in @($project,$candidateRootFull,$bundleRootFull,$verificationSandbox)) { if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw "Required root missing: $root" }; Assert-NoReparseParents $root }
if ($TestFailPoint -and -not $TestMode) { throw 'Failure injection requires TestMode.' }
if ($TestMode) { $marker = Join-Path $demo '.aevrynd-engine-lab-publisher-test-root'; if (-not (Test-Path -LiteralPath $marker -PathType Leaf) -or [IO.File]::ReadAllText($marker) -cne "schema=1`npurpose=engine-lab-publisher-test`n") { throw 'TestMode Demo marker rejected.' } }
if ([IO.Path]::GetPathRoot($candidateRootFull) -cne [IO.Path]::GetPathRoot($demo) -or [IO.Path]::GetPathRoot($bundleRootFull) -cne [IO.Path]::GetPathRoot($demo) -or [IO.Path]::GetPathRoot($verificationSandbox) -cne [IO.Path]::GetPathRoot($demo)) { throw 'Publisher roots must share one volume.' }

$bundle = Get-BundleIdentity $bundleRootFull; $candidate = Get-CandidateIdentity $candidateRootFull
# 驗證器執行期間持有 manifest 與三支腳本的唯讀 lease，封鎖 replace/delete/write TOCTOU。
$bundleLeases=[Collections.Generic.List[IO.FileStream]]::new()
try{
    foreach($name in @('BUNDLE.MANIFEST.json')+$script:ExpectedBundleFiles){$bundleLeases.Add([IO.File]::Open((Join-Path $bundle.root $name),'Open','Read','Read'))}
    $bundleLocked=Get-BundleIdentity $bundle.root;if($bundleLocked.sha256-cne$bundle.sha256-or$bundleLocked.manifestSha256-cne$bundle.manifestSha256){throw 'Verifier bundle changed before lease.'}
    $candidateManifestBefore = $candidate.manifestSha256; $phaseB = Invoke-PhaseBVerifier $bundle $candidate $project $verificationSandbox
}finally{foreach($lease in $bundleLeases){$lease.Dispose()}}
if ((Get-Sha256 $candidate.manifestPath) -cne $candidateManifestBefore) { throw 'Candidate changed during Phase B verification.' }
if ($TestFailPoint -ceq 'Swap') { [IO.File]::AppendAllText((Join-Path $candidate.root 'assets/fixture.bin'),'swap') }

$evidenceNames = @('evidence/candidate-build-receipt.json','evidence/toolchain.json','evidence/pe-tool.json','evidence/normal-network-capture-receipt.json','evidence/normal-network-runtime-receipt.json','evidence/network-capture-receipt.json','evidence/network-runtime-receipt.json')
$evidence = @(); foreach ($relative in $evidenceNames) { $record = @($candidate.files | Where-Object path -CEQ $relative)[0]; if ($null -eq $record) { throw "Required evidence missing: $relative" }; $evidence += ,([ordered]@{ path=$relative; size=[int64]$record.size; sha256=[string]$record.sha256 }) }
$peFiles = @($candidate.files | Where-Object { ([string]$_.path).EndsWith('.exe',[StringComparison]::OrdinalIgnoreCase) -or ([string]$_.path).EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase) } | ForEach-Object { [string]$_.path } | Sort-Object)
$dllFiles = @($peFiles | Where-Object { $_.EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase) })
$releaseId = [string]$candidate.manifest.releaseId

$plan = [ordered]@{ schemaVersion=1; dryRun=[bool]$DryRun; releaseId=$releaseId; candidateFiles=$candidate.files.Count; releaseFiles=($candidate.files.Count+1); peFiles=$peFiles.Count; bundledDlls=$dllFiles.Count; candidateManifestSha256=$candidate.manifestSha256; verifierBundleSha256=$bundle.sha256; sourceCommit=$candidate.manifest.commit }
if ($DryRun) { $plan | ConvertTo-Json -Depth 8; exit 0 }

New-Item -ItemType Directory -Path $demo -Force | Out-Null; Assert-NoReparseParents $demo
$releaseBase = Join-Path $demo 'releases\engine-lab'; $stagingBase = Join-Path $releaseBase '.staging'; $currentRoot = Join-Path $demo 'current';$generationRoot=Join-Path $currentRoot 'engine-lab.generations'
New-Item -ItemType Directory -Path $releaseBase,$stagingBase,$currentRoot,$generationRoot -Force | Out-Null
foreach ($directory in @($demo,$releaseBase,$stagingBase,$currentRoot,$generationRoot)) { if (((Get-Item -LiteralPath $directory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Publisher directory reparse rejected: $directory" } }
if (@(Get-ChildItem -LiteralPath $currentRoot -Filter 'engine-lab.*.pending' -Force -ErrorAction SilentlyContinue).Count-or@(Get-ChildItem -LiteralPath $generationRoot -Filter '*.pending' -Force -ErrorAction SilentlyContinue).Count) { throw 'Unowned engine-lab pointer pending residue rejected.' }
$resolver = Join-Path $currentRoot 'engine-lab.current.json'
$lockPath = Join-Path $demo '.engine-lab-publish.lock'; $lock = Open-PublisherLock $lockPath
$staging = Join-Path $stagingBase ($releaseId + '-' + [Guid]::NewGuid().ToString('N')); $release = Join-Path $releaseBase $releaseId
$resolverPending = Join-Path $currentRoot ('engine-lab.resolver.' + [Guid]::NewGuid().ToString('N') + '.pending');$generationPending=Join-Path $generationRoot ('generation.'+[Guid]::NewGuid().ToString('N')+'.pending');$generation=''
try {
    [void](Assert-Resolver $resolver);[void](Read-GenerationSet $generationRoot $demo)
    New-Item -ItemType Directory -Path $staging | Out-Null; $copied = @(); $index = 0
    foreach ($record in @($candidate.files | Sort-Object path)) {
        $relative = [string]$record.path; $source = Join-Path $candidate.root $relative; Assert-SafeFile $source | Out-Null
        $destination = Join-Path $staging $relative; New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force | Out-Null
        $copied += ,(Copy-VerifiedFile $source $destination $record); if ($index++ -eq 0) { Invoke-Failure 'Copy' }
    }
    Invoke-Failure 'Hash'
    if ((Get-Sha256 $candidate.manifestPath) -cne $candidateManifestBefore) { throw 'Candidate manifest changed after copy.' }
    $receiptClaims = [ordered]@{ sourceCommit=[string]$phaseB.receipt.sourceCommit; builderCommit=[string]$phaseB.receipt.builderCommit; builderScriptSha256=[string]$phaseB.receipt.builderScriptSha256; manifestSha256=[string]$phaseB.receipt.manifestSha256; manifestFiles=[int64]$phaseB.receipt.manifestFiles; ctestPassed=[int64]$phaseB.receipt.ctestPassed; networkGates=[int64]$phaseB.receipt.networkGates }
    $receiptClaimsSha256 = Get-StringSha256 ((($receiptClaims|ConvertTo-Json -Depth 8 -Compress).Replace("`r`n","`n"))+"`n")
    $manifestObject = [ordered]@{
        schemaVersion=1; product='EngineLab'; productVersion='2.54.0'; engineVersion='2.86.0'; releaseId=$releaseId; entry=[string]$candidate.manifest.entry
        treeAlgorithm='candidate-files-plus-self-excluded-release-manifest-v1'; releaseFileCount=($candidate.files.Count+1); pointerContract='immutable-resolver-append-only-semver-generations-v1'; legacyLatestExecutableMode='preserved-unmanaged'
        source=[ordered]@{ commit=[string]$candidate.manifest.commit; candidateManifestSha256=$candidate.manifestSha256; candidateFileCount=$candidate.files.Count }
        builder=[ordered]@{ commit=[string]$phaseB.receipt.builderCommit; scriptSha256=[string]$phaseB.receipt.builderScriptSha256; evidence=$evidence[0..2] }
        verifier=[ordered]@{ bundleSha256=$bundle.sha256; bundleManifestSha256=$bundle.manifestSha256; files=$bundle.files; powerShellSha256=$phaseB.powerShellSha256; receiptClaimsSha256=$receiptClaimsSha256; receiptClaims=$receiptClaims }
        validation=[ordered]@{ normalReceipts=$evidence[3..4]; gbvReceipts=$evidence[5..6]; ctestPassed=[int64]$phaseB.receipt.ctestPassed; networkGates=[int64]$phaseB.receipt.networkGates }
        peClosure=[ordered]@{ recursive=$true; peFiles=$peFiles; bundledDlls=$dllFiles; externalDependenciesEvidenceSha256=[string]$evidence[0].sha256 }
        files=@($copied | ForEach-Object { [ordered]@{ path=$_.path; size=$_.size; sha256=$_.sha256 } })
    }
    $manifestPath = Join-Path $staging 'release.manifest.json'; Write-CanonicalJson $manifestPath $manifestObject; $manifestHash = Get-Sha256 $manifestPath
    Assert-ReleaseTree $staging $manifestObject.files $manifestHash; Invoke-Failure 'Verify'; Invoke-Failure 'BeforeRename'; Invoke-Failure 'Rename'
    if (Test-Path -LiteralPath $release) {
        # 同一 release id 只允許採用逐位元完全相同的既有版本，絕不覆寫。
        Assert-ReleaseTree $release $manifestObject.files $manifestHash
        Remove-Item -LiteralPath $staging -Recurse -Force
    } else { [IO.Directory]::Move($staging,$release) }
    Assert-ReleaseTree $release $manifestObject.files $manifestHash
    $generation=Join-Path $generationRoot ("v2.54.0-$manifestHash.json")
    $generationObject=[ordered]@{schemaVersion=1;role='release-generation';product='EngineLab';productVersion='2.54.0';releaseId=$releaseId;releasePath=('releases/engine-lab/'+$releaseId);entry=([string]$candidate.manifest.entry);manifestSha256=$manifestHash;legacyLatestExecutableMode='preserved-unmanaged'}
    Write-CanonicalJson $generationPending $generationObject;Write-CanonicalJson $resolverPending (New-ResolverObject);Invoke-Failure 'BeforePointer';Invoke-Failure 'Pointer'
    Publish-CreateOnlyFile $generationPending $generation 'Generation pointer';Publish-CreateOnlyFile $resolverPending $resolver 'Current/latest resolver'
    if(-not(Assert-Resolver $resolver)){throw 'Resolver commit missing.'};[void](Read-GenerationSet $generationRoot $demo)
    [ordered]@{ schemaVersion=1; published=$true; releaseId=$releaseId; releaseRoot=$release; manifestSha256=$manifestHash; files=$manifestObject.files.Count; peFiles=$peFiles.Count; bundledDlls=$dllFiles.Count } | ConvertTo-Json -Depth 6
} finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
    foreach($pending in @($resolverPending,$generationPending)){if(Test-Path -LiteralPath $pending){Remove-Item -LiteralPath $pending -Force}}
    $lock.Dispose()
}
