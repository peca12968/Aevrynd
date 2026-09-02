Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\Enrollment.Common.ps1')
$passed=0;$failed=0
function Sign([byte[]]$Body,[string]$Secret){$h=[Security.Cryptography.HMACSHA256]::new([Text.Encoding]::UTF8.GetBytes($Secret));try{return [Convert]::ToBase64String($h.ComputeHash($Body))}finally{$h.Dispose()}}
function Run([string]$Name,[scriptblock]$Action){try{&$Action;$script:passed++;"PASS $Name"}catch{$script:failed++;"FAIL $Name : $($_.Exception.Message)"}}
function Reject([scriptblock]$Action){$rejected=$false;try{&$Action|Out-Null}catch{$rejected=$true};if(-not$rejected){throw 'Hostile fixture was accepted.'}}
$secret='fixture-channel-secret-32-bytes';$developer='U'+'1'*32;$group='C'+'2'*32;$nonce='a'*64;$display='@李氏集團秘書';$text="$display enroll $nonce";$mention=@{type='user';isSelf=$true;index=0;length=$display.Length}
function Body([object]$Mention=$script:mention,[string]$User=$script:developer,[string]$Group=$script:group,[string]$Text=$script:text,[string]$SourceType='group'){$doc=@{events=@(@{type='message';source=@{type=$SourceType;userId=$User;groupId=$Group};message=@{type='text';text=$Text;mention=@{mentionees=@($Mention)}}})};return [Text.Encoding]::UTF8.GetBytes(($doc|ConvertTo-Json -Depth 8 -Compress))}
Run 'accept signed developer self-mention enrollment' {$b=Body;$r=Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce;if($r.allowedGroupIds[0]-ne$group){throw 'Wrong group captured.'}}
Run 'reject bad signature' {$b=Body;Reject{Test-EnrollmentWebhook $b (Sign $b 'wrong-secret-value') $secret $developer $nonce}}
Run 'reject wrong developer' {$b=Body -User ('U'+'3'*32);Reject{Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}}
Run 'reject nonce mismatch' {$b=Body -Text "$display enroll $('b'*64)";Reject{Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}}
Run 'reject textual fake without self mention' {$b=Body -Mention @{type='user';isSelf=$false;index=0;length=$display.Length};Reject{Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}}
Run 'reject direct chat source' {$b=Body -SourceType 'user';Reject{Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}}
Run 'reject overlapping mentions' {$m=@($mention,@{type='user';isSelf=$false;index=1;length=2});$b=Body -Mention $m;Reject{Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}}
foreach($case in @(
    @{name='string index';mention=@{type='user';isSelf=$true;index='0';length=$display.Length}},
    @{name='null length';mention=@{type='user';isSelf=$true;index=0;length=$null}},
    @{name='boolean index';mention=@{type='user';isSelf=$true;index=$false;length=$display.Length}},
    @{name='string isSelf';mention=@{type='user';isSelf='true';index=0;length=$display.Length}},
    @{name='missing isSelf';mention=@{type='user';index=0;length=$display.Length}},
    @{name='wrong type';mention=@{type='all';isSelf=$true;index=0;length=$display.Length}},
    @{name='overflow range';mention=@{type='user';isSelf=$true;index=0;length=[long]::MaxValue}}
)){
    Run "reject $($case.name)" {$b=Body -Mention $case.mention;Reject{Test-EnrollmentWebhook $b (Sign $b $secret) $secret $developer $nonce}}
}
Run 'bounded reader accepts declared body' {$source=[IO.MemoryStream]::new([byte[]](1..64));try{$read=Read-BoundedWebhookBody $source 64 65536;if($read.Length-ne64){throw 'Wrong bounded length.'}}finally{$source.Dispose()}}
Run 'bounded reader rejects declared oversize before read' {$source=[IO.MemoryStream]::new([byte[]](1..8));try{Reject{Read-BoundedWebhookBody $source 65537 65536};if($source.Position-ne0){throw 'Oversize declared body was read.'}}finally{$source.Dispose()}}
Run 'bounded reader rejects chunked-style oversize' {$source=[IO.MemoryStream]::new([byte[]]::new(65537));try{Reject{Read-BoundedWebhookBody $source -1 65536}}finally{$source.Dispose()}}
if($failed){throw "$failed enrollment tests failed; $passed passed."};"Enrollment hostile tests passed ($passed/17)."
