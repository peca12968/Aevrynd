Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\Portal.Common.ps1')

function Assert-True([bool]$Value,[string]$Message){if(-not$Value){throw $Message}}

# 實際建立受控程序，驗證 PID、exe 與 command line 三者都必須吻合。
$pwsh=[IO.Path]::GetFullPath((Join-Path $PSHOME 'pwsh.exe'))
$info=[Diagnostics.ProcessStartInfo]::new($pwsh);$info.UseShellExecute=$false;$info.CreateNoWindow=$true
foreach($argument in @('-NoProfile','-Command','Start-Sleep -Seconds 20')){$info.ArgumentList.Add($argument)}
$process=[Diagnostics.Process]::Start($info)
try{
    Start-Sleep -Milliseconds 300
    $record=Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)" -ErrorAction Stop
    $state=[pscustomobject]@{pid=[long]$process.Id}
    Assert-True ($null-ne(Get-OwnedProcess -State $state -ExpectedExecutable $pwsh -ExpectedCommandLine ([string]$record.CommandLine))) 'Actual process identity acceptance failed.'
    $rejected=$false;try{$null=Get-OwnedProcess -State $state -ExpectedExecutable $pwsh -ExpectedCommandLine ([string]$record.CommandLine+' --tampered')}catch{$rejected=$true}
    Assert-True $rejected 'Tampered process command line was accepted.'
}finally{if(-not$process.HasExited){$process.Kill($true);$process.WaitForExit()}}

# 實際讀取目前使用者的 Credential Manager context，但只查不存在的固定測試 target。
Add-PortalCredentialInterop
Assert-True (-not[AevryndPortalCredential]::Available('Aevrynd/AssistantPortal/DefinitelyMissingTestCredential',16,128)) 'Missing credential unexpectedly resolved.'
$canonicalPointer=[Runtime.InteropServices.Marshal]::StringToCoTaskMemUni('AbCdEfGh12345678');$groupedPointer=[Runtime.InteropServices.Marshal]::StringToCoTaskMemUni('AbCd EfGh 1234 5678')
try{Assert-True ([AevryndPortalCredential]::ValidGmailBlob($canonicalPointer,32)) 'Canonical Gmail blob was rejected.';Assert-True (-not[AevryndPortalCredential]::ValidGmailBlob($groupedPointer,38)) 'Legacy grouped Gmail blob was incorrectly retained.';Assert-True (-not[AevryndPortalCredential]::ValidGmailBlob($canonicalPointer,30)) 'Non-32-byte Gmail blob was accepted.'}finally{[Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($canonicalPointer);[Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($groupedPointer)}

$valid=[pscustomobject]@{tunnels=@([pscustomobject]@{public_url='https://sample-cupid-overdrawn.ngrok-free.dev';proto='https';config=[pscustomobject]@{addr='http://127.0.0.1:8792'}})}
Assert-NgrokTunnelDocument $valid
$invalid=[pscustomobject]@{tunnels=@([pscustomobject]@{public_url='https://wrong.ngrok-free.dev';proto='https';config=[pscustomobject]@{addr='http://127.0.0.1:8792'}})}
$rejected=$false;try{Assert-NgrokTunnelDocument $invalid}catch{$rejected=$true};Assert-True $rejected 'Invalid ngrok local API tuple was accepted.'

# 實際 socket 驗證 listener owner，並確認錯誤 PID 必定拒絕。
$listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0);$listener.Start()
try{$port=([Net.IPEndPoint]$listener.LocalEndpoint).Port;Assert-ExactLoopbackListener -Port $port -ProcessId $PID;$rejected=$false;try{Assert-ExactLoopbackListener -Port $port -ProcessId ($PID+1)}catch{$rejected=$true};Assert-True $rejected 'Listener owned by a different PID was accepted.'}finally{$listener.Stop()}

# 在第二步注入失敗，驗證 transaction wrapper 必定且只執行一次 rollback。
$trace=[Collections.Generic.List[string]]::new();$failed=$false
try{Invoke-FailSafeSequence -Steps @({$trace.Add('step1')},{throw 'injected_failure'},{ $trace.Add('step3')}) -Rollback {$trace.Add('rollback')}}catch{$failed=$true}
Assert-True $failed 'Injected transaction failure did not propagate.'
Assert-True (($trace -join ',')-ceq'step1,rollback') 'Injected transaction failure did not perform exact rollback.'

# 在 state/ACL commit 區段注入失敗，實際子程序必須被清除且 cleanup 只執行一次。
$cleanupTrace=[Collections.Generic.List[string]]::new();$info=[Diagnostics.ProcessStartInfo]::new($pwsh);$info.UseShellExecute=$false;$info.CreateNoWindow=$true
foreach($argument in @('-NoProfile','-Command','Start-Sleep -Seconds 20')){$info.ArgumentList.Add($argument)}
$guarded=[Diagnostics.Process]::Start($info);$failed=$false
try{Invoke-ManagedProcessLaunchGuard -Process $guarded -Operation {throw 'injected_state_acl_failure'} -Cleanup {$cleanupTrace.Add('cleanup')}}catch{$failed=$true}
Assert-True $failed 'Injected state/ACL failure did not propagate.';Assert-True $guarded.HasExited 'Child survived state/ACL failure.';Assert-True ($cleanupTrace.Count-eq1) 'State cleanup was not invoked exactly once.'
$null=Assert-NgrokPaginationUri 'https://api.ngrok.com/reserved_domains?limit=100&before_id=x'
foreach($malicious in @('http://api.ngrok.com/reserved_domains','https://evil.example/reserved_domains','https://api.ngrok.com.evil.example/reserved_domains','https://api.ngrok.com:444/reserved_domains')){$rejected=$false;try{$null=Assert-NgrokPaginationUri $malicious}catch{$rejected=$true};Assert-True $rejected 'Malicious ngrok pagination URI was accepted.'}
Write-Output 'PORTAL_RUNTIME_TESTS_OK'
