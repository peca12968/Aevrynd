param(
    [Parameter(Mandatory)][ValidatePattern('^U[0-9a-fA-F]{32}$')][string]$DeveloperUserId,
    [string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway",
    [ValidateRange(30,300)][int]$DurationSeconds=120
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Enrollment.Common.ps1')
. (Join-Path $PSScriptRoot 'Funnel.Common.ps1')
Assert-PowerShellSeven
$transportLock=Enter-PublicTransportLock
$listener=$null
try{
$paths=Get-GatewayPaths -InstallRoot $InstallRoot
$namedTunnelActive=$false
$state=Read-TunnelState -Paths $paths
if($null-ne$state){
    $configuration=Read-NamedTunnelConfiguration -ConfigPath $paths.TunnelConfigFile
    $tunnel=Test-TunnelProcess -Paths $paths -State $state
    if($null-eq$tunnel-or-not[string]::Equals([string]$state.configSha256,[string]$configuration.ConfigSha256,[StringComparison]::Ordinal)){throw 'Cloudflare tunnel state is present but cannot be verified.'}
    $namedTunnelActive=$true
}elseif(@(Get-CimInstance Win32_Process -Filter "Name = 'cloudflared.exe'" -ErrorAction SilentlyContinue).Count-ne0){throw 'An unmanaged cloudflared process prevents transport selection.'}
$funnelActive=$false
$funnelState=Read-FunnelState -Paths $paths
if($null-eq$funnelState){
    Test-FunnelTransportAbsent
}else{
    $funnel=& (Join-Path $PSScriptRoot 'Test-FunnelConfiguration.ps1') -InstallRoot $InstallRoot
    if($funnel.Mode-eq'active'){
        if($null-eq$funnelState-or$funnelState.status-ne'active'){throw 'An active Funnel is not bound to active Aevrynd state.'}
        Assert-FunnelIdentityMatchesState $funnelState $funnel $funnel
        $funnelActive=$true
    }else{throw 'Funnel state exists while authoritative status is disabled.'}
}
$ngrokActive=$false
if(Test-Path -LiteralPath $paths.NgrokStateFile){$ngrok=& (Join-Path $PSScriptRoot 'Test-NgrokConfiguration.ps1') -InstallRoot $InstallRoot;if($ngrok.Mode-ne'active'){throw 'Ngrok state exists but its endpoint cannot be verified.'};$ngrokActive=$true}elseif(@(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue).Count-ne0){throw 'An unmanaged ngrok process prevents transport selection.'}
if(@(@($namedTunnelActive,$funnelActive,$ngrokActive)|Where-Object{$_}).Count-ne1){throw 'Enrollment requires exactly one verified public transport: Cloudflare, Tailscale Funnel, or ngrok.'}
if(Test-NetConnection 127.0.0.1 -Port 8787 -InformationLevel Quiet -WarningAction SilentlyContinue){throw 'Port 8787 is already in use; stop Gateway before one-shot enrollment.'}
$secret=Read-Host 'Current LINE Channel Secret' -AsSecureString
$pointer=[Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secret)
try{$channelSecret=[Runtime.InteropServices.Marshal]::PtrToStringUni($pointer)}finally{[Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer);$secret.Dispose()}
$bytes=[byte[]]::new(32);[Security.Cryptography.RandomNumberGenerator]::Fill($bytes);$nonce=[Convert]::ToHexString($bytes).ToLowerInvariant();[Array]::Clear($bytes,0,$bytes.Length)
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$captureRoot=Join-Path $env:LOCALAPPDATA 'Aevrynd\LineGatewayEnrollment';if(-not(Test-Path -LiteralPath $captureRoot)){New-Item -ItemType Directory -Path $captureRoot|Out-Null};Set-EnrollmentOwnerOnlyDacl -Path $captureRoot -OwnerSid $identity.User -Directory
$capturePath=Join-Path $captureRoot "allowlist-$([Guid]::NewGuid().ToString('N')).json"
$currentPath=Join-Path $captureRoot 'current-enrollment.json';if(Test-Path -LiteralPath $currentPath){throw 'A current enrollment state already exists; refuse ambiguous enrollment ownership.'}
$deadline=[DateTimeOffset]::UtcNow.AddSeconds($DurationSeconds);$currentDocument=[ordered]@{schemaVersion=1;nonce=$nonce;expiresUtc=$deadline.ToString('o',[Globalization.CultureInfo]::InvariantCulture);pid=$PID};$currentBytes=[Text.UTF8Encoding]::new($false).GetBytes(($currentDocument|ConvertTo-Json -Compress))
try{$current=[IO.FileStream]::new($currentPath,'CreateNew','Write','None');try{$current.Write($currentBytes,0,$currentBytes.Length);$current.Flush($true)}finally{$current.Dispose()}}finally{[Array]::Clear($currentBytes,0,$currentBytes.Length)};Set-EnrollmentOwnerOnlyDacl -Path $currentPath -OwnerSid $identity.User
try{
$listener=[Net.HttpListener]::new();$listener.Prefixes.Add('http://127.0.0.1:8787/');$listener.Start()
Write-Output 'ENROLLMENT_LISTENER_READY http://127.0.0.1:8787/webhook'
Write-Output "For the next $DurationSeconds seconds, send exactly: [mention this bot] enroll $nonce"
    $rejectedRequests=0
    $maximumRejectedRequests=32
    while([DateTimeOffset]::UtcNow-lt$deadline){
        $remaining=[int][Math]::Max(1,($deadline-[DateTimeOffset]::UtcNow).TotalMilliseconds)
        $task=$listener.GetContextAsync();if(-not$task.Wait($remaining)){break};$context=$task.Result
        $rejectionStatus=0
        try{
            if($context.Request.HttpMethod-ne'POST'-or$context.Request.Url.AbsolutePath-ne'/webhook'){throw 'Enrollment only accepts POST /webhook.'}
            if($context.Request.ContentLength64-gt65536){throw [IO.InvalidDataException]::new('Webhook body exceeds enrollment policy.')}
            $raw=Read-BoundedWebhookBody -Stream $context.Request.InputStream -DeclaredLength $context.Request.ContentLength64 -MaximumBytes 65536 -EnrollmentDeadlineUtc $deadline -RequestTimeoutMilliseconds 3000
            if(Test-LineWebhookPreflight -RawBody $raw -Signature ([string]$context.Request.Headers['X-Line-Signature']) -ChannelSecret $channelSecret){$context.Response.StatusCode=200;$context.Response.Close();continue}
            $result=Test-EnrollmentWebhook -RawBody $raw -Signature ([string]$context.Request.Headers['X-Line-Signature']) -ChannelSecret $channelSecret -DeveloperUserId $DeveloperUserId -Nonce $nonce
            $payload=[Text.UTF8Encoding]::new($false).GetBytes(($result|ConvertTo-Json -Depth 4));try{$capture=[IO.FileStream]::new($capturePath,'CreateNew','Write','None');try{$capture.Write($payload,0,$payload.Length);$capture.Flush($true)}finally{$capture.Dispose()}}finally{[Array]::Clear($payload,0,$payload.Length)};Set-EnrollmentOwnerOnlyDacl -Path $capturePath -OwnerSid $identity.User
            $context.Response.StatusCode=200;$context.Response.Close();Write-Output "Allowlist IDs captured to protected file: $capturePath";exit 0
        }catch [TimeoutException]{$rejectionStatus=408
        }catch [IO.InvalidDataException]{$rejectionStatus=413
        }catch{$rejectionStatus=403}
        if($rejectionStatus-ne0){
            try{$context.Response.StatusCode=$rejectionStatus;$context.Response.Close()}catch{}
            $rejectedRequests++
            if($rejectedRequests-ge$maximumRejectedRequests){throw 'Enrollment stopped after too many rejected requests.'}
            Start-Sleep -Milliseconds ([Math]::Min(200,25*$rejectedRequests))
        }
    }
    throw 'Enrollment window expired without a valid signed event.'
}finally{$channelSecret=$null;if($null-ne$listener){if($listener.IsListening){$listener.Stop()};$listener.Close()};if(Test-Path -LiteralPath $currentPath -PathType Leaf){$item=Get-Item -LiteralPath $currentPath -Force;if(($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'Current enrollment state became a reparse point; refusing unsafe cleanup.'};$owner=(Get-Acl -LiteralPath $currentPath).Owner;$ownerSid=try{([Security.Principal.NTAccount]$owner).Translate([Security.Principal.SecurityIdentifier])}catch{[Security.Principal.SecurityIdentifier]::new($owner)};if($ownerSid.Value-cne$identity.User.Value){throw 'Current enrollment state ownership changed; refusing unsafe cleanup.'};Remove-Item -LiteralPath $currentPath -Force}}
}finally{Exit-PublicTransportLock $transportLock}
