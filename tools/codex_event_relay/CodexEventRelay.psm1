Set-StrictMode -Version Latest

$script:ProtocolVersion = 1
$script:MaximumMessageBytes = 32768
$script:MaximumTaskBytes = 16384
$script:MaximumOperations = 1024
$script:MaximumNonces = 256
$script:MaximumOwnedFiles = 32

function Assert-ExactProperties {
    param($Value,[string[]]$Names,[string]$Kind)
    if($null-eq$Value-or$Value-is[string]-or$Value-is[Array]){throw "$Kind schema is invalid"}
    $actual=@($Value.PSObject.Properties.Name|Sort-Object)
    $expected=@($Names|Sort-Object)
    if(($actual-join',')-cne($expected-join',')){throw "$Kind schema is invalid"}
}

function Test-UtcTimestamp {
    param($Value,[int]$MaximumSkewSeconds=120)
    if($Value-isnot[string]){return $false}
    try{$stamp=[DateTimeOffset]::ParseExact($Value,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{return $false}
    return $stamp.Offset-eq[TimeSpan]::Zero-and[Math]::Abs(([DateTimeOffset]::UtcNow-$stamp).TotalSeconds)-le$MaximumSkewSeconds
}

function New-SecureNonce {
    $bytes=New-Object byte[] 32
    $rng=[Security.Cryptography.RandomNumberGenerator]::Create()
    try{$rng.GetBytes($bytes)}finally{$rng.Dispose()}
    return ([Convert]::ToBase64String($bytes)).TrimEnd('=').Replace('+','-').Replace('/','_')
}

function Get-HmacHex {
    param([byte[]]$Secret,[string]$Canonical)
    $hmac=[Security.Cryptography.HMACSHA256]::new($Secret)
    try{return ([BitConverter]::ToString($hmac.ComputeHash([Text.Encoding]::UTF8.GetBytes($Canonical)))).Replace('-','').ToLowerInvariant()}finally{$hmac.Dispose()}
}

function Test-FixedTimeHex {
    param([string]$Expected,[string]$Actual)
    if($Expected-notmatch'^[0-9a-f]{64}$'-or$Actual-notmatch'^[0-9a-f]{64}$'){return $false}
    $left=New-Object byte[] 32;$right=New-Object byte[] 32
    for($i=0;$i-lt32;$i++){$left[$i]=[Convert]::ToByte($Expected.Substring($i*2,2),16);$right[$i]=[Convert]::ToByte($Actual.Substring($i*2,2),16)}
    return [Security.Cryptography.CryptographicOperations]::FixedTimeEquals($left,$right)
}

function Read-EventRelaySecret {
    param([string]$Path,[switch]$RequireSecureAcl)
    $safe=Assert-RelayPathSafe $Path
    if(-not(Test-Path -LiteralPath $safe -PathType Leaf)){throw 'Handshake secret file was not found'}
    $file=Get-Item -LiteralPath $safe -Force
    if(($file.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0-or$file.Length-lt32-or$file.Length-gt4096){throw 'Handshake secret file is unsafe'}
    if($RequireSecureAcl){$null=Assert-RelayFileAcl $safe}
    $text=(Get-Content -LiteralPath $safe -Raw -Encoding UTF8).Trim()
    try{$bytes=[Convert]::FromBase64String($text)}catch{throw 'Handshake secret must be base64'}
    if($bytes.Length-lt32-or$bytes.Length-gt64){throw 'Handshake secret must decode to 32-64 bytes'}
    return $bytes
}

function New-InitialEventState {
    param([string]$Node)
    [pscustomobject][ordered]@{schemaVersion=1;nodeId=$Node;operations=@();pendingMessages=@();serverNonces=@()}
}

function Assert-EventState {
    param($State,[string]$Node)
    Assert-ExactProperties $State @('schemaVersion','nodeId','operations','pendingMessages','serverNonces') 'Event state'
    if([int]$State.schemaVersion-ne1-or[string]$State.nodeId-cne$Node-or@($State.operations).Count-gt$script:MaximumOperations-or@($State.pendingMessages).Count-gt$script:MaximumOperations-or@($State.serverNonces).Count-gt$script:MaximumNonces){throw 'Event state is invalid'}
    $ids=@{}
    foreach($op in @($State.operations)){
        Assert-ExactProperties $op @('commandId','state','startedAt','finishedAt','exitCode','summary','executionStarted') 'Operation'
        if($op.commandId-isnot[string]-or$op.commandId-notmatch'^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$'-or$ids.ContainsKey($op.commandId)-or$op.state-notin@('running','completed','failed')-or$op.executionStarted-isnot[bool]-or$op.startedAt-isnot[string]-or$op.summary-isnot[string]-or$op.summary.Length-lt1-or$op.summary.Length-gt240-or$op.summary-match'[\x00-\x1F]'){throw 'Operation identity is invalid'}
        try{$start=[DateTimeOffset]::ParseExact($op.startedAt,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw 'Operation timestamp is invalid'}
        if($start.Offset-ne[TimeSpan]::Zero){throw 'Operation timestamp is invalid'}
        if($op.state-ceq'running'){if($null-ne$op.finishedAt-or$null-ne$op.exitCode){throw 'Running operation is invalid'}}else{if(-not$op.executionStarted-or$op.finishedAt-isnot[string]-or$op.exitCode-is[bool]-or$op.exitCode-is[string]){throw 'Terminal operation is invalid'};try{$finish=[DateTimeOffset]::ParseExact($op.finishedAt,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind);$code=[int]$op.exitCode}catch{throw 'Terminal operation is invalid'};if($finish.Offset-ne[TimeSpan]::Zero-or$code-lt0-or$code-gt65535-or($op.state-ceq'completed'-and$code-ne0)-or($op.state-ceq'failed'-and$code-eq0)){throw 'Terminal operation is invalid'}}
        $ids[$op.commandId]=$true
    }
    foreach($message in @($State.pendingMessages)){
        Assert-ExactProperties $message @('commandId','state','payload') 'Pending message'
        if($message.commandId-isnot[string]-or-not$ids.ContainsKey($message.commandId)-or$message.state-notin@('running','completed','failed')-or$message.payload-isnot[string]-or[Text.Encoding]::UTF8.GetByteCount($message.payload)-gt$script:MaximumMessageBytes){throw 'Pending message is invalid'}
    }
    foreach($nonce in @($State.serverNonces)){if($nonce-isnot[string]-or$nonce-notmatch'^[A-Za-z0-9_-]{32,128}$'){throw 'Nonce history is invalid'}}
    return $State
}

function Read-EventState {
    param([string]$Path,[string]$Node,[switch]$RequireSecureAcl)
    if(-not(Test-Path -LiteralPath $Path -PathType Leaf)){return New-InitialEventState $Node}
    $shared=Get-Module CodexCommandRelay
    if(-not$shared){throw 'Shared command relay module is unavailable'}
    $value=&$shared { param($StatePath,$Verify) Read-RelayJsonStrict $StatePath $null 'event state' -VerifyAcl:$Verify } $Path ([bool]$RequireSecureAcl)
    return Assert-EventState $value $Node
}

function Write-EventState {
    param([string]$Path,$State,[switch]$RequireSecureAcl)
    $null=Assert-EventState $State ([string]$State.nodeId)
    Write-RelayJsonAtomic $Path $State -VerifyAcl:$RequireSecureAcl
}

function Read-EventRelayJsonStrict {
    param([string]$Path,[string]$Kind,[switch]$RequireSecureAcl)
    $shared=Get-Module CodexCommandRelay
    if(-not$shared){throw 'Shared command relay module is unavailable'}
    return &$shared { param($JsonPath,$JsonKind,$Verify) Read-RelayJsonStrict $JsonPath $null $JsonKind -VerifyAcl:$Verify } $Path $Kind ([bool]$RequireSecureAcl)
}

function Read-ReadinessRequest {
    param([string]$Path,[string]$Node,[switch]$RequireSecureAcl)
    $request=Read-EventRelayJsonStrict $Path 'readiness request' -RequireSecureAcl:$RequireSecureAcl
    Assert-ExactProperties $request @('schemaVersion','nodeId','installNonce','requestedAtUtc') 'Readiness request'
    if($request.schemaVersion-is[bool]-or[int]$request.schemaVersion-ne1-or$request.nodeId-isnot[string]-or$request.nodeId-cne$Node-or$request.installNonce-isnot[string]-or$request.installNonce-notmatch'^[A-Za-z0-9_-]{43}$'-or$request.requestedAtUtc-isnot[string]){throw 'Readiness request is invalid'}
    try{$requested=[DateTimeOffset]::ParseExact($request.requestedAtUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw 'Readiness request timestamp is invalid'}
    if($requested.Offset-ne[TimeSpan]::Zero-or$requested-gt[DateTimeOffset]::UtcNow.AddSeconds(30)-or$requested-lt[DateTimeOffset]::UtcNow.AddMinutes(-10)){throw 'Readiness request is stale'}
    return $request
}

function Publish-EventRelayReadiness {
    param([string]$RequestPath,[string]$ReceiptPath,[string]$Node,[switch]$RequireSecureAcl)
    if(-not(Test-Path -LiteralPath $RequestPath -PathType Leaf)){return $null}
    try{$request=Read-ReadinessRequest $RequestPath $Node -RequireSecureAcl:$RequireSecureAcl}catch{if($_.Exception.Message-ceq'Readiness request is stale'){return $null};throw}
    $receipt=[pscustomobject][ordered]@{schemaVersion=1;nodeId=$Node;installNonce=[string]$request.installNonce;processId=[int]$PID;startedAtUtc=[DateTimeOffset]::UtcNow.ToString('o')}
    Write-RelayJsonAtomic $ReceiptPath $receipt -VerifyAcl:$RequireSecureAcl
    return $receipt
}

function Test-EventRelayReadinessReceipt {
    param([string]$Path,[string]$Node,[string]$InstallNonce,[DateTimeOffset]$RequestedAtUtc,[switch]$RequireSecureAcl,[switch]$RequireLiveProcess)
    if(-not(Test-Path -LiteralPath $Path -PathType Leaf)){throw 'Readiness receipt was not found'}
    $receipt=Read-EventRelayJsonStrict $Path 'readiness receipt' -RequireSecureAcl:$RequireSecureAcl
    Assert-ExactProperties $receipt @('schemaVersion','nodeId','installNonce','processId','startedAtUtc') 'Readiness receipt'
    if($receipt.schemaVersion-is[bool]-or[int]$receipt.schemaVersion-ne1-or$receipt.nodeId-isnot[string]-or$receipt.nodeId-cne$Node-or$receipt.installNonce-isnot[string]-or$receipt.installNonce-cne$InstallNonce-or$receipt.processId-is[bool]-or$receipt.processId-is[string]-or$receipt.startedAtUtc-isnot[string]){throw 'Readiness receipt identity is invalid'}
    try{$processId=[int]$receipt.processId;$started=[DateTimeOffset]::ParseExact($receipt.startedAtUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw 'Readiness receipt fields are invalid'}
    if($processId-le0-or$started.Offset-ne[TimeSpan]::Zero-or$started-lt$RequestedAtUtc-or$started-gt[DateTimeOffset]::UtcNow.AddSeconds(30)){throw 'Readiness receipt timestamp or process is invalid'}
    if($RequireLiveProcess){$process=Get-Process -Id $processId -ErrorAction SilentlyContinue;if($null-eq$process-or$process.HasExited){throw 'Readiness process is not alive'}}
    return $receipt
}

function New-EventRelayFileSnapshots {
    param([string[]]$Paths)
    $snapshots=[Collections.Generic.List[object]]::new()
    foreach($path in $Paths){$exists=Test-Path -LiteralPath $path -PathType Leaf;$snapshots.Add([pscustomobject]@{path=$path;existed=$exists;bytes=if($exists){[IO.File]::ReadAllBytes($path)}else{$null};acl=if($exists){Get-Acl -LiteralPath $path}else{$null}})}
    return @($snapshots)
}

function Restore-EventRelayFileSnapshots {
    param([object[]]$Snapshots)
    foreach($snapshot in $Snapshots){
        $path=[string]$snapshot.path
        if($snapshot.existed){
            $directory=Split-Path -Parent $path;if(-not(Test-Path -LiteralPath $directory -PathType Container)){New-Item -ItemType Directory -Path $directory -Force|Out-Null}
            $temporary=Join-Path $directory ('.rollback-'+[Guid]::NewGuid().ToString('N')+'.tmp')
            try{[IO.File]::WriteAllBytes($temporary,[byte[]]$snapshot.bytes);if(Test-Path -LiteralPath $path -PathType Leaf){$backup=$temporary+'.bak';[IO.File]::Replace($temporary,$path,$backup,$true);if(Test-Path -LiteralPath $backup){Remove-Item -LiteralPath $backup -Force}}else{Move-Item -LiteralPath $temporary -Destination $path};if($snapshot.acl){Set-Acl -LiteralPath $path -AclObject $snapshot.acl}}finally{if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force}}
        }elseif(Test-Path -LiteralPath $path -PathType Leaf){Remove-Item -LiteralPath $path -Force}
    }
}

function ConvertFrom-CommandMessage {
    param([string]$Json,[string]$Node,[string]$Workspace)
    if([Text.Encoding]::UTF8.GetByteCount($Json)-gt$script:MaximumMessageBytes){throw 'Server message is oversized'}
    try{
        # PowerShell 7 會預設把 ISO 日期轉型；保留字串後再由同一套嚴格 UTC 契約驗證。
        if((Get-Command ConvertFrom-Json).Parameters.ContainsKey('DateKind')){$value=$Json|ConvertFrom-Json -DateKind String}
        else{$value=$Json|ConvertFrom-Json}
    }catch{throw 'Server message JSON is invalid'}
    Assert-ExactProperties $value @('type','version','commandId','nodeId','ownedFiles','task','createdAtUtc') 'Command'
    if($value.type-isnot[string]-or$value.type-cne'command'-or$value.version-is[bool]-or[int]$value.version-ne1){throw 'Command version is invalid'}
    if($value.commandId-isnot[string]-or$value.commandId-notmatch'^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$'){throw 'Command id is invalid'}
    if($value.nodeId-isnot[string]-or$value.nodeId-cne$Node){throw 'Command targets another node'}
    if($value.createdAtUtc-isnot[string]){throw 'Command timestamp is invalid'}
    try{$created=[DateTimeOffset]::ParseExact($value.createdAtUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw 'Command timestamp is invalid'}
    if($created.Offset-ne[TimeSpan]::Zero){throw 'Command timestamp is invalid'}
    if($value.task-isnot[string]-or[string]::IsNullOrWhiteSpace($value.task)-or[Text.Encoding]::UTF8.GetByteCount($value.task)-gt$script:MaximumTaskBytes-or$value.task.IndexOf([char]0)-ge0){throw 'Command task is invalid'}
    if($value.ownedFiles-isnot[Array]-or$value.ownedFiles.Count-lt1-or$value.ownedFiles.Count-gt$script:MaximumOwnedFiles){throw 'Command ownedFiles is invalid'}
    $root=(Assert-RelayPathSafe $Workspace).TrimEnd('\')+'\';$owned=[Collections.Generic.List[string]]::new();$seen=@{}
    foreach($item in @($value.ownedFiles)){
        if($item-isnot[string]){throw 'Owned path is invalid'}
        $path=$item.Replace('\','/')
        if($path.Length-lt1-or$path.Length-gt256-or[IO.Path]::IsPathRooted($path)-or$path-match'(^|/)\.\.(/|$)|(^|/)\.git(/|$)' -or$path-notmatch'^[A-Za-z0-9._/*?\[\]-]+$'-or$seen.ContainsKey($path)){throw 'Owned path is invalid'}
        $literalPrefix=($path-split'[?*\[]',2)[0].TrimEnd('/')
        if($literalPrefix){$resolved=[IO.Path]::GetFullPath((Join-Path $Workspace $literalPrefix));if(-not($resolved+'\').StartsWith($root,[StringComparison]::OrdinalIgnoreCase)-and-not$resolved.Equals($root.TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Owned path escapes workspace'}}
        $seen[$path]=$true;$owned.Add($path)
    }
    return [pscustomobject]@{CommandId=$value.commandId;NodeId=$value.nodeId;OwnedFiles=@($owned);Task=$value.task;CreatedAtUtc=$value.createdAtUtc}
}

function New-StatusPayload {
    param([string]$Node,[string]$CommandId,[string]$State,[Nullable[int]]$ExitCode,[string]$Summary)
    $value=[ordered]@{version=1;type='ack';commandId=$CommandId;nodeId=$Node;state=$State;occurredAtUtc=[DateTimeOffset]::UtcNow.ToString('o');exitCode=if($null-eq$ExitCode){$null}else{[int]$ExitCode};summary=$Summary}
    return ($value|ConvertTo-Json -Compress)
}

function Add-PendingStatus {
    param($State,[string]$CommandId,[string]$Status,[Nullable[int]]$ExitCode,[string]$Summary)
    $payload=New-StatusPayload ([string]$State.nodeId) $CommandId $Status $ExitCode $Summary
    $State.pendingMessages=@($State.pendingMessages|Where-Object{[string]$_.commandId-cne$CommandId-or[string]$_.state-cne$Status})+@([pscustomobject][ordered]@{commandId=$CommandId;state=$Status;payload=$payload})
}

function Invoke-SharedBoundarySnapshot {
    param([string]$Workspace)
    $shared=Get-Module CodexCommandRelay
    if(-not$shared){throw 'Shared command relay module is unavailable'}
    return & $shared { param($Path) Get-GitBoundarySnapshot $Path } $Workspace
}

function Assert-TrustedCodex {
    param([string]$CodexPath)
    $safe=Assert-RelayPathSafe $CodexPath
    if(-not(Test-Path -LiteralPath $safe -PathType Leaf)){throw 'Codex executable was not found'}
    $signature=Get-AuthenticodeSignature -LiteralPath $safe
    if($signature.Status-ne[Management.Automation.SignatureStatus]::Valid-or$signature.SignerCertificate.Subject-notmatch'(?i)\bO="?OpenAI OpCo, LLC"?'){throw 'Codex executable is not trusted'}
    $login=&$safe login status 2>&1
    if($LASTEXITCODE-ne0-or($login-join"`n")-notmatch'(?i)logged in|authenticated'){throw 'Codex is not logged in'}
    return $safe
}

function Invoke-EventCodex {
    param([string]$CodexPath,[string]$Workspace,$Command,[string]$OutputPath)
    $prompt=@"
你是 Aevrynd $($Command.NodeId) 副機的受限執行代理。

強制政策：
- 只處理 TASK 與 OWNED_FILES 明確授權的範圍。
- 禁止所有 Git 寫操作、排程變更、relay 狀態變更與秘密存取。
- 不得修改 tools/codex_command_relay 或 tools/codex_event_relay。

COMMAND_ID: $($Command.CommandId)
OWNED_FILES: $($Command.OwnedFiles -join ', ')
TASK:
$($Command.Task)
"@
    $arguments=@('exec','--ephemeral','-m','gpt-5.6-luna','-c','model_reasoning_effort="medium"','-s','workspace-write','--approve-for-me','-C',$Workspace)
    $prompt|&$CodexPath @arguments *> $OutputPath
    return $LASTEXITCODE
}

function Send-PendingMessages {
    param($State,[scriptblock]$Send,[string]$StatePath,[switch]$RequireSecureAcl)
    while(@($State.pendingMessages).Count){
        $message=@($State.pendingMessages)[0]
        & $Send ([string]$message.payload)
        $State.pendingMessages=@($State.pendingMessages|Select-Object -Skip 1)
        Write-EventState $StatePath $State -RequireSecureAcl:$RequireSecureAcl
    }
}

function Invoke-ClaimedCommand {
    param($Command,$State,[string]$StatePath,[string]$AuditRoot,[string]$Workspace,[string]$CodexPath,[scriptblock]$Send,[scriptblock]$CaptureBoundary,[scriptblock]$CompareBoundary,[scriptblock]$RunCodex,[switch]$RequireSecureAcl)
    $existing=@($State.operations|Where-Object{[string]$_.commandId-ceq$Command.CommandId})
    if($existing.Count){
        $op=$existing[0];$code=if($null-eq$op.exitCode){$null}else{[Nullable[int]][int]$op.exitCode}
        if($op.state-cne'running'){Add-PendingStatus $State $Command.CommandId ([string]$op.state) $code ([string]$op.summary);Write-EventState $StatePath $State -RequireSecureAcl:$RequireSecureAcl;Send-PendingMessages $State $Send $StatePath -RequireSecureAcl:$RequireSecureAcl;return}
        if($op.executionStarted){$op.state='failed';$op.finishedAt=[DateTimeOffset]::UtcNow.ToString('o');$op.exitCode=92;$op.summary='Previous Codex launch ended without a durable result';Add-PendingStatus $State $Command.CommandId 'failed' ([Nullable[int]]92) $op.summary;Write-EventState $StatePath $State -RequireSecureAcl:$RequireSecureAcl;Send-PendingMessages $State $Send $StatePath -RequireSecureAcl:$RequireSecureAcl;return}
        # running claim 在 ACK 送達前不啟動模型；重連後由 server 重送同一命令即可安全續跑。
        Send-PendingMessages $State $Send $StatePath -RequireSecureAcl:$RequireSecureAcl
    }else{
        if(@($State.operations).Count-ge$script:MaximumOperations){throw 'Durable claim ledger is full; refusing to forget an earlier command'}
        $started=[DateTimeOffset]::UtcNow.ToString('o')
        $op=[pscustomobject][ordered]@{commandId=$Command.CommandId;state='running';executionStarted=$false;startedAt=$started;finishedAt=$null;exitCode=$null;summary='Command claimed before execution'}
        # claim ledger 不淘汰舊 id；滿額時 fail closed，避免古老 duplicate 被當成新命令重跑。
        $State.operations=@($State.operations)+@($op)
        Add-PendingStatus $State $Command.CommandId 'running' $null $op.summary
        Write-EventState $StatePath $State -RequireSecureAcl:$RequireSecureAcl
        Send-PendingMessages $State $Send $StatePath -RequireSecureAcl:$RequireSecureAcl
    }
    # 先 durable 記錄 launch boundary；此後若程序中斷，只回報 indeterminate failure，絕不重跑。
    $op.executionStarted=$true;Write-EventState $StatePath $State -RequireSecureAcl:$RequireSecureAcl
    $safeId=$Command.CommandId-replace'[^A-Za-z0-9._-]','_';if($safeId.Length-gt80){$safeId=$safeId.Substring(0,80)}
    $outputDirectory=Join-Path $AuditRoot ([string]$State.nodeId).ToLowerInvariant();if(-not(Test-Path -LiteralPath $outputDirectory)){New-Item -ItemType Directory -Path $outputDirectory -Force|Out-Null}
    $outputPath=Join-Path $outputDirectory ($safeId+'.log');$boundaryPath=Join-Path $outputDirectory ($safeId+'-boundary.json')
    $exitCode=1;$summary='Relay execution failed';$before=$null
    try{
        $before=if($CaptureBoundary){&$CaptureBoundary 'before' $Workspace}else{Invoke-SharedBoundarySnapshot $Workspace}
        if($RunCodex){$exitCode=[int](& $RunCodex $CodexPath $Workspace $Command $outputPath)}else{$trusted=Assert-TrustedCodex $CodexPath;$exitCode=[int](Invoke-EventCodex $trusted $Workspace $Command $outputPath)}
        $summary=if($exitCode-eq0){'Codex completed'}else{'Codex failed'}
    }catch{$exitCode=1;$summary='Relay execution failed'}
    if($before){
        try{$after=if($CaptureBoundary){&$CaptureBoundary 'after' $Workspace}else{Invoke-SharedBoundarySnapshot $Workspace};$comparison=if($CompareBoundary){&$CompareBoundary $before $after $Command.OwnedFiles}else{Compare-GitBoundarySnapshot $before $after $Command.OwnedFiles};if($null-eq$comparison-or$null-eq$comparison.passed){throw 'Invalid boundary result'};Write-RelayJsonAtomic $boundaryPath ([ordered]@{schemaVersion=1;commandId=$Command.CommandId;nodeId=$State.nodeId;passed=[bool]$comparison.passed;violations=@($comparison.violations);beforeHead=[string]$before.head;afterHead=[string]$after.head}) -VerifyAcl:$RequireSecureAcl;if(-not$comparison.passed){$exitCode=90;$summary='Git boundary audit failed'}}catch{$exitCode=91;$summary='Git boundary audit could not be completed'}
    }
    $op.state=if($exitCode-eq0){'completed'}else{'failed'};$op.finishedAt=[DateTimeOffset]::UtcNow.ToString('o');$op.exitCode=$exitCode;$op.summary=$summary
    Add-PendingStatus $State $Command.CommandId $op.state ([Nullable[int]]$exitCode) $summary
    Write-EventState $StatePath $State -RequireSecureAcl:$RequireSecureAcl
    Send-PendingMessages $State $Send $StatePath -RequireSecureAcl:$RequireSecureAcl
}

function New-ClientWebSocketTransport {
    param([Uri]$Endpoint)
    $socket=[Net.WebSockets.ClientWebSocket]::new();$socket.Options.KeepAliveInterval=[TimeSpan]::FromSeconds(30)
    [pscustomobject]@{
        Socket=$socket
        Connect={param($self,$headers)foreach($name in $headers.Keys){$self.Socket.Options.SetRequestHeader([string]$name,[string]$headers[$name])};$self.Socket.ConnectAsync($Endpoint,[Threading.CancellationToken]::None).GetAwaiter().GetResult()}
        Send={param($self,$text)$bytes=[Text.Encoding]::UTF8.GetBytes($text);$segment=[ArraySegment[byte]]::new($bytes);$self.Socket.SendAsync($segment,[Net.WebSockets.WebSocketMessageType]::Text,$true,[Threading.CancellationToken]::None).GetAwaiter().GetResult()}
        Receive={param($self)$stream=[IO.MemoryStream]::new();try{do{$buffer=New-Object byte[] 4096;$segment=[ArraySegment[byte]]::new($buffer);$result=$self.Socket.ReceiveAsync($segment,[Threading.CancellationToken]::None).GetAwaiter().GetResult();if($result.MessageType-eq[Net.WebSockets.WebSocketMessageType]::Close){throw 'WebSocket closed'};$stream.Write($buffer,0,$result.Count);if($stream.Length-gt$script:MaximumMessageBytes){throw 'Server message is oversized'}}until($result.EndOfMessage);return [Text.Encoding]::UTF8.GetString($stream.ToArray())}finally{$stream.Dispose()}}
        Dispose={param($self)$self.Socket.Dispose()}
    }
}

function Invoke-CodexEventRelay {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][ValidateSet('taichung','taoyuan')][string]$Node,
        [Parameter(Mandatory)][Uri]$Endpoint,[Parameter(Mandatory)][string]$SecretPath,[Parameter(Mandatory)][string]$CodexPath,[string]$SharedModulePath,
        [string]$Workspace='C:\Aevrynd\Games',[string]$StateRoot='C:\ProgramData\Aevrynd\Codex\event-relay',[string]$AuditRoot='C:\AevryndAudit\codex-event-relay',
        [scriptblock]$TransportFactory,[scriptblock]$Sleep,[scriptblock]$CaptureBoundary,[scriptblock]$CompareBoundary,[scriptblock]$RunCodex,[int]$MaximumConnections=0,[switch]$RequireSecureAcl=$true
    )
    if(-not$SharedModulePath){$SharedModulePath=Join-Path (Split-Path -Parent $PSScriptRoot) 'codex_command_relay\CodexCommandRelay.psm1'}
    if(-not(Test-Path -LiteralPath $SharedModulePath -PathType Leaf)){throw 'Shared command relay module was not found'};Import-Module $SharedModulePath -Force
    $Workspace=Assert-RelayPathSafe $Workspace;$StateRoot=Assert-RelayPathSafe $StateRoot;$AuditRoot=Assert-RelayPathSafe $AuditRoot
    if($Endpoint.Scheme-notin@('wss','ws')-or$Endpoint.AbsolutePath-cne'/api/codex-relay/connect'-or$Endpoint.Query-or$Endpoint.Fragment){throw 'WebSocket endpoint is invalid'}
    if($Endpoint.Scheme-ceq'ws'-and-not$Endpoint.IsLoopback){throw 'Remote endpoint must use wss'}
    if($RequireSecureAcl){foreach($path in @($StateRoot,$AuditRoot)){Protect-RelayDirectoryAcl $path}}
    else{foreach($path in @($StateRoot,$AuditRoot)){if(-not(Test-Path -LiteralPath $path)){New-Item -ItemType Directory -Path $path -Force|Out-Null}}}
    $lockPath=Join-Path $StateRoot 'event-relay.lock';$lock=$null
    try{$lock=[IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)}catch{throw 'Another event relay instance is already running'}
    try{
      $secret=Read-EventRelaySecret $SecretPath -RequireSecureAcl:$RequireSecureAcl;$statePath=Join-Path $StateRoot 'event-state.json';$state=Read-EventState $statePath $Node -RequireSecureAcl:$RequireSecureAcl
      # readiness 只能越過 module、ACL、single-instance lock、secret 與 durable state 全部成功後的邊界。
      $null=Publish-EventRelayReadiness (Join-Path $StateRoot 'readiness-request.json') (Join-Path $StateRoot 'readiness.json') $Node -RequireSecureAcl:$RequireSecureAcl
      $connection=0;$delay=1
      while($MaximumConnections-eq0-or$connection-lt$MaximumConnections){
        $connection++;$transport=$null
        try{
            $transport=if($TransportFactory){&$TransportFactory $connection}else{New-ClientWebSocketTransport $Endpoint}
            # 握手只使用 HTTP upgrade headers；idle 期間不送 application message。
            $clientNonce=New-SecureNonce;$timestamp=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds().ToString([Globalization.CultureInfo]::InvariantCulture)
            $canonical="GET`n/api/codex-relay/connect`n$Node`n$timestamp`n$clientNonce`n";$signature=Get-HmacHex $secret $canonical
            $headers=@{'X-Aevrynd-Node'=$Node;'X-Aevrynd-Timestamp'=$timestamp;'X-Aevrynd-Nonce'=$clientNonce;'X-Aevrynd-Signature'=('sha256='+$signature)}
            & $transport.Connect $transport $headers
            $sender={param($payload)&$transport.Send $transport $payload}.GetNewClosure();Send-PendingMessages $state $sender $statePath -RequireSecureAcl:$RequireSecureAcl;$delay=1
            while($true){$json=&$transport.Receive $transport;$command=ConvertFrom-CommandMessage $json $Node $Workspace;Invoke-ClaimedCommand $command $state $statePath $AuditRoot $Workspace $CodexPath $sender $CaptureBoundary $CompareBoundary $RunCodex -RequireSecureAcl:$RequireSecureAcl}
        }catch{
            if($MaximumConnections-ne0-and$connection-ge$MaximumConnections){break}
            if($Sleep){&$Sleep $delay}else{Start-Sleep -Seconds $delay};$delay=[Math]::Min($delay*2,60)
        }finally{if($transport-and$transport.Dispose){try{&$transport.Dispose $transport}catch{}}}
      }
    }finally{if($lock){$lock.Dispose()}}
}

Export-ModuleMember -Function Invoke-CodexEventRelay,ConvertFrom-CommandMessage,Get-HmacHex,New-SecureNonce,Invoke-ClaimedCommand,New-InitialEventState,Write-EventState,Publish-EventRelayReadiness,Test-EventRelayReadinessReceipt,New-EventRelayFileSnapshots,Restore-EventRelayFileSnapshots
