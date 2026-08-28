param(
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'
if ( -not ('Aevrynd.PublisherTestNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Aevrynd { public static class PublisherTestNative {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern uint GetFileAttributes(string path);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool RemoveDirectory(string path);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool MoveFileEx(string existingName,string newName,uint flags);
} }
'@
}
$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
$publisher = Join-Path $ProjectRoot 'tools\ci\publish-character-lab-release.ps1'
$bin = Join-Path $ProjectRoot 'engine\build\bin\Release'
$launcher = Join-Path $bin 'Aevrynd-Character-Lab-Launcher.exe'
$runtime = Join-Path $bin 'Aevrynd-Character-Lab.exe'
$tempBase = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('Aep-' + [Guid]::NewGuid().ToString('N').Substring(0,8))))
$safePrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\Aep-'

function Assert-True([bool]$Condition,[string]$Message) {
    if ( -not $Condition) { throw $Message }
}

function Remove-FixtureJunction([string]$Path,[string]$FixtureRoot,[string]$FixtureName,[string]$ExpectedLeaf) {
    $canonicalTemp = $tempBase.TrimEnd('\')
    $canonicalFixture = [IO.Path]::GetFullPath($FixtureRoot).TrimEnd('\')
    $canonicalPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    if ([IO.Path]::GetDirectoryName($canonicalFixture) -ne $canonicalTemp -or
        [IO.Path]::GetFileName($canonicalFixture) -ne $FixtureName -or
        $FixtureName -notmatch '^[jmf]$' -or
        -not $canonicalPath.StartsWith($canonicalFixture + '\',[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($canonicalPath) -ne $ExpectedLeaf) {
        throw 'Junction cleanup path escaped its fixture boundary.'
    }
    $attributes = [Aevrynd.PublisherTestNative]::GetFileAttributes($canonicalPath)
    if ($attributes -eq [uint32]::MaxValue -or
        ($attributes -band [uint32][IO.FileAttributes]::Directory) -eq 0 -or
        ($attributes -band [uint32][IO.FileAttributes]::ReparsePoint) -eq 0) {
        throw 'Junction cleanup target is not a directory reparse point.'
    }
    if ( -not [Aevrynd.PublisherTestNative]::RemoveDirectory($canonicalPath)) {
        throw "Junction cleanup failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
}

function Invoke-JunctionRejection([string]$DemoRoot,[string]$NewId,[string]$Junction,[string]$FixtureName,[string]$ExpectedLeaf) {
    $caseError = $null
    $cleanupError = $null
    try { Assert-RejectedUnchanged $DemoRoot $NewId } catch { $caseError = $_.Exception }
    try { Remove-FixtureJunction $Junction $DemoRoot $FixtureName $ExpectedLeaf } catch { $cleanupError = $_.Exception }
    if ($null -ne $caseError) {
        $suffix = if ($null -ne $cleanupError) { " Cleanup also failed: $($cleanupError.Message)" } else { '' }
        throw [InvalidOperationException]::new("Junction rejection case failed: $($caseError.Message)$suffix",$caseError)
    }
    if ($null -ne $cleanupError) { throw $cleanupError }
}

function Read-CurrentId([string]$DemoRoot) {
    $line = Get-Content -LiteralPath (Join-Path $DemoRoot 'current\character-lab.current') | Where-Object { $_ -like 'releaseId=*' }
    if (@($line).Count -ne 1) { throw 'Current pointer release id is invalid.' }
    return $line.Substring(10)
}

function Write-Current([string]$DemoRoot,[string]$ReleaseId) {
    $manifest = Join-Path $DemoRoot "releases\character-lab\$ReleaseId\release.manifest"
    $hash = (Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllLines((Join-Path $DemoRoot 'current\character-lab.current'),@('schema=1',"releaseId=$ReleaseId","manifestSha256=$hash"),[Text.UTF8Encoding]::new($false))
}

function Add-ExecutableOverlay([string]$Path,[byte]$Value) {
    $stream = [IO.File]::Open($Path,[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $stream.WriteByte($Value);$stream.Flush($true) } finally { $stream.Dispose() }
}

function Initialize-OldPinnedState([string]$DemoRoot) {
    $game = Get-Content -LiteralPath (Join-Path $ProjectRoot 'games\character_lab\game.json') -Raw | ConvertFrom-Json
    $newId = "v$($game.version)-engine-v$($game.engineVersion)-$((& git -C $ProjectRoot rev-parse --short=8 HEAD).Trim())"
    $oldId = 'v0.0.0-engine-v0.0.0-00000001'
    $releaseBase = Join-Path $DemoRoot 'releases\character-lab'
    $oldRelease = Join-Path $releaseBase $oldId
    New-Item -ItemType Directory -Path $oldRelease,(Join-Path $DemoRoot 'current') -Force | Out-Null
    $oldRuntime = Join-Path $oldRelease 'Aevrynd-Character-Lab.runtime.exe'
    Copy-Item -LiteralPath $runtime -Destination $oldRuntime
    $oldManifest = Join-Path $oldRelease 'release.manifest'
    $runtimeItem=Get-Item -LiteralPath $oldRuntime;$runtimeHash=(Get-FileHash -LiteralPath $oldRuntime -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllLines($oldManifest,@('schema=1',"releaseId=$oldId",'gameVersion=0.0.0','engineVersion=0.0.0',"file=Aevrynd-Character-Lab.runtime.exe size=$($runtimeItem.Length) sha256=$runtimeHash"),[Text.UTF8Encoding]::new($false))
    $oldPinned = Join-Path $DemoRoot "Aevrynd-Character-Lab-$oldId.exe"
    Copy-Item -LiteralPath $launcher -Destination $oldPinned
    Add-ExecutableOverlay $oldPinned 0xA5
    Copy-Item -LiteralPath $oldPinned -Destination (Join-Path $DemoRoot 'Aevrynd-Character-Lab.exe') -Force
    Write-Current $DemoRoot $oldId
    return [pscustomobject]@{ NewId=$newId;OldId=$oldId;OldPinned=$oldPinned }
}

function Invoke-ExpectedFailure([string]$DemoRoot) {
    $failed = $false
    try { & $publisher -ProjectRoot $ProjectRoot -DemoRoot $DemoRoot -SkipValidation } catch { $failed = $true }
    Assert-True $failed 'Publisher unexpectedly accepted an invalid latest executable.'
}

function Assert-NoPartial([string]$DemoRoot) {
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot 'Aevrynd-Character-Lab.pending'))) 'Latest pending file was left behind.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot 'current\character-lab.pending'))) 'Current pending file was left behind.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot '.known-release-lease.test'))) 'Known release lease signal was left behind.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot '.current-commit.test'))) 'Current commit signal was left behind.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot '.publisher-test-receipt'))) 'Publisher test receipt was left behind.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot '.publisher-test-receipt.pending'))) 'Publisher test receipt pending file was left behind.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot '.publisher-test-ack'))) 'Publisher test acknowledgement was left behind.'
    $staging = Join-Path $DemoRoot 'releases\character-lab\.staging'
    Assert-True (@(Get-ChildItem -LiteralPath $staging -Directory -ErrorAction SilentlyContinue).Count -eq 0) 'Release staging directory was left behind.'
}

function Assert-FreshKnownState([string]$DemoRoot,$State) {
    $current = Join-Path $DemoRoot 'current\character-lab.current'
    $manifest = Join-Path $DemoRoot "releases\character-lab\$($State.OldId)\release.manifest"
    $pointerHash = @([IO.File]::ReadAllLines($current) | Where-Object { $_ -like 'manifestSha256=*' })[0].Substring(15)
    Assert-True ((Read-CurrentId $DemoRoot) -eq $State.OldId) 'Fresh known fixture current id is wrong.'
    Assert-True ($pointerHash -eq (Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()) 'Fresh known fixture manifest hash is wrong.'
    Assert-True ((Get-FileHash -LiteralPath (Join-Path $DemoRoot 'Aevrynd-Character-Lab.exe')).Hash -eq (Get-FileHash -LiteralPath $State.OldPinned).Hash) 'Fresh known fixture latest and pinned differ.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot "releases\character-lab\$($State.NewId)"))) 'Fresh known fixture already contains the new release.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot "Aevrynd-Character-Lab-$($State.NewId).exe"))) 'Fresh known fixture already contains the new pinned launcher.'
}

function Start-PublisherChild([string]$DemoRoot,[string]$FailPoint,[string]$OutputPath,[string]$ErrorPath) {
    $receipt = Join-Path $DemoRoot '.publisher-test-receipt'
    $quote = { param([string]$Value) "'" + $Value.Replace("'","''") + "'" }
    $command = "& $(& $quote $publisher) -ProjectRoot $(& $quote $ProjectRoot) -DemoRoot $(& $quote $DemoRoot) -SkipValidation -FailPoint $FailPoint -TestReceiptPath $(& $quote $receipt)"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    return Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList @('-NoProfile','-EncodedCommand',$encoded) -RedirectStandardOutput $OutputPath -RedirectStandardError $ErrorPath -PassThru
}

function Stop-TestProcessForDiagnostics([Diagnostics.Process]$Process,[string]$OutputPath,[string]$ErrorPath,[string]$Reason) {
    if( -not $Process.HasExited -and -not $Process.WaitForExit(1000)){[void]$Process.Kill();[void]$Process.WaitForExit()}
    else{[void]$Process.WaitForExit()}
    $exitCode=$Process.ExitCode
    [void]$Process.Dispose()
    $stdout=if(Test-Path -LiteralPath $OutputPath){[IO.File]::ReadAllText($OutputPath)}else{''}
    $stderr=if(Test-Path -LiteralPath $ErrorPath){[IO.File]::ReadAllText($ErrorPath)}else{''}
    Write-Host "Publisher child diagnostic exit=$exitCode out=$stdout err=$stderr"
    throw "$Reason exit=$exitCode"
}

function Complete-TestProcess {
    [OutputType([int])]
    param([Diagnostics.Process]$Process,[int]$TimeoutMilliseconds,[string]$Name)
    $completed=[bool]$Process.WaitForExit($TimeoutMilliseconds)
    if( -not $completed){[void]$Process.Kill();[void]$Process.WaitForExit();[void]$Process.Dispose();throw "$Name timed out."}
    [void]$Process.WaitForExit()
    $exitCode=[int]$Process.ExitCode
    [void]$Process.Dispose()
    return [int]$exitCode
}

function Close-TestProcess([Diagnostics.Process]$Process) {
    if($null -eq $Process){return}
    try{if( -not $Process.HasExited){[void]$Process.Kill();[void]$Process.WaitForExit()}}catch{}finally{try{[void]$Process.Dispose()}catch{}}
}

function Wait-PublisherHandshake([Diagnostics.Process]$Process,[string]$DemoRoot,[string]$ExpectedPhase,[string]$ExpectedPublisherSha,[string]$OutputPath,[string]$ErrorPath) {
    $receipt = Join-Path $DemoRoot '.publisher-test-receipt'
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds-lt30) {
        if(Test-Path -LiteralPath $receipt) {
            $lines=@([IO.File]::ReadAllLines($receipt))
            if($lines.Count -eq 4 -and $lines[0] -eq 'mode=Known' -and $lines[1] -eq "phase=$ExpectedPhase" -and $lines[2] -eq "scriptSha=$ExpectedPublisherSha" -and $lines[3] -match '^pid=[0-9]+$'){return}
            Stop-TestProcessForDiagnostics $Process $OutputPath $ErrorPath "Publisher child emitted an invalid handshake receipt; expectedScriptSha=$ExpectedPublisherSha receipt=$($lines -join ',')"
        }
        if($Process.HasExited) { break }
        Start-Sleep -Milliseconds 20
    }
    Stop-TestProcessForDiagnostics $Process $OutputPath $ErrorPath "Publisher child did not reach $ExpectedPhase; expectedScriptSha=$ExpectedPublisherSha."
}

function Send-PublisherAck([string]$DemoRoot,[string]$Phase) {
    $ack=Join-Path $DemoRoot '.publisher-test-ack';$pending=$ack+'.pending'
    [IO.File]::WriteAllText($pending,"ack=$Phase`n",[Text.UTF8Encoding]::new($false))
    Assert-True ([Aevrynd.PublisherTestNative]::MoveFileEx($pending,$ack,0x9)) 'Atomic publisher test acknowledgement failed.'
}

function Remove-TestTempTree {
    if( -not $tempBase.StartsWith($safePrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Test temp root failed its deletion boundary.'}
    $lastError=$null
    for($attempt=0;$attempt-lt20;$attempt++) {
        try { if(Test-Path -LiteralPath $tempBase){Remove-Item -LiteralPath $tempBase -Recurse -Force};if( -not (Test-Path -LiteralPath $tempBase)){return} } catch { $lastError=$_.Exception }
        Start-Sleep -Milliseconds 50
    }
    $detail=if($null -ne $lastError){$lastError.Message}else{'path still exists'}
    throw "Test temp cleanup did not complete within its bounded retry: $detail"
}

function Assert-RejectedUnchanged([string]$DemoRoot,[string]$NewId) {
    $latest=Join-Path $DemoRoot 'Aevrynd-Character-Lab.exe';$current=Join-Path $DemoRoot 'current\character-lab.current'
    $latestHash=(Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash;$currentBytes=[IO.File]::ReadAllBytes($current)
    Invoke-ExpectedFailure $DemoRoot
    Assert-True ((Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash -eq $latestHash) 'Rejected publisher changed latest.'
    Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($current)) -eq [Convert]::ToBase64String($currentBytes)) 'Rejected publisher changed current.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $DemoRoot "releases\character-lab\$NewId"))) 'Rejected publisher left an unreachable release.'
    Assert-NoPartial $DemoRoot
}

function Add-ManifestLine([string]$DemoRoot,[string]$ReleaseId,[string]$Line) {
    $manifest=Join-Path $DemoRoot "releases\character-lab\$ReleaseId\release.manifest";$lines=[Collections.Generic.List[string]]::new();$lines.AddRange([string[]][IO.File]::ReadAllLines($manifest));$lines.Add($Line);[IO.File]::WriteAllLines($manifest,$lines,[Text.UTF8Encoding]::new($false));Write-Current $DemoRoot $ReleaseId
}

function Assert-FailPointSyntax {
    $tokens=@('BeforeRename','BeforeCurrentCommit','HoldKnownLease','HoldCurrentCommit')
    foreach($token in $tokens){
        $normalized=switch -Exact ([string]$token){
            'BeforeRename'{'BeforeRename'}
            'BeforeCurrentCommit'{'BeforeCurrentCommit'}
            'HoldKnownLease'{'HoldKnownLease'}
            'HoldCurrentCommit'{'HoldCurrentCommit'}
            default{throw "Unexpected FailPoint token: $token"}
        }
        $conditions=@([bool]($normalized -eq 'BeforeRename'),[bool]($normalized -eq 'BeforeCurrentCommit'),[bool]($normalized -eq 'HoldKnownLease'),[bool]($normalized -eq 'HoldCurrentCommit'))
        Assert-True ($normalized -is [string] -and @($conditions|Where-Object{$_}).Count -eq 1) "FailPoint condition self-check failed for $token."
    }
    $unsafe='[^\s]-(eq|ne|lt|le|gt|ge|like|notlike|match|notmatch|contains|notcontains|in|notin|and|or|xor|is|isnot|as|not)\b'
    foreach($script in @($publisher,$PSCommandPath)){
        $parseTokens=$null
        $parseErrors=$null
        [System.Management.Automation.Language.Parser]::ParseFile($script,[ref]$parseTokens,[ref]$parseErrors)|Out-Null
        $parseDetail=($parseErrors|ForEach-Object{"$($_.Extent.StartLineNumber):$($_.Extent.StartColumnNumber) $($_.Message)"}) -join '; '
        Assert-True ($parseErrors.Count -eq 0) "PowerShell parser rejected ${script}: $parseDetail"
        $match=[regex]::Match([IO.File]::ReadAllText($script),$unsafe)
        Assert-True ( -not $match.Success) "Unsafe PowerShell operator spacing remains in $script at index $($match.Index)."
    }
}

function Assert-CompleteTestProcessContract {
    foreach($expectedExitCode in @(0,7)){
        $contractProcess=Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList @('-NoProfile','-Command',"exit $expectedExitCode") -PassThru
        try{
            $contractResult=@(Complete-TestProcess $contractProcess 5000 "Helper contract exit $expectedExitCode")
            $contractProcess=$null
            $actualTypes=@($contractResult|ForEach-Object{$_.GetType().FullName}) -join ','
            Assert-True ($contractResult.Count -eq 1 -and $contractResult[0] -is [int] -and $contractResult[0] -eq $expectedExitCode) "Complete-TestProcess contract failed: expected=$expectedExitCode count=$($contractResult.Count) types=$actualTypes actual=$($contractResult -join ',')"
        } finally { Close-TestProcess $contractProcess }
    }
}

Assert-FailPointSyntax
Assert-CompleteTestProcessContract
Assert-True (Test-Path -LiteralPath $launcher -PathType Leaf) 'Character launcher build is missing.'
Assert-True (Test-Path -LiteralPath $runtime -PathType Leaf) 'Character runtime build is missing.'
New-Item -ItemType Directory -Path $tempBase | Out-Null
try {
    $knownRoot = Join-Path $tempBase 'k'
    $known = Initialize-OldPinnedState $knownRoot
    $knownLatestHash=(Get-FileHash -LiteralPath (Join-Path $knownRoot 'Aevrynd-Character-Lab.exe') -Algorithm SHA256).Hash
    $legacyBefore = @(Get-ChildItem -LiteralPath $knownRoot -File -Filter 'Aevrynd-Character-Lab-Legacy-*.exe').Count
    & $publisher -ProjectRoot $ProjectRoot -DemoRoot $knownRoot -SkipValidation
    Assert-True ((Read-CurrentId $knownRoot) -eq $known.NewId) 'Known launcher upgrade did not commit current.'
    Assert-True ((Get-FileHash (Join-Path $knownRoot 'Aevrynd-Character-Lab.exe')).Hash -eq $knownLatestHash) 'Known launcher publish replaced the stable latest.'
    Assert-True (@(Get-ChildItem -LiteralPath $knownRoot -File -Filter 'Aevrynd-Character-Lab-Legacy-*.exe').Count -eq $legacyBefore) 'Known launcher upgrade created a legacy backup.'
    Assert-NoPartial $knownRoot

    $unknownRoot = Join-Path $tempBase 'u'
    $unknown = Initialize-OldPinnedState $unknownRoot
    $unknownLatest = Join-Path $unknownRoot 'Aevrynd-Character-Lab.exe'
    Add-ExecutableOverlay $unknownLatest 0x5A
    $unknownHash = (Get-FileHash $unknownLatest).Hash
    $unknownCurrent = [IO.File]::ReadAllBytes((Join-Path $unknownRoot 'current\character-lab.current'))
    Invoke-ExpectedFailure $unknownRoot
    Assert-True ((Get-FileHash $unknownLatest).Hash -eq $unknownHash) 'Unknown latest executable changed after rejection.'
    Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $unknownRoot 'current\character-lab.current'))) -eq [Convert]::ToBase64String($unknownCurrent)) 'Unknown latest rejection changed current.'
    Assert-True ( -not (Test-Path -LiteralPath (Join-Path $unknownRoot "releases\character-lab\$($unknown.NewId)"))) 'Unknown latest rejection left an unreachable release.'
    Assert-NoPartial $unknownRoot

    $wrongIdRoot=Join-Path $tempBase 'i';$wrongIdState=Initialize-OldPinnedState $wrongIdRoot;$wrongId='v0.0.0-engine-v0.0.0-00000002';$oldRelease=Join-Path $wrongIdRoot "releases\character-lab\$($wrongIdState.OldId)";$wrongRelease=Join-Path $wrongIdRoot "releases\character-lab\$wrongId";Copy-Item -LiteralPath $oldRelease -Destination $wrongRelease -Recurse;$wrongManifest=Join-Path $wrongRelease 'release.manifest';$wrongLines=[IO.File]::ReadAllLines($wrongManifest)|ForEach-Object{if($_ -eq "releaseId=$($wrongIdState.OldId)"){"releaseId=$wrongId"}else{$_}};[IO.File]::WriteAllLines($wrongManifest,$wrongLines,[Text.UTF8Encoding]::new($false));$wrongPinned=Join-Path $wrongIdRoot "Aevrynd-Character-Lab-$wrongId.exe";Copy-Item -LiteralPath $launcher -Destination $wrongPinned;Add-ExecutableOverlay $wrongPinned 0xB6;Write-Current $wrongIdRoot $wrongId;Assert-RejectedUnchanged $wrongIdRoot $wrongIdState.NewId

    $wrongHashRoot=Join-Path $tempBase 'h';$wrongHashState=Initialize-OldPinnedState $wrongHashRoot;[IO.File]::WriteAllLines((Join-Path $wrongHashRoot 'current\character-lab.current'),@('schema=1',"releaseId=$($wrongHashState.OldId)",('manifestSha256='+'0'*64)),[Text.UTF8Encoding]::new($false));Assert-RejectedUnchanged $wrongHashRoot $wrongHashState.NewId

    $duplicateRoot=Join-Path $tempBase 'd';$duplicateState=Initialize-OldPinnedState $duplicateRoot;$duplicateManifest=Join-Path $duplicateRoot "releases\character-lab\$($duplicateState.OldId)\release.manifest";$runtimeLine=@([IO.File]::ReadAllLines($duplicateManifest)|Where-Object{$_ -like 'file=Aevrynd-Character-Lab.runtime.exe *'})[0];Add-ManifestLine $duplicateRoot $duplicateState.OldId $runtimeLine;Assert-RejectedUnchanged $duplicateRoot $duplicateState.NewId

    $extraRoot=Join-Path $tempBase 'e';$extraState=Initialize-OldPinnedState $extraRoot;[IO.File]::WriteAllBytes((Join-Path $extraRoot "releases\character-lab\$($extraState.OldId)\extra.bin"),[byte[]](1,2,3));Assert-RejectedUnchanged $extraRoot $extraState.NewId

    $unknownLineRoot=Join-Path $tempBase 'n';$unknownLineState=Initialize-OldPinnedState $unknownLineRoot;Add-ManifestLine $unknownLineRoot $unknownLineState.OldId 'unknown=value';Assert-RejectedUnchanged $unknownLineRoot $unknownLineState.NewId

    $dirLinkRoot=Join-Path $tempBase 'j';$dirLinkState=Initialize-OldPinnedState $dirLinkRoot;$dirLinkRelease=Join-Path $dirLinkRoot "releases\character-lab\$($dirLinkState.OldId)";$dirTarget=Join-Path $dirLinkRoot 'outside-release';Copy-Item -LiteralPath $dirLinkRelease -Destination $dirTarget -Recurse;Remove-Item -LiteralPath $dirLinkRelease -Recurse -Force;New-Item -ItemType Junction -Path $dirLinkRelease -Target $dirTarget|Out-Null;Invoke-JunctionRejection $dirLinkRoot $dirLinkState.NewId $dirLinkRelease 'j' $dirLinkState.OldId

    $manifestLinkRoot=Join-Path $tempBase 'm';$manifestLinkState=Initialize-OldPinnedState $manifestLinkRoot;$manifestLink=Join-Path $manifestLinkRoot "releases\character-lab\$($manifestLinkState.OldId)\release.manifest";$manifestTarget=Join-Path $manifestLinkRoot 'manifest-target';New-Item -ItemType Directory -Path $manifestTarget|Out-Null;Remove-Item -LiteralPath $manifestLink -Force;New-Item -ItemType Junction -Path $manifestLink -Target $manifestTarget|Out-Null;Invoke-JunctionRejection $manifestLinkRoot $manifestLinkState.NewId $manifestLink 'm' 'release.manifest'

    $fileLinkRoot=Join-Path $tempBase 'f';$fileLinkState=Initialize-OldPinnedState $fileLinkRoot;$fileLink=Join-Path $fileLinkRoot "releases\character-lab\$($fileLinkState.OldId)\Aevrynd-Character-Lab.runtime.exe";$fileTarget=Join-Path $fileLinkRoot 'runtime-target';New-Item -ItemType Directory -Path $fileTarget|Out-Null;Remove-Item -LiteralPath $fileLink -Force;New-Item -ItemType Junction -Path $fileLink -Target $fileTarget|Out-Null;Invoke-JunctionRejection $fileLinkRoot $fileLinkState.NewId $fileLink 'f' 'Aevrynd-Character-Lab.runtime.exe'

    $legacyRoot = Join-Path $tempBase 'g'
    New-Item -ItemType Directory -Path $legacyRoot | Out-Null
    Copy-Item -LiteralPath $runtime -Destination (Join-Path $legacyRoot 'Aevrynd-Character-Lab.exe')
    $runtimeHash = (Get-FileHash $runtime).Hash
    & $publisher -ProjectRoot $ProjectRoot -DemoRoot $legacyRoot -SkipValidation
    $legacyFiles = @(Get-ChildItem -LiteralPath $legacyRoot -File -Filter 'Aevrynd-Character-Lab-Legacy-*.exe')
    Assert-True ($legacyFiles.Count -eq 1 -and (Get-FileHash $legacyFiles[0].FullName).Hash -eq $runtimeHash) 'Legacy runtime migration backup failed.'
    Assert-True ((Get-FileHash (Join-Path $legacyRoot 'Aevrynd-Character-Lab.exe')).Hash -eq (Get-FileHash $launcher).Hash) 'Legacy runtime migration did not install the launcher.'
    Assert-NoPartial $legacyRoot

    $raceRoot=Join-Path $tempBase 'r';$raceState=Initialize-OldPinnedState $raceRoot;Assert-FreshKnownState $raceRoot $raceState;$raceOut=Join-Path $tempBase 'race.out';$raceErr=Join-Path $tempBase 'race.err';$raceExpectedPublisherSha=(Get-FileHash -LiteralPath $publisher -Algorithm SHA256).Hash.ToLowerInvariant();$raceProcess=Start-PublisherChild $raceRoot 'HoldKnownLease' $raceOut $raceErr
    try{Wait-PublisherHandshake $raceProcess $raceRoot 'KnownLeaseReady' $raceExpectedPublisherSha $raceOut $raceErr;$raceRelease=Join-Path $raceRoot "releases\character-lab\$($raceState.OldId)";$raceSwap=Join-Path $raceRoot 'release-swap';$moveBlocked=$false;try{Move-Item -LiteralPath $raceRelease -Destination $raceSwap -ErrorAction Stop}catch{$moveBlocked=$true};$writeBlocked=$false;try{$write=[IO.File]::Open((Join-Path $raceRelease 'Aevrynd-Character-Lab.runtime.exe'),[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::None);$write.Dispose()}catch{$writeBlocked=$true};Assert-True ($moveBlocked -and $writeBlocked) 'Known release lease allowed a directory swap or file write.';Send-PublisherAck $raceRoot 'KnownLeaseReady';$raceResult=@(Complete-TestProcess $raceProcess 10000 'Swap-race publisher');$raceProcess=$null;$raceTypes=@($raceResult|ForEach-Object{$_.GetType().FullName}) -join ',';Assert-True ($raceResult.Count -eq 1 -and $raceResult[0] -is [int]) "Swap-race helper contract failed: count=$($raceResult.Count) types=$raceTypes actual=$($raceResult -join ',')";$raceExit=[int]$raceResult[0];Assert-True ($raceExit -eq 0) ("Swap-race publisher failed; expectedScriptSha=$raceExpectedPublisherSha out="+([IO.File]::ReadAllText($raceOut))+' err='+([IO.File]::ReadAllText($raceErr)));Assert-True ((Read-CurrentId $raceRoot) -eq $raceState.NewId) 'Swap-race publisher did not commit current.';Assert-True ((Get-FileHash (Join-Path $raceRoot 'Aevrynd-Character-Lab.exe')).Hash -eq (Get-FileHash $raceState.OldPinned).Hash) 'Swap-race publisher replaced the stable latest.';Assert-NoPartial $raceRoot}finally{Close-TestProcess $raceProcess}

    $currentRaceRoot=Join-Path $tempBase 'c';$currentRaceState=Initialize-OldPinnedState $currentRaceRoot;Assert-FreshKnownState $currentRaceRoot $currentRaceState;$currentRaceOut=Join-Path $tempBase 'current-race.out';$currentRaceErr=Join-Path $tempBase 'current-race.err';$currentExpectedPublisherSha=(Get-FileHash -LiteralPath $publisher -Algorithm SHA256).Hash.ToLowerInvariant();$currentProcess=Start-PublisherChild $currentRaceRoot 'HoldCurrentCommit' $currentRaceOut $currentRaceErr
    try{Wait-PublisherHandshake $currentProcess $currentRaceRoot 'CurrentCommitReady' $currentExpectedPublisherSha $currentRaceOut $currentRaceErr;$attacker=Join-Path $currentRaceRoot 'current\attacker.pending';[IO.File]::WriteAllLines($attacker,@('schema=1','releaseId=v9.9.9-engine-v9.9.9-deadbeef',('manifestSha256='+'f'*64)),[Text.UTF8Encoding]::new($false));Assert-True ([Aevrynd.ReleaseNative]::MoveFileEx($attacker,(Join-Path $currentRaceRoot 'current\character-lab.current'),0x9)) 'Current-race attacker could not exercise the close window.';Send-PublisherAck $currentRaceRoot 'CurrentCommitReady';$currentResult=@(Complete-TestProcess $currentProcess 10000 'Current-race publisher');$currentProcess=$null;$currentTypes=@($currentResult|ForEach-Object{$_.GetType().FullName}) -join ',';Assert-True ($currentResult.Count -eq 1 -and $currentResult[0] -is [int]) "Current-race helper contract failed: count=$($currentResult.Count) types=$currentTypes actual=$($currentResult -join ',')";$currentExit=[int]$currentResult[0];Assert-True ($currentExit -eq 0) ("Current-race publisher failed; expectedScriptSha=$currentExpectedPublisherSha out="+([IO.File]::ReadAllText($currentRaceOut))+' err='+([IO.File]::ReadAllText($currentRaceErr)));Assert-True ((Read-CurrentId $currentRaceRoot) -eq $currentRaceState.NewId) 'Current-race final pointer is not the expected release.';Assert-NoPartial $currentRaceRoot}finally{Close-TestProcess $currentProcess}

    $lockedRoot = Join-Path $tempBase 'l'
    $locked = Initialize-OldPinnedState $lockedRoot
    Assert-FreshKnownState $lockedRoot $locked
    $lockedLatest = Join-Path $lockedRoot 'Aevrynd-Character-Lab.exe'
    $lockedHash = (Get-FileHash $lockedLatest).Hash
    $lock = [IO.File]::Open($lockedLatest,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try { & $publisher -ProjectRoot $ProjectRoot -DemoRoot $lockedRoot -SkipValidation } finally { $lock.Dispose() }
    Assert-True ((Get-FileHash $lockedLatest).Hash -eq $lockedHash) 'Locked latest executable changed.'
    Assert-True ((Read-CurrentId $lockedRoot) -eq $locked.NewId) 'Locked legitimate launcher did not allow current commit.'
    Assert-NoPartial $lockedRoot
    Write-Output 'CHARACTER_PUBLISHER_TEST=PASS known=1 unknown=1 legacy=1 locked=1 trust=2 manifest=3 reparse=3 race=2'
} finally {
    Remove-TestTempTree
}
