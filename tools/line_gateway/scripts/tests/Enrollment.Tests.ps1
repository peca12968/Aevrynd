Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\Enrollment.Common.ps1')

Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

public sealed class EnrollmentControlledAsyncStream : Stream
{
    private readonly byte[] data;
    private readonly int delayMilliseconds;
    private readonly int chunkSize;
    private readonly bool honorCancellation;
    private int position;

    public EnrollmentControlledAsyncStream(byte[] data, int delayMilliseconds, int chunkSize, bool honorCancellation)
    {
        this.data = data;
        this.delayMilliseconds = delayMilliseconds;
        this.chunkSize = chunkSize;
        this.honorCancellation = honorCancellation;
    }

    public override async Task<int> ReadAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken)
    {
        if (delayMilliseconds > 0)
        {
            if (honorCancellation)
                await Task.Delay(delayMilliseconds, cancellationToken).ConfigureAwait(false);
            else
                await Task.Delay(delayMilliseconds).ConfigureAwait(false);
        }
        if (position >= data.Length) return 0;
        int length = Math.Min(Math.Min(count, chunkSize), data.Length - position);
        Array.Copy(data, position, buffer, offset, length);
        position += length;
        return length;
    }

    public override int Read(byte[] buffer, int offset, int count) => throw new NotSupportedException("Synchronous read is forbidden by this fixture.");
    public override bool CanRead => true;
    public override bool CanSeek => false;
    public override bool CanWrite => false;
    public override long Length => throw new NotSupportedException();
    public override long Position { get => throw new NotSupportedException(); set => throw new NotSupportedException(); }
    public override void Flush() { }
    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
    public override void SetLength(long value) => throw new NotSupportedException();
    public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
}
'@

$passed=0
$failed=0
function Sign([byte[]]$Body,[string]$Secret){
    $secretBytes=[Text.Encoding]::UTF8.GetBytes($Secret)
    $h=[Security.Cryptography.HMACSHA256]::new($secretBytes)
    try{return [Convert]::ToBase64String($h.ComputeHash($Body))}finally{$h.Dispose();[Array]::Clear($secretBytes,0,$secretBytes.Length)}
}
function Run([string]$Name,[scriptblock]$Action){
    try{&$Action;$script:passed++;"PASS $Name"}catch{$script:failed++;"FAIL $Name : $($_.Exception.GetType().FullName) : $($_.Exception.Message)"}
}
function Reject([type]$ExpectedType,[string]$ExpectedMessage,[scriptblock]$Action){
    try{&$Action|Out-Null}catch{
        $actual=$_.Exception
        while($null-ne$actual.InnerException-and$actual-isnot$ExpectedType){$actual=$actual.InnerException}
        if(-not$ExpectedType.IsAssignableFrom($actual.GetType())){throw "Expected $($ExpectedType.FullName), received $($actual.GetType().FullName)."}
        if(-not[string]::Equals($actual.Message,$ExpectedMessage,[StringComparison]::Ordinal)){throw "Expected '$ExpectedMessage', received '$($actual.Message)'."}
        return
    }
    throw 'Hostile fixture was accepted.'
}

$runtimeError=[Management.Automation.RuntimeException]
$invalidData=[IO.InvalidDataException]
$timeoutError=[TimeoutException]
$secret='fixture-channel-secret-32-bytes'
$developer='U'+'1'*32
$group='C'+'2'*32
$nonce='a'*64
$display='@李氏集團秘書'
$text="$display enroll $nonce"
$mention=@{type='user';isSelf=$true;index=0;length=$display.Length}
function Body([object]$Mention=$script:mention,[string]$User=$script:developer,[string]$Group=$script:group,[string]$Text=$script:text,[string]$SourceType='group'){
    $doc=@{events=@(@{type='message';source=@{type=$SourceType;userId=$User;groupId=$Group};message=@{type='text';text=$Text;mention=@{mentionees=@($Mention)}}})}
    return [Text.Encoding]::UTF8.GetBytes(($doc|ConvertTo-Json -Depth 8 -Compress))
}

Run 'accept signed developer self-mention enrollment' {
    $b=Body
    $r=Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce
    if($r.allowedGroupIds[0]-ne$group){throw 'Wrong group captured.'}
}
Run 'reject bad signature with exact reason' {
    $b=Body
    Reject $runtimeError 'LINE signature verification failed.' {Test-EnrollmentWebhook $b (Sign $b 'wrong-secret-value') $secret $developer $nonce}
}
Run 'reject malformed signature with exact reason' {
    $b=Body
    Reject $runtimeError 'LINE signature is malformed.' {Test-EnrollmentWebhook $b '***' $secret $developer $nonce}
}
Run 'reject wrong developer with exact reason' {
    $b=Body -User ('U'+'3'*32)
    Reject $runtimeError 'Enrollment sender is not the allowlisted developer.' {Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}
}
Run 'reject nonce mismatch with exact reason' {
    $b=Body -Text "$display enroll $('b'*64)"
    Reject $runtimeError 'Enrollment nonce command does not match.' {Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}
}
Run 'reject textual fake without self mention with exact reason' {
    $b=Body -Mention @{type='user';isSelf=$false;index=0;length=$display.Length}
    Reject $runtimeError 'Enrollment requires a verified self mention.' {Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}
}
Run 'reject direct chat source with exact reason' {
    $b=Body -SourceType 'user'
    Reject $runtimeError 'Enrollment is only valid inside a LINE group.' {Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}
}
Run 'reject overlapping mentions with exact reason' {
    $m=@($mention,@{type='user';isSelf=$false;index=1;length=2})
    $b=Body -Mention $m
    Reject $runtimeError 'Enrollment requires exactly one mention.' {Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}
}
foreach($case in @(
    @{name='string index';mention=@{type='user';isSelf=$true;index='0';length=$display.Length};message='Mention index and length must be JSON integers.'},
    @{name='null length';mention=@{type='user';isSelf=$true;index=0;length=$null};message='Mention index and length must be JSON integers.'},
    @{name='boolean index';mention=@{type='user';isSelf=$true;index=$false;length=$display.Length};message='Mention index and length must be JSON integers.'},
    @{name='string isSelf';mention=@{type='user';isSelf='true';index=0;length=$display.Length};message='Enrollment requires a verified self mention.'},
    @{name='missing isSelf';mention=@{type='user';index=0;length=$display.Length};message='Mention is missing required field: isSelf'},
    @{name='wrong type';mention=@{type='all';isSelf=$true;index=0;length=$display.Length};message='Enrollment mention type must be user.'},
    @{name='overflow range';mention=@{type='user';isSelf=$true;index=0;length=[long]::MaxValue};message='Mention range is outside the UTF-16 text.'}
)){
    Run "reject $($case.name) with exact reason" {
        $b=Body -Mention $case.mention
        Reject $runtimeError $case.message {Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}
    }
}

Run 'bounded async reader accepts declared body' {
    $source=[IO.MemoryStream]::new([byte[]](1..64))
    try{
        $read=Read-BoundedWebhookBody -Stream $source -DeclaredLength 64 -MaximumBytes 65536 -EnrollmentDeadlineUtc ([DateTimeOffset]::UtcNow.AddSeconds(5)) -RequestTimeoutMilliseconds 1000
        if($read.Length-ne64){throw 'Wrong bounded length.'}
    }finally{$source.Dispose()}
}
Run 'bounded async reader rejects declared oversize before read' {
    $source=[IO.MemoryStream]::new([byte[]](1..8))
    try{
        Reject $invalidData 'Webhook body exceeds enrollment policy.' {Read-BoundedWebhookBody -Stream $source -DeclaredLength 65537 -MaximumBytes 65536 -EnrollmentDeadlineUtc ([DateTimeOffset]::UtcNow.AddSeconds(5))}
        if($source.Position-ne0){throw 'Oversize declared body was read.'}
    }finally{$source.Dispose()}
}
Run 'bounded async reader rejects chunked-style oversize' {
    $source=[IO.MemoryStream]::new([byte[]]::new(65537))
    try{Reject $invalidData 'Webhook body exceeds enrollment policy.' {Read-BoundedWebhookBody -Stream $source -DeclaredLength -1 -MaximumBytes 65536 -EnrollmentDeadlineUtc ([DateTimeOffset]::UtcNow.AddSeconds(5))}}finally{$source.Dispose()}
}
Run 'bounded async reader rejects declared length mismatch' {
    $source=[IO.MemoryStream]::new([byte[]](1..8))
    try{Reject $invalidData 'Webhook body length does not match Content-Length.' {Read-BoundedWebhookBody -Stream $source -DeclaredLength 9 -MaximumBytes 65536 -EnrollmentDeadlineUtc ([DateTimeOffset]::UtcNow.AddSeconds(5))}}finally{$source.Dispose()}
}
Run 'non-cooperative slow stream obeys per-request deadline' {
    $source=[EnrollmentControlledAsyncStream]::new([byte[]](1..8),500,1,$false)
    $watch=[Diagnostics.Stopwatch]::StartNew()
    try{Reject $timeoutError 'Webhook body read timed out.' {Read-BoundedWebhookBody -Stream $source -DeclaredLength -1 -MaximumBytes 65536 -EnrollmentDeadlineUtc ([DateTimeOffset]::UtcNow.AddSeconds(5)) -RequestTimeoutMilliseconds 100}}finally{$source.Dispose();$watch.Stop()}
    if($watch.ElapsedMilliseconds-ge1000){throw "Per-request deadline was not bounded: $($watch.ElapsedMilliseconds) ms."}
}
Run 'slow stream obeys enrollment absolute deadline' {
    $source=[EnrollmentControlledAsyncStream]::new([byte[]](1..8),500,1,$true)
    try{Reject $timeoutError 'Enrollment deadline expired while reading webhook body.' {Read-BoundedWebhookBody -Stream $source -DeclaredLength -1 -MaximumBytes 65536 -EnrollmentDeadlineUtc ([DateTimeOffset]::UtcNow.AddMilliseconds(100)) -RequestTimeoutMilliseconds 1000}}finally{$source.Dispose()}
}
Run 'valid request can continue after timed-out rejection' {
    $slow=[EnrollmentControlledAsyncStream]::new([byte[]](1..8),500,1,$false)
    try{Reject $timeoutError 'Webhook body read timed out.' {Read-BoundedWebhookBody -Stream $slow -DeclaredLength -1 -MaximumBytes 65536 -EnrollmentDeadlineUtc ([DateTimeOffset]::UtcNow.AddSeconds(5)) -RequestTimeoutMilliseconds 100}}finally{$slow.Dispose()}
    $valid=[IO.MemoryStream]::new([byte[]](1..8))
    try{
        $read=Read-BoundedWebhookBody -Stream $valid -DeclaredLength 8 -MaximumBytes 65536 -EnrollmentDeadlineUtc ([DateTimeOffset]::UtcNow.AddSeconds(5)) -RequestTimeoutMilliseconds 1000
        if($read.Length-ne8){throw 'Valid request did not continue after a timeout.'}
    }finally{$valid.Dispose()}
}

if($failed){throw "$failed enrollment tests failed; $passed passed."}
"Enrollment hostile tests passed ($passed/22)."
