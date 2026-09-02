Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Test-NgrokDevelopmentDomain {
    param([object]$Value)
    return $Value-is[string]-and$Value-ceq$Value.ToLowerInvariant()-and$Value-match'^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.ngrok-free\.dev$'-and$Value.Length-le253
}

function Assert-NgrokStorePackageIdentity {
    param([object]$Identity)
    # MSIX 內的執行檔可不帶獨立簽章；信任 Store 簽署套件身分並再核對固定雜湊。
    $package=Get-AppxPackage -Name ([string]$Identity.packageId) -ErrorAction Stop
    if(@($package).Count-ne1-or$package.PackageFullName-cne[string]$Identity.packageFullName-or
       $package.Version.ToString()-cne[string]$Identity.packageVersion-or$package.Publisher-cne[string]$Identity.packagePublisher-or
       $package.PublisherId-cne[string]$Identity.packagePublisherId-or$package.SignatureKind.ToString()-cne[string]$Identity.packageSignatureKind-or
       $package.Status.ToString()-cne'Ok'){throw 'Pinned ngrok Microsoft Store package identity is invalid.'}
    $expectedExe=[IO.Path]::GetFullPath((Join-Path ([string]$package.InstallLocation) 'ngrok.exe'))
    if($expectedExe-cne[IO.Path]::GetFullPath([string]$Identity.installedExecutablePath)){throw 'Pinned ngrok executable is outside the Store package identity.'}
    return $expectedExe
}

function Get-NgrokExecutableIdentity {
    param([string]$ScriptsRoot=$PSScriptRoot)
    $identity=Get-Content -Raw (Join-Path $ScriptsRoot 'ngrok-package-identity.json')|ConvertFrom-Json
    $path=Assert-NgrokStorePackageIdentity $identity
    Assert-NoReparseAncestors $path
    $stream=[IO.FileStream]::new($path,'Open','Read','Read')
    try{
        if([Aevrynd.SafePath]::FinalPath($stream.SafeFileHandle)-cne$path){throw 'Ngrok executable final path changed.'}
        $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if($hash-cne[string]$identity.installedExecutableSha256){throw 'Ngrok executable hash differs from package identity.'}
        return [pscustomobject]@{Path=$path;Sha256=$hash;Stream=$stream}
    }catch{$stream.Dispose();throw}
}

function Test-ExactNgrokCommandLine {
    param([string]$CommandLine,[string]$ExecutablePath,[string]$Domain)
    if([string]::IsNullOrWhiteSpace($CommandLine)-or-not(Test-NgrokDevelopmentDomain $Domain)){return $false}
    $exe=[regex]::Escape([IO.Path]::GetFullPath($ExecutablePath));$escapedDomain=[regex]::Escape($Domain)
    $pattern='(?i)^\s*"?'+$exe+'"?\s+http\s+"?http://127\.0\.0\.1:8787"?\s+--domain='+$escapedDomain+'\s+--scheme=https\s+--inspect=false\s+--log=false\s+--web-addr=127\.0\.0\.1:4040\s*$'
    return $CommandLine-match$pattern
}

function Assert-NgrokProcessIdentity {
    param([object]$State,[object]$Executable)
    try{
        if($Executable.Stream.SafeFileHandle.IsClosed-or[Aevrynd.SafePath]::FinalPath($Executable.Stream.SafeFileHandle)-cne$Executable.Path){throw 'Live Ngrok executable handle identity changed.'}
        if($State.executablePath-cne$Executable.Path-or$State.executableSha256-cne$Executable.Sha256){throw 'Ngrok state executable identity differs from Store package.'}
        $record=Get-CimInstance Win32_Process -Filter "ProcessId = $($State.pid)" -ErrorAction Stop
        if($null-eq$record-or-not(Test-ExactNgrokCommandLine ([string]$record.CommandLine) $Executable.Path $State.domain)){throw 'Ngrok process command line is outside the fixed tuple.'}
        $process=Get-Process -Id ([int]$State.pid) -ErrorAction Stop
        if($process.Path-cne$Executable.Path-or$process.StartTime.ToUniversalTime().ToString('o')-cne$State.startedUtc){throw 'Ngrok process identity changed.'}
        return $process
    }catch{throw}
}

function Write-NgrokPendingState {
    param([hashtable]$Paths,[string]$Domain,[object]$Executable,[string]$Nonce)
    if($Nonce-notmatch'^[0-9a-f]{32}$'){throw 'Ngrok pending nonce is invalid.'}
    $document=[ordered]@{schemaVersion=1;createdUtc=[DateTimeOffset]::UtcNow.ToString('o');launchNonce=$Nonce;domain=$Domain;executablePath=$Executable.Path;executableSha256=$Executable.Sha256;origin='http://127.0.0.1:8787';path='/webhook'}
    $temporary="$($Paths.NgrokPendingFile).$Nonce.tmp";$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($document|ConvertTo-Json -Compress));$directory=Open-VerifiedDirectoryHandle $Paths.State
    try{$stream=[IO.FileStream]::new($temporary,'CreateNew','Write','None',4096,[IO.FileOptions]::WriteThrough);try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()};[IO.File]::Move($temporary,$Paths.NgrokPendingFile)}finally{[Array]::Clear($bytes,0,$bytes.Length);if(Test-Path $temporary){Remove-Item $temporary -Force};$directory.Dispose()}
}

function Read-NgrokPendingState {
    param([hashtable]$Paths)
    if(-not(Test-Path $Paths.NgrokPendingFile -PathType Leaf)){return $null}
    Assert-NoReparseAncestors $Paths.NgrokPendingFile;$stream=[IO.FileStream]::new($Paths.NgrokPendingFile,'Open','Read','Read');try{if($stream.Length-lt2-or$stream.Length-gt8192){throw 'Ngrok pending state size is invalid.'};$bytes=[byte[]]::new([int]$stream.Length);$null=$stream.Read($bytes,0,$bytes.Length);try{$raw=[Text.UTF8Encoding]::new($false,$true).GetString($bytes)}finally{[Array]::Clear($bytes,0,$bytes.Length)}}finally{$stream.Dispose()}
    $state=ConvertFrom-StrictFunnelJson $raw 8192;Assert-ExactJsonKeys $state @('createdUtc','domain','executablePath','executableSha256','launchNonce','origin','path','schemaVersion') 'Ngrok pending state'
    if(-not(Test-NgrokJsonInteger $state.schemaVersion)-or[long]$state.schemaVersion-ne1-or$state.launchNonce-isnot[string]-or$state.launchNonce-notmatch'^[0-9a-f]{32}$'-or-not(Test-NgrokDevelopmentDomain $state.domain)-or$state.executableSha256-isnot[string]-or$state.executableSha256-notmatch'^[0-9a-f]{64}$'-or$state.origin-cne'http://127.0.0.1:8787'-or$state.path-cne'/webhook'){throw 'Ngrok pending state is invalid.'}
    $null=ConvertFrom-CanonicalFunnelTimestamp ([string]$state.createdUtc) 'Ngrok pending createdUtc';return $state
}

function Resolve-NgrokPendingLaunch {
    param([hashtable]$Paths,[object]$Executable)
    $pending=Read-NgrokPendingState $Paths;if($null-eq$pending){return}
    if($pending.executablePath-cne$Executable.Path-or$pending.executableSha256-cne$Executable.Sha256){throw 'Pending Ngrok executable identity differs from Store package.'}
    $records=@(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue)
    if($records.Count-gt1){throw 'Pending Ngrok recovery is ambiguous.'}
    if($records.Count-eq1){if($records[0].ExecutablePath-cne$Executable.Path-or-not(Test-ExactNgrokCommandLine ([string]$records[0].CommandLine) $Executable.Path $pending.domain)){throw 'Pending Ngrok process tuple is not exact.'};$process=Get-Process -Id ([int]$records[0].ProcessId) -ErrorAction Stop;if($process.Path-cne$Executable.Path){throw 'Pending Ngrok process path changed.'};$process.Kill($true);$process.WaitForExit()}
    Remove-Item -LiteralPath $Paths.NgrokPendingFile -Force
}

function Test-NgrokJsonInteger {
    param([object]$Value)
    return $null-ne$Value-and$Value-isnot[bool]-and($Value-is[int]-or$Value-is[long])
}

function Read-NgrokState {
    param([hashtable]$Paths)
    if(-not(Test-Path -LiteralPath $Paths.NgrokStateFile -PathType Leaf)){return $null}
    $directoryHandle=Open-VerifiedDirectoryHandle $Paths.State
    try{
        Assert-NoReparseAncestors $Paths.NgrokStateFile
        $stream=[IO.FileStream]::new($Paths.NgrokStateFile,'Open','Read','Read',4096,[IO.FileOptions]::SequentialScan)
        try{
            if([Aevrynd.SafePath]::FinalPath($stream.SafeFileHandle)-cne[IO.Path]::GetFullPath($Paths.NgrokStateFile)){throw 'Ngrok state final path is invalid.'}
            if($stream.Length-lt2-or$stream.Length-gt8192){throw 'Ngrok state size is outside policy.'}
            $bytes=[byte[]]::new([int]$stream.Length);$offset=0
            while($offset-lt$bytes.Length){$count=$stream.Read($bytes,$offset,$bytes.Length-$offset);if($count-eq0){throw 'Ngrok state was truncated.'};$offset+=$count}
            if($stream.ReadByte()-ne-1){throw 'Ngrok state changed during read.'}
            try{$raw=[Text.UTF8Encoding]::new($false,$true).GetString($bytes)}finally{[Array]::Clear($bytes,0,$bytes.Length)}
        }finally{$stream.Dispose()}
    }finally{$directoryHandle.Dispose()}
    $state=ConvertFrom-StrictFunnelJson $raw 8192
    Assert-ExactJsonKeys $state @('domain','executablePath','executableSha256','origin','path','pid','schemaVersion','startedUtc') 'Ngrok state'
    if(-not(Test-NgrokJsonInteger $state.schemaVersion)-or[long]$state.schemaVersion-ne1-or-not(Test-NgrokJsonInteger $state.pid)-or[long]$state.pid-lt1-or[long]$state.pid-gt[int]::MaxValue){throw 'Ngrok state numeric fields are invalid.'}
    foreach($name in @('domain','executablePath','executableSha256','origin','path','startedUtc')){if($state[$name]-isnot[string]-or[string]::IsNullOrWhiteSpace($state[$name])){throw "Ngrok state $name must be a non-empty string."}}
    if(-not(Test-NgrokDevelopmentDomain $state.domain)-or$state.executableSha256-notmatch'^[0-9a-f]{64}$'-or$state.origin-cne'http://127.0.0.1:8787'-or$state.path-cne'/webhook'){throw 'Ngrok state transport or executable identity is invalid.'}
    $stamp=ConvertFrom-CanonicalFunnelTimestamp $state.startedUtc 'Ngrok startedUtc';if($stamp-gt[DateTimeOffset]::UtcNow.AddMinutes(5)){throw 'Ngrok start time is unreasonable.'}
    return $state
}

function Write-NgrokState {
    param([hashtable]$Paths,[int]$ProcessId,[string]$StartedUtc,[string]$Domain,[string]$ExecutablePath,[string]$ExecutableSha256)
    $directoryHandle=Open-VerifiedDirectoryHandle $Paths.State;$temporary=$null;$backup=$null;$bytes=$null
    try{
        if(Test-Path -LiteralPath $Paths.NgrokStateFile){if(((Get-Item -LiteralPath $Paths.NgrokStateFile).Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'Ngrok state file must not be a reparse point.'}}
        $document=[ordered]@{schemaVersion=1;pid=$ProcessId;startedUtc=$StartedUtc;domain=$Domain;executablePath=$ExecutablePath;executableSha256=$ExecutableSha256;origin='http://127.0.0.1:8787';path='/webhook'}
        $temporary="$($Paths.NgrokStateFile).$([Guid]::NewGuid().ToString('N')).tmp";$backup="$($Paths.NgrokStateFile).$([Guid]::NewGuid().ToString('N')).bak"
        $bytes=[Text.UTF8Encoding]::new($false,$true).GetBytes(($document|ConvertTo-Json -Compress))
        try{$stream=[IO.FileStream]::new($temporary,'CreateNew','Write','None',4096,[IO.FileOptions]::WriteThrough);try{if([Aevrynd.SafePath]::FinalPath($stream.SafeFileHandle)-cne[IO.Path]::GetFullPath($temporary)){throw 'Temporary Ngrok state escaped its directory.'};$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()};if(Test-Path $Paths.NgrokStateFile){[IO.File]::Replace($temporary,$Paths.NgrokStateFile,$backup,$true);Remove-Item $backup -Force}else{[IO.File]::Move($temporary,$Paths.NgrokStateFile)};$verified=Read-NgrokState $Paths;if($verified.pid-ne$ProcessId-or$verified.startedUtc-cne$StartedUtc-or$verified.executableSha256-cne$ExecutableSha256){throw 'Durable Ngrok state read-back mismatch.'}}finally{if($null-ne$bytes){[Array]::Clear($bytes,0,$bytes.Length)};foreach($artifact in @($temporary,$backup)){if($artifact-and(Test-Path $artifact)){Remove-Item $artifact -Force}}}
    }finally{$directoryHandle.Dispose()}
}

function Read-NgrokDomainConfiguration {
    param([hashtable]$Paths)
    if(-not(Test-Path -LiteralPath $Paths.NgrokDomainFile -PathType Leaf)){throw 'Ngrok development domain is not configured.'}
    $item=Get-Item -LiteralPath $Paths.NgrokDomainFile
    if(($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0-or$item.Length-lt2-or$item.Length-gt2048){throw 'Ngrok domain configuration path or size is invalid.'}
    $raw=[IO.File]::ReadAllText($item.FullName,[Text.UTF8Encoding]::new($false,$true));$document=$null
    try{
        $options=[Text.Json.JsonDocumentOptions]::new();$options.AllowTrailingCommas=$false;$options.CommentHandling=[Text.Json.JsonCommentHandling]::Disallow;$options.MaxDepth=4
        $document=[Text.Json.JsonDocument]::Parse($raw,$options);$root=$document.RootElement
        if($root.ValueKind-ne[Text.Json.JsonValueKind]::Object){throw 'Ngrok domain configuration must be a JSON object.'}
        $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($property in $root.EnumerateObject()){if(-not$names.Add($property.Name)){throw 'Ngrok domain configuration contains duplicate fields.'}}
        if($names.Count-ne2-or-not$names.Contains('schemaVersion')-or-not$names.Contains('domain')){throw 'Ngrok domain configuration contains unsupported fields.'}
        $schema=$root.GetProperty('schemaVersion');$domainElement=$root.GetProperty('domain')
        $schemaValue=[long]0;if($schema.ValueKind-ne[Text.Json.JsonValueKind]::Number-or-not$schema.TryGetInt64([ref]$schemaValue)-or$schemaValue-ne1){throw 'Ngrok domain schemaVersion must be integer 1.'}
        if($domainElement.ValueKind-ne[Text.Json.JsonValueKind]::String){throw 'Ngrok domain must be a JSON string.'};$domain=$domainElement.GetString()
        if(-not(Test-NgrokDevelopmentDomain $domain)){throw 'Ngrok domain must be one exact lowercase ngrok-free.dev development domain.'}
        return [pscustomobject]@{SchemaVersion=1;Domain=$domain;WebhookUrl="https://$domain/webhook"}
    }finally{if($null-ne$document){$document.Dispose()}}
}
