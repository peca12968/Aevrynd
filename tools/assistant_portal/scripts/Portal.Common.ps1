Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:PortalRoot = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\AssistantPortal")
$script:PortalDomain = 'sample-cupid-overdrawn.ngrok-free.dev'
$script:PortalPort = 8792
$script:GmailCredentialTarget = 'Aevrynd/AssistantPortal/GmailAppPassword'
$script:NgrokCredentialTarget = 'Aevrynd/LineGateway/NgrokAuthtoken'
$script:NgrokApiCredentialTarget = 'Aevrynd/AssistantPortal/NgrokApiKey'

function Assert-PowerShellSeven {
    if ($PSVersionTable.PSVersion.Major -lt 7) { throw 'PowerShell 7 or newer is required.' }
}

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Administrator privileges are required.'
    }
}

function Assert-StandardUserProcess {
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent();$principal=[Security.Principal.WindowsPrincipal]::new($identity)
    if($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Production portal processes must be launched by the Limited interactive-user scheduled task.'}
}

function Get-PortalPaths {
    return @{
        Root = $script:PortalRoot
        App = Join-Path $script:PortalRoot 'app'
        Scripts = Join-Path $script:PortalRoot 'scripts'
        State = Join-Path $script:PortalRoot 'state'
        Logs = Join-Path $script:PortalRoot 'logs'
        Privileged = Join-Path $script:PortalRoot 'privileged'
        Manifest = Join-Path $script:PortalRoot 'manifest.json'
        Install = Join-Path $script:PortalRoot 'install.json'
        PortalState = Join-Path $script:PortalRoot 'state\portal.json'
        NgrokState = Join-Path $script:PortalRoot 'state\ngrok.json'
        SwitchState = Join-Path $script:PortalRoot 'privileged\switch.json'
        RuntimeIntent = Join-Path $script:PortalRoot 'state\intent.json'
        LaunchIntent = Join-Path $script:PortalRoot 'state\launch-intent.json'
        TaskResult = Join-Path $script:PortalRoot 'state\task-result.json'
        SmtpDiagnostic = Join-Path $script:PortalRoot 'state\smtp-readiness.json'
    }
}

function Assert-PortalPath {
    param([Parameter(Mandatory)][string]$Path)
    $candidate = [IO.Path]::GetFullPath($Path)
    $prefix = $script:PortalRoot.TrimEnd('\') + '\'
    if (-not $candidate.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Path escaped the Assistant Portal installation root.'
    }
    return $candidate
}

function Assert-NoReparseAncestors {
    param([Parameter(Mandatory)][string]$Path)
    $cursor=[IO.Path]::GetFullPath($Path)
    while($null-ne$cursor){
        if(Test-Path -LiteralPath $cursor){if(((Get-Item -LiteralPath $cursor -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'A trusted path contains a reparse point.'}}
        $parent=[IO.Path]::GetDirectoryName($cursor);if([string]::IsNullOrEmpty($parent)-or$parent-ceq$cursor){break};$cursor=$parent
    }
}

function Add-PortalSafePathInterop {
    if('AevryndPortalSafePath'-as[type]){return}
    Add-Type @'
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class AevryndPortalSafePath {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint GetFinalPathNameByHandleW(SafeFileHandle h,System.Text.StringBuilder b,uint n,uint f);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern SafeFileHandle CreateFileW(string n,uint a,uint s,IntPtr p,uint c,uint f,IntPtr t);
 public static string Final(SafeFileHandle h){var b=new System.Text.StringBuilder(32768);uint n=GetFinalPathNameByHandleW(h,b,(uint)b.Capacity,0);if(n==0||n>=b.Capacity)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());var s=b.ToString();return s.StartsWith(@"\\?\")?s.Substring(4):s;}
 public static SafeFileHandle OpenDirectory(string p){var h=CreateFileW(p,0x80,1|2|4,IntPtr.Zero,3,0x02000000|0x00200000,IntPtr.Zero);if(h.IsInvalid)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());var expected=System.IO.Path.GetFullPath(p);var actual=System.IO.Path.GetFullPath(Final(h));if(!String.Equals(expected,actual,StringComparison.Ordinal)){h.Dispose();throw new InvalidOperationException("Directory final path changed.");}return h;}
}
'@
}

function Get-VerifiedDirectoryHandle {
    param([Parameter(Mandatory)][string]$Path)
    $full=[IO.Path]::GetFullPath($Path);Assert-NoReparseAncestors $full;Add-PortalSafePathInterop
    return [AevryndPortalSafePath]::OpenDirectory($full)
}

function Get-VerifiedFileHash {
    param([Parameter(Mandatory)][string]$Path)
    $full=[IO.Path]::GetFullPath($Path);Assert-NoReparseAncestors $full;Add-PortalSafePathInterop
    $stream=[IO.FileStream]::new($full,'Open','Read','Read')
    try{
        if([IO.Path]::GetFullPath([AevryndPortalSafePath]::Final($stream.SafeFileHandle))-cne$full){throw 'Final file path changed.'}
        $sha=[Security.Cryptography.SHA256]::Create();try{return [pscustomobject]@{Hash=([Convert]::ToHexString($sha.ComputeHash($stream))).ToLowerInvariant();Length=$stream.Length;Stream=$stream}}finally{$sha.Dispose()}
    }catch{$stream.Dispose();throw}
}

function Read-PortalJson {
    param([Parameter(Mandatory)][string]$Path, [ValidateRange(1,65536)][int]$MaximumBytes = 8192)
    $full = Assert-PortalPath $Path
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { return $null }
    $item = Get-Item -LiteralPath $full -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $item.Length -lt 2 -or $item.Length -gt $MaximumBytes) {
        throw 'Portal state file is outside policy.'
    }
    return Get-Content -Raw -LiteralPath $full | ConvertFrom-Json
}

function Write-PortalJson {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][object]$Value)
    $full = Assert-PortalPath $Path
    $temporary = "$full.$([Guid]::NewGuid().ToString('N')).tmp"
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Compress -Depth 5))
    try {
        [IO.File]::WriteAllBytes($temporary, $bytes)
        [IO.File]::Move($temporary, $full, $true)
    } finally {
        [Array]::Clear($bytes, 0, $bytes.Length)
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Protect-PortalStateFile {
    param([Parameter(Mandatory)][string]$Path)
    $full=Assert-PortalPath $Path
    $acl=Get-Acl -LiteralPath $full
    $acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @(
        [Security.Principal.WindowsIdentity]::GetCurrent().User,
        [Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::LocalSystemSid,$null),
        [Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::BuiltinAdministratorsSid,$null)
    )){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','Allow'))}
    Set-Acl -LiteralPath $full -AclObject $acl
}

function Protect-PrivilegedStateFile {
    param([Parameter(Mandatory)][string]$Path)
    Assert-Administrator
    $full=Assert-PortalPath $Path;$acl=[Security.AccessControl.FileSecurity]::new();$acl.SetAccessRuleProtection($true,$false);$adminSid=[Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::BuiltinAdministratorsSid,$null);$systemSid=[Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::LocalSystemSid,$null);$acl.SetOwner($adminSid)
    foreach($sid in @($systemSid,$adminSid)){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','Allow'))}
    Set-Acl -LiteralPath $full -AclObject $acl
    $actual=Get-Acl -LiteralPath $full;$owner=([Security.Principal.NTAccount]$actual.Owner).Translate([Security.Principal.SecurityIdentifier]);if(-not$actual.AreAccessRulesProtected-or$owner.Value-cne$adminSid.Value){throw 'Privileged state ACL owner or inheritance protection is not canonical.'};$rules=@($actual.GetAccessRules($true,$false,[Security.Principal.SecurityIdentifier]));$allowed=@($systemSid.Value,$adminSid.Value);if($rules.Count-ne2-or@($rules|Where-Object{$_.IdentityReference.Value-notin$allowed-or$_.AccessControlType-ne[Security.AccessControl.AccessControlType]::Allow-or($_.FileSystemRights-band[Security.AccessControl.FileSystemRights]::FullControl)-ne[Security.AccessControl.FileSystemRights]::FullControl-or$_.InheritanceFlags-ne[Security.AccessControl.InheritanceFlags]::None-or$_.PropagationFlags-ne[Security.AccessControl.PropagationFlags]::None}).Count){throw 'Privileged state ACL rules are not canonical.'};foreach($sid in @($systemSid,$adminSid)){if(@($rules|Where-Object{$_.IdentityReference.Value-ceq$sid.Value}).Count-ne1){throw 'Privileged state ACL identities are not canonical.'}}
}

function Test-InstalledPortal {
    param([switch]$AllowRuntimeChange,[switch]$RecoveryOnly)
    $paths = Get-PortalPaths
    $manifest = Read-PortalJson -Path $paths.Manifest -MaximumBytes 65536
    $install = Read-PortalJson -Path $paths.Install
    if ($null -eq $manifest -or $null -eq $install -or $manifest.schemaVersion -ne 1 -or $install.schemaVersion -ne 1) {
        throw 'Assistant Portal installation metadata is missing or invalid.'
    }
    if([string]$install.ownerSid-cne[Security.Principal.WindowsIdentity]::GetCurrent().User.Value){throw 'Assistant Portal must run as the Windows user that installed its credential context.'}
    $expectedFiles=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in @($manifest.files)) {
        if ($entry.path -isnot [string] -or $entry.sha256 -notmatch '^[0-9a-f]{64}$' -or $entry.size -isnot [long] -and $entry.size -isnot [int]) {
            throw 'Assistant Portal manifest entry is invalid.'
        }
        $path = Assert-PortalPath (Join-Path $paths.Root ([string]$entry.path))
        if(-not$expectedFiles.Add([IO.Path]::GetRelativePath($paths.Root,$path))){throw 'Assistant Portal manifest contains a duplicate path.'}
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw 'An installed Assistant Portal file is missing.' }
        $verifiedFile=Get-VerifiedFileHash $path
        try{if ($verifiedFile.Length -ne [long]$entry.size -or $verifiedFile.Hash -cne [string]$entry.sha256) {
            throw 'Assistant Portal installation integrity validation failed.'
        }}finally{$verifiedFile.Stream.Dispose()}
    }
    $actualFiles=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($directory in @($paths.App,$paths.Scripts)){foreach($file in Get-ChildItem -LiteralPath $directory -File -Recurse -Force){if(($file.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'Installed executable tree contains a reparse point.'};$null=$actualFiles.Add([IO.Path]::GetRelativePath($paths.Root,$file.FullName))}}
    if(-not$actualFiles.SetEquals($expectedFiles)){throw 'Installed executable tree differs from the closed manifest.'}
    if(-not$RecoveryOnly){foreach ($name in @('pythonExe','ngrokExe')) {
        $path = [IO.Path]::GetFullPath([string]$install.$name)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$name is unavailable."
        }
    }}
    if(-not$RecoveryOnly-and-not$AllowRuntimeChange){$pythonFile=Get-VerifiedFileHash $install.pythonExe;$ngrokFile=Get-VerifiedFileHash $install.ngrokExe;try{if($pythonFile.Hash-cne[string]$install.pythonSha256-or$ngrokFile.Hash-cne[string]$install.ngrokSha256){throw 'Pinned runtime identity changed.'}}finally{$pythonFile.Stream.Dispose();$ngrokFile.Stream.Dispose()}}
    return [pscustomobject]@{ Paths = $paths; Install = $install }
}

function Get-OwnedProcess {
    param([Parameter(Mandatory)][object]$State, [Parameter(Mandatory)][string]$ExpectedExecutable, [Parameter(Mandatory)][string]$ExpectedCommandLine)
    if ($State.pid -isnot [int] -and $State.pid -isnot [long]) { throw 'Process state PID is invalid.' }
    $record = Get-CimInstance Win32_Process -Filter "ProcessId = $([int]$State.pid)" -ErrorAction SilentlyContinue
    if ($null -eq $record) { return $null }
    if ([IO.Path]::GetFullPath([string]$record.ExecutablePath) -cne [IO.Path]::GetFullPath($ExpectedExecutable) -or
        [string]$record.CommandLine -cne $ExpectedCommandLine) {
        throw 'State PID belongs to a different process.'
    }
    return Get-Process -Id ([int]$State.pid) -ErrorAction Stop
}

function Read-PortalRuntimeState {
    param([Parameter(Mandatory)][string]$Path,[ValidateSet('portal','ngrok')][string]$Kind)
    $state=Read-PortalJson -Path $Path
    if($null-eq$state){return $null}
    $names=@($state.PSObject.Properties.Name)
    $expected=if($Kind-eq'portal'){@('instanceNonce','pid','schemaVersion','startedUtc')}else{@('pid','schemaVersion','startedUtc')}
    if((@($names|Sort-Object)-join',')-cne(@($expected|Sort-Object)-join',')-or$state.schemaVersion-ne1-or($state.pid-isnot[long]-and$state.pid-isnot[int])-or[long]$state.pid-lt1-or[long]$state.pid-gt[int]::MaxValue){throw "$Kind state schema is invalid."}
    if($state.startedUtc-isnot[string]){throw "$Kind state timestamp is invalid."}
    try{$stamp=[DateTimeOffset]::ParseExact([string]$state.startedUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw "$Kind state timestamp is invalid."}
    if($stamp-gt[DateTimeOffset]::UtcNow.AddMinutes(5)){throw "$Kind state timestamp is unreasonable."}
    if($Kind-eq'portal'-and($state.instanceNonce-isnot[string]-or$state.instanceNonce-notmatch'^[0-9a-f]{32}$')){throw 'Portal instance nonce is invalid.'}
    return $state
}

function Assert-StartedOwnedProcess {
    param([Parameter(Mandatory)][Diagnostics.Process]$Process,[Parameter(Mandatory)][string]$ExpectedExecutable,[Parameter(Mandatory)][string]$ExpectedCommandLine,[Parameter(Mandatory)][DateTimeOffset]$NotBefore)
    $record=Get-CimInstance Win32_Process -Filter "ProcessId = $($Process.Id)" -ErrorAction Stop
    if($null-eq$record-or[IO.Path]::GetFullPath([string]$record.ExecutablePath)-cne[IO.Path]::GetFullPath($ExpectedExecutable)-or[string]$record.CommandLine-cne$ExpectedCommandLine){throw 'Started process identity or command tuple is invalid.'}
    $started=[DateTimeOffset]::new($Process.StartTime.ToUniversalTime())
    if($started-lt$NotBefore.AddSeconds(-2)-or$started-gt[DateTimeOffset]::UtcNow.AddSeconds(2)){throw 'Started process time is outside the launch window.'}
}

function Assert-PortalListener {
    param([Parameter(Mandatory)][int]$ProcessId)
    Assert-ExactLoopbackListener -Port $script:PortalPort -ProcessId $ProcessId
}

function Assert-ExactLoopbackListener {
    param([Parameter(Mandatory)][ValidateRange(1,65535)][int]$Port,[Parameter(Mandatory)][int]$ProcessId)
    $listeners=@(Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue)
    if($listeners.Count-ne1-or$listeners[0].LocalAddress-cne'127.0.0.1'-or[int]$listeners[0].OwningProcess-ne$ProcessId){throw 'Loopback listener owner or binding is invalid.'}
}

function Assert-PortalRuntime {
    param([Parameter(Mandatory)][object]$Verified,[Parameter(Mandatory)][object]$State)
    $process=Get-OwnedProcess -State $State -ExpectedExecutable $Verified.Install.pythonExe -ExpectedCommandLine (Get-PortalCommandLine $Verified)
    if($null-eq$process){throw 'Managed production portal process is absent.'}
    $recorded=[DateTimeOffset]::ParseExact([string]$State.startedUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)
    $actual=[DateTimeOffset]::new($process.StartTime.ToUniversalTime())
    if([Math]::Abs(($actual-$recorded).TotalMilliseconds)-gt1000){throw 'Portal process start time differs from durable state.'}
    Assert-PortalListener -ProcessId $process.Id
    $response=Invoke-WebRequest -Uri "http://127.0.0.1:$script:PortalPort/" -Headers @{Host=$script:PortalDomain} -TimeoutSec 3
    $marker='<meta name="aevrynd-instance" content="'+[string]$State.instanceNonce+'">'
    if($response.StatusCode-ne200-or-not$response.Content.Contains($marker)-or$response.Headers.'Strict-Transport-Security'-notcontains'max-age=31536000; includeSubDomains'){throw 'Managed production portal marker or security response is invalid.'}
    return $process
}

function Assert-NgrokRuntime {
    param([Parameter(Mandatory)][object]$Verified,[Parameter(Mandatory)][object]$State,[Parameter(Mandatory)][string]$InstanceNonce)
    $process=Get-OwnedProcess -State $State -ExpectedExecutable $Verified.Install.ngrokExe -ExpectedCommandLine (Get-NgrokCommandLine $Verified)
    if($null-eq$process){throw 'Managed portal ngrok process is absent.'}
    $recorded=[DateTimeOffset]::ParseExact([string]$State.startedUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)
    $actual=[DateTimeOffset]::new($process.StartTime.ToUniversalTime())
    if([Math]::Abs(($actual-$recorded).TotalMilliseconds)-gt1000){throw 'Ngrok process start time differs from durable state.'}
    $apiListeners=@(Get-NetTCPConnection -State Listen -LocalPort 4040 -ErrorAction SilentlyContinue)
    if($apiListeners.Count-ne1-or$apiListeners[0].LocalAddress-notin@('127.0.0.1','::1')-or[int]$apiListeners[0].OwningProcess-ne$process.Id){throw 'Ngrok local API listener owner is invalid.'}
    $api=Invoke-RestMethod -Uri 'http://127.0.0.1:4040/api/tunnels' -TimeoutSec 2;Assert-NgrokTunnelDocument $api
    $response=Invoke-WebRequest -Uri "https://$script:PortalDomain/" -Headers @{'ngrok-skip-browser-warning'='1'} -MaximumRedirection 0 -TimeoutSec 3 -SkipHttpErrorCheck
    $marker='<meta name="aevrynd-instance" content="'+$InstanceNonce+'">'
    if($response.StatusCode-ne200-or-not$response.Content.Contains($marker)){throw 'Public ngrok endpoint does not expose the exact managed portal instance.'}
    return $process
}

function Assert-NgrokTunnelDocument {
    param([Parameter(Mandatory)][object]$Document)
    $tunnels=@($Document.tunnels)
    if($tunnels.Count-ne1-or[string]$tunnels[0].public_url-cne"https://$script:PortalDomain"-or[string]$tunnels[0].proto-cne'https'-or[string]$tunnels[0].config.addr-cne"http://127.0.0.1:$script:PortalPort"){throw 'Ngrok local API tuple mismatch.'}
}

function Enter-PortalSwitchLock {
    $mutex = [Threading.Mutex]::new($false, 'Global\AevryndAssistantPortalSwitch')
    try { if (-not $mutex.WaitOne(30000)) { throw 'Timed out waiting for public transport lock.' } }
    catch [Threading.AbandonedMutexException] { }
    return $mutex
}

function Enter-PublicTransportLock {
    $mutex = [Threading.Mutex]::new($false, 'Global\AevryndLineGatewayPublicTransportControl')
    try { if (-not $mutex.WaitOne(30000)) { throw 'Timed out waiting for public transport lock.' } }
    catch [Threading.AbandonedMutexException] { }
    return $mutex
}

function Exit-PortalSwitchLock {
    param([Parameter(Mandatory)][Threading.Mutex]$Mutex)
    try { $Mutex.ReleaseMutex() } finally { $Mutex.Dispose() }
}

function Invoke-FailSafeSequence {
    param([Parameter(Mandatory)][scriptblock[]]$Steps,[Parameter(Mandatory)][scriptblock]$Rollback)
    try{foreach($step in $Steps){& $step}}catch{$cause=$_.Exception.Message;try{& $Rollback}catch{throw "Transaction failed ($cause); rollback failed: $($_.Exception.Message)"};throw "Transaction failed and rollback completed: $cause"}
}

function Invoke-ManagedProcessLaunchGuard {
    param([Parameter(Mandatory)][Diagnostics.Process]$Process,[Parameter(Mandatory)][scriptblock]$Operation,[Parameter(Mandatory)][scriptblock]$Cleanup)
    try{& $Operation}catch{if(-not$Process.HasExited){$Process.Kill($true);$Process.WaitForExit()};try{& $Cleanup}catch{};throw}
}

function Stop-ProtectedPortalLaunchTree {
    param([Parameter(Mandatory)][object]$Verified,[Parameter(Mandatory)][DateTimeOffset]$NotBefore)
    $portalCommand=Get-PortalCommandLine $Verified;$ngrokCommand=Get-NgrokCommandLine $Verified;$candidates=[Collections.Generic.Dictionary[int,object]]::new();$unknown=$false
    foreach($listener in @(Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort -ErrorAction SilentlyContinue)){$record=Get-CimInstance Win32_Process -Filter "ProcessId = $([int]$listener.OwningProcess)" -ErrorAction SilentlyContinue;if($null-eq$record-or[string]$record.CommandLine-cne$portalCommand-or[IO.Path]::GetFullPath([string]$record.ExecutablePath)-cne[IO.Path]::GetFullPath([string]$Verified.Install.pythonExe)){$unknown=$true}else{$candidates[[int]$record.ProcessId]=$record}}
    foreach($record in @(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue)){if([string]$record.CommandLine-cne$ngrokCommand-or[IO.Path]::GetFullPath([string]$record.ExecutablePath)-cne[IO.Path]::GetFullPath([string]$Verified.Install.ngrokExe)){$unknown=$true}else{$candidates[[int]$record.ProcessId]=$record}}
    foreach($record in @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)){if(([string]$record.CommandLine-ceq$portalCommand-and[IO.Path]::GetFullPath([string]$record.ExecutablePath)-ceq[IO.Path]::GetFullPath([string]$Verified.Install.pythonExe))-or([string]$record.CommandLine-ceq$ngrokCommand-and[IO.Path]::GetFullPath([string]$record.ExecutablePath)-ceq[IO.Path]::GetFullPath([string]$Verified.Install.ngrokExe))){$candidates[[int]$record.ProcessId]=$record}}
    foreach($pid in @($candidates.Keys)){try{$process=Get-Process -Id $pid -ErrorAction Stop;$started=[DateTimeOffset]::new($process.StartTime.ToUniversalTime());if($started-lt$NotBefore.AddSeconds(-2)){$unknown=$true;continue};$process.Kill($true);$process.WaitForExit()}catch{$unknown=$true}}
    if(@(Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort -ErrorAction SilentlyContinue).Count-or@(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue).Count-or$unknown){throw 'Protected launch cleanup found an unknown or unterminated public process.'}
}

function ConvertTo-ExactCommandLine {
    param([Parameter(Mandatory)][string[]]$Tokens)
    return ($Tokens | ForEach-Object { if ($_ -match '[\s"]') {'"'+($_ -replace '"','\"')+'"'} else {$_} }) -join ' '
}

function Get-PortalCommandLine {
    param([Parameter(Mandatory)][object]$Verified)
    $arguments=ConvertTo-ExactCommandLine @(
        '-B',(Join-Path $Verified.Paths.App 'server.py'),
        '--host','127.0.0.1','--port',[string]$script:PortalPort,'--production',
        '--public-host',$script:PortalDomain,'--trusted-proxy-source','127.0.0.1'
    )
    return '"'+([string]$Verified.Install.pythonExe -replace '"','\"')+'" '+$arguments
}

function Get-NgrokCommandLine {
    param([Parameter(Mandatory)][object]$Verified)
    $arguments=ConvertTo-ExactCommandLine @(
        'http',"http://127.0.0.1:$script:PortalPort",
        "--domain=$script:PortalDomain",'--scheme=https','--inspect=false','--log=false'
    )
    return '"'+([string]$Verified.Install.ngrokExe -replace '"','\"')+'" '+$arguments
}

function Add-PortalCredentialInterop {
    if ('AevryndPortalCredential' -as [type]) { return }
    Add-Type @'
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Net.Http;
using System.Runtime.InteropServices;
public static class AevryndPortalCredential {
 [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Credential { public uint Flags,Type; public IntPtr TargetName,Comment; public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten; public uint BlobSize; public IntPtr Blob; public uint Persist,AttributeCount; public IntPtr Attributes,Alias,User; }
 [DllImport("advapi32.dll",EntryPoint="CredReadW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool Read(string target,uint type,uint flags,out IntPtr value);
 [DllImport("advapi32.dll")] static extern void CredFree(IntPtr value);
 [DllImport("advapi32.dll",EntryPoint="CredWriteW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool Write(ref Credential value,uint flags);
 public static void Store(string target, IntPtr secret, uint bytes) { var c=new Credential{Type=1,TargetName=Marshal.StringToCoTaskMemUni(target),Blob=secret,BlobSize=bytes,Persist=2,User=Marshal.StringToCoTaskMemUni(Environment.UserName)}; try { if(!Write(ref c,0))throw new Win32Exception(Marshal.GetLastWin32Error()); } finally { Marshal.ZeroFreeCoTaskMemUnicode(c.TargetName); Marshal.ZeroFreeCoTaskMemUnicode(c.User); } }
 static char[] NormalizeGmail(IntPtr blob,uint bytes) { var value=new char[16];int count=0;try{if(bytes%2!=0)throw new InvalidOperationException("Invalid Gmail credential.");for(int offset=0;offset<bytes;offset+=2){char c=(char)Marshal.ReadInt16(blob,offset);if(c==' ')continue;if(count>=16||!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')))throw new InvalidOperationException("Invalid Gmail credential.");value[count++]=c;}if(count!=16)throw new InvalidOperationException("Invalid Gmail credential.");return value;}catch{Array.Clear(value,0,value.Length);throw;} }
 public static bool ValidGmailBlob(IntPtr blob,uint bytes) { if(bytes!=32)return false;try{var value=NormalizeGmail(blob,bytes);Array.Clear(value,0,value.Length);return true;}catch{return false;} }
 public static bool ValidGmail(string target) { IntPtr p;if(!Read(target,1,0,out p))return false;try{var c=Marshal.PtrToStructure<Credential>(p);return ValidGmailBlob(c.Blob,c.BlobSize);}finally{CredFree(p);} }
 public static void StoreGmail(string target,IntPtr secret,uint bytes) { var value=NormalizeGmail(secret,bytes);var handle=GCHandle.Alloc(value,GCHandleType.Pinned);try{Store(target,handle.AddrOfPinnedObject(),32);}finally{Array.Clear(value,0,value.Length);handle.Free();} }
 static string Load(string target) { IntPtr p;if(!Read(target,1,0,out p))throw new Win32Exception(Marshal.GetLastWin32Error());try{var c=Marshal.PtrToStructure<Credential>(p);return Marshal.PtrToStringUni(c.Blob,(int)c.BlobSize/2);}finally{CredFree(p);} }
 public static bool Available(string target,int minimum,int maximum){string value=null;try{value=Load(target);return value!=null&&value.Length>=minimum&&value.Length<=maximum;}catch{return false;}finally{value=null;}}
 static ProcessStartInfo PortalInfo(string exe,string[] args,string secret,string nonce,string diagnostic,bool redirect){var i=new ProcessStartInfo(exe){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=System.IO.Path.GetDirectoryName(exe),RedirectStandardOutput=redirect,RedirectStandardError=redirect};foreach(var a in args)i.ArgumentList.Add(a);foreach(var key in new System.Collections.Generic.List<string>(i.Environment.Keys)){if(key.StartsWith("AEVRYND_",StringComparison.OrdinalIgnoreCase))i.Environment.Remove(key);}i.Environment["AEVRYND_AUTH_ADAPTER"]="smtp";i.Environment["AEVRYND_SMTP_HOST"]="smtp.gmail.com";i.Environment["AEVRYND_SMTP_PORT"]="465";i.Environment["AEVRYND_SMTP_USERNAME"]="slime12968@gmail.com";i.Environment["AEVRYND_SMTP_SENDER"]="slime12968@gmail.com";i.Environment["AEVRYND_SMTP_PASSWORD"]=secret;i.Environment["AEVRYND_SMTP_DIAGNOSTIC_PATH"]=diagnostic;if(!String.IsNullOrEmpty(nonce))i.Environment["AEVRYND_INSTANCE_NONCE"]=nonce;return i;}
 static bool CanonicalGmailText(string value) { if(value==null||value.Length!=16)return false;foreach(char c in value)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')))return false;return true; }
 public static Process StartPortal(string exe,string[] args,string gmailTarget,string nonce,string diagnostic) { string secret=Load(gmailTarget);try{if(!CanonicalGmailText(secret))throw new InvalidOperationException("Gmail credential is not canonical; replace it securely.");return Process.Start(PortalInfo(exe,args,secret,nonce,diagnostic,false));}finally{secret=null;} }
 public static Process StartPortalPreflight(string exe,string[] args,string gmailTarget,string diagnostic) { string secret=Load(gmailTarget);try{if(!CanonicalGmailText(secret))throw new InvalidOperationException("Gmail credential is not canonical; replace it securely.");return Process.Start(PortalInfo(exe,args,secret,null,diagnostic,true));}finally{secret=null;} }
 public static Process StartNgrok(string exe,string[] args,string tokenTarget) { string secret=Load(tokenTarget);try{if(String.IsNullOrWhiteSpace(secret))throw new InvalidOperationException("Ngrok credential is empty.");var i=new ProcessStartInfo(exe){UseShellExecute=false,CreateNoWindow=true};foreach(var a in args)i.ArgumentList.Add(a);foreach(var key in new System.Collections.Generic.List<string>(i.Environment.Keys)){if(key.StartsWith("NGROK_",StringComparison.OrdinalIgnoreCase))i.Environment.Remove(key);}i.Environment["NGROK_AUTHTOKEN"]=secret;return Process.Start(i);}finally{secret=null;} }
 public static string NgrokApiGet(string target,string uri) { string secret=Load(target);try{using(var c=new HttpClient()){c.Timeout=TimeSpan.FromSeconds(15);c.DefaultRequestHeaders.Add("Authorization","Bearer "+secret);c.DefaultRequestHeaders.Add("Ngrok-Version","2");var r=c.GetAsync(uri).GetAwaiter().GetResult();if(!r.IsSuccessStatusCode)throw new InvalidOperationException("ngrok API rejected the request.");var body=r.Content.ReadAsStringAsync().GetAwaiter().GetResult();if(body.Length>1048576)throw new InvalidOperationException("ngrok API response exceeded policy.");return body;}}finally{secret=null;} }
}
'@
}

function Get-LineServiceTruth {
    param([switch]$RecoveryMode)
    $lineRoot=[IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\LineGateway")
    $portalNgrokRunning=$false
    if(-not$RecoveryMode){
        $portalVerified=Test-InstalledPortal
        $portalNgrokState=Read-PortalRuntimeState -Path $portalVerified.Paths.NgrokState -Kind ngrok
        if($null-ne$portalNgrokState){$portalNgrokRunning=$null-ne(Get-OwnedProcess -State $portalNgrokState -ExpectedExecutable $portalVerified.Install.ngrokExe -ExpectedCommandLine (Get-NgrokCommandLine $portalVerified))}
    }
    if(-not(Test-Path -LiteralPath $lineRoot -PathType Container)){
        if(@(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue).Count-ne([int]$portalNgrokRunning)){throw 'An unmanaged ngrok process exists without the managed LINE installation.'}
        return [pscustomobject]@{GatewayRunning=$false;NgrokRunning=$false}
    }
    return & {
        . (Join-Path $lineRoot 'scripts\Common.ps1')
        . (Join-Path $lineRoot 'scripts\Funnel.Common.ps1')
        . (Join-Path $lineRoot 'scripts\Ngrok.Common.ps1')
        $paths=Get-GatewayPaths -InstallRoot $lineRoot
        $null=Test-InstallationManifest $paths
        $gatewayState=Read-GatewayState -Paths $paths
        $gatewayRunning=$false
        $gatewayPid=0
        if($null-ne$gatewayState){$gatewayProcess=Test-GatewayProcess -Paths $paths -State $gatewayState;if($null-eq$gatewayProcess){throw 'LINE gateway state is stale.'};$gatewayRunning=$true;$gatewayPid=$gatewayProcess.Id}
        $gatewayListeners=@(Get-NetTCPConnection -State Listen -LocalPort 8787 -ErrorAction SilentlyContinue)
        if($gatewayRunning){if($gatewayListeners.Count-ne1-or$gatewayListeners[0].LocalAddress-cne'127.0.0.1'-or[int]$gatewayListeners[0].OwningProcess-ne$gatewayPid){throw 'LINE gateway listener and managed process disagree.'}}
        elseif($gatewayListeners.Count){throw 'Port 8787 has a listener without managed LINE gateway state.'}
        $ngrokState=Read-NgrokState -Paths $paths;$ngrokRunning=$false
        if($null-ne$ngrokState){$identity=Get-NgrokExecutableIdentity (Join-Path $lineRoot 'scripts');try{$null=Assert-NgrokProcessIdentity $ngrokState $identity;$ngrokRunning=$true}finally{$identity.Stream.Dispose()}}
        $allNgrok=@(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue)
        if($allNgrok.Count-ne([int]$ngrokRunning+[int]$portalNgrokRunning)){throw 'LINE/ngrok process and durable state disagree.'}
        if((Test-Path $paths.TunnelStateFile)-or(Test-Path $paths.FunnelStateFile)){throw 'A non-ngrok LINE public transport is active; portal switch is not authorized.'}
        [pscustomobject]@{GatewayRunning=$gatewayRunning;NgrokRunning=$ngrokRunning}
    }
}

function Set-LineBaseline {
    param([Parameter(Mandatory)][bool]$GatewayRunning,[Parameter(Mandatory)][bool]$NgrokRunning,[switch]$RecoveryMode)
    if($NgrokRunning-and-not$GatewayRunning){throw 'Invalid LINE baseline.'}
    $lineRoot="$env:ProgramData\Aevrynd\LineGateway\scripts";$truth=Get-LineServiceTruth -RecoveryMode:$RecoveryMode
    if($truth.NgrokRunning-and-not$NgrokRunning){& (Join-Path $lineRoot 'Stop-Ngrok.ps1');$truth=Get-LineServiceTruth -RecoveryMode:$RecoveryMode}
    if($truth.GatewayRunning-and-not$GatewayRunning){& (Join-Path $lineRoot 'Stop-Gateway.ps1');$truth=Get-LineServiceTruth -RecoveryMode:$RecoveryMode}
    if($GatewayRunning-and-not$truth.GatewayRunning){& (Join-Path $lineRoot 'Start-Gateway.ps1');$truth=Get-LineServiceTruth -RecoveryMode:$RecoveryMode}
    if($NgrokRunning-and-not$truth.NgrokRunning){& (Join-Path $lineRoot 'Start-Ngrok.ps1');$truth=Get-LineServiceTruth -RecoveryMode:$RecoveryMode}
    if([bool]$truth.GatewayRunning-ne$GatewayRunning-or[bool]$truth.NgrokRunning-ne$NgrokRunning){throw 'LINE services did not reach the requested durable baseline.'}
}

function Invoke-PortalPreflight {
    $verified=Test-InstalledPortal;$paths=$verified.Paths;$install=$verified.Install
    Add-PortalCredentialInterop
    if(-not[AevryndPortalCredential]::ValidGmail($script:GmailCredentialTarget)){throw 'Gmail App Password is missing or invalid in this Windows user credential context; use Set-GmailCredential.ps1 -Replace.'}
    if(-not[AevryndPortalCredential]::Available($script:NgrokCredentialTarget,16,2048)){throw 'Ngrok authtoken is missing from this Windows user credential context.'}
    if(-not[AevryndPortalCredential]::Available($script:NgrokApiCredentialTarget,16,2048)){throw 'Ngrok API key is missing from this Windows user credential context.'}
    if($install.domain-cne$script:PortalDomain-or[int]$install.port-ne$script:PortalPort){throw 'Installed portal domain or port differs from the fixed deployment tuple.'}
    $smtp=[AevryndPortalCredential]::StartPortalPreflight([string]$install.pythonExe,@('-B',(Join-Path $paths.App 'server.py'),'--smtp-preflight'),$script:GmailCredentialTarget,$paths.SmtpDiagnostic)
    if(-not$smtp.WaitForExit(30000)){$smtp.Kill($true);throw 'SMTP TLS/login preflight timed out.'}
    $stdout=$smtp.StandardOutput.ReadToEnd().Trim();$stderr=$smtp.StandardError.ReadToEnd().Trim()
    if($smtp.ExitCode-ne0-or$stdout-cne'SMTP_PREFLIGHT_OK'){throw "SMTP TLS/login preflight failed safely: $stderr"}
    Test-NgrokReservedDomainAuthorization -Verified $verified
    $line=Get-LineServiceTruth
    $previewPid=0
    $listeners=@(Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort -ErrorAction SilentlyContinue)
    if($listeners.Count-gt0){
        if($listeners.Count-ne1-or$listeners[0].LocalAddress-cne'127.0.0.1'){throw 'Port 8792 has an unknown or non-loopback listener.'}
        $managedState=Read-PortalRuntimeState -Path $paths.PortalState -Kind portal
        if($null-ne$managedState){$null=Assert-PortalRuntime -Verified $verified -State $managedState}
        else{$previewPid=[int]$listeners[0].OwningProcess;$null=Get-VerifiedPreviewProcess -Verified $verified -ProcessId $previewPid}
    }
    return [pscustomobject]@{Verified=$verified;LineGatewayRunning=[bool]$line.GatewayRunning;LineNgrokRunning=[bool]$line.NgrokRunning;PreviewPid=$previewPid}
}

function Test-NgrokReservedDomainAuthorization {
    param([Parameter(Mandatory)][object]$Verified)
    Add-PortalCredentialInterop
    # 管理 API 是唯一能在不建立公開 tunnel 的前提下驗證固定網域授權的方法。
    $uri='https://api.ngrok.com/reserved_domains?limit=100';$found=$false
    for($page=0;$page-lt20;$page++){
        $parsed=([AevryndPortalCredential]::NgrokApiGet($script:NgrokApiCredentialTarget,$uri)|ConvertFrom-Json -Depth 12)
        $names=@($parsed.PSObject.Properties.Name|Sort-Object)
        if((@($names|Where-Object{$_-notin@('next_page_uri','reserved_domains','uri')})).Count-or$null-eq$parsed.reserved_domains){throw 'Ngrok domain response schema is invalid.'}
        foreach($record in @($parsed.reserved_domains)){if($record.domain-is[string]-and$record.domain-ceq$script:PortalDomain){$found=$true;break}}
        if($found){break}
        $next=[string]$parsed.next_page_uri;if([string]::IsNullOrEmpty($next)){break}
        $uri=Assert-NgrokPaginationUri $next
    }
    if(-not$found){throw 'Ngrok fixed-domain authorization failed safely.'}
}

function Assert-NgrokPaginationUri {
    param([Parameter(Mandatory)][string]$Value)
    try{$nextUri=[Uri]$Value}catch{throw 'Ngrok pagination URI is invalid.'}
    if(-not$nextUri.IsAbsoluteUri-or$nextUri.Scheme-cne'https'-or$nextUri.Host-cne'api.ngrok.com'-or$nextUri.Port-ne443-or-not[string]::IsNullOrEmpty($nextUri.UserInfo)-or-not[string]::IsNullOrEmpty($nextUri.Fragment)-or-not$nextUri.AbsolutePath.StartsWith('/reserved_domains',[StringComparison]::Ordinal)){throw 'Ngrok pagination escaped the fixed API origin.'}
    return $nextUri.AbsoluteUri
}

function Read-PortalSwitchState {
    $paths=Get-PortalPaths;$state=Read-PortalJson -Path $paths.SwitchState
    if($null-eq$state){return $null}
    $expected=@('baseline','schemaVersion','stage','transactionId','updatedUtc')
    if((@($state.PSObject.Properties.Name|Sort-Object)-join',')-cne(@($expected|Sort-Object)-join',')-or$state.schemaVersion-ne1-or$state.transactionId-isnot[string]-or$state.transactionId-notmatch'^[0-9a-f]{32}$'-or$state.stage-notin@('prepared','line_stopping','line_stopped','preview_stopped','portal_started','ngrok_started','committed','rollback_complete','restore_failed','portal_failed')){throw 'Portal switch journal is invalid.'}
    $baseline=$state.baseline
    if($null-eq$baseline-or(@($baseline.PSObject.Properties.Name|Sort-Object)-join',')-cne(@('lineGatewayRunning','lineNgrokRunning','previewRunning'|Sort-Object)-join',')-or$baseline.lineGatewayRunning-isnot[bool]-or$baseline.lineNgrokRunning-isnot[bool]-or$baseline.previewRunning-isnot[bool]-or($baseline.lineNgrokRunning-and-not$baseline.lineGatewayRunning)){throw 'Portal switch baseline is invalid.'}
    try{$updated=[DateTimeOffset]::ParseExact([string]$state.updatedUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw 'Portal switch journal timestamp is invalid.'}
    if($updated-gt[DateTimeOffset]::UtcNow.AddMinutes(5)){throw 'Portal switch journal timestamp is unreasonable.'}
    return $state
}

function Write-PortalSwitchState {
    param([Parameter(Mandatory)][object]$State,[Parameter(Mandatory)][string]$Stage)
    $State.stage=$Stage;$State.updatedUtc=[DateTimeOffset]::UtcNow.ToString('o')
    $path=(Get-PortalPaths).SwitchState;Write-PortalJson -Path $path -Value $State;Protect-PrivilegedStateFile $path
}

function Read-PortalRuntimeIntent {
    $state=Read-PortalJson -Path (Get-PortalPaths).RuntimeIntent
    if($null-eq$state){return $null}
    $expected=@('previewRunning','schemaVersion','stage','transactionId','updatedUtc')
    if((@($state.PSObject.Properties.Name|Sort-Object)-join',')-cne(@($expected|Sort-Object)-join',')-or$state.schemaVersion-ne1-or$state.transactionId-isnot[string]-or$state.transactionId-notmatch'^[0-9a-f]{32}$'-or$state.previewRunning-isnot[bool]-or$state.stage-notin@('line_stopped','preview_stopped','portal_started','ngrok_started','committed','portal_failed','rollback_complete')){throw 'Portal runtime intent is invalid.'}
    return $state
}

function Write-PortalRuntimeIntent {
    param([Parameter(Mandatory)][object]$State,[Parameter(Mandatory)][string]$Stage)
    $State.stage=$Stage;$State.updatedUtc=[DateTimeOffset]::UtcNow.ToString('o')
    $path=(Get-PortalPaths).RuntimeIntent;Write-PortalJson -Path $path -Value $State;Protect-PortalStateFile $path
}

function Get-VerifiedPreviewProcess {
    param([Parameter(Mandatory)][object]$Verified,[Parameter(Mandatory)][int]$ProcessId)
    if($ProcessId-lt1){return $null}
    $sourceServer=[IO.Path]::GetFullPath([string]$Verified.Install.previewServerPath)
    $sourceIdentity=Get-VerifiedFileHash $sourceServer;try{if($sourceIdentity.Hash-cne[string]$Verified.Install.previewServerSha256){throw 'Preview source identity changed.'}}finally{$sourceIdentity.Stream.Dispose()}
    $record=Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" -ErrorAction SilentlyContinue;if($null-eq$record){return $null}
    $allowed=@(
        ('"'+[regex]::Escape([string]$Verified.Install.pythonExe)+'" '+[regex]::Escape($sourceServer)+' --port 8792 --dev-show-otp'),
        ('"'+[regex]::Escape([string]$Verified.Install.pythonExe)+'" '+[regex]::Escape($sourceServer)+' --host 127\.0\.0\.1 --port 8792 --dev-show-otp')
    )
    if([IO.Path]::GetFullPath([string]$record.ExecutablePath)-cne[IO.Path]::GetFullPath([string]$Verified.Install.pythonExe)-or-not($allowed|Where-Object{[string]$record.CommandLine-match('^'+$_+'$')})){throw 'Preview PID identity changed.'}
    $listeners=@(Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort -ErrorAction SilentlyContinue)
    if($listeners.Count-ne1-or$listeners[0].LocalAddress-cne'127.0.0.1'-or[int]$listeners[0].OwningProcess-ne$ProcessId){throw 'Preview listener identity changed.'}
    return Get-Process -Id $ProcessId -ErrorAction Stop
}

function Start-RecordedPreview {
    param([Parameter(Mandatory)][object]$Verified)
    $existing=@(Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort -ErrorAction SilentlyContinue)
    if($existing.Count){if($existing.Count-eq1){$null=Get-VerifiedPreviewProcess -Verified $Verified -ProcessId ([int]$existing[0].OwningProcess);return};throw 'Cannot restore preview because port 8792 has ambiguous listeners.'}
    $process=Start-Process -FilePath $Verified.Install.pythonExe -ArgumentList @($Verified.Install.previewServerPath,'--port','8792','--dev-show-otp') -WindowStyle Hidden -PassThru
    for($i=0;$i-lt20;$i++){try{$listeners=@(Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort -ErrorAction Stop);if($listeners.Count-eq1-and[int]$listeners[0].OwningProcess-eq$process.Id){return}}catch{};Start-Sleep -Milliseconds 250}
    if(-not$process.HasExited){$process.Kill($true)};throw 'Preview rollback failed.'
}

function Invoke-LimitedPortalTask {
    param([Parameter(Mandatory)][ValidateSet('Preflight','Start','Stop','RestorePreview','ResumePortal')][string]$Operation,[switch]$AllowFailure)
    Assert-Administrator
    $paths=Get-PortalPaths
    $taskName="Aevrynd Assistant Portal $Operation"
    $task=Get-ScheduledTask -TaskName $taskName -TaskPath '\Aevrynd\' -ErrorAction Stop
    if($task.State-eq'Running'){throw "Limited operation $Operation is already running."}
    Remove-Item -LiteralPath $paths.TaskResult -Force -ErrorAction SilentlyContinue
    $notBefore=[DateTimeOffset]::UtcNow
    Start-ScheduledTask -TaskName $taskName -TaskPath '\Aevrynd\'
    $deadline=[DateTimeOffset]::UtcNow.AddMinutes(2)
    do{
        Start-Sleep -Milliseconds 250
        $result=Read-PortalJson -Path $paths.TaskResult
        $completed=$null;try{$completed=[DateTimeOffset]::ParseExact([string]$result.completedUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{}
        if($null-ne$result-and$result.operation-ceq$Operation-and$result.schemaVersion-eq1-and$result.success-is[bool]-and$result.safeCode-in@('ok','operation_failed')-and$null-ne$completed-and$completed-ge$notBefore.AddSeconds(-1)){
            if(-not$result.success-and-not$AllowFailure){throw "Limited operation $Operation failed safely."}
            return $result
        }
    }while([DateTimeOffset]::UtcNow-lt$deadline)
    Stop-ScheduledTask -TaskName $taskName -TaskPath '\Aevrynd\' -ErrorAction SilentlyContinue
    $stopDeadline=[DateTimeOffset]::UtcNow.AddSeconds(10)
    do{$task=Get-ScheduledTask -TaskName $taskName -TaskPath '\Aevrynd\' -ErrorAction Stop;if($task.State-ne'Running'){break};Start-Sleep -Milliseconds 200}while([DateTimeOffset]::UtcNow-lt$stopDeadline)
    if($task.State-eq'Running'){throw "Limited operation $Operation timed out and could not be terminated."}
    $verified=Test-InstalledPortal -RecoveryOnly
    Stop-ProtectedPortalLaunchTree -Verified $verified -NotBefore $notBefore
    if(-not$AllowFailure){throw "Limited operation $Operation timed out and was terminated."}
}

function Disable-PortalStartupTask {
    Assert-Administrator
    foreach($name in @('Aevrynd Assistant Portal Start','Aevrynd Assistant Portal')){$task=Get-ScheduledTask -TaskName $name -TaskPath '\Aevrynd\' -ErrorAction SilentlyContinue;if($null-ne$task-and$task.State-ne'Disabled'){Stop-ScheduledTask -InputObject $task -ErrorAction SilentlyContinue;Disable-ScheduledTask -InputObject $task|Out-Null}}
}

function Assert-PortalStackStoppedForRecovery {
    $verified=Test-InstalledPortal -RecoveryOnly;$paths=$verified.Paths
    $launch=Read-PortalJson -Path $paths.LaunchIntent
    if($null-ne$launch){$names=@($launch.PSObject.Properties.Name|Sort-Object);if(($names-join',')-cne((@('expectedCommandLine','expectedExecutable','kind','notBeforeUtc','schemaVersion')|Sort-Object)-join',')-or$launch.schemaVersion-ne1-or$launch.kind-notin@('portal','ngrok')){throw 'Protected launch intent is invalid.'};$notBefore=[DateTimeOffset]::ParseExact([string]$launch.notBeforeUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind);$expectedExe=if($launch.kind-ceq'portal'){[string]$verified.Install.pythonExe}else{[string]$verified.Install.ngrokExe};$expectedCommand=if($launch.kind-ceq'portal'){Get-PortalCommandLine $verified}else{Get-NgrokCommandLine $verified};if([IO.Path]::GetFullPath([string]$launch.expectedExecutable)-cne[IO.Path]::GetFullPath($expectedExe)-or[string]$launch.expectedCommandLine-cne$expectedCommand){throw 'Protected launch intent tuple is invalid.'};Stop-ProtectedPortalLaunchTree -Verified $verified -NotBefore $notBefore;Remove-Item -LiteralPath $paths.LaunchIntent -Force}
    $portal=Read-PortalRuntimeState -Path $paths.PortalState -Kind portal
    if($null-ne$portal){
        $process=Get-OwnedProcess -State $portal -ExpectedExecutable $verified.Install.pythonExe -ExpectedCommandLine (Get-PortalCommandLine $verified)
        if($null-ne$process){throw 'Managed portal is still running; LINE restore is not safe.'}
        Remove-Item -LiteralPath $paths.PortalState -Force
    }
    $ngrok=Read-PortalRuntimeState -Path $paths.NgrokState -Kind ngrok
    if($null-ne$ngrok){
        $process=Get-OwnedProcess -State $ngrok -ExpectedExecutable $verified.Install.ngrokExe -ExpectedCommandLine (Get-NgrokCommandLine $verified)
        if($null-ne$process){throw 'Managed portal ngrok is still running; LINE restore is not safe.'}
        Remove-Item -LiteralPath $paths.NgrokState -Force
    }
    if(@(Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort -ErrorAction SilentlyContinue).Count-or@(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue).Count){throw 'A public portal process exists without managed runtime state.'}
}
