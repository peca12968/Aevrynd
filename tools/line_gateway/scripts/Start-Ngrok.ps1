param([string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Ngrok.Common.ps1')
. (Join-Path $PSScriptRoot 'Funnel.Common.ps1')
$paths=Get-GatewayPaths -InstallRoot $InstallRoot;$lock=Enter-PublicTransportLock;$executableStream=$null
try{
 $null=Test-InstallationManifest $paths
 if((Test-Path $paths.TunnelStateFile)-or(Test-Path $paths.FunnelStateFile)){throw 'Ngrok, Cloudflare, and Funnel transports are mutually exclusive.'}
 Test-FunnelTransportAbsent
 if(@(Get-CimInstance Win32_Process -Filter "Name='cloudflared.exe'" -ErrorAction SilentlyContinue).Count){throw 'Cloudflared is running; refusing a second public transport.'}
 if(@(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction SilentlyContinue).Count-and-not(Test-Path $paths.NgrokStateFile)){throw 'An unmanaged ngrok process exists; refusing ambiguous ownership.'}
 $config=Read-NgrokDomainConfiguration $paths;$identity=Get-Content -Raw (Join-Path $PSScriptRoot 'ngrok-package-identity.json')|ConvertFrom-Json
 $exe=Assert-NgrokStorePackageIdentity $identity;if(-not(Test-Path $exe -PathType Leaf)){throw 'Pinned ngrok executable is unavailable.'}
 Assert-NoReparseAncestors $exe;$executableStream=[IO.FileStream]::new($exe,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);if([Aevrynd.SafePath]::FinalPath($executableStream.SafeFileHandle)-cne$exe){throw 'Ngrok executable final path differs from the pinned package identity.'}
 if((Get-FileHash -Algorithm SHA256 $exe).Hash.ToLowerInvariant()-cne[string]$identity.installedExecutableSha256){throw 'Ngrok executable hash differs from the pinned package identity.'}
 $launchIdentity=[pscustomobject]@{Path=$exe;Sha256=[string]$identity.installedExecutableSha256;Stream=$executableStream};Resolve-NgrokPendingLaunch $paths $launchIdentity
 try{$probe=Invoke-WebRequest -Uri 'http://127.0.0.1:8787/not-webhook' -MaximumRedirection 0 -TimeoutSec 2 -SkipHttpErrorCheck;if($probe.StatusCode-ge200-and$probe.StatusCode-lt300){throw 'Origin path guard accepted a non-webhook path.'}}catch [Net.Http.HttpRequestException]{$cursor=$_.Exception;$absent=$false;while($null-ne$cursor){if(($cursor-is[Net.Sockets.SocketException]-and$cursor.SocketErrorCode-eq[Net.Sockets.SocketError]::ConnectionRefused)-or$cursor-is[Threading.Tasks.TaskCanceledException]-or$cursor-is[TimeoutException]){$absent=$true;break};$cursor=$cursor.InnerException};if(-not$absent){throw}}catch [Threading.Tasks.TaskCanceledException]{}catch [TimeoutException]{}
 if(Test-Path $paths.NgrokStateFile){$old=Read-NgrokState $paths;$oldProcess=Get-Process -Id ([int]$old.pid) -ErrorAction SilentlyContinue;if($null-ne$oldProcess){throw 'Ngrok state references a live process; run Test-NgrokConfiguration.ps1 or Stop-Ngrok.ps1.'};Remove-Item $paths.NgrokStateFile -Force}
 if(-not('AevryndNgrokLauncher'-as[type])){Add-Type @'
using System;using System.Diagnostics;using System.Runtime.InteropServices;
public static class AevryndNgrokLauncher {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Credential { public uint Flags,Type; public IntPtr TargetName,Comment; public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten; public uint BlobSize; public IntPtr Blob; public uint Persist,AttributeCount; public IntPtr Attributes,Alias,User; }
 [DllImport("advapi32.dll",EntryPoint="CredReadW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool Read(string target,uint type,uint flags,out IntPtr value);[DllImport("advapi32.dll")]static extern void CredFree(IntPtr value);
 public static Process Start(string exe,string[] args){IntPtr p;if(!Read("Aevrynd/LineGateway/NgrokAuthtoken",1,0,out p))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());string token=null;try{var c=Marshal.PtrToStructure<Credential>(p);token=Marshal.PtrToStringUni(c.Blob,(int)c.BlobSize/2);}finally{CredFree(p);}if(String.IsNullOrWhiteSpace(token))throw new InvalidOperationException("Ngrok credential is empty.");var i=new ProcessStartInfo(exe){UseShellExecute=false,CreateNoWindow=true};foreach(var key in new System.Collections.Generic.List<string>(i.Environment.Keys)){if(key.StartsWith("NGROK_",StringComparison.OrdinalIgnoreCase))i.Environment.Remove(key);}foreach(var a in args)i.ArgumentList.Add(a);i.Environment["NGROK_AUTHTOKEN"]=token;var result=Process.Start(i);token=null;return result;}
}
'@}
 $arguments=@('http','http://127.0.0.1:8787',"--domain=$($config.Domain)",'--scheme=https','--inspect=false','--log=false')
 $launchNonce=[Guid]::NewGuid().ToString('N');Write-NgrokPendingState $paths $config.Domain $launchIdentity $launchNonce
 $process=[AevryndNgrokLauncher]::Start($exe,$arguments);$started=ConvertTo-NgrokProcessTimestamp $process.StartTime;$commandDeadline=[DateTime]::UtcNow.AddSeconds(3);$commandVerified=$false
 while([DateTime]::UtcNow-lt$commandDeadline-and-not$process.HasExited){$record=Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)" -ErrorAction SilentlyContinue;if($null-ne$record-and(Test-ExactNgrokCommandLine ([string]$record.CommandLine) $exe $config.Domain)){$commandVerified=$true;break};Start-Sleep -Milliseconds 100}
 if(-not$commandVerified){if(-not$process.HasExited){$process.Kill($true);throw 'Ngrok process command line did not match the fixed launch tuple.'};throw "Ngrok exited before command verification (exit code $($process.ExitCode)); verify MSIX CLI compatibility and credential state."}
 $listenerDeadline=[DateTime]::UtcNow.AddSeconds(5);$listenerVerified=$false
 while([DateTime]::UtcNow-lt$listenerDeadline-and-not$process.HasExited){$listeners=@(Get-NetTCPConnection -State Listen -LocalPort 4040 -ErrorAction SilentlyContinue);if($listeners.Count-eq1-and$listeners[0].LocalAddress-ceq'127.0.0.1'-and[long]$listeners[0].OwningProcess-eq$process.Id){$listenerVerified=$true;break};if($listeners.Count-gt0){break};Start-Sleep -Milliseconds 100}
 if(-not$listenerVerified){if(-not$process.HasExited){$process.Kill($true)};throw 'Ngrok local API is not uniquely bound to 127.0.0.1:4040 by the launched process.'}
 $deadline=[DateTime]::UtcNow.AddSeconds(20);$verified=$false
 while([DateTime]::UtcNow-lt$deadline-and-not$process.HasExited){try{$api=Invoke-RestMethod 'http://127.0.0.1:4040/api/tunnels' -TimeoutSec 2;$tunnels=@($api.tunnels);if($tunnels.Count-eq1-and$tunnels[0].public_url-ceq"https://$($config.Domain)"-and$tunnels[0].proto-ceq'https'-and$tunnels[0].config.addr-ceq'http://127.0.0.1:8787'){$verified=$true;break}}catch{};Start-Sleep -Milliseconds 200}
 if(-not$verified){if(-not$process.HasExited){$process.Kill($true)};throw 'Ngrok did not expose the exact pinned HTTPS endpoint and loopback origin.'}
 try{$publicGuard=Invoke-WebRequest -Method Post -Uri "https://$($config.Domain)/not-webhook" -ContentType 'application/json' -Body '{}' -UserAgent 'Aevrynd-Line-Gateway/1.0' -Headers @{'ngrok-skip-browser-warning'='1'} -MaximumRedirection 0 -TimeoutSec 5 -SkipHttpErrorCheck}catch{if(-not$process.HasExited){$process.Kill($true)};throw 'Public ngrok endpoint transport failed during path-guard verification.'}
 if($publicGuard.StatusCode-ge200-and$publicGuard.StatusCode-lt300){if(-not$process.HasExited){$process.Kill($true)};throw 'Public ngrok endpoint accepted a non-webhook API request.'}
 Write-NgrokState $paths $process.Id $started $config.Domain $exe ([string]$identity.installedExecutableSha256);Remove-Item -LiteralPath $paths.NgrokPendingFile -Force;Write-Output "Ngrok endpoint verified: https://$($config.Domain)/webhook"
}finally{if($null-ne$executableStream){$executableStream.Dispose()};Exit-PublicTransportLock $lock}
