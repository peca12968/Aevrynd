Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

if(-not('AevryndEnrollmentDacl'-as[type])){Add-Type @'
using System;using System.ComponentModel;using System.Runtime.InteropServices;
public static class AevryndEnrollmentDacl {
 const uint DACL_SECURITY_INFORMATION=0x00000004,PROTECTED_DACL_SECURITY_INFORMATION=0x80000000;
 [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool SetFileSecurityW(string path,uint info,byte[] descriptor);
 public static void Set(string path,string sid,bool directory){var sddl=directory?"D:P(A;OICI;FA;;;"+sid+")":"D:P(A;;FA;;;"+sid+")";var raw=new System.Security.AccessControl.RawSecurityDescriptor(sddl);var bytes=new byte[raw.BinaryLength];raw.GetBinaryForm(bytes,0);try{if(!SetFileSecurityW(path,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,bytes))throw new Win32Exception(Marshal.GetLastWin32Error());}finally{Array.Clear(bytes,0,bytes.Length);}}
}
'@}

function Set-EnrollmentOwnerOnlyDacl {
    param([Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][Security.Principal.SecurityIdentifier]$OwnerSid,[switch]$Directory)
    $item=Get-Item -LiteralPath $Path -Force
    if(($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'Enrollment capture path must not be a reparse point.'}
    $owner=(Get-Acl -LiteralPath $Path).Owner;try{$actual=([Security.Principal.NTAccount]$owner).Translate([Security.Principal.SecurityIdentifier])}catch{$actual=[Security.Principal.SecurityIdentifier]::new($owner)}
    if($actual.Value-cne$OwnerSid.Value){throw 'Enrollment capture path is not owned by the current user.'}
    [AevryndEnrollmentDacl]::Set([IO.Path]::GetFullPath($Path),$OwnerSid.Value,$Directory.IsPresent)
    $acl=Get-Acl -LiteralPath $Path;if(-not$acl.AreAccessRulesProtected){throw 'Enrollment capture DACL is not protected.'}
    foreach($rule in $acl.Access){if($rule.AccessControlType-ne[Security.AccessControl.AccessControlType]::Allow-or$rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value-cne$OwnerSid.Value){throw 'Enrollment capture DACL grants access outside the owner.'}}
}

function Test-FixedTimeBytes {
    param([byte[]]$Actual,[byte[]]$Expected)
    if($Actual.Length-ne$Expected.Length){return $false}
    return [Security.Cryptography.CryptographicOperations]::FixedTimeEquals($Actual,$Expected)
}

function Read-BoundedWebhookBody {
    param(
        [Parameter(Mandatory)][IO.Stream]$Stream,
        [long]$DeclaredLength=-1,
        [ValidateRange(1,65536)][int]$MaximumBytes=65536,
        [Parameter(Mandatory)][DateTimeOffset]$EnrollmentDeadlineUtc,
        [ValidateRange(100,10000)][int]$RequestTimeoutMilliseconds=3000
    )
    if($DeclaredLength-lt-1){throw [IO.InvalidDataException]::new('Webhook Content-Length is invalid.')}
    if($DeclaredLength-gt$MaximumBytes){throw [IO.InvalidDataException]::new('Webhook body exceeds enrollment policy.')}
    $requestDeadlineUtc=[DateTimeOffset]::UtcNow.AddMilliseconds($RequestTimeoutMilliseconds)
    $effectiveDeadlineUtc=$(if($requestDeadlineUtc-lt$EnrollmentDeadlineUtc){$requestDeadlineUtc}else{$EnrollmentDeadlineUtc})
    $buffer=[byte[]]::new(8192)
    $memory=[IO.MemoryStream]::new([Math]::Min($(if($DeclaredLength-ge0){[int]$DeclaredLength}else{$MaximumBytes}),$MaximumBytes))
    $bufferMayBeCleared=$true
    try{
        while($true){
            $remainingMilliseconds=[int][Math]::Ceiling(($effectiveDeadlineUtc-[DateTimeOffset]::UtcNow).TotalMilliseconds)
            if($remainingMilliseconds-le0){
                if([DateTimeOffset]::UtcNow-ge$EnrollmentDeadlineUtc){throw [TimeoutException]::new('Enrollment deadline expired while reading webhook body.')}
                throw [TimeoutException]::new('Webhook body read timed out.')
            }
            $readCancellation=[Threading.CancellationTokenSource]::new()
            $readTask=$null
            try{
                $readTask=$Stream.ReadAsync($buffer,0,$buffer.Length,$readCancellation.Token)
                $timeoutTask=[Threading.Tasks.Task]::Delay($remainingMilliseconds)
                $completed=[Threading.Tasks.Task]::WhenAny([Threading.Tasks.Task[]]@($readTask,$timeoutTask)).GetAwaiter().GetResult()
                if(-not[object]::ReferenceEquals($completed,$readTask)){
                    $readCancellation.Cancel()
                    try{$Stream.Dispose()}catch{}
                    $bufferMayBeCleared=$readTask.IsCompleted
                    if([DateTimeOffset]::UtcNow-ge$EnrollmentDeadlineUtc){throw [TimeoutException]::new('Enrollment deadline expired while reading webhook body.')}
                    throw [TimeoutException]::new('Webhook body read timed out.')
                }
                $read=$readTask.GetAwaiter().GetResult()
            }finally{$readCancellation.Dispose()}
            if([DateTimeOffset]::UtcNow-ge$effectiveDeadlineUtc){
                if([DateTimeOffset]::UtcNow-ge$EnrollmentDeadlineUtc){throw [TimeoutException]::new('Enrollment deadline expired while reading webhook body.')}
                throw [TimeoutException]::new('Webhook body read timed out.')
            }
            if($read-eq0){break}
            if($memory.Length+$read-gt$MaximumBytes){throw [IO.InvalidDataException]::new('Webhook body exceeds enrollment policy.')}
            $memory.Write($buffer,0,$read)
        }
        if($DeclaredLength-ge0-and$memory.Length-ne$DeclaredLength){throw [IO.InvalidDataException]::new('Webhook body length does not match Content-Length.')}
        return $memory.ToArray()
    }finally{
        $memory.Dispose()
        if($bufferMayBeCleared){[Array]::Clear($buffer,0,$buffer.Length)}
    }
}

function Test-JsonInteger {
    param([object]$Value)
    return $null-ne$Value-and$Value-isnot[bool]-and($Value-is[int]-or$Value-is[long])
}

function Assert-LineWebhookSignature {
    param([Parameter(Mandatory)][byte[]]$RawBody,[Parameter(Mandatory)][string]$Signature,[Parameter(Mandatory)][string]$ChannelSecret)
    if($RawBody.Length-lt2-or$RawBody.Length-gt65536){throw 'Webhook body size is outside enrollment policy.'}
    if($ChannelSecret.Length-lt16){throw 'Channel secret is invalid.'}
    try{$provided=[Convert]::FromBase64String($Signature)}catch{throw 'LINE signature is malformed.'}
    $secretBytes=[Text.Encoding]::UTF8.GetBytes($ChannelSecret);$hmac=[Security.Cryptography.HMACSHA256]::new($secretBytes)
    try{$expected=$hmac.ComputeHash($RawBody)}finally{$hmac.Dispose();[Array]::Clear($secretBytes,0,$secretBytes.Length)}
    if(-not(Test-FixedTimeBytes $provided $expected)){throw 'LINE signature verification failed.'}
}

function Test-LineWebhookPreflight {
    param([Parameter(Mandatory)][byte[]]$RawBody,[Parameter(Mandatory)][string]$Signature,[Parameter(Mandatory)][string]$ChannelSecret)
    Assert-LineWebhookSignature $RawBody $Signature $ChannelSecret
    $document=$null
    try{
        $options=[Text.Json.JsonDocumentOptions]::new();$options.AllowTrailingCommas=$false;$options.CommentHandling=[Text.Json.JsonCommentHandling]::Disallow;$options.MaxDepth=8
        $json=[Text.UTF8Encoding]::new($false,$true).GetString($RawBody);$document=[Text.Json.JsonDocument]::Parse($json,$options);$root=$document.RootElement
        if($root.ValueKind-ne[Text.Json.JsonValueKind]::Object){return $false}
        $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($property in $root.EnumerateObject()){if(-not$names.Add($property.Name)){throw 'LINE webhook JSON contains duplicate envelope fields.'}}
        if($names.Count-ne2-or-not$names.Contains('destination')-or-not$names.Contains('events')){return $false}
        $destination=$root.GetProperty('destination');$events=$root.GetProperty('events')
        if($destination.ValueKind-ne[Text.Json.JsonValueKind]::String-or$destination.GetString()-notmatch'^U[0-9a-fA-F]{32}$'){return $false}
        return $events.ValueKind-eq[Text.Json.JsonValueKind]::Array-and$events.GetArrayLength()-eq0
    }catch [Management.Automation.MethodInvocationException]{throw 'Webhook JSON is invalid.'}finally{if($null-ne$document){$document.Dispose()}}
}

function Test-EnrollmentWebhook {
    param(
        [Parameter(Mandatory)][byte[]]$RawBody,
        [Parameter(Mandatory)][string]$Signature,
        [Parameter(Mandatory)][string]$ChannelSecret,
        [Parameter(Mandatory)][string]$DeveloperUserId,
        [Parameter(Mandatory)][string]$Nonce
    )
    Assert-LineWebhookSignature $RawBody $Signature $ChannelSecret
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
