Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

$script:AevryndFunnelDnsName='desktop-2cp53ov-1.tail972fc3.ts.net'
$script:AevryndFunnelTarget='http://127.0.0.1:8787'
$script:AevryndFunnelPath='/webhook'
$script:AevryndFunnelPort=443

if(-not('Aevrynd.SafePath' -as[type])){
Add-Type @'
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
namespace Aevrynd {
  public sealed class ProcessResult { public int ExitCode; public string Output=""; public string Error=""; public bool TimedOut; public string[] AuthorizationUrls=Array.Empty<string>(); }
  public static class SafeProcessRunner {
    static readonly System.Text.RegularExpressions.Regex LoginUrl=new System.Text.RegularExpressions.Regex(@"https://login\.tailscale\.com/a/[A-Za-z0-9_-]{8,256}",System.Text.RegularExpressions.RegexOptions.CultureInvariant);
    public static ProcessResult Run(string path, string[] args, int timeoutMs, bool showAuthorizationHint) {
      var info=new System.Diagnostics.ProcessStartInfo(path){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
      foreach(var arg in args) info.ArgumentList.Add(arg);
      using(var process=new System.Diagnostics.Process()){process.StartInfo=info;var output=new System.Text.StringBuilder();var error=new System.Text.StringBuilder();var gate=new object();var seen=new System.Collections.Generic.HashSet<string>(StringComparer.Ordinal);bool oversized=false;
        Action<string,System.Text.StringBuilder> receive=(line,target)=>{if(line==null)return;lock(gate){if((long)output.Length+error.Length+line.Length+1>1048576){oversized=true;try{process.Kill(true);}catch{}return;}target.AppendLine(line);if(showAuthorizationHint)foreach(System.Text.RegularExpressions.Match m in LoginUrl.Matches(line))if(seen.Add(m.Value))Console.WriteLine("Open this official URL and approve Funnel in Tailscale Access Controls: "+m.Value);}};
        process.OutputDataReceived+=(s,e)=>receive(e.Data,output);process.ErrorDataReceived+=(s,e)=>receive(e.Data,error);
        if(!process.Start()) throw new InvalidOperationException("Tailscale CLI did not start.");process.BeginOutputReadLine();process.BeginErrorReadLine();bool exited=process.WaitForExit(timeoutMs);
        if(!exited){try{process.Kill(true);}catch{}}process.WaitForExit();if(oversized)throw new InvalidOperationException("Tailscale CLI output exceeds policy.");
        lock(gate){var urls=new string[seen.Count];seen.CopyTo(urls);return new ProcessResult{ExitCode=exited?process.ExitCode:-1,Output=output.ToString(),Error=error.ToString(),TimedOut=!exited,AuthorizationUrls=urls};}
      }
    }
  }
  public static class SafePath {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern SafeFileHandle CreateFile(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern uint GetFinalPathNameByHandle(SafeFileHandle hFile, System.Text.StringBuilder path, uint size, uint flags);
    public static string FinalPath(SafeFileHandle handle) {
      var b=new System.Text.StringBuilder(32768); uint n=GetFinalPathNameByHandle(handle,b,(uint)b.Capacity,0);
      if(n==0 || n>=b.Capacity) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
      string p=b.ToString(); return p.StartsWith(@"\\?\") ? p.Substring(4) : p;
    }
    public static SafeFileHandle OpenDirectory(string path) {
      var h=CreateFile(path,0,1|2,IntPtr.Zero,3,0x02000000,IntPtr.Zero);
      if(h.IsInvalid) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
      return h;
    }
  }
}
'@
}

function Assert-NoReparseAncestors {
    param([Parameter(Mandatory)][string]$Path)
    $current=[IO.Path]::GetFullPath($Path)
    while($null-ne$current){if(Test-Path -LiteralPath $current){if(((Get-Item -Force -LiteralPath $current).Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'Path ancestors must not contain a reparse point.'}};$parent=[IO.Directory]::GetParent($current);$current=if($null-eq$parent){$null}else{$parent.FullName}}
}

function Open-VerifiedDirectoryHandle {
    param([Parameter(Mandatory)][string]$Path)
    $expected=[IO.Path]::GetFullPath($Path);Assert-NoReparseAncestors $expected
    $handle=[Aevrynd.SafePath]::OpenDirectory($expected)
    try{if([Aevrynd.SafePath]::FinalPath($handle)-cne$expected){throw 'Directory final handle path differs from the canonical state path.'};return $handle}catch{$handle.Dispose();throw}
}

function Assert-ExactJsonKeys {
    param([Collections.IDictionary]$Object,[string[]]$Expected,[string]$Name)
    if($null-eq$Object){throw "$Name must be a JSON object."}
    $actual=@($Object.Keys|ForEach-Object{[string]$_}|Sort-Object)
    $wanted=@($Expected|Sort-Object)
    if(($actual.Count-ne$wanted.Count)-or(Compare-Object $actual $wanted -CaseSensitive)){throw "$Name contains an unsupported JSON field."}
}

function ConvertFrom-StrictFunnelJson {
    param([Parameter(Mandatory)][string]$Json,[ValidateRange(2,1048576)][int]$MaximumCharacters=1048576)
    if($Json.Length-lt2-or$Json.Length-gt$MaximumCharacters){throw 'Tailscale JSON size is outside policy.'}
    $document=$null
    try{
        $options=[Text.Json.JsonDocumentOptions]::new();$options.AllowTrailingCommas=$false;$options.CommentHandling=[Text.Json.JsonCommentHandling]::Disallow;$options.MaxDepth=32
        $document=[Text.Json.JsonDocument]::Parse($Json,$options)
        $visit=$null
        $visit={param([Text.Json.JsonElement]$Element)
            if($Element.ValueKind-eq[Text.Json.JsonValueKind]::Object){
                $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
                $result=[ordered]@{}
                foreach($property in $Element.EnumerateObject()){
                    if(-not$names.Add($property.Name)){throw 'Tailscale JSON contains duplicate object keys.'}
                    $result[$property.Name]=&$visit $property.Value
                }
                return $result
            }
            if($Element.ValueKind-eq[Text.Json.JsonValueKind]::Array){return @($Element.EnumerateArray()|ForEach-Object{&$visit $_})}
            if($Element.ValueKind-eq[Text.Json.JsonValueKind]::String){return $Element.GetString()}
            if($Element.ValueKind-eq[Text.Json.JsonValueKind]::True){return $true}
            if($Element.ValueKind-eq[Text.Json.JsonValueKind]::False){return $false}
            if($Element.ValueKind-eq[Text.Json.JsonValueKind]::Null){return $null}
            if($Element.ValueKind-eq[Text.Json.JsonValueKind]::Number){$integer=[long]0;if($Element.TryGetInt64([ref]$integer)){return $integer};return $Element.GetDouble()}
            throw 'Tailscale JSON contains an unsupported scalar.'
        }
        return &$visit $document.RootElement
    }catch{throw "Tailscale JSON is invalid: $($_.Exception.Message)"}finally{if($null-ne$document){$document.Dispose()}}
}

function Invoke-TailscaleCli {
    param([Parameter(Mandatory)][object]$Executable,[Parameter(Mandatory)][string[]]$Arguments,[ValidateRange(1,300)][int]$TimeoutSeconds=15,[switch]$ShowAuthorizationHint)
    if($Executable.Stream-is[IO.FileStream]-and-not$Executable.Stream.SafeFileHandle.IsClosed){$path=$Executable.ExecutablePath;if([Aevrynd.SafePath]::FinalPath($Executable.Stream.SafeFileHandle)-cne$path){throw 'Tailscale executable handle identity changed.'}}else{throw 'A live Tailscale executable identity handle is required.'}
    $runPath=$path;$runArguments=$Arguments
    if($env:AEVRYND_FUNNEL_TEST_MODE-ceq'isolated-fixture'-and$null-ne$Executable.PSObject.Properties['TestScript']){$runPath=(Get-Command pwsh -ErrorAction Stop).Source;$runArguments=@('-NoLogo','-NoProfile','-File',[string]$Executable.TestScript)+$Arguments}
    return [Aevrynd.SafeProcessRunner]::Run($runPath,$runArguments,$TimeoutSeconds*1000,$ShowAuthorizationHint.IsPresent)
}

function Get-FixedTailscaleExecutableIdentity {
    $expected=[IO.Path]::GetFullPath((Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)) 'Tailscale\tailscale.exe'))
    Assert-NoReparseAncestors $expected
    $stream=[IO.FileStream]::new($expected,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try{$final=[Aevrynd.SafePath]::FinalPath($stream.SafeFileHandle);if($final-cne$expected){throw 'Tailscale executable final handle path is not the fixed Program Files path.'};$item=Get-Item -LiteralPath $expected
    $signature=Get-AuthenticodeSignature -LiteralPath $expected
    if($signature.Status-ne[Management.Automation.SignatureStatus]::Valid-or$null-eq$signature.SignerCertificate-or$signature.SignerCertificate.Subject-notmatch '(^|, )O=Tailscale Inc\.(,|$)'){throw 'Tailscale executable signature is not valid for Tailscale Inc.'}
    $version=[string]$item.VersionInfo.ProductVersion
    $hasher=[Security.Cryptography.SHA256]::Create();try{$stream.Position=0;$sha=([Convert]::ToHexString($hasher.ComputeHash($stream))).ToLowerInvariant()}finally{$hasher.Dispose()}
    if([string]::IsNullOrWhiteSpace($version)-or$sha-notmatch'^[0-9a-f]{64}$'){throw 'Tailscale executable identity is incomplete.'}
    return [pscustomobject]@{ExecutablePath=$expected;ExecutableSha256=$sha;ExecutableVersion=$version;SignerThumbprint=$signature.SignerCertificate.Thumbprint;Stream=$stream}
    }catch{$stream.Dispose();throw}
}

function Get-TailscaleNodeIdentity {
    param([Parameter(Mandatory)][object]$Executable)
    $result=Invoke-TailscaleCli $Executable @('status','--json')
    if($result.ExitCode-ne0){throw 'tailscale status --json failed.'}
    $status=ConvertFrom-StrictFunnelJson $result.Output
    foreach($key in @('Version','TUN','BackendState','Self','CurrentTailnet')){if(-not$status.Contains($key)){throw "Tailscale status is missing $key."}}
    if($status.TUN-isnot[bool]-or-not$status.TUN-or$status.BackendState-cne'Running'){throw 'Tailscale backend is not running.'}
    if($status.Self-isnot[Collections.IDictionary]-or$status.Self.Online-isnot[bool]-or-not$status.Self.Online){throw 'Tailscale self node is not online.'}
    $dns=([string]$status.Self.DNSName).TrimEnd('.').ToLowerInvariant()
    if($dns-cne$script:AevryndFunnelDnsName){throw 'Tailscale self DNS name is outside the fixed host allowlist.'}
    if($status.CurrentTailnet-isnot[Collections.IDictionary]-or$status.CurrentTailnet.MagicDNSEnabled-isnot[bool]-or-not$status.CurrentTailnet.MagicDNSEnabled){throw 'Tailscale MagicDNS is not enabled.'}
    $suffix=[string]$status.CurrentTailnet.MagicDNSSuffix
    if(-not$dns.EndsWith(".$suffix",[StringComparison]::Ordinal)){throw 'Tailscale DNS suffix does not match the current tailnet.'}
    $ipv4=@($status.Self.TailscaleIPs|Where-Object{
        $address=$null
        if(-not[Net.IPAddress]::TryParse([string]$_,[ref]$address)-or$address.AddressFamily-ne[Net.Sockets.AddressFamily]::InterNetwork){return $false}
        $octets=$address.GetAddressBytes();return $octets[0]-eq100-and$octets[1]-ge64-and$octets[1]-le127
    })
    if($ipv4.Count-ne1){throw 'Tailscale self identity requires exactly one CGNAT IPv4 address.'}
    if([string]$status.Self.ID-notmatch'^n[A-Za-z0-9]+$'-or[string]$status.Self.PublicKey-notmatch'^nodekey:[0-9a-f]{64}$'){throw 'Tailscale self node identity is malformed.'}
    return [pscustomobject]@{DnsName=$dns;NodeId=[string]$status.Self.ID;PublicKey=[string]$status.Self.PublicKey;TailscaleIPv4=[string]$ipv4[0];BackendVersion=[string]$status.Version}
}

function Get-FunnelStatusSnapshot {
    param([Parameter(Mandatory)][object]$Executable,[Parameter(Mandatory)][string]$DnsName)
    if($DnsName-cne$script:AevryndFunnelDnsName){throw 'Funnel DNS name is outside the fixed host allowlist.'}
    $result=Invoke-TailscaleCli $Executable @('funnel','status','--json')
    if($result.ExitCode-ne0){throw 'tailscale funnel status --json failed.'}
    $status=ConvertFrom-StrictFunnelJson $result.Output
    if($status.Count-eq0){return [pscustomobject]@{Mode='disabled';DnsName=$DnsName}}
    Assert-ExactJsonKeys $status @('AllowFunnel','TCP','Web') 'Funnel status'
    foreach($key in @('AllowFunnel','TCP','Web')){if($status[$key]-isnot[Collections.IDictionary]){throw "Funnel $key must be a JSON object."}}
    Assert-ExactJsonKeys $status.TCP @('443') 'Funnel TCP'
    if($status.TCP['443']-isnot[Collections.IDictionary]){throw 'Funnel TCP 443 must be a JSON object.'}
    Assert-ExactJsonKeys $status.TCP['443'] @('HTTPS') 'Funnel TCP 443'
    if($status.TCP['443'].HTTPS-isnot[bool]-or-not$status.TCP['443'].HTTPS){throw 'Funnel must terminate HTTPS on port 443.'}
    $hostPort="$DnsName`:443"
    Assert-ExactJsonKeys $status.Web @($hostPort) 'Funnel Web'
    $web=$status.Web[$hostPort]
    if($web-isnot[Collections.IDictionary]){throw 'Funnel web host must be a JSON object.'}
    Assert-ExactJsonKeys $web @('Handlers') 'Funnel web host'
    if($web.Handlers-isnot[Collections.IDictionary]){throw 'Funnel handlers must be a JSON object.'}
    Assert-ExactJsonKeys $web.Handlers @($script:AevryndFunnelPath) 'Funnel handlers'
    $handler=$web.Handlers[$script:AevryndFunnelPath]
    if($handler-isnot[Collections.IDictionary]){throw 'Funnel webhook handler must be a JSON object.'}
    Assert-ExactJsonKeys $handler @('Proxy') 'Funnel webhook handler'
    if($handler.Proxy-isnot[string]-or$handler.Proxy-cne$script:AevryndFunnelTarget){throw 'Funnel backend is outside the fixed loopback allowlist.'}
    Assert-ExactJsonKeys $status.AllowFunnel @($hostPort) 'Funnel AllowFunnel'
    if($status.AllowFunnel[$hostPort]-isnot[bool]-or-not$status.AllowFunnel[$hostPort]){throw 'Funnel public HTTPS permission is not enabled.'}
    return [pscustomobject]@{Mode='active';DnsName=$DnsName;HostPort=$hostPort;HttpsPort=443;Path=$script:AevryndFunnelPath;Target=$script:AevryndFunnelTarget}
}

function Test-FunnelJsonInteger {
    param([object]$Value)
    return $null-ne$Value-and$Value-isnot[bool]-and($Value-is[int]-or$Value-is[long])
}

function ConvertFrom-CanonicalFunnelTimestamp {
    param([Parameter(Mandatory)][string]$Value,[string]$Name)
    $parsed=[DateTimeOffset]::MinValue
    if(-not[DateTimeOffset]::TryParseExact($Value,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind,[ref]$parsed)-or$parsed.Offset-ne[TimeSpan]::Zero-or$parsed.ToString('o',[Globalization.CultureInfo]::InvariantCulture)-cne$Value){throw "$Name must be a canonical UTC timestamp."}
    return $parsed
}

function Read-FunnelState {
    param([hashtable]$Paths)
    if(-not(Test-Path -LiteralPath $Paths.FunnelStateFile -PathType Leaf)){return $null}
    $directoryHandle=Open-VerifiedDirectoryHandle $Paths.State
    try{
      Assert-NoReparseAncestors $Paths.FunnelStateFile
      $stream=[IO.FileStream]::new($Paths.FunnelStateFile,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
      try{
        if([Aevrynd.SafePath]::FinalPath($stream.SafeFileHandle)-cne[IO.Path]::GetFullPath($Paths.FunnelStateFile)){throw 'Funnel state final handle path differs from the canonical state path.'}
        if($stream.Length-lt2-or$stream.Length-gt16384){throw 'Funnel state file size is outside policy.'}
        $bytes=[byte[]]::new([int]$stream.Length);$offset=0
        while($offset-lt$bytes.Length){$read=$stream.Read($bytes,$offset,$bytes.Length-$offset);if($read-eq0){throw 'Funnel state file was truncated during read.'};$offset+=$read}
        if($stream.ReadByte()-ne-1){throw 'Funnel state file changed during read.'}
        try{$raw=[Text.UTF8Encoding]::new($false,$true).GetString($bytes)}finally{[Array]::Clear($bytes,0,$bytes.Length)}
      }finally{$stream.Dispose()}
    }finally{$directoryHandle.Dispose()}
    $state=ConvertFrom-StrictFunnelJson $raw 16384
    $keys=@('activatedUtc','createdUtc','dnsName','executablePath','executableSha256','executableVersion','httpsPort','launchNonce','nodeId','path','publicKey','schemaVersion','status','tailscaleIPv4','target')
    Assert-ExactJsonKeys $state $keys 'Funnel state'
    if(-not(Test-FunnelJsonInteger $state.schemaVersion)-or[long]$state.schemaVersion-ne1-or-not(Test-FunnelJsonInteger $state.httpsPort)-or[long]$state.httpsPort-ne443){throw 'Funnel state numeric fields must be exact JSON integers.'}
    foreach($name in @('createdUtc','dnsName','executablePath','executableSha256','executableVersion','launchNonce','nodeId','path','publicKey','status','tailscaleIPv4','target')){if($state[$name]-isnot[string]-or[string]::IsNullOrWhiteSpace($state[$name])){throw "Funnel state $name must be a non-empty JSON string."}}
    if($state.status-notin@('pending','active')-or$state.launchNonce-notmatch'^[0-9a-f]{32}$'-or$state.executableSha256-notmatch'^[0-9a-f]{64}$'){throw 'Funnel state identity is invalid.'}
    $created=ConvertFrom-CanonicalFunnelTimestamp $state.createdUtc 'Funnel createdUtc'
    if($created-gt[DateTimeOffset]::UtcNow.AddMinutes(5)){throw 'Funnel createdUtc is unreasonably far in the future.'}
    if($state.status-eq'pending'-and$null-ne$state.activatedUtc){throw 'Pending Funnel state must have a null activation time.'}
    if($state.status-eq'active'){
        if($state.activatedUtc-isnot[string]-or[string]::IsNullOrWhiteSpace($state.activatedUtc)){throw 'Active Funnel state requires a string activation time.'}
        $activated=ConvertFrom-CanonicalFunnelTimestamp $state.activatedUtc 'Funnel activatedUtc'
        if($activated-lt$created-or$activated-gt[DateTimeOffset]::UtcNow.AddMinutes(5)){throw 'Funnel activation time is outside the valid sequence.'}
    }
    $stateAddress=$null
    if([string]::IsNullOrWhiteSpace([string]$state.executableVersion)-or[string]$state.nodeId-notmatch'^n[A-Za-z0-9]+$'-or[string]$state.publicKey-notmatch'^nodekey:[0-9a-f]{64}$'-or-not[Net.IPAddress]::TryParse([string]$state.tailscaleIPv4,[ref]$stateAddress)-or$stateAddress.AddressFamily-ne[Net.Sockets.AddressFamily]::InterNetwork){throw 'Funnel state identity fields are malformed.'}
    if([string]$state.dnsName-cne$script:AevryndFunnelDnsName-or[int]$state.httpsPort-ne443-or[string]$state.path-cne$script:AevryndFunnelPath-or[string]$state.target-cne$script:AevryndFunnelTarget){throw 'Funnel state transport tuple is invalid.'}
    return $state
}

function Write-FunnelState {
    param([hashtable]$Paths,[string]$Status,[object]$Executable,[object]$Node,[string]$LaunchNonce,[string]$CreatedUtc,[object]$ActivatedUtc=$null)
    $directoryHandle=Open-VerifiedDirectoryHandle $Paths.State
    $temporary=$null;$backup=$null;$bytes=$null
    try{
      if(Test-Path -LiteralPath $Paths.FunnelStateFile){if(((Get-Item -LiteralPath $Paths.FunnelStateFile).Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'Funnel state file must not be a reparse point.'}}
      $document=[ordered]@{schemaVersion=1;status=$Status;createdUtc=$CreatedUtc;activatedUtc=$ActivatedUtc;launchNonce=$LaunchNonce;executablePath=$Executable.ExecutablePath;executableSha256=$Executable.ExecutableSha256;executableVersion=$Executable.ExecutableVersion;dnsName=$Node.DnsName;nodeId=$Node.NodeId;publicKey=$Node.PublicKey;tailscaleIPv4=$Node.TailscaleIPv4;httpsPort=443;path=$script:AevryndFunnelPath;target=$script:AevryndFunnelTarget}
      $temporary="$($Paths.FunnelStateFile).$([Guid]::NewGuid().ToString('N')).tmp"
      $backup="$($Paths.FunnelStateFile).$([Guid]::NewGuid().ToString('N')).bak"
      $bytes=[Text.UTF8Encoding]::new($false,$true).GetBytes(($document|ConvertTo-Json))
      try{
        $stream=[IO.FileStream]::new($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None,4096,[IO.FileOptions]::WriteThrough)
        try{if([Aevrynd.SafePath]::FinalPath($stream.SafeFileHandle)-cne[IO.Path]::GetFullPath($temporary)){throw 'Temporary Funnel state escaped the verified state directory.'};$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
        if(Test-Path -LiteralPath $Paths.FunnelStateFile){[IO.File]::Replace($temporary,$Paths.FunnelStateFile,$backup,$true);Remove-Item -LiteralPath $backup -Force}else{[IO.File]::Move($temporary,$Paths.FunnelStateFile)}
        $verified=Read-FunnelState -Paths $Paths
        if($verified.status-cne$Status-or$verified.launchNonce-cne$LaunchNonce-or$verified.executableSha256-cne$Executable.ExecutableSha256){throw 'Durable Funnel state read-back identity mismatch.'}
      }finally{if($null-ne$bytes){[Array]::Clear($bytes,0,$bytes.Length)};foreach($artifact in @($temporary,$backup)){if($null-ne$artifact-and(Test-Path -LiteralPath $artifact)){Remove-Item -LiteralPath $artifact -Force}}}
    }finally{$directoryHandle.Dispose()}
}

function Assert-FunnelIdentityMatchesState {
    param([object]$State,[object]$Executable,[object]$Node)
    if(-not[string]::Equals([IO.Path]::GetFullPath([string]$State.executablePath),$Executable.ExecutablePath,[StringComparison]::OrdinalIgnoreCase)-or[string]$State.executableSha256-cne$Executable.ExecutableSha256-or[string]$State.executableVersion-cne$Executable.ExecutableVersion-or[string]$State.dnsName-cne$Node.DnsName-or[string]$State.nodeId-cne$Node.NodeId-or[string]$State.publicKey-cne$Node.PublicKey-or[string]$State.tailscaleIPv4-cne$Node.TailscaleIPv4){throw 'Current Tailscale executable or node identity does not match Funnel state.'}
}

function Remove-ExactPendingFunnelState {
    param([hashtable]$Paths,[Parameter(Mandatory)][string]$LaunchNonce)
    $pending=Read-FunnelState -Paths $Paths
    if($null-eq$pending-or$pending.status-cne'pending'-or$pending.launchNonce-cne$LaunchNonce){throw 'Pending Funnel state does not match the activation attempt.'}
    Remove-Item -LiteralPath $Paths.FunnelStateFile -Force
}

function Test-FunnelTransportAbsent {
    $expected=[IO.Path]::GetFullPath((Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)) 'Tailscale\tailscale.exe'));if(-not(Test-Path -LiteralPath $expected -PathType Leaf)){return}
    $executable=Get-FixedTailscaleExecutableIdentity
    try{$result=Invoke-TailscaleCli $executable @('funnel','status','--json')}finally{$executable.Stream.Dispose()}
    if($result.ExitCode-ne0){throw 'Unable to prove that Tailscale Funnel is disabled.'}
    $status=ConvertFrom-StrictFunnelJson $result.Output
    if($status.Count-ne0){throw 'Tailscale Funnel is configured; refusing a second public transport.'}
}
