param(
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$DemoRoot = '',
    [switch]$SkipValidation,
    [ValidateSet('','BeforeRename','BeforeCurrentCommit','HoldKnownLease','HoldCurrentCommit')] [string]$FailPoint = '',
    [string]$TestReceiptPath = ''
)
$ErrorActionPreference = 'Stop'
if ($null -eq $FailPoint -or $FailPoint -isnot [string]) { throw 'FailPoint must be a string.' }
$normalizedFailPoint = switch -Exact ([string]$FailPoint) {
    '' { '' }
    'BeforeRename' { 'BeforeRename' }
    'BeforeCurrentCommit' { 'BeforeCurrentCommit' }
    'HoldKnownLease' { 'HoldKnownLease' }
    'HoldCurrentCommit' { 'HoldCurrentCommit' }
    default { throw "Unknown FailPoint token: $FailPoint" }
}
$publisherScriptPath=[IO.Path]::GetFullPath($PSCommandPath)
$publisherScriptSha=(Get-FileHash -LiteralPath $publisherScriptPath -Algorithm SHA256).Hash.ToLowerInvariant()
$publisherProcessPath=(Get-Process -Id $PID).Path
$publisherPowerShellVersion=$PSVersionTable.PSVersion.ToString()
$publisherIdentity="scriptSha=$publisherScriptSha scriptPath=$publisherScriptPath pid=$PID processPath=$publisherProcessPath psVersion=$publisherPowerShellVersion"
if ( -not ('Aevrynd.ReleaseNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Aevrynd { public static class ReleaseNative { [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern bool MoveFileEx(string existingName,string newName,uint flags); } }
'@
}
if ( -not ('Aevrynd.ReleaseLeaseNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Aevrynd { public static class ReleaseLeaseNative {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr CreateFile(string path,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
} }
'@
}
function Test-Reparse([string]$Path) {
    return ((Get-Item -LiteralPath $Path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
}
function Get-StreamHash([IO.FileStream]$Stream) {
    $Stream.Position=0;$hash=(Get-FileHash -InputStream $Stream -Algorithm SHA256).Hash.ToLowerInvariant();$Stream.Position=0;return $hash
}
function Read-StreamText([IO.FileStream]$Stream) {
    if($Stream.Length -gt 1MB){throw 'Release metadata is too large.'};$bytes=[byte[]]::new([int]$Stream.Length);$Stream.Position=0;$read=0;while($read -lt $bytes.Length){$count=$Stream.Read($bytes,$read,$bytes.Length-$read);if($count -le 0){throw 'Release metadata read was incomplete.'};$read+=$count};$Stream.Position=0;return [Text.Encoding]::UTF8.GetString($bytes)
}
function Close-ReleaseLease($Lease) {
    if($null -eq $Lease){return};foreach($stream in @($Lease.Streams)){if($null -ne $stream){$stream.Dispose()}};foreach($handle in @($Lease.Handles)){if($handle -ne [IntPtr]::new(-1) -and $handle -ne [IntPtr]::Zero){[Aevrynd.ReleaseLeaseNative]::CloseHandle($handle)|Out-Null}}
}
function Release-CurrentPointerLease($Lease) {
    if($null -eq $Lease -or $null -eq $Lease.CurrentStream){return};$Lease.CurrentStream.Dispose();$Lease.Streams.Remove($Lease.CurrentStream)|Out-Null;$Lease.CurrentStream=$null
}
function Invoke-TestHandshake([string]$Mode,[string]$Phase,[string]$ReceiptPath,[string]$AckPath,[string]$ScriptSha,[int]$ProcessId) {
    $receiptPending=$ReceiptPath+'.pending'
    [IO.File]::WriteAllLines($receiptPending,@("mode=$Mode","phase=$Phase","scriptSha=$ScriptSha","pid=$ProcessId"),[Text.UTF8Encoding]::new($false))
    if( -not [Aevrynd.ReleaseNative]::MoveFileEx($receiptPending,$ReceiptPath,0x9)){throw 'Atomic publisher test receipt failed.'}
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds-lt30){
        if(Test-Path -LiteralPath $AckPath -PathType Leaf){$ack=[IO.File]::ReadAllText($AckPath).Trim();if($ack -ne "ack=$Phase"){throw 'Publisher test acknowledgement is invalid.'};Remove-Item -LiteralPath $AckPath -Force;return}
        Start-Sleep -Milliseconds 20
    }
    throw 'Publisher test acknowledgement timed out.'
}
function Test-ReleaseContent([string]$Root,[string]$ManifestPath,[string]$ExpectedId='',[System.Collections.Generic.List[object]]$Streams=$null) {
    try {
        $canonicalRoot=[IO.Path]::GetFullPath($Root).TrimEnd('\');$canonicalManifest=[IO.Path]::GetFullPath($ManifestPath)
        if([IO.Path]::GetDirectoryName($canonicalManifest) -ne $canonicalRoot -or (Test-Reparse $canonicalRoot) -or (Test-Reparse $canonicalManifest)){return $false}
        $ownedStreams=$null;if($null -eq $Streams){$ownedStreams=[Collections.Generic.List[object]]::new();$Streams=$ownedStreams}
        try {
            $manifestStream=[IO.File]::Open($canonicalManifest,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$Streams.Add($manifestStream)
            $lines=(Read-StreamText $manifestStream).Split("`n",[StringSplitOptions]::None)|ForEach-Object{$_.TrimEnd("`r")}|Where-Object{$_ -ne ''}
            if(@($lines|Where-Object{$_ -eq 'schema=1'}).Count -ne 1 -or @($lines|Where-Object{$_ -match '^releaseId=[A-Za-z0-9._+\-]{1,96}$'}).Count -ne 1 -or @($lines|Where-Object{$_ -match '^gameVersion=[0-9]+\.[0-9]+\.[0-9]+$'}).Count -ne 1 -or @($lines|Where-Object{$_ -match '^engineVersion=[0-9]+\.[0-9]+\.[0-9]+$'}).Count -ne 1){return $false}
            $idLine=@($lines|Where-Object{$_ -like 'releaseId=*'})[0];if($ExpectedId -and $idLine -ne "releaseId=$ExpectedId"){return $false}
            $fileLines=@($lines|Where-Object{$_ -like 'file=*'});$knownLines=@($lines|Where-Object{$_ -eq 'schema=1' -or $_ -match '^releaseId=[A-Za-z0-9._+\-]{1,96}$' -or $_ -match '^(gameVersion|engineVersion)=[0-9]+\.[0-9]+\.[0-9]+$' -or $_ -like 'file=*'})
            if($knownLines.Count -ne $lines.Count -or $fileLines.Count -eq 0){return $false}
            $declared=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);$runtimeCount=0
            foreach($line in $fileLines){if($line -notmatch '^file=([^ ]+) size=([0-9]+) sha256=([0-9a-f]{64})$'){return $false};$relative=$Matches[1];$expectedSize=[Int64]$Matches[2];$expectedHash=$Matches[3];if($relative.Contains('\') -or $relative.Contains('..') -or [IO.Path]::IsPathRooted($relative) -or -not $declared.Add($relative)){return $false};if($relative -eq 'Aevrynd-Character-Lab.runtime.exe'){$runtimeCount++};$file=[IO.Path]::GetFullPath((Join-Path $canonicalRoot $relative));$prefix=$canonicalRoot+'\';if( -not $file.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $file -PathType Leaf) -or (Test-Reparse $file)){return $false};$stream=[IO.File]::Open($file,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$Streams.Add($stream);if($stream.Length -ne $expectedSize -or (Get-StreamHash $stream) -ne $expectedHash){return $false}}
            if($runtimeCount-ne1){return $false}
            $actual=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);$pending=[Collections.Generic.Stack[string]]::new();$pending.Push($canonicalRoot)
            while($pending.Count){$directory=$pending.Pop();foreach($entry in Get-ChildItem -LiteralPath $directory -Force){if(Test-Reparse $entry.FullName){return $false};if($entry.PSIsContainer){$pending.Push($entry.FullName)}elseif($entry.FullName -ne $canonicalManifest){$relative=$entry.FullName.Substring($canonicalRoot.Length+1).Replace('\','/');if( -not $actual.Add($relative)){return $false}}}}
            return $actual.SetEquals($declared)
        } finally {if($null -ne $ownedStreams){foreach($stream in $ownedStreams){$stream.Dispose()}}}
    } catch {Write-Verbose "Release content validation rejected: $($_.Exception.Message)";return $false}
}
function Get-KnownPinnedLauncherLease([string]$DemoRoot,[string]$ExpectedLatestHash,[string]$ReleaseBase) {
    $streams=[Collections.Generic.List[object]]::new();$handles=[Collections.Generic.List[IntPtr]]::new();$lease=[pscustomobject]@{Streams=$streams;Handles=$handles;CurrentStream=$null;CurrentHash=''}
    try {
        $canonicalDemo=[IO.Path]::GetFullPath($DemoRoot).TrimEnd('\');$currentRoot=Join-Path $canonicalDemo 'current';$currentPath=Join-Path $currentRoot 'character-lab.current';if((Test-Reparse $canonicalDemo) -or (Test-Reparse $currentRoot) -or -not (Test-Path -LiteralPath $currentPath -PathType Leaf) -or (Test-Reparse $currentPath)){throw 'Current pointer is invalid.'}
        $currentStream=[IO.File]::Open($currentPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$streams.Add($currentStream);$lease.CurrentStream=$currentStream;$lease.CurrentHash=Get-StreamHash $currentStream;$currentLines=(Read-StreamText $currentStream).Split("`n",[StringSplitOptions]::RemoveEmptyEntries)|ForEach-Object{$_.TrimEnd("`r")}
        if(@($currentLines|Where-Object{$_ -eq 'schema=1'}).Count -ne 1 -or @($currentLines|Where-Object{$_ -match '^releaseId=v[0-9]+\.[0-9]+\.[0-9]+-engine-v[0-9]+\.[0-9]+\.[0-9]+-[0-9a-f]{8}$'}).Count -ne 1 -or @($currentLines|Where-Object{$_ -match '^manifestSha256=([0-9a-f]{64})$'}).Count -ne 1 -or $currentLines.Count -ne 3){throw 'Current pointer contract failed.'}
        $candidateId=(@($currentLines|Where-Object{$_ -like 'releaseId=*'})[0]).Substring(10);$pointerHash=(@($currentLines|Where-Object{$_ -like 'manifestSha256=*'})[0]).Substring(15)
        $releasesRoot=Join-Path $canonicalDemo 'releases';$canonicalBase=[IO.Path]::GetFullPath($ReleaseBase).TrimEnd('\');$candidate=[IO.Path]::GetFullPath((Join-Path $canonicalBase $candidateId));if([IO.Path]::GetDirectoryName($candidate) -ne $canonicalBase){throw 'Release directory escaped its root.'};foreach($directory in @($canonicalDemo,$releasesRoot,$canonicalBase,$candidate)){if( -not (Test-Path -LiteralPath $directory -PathType Container) -or (Test-Reparse $directory)){throw 'Release directory contract failed.'}}
        foreach($entry in @(@($canonicalBase,[uint32]3),@($candidate,[uint32]1))){$handle=[Aevrynd.ReleaseLeaseNative]::CreateFile($entry[0],[uint32]2147483648,$entry[1],[IntPtr]::Zero,[uint32]3,[uint32]33554432,[IntPtr]::Zero);if($handle -eq [IntPtr]::new(-1)){throw 'Release directory lease failed.'};$handles.Add($handle)}
        $pinned=Join-Path $DemoRoot "Aevrynd-Character-Lab-$candidateId.exe";if( -not (Test-Path -LiteralPath $pinned -PathType Leaf) -or (Test-Reparse $pinned)){throw 'Pinned launcher is invalid.'};$pinnedStream=[IO.File]::Open($pinned,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$streams.Add($pinnedStream);if((Get-StreamHash $pinnedStream) -ne $ExpectedLatestHash.ToLowerInvariant()){throw 'Pinned launcher hash does not match latest.'}
        $manifest=Join-Path $candidate 'release.manifest';if( -not (Test-ReleaseContent $candidate $manifest $candidateId $streams)){throw 'Pinned release content is invalid.'};$manifestStream=@($streams|Where-Object{$_ -is [IO.FileStream] -and $_.Name -eq $manifest})[0];if($null -eq $manifestStream -or (Get-StreamHash $manifestStream) -ne $pointerHash){throw 'Current manifest hash does not match.'}
        return $lease
    } catch {Write-Verbose "Known launcher validation rejected: $($_.Exception.Message)";Close-ReleaseLease $lease;return $null}
}
function Get-PublishedReleaseLease([string]$Release,[string]$ExpectedId,[string]$ExpectedManifestHash) {
    $streams=[Collections.Generic.List[object]]::new();$handles=[Collections.Generic.List[IntPtr]]::new();$lease=[pscustomobject]@{Streams=$streams;Handles=$handles}
    try{$canonical=[IO.Path]::GetFullPath($Release).TrimEnd('\');if( -not (Test-Path -LiteralPath $canonical -PathType Container) -or (Test-Reparse $canonical)){throw 'Published release directory is invalid.'};$handle=[Aevrynd.ReleaseLeaseNative]::CreateFile($canonical,[uint32]2147483648,[uint32]1,[IntPtr]::Zero,[uint32]3,[uint32]33554432,[IntPtr]::Zero);if($handle -eq [IntPtr]::new(-1)){throw 'Published release directory lease failed.'};$handles.Add($handle);$manifest=Join-Path $canonical 'release.manifest';if( -not (Test-ReleaseContent $canonical $manifest $ExpectedId $streams)){throw 'Published release content is invalid.'};$manifestStream=@($streams|Where-Object{$_ -is [IO.FileStream] -and $_.Name -eq $manifest})[0];if($null -eq $manifestStream -or (Get-StreamHash $manifestStream) -ne $ExpectedManifestHash){throw 'Published release manifest hash is invalid.'};return $lease}catch{Write-Verbose "Published release validation rejected: $($_.Exception.Message)";Close-ReleaseLease $lease;return $null}
}
if ( -not $DemoRoot) { $DemoRoot = Join-Path $ProjectRoot 'Demo' }
$DemoRoot = [IO.Path]::GetFullPath($DemoRoot)
$manifest = Get-Content -LiteralPath (Join-Path $ProjectRoot 'games\character_lab\game.json') -Raw | ConvertFrom-Json
$gitId = (& git -C $ProjectRoot rev-parse --short=8 HEAD).Trim()
$releaseId = "v$($manifest.version)-engine-v$($manifest.engineVersion)-$gitId"
if ($releaseId -notmatch '^[A-Za-z0-9._+-]{1,96}$') { throw 'Invalid release id.' }
$releaseBase = Join-Path $DemoRoot 'releases\character-lab'
$stagingBase = Join-Path $releaseBase '.staging'
$staging = Join-Path $stagingBase ($releaseId + '-' + [Guid]::NewGuid().ToString('N'))
$release = Join-Path $releaseBase $releaseId
$currentDirectory = Join-Path $DemoRoot 'current'
$pending = Join-Path $currentDirectory 'character-lab.pending'
$lockPath = Join-Path $DemoRoot '.character-lab-publish.lock'
New-Item -ItemType Directory -Force -Path $DemoRoot,$releaseBase,$stagingBase,$currentDirectory | Out-Null
$lock = [IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$knownLauncherLease=$null
$publishedReleaseLease=$null
$failPointConsumed=$false
try {
    $bin = Join-Path $ProjectRoot 'engine\build\bin\Release';$launcher = Join-Path $bin 'Aevrynd-Character-Lab-Launcher.exe';$latest=Join-Path $DemoRoot 'Aevrynd-Character-Lab.exe';$latestPending=Join-Path $DemoRoot 'Aevrynd-Character-Lab.pending';$knownLeaseSignal=Join-Path $DemoRoot '.known-release-lease.test';$launcherHash=(Get-FileHash -LiteralPath $launcher -Algorithm SHA256).Hash;$latestMode='Create';$previousLatestHash=''
    if(Test-Path -LiteralPath $latest){$previousLatestHash=(Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash;$knownLauncherLease=Get-KnownPinnedLauncherLease $DemoRoot $previousLatestHash $releaseBase;if($null -ne $knownLauncherLease){$latestMode='Known'}elseif($previousLatestHash -eq $launcherHash){$latestMode='Same'}else{$approvedLegacy=@((Get-FileHash -LiteralPath (Join-Path $bin 'Aevrynd-Character-Lab.exe') -Algorithm SHA256).Hash);Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'games\character_lab\dist') -File -Filter '*.exe' -ErrorAction SilentlyContinue|ForEach-Object{$approvedLegacy+=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash};if($previousLatestHash -notin $approvedLegacy){throw 'Existing Character Lab latest executable is neither an approved pinned launcher nor a known legacy runtime.'};$latestMode='Legacy'}}
    $testAckPath=Join-Path $DemoRoot '.publisher-test-ack';if($TestReceiptPath){$canonicalReceipt=[IO.Path]::GetFullPath($TestReceiptPath);$canonicalAck=[IO.Path]::GetFullPath($testAckPath);if( -not $normalizedFailPoint -or [IO.Path]::GetDirectoryName($canonicalReceipt) -ne $DemoRoot -or [IO.Path]::GetFileName($canonicalReceipt) -ne '.publisher-test-receipt' -or [IO.Path]::GetDirectoryName($canonicalAck) -ne $DemoRoot -or [IO.Path]::GetFileName($canonicalAck) -ne '.publisher-test-ack'){throw 'Publisher test handshake path is invalid.'}}
    if($normalizedFailPoint){Write-Host "Publisher test classification mode=$latestMode failPoint=$normalizedFailPoint"}
    $canonicalStaging = [IO.Path]::GetFullPath($stagingBase).TrimEnd('\') + '\'
    foreach ($orphan in Get-ChildItem -LiteralPath $stagingBase -Directory -ErrorAction SilentlyContinue) {
        $canonicalOrphan = [IO.Path]::GetFullPath($orphan.FullName)
        if ($canonicalOrphan.StartsWith($canonicalStaging,[StringComparison]::OrdinalIgnoreCase) -and $orphan.Name -match '^[A-Za-z0-9._+-]{1,96}-[0-9a-f]{32}$') { Remove-Item -LiteralPath $canonicalOrphan -Recurse -Force }
    }
    New-Item -ItemType Directory -Path $staging | Out-Null
    Copy-Item -LiteralPath (Join-Path $bin 'Aevrynd-Character-Lab.exe') -Destination (Join-Path $staging 'Aevrynd-Character-Lab.runtime.exe')
    Get-ChildItem -LiteralPath $bin -File -Filter '*.dll' | Copy-Item -Destination $staging
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'games\character_lab\assets') -Destination (Join-Path $staging 'assets') -Recurse
    foreach ($required in @('assets\model-packs\realistic-female\realistic-female.aeskin','assets\model-packs\realistic-male\realistic-male.aeskin','assets\material-packs\original\female.materials.json','assets\material-packs\original\male.materials.json')) { if ( -not (Test-Path -LiteralPath (Join-Path $staging $required))) { throw "Missing release asset: $required" } }
    $entries = Get-ChildItem -LiteralPath $staging -File -Recurse | Sort-Object { $_.FullName.Substring($staging.Length + 1) }
    $manifestPath = Join-Path $staging 'release.manifest'
    $lines = @('schema=1',"releaseId=$releaseId","gameVersion=$($manifest.version)","engineVersion=$($manifest.engineVersion)")
    foreach ($file in $entries) { $relative = $file.FullName.Substring($staging.Length + 1).Replace('\','/'); $lines += "file=$relative size=$($file.Length) sha256=$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())" }
    [IO.File]::WriteAllLines($manifestPath,$lines,[Text.UTF8Encoding]::new($false))
    $manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ( -not $SkipValidation) {
        $validationState = Join-Path $stagingBase ('validation-' + [Guid]::NewGuid().ToString('N'))
        $oldReleaseId = $env:AEVRYND_CHARACTER_RELEASE_ID;$oldStateRoot = $env:AEVRYND_CHARACTER_STATE_ROOT
        try {
            $env:AEVRYND_CHARACTER_RELEASE_ID = $releaseId;$env:AEVRYND_CHARACTER_STATE_ROOT = $validationState
            $validation = Start-Process -FilePath (Join-Path $staging 'Aevrynd-Character-Lab.runtime.exe') -ArgumentList '--capture-demo' -WorkingDirectory $staging -PassThru -Wait
            if ($validation.ExitCode -ne 0) { throw "Release validation failed with exit $($validation.ExitCode)." }
        } finally { $env:AEVRYND_CHARACTER_RELEASE_ID=$oldReleaseId;$env:AEVRYND_CHARACTER_STATE_ROOT=$oldStateRoot;if(Test-Path -LiteralPath $validationState){Remove-Item -LiteralPath $validationState -Recurse -Force} }
    }
    if ($normalizedFailPoint -eq 'BeforeRename') { $failPointConsumed=$true;throw "Injected failure before immutable release rename; mode=$latestMode" }
    if (Test-Path -LiteralPath $release) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    } else { Move-Item -LiteralPath $staging -Destination $release }
    $publishedReleaseLease=Get-PublishedReleaseLease $release $releaseId $manifestHash;if($null -eq $publishedReleaseLease){throw 'Release id already exists with different or damaged content.'}
    if($latestMode -eq 'Create') {
        Copy-Item -LiteralPath $launcher -Destination $latestPending -Force;if( -not [Aevrynd.ReleaseNative]::MoveFileEx($latestPending,$latest,0x8)){throw 'Atomic latest launcher creation failed.'}
    } elseif($latestMode -eq 'Same') {
        if((Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash -ne $launcherHash){throw 'Latest launcher changed after preflight.'}
    } elseif($latestMode -eq 'Known') {
        switch -Exact ($normalizedFailPoint) {
            'HoldKnownLease' {
                $failPointConsumed=$true
                Invoke-TestHandshake $latestMode 'KnownLeaseReady' $canonicalReceipt $canonicalAck $publisherScriptSha $PID
            }
        }
        if((Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash -ne $previousLatestHash){throw 'Known launcher changed after preflight.'}
    } else {
        if((Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash -ne $previousLatestHash){throw 'Legacy runtime changed after preflight.'};$legacyBackup=Join-Path $DemoRoot ('Aevrynd-Character-Lab-Legacy-'+$previousLatestHash.Substring(0,8).ToLowerInvariant()+'.exe');if(Test-Path -LiteralPath $legacyBackup){if((Get-FileHash -LiteralPath $legacyBackup -Algorithm SHA256).Hash -ne $previousLatestHash){throw 'Legacy backup name collision.'}}else{Copy-Item -LiteralPath $latest -Destination $legacyBackup;if((Get-FileHash -LiteralPath $legacyBackup -Algorithm SHA256).Hash -ne $previousLatestHash){throw 'Legacy backup verification failed.'}};Copy-Item -LiteralPath $launcher -Destination $latestPending -Force;if( -not [Aevrynd.ReleaseNative]::MoveFileEx($latestPending,$latest,0x9)){throw 'Atomic legacy launcher migration failed.'}
    }
    $pinned = Join-Path $DemoRoot "Aevrynd-Character-Lab-$releaseId.exe";$pinnedSource=if($latestMode -eq 'Known'){$latest}else{$launcher};$pinnedSourceHash=(Get-FileHash -LiteralPath $pinnedSource -Algorithm SHA256).Hash
    if (Test-Path -LiteralPath $pinned) { if((Get-FileHash $pinned).Hash -ne $pinnedSourceHash){throw 'Pinned launcher exists with different content.'} } else { $pinnedPending=$pinned+'.pending';Copy-Item -LiteralPath $pinnedSource -Destination $pinnedPending;if( -not [Aevrynd.ReleaseNative]::MoveFileEx($pinnedPending,$pinned,0x8)){throw 'Atomic pinned launcher publish failed.'} }
    $current = Join-Path $currentDirectory 'character-lab.current'
    [IO.File]::WriteAllLines($pending,@('schema=1',"releaseId=$releaseId","manifestSha256=$manifestHash"),[Text.UTF8Encoding]::new($false));$expectedCurrent=[IO.File]::ReadAllBytes($pending)
    if ($normalizedFailPoint -eq 'BeforeCurrentCommit') { $failPointConsumed=$true;throw "Injected failure before current pointer commit; mode=$latestMode" }
    if($latestMode -eq 'Known'){if((Get-StreamHash $knownLauncherLease.CurrentStream) -ne $knownLauncherLease.CurrentHash){throw 'Current pointer changed while leased.'};Release-CurrentPointerLease $knownLauncherLease}
    $currentCommitSignal=Join-Path $DemoRoot '.current-commit.test'
    switch -Exact ($normalizedFailPoint) {
        'HoldCurrentCommit' {
            $failPointConsumed=$true
            Invoke-TestHandshake $latestMode 'CurrentCommitReady' $canonicalReceipt $canonicalAck $publisherScriptSha $PID
        }
    }
    if($normalizedFailPoint -and -not $failPointConsumed){throw "FailPoint $normalizedFailPoint not reached; mode=$latestMode $publisherIdentity"}
    if ( -not [Aevrynd.ReleaseNative]::MoveFileEx($pending,$current,0x9)) { throw "Atomic current pointer commit failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
    if([Convert]::ToBase64String([IO.File]::ReadAllBytes($current)) -ne [Convert]::ToBase64String($expectedCurrent)){throw 'Committed current pointer verification failed.'}
    Close-ReleaseLease $knownLauncherLease;$knownLauncherLease=$null
    Close-ReleaseLease $publishedReleaseLease;$publishedReleaseLease=$null
    Write-Host "Published immutable Character Lab release $releaseId" -ForegroundColor Green
} finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
    if (Test-Path -LiteralPath $pending) { Remove-Item -LiteralPath $pending -Force }
    if ($latestPending -and (Test-Path -LiteralPath $latestPending)) { Remove-Item -LiteralPath $latestPending -Force }
    if ($knownLeaseSignal -and (Test-Path -LiteralPath $knownLeaseSignal)) { Remove-Item -LiteralPath $knownLeaseSignal -Force }
    if ($currentCommitSignal -and (Test-Path -LiteralPath $currentCommitSignal)) { Remove-Item -LiteralPath $currentCommitSignal -Force }
    if ($TestReceiptPath -and (Test-Path -LiteralPath $TestReceiptPath)) { Remove-Item -LiteralPath $TestReceiptPath -Force }
    if ($TestReceiptPath -and (Test-Path -LiteralPath ($TestReceiptPath+'.pending'))) { Remove-Item -LiteralPath ($TestReceiptPath+'.pending') -Force }
    if ($testAckPath -and (Test-Path -LiteralPath $testAckPath)) { Remove-Item -LiteralPath $testAckPath -Force }
    Close-ReleaseLease $knownLauncherLease
    Close-ReleaseLease $publishedReleaseLease
    $lock.Dispose()
}
