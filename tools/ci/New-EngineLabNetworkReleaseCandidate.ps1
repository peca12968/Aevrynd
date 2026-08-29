[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string]$SandboxRoot,
    [Parameter(Mandatory)][string]$CandidateRoot,
    [ValidateSet('', 'RunnerSelfTest', 'VerifierSelfTest', 'DirtySource', 'StaleSource', 'Configure', 'Build', 'CTest', 'Gate', 'Capture', 'MissingExe', 'ExtraExe', 'MissingPeTool', 'MissingDependency', 'ReceiptMismatch', 'TempReparse')]
    [string]$TestFailPoint = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$entryName = 'Aevrynd-Engine-Lab.exe'
$networkNames = @(
    'AevryndReplicationProtocol3DTest.exe',
    'AevryndReplicationProtocol3DFuzzTest.exe',
    'AevryndReplicationProtocol3DBenchmark.exe',
    'AevryndReplicationRuntime3DTest.exe',
    'AevryndReplicationRuntime3DBenchmark.exe',
    'AevryndReplicationWorldAdapter3DTest.exe',
    'AevryndReplicationWorldAdapter3DBenchmark.exe'
)
$captureNames = @('first-person.png','third-person.png','god-view.png','isometric-2-5d.png','network-replication-overview.png','network-interest-durability.png')
# Optional graphics plugins are never fixed release inputs; the PE closure below is authoritative.
$runtimeNames = @()
$script:authorizedToolHashes=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
$script:vswhereTool=$null
$script:buildToolBinding=$null
function Get-Full([string]$Path) { [IO.Path]::GetFullPath($Path).TrimEnd('\') }
function Get-Sha([string]$Path) {
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try{$sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}}finally{$stream.Dispose()}
}
function Assert-ExactProperties([object]$Value,[string[]]$Names,[string]$Label) {
    if($null -eq $Value){throw "$Label schema rejected."};$expected=[Collections.Generic.HashSet[string]]::new($Names,[StringComparer]::Ordinal);$actual=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($property in $Value.PSObject.Properties){[void]$actual.Add($property.Name)};if( -not $expected.SetEquals($actual)){throw "$Label schema rejected."}
}
function Assert-NoDuplicateJsonKeys([string]$Json,[string]$Label) {
    $document=[Text.Json.JsonDocument]::Parse($Json)
    try {
        $walk=$null
        $walk={param([Text.Json.JsonElement]$Element,[string]$Path)
            if($Element.ValueKind -eq [Text.Json.JsonValueKind]::Object){$names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($property in $Element.EnumerateObject()){if( -not $names.Add($property.Name)){throw "$Label duplicate JSON key rejected at $Path/$($property.Name)"};& $walk $property.Value ($Path+'/'+$property.Name)}}
            elseif($Element.ValueKind -eq [Text.Json.JsonValueKind]::Array){$index=0;foreach($item in $Element.EnumerateArray()){& $walk $item ($Path+'/'+$index);$index++}}
        }
        & $walk $document.RootElement '$'
    } finally {$document.Dispose()}
}
function Assert-JsonPrimitiveTypes([object]$Value,[string[]]$IntegerFields,[string[]]$NumberFields,[string]$Label) {
    foreach($name in $IntegerFields){$item=$Value.PSObject.Properties[$name].Value;if($item -isnot [int] -and $item -isnot [long]){throw "$Label integer type rejected: $name"}}
    foreach($name in $NumberFields){$item=$Value.PSObject.Properties[$name].Value;if($item -isnot [double] -and $item -isnot [decimal] -and $item -isnot [int] -and $item -isnot [long]){throw "$Label number type rejected: $name"}}
}
function Get-TrustedToolRecord([string]$Path,[string]$Label,[string]$ExpectedSignerPattern) {
    $full=Get-Full $Path;$allowedRoots=@((Get-Full $env:SystemRoot),(Get-Full $env:ProgramFiles),(Get-Full ${env:ProgramFiles(x86)}))
    if( -not ($allowedRoots|Where-Object{$full.StartsWith($_+'\',[StringComparison]::OrdinalIgnoreCase)})){throw "$Label tool path is outside the allowlist: $full"}
    $signature=Get-AuthenticodeSignature -LiteralPath $full;if($signature.Status -ne 'Valid' -or $null -eq $signature.SignerCertificate -or $signature.SignerCertificate.Subject -notmatch $ExpectedSignerPattern){throw "$Label tool signature rejected: $full"}
    $hash=Get-Sha $full;$script:authorizedToolHashes[$full]=$hash
    return [ordered]@{label=$Label;path=$full;version=(Get-Item -LiteralPath $full).VersionInfo.FileVersion;sha256=$hash;signatureStatus=[string]$signature.Status;signer=$signature.SignerCertificate.Subject}
}
function Invoke-TrustedGit([string]$Git,[string[]]$Arguments,[string]$Label) {
    $expected=$script:authorizedToolHashes[(Get-Full $Git)];if( -not $expected){throw 'Git has no authorization hash.'};$before=Get-Sha $Git;if($before -ne $expected){throw "Git authorization hash changed before $Label"};$started=[DateTimeOffset]::UtcNow;$output=@(& $Git @Arguments 2>&1);$exitCode=$LASTEXITCODE;$after=Get-Sha $Git
    if($before -ne $after -or $after -ne $expected){throw "Git executable changed during $Label"}
    if($null -ne $script:gitCommandRecords){$script:gitCommandRecords.Add([ordered]@{label=$Label;arguments=@($Arguments);startedUtc=$started.ToString('o');endedUtc=[DateTimeOffset]::UtcNow.ToString('o');exitCode=$exitCode;fileSha256=$before;fileSha256After=$after})}
    if($exitCode -ne 0){throw "Git command failed: $Label exit=$exitCode output=$($output -join "`n")"}
    return @($output|ForEach-Object{[string]$_})
}
function Test-Reparse([string]$Path) {
    return ((Get-Item -LiteralPath $Path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
}
function Assert-NoReparseTree([string]$Root) {
    $stack=[Collections.Generic.Stack[string]]::new();$stack.Push((Get-Full $Root))
    while($stack.Count){$directory=$stack.Pop();if(Test-Reparse $directory){throw "Tree reparse rejected: $directory"};foreach($item in Get-ChildItem -LiteralPath $directory -Force){if(($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw "Tree reparse rejected: $($item.FullName)"};if($item.PSIsContainer){$stack.Push($item.FullName)}}}
}
function Assert-SafeRoot([string]$Root) {
    if( -not (Test-Path -LiteralPath $Root -PathType Container)){throw "Sandbox root is missing: $Root"}
    $cursor=Get-Full $Root
    $sandboxLeaf=[IO.Path]::GetFileName($cursor);$testSandbox=$TestFailPoint -ne '' -and $sandboxLeaf -match '^AevryndCandidateTest-[0-9a-f]{32}$';if(($sandboxLeaf -cne 'AevryndRC' -and -not $testSandbox) -or $cursor.Length -gt 180 -or [IO.Path]::GetPathRoot($cursor) -eq $cursor){throw 'Sandbox root must be the dedicated AevryndRC directory.'}
    while($cursor -and [IO.Path]::GetPathRoot($cursor) -ne $cursor){if(Test-Reparse $cursor){throw "Reparse component rejected: $cursor"};$cursor=[IO.Path]::GetDirectoryName($cursor)}
}
function Assert-DirectNewChild([string]$Root,[string]$Child) {
    $rootFull=Get-Full $Root;$childFull=Get-Full $Child
    if([IO.Path]::GetDirectoryName($childFull) -ne $rootFull){throw 'CandidateRoot must be a direct sandbox child.'}
    $leaf=[IO.Path]::GetFileName($childFull);$selfTestLeaf=$TestFailPoint -in @('RunnerSelfTest','VerifierSelfTest') -and $leaf -ceq ('self-'+$TestFailPoint);$failTestLeaf=$TestFailPoint -ne '' -and $leaf -ceq ('candidate-'+$TestFailPoint);if(($leaf -notmatch '^engine-lab-v254-[a-z0-9][a-z0-9.-]{0,79}$' -and -not $selfTestLeaf -and -not $failTestLeaf) -or $childFull.Length -gt 180){throw 'CandidateRoot leaf, charset, or length is invalid.'}
    if(Test-Path -LiteralPath $childFull){throw 'CandidateRoot must not exist.'}
}
function Remove-OwnedTree([string]$Root,[string]$Path,[string]$ExpectedPrefix,[string]$ExpectedIdentity='',[string]$ExpectedSource='') {
    if( -not (Test-Path -LiteralPath $Path)){return}
    $rootFull=Get-Full $Root;$pathFull=Get-Full $Path
    if([IO.Path]::GetDirectoryName($pathFull) -ne $rootFull -or -not [IO.Path]::GetFileName($pathFull).StartsWith($ExpectedPrefix,[StringComparison]::Ordinal)){throw "Unsafe cleanup rejected: $pathFull"}
    if(Test-Reparse $pathFull){throw "Cleanup reparse rejected: $pathFull"}
    if($ExpectedPrefix -eq '.a-'){$marker=Join-Path $pathFull '.aevrynd-candidate-owner';if( -not $ExpectedIdentity -or -not $ExpectedSource -or [IO.Path]::GetFileName($pathFull) -cne ('.a-'+$ExpectedIdentity.Substring(0,8)) -or -not(Test-Path -LiteralPath $marker -PathType Leaf)){throw 'Owned temp identity is invalid.'};$expectedMarker="schema=1`nidentity=$ExpectedIdentity`nsource=$ExpectedSource`n";if([IO.File]::ReadAllText($marker) -cne $expectedMarker){throw 'Owned temp marker is invalid.'}}
    Assert-NoReparseTree $pathFull
    Remove-Item -LiteralPath $pathFull -Recurse -Force
}
function ConvertTo-WindowsArgument([string]$Value) {
    if($Value.Length -and $Value -notmatch '[\s"]'){return $Value}
    $builder=[Text.StringBuilder]::new();[void]$builder.Append('"');$slashes=0
    foreach($character in $Value.ToCharArray()){
        if($character -eq '\'){$slashes++;continue}
        if($character -eq '"'){[void]$builder.Append(('\' * ($slashes*2+1)));[void]$builder.Append('"');$slashes=0;continue}
        if($slashes){[void]$builder.Append(('\' * $slashes));$slashes=0};[void]$builder.Append($character)
    }
    if($slashes){[void]$builder.Append(('\' * ($slashes*2)))};[void]$builder.Append('"');return $builder.ToString()
}
function Invoke-Recorded {
    param([string]$Name,[string]$File,[string[]]$Arguments,[string]$WorkingDirectory,[string]$EvidenceDirectory,[int]$TimeoutSeconds=1800,[int[]]$AcceptedExitCodes=@(0),[bool]$EnforceSingleCompilerProcess=$false)
    $outPath=Join-Path $EvidenceDirectory ($Name+'.out.log');$errPath=Join-Path $EvidenceDirectory ($Name+'.err.log');$recordPath=Join-Path $EvidenceDirectory ($Name+'.command.json')
    $preFileSha=Get-Sha $File;$authorizedSha=$script:authorizedToolHashes[(Get-Full $File)];if($authorizedSha -and $preFileSha -ne $authorizedSha){throw "Authorized command hash changed before run: $Name"};$start=[DateTimeOffset]::UtcNow;$startInfo=[Diagnostics.ProcessStartInfo]::new();$startInfo.FileName=$File;$startInfo.WorkingDirectory=$WorkingDirectory;$quotedArguments=@($Arguments|ForEach-Object{ConvertTo-WindowsArgument ([string]$_)});$startInfo.Arguments=$quotedArguments -join ' ';$startInfo.UseShellExecute=$false;$startInfo.CreateNoWindow=$true;$startInfo.RedirectStandardOutput=$true;$startInfo.RedirectStandardError=$true;$startInfo.StandardOutputEncoding=[Text.UTF8Encoding]::new($false);$startInfo.StandardErrorEncoding=[Text.UTF8Encoding]::new($false)
    $singleCompilerFlags=@('/m:1','/nr:false','/p:UseMultiToolTask=true','/p:CL_MPCount=1','/p:EnforceProcessCountAcrossBuilds=true')
    if($EnforceSingleCompilerProcess){foreach($flag in $singleCompilerFlags){if($Arguments -cnotcontains $flag){throw "Single-compiler build flag missing: $flag"}};$disabledMultiToolFlag='/p:UseMultiToolTask='+'false';if(@($Arguments|Where-Object{$_ -ceq $disabledMultiToolFlag}).Count){throw 'Conflicting multi-tool build flag rejected.'}}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$startInfo;if( -not $process.Start()){throw "Process did not start: $Name"};$processId=$process.Id;$stdoutTask=$process.StandardOutput.ReadToEndAsync();$stderrTask=$process.StandardError.ReadToEndAsync();$deadline=[Diagnostics.Stopwatch]::StartNew()
    while( -not $process.WaitForExit(250)){if($deadline.Elapsed.TotalSeconds -ge $TimeoutSeconds){break}}
    if( -not $process.HasExited){try{$taskkillExpected=$script:authorizedToolHashes[(Get-Full $script:taskkill)];$taskkillBefore=Get-Sha $script:taskkill;if( -not $taskkillExpected -or $taskkillBefore -ne $taskkillExpected){throw 'taskkill authorization hash changed.'};& $script:taskkill /PID $processId /T /F|Out-Null;$taskkillAfter=Get-Sha $script:taskkill;if($taskkillAfter -ne $taskkillExpected){throw 'taskkill changed during timeout cleanup.'}}catch{};if( -not $process.WaitForExit(30000)){try{$process.Kill()}catch{};$process.WaitForExit()};$exitCode=-1}else{$process.WaitForExit();$exitCode=[int]$process.ExitCode}
    $stdout=$stdoutTask.GetAwaiter().GetResult();$stderr=$stderrTask.GetAwaiter().GetResult();[IO.File]::WriteAllText($outPath,$stdout,[Text.UTF8Encoding]::new($false));[IO.File]::WriteAllText($errPath,$stderr,[Text.UTF8Encoding]::new($false));$process.Dispose();$end=[DateTimeOffset]::UtcNow
    $postFileSha=Get-Sha $File;if($postFileSha -ne $preFileSha -or ($authorizedSha -and $postFileSha -ne $authorizedSha)){throw "Command executable changed during run: $Name"}
    $record=[ordered]@{schemaVersion=1;name=$Name;file=(Get-Full $File);fileSha256=$preFileSha;fileSha256After=$postFileSha;arguments=@($Arguments);workingDirectory=(Get-Full $WorkingDirectory);pid=$processId;startedUtc=$start.ToString('o');endedUtc=$end.ToString('o');exitCode=$exitCode;stdoutSha256=(Get-Sha $outPath);stderrSha256=(Get-Sha $errPath);gpuValidationEnvironment=[string]$env:AEVRYND_GPU_VALIDATION;singleCompilerProcessEnforced=$EnforceSingleCompilerProcess;configuredMaximumClProcessCount=if($EnforceSingleCompilerProcess){1}else{0};compilerLimitEvidence=if($EnforceSingleCompilerProcess){'MSBuildProperties'}else{'NotApplicable'};boundBuildToolPath=if($null -ne $script:buildToolBinding){$script:buildToolBinding.path}else{''};boundBuildToolVersion=if($null -ne $script:buildToolBinding){$script:buildToolBinding.version}else{''};boundBuildToolSha256=if($null -ne $script:buildToolBinding){$script:buildToolBinding.sha256}else{''}}
    $record|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $recordPath -Encoding UTF8
    if($AcceptedExitCodes -notcontains $exitCode){$diagnostics=@(($stdout+"`n"+$stderr)-split"`r?`n"|Where-Object{$_ -match '(?i)(\berror\b|fatal|MSB[0-9]{4}|LNK[0-9]{4})'}|Select-Object -First 80)-join"`n";$outTail=if($stdout.Length -gt 4000){$stdout.Substring($stdout.Length-4000)}else{$stdout};$errTail=if($stderr.Length -gt 4000){$stderr.Substring($stderr.Length-4000)}else{$stderr};throw "Command failed: $Name exit=$exitCode diagnostics=$diagnostics stdoutTail=$outTail stderrTail=$errTail"}
    return $record
}
function Find-Dumpbin {
    if($TestFailPoint -eq 'MissingPeTool'){return $null}
    $command=Get-Command dumpbin.exe -ErrorAction SilentlyContinue;if($command){return $command.Source}
    $vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if( -not (Test-Path -LiteralPath $vswhere)){return $null}
    $script:vswhereTool=Get-TrustedToolRecord $vswhere 'vswhere' 'Microsoft'
    $expected=$script:vswhereTool.sha256;$before=Get-Sha $vswhere;if($before -ne $expected){throw 'vswhere authorization hash changed before discovery.'};$install=@(& $vswhere -latest -products '*' -property installationPath)[0];$exitCode=$LASTEXITCODE;$after=Get-Sha $vswhere;if($exitCode -ne 0 -or $before -ne $after -or $after -ne $expected){throw 'vswhere trusted invocation failed.'}
    if( -not $install){return $null}
    return @(Get-ChildItem -LiteralPath (Join-Path $install 'VC\Tools\MSVC') -Directory|Sort-Object Name -Descending|ForEach-Object{Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe'}|Where-Object{Test-Path -LiteralPath $_})[0]
}
function Get-PeImports([string]$Dumpbin,[string]$Binary,[string]$EvidenceDirectory) {
    $safeName=([IO.Path]::GetFileName($Binary)-replace'[^A-Za-z0-9_.-]','_')+'.imports'
    $record=Invoke-Recorded $safeName $Dumpbin @('/nologo','/dependents',$Binary) ([IO.Path]::GetDirectoryName($Binary)) $EvidenceDirectory 60
    $text=Get-Content -LiteralPath (Join-Path $EvidenceDirectory ($safeName+'.out.log')) -Raw
    return @([regex]::Matches($text,'(?im)^\s+([A-Za-z0-9_.-]+\.dll)\s*$')|ForEach-Object{$_.Groups[1].Value.ToLowerInvariant()}|Sort-Object -Unique)
}
function Copy-ExactTree([string]$Source,[string]$Destination) {
    New-Item -ItemType Directory -Path $Destination|Out-Null
    $seen=@{}
    $pending=[Collections.Generic.Stack[object]]::new();$pending.Push(@($Source,$Destination))
    while($pending.Count){$pair=$pending.Pop();$sourceDirectory=$pair[0];$destinationDirectory=$pair[1];if(Test-Reparse $sourceDirectory){throw "Asset reparse rejected: $sourceDirectory"}
        foreach($item in Get-ChildItem -LiteralPath $sourceDirectory -Force){if(($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw "Asset reparse rejected: $($item.FullName)"};$key=$item.Name.ToLowerInvariant();if($seen.ContainsKey((Get-Full $sourceDirectory)+'|'+$key)){throw 'Asset case collision.'};$seen[(Get-Full $sourceDirectory)+'|'+$key]=$true;$target=Join-Path $destinationDirectory $item.Name;if($item.PSIsContainer){New-Item -ItemType Directory -Path $target|Out-Null;$pending.Push(@($item.FullName,$target))}else{if($item.Name.Contains(':')){throw 'Asset ADS rejected.'};Copy-Item -LiteralPath $item.FullName -Destination $target}}
    }
}
function Get-FileRecords([string]$Root) {
    $rootFull=Get-Full $Root;$records=[Collections.Generic.List[object]]::new();$keys=@{}
    foreach($file in Get-ChildItem -LiteralPath $rootFull -File -Recurse){if(Test-Reparse $file.FullName){throw "Candidate reparse rejected: $($file.FullName)"};$relative=$file.FullName.Substring($rootFull.Length+1).Replace('\','/');$key=$relative.ToLowerInvariant();if($keys.ContainsKey($key)){throw "Candidate case collision: $relative"};$keys[$key]=$true;$records.Add([ordered]@{path=$relative;size=[uint64]$file.Length;sha256=(Get-Sha $file.FullName)})}
    return @($records|Sort-Object path)
}
function Assert-PngAndReceipt([string]$CaptureRoot,[string]$SourceCommit,[bool]$ExpectGpuValidation) {
    Add-Type -AssemblyName System.Drawing
    $hashes=@{}
    foreach($name in $captureNames){$path=Join-Path $CaptureRoot $name;if( -not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Capture missing: $name"};$bitmap=[Drawing.Bitmap]::FromFile($path);try{if($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720){throw "Capture dimensions invalid: $name"};$values=@();for($y=16;$y -lt 720;$y+=57){for($x=16;$x -lt 1280;$x+=53){$pixel=$bitmap.GetPixel($x,$y);$values+=($pixel.R+$pixel.G+$pixel.B)}};$measure=$values|Measure-Object -Minimum -Maximum;if($measure.Maximum-$measure.Minimum -lt 96){throw "Capture variance invalid: $name"}}finally{$bitmap.Dispose()};$hash=Get-Sha $path;if($hashes.ContainsKey($hash)){throw 'Duplicate capture hash.'};$hashes[$hash]=$name}
    foreach($receipt in @('network-runtime-receipt.json','network-capture-receipt.json')){if( -not(Test-Path -LiteralPath (Join-Path $CaptureRoot $receipt))){throw "Capture receipt missing: $receipt"}}
    $runtimeRaw=Get-Content -LiteralPath (Join-Path $CaptureRoot 'network-runtime-receipt.json') -Raw;Assert-NoDuplicateJsonKeys $runtimeRaw 'Runtime receipt'
    $runtimeReceipt=$runtimeRaw|ConvertFrom-Json
    $runtimeFields=@('runId','phase','seed','serverTick','clients','snapshots','full','delta','acks','loss','duplicate','reorder','client1Interest','client2Interest','visible1','visible2','interestEntered','interestExited','reentries','despawns','authorized','durable','denied','committed','aborted','restart','pending','queuePackets','packetCapacity','queueBytes','byteCapacity','auth1ChunkX','auth1OffsetX','pred1ChunkX','pred1OffsetX','auth2ChunkX','auth2OffsetX','pred2ChunkX','pred2OffsetX','gpuValidationRequested','gpuValidationAvailable','gpuValidationExecuted','gpuValidationPassed','debugLayerEnabled','gpuValidationErrors','gpuValidationWarnings','gpuValidationFirstErrorId','gpuValidationSource','gpuValidationEntry')
    Assert-ExactProperties $runtimeReceipt $runtimeFields 'Runtime receipt';$expectedGpu=if($ExpectGpuValidation){1}else{0};if($runtimeReceipt.phase -cne 'network-v254' -or [int]$runtimeReceipt.clients -ne 2 -or [int]$runtimeReceipt.serverTick -le 0 -or [int]$runtimeReceipt.gpuValidationRequested -ne $expectedGpu -or [int]$runtimeReceipt.gpuValidationAvailable -ne $expectedGpu -or [int]$runtimeReceipt.gpuValidationExecuted -ne $expectedGpu -or [int]$runtimeReceipt.gpuValidationPassed -ne $expectedGpu -or [int]$runtimeReceipt.debugLayerEnabled -ne $expectedGpu -or [int]$runtimeReceipt.gpuValidationErrors -ne 0 -or [int]$runtimeReceipt.gpuValidationWarnings -ne 0 -or [int]$runtimeReceipt.gpuValidationFirstErrorId -ne 0 -or $runtimeReceipt.gpuValidationSource -cne 'D3D12InfoQueue' -or $runtimeReceipt.gpuValidationEntry -cne 'EngineLabCapture'){throw 'Runtime receipt values rejected.'}
    Assert-JsonPrimitiveTypes $runtimeReceipt @('seed','serverTick','clients','snapshots','full','delta','acks','loss','duplicate','reorder','client1Interest','client2Interest','visible1','visible2','interestEntered','interestExited','reentries','despawns','authorized','durable','denied','committed','aborted','restart','pending','queuePackets','packetCapacity','queueBytes','byteCapacity','auth1ChunkX','pred1ChunkX','auth2ChunkX','pred2ChunkX','gpuValidationRequested','gpuValidationAvailable','gpuValidationExecuted','gpuValidationPassed','debugLayerEnabled','gpuValidationErrors','gpuValidationWarnings','gpuValidationFirstErrorId') @('auth1OffsetX','pred1OffsetX','auth2OffsetX','pred2OffsetX') 'Runtime receipt'
    $captureReceiptPath=Join-Path $CaptureRoot 'network-capture-receipt.json';$receiptRaw=Get-Content -LiteralPath $captureReceiptPath -Raw;Assert-NoDuplicateJsonKeys $receiptRaw 'Capture receipt'
    $captureReceipt=$receiptRaw|ConvertFrom-Json
    Assert-ExactProperties $captureReceipt @('runId','gpuValidationSource','gpuValidationEntry','captures','runtime','schema','seed') 'Capture receipt top-level'
    if(($captureReceipt.schema -isnot [int] -and $captureReceipt.schema -isnot [long]) -or $captureReceipt.schema -ne 1 -or ($captureReceipt.seed -isnot [int] -and $captureReceipt.seed -isnot [long]) -or @($captureReceipt.captures).Count -ne 2){throw 'Capture receipt phase count invalid.'}
    Assert-ExactProperties $captureReceipt.runtime $runtimeFields 'Capture runtime';foreach($field in $runtimeFields){if(($captureReceipt.runtime.PSObject.Properties[$field].Value|ConvertTo-Json -Compress) -cne ($runtimeReceipt.PSObject.Properties[$field].Value|ConvertTo-Json -Compress)){throw "Capture runtime differs: $field"}}
    if($runtimeReceipt.runId -isnot [string] -or $runtimeReceipt.runId -notmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'){throw 'Runtime receipt runId rejected.'}
    if($captureReceipt.runId -cne $runtimeReceipt.runId -or $captureReceipt.gpuValidationSource -cne 'D3D12InfoQueue' -or $captureReceipt.gpuValidationEntry -cne 'EngineLabCapture'){throw 'Capture receipt top-level identity mismatch.'}
    $expectedNetwork=@{'network-replication-overview.png'='replication-overview';'network-interest-durability.png'='interest-durability'}
    foreach($captureItem in $captureReceipt.captures){Assert-ExactProperties $captureItem @('file','height','phase','sha256','telemetry','variance','width') 'Capture item';if($captureItem.width -is [long]){$captureItem.width=[int]$captureItem.width};if($captureItem.height -is [long]){$captureItem.height=[int]$captureItem.height}}
    $seenNetwork=@{};foreach($item in $captureReceipt.captures){if(Compare-Object @('file','height','phase','sha256','telemetry','variance','width') @($item.PSObject.Properties.Name|Sort-Object)){throw 'Capture item schema rejected.'};if($item.file -isnot [string] -or $item.phase -isnot [string] -or $item.sha256 -isnot [string] -or $item.variance -cne 'pass' -or $item.width -isnot [int] -or $item.height -isnot [int] -or $item.width -ne 1280 -or $item.height -ne 720 -or $item.file -notmatch '^network-(replication-overview|interest-durability)\.png$' -or $seenNetwork.ContainsKey($item.file) -or $expectedNetwork[$item.file] -ne $item.phase -or $item.sha256 -notmatch '^[0-9a-f]{64}$'){throw 'Capture receipt reference rejected.'};$expectedTelemetry=if($item.phase -eq 'replication-overview'){@('serverTick','loss','delta','pred1ChunkX','auth2OffsetX','acks','auth2ChunkX','reorder','full','auth1OffsetX','pred2OffsetX','snapshots','auth1ChunkX','pred2ChunkX','duplicate','pred1OffsetX')}else{@('despawns','committed','packetCapacity','authorized','interestEntered','pending','aborted','durable','restart','interestExited','denied','queuePackets','visible1','reentries','visible2','byteCapacity','queueBytes')};Assert-ExactProperties $item.telemetry $expectedTelemetry 'Capture telemetry';foreach($field in $expectedTelemetry){if(($item.telemetry.PSObject.Properties[$field].Value|ConvertTo-Json -Compress) -cne ($runtimeReceipt.PSObject.Properties[$field].Value|ConvertTo-Json -Compress)){throw "Capture telemetry differs: $field"}};$seenNetwork[$item.file]=$true;$path=Get-Full (Join-Path $CaptureRoot $item.file);if([IO.Path]::GetDirectoryName($path) -ne (Get-Full $CaptureRoot)){throw 'Capture receipt path escaped.'};$actual=Get-Sha $path;if($actual -ne $item.sha256){throw "Capture receipt hash mismatch: $($item.file)"}}
    if($ExpectGpuValidation){$diagnosticsPath=Join-Path $CaptureRoot 'gpu-validation-diagnostics.txt';if( -not(Test-Path -LiteralPath $diagnosticsPath -PathType Leaf)){throw 'GPU validation diagnostics missing.'};$diagnosticLines=@(Get-Content -LiteralPath $diagnosticsPath);if($diagnosticLines.Count -lt 3 -or $diagnosticLines[0] -cne ('runId='+$runtimeReceipt.runId) -or $diagnosticLines[1] -cne 'source=D3D12InfoQueue' -or $diagnosticLines[2] -cne 'entry=EngineLabCapture'){throw 'GPU validation diagnostics identity mismatch.'}}
    return [string]$runtimeReceipt.runId
}
function Assert-CandidateManifest([string]$Root) {
    $manifestPath=Join-Path $Root 'candidate.manifest.json';$manifestRaw=Get-Content -LiteralPath $manifestPath -Raw;Assert-NoDuplicateJsonKeys $manifestRaw 'Candidate manifest';$manifest=($manifestRaw|ConvertFrom-Json)
    Assert-ExactProperties $manifest @('schemaVersion','releaseId','product','productVersion','engineVersion','commit','entry','featureStage','maturity','transportScope','derivedCaptures','files') 'Candidate manifest';$manifestDocument=[Text.Json.JsonDocument]::Parse($manifestRaw);try{$derivedKind=$manifestDocument.RootElement.GetProperty('derivedCaptures').ValueKind;$filesKind=$manifestDocument.RootElement.GetProperty('files').ValueKind}finally{$manifestDocument.Dispose()}
    if(($manifest.schemaVersion -isnot [int] -and $manifest.schemaVersion -isnot [long]) -or $manifest.schemaVersion -ne 1 -or $manifest.releaseId -isnot [string] -or $manifest.product -cne 'EngineLab' -or $manifest.productVersion -cne '2.54.0' -or $manifest.engineVersion -cne '2.86.0' -or $manifest.commit -notmatch '^[0-9a-f]{40}$' -or $manifest.entry -cne $entryName -or $manifest.featureStage -cne 'NetworkReplicationPhase1ValidatedWip' -or $manifest.maturity -cne 'validated-wip' -or $manifest.transportScope -cne 'deterministic-loopback' -or @($manifest.derivedCaptures).Count -ne 0){throw "Candidate manifest values rejected. schema=$($manifest.schemaVersion.GetType().FullName) product=$($manifest.product) version=$($manifest.productVersion) engine=$($manifest.engineVersion) commit=$($manifest.commit) entry=$($manifest.entry) stage=$($manifest.featureStage) maturity=$($manifest.maturity) transport=$($manifest.transportScope) derived=$(@($manifest.derivedCaptures).Count)"}
    if($manifest.releaseId -cne $releaseId -or $manifest.commit -cne $sourceCommit -or $derivedKind -ne [Text.Json.JsonValueKind]::Array -or $filesKind -ne [Text.Json.JsonValueKind]::Array){throw 'Candidate manifest identity or array kind rejected.'}
    $declared=@($manifest.files);$actual=@(Get-FileRecords $Root|Where-Object path -ne 'candidate.manifest.json')
    if($declared.Count -ne $actual.Count){throw 'Candidate manifest count mismatch.'}
    $declaredByPath=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase);foreach($record in $declared){Assert-ExactProperties $record @('path','size','sha256') 'Candidate manifest file';if($record.path -isnot [string] -or ($record.size -isnot [int] -and $record.size -isnot [long]) -or $record.size -lt 0 -or $record.sha256 -isnot [string] -or $record.path -notmatch '^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$' -or $record.sha256 -notmatch '^[0-9a-f]{64}$' -or $declaredByPath.ContainsKey([string]$record.path)){throw 'Candidate manifest path or duplicate rejected.'};$declaredByPath.Add([string]$record.path,$record)}
    foreach($actualRecord in $actual){$declaredRecord=$null;if( -not $declaredByPath.TryGetValue([string]$actualRecord.path,[ref]$declaredRecord) -or [string]$declaredRecord.path -cne [string]$actualRecord.path -or [uint64]$declaredRecord.size -ne $actualRecord.size -or [string]$declaredRecord.sha256 -cne [string]$actualRecord.sha256){throw "Candidate manifest mismatch actual=$($actualRecord.path),$($actualRecord.size),$($actualRecord.sha256) declared=$($declaredRecord.path),$($declaredRecord.size),$($declaredRecord.sha256)"};$stream=[IO.File]::Open((Join-Path $Root $actualRecord.path),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);try{if($stream.Length -ne $actualRecord.size){throw 'Candidate stream size changed.'};$sha=[Security.Cryptography.SHA256]::Create();try{$hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()};if($hash -ne $actualRecord.sha256){throw 'Candidate stream hash changed.'}}finally{$stream.Dispose()}}
}
function Test-ExactRuntimeSet([string[]]$Expected,[string[]]$Actual) {
    if($Expected.Count -ne $Actual.Count){return $false}
    if( -not $Expected.Count){return $true}
    return -not [bool](Compare-Object ($Expected|Sort-Object) ($Actual|Sort-Object))
}

$ProjectRoot=Get-Full $ProjectRoot;$SandboxRoot=Get-Full $SandboxRoot;$CandidateRoot=Get-Full $CandidateRoot;$builderScriptSha=Get-Sha $PSCommandPath
Assert-SafeRoot $SandboxRoot;Assert-DirectNewChild $SandboxRoot $CandidateRoot
$script:gitCommandRecords=[Collections.Generic.List[object]]::new();$git=(Get-Command git.exe -ErrorAction Stop).Source;$gitTool=Get-TrustedToolRecord $git 'git' 'Johannes Schindelin|Git for Windows';$powershell=(Get-Command powershell.exe -ErrorAction Stop).Source;$powershellTool=Get-TrustedToolRecord $powershell 'powershell' 'Microsoft';$script:taskkill=Join-Path $env:SystemRoot 'System32\taskkill.exe';$taskkillTool=Get-TrustedToolRecord $script:taskkill 'taskkill' 'Microsoft'
[void](Invoke-TrustedGit $git @('-C',$ProjectRoot,'fetch','--quiet','origin','main') 'source-fetch')
$sourceCommit=((Invoke-TrustedGit $git @('-C',$ProjectRoot,'rev-parse','HEAD') 'source-head') -join '').Trim().ToLowerInvariant();if($sourceCommit -notmatch '^[0-9a-f]{40}$'){throw 'Source HEAD unavailable.'}
$originCommit=((Invoke-TrustedGit $git @('-C',$ProjectRoot,'rev-parse','origin/main') 'source-origin') -join '').Trim().ToLowerInvariant();if($originCommit -ne $sourceCommit){throw 'Source pin is stale.'}
if((Invoke-TrustedGit $git @('-C',$ProjectRoot,'status','--porcelain','--untracked-files=no') 'source-status') -or $TestFailPoint -eq 'DirtySource'){throw 'Tracked source is dirty.'}
if($TestFailPoint -eq 'StaleSource'){throw 'Injected stale source.'}
$releaseId='v2.54.0-engine-v2.86.0-'+$sourceCommit.Substring(0,8)
$tempRoot=$null;$worktree=$null;$build=$null;$assembly=$null;$evidence=$null
$worktreeAdded=$false;$candidateCommitted=$false;$primaryFailure=$false;$readyMessage=$null
try {
    $tempIdentity=$null
    for($attempt=0;$attempt -lt 8 -and -not $tempRoot;$attempt++){$attemptIdentity=[Guid]::NewGuid().ToString('N');$candidateTemp=Join-Path $SandboxRoot ('.a-'+$attemptIdentity.Substring(0,8));try{New-Item -ItemType Directory -Path $candidateTemp -ErrorAction Stop|Out-Null;$tempRoot=$candidateTemp;$tempIdentity=$attemptIdentity}catch{if(Test-Path -LiteralPath $candidateTemp){continue};throw}}
    if( -not $tempRoot){throw 'Publisher temp allocation collision limit reached.'}
    [IO.File]::WriteAllText((Join-Path $tempRoot '.aevrynd-candidate-owner'),("schema=1`nidentity=$tempIdentity`nsource=$sourceCommit`n"),[Text.UTF8Encoding]::new($false))
    $worktree=Join-Path $tempRoot 's';$build=Join-Path $tempRoot 'b';$assembly=Join-Path $tempRoot 'c';$evidence=Join-Path $assembly 'evidence'
    New-Item -ItemType Directory -Path $assembly,$evidence|Out-Null
    if($TestFailPoint -eq 'TempReparse'){throw 'Injected temp reparse rejection.'}
    if($TestFailPoint -eq 'RunnerSelfTest'){
        $fixture=Join-Path $ProjectRoot 'tools\ci\process-runner-fixture.ps1'
        $unicodeText='space value "quoted" '+[string]([char]0x975e)+'ASCII'
        Invoke-Recorded 'runner-argv' $powershell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$fixture,'-Text',$unicodeText,'-ErrorText','stderr value','-Bytes','1048576','-ExitCode','0') $ProjectRoot $evidence 30|Out-Null
        $record=Get-Content -LiteralPath (Join-Path $evidence 'runner-argv.command.json') -Raw|ConvertFrom-Json;$expectedArguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$fixture,'-Text',$unicodeText,'-ErrorText','stderr value','-Bytes','1048576','-ExitCode','0');$argumentMismatch=@(Compare-Object $expectedArguments @($record.arguments) -SyncWindow 0).Count;$runnerOut=Get-Content (Join-Path $evidence 'runner-argv.out.log') -Raw -Encoding UTF8;$runnerErr=Get-Content (Join-Path $evidence 'runner-argv.err.log') -Raw -Encoding UTF8;$textOk=$runnerOut.Contains($unicodeText);$sizeOk=(Get-Item (Join-Path $evidence 'runner-argv.out.log')).Length -ge 1048576;$stderrOk=$runnerErr.Contains('stderr value');if($record.exitCode -ne 0 -or $argumentMismatch -ne 0 -or -not $textOk -or -not $sizeOk -or -not $stderrOk){throw "Runner argv/output contract failed. exit=$($record.exitCode) argumentMismatch=$argumentMismatch textOk=$textOk sizeOk=$sizeOk stderrOk=$stderrOk outLength=$($runnerOut.Length)"}
        $nonzero=$false;try{Invoke-Recorded 'runner-nonzero' $powershell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$fixture,'-ExitCode','7') $ProjectRoot $evidence 30|Out-Null}catch{$nonzero=$_.Exception.Message -match 'exit=7'};if( -not $nonzero){throw 'Runner nonzero contract failed.'}
        $timeout=$false;try{Invoke-Recorded 'runner-timeout' $powershell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$fixture,'-SleepSeconds','5') $ProjectRoot $evidence 1|Out-Null}catch{$timeout=$_.Exception.Message -match 'exit=-1'};if( -not $timeout){throw 'Runner timeout contract failed.'}
        Write-Host 'RUNNER_SELF_TEST_PASS';return
    }
    if($TestFailPoint -eq 'VerifierSelfTest'){
        [IO.File]::WriteAllText((Join-Path $assembly $entryName),'alpha');[IO.File]::WriteAllText((Join-Path $assembly 'Beta.txt'),'beta')
        $records=@(Get-FileRecords $assembly|Sort-Object path -Descending);[ordered]@{schemaVersion=1;releaseId=$releaseId;product='EngineLab';productVersion='2.54.0';engineVersion='2.86.0';commit=$sourceCommit;entry=$entryName;featureStage='NetworkReplicationPhase1ValidatedWip';maturity='validated-wip';transportScope='deterministic-loopback';derivedCaptures=@();files=$records}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $assembly 'candidate.manifest.json') -Encoding UTF8;Assert-CandidateManifest $assembly
        if( -not (Test-ExactRuntimeSet @() @()) -or (Test-ExactRuntimeSet @() @('extra.dll')) -or (Test-ExactRuntimeSet @('required.dll') @())){throw 'Runtime exact-set synthetic contract failed.'}
        $manifestPath=Join-Path $assembly 'candidate.manifest.json';$baseline=[IO.File]::ReadAllText($manifestPath)
        foreach($case in @('missing','extra','duplicate','case','size','hash','duplicateTop','unknownTop','sizeType')){[IO.File]::WriteAllText($manifestPath,$baseline);if($case -eq 'missing'){$json=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json;$json.files=@($json.files|Select-Object -Skip 1);$json|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding UTF8}elseif($case -eq 'extra'){[IO.File]::WriteAllText((Join-Path $assembly 'extra.txt'),'extra')}elseif($case -eq 'duplicate'){$json=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json;$json.files=@($json.files)+@($json.files[0]);$json|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding UTF8}elseif($case -eq 'case'){$json=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json;$json.files[0].path=$json.files[0].path.ToUpperInvariant();$json|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding UTF8}elseif($case -eq 'size'){$json=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json;$json.files[0].size=[uint64]$json.files[0].size+1;$json|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding UTF8}elseif($case -eq 'hash'){$json=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json;$json.files[0].sha256='0'*64;$json|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding UTF8}elseif($case -eq 'duplicateTop'){$raw=[IO.File]::ReadAllText($manifestPath);[IO.File]::WriteAllText($manifestPath,$raw.Insert($raw.IndexOf('{')+1,'"schemaVersion":1,'))}elseif($case -eq 'unknownTop'){$json=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json;$json|Add-Member unexpected 1;$json|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding UTF8}else{$json=[IO.File]::ReadAllText($manifestPath)|ConvertFrom-Json;$json.files[0].size=[string]$json.files[0].size;$json|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding UTF8};$rejected=$false;try{Assert-CandidateManifest $assembly}catch{$rejected=$true};if( -not $rejected){throw "Manifest hostile case accepted: $case"};if($case -eq 'extra'){Remove-Item -LiteralPath (Join-Path $assembly 'extra.txt') -Force}}
        $cleanupIdentity=[Guid]::NewGuid().ToString('N');$cleanupPath=Join-Path $SandboxRoot ('.a-'+$cleanupIdentity.Substring(0,8));New-Item -ItemType Directory -Path $cleanupPath|Out-Null;[IO.File]::WriteAllText((Join-Path $cleanupPath '.aevrynd-candidate-owner'),("schema=1`nidentity=$cleanupIdentity`nsource=$sourceCommit`n"),[Text.UTF8Encoding]::new($false));$ownerRejected=$false;try{Remove-OwnedTree $SandboxRoot $cleanupPath '.a-' ([Guid]::NewGuid().ToString('N')) $sourceCommit}catch{$ownerRejected=$true};if( -not $ownerRejected -or -not(Test-Path $cleanupPath)){throw 'Cleanup owner mismatch contract failed.'};$junction=Join-Path $cleanupPath 'junction';New-Item -ItemType Junction -Path $junction -Target $assembly|Out-Null;$junctionRejected=$false;try{Remove-OwnedTree $SandboxRoot $cleanupPath '.a-' $cleanupIdentity $sourceCommit}catch{$junctionRejected=$true};if( -not $junctionRejected -or -not(Test-Path $cleanupPath)){throw 'Cleanup junction contract failed.'};Remove-Item -LiteralPath $junction -Force;Remove-OwnedTree $SandboxRoot $cleanupPath '.a-' $cleanupIdentity $sourceCommit
        Write-Host 'VERIFIER_SELF_TEST_PASS cases=10 runtimeCases=3 cleanupCases=2';return
    }
    $longestRelative='engine\native\AevryndWorldPartitionTerrainVegetationDeltaComposeFakeTest.dir\Release\AevryndW.64D192E1.tlog\AevryndWorldPartitionTerrainVegetationDeltaComposeFakeTest.lastbuildstate'
    $maximumExpectedPath=(Join-Path $build $longestRelative).Length;if($maximumExpectedPath -ge 230){throw "Fresh build path safety margin failed: $maximumExpectedPath"}
    [void](Invoke-TrustedGit $git @('-C',$ProjectRoot,'worktree','add','--detach',$worktree,$sourceCommit) 'worktree-add');$worktreeAdded=$true
    $detached=((Invoke-TrustedGit $git @('-C',$worktree,'rev-parse','HEAD') 'detached-head') -join '').Trim().ToLowerInvariant();if($detached -ne $sourceCommit){throw 'Detached source pin mismatch.'}
    $cmake=(Get-Command cmake.exe -ErrorAction Stop).Source;$ctest=(Get-Command ctest.exe -ErrorAction Stop).Source
    $toolchain=@($gitTool,(Get-TrustedToolRecord $cmake 'cmake' 'Kitware'),(Get-TrustedToolRecord $ctest 'ctest' 'Kitware'),$powershellTool,$taskkillTool)
    $vswherePath='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe';if( -not(Test-Path -LiteralPath $vswherePath)){throw 'Trusted Visual Studio discovery tool is unavailable.'};$script:vswhereTool=Get-TrustedToolRecord $vswherePath 'vswhere' 'Microsoft';$toolchain+=@($script:vswhereTool);Invoke-Recorded 'vswhere-msbuild' $vswherePath @('-latest','-products','*','-property','installationPath') $worktree $evidence 60|Out-Null;$visualStudioRoot=((Get-Content -LiteralPath (Join-Path $evidence 'vswhere-msbuild.out.log') -Raw).Trim());if( -not $visualStudioRoot){throw 'Visual Studio installation path is empty.'};$msbuildPath=Join-Path $visualStudioRoot 'MSBuild\Current\Bin\MSBuild.exe';if( -not(Test-Path -LiteralPath $msbuildPath -PathType Leaf)){throw 'MSBuild executable is unavailable.'};$msbuildTool=Get-TrustedToolRecord $msbuildPath 'msbuild' 'Microsoft';$toolchain+=@($msbuildTool);$msbuildPreSha=$msbuildTool.sha256
    if($TestFailPoint -eq 'Configure'){throw 'Injected configure failure.'};Invoke-Recorded 'build-configure' $cmake @('-S',$worktree,'-B',$build,'-G','Visual Studio 17 2022','-A','x64',('-DCMAKE_GENERATOR_INSTANCE:INTERNAL='+$visualStudioRoot)) $worktree $evidence|Out-Null
    $cachePath=Join-Path $build 'CMakeCache.txt';if( -not(Test-Path -LiteralPath $cachePath -PathType Leaf)){throw 'CMake cache is missing after configure.'};$cacheText=[IO.File]::ReadAllText($cachePath);$instanceMatch=[regex]::Match($cacheText,'(?m)^CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)$');if( -not $instanceMatch.Success -or (Get-Full ($instanceMatch.Groups[1].Value.Trim().Replace('/','\'))) -cne (Get-Full $visualStudioRoot)){throw 'CMake generator instance does not match the authorized MSBuild instance.'}
    if($TestFailPoint -eq 'Build'){throw 'Injected build failure.'};$oldNodeReuse=$env:MSBUILDDISABLENODEREUSE;$script:buildToolBinding=$msbuildTool;try{$env:MSBUILDDISABLENODEREUSE='1';$safeMsBuild=@('/m:1','/nr:false','/p:UseMultiToolTask=true','/p:CL_MPCount=1','/p:EnforceProcessCountAcrossBuilds=true');Invoke-Recorded 'build-engine-clean-first' $cmake (@('--build',$build,'--config','Release','--target','AevryndEngine','--clean-first','--')+$safeMsBuild) $worktree $evidence 3600 @(0) $true|Out-Null;Invoke-Recorded 'build-all' $cmake (@('--build',$build,'--config','Release','--target','ALL_BUILD','--')+$safeMsBuild) $worktree $evidence 3600 @(0) $true|Out-Null}finally{$script:buildToolBinding=$null;$env:MSBUILDDISABLENODEREUSE=$oldNodeReuse}
    $msbuildPostSha=Get-Sha $msbuildPath;if($msbuildPostSha -ne $msbuildPreSha){throw 'MSBuild executable changed during build.'};$cleanBuildRecord=Get-Content -LiteralPath (Join-Path $evidence 'build-engine-clean-first.command.json') -Raw|ConvertFrom-Json;$allBuildRecord=Get-Content -LiteralPath (Join-Path $evidence 'build-all.command.json') -Raw|ConvertFrom-Json
    if( -not $cleanBuildRecord.singleCompilerProcessEnforced -or -not $allBuildRecord.singleCompilerProcessEnforced -or [int]$cleanBuildRecord.configuredMaximumClProcessCount -ne 1 -or [int]$allBuildRecord.configuredMaximumClProcessCount -ne 1 -or $cleanBuildRecord.compilerLimitEvidence -cne 'MSBuildProperties' -or $allBuildRecord.compilerLimitEvidence -cne 'MSBuildProperties' -or $cleanBuildRecord.boundBuildToolPath -cne (Get-Full $msbuildPath) -or $allBuildRecord.boundBuildToolPath -cne (Get-Full $msbuildPath) -or $cleanBuildRecord.boundBuildToolSha256 -cne $msbuildPreSha -or $allBuildRecord.boundBuildToolSha256 -cne $msbuildPreSha){throw 'Compiler process limit evidence rejected.'}
    [ordered]@{schemaVersion=1;msbuildFlags=$safeMsBuild;nodeReuseDisabled=$true;configuredMaximumClProcessCount=1;enforcementSource='MSBuildProperties';generatorInstance=(Get-Full $visualStudioRoot);cmakeCacheSha256=(Get-Sha $cachePath);msbuildPath=(Get-Full $msbuildPath);msbuildVersion=$msbuildTool.version;msbuildSha256=$msbuildPreSha;msbuildSha256After=$msbuildPostSha;cleanFirstCommandRecordSha256=(Get-Sha (Join-Path $evidence 'build-engine-clean-first.command.json'));allBuildCommandRecordSha256=(Get-Sha (Join-Path $evidence 'build-all.command.json'))}|ConvertTo-Json -Depth 3|Set-Content -LiteralPath (Join-Path $evidence 'build-process-policy.json') -Encoding UTF8
    Invoke-Recorded 'ctest-inventory' $ctest @('--test-dir',$build,'-C','Release','-N') $worktree $evidence|Out-Null
    if($TestFailPoint -eq 'CTest'){throw 'Injected CTest failure.'};Invoke-Recorded 'ctest-full' $ctest @('--test-dir',$build,'-C','Release','-j','1','--output-on-failure') $worktree $evidence 3600|Out-Null
    $inventoryLog=[string](Get-Content -LiteralPath (Join-Path $evidence 'ctest-inventory.out.log') -Raw);$fullLog=[string](Get-Content -LiteralPath (Join-Path $evidence 'ctest-full.out.log') -Raw)
    $inventoryMatch=[regex]::Match($inventoryLog,'Total Tests:\s*(\d+)');$passedMatch=[regex]::Match($fullLog,'100% tests passed(?:,\s*0 tests failed)?\s+out of\s+(\d+)')
    if( -not $inventoryMatch.Success -or [int]$inventoryMatch.Groups[1].Value -le 0 -or -not $passedMatch.Success -or $passedMatch.Groups[1].Value -ne $inventoryMatch.Groups[1].Value){throw "CTest inventory/full result mismatch. inventory=$inventoryLog full=$fullLog"}
    $bin=Join-Path $build 'bin\Release';foreach($name in @($entryName)+$networkNames){$path=Join-Path $bin $name;if( -not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Fresh binary missing: $name"}}
    if($TestFailPoint -eq 'MissingExe'){throw 'Injected missing executable.'};if($TestFailPoint -eq 'ExtraExe'){throw 'Injected extra executable.'}
    foreach($name in $networkNames){if($TestFailPoint -eq 'Gate'){throw 'Injected network gate failure.'};Invoke-Recorded $name (Join-Path $bin $name) @() $bin $evidence 600|Out-Null}
    foreach($name in @($entryName)+$networkNames){Copy-Item -LiteralPath (Join-Path $bin $name) -Destination (Join-Path $assembly $name)}
    $dumpbin=Find-Dumpbin;if( -not $dumpbin){throw 'Official PE dependency tool is unavailable.'}
    $dumpbinTool=Get-TrustedToolRecord $dumpbin 'dumpbin' 'Microsoft';$toolchain+=@($dumpbinTool);if(@($toolchain|Where-Object{$null -eq $_}).Count){throw 'Toolchain evidence contains null.'}
    Invoke-Recorded 'pe-tool-version' $dumpbin @('/?') ([IO.Path]::GetDirectoryName($dumpbin)) $evidence 60 @(1100)|Out-Null
    $dumpbinTool|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $evidence 'pe-tool.json') -Encoding UTF8
    $toolchain|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $evidence 'toolchain.json') -Encoding UTF8
    $queue=[Collections.Generic.Queue[string]]::new();foreach($name in @($entryName)+$networkNames){$queue.Enqueue((Join-Path $assembly $name))};$processed=@{};$nonSystem=@{};$externalSystem=[Collections.Generic.List[object]]::new();$externalSystemNames=@{}
    while($queue.Count){$binary=$queue.Dequeue();$binaryHash=Get-Sha $binary;if($processed.ContainsKey($binaryHash)){continue};$processed[$binaryHash]=$true;foreach($dependency in Get-PeImports $dumpbin $binary $evidence){
        $source=Join-Path $bin $dependency
        if(Test-Path -LiteralPath $source){if($TestFailPoint -eq 'MissingDependency'){throw "Injected reachable dependency failure: $dependency"};$nonSystem[$dependency]=$true;$destination=Join-Path $assembly $dependency;if( -not (Test-Path $destination)){Copy-Item -LiteralPath $source -Destination $destination};$queue.Enqueue($destination);continue}
        if($dependency -match '^(api|ext)-ms-win-[a-z0-9-]+\.dll$'){if( -not $externalSystemNames.ContainsKey($dependency)){$externalSystemNames[$dependency]=$true;$externalSystem.Add([ordered]@{name=$dependency;kind='windows-api-set'})};continue}
        $systemPath=Join-Path $env:SystemRoot ('System32\'+$dependency)
        if(Test-Path -LiteralPath $systemPath){$signature=Get-AuthenticodeSignature -LiteralPath $systemPath;if($signature.Status -ne 'Valid' -or $null -eq $signature.SignerCertificate -or $signature.SignerCertificate.Subject -notmatch 'Microsoft'){throw "System dependency signature rejected: $dependency"};if( -not $externalSystemNames.ContainsKey($dependency)){$externalSystemNames[$dependency]=$true;$externalSystem.Add([ordered]@{name=$dependency;kind='system32';path=(Get-Full $systemPath);sha256=(Get-Sha $systemPath);version=(Get-Item -LiteralPath $systemPath).VersionInfo.FileVersion;signer=$signature.SignerCertificate.Subject})};continue}
        throw "Reachable PE dependency is unresolved: $dependency"
    }}
    if( -not (Test-ExactRuntimeSet $runtimeNames @($nonSystem.Keys))){throw "PE runtime dependency set differs: $($nonSystem.Keys -join ',')"}
    Copy-ExactTree (Join-Path $bin 'assets') (Join-Path $assembly 'assets')
    $entryBefore=Get-Sha (Join-Path $assembly $entryName);$captureScript=Join-Path $worktree 'games\engine_lab\native\tests\EngineLabNetworkCaptureSmoke.ps1';$captureScriptSha=Get-Sha $captureScript
    if($TestFailPoint -eq 'Capture'){throw 'Injected capture failure.'};Invoke-Recorded 'engine-lab-normal-capture' $powershell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$captureScript,'-Executable',(Join-Path $assembly $entryName),'-SkipUnavailableGate') $assembly $evidence 600|Out-Null;$normalCaptureRoot=Join-Path $assembly 'captures';$normalRunId=Assert-PngAndReceipt $normalCaptureRoot $sourceCommit $false;Copy-Item -LiteralPath (Join-Path $normalCaptureRoot 'network-runtime-receipt.json') -Destination (Join-Path $evidence 'normal-network-runtime-receipt.json');Copy-Item -LiteralPath (Join-Path $normalCaptureRoot 'network-capture-receipt.json') -Destination (Join-Path $evidence 'normal-network-capture-receipt.json')
    $oldGpuValidation=$env:AEVRYND_GPU_VALIDATION;try{$env:AEVRYND_GPU_VALIDATION='1';Invoke-Recorded 'gpu-validation' $powershell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$captureScript,'-Executable',(Join-Path $assembly $entryName),'-SkipUnavailableGate') $assembly $evidence 600|Out-Null}finally{$env:AEVRYND_GPU_VALIDATION=$oldGpuValidation}
    $gpuCommand=Get-Content -LiteralPath (Join-Path $evidence 'gpu-validation.command.json') -Raw|ConvertFrom-Json;$gpuError=Get-Content -LiteralPath (Join-Path $evidence 'gpu-validation.err.log') -Raw;if($gpuCommand.exitCode -ne 0 -or $gpuCommand.gpuValidationEnvironment -cne '1' -or $gpuError -match '(?i)(device removed|live object|resource state|descriptor.*error|validation error|missing.*dll)'){throw 'GPU validation diagnostics rejected.'}
    [ordered]@{schemaVersion=1;requested=$true;environment='AEVRYND_GPU_VALIDATION=1';commandRecordSha256=(Get-Sha (Join-Path $evidence 'gpu-validation.command.json'));exitCode=[int]$gpuCommand.exitCode;stderrSha256=(Get-Sha (Join-Path $evidence 'gpu-validation.err.log'));passed=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $evidence 'gpu-validation-receipt.json') -Encoding UTF8
    $entryAfter=Get-Sha (Join-Path $assembly $entryName);if($entryAfter -ne $entryBefore){throw 'Entry changed during capture gate.'}
    $captureRoot=Join-Path $assembly 'captures';$gpuRunId=Assert-PngAndReceipt $captureRoot $sourceCommit $true;if($gpuRunId -ceq $normalRunId){throw 'Normal and GPU validation runId values were reused.'};if($TestFailPoint -eq 'ReceiptMismatch'){throw 'Injected receipt mismatch.'}
    $runtimeReceipt=Join-Path $captureRoot 'network-runtime-receipt.json';$captureReceipt=Join-Path $captureRoot 'network-capture-receipt.json';$gpuDiagnostics=Join-Path $captureRoot 'gpu-validation-diagnostics.txt'
    $runtimeReceiptSha=Get-Sha $runtimeReceipt;$captureReceiptSha=Get-Sha $captureReceipt
    Copy-Item -LiteralPath $runtimeReceipt -Destination (Join-Path $evidence 'network-runtime-receipt.json');Copy-Item -LiteralPath $captureReceipt -Destination (Join-Path $evidence 'network-capture-receipt.json');Copy-Item -LiteralPath $gpuDiagnostics -Destination (Join-Path $evidence 'gpu-validation-diagnostics.txt')
    Remove-Item -LiteralPath $runtimeReceipt,$captureReceipt,$gpuDiagnostics -Force
    $captureTree=@(Get-ChildItem -LiteralPath $captureRoot -File -Recurse|ForEach-Object{$_.FullName.Substring((Get-Full $captureRoot).Length+1).Replace('\','/')}|Sort-Object);if(Compare-Object -CaseSensitive ($captureNames|Sort-Object) $captureTree){throw 'Candidate capture tree is not exact six.'}
    $captureEnvelope=[ordered]@{schemaVersion=1;sourceCommit=$sourceCommit;detachedCommit=$detached;builderScriptSha256=$builderScriptSha;entrySha256=$entryBefore;captureScriptSha256=$captureScriptSha;runtimeReceiptSha256=$runtimeReceiptSha;captureReceiptSha256=$captureReceiptSha;captures=@($captureNames|ForEach-Object{[ordered]@{file=$_;sha256=(Get-Sha (Join-Path $captureRoot $_))}})}
    $captureEnvelope|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $evidence 'capture-provenance.json') -Encoding UTF8
    [IO.File]::WriteAllText((Join-Path $evidence 'source-commit.txt'),$sourceCommit+"`n",[Text.UTF8Encoding]::new($false))
    $detachedFinal=((Invoke-TrustedGit $git @('-C',$worktree,'rev-parse','HEAD') 'detached-final-head') -join '').Trim().ToLowerInvariant();$detachedDirty=Invoke-TrustedGit $git @('-C',$worktree,'status','--porcelain','--untracked-files=all') 'detached-final-status';if($detachedFinal -ne $sourceCommit -or $detachedDirty){throw 'Detached source changed during candidate build.'}
    $preCommitHead=((Invoke-TrustedGit $git @('-C',$ProjectRoot,'rev-parse','HEAD') 'final-head') -join '').Trim().ToLowerInvariant();$preCommitOrigin=((Invoke-TrustedGit $git @('-C',$ProjectRoot,'rev-parse','origin/main') 'final-origin') -join '').Trim().ToLowerInvariant();if($preCommitHead -ne $sourceCommit -or $preCommitOrigin -ne $sourceCommit){throw 'Source pin changed before candidate assembly.'};[void](Invoke-TrustedGit $git @('-C',$ProjectRoot,'worktree','remove','--force',$worktree) 'worktree-remove');$worktreeAdded=$false
    $script:gitCommandRecords|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $evidence 'git-command-provenance.json') -Encoding UTF8
    $inventoryText=Get-Content -LiteralPath (Join-Path $evidence 'ctest-inventory.out.log') -Raw;$testCount=[int]([regex]::Match($inventoryText,'Total Tests:\s*(\d+)').Groups[1].Value)
    [ordered]@{schemaVersion=1;sourceCommit=$sourceCommit;builderScriptSha256=$builderScriptSha;releaseId=$releaseId;configuration='Release';ctestInventory=$testCount;networkGates=$networkNames;maturity='deterministic-loopback-validated-wip';peTool=(Get-Full $dumpbin);runtimeBundledDependencies=@($nonSystem.Keys|Sort-Object);externalSystemDependencies=@($externalSystem)}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $evidence 'candidate-build-receipt.json') -Encoding UTF8
    $records=Get-FileRecords $assembly
    [ordered]@{schemaVersion=1;releaseId=$releaseId;product='EngineLab';productVersion='2.54.0';engineVersion='2.86.0';commit=$sourceCommit;entry=$entryName;featureStage='NetworkReplicationPhase1ValidatedWip';maturity='validated-wip';transportScope='deterministic-loopback';derivedCaptures=@();files=$records}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $assembly 'candidate.manifest.json') -Encoding UTF8
    Assert-CandidateManifest $assembly
    Assert-DirectNewChild $SandboxRoot $CandidateRoot;[IO.Directory]::Move($assembly,$CandidateRoot);$candidateCommitted=$true;Assert-CandidateManifest $CandidateRoot
    if((Get-Sha $PSCommandPath) -ne $builderScriptSha){throw 'Builder script changed during candidate creation.'}
    $readyMessage="CANDIDATE_READY path=$CandidateRoot source=$sourceCommit release=$releaseId tests=$testCount gates=7"
} catch {
    $primaryFailure=$true;$primaryMessage=$_.Exception.Message
    if($candidateCommitted){try{Remove-OwnedTree $SandboxRoot $CandidateRoot ([IO.Path]::GetFileName($CandidateRoot));$candidateCommitted=$false}catch{$primaryMessage+="; secondary candidate cleanup: $($_.Exception.Message)"}}
    throw "CANDIDATE_FAILED_CLOSED: $primaryMessage"
} finally {
    $cleanupErrors=[Collections.Generic.List[string]]::new()
    if($worktreeAdded){try{[void](Invoke-TrustedGit $git @('-C',$ProjectRoot,'worktree','remove','--force',$worktree) 'worktree-remove')}catch{$cleanupErrors.Add("worktree: $($_.Exception.Message)")}}
    if(Test-Path -LiteralPath $tempRoot){try{Remove-OwnedTree $SandboxRoot $tempRoot '.a-' $tempIdentity $sourceCommit}catch{$cleanupErrors.Add("owned-temp: $($_.Exception.Message)")}}
    if($cleanupErrors.Count){$bounded=($cleanupErrors -join '; ');if($bounded.Length -gt 4096){$bounded=$bounded.Substring(0,4096)};if($primaryFailure){Write-Warning "Secondary cleanup failure: $bounded"}else{if($candidateCommitted){try{Remove-OwnedTree $SandboxRoot $CandidateRoot ([IO.Path]::GetFileName($CandidateRoot));$candidateCommitted=$false}catch{$bounded+="; candidate revoke: $($_.Exception.Message)"}};throw "Candidate cleanup failed closed: $bounded"}}
}
if($readyMessage){Write-Host $readyMessage}
