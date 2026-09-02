param(
    [Parameter(Mandatory)][ValidatePattern('^U[0-9a-fA-F]{32}$')][string]$DeveloperUserId,
    [string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway",
    [ValidateRange(30,300)][int]$DurationSeconds=120
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Enrollment.Common.ps1')
Assert-PowerShellSeven
$paths=Get-GatewayPaths -InstallRoot $InstallRoot
$configuration=Read-NamedTunnelConfiguration -ConfigPath $paths.TunnelConfigFile
$state=Read-TunnelState -Paths $paths
$tunnel=Test-TunnelProcess -Paths $paths -State $state
if($null-eq$tunnel-or-not[string]::Equals([string]$state.configSha256,[string]$configuration.ConfigSha256,[StringComparison]::Ordinal)){throw 'A verified active named tunnel is required before enrollment.'}
if(Test-NetConnection 127.0.0.1 -Port 8787 -InformationLevel Quiet -WarningAction SilentlyContinue){throw 'Port 8787 is already in use; stop Gateway before one-shot enrollment.'}
$secret=Read-Host 'Current LINE Channel Secret' -AsSecureString
$pointer=[Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secret)
try{$channelSecret=[Runtime.InteropServices.Marshal]::PtrToStringUni($pointer)}finally{[Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer);$secret.Dispose()}
$bytes=[byte[]]::new(32);[Security.Cryptography.RandomNumberGenerator]::Fill($bytes);$nonce=[Convert]::ToHexString($bytes).ToLowerInvariant();[Array]::Clear($bytes,0,$bytes.Length)
$identity=[Security.Principal.WindowsIdentity]::GetCurrent();$acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetAccessRuleProtection($true,$false);$rule=[Security.AccessControl.FileSystemAccessRule]::new($identity.User,[Security.AccessControl.FileSystemRights]::FullControl,[Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit',[Security.AccessControl.PropagationFlags]::None,[Security.AccessControl.AccessControlType]::Allow);$acl.AddAccessRule($rule)
$captureRoot=Join-Path $env:LOCALAPPDATA 'Aevrynd\LineGatewayEnrollment';if(-not(Test-Path -LiteralPath $captureRoot)){New-Item -ItemType Directory -Path $captureRoot|Out-Null};Set-Acl -LiteralPath $captureRoot -AclObject $acl
$capturePath=Join-Path $captureRoot "allowlist-$([Guid]::NewGuid().ToString('N')).json"
Write-Output "For the next $DurationSeconds seconds, send exactly: [mention this bot] enroll $nonce"
$listener=[Net.HttpListener]::new();$listener.Prefixes.Add('http://127.0.0.1:8787/');$listener.Start()
try{
    $deadline=[DateTimeOffset]::UtcNow.AddSeconds($DurationSeconds)
    while([DateTimeOffset]::UtcNow-lt$deadline){
        $remaining=[int][Math]::Max(1,($deadline-[DateTimeOffset]::UtcNow).TotalMilliseconds)
        $task=$listener.GetContextAsync();if(-not$task.Wait($remaining)){break};$context=$task.Result
        try{
            if($context.Request.HttpMethod-ne'POST'-or$context.Request.Url.AbsolutePath-ne'/webhook'){throw 'Enrollment only accepts POST /webhook.'}
            if($context.Request.ContentLength64-gt65536){$context.Response.StatusCode=413;$context.Response.Close();continue}
            try {
                $raw=Read-BoundedWebhookBody -Stream $context.Request.InputStream -DeclaredLength $context.Request.ContentLength64 -MaximumBytes 65536
            } catch [IO.InvalidDataException] {
                $context.Response.StatusCode=413
                $context.Response.Close()
                continue
            }
            $result=Test-EnrollmentWebhook -RawBody $raw -Signature ([string]$context.Request.Headers['X-Line-Signature']) -ChannelSecret $channelSecret -DeveloperUserId $DeveloperUserId -Nonce $nonce
            [IO.File]::WriteAllText($capturePath,($result|ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false));Set-Acl -LiteralPath $capturePath -AclObject $acl
            $context.Response.StatusCode=200;$context.Response.Close();Write-Output "Allowlist IDs captured to protected file: $capturePath";exit 0
        }catch{$context.Response.StatusCode=403;$context.Response.Close()}
    }
    throw 'Enrollment window expired without a valid signed event.'
}finally{$channelSecret=$null;$listener.Stop();$listener.Close()}
