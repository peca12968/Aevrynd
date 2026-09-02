Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Test-FixedTimeBytes {
    param([byte[]]$Actual,[byte[]]$Expected)
    if($Actual.Length-ne$Expected.Length){return $false}
    return [Security.Cryptography.CryptographicOperations]::FixedTimeEquals($Actual,$Expected)
}

function Read-BoundedWebhookBody {
    param(
        [Parameter(Mandatory)][IO.Stream]$Stream,
        [long]$DeclaredLength=-1,
        [ValidateRange(1,65536)][int]$MaximumBytes=65536
    )
    if($DeclaredLength-lt-1){throw [IO.InvalidDataException]::new('Webhook Content-Length is invalid.')}
    if($DeclaredLength-gt$MaximumBytes){throw [IO.InvalidDataException]::new('Webhook body exceeds enrollment policy.')}
    $buffer=[byte[]]::new(8192)
    $memory=[IO.MemoryStream]::new([Math]::Min($(if($DeclaredLength-ge0){[int]$DeclaredLength}else{$MaximumBytes}),$MaximumBytes))
    try{
        while($true){
            $read=$Stream.Read($buffer,0,$buffer.Length)
            if($read-eq0){break}
            if($memory.Length+$read-gt$MaximumBytes){throw [IO.InvalidDataException]::new('Webhook body exceeds enrollment policy.')}
            $memory.Write($buffer,0,$read)
        }
        if($DeclaredLength-ge0-and$memory.Length-ne$DeclaredLength){throw [IO.InvalidDataException]::new('Webhook body length does not match Content-Length.')}
        return $memory.ToArray()
    }finally{$memory.Dispose();[Array]::Clear($buffer,0,$buffer.Length)}
}

function Test-JsonInteger {
    param([object]$Value)
    return $null-ne$Value-and$Value-isnot[bool]-and($Value-is[int]-or$Value-is[long])
}

function Test-EnrollmentWebhook {
    param(
        [Parameter(Mandatory)][byte[]]$RawBody,
        [Parameter(Mandatory)][string]$Signature,
        [Parameter(Mandatory)][string]$ChannelSecret,
        [Parameter(Mandatory)][string]$DeveloperUserId,
        [Parameter(Mandatory)][string]$Nonce
    )
    if($RawBody.Length-lt2-or$RawBody.Length-gt65536){throw 'Webhook body size is outside enrollment policy.'}
    if($ChannelSecret.Length-lt16){throw 'Channel secret is invalid.'}
    try{$provided=[Convert]::FromBase64String($Signature)}catch{throw 'LINE signature is malformed.'}
    $hmac=[Security.Cryptography.HMACSHA256]::new([Text.Encoding]::UTF8.GetBytes($ChannelSecret))
    try{$expected=$hmac.ComputeHash($RawBody)}finally{$hmac.Dispose()}
    if(-not(Test-FixedTimeBytes $provided $expected)){throw 'LINE signature verification failed.'}
    try{$document=[Text.Encoding]::UTF8.GetString($RawBody)|ConvertFrom-Json -ErrorAction Stop}catch{throw 'Webhook JSON is invalid.'}
    $events=@($document.events)
    if($events.Count-ne1){throw 'Enrollment requires exactly one webhook event.'}
    $event=$events[0]
    if($event.type-ne'message'-or$event.message.type-ne'text'){throw 'Enrollment requires one text message event.'}
    if($event.source.type-ne'group'){throw 'Enrollment is only valid inside a LINE group.'}
    $userId=[string]$event.source.userId;$groupId=[string]$event.source.groupId
    if($userId-notmatch'^U[0-9a-fA-F]{32}$'-or$groupId-notmatch'^C[0-9a-fA-F]{32}$'){throw 'LINE source identifiers are malformed.'}
    if(-not[string]::Equals($userId,$DeveloperUserId,[StringComparison]::Ordinal)){throw 'Enrollment sender is not the allowlisted developer.'}
    $text=[string]$event.message.text
    $mentions=@($event.message.mention.mentionees)
    if($mentions.Count-ne1){throw 'Enrollment requires exactly one mention.'}
    $self=$mentions[0];$properties=@($self.PSObject.Properties.Name)
    foreach($required in @('index','length','type','isSelf')){if($required-notin$properties){throw "Mention is missing required field: $required"}}
    if(-not(Test-JsonInteger $self.index)-or-not(Test-JsonInteger $self.length)){throw 'Mention index and length must be JSON integers.'}
    if($self.type-isnot[string]-or-not[string]::Equals([string]$self.type,'user',[StringComparison]::Ordinal)){throw 'Enrollment mention type must be user.'}
    if($self.isSelf-isnot[bool]-or$self.isSelf-ne$true){throw 'Enrollment requires a verified self mention.'}
    $index=[long]$self.index;$length=[long]$self.length
    if($index-lt0-or$length-lt1-or$index-gt$text.Length-or$length-gt$text.Length-$index){throw 'Mention range is outside the UTF-16 text.'}
    $command=($text.Remove([int]$index,[int]$length)).Trim()
    if(-not[string]::Equals($command,"enroll $Nonce",[StringComparison]::Ordinal)){throw 'Enrollment nonce command does not match.'}
    return [pscustomobject]@{schemaVersion=1;allowedUserIds=@($userId);allowedGroupIds=@($groupId);capturedUtc=[DateTimeOffset]::UtcNow.ToString('o')}
}
