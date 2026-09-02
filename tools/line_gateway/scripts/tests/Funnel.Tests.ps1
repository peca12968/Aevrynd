Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\Funnel.Common.ps1')
. (Join-Path $PSScriptRoot '..\Common.ps1')
$passed=0;$failed=0
function Run([string]$Name,[scriptblock]$Action){try{&$Action;$script:passed++;"PASS $Name"}catch{$script:failed++;"FAIL $Name : $($_.Exception.Message)"}}
function Reject([string]$Message,[scriptblock]$Action){try{&$Action|Out-Null}catch{if($_.Exception.Message-notlike"*$Message*"){throw "Expected '$Message', received '$($_.Exception.Message)'."};return};throw 'Hostile fixture was accepted.'}
$root=Join-Path ([IO.Path]::GetTempPath()) "aevrynd-funnel-tests-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $root|Out-Null
try{
    $env:AEVRYND_FUNNEL_TEST_MODE='isolated-fixture'
    $fake=Join-Path $root 'fake-tailscale.ps1';$nodeFile=Join-Path $root 'node.json';$funnelFile=Join-Path $root 'funnel.json';$argumentFile=Join-Path $root 'arguments.txt'
    [IO.File]::WriteAllText($fake,@'
$joined=$args-join ' '
[IO.File]::WriteAllText($env:AEVRYND_FUNNEL_TEST_ARGUMENTS,$joined)
if($joined-ceq 'status --json'){Get-Content -Raw -LiteralPath $env:AEVRYND_FUNNEL_TEST_NODE;exit 0}
if($joined-ceq 'funnel status --json'){Get-Content -Raw -LiteralPath $env:AEVRYND_FUNNEL_TEST_STATUS;exit 0}
if($joined-ceq 'ux-timeout'){Write-Output 'Visit https://login.tailscale.com/a/OfficialToken_123';Write-Output 'ignore https://attacker.example/a/secret';Start-Sleep -Seconds 5;exit 0}
Write-Error 'unexpected fake CLI command';exit 41
'@,[Text.UTF8Encoding]::new($false))
    $fakeStream=[IO.FileStream]::new($fake,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$fakeCommand=[pscustomobject]@{ExecutablePath=[IO.Path]::GetFullPath($fake);Stream=$fakeStream;TestScript=$fake}
    $env:AEVRYND_FUNNEL_TEST_ARGUMENTS=$argumentFile;$env:AEVRYND_FUNNEL_TEST_NODE=$nodeFile;$env:AEVRYND_FUNNEL_TEST_STATUS=$funnelFile
    $dns=$script:AevryndFunnelDnsName;$hostPort="$dns`:443"
    $node=[ordered]@{Version='fake-1';TUN=$true;BackendState='Running';Self=[ordered]@{Online=$true;DNSName="$dns.";ID='nFixture123';PublicKey=('nodekey:'+'1'*64);TailscaleIPs=@('100.115.174.48')};CurrentTailnet=[ordered]@{MagicDNSEnabled=$true;MagicDNSSuffix='tail972fc3.ts.net'}}
    [IO.File]::WriteAllText($nodeFile,($node|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    function ActiveStatus {
        return [ordered]@{TCP=[ordered]@{'443'=[ordered]@{HTTPS=$true}};Web=[ordered]@{$hostPort=[ordered]@{Handlers=[ordered]@{'/webhook'=[ordered]@{Proxy='http://127.0.0.1:8787'}}}};AllowFunnel=[ordered]@{$hostPort=$true}}
    }
    Run 'fake CLI validates fixed node identity' {$identity=Get-TailscaleNodeIdentity $fakeCommand;if($identity.DnsName-cne$dns){throw 'Wrong DNS identity.'};if((Get-Content -Raw $argumentFile)-cne'status --json'){throw 'CLI arguments changed.'}}
    Run 'empty authoritative status is disabled' {[IO.File]::WriteAllText($funnelFile,'{}');$snapshot=Get-FunnelStatusSnapshot $fakeCommand $dns;if($snapshot.Mode-ne'disabled'){throw 'Expected disabled.'}}
    Run 'exact HTTPS 443 webhook proxy is active' {[IO.File]::WriteAllText($funnelFile,((ActiveStatus)|ConvertTo-Json -Depth 8));$snapshot=Get-FunnelStatusSnapshot $fakeCommand $dns;if($snapshot.Mode-ne'active'){throw 'Expected active.'}}
    foreach($case in @(
        @{name='root wildcard path';edit={param($x)$x.Web[$hostPort].Handlers=[ordered]@{'/'=[ordered]@{Proxy='http://127.0.0.1:8787'}}};error='unsupported JSON field'},
        @{name='additional path';edit={param($x)$x.Web[$hostPort].Handlers['/extra']=[ordered]@{Proxy='http://127.0.0.1:8787'}};error='unsupported JSON field'},
        @{name='redirect backend';edit={param($x)$x.Web[$hostPort].Handlers['/webhook'].Proxy='https://attacker.example'};error='backend is outside'},
        @{name='raw TCP field';edit={param($x)$x.TCP['443'].TCPForward='127.0.0.1:8787'};error='unsupported JSON field'},
        @{name='wrong public port';edit={param($x)$x.TCP=[ordered]@{'8443'=[ordered]@{HTTPS=$true}}};error='unsupported JSON field'},
        @{name='extra hostname';edit={param($x)$x.Web['other.tail972fc3.ts.net:443']=$x.Web[$hostPort]};error='unsupported JSON field'},
        @{name='funnel permission false';edit={param($x)$x.AllowFunnel[$hostPort]=$false};error='permission is not enabled'},
        @{name='extra top level field';edit={param($x)$x.Services=@{}};error='unsupported JSON field'}
    )){
        Run "reject $($case.name)" {$fixture=ActiveStatus;&$case.edit $fixture;[IO.File]::WriteAllText($funnelFile,($fixture|ConvertTo-Json -Depth 8));Reject $case.error {Get-FunnelStatusSnapshot $fakeCommand $dns}}
    }
    Run 'reject duplicate JSON keys' {[IO.File]::WriteAllText($funnelFile,'{"TCP":{},"TCP":{},"Web":{},"AllowFunnel":{}}');Reject 'duplicate object keys' {Get-FunnelStatusSnapshot $fakeCommand $dns}}
    Run 'reject wrong self DNS identity' {$bad=($node|ConvertTo-Json -Depth 8|ConvertFrom-Json -AsHashtable);$bad.Self.DNSName='other.tail972fc3.ts.net.';[IO.File]::WriteAllText($nodeFile,($bad|ConvertTo-Json -Depth 8));Reject 'outside the fixed host allowlist' {Get-TailscaleNodeIdentity $fakeCommand};[IO.File]::WriteAllText($nodeFile,($node|ConvertTo-Json -Depth 8))}
    Run 'live executable handle prevents path replacement until CLI completes' {try{Rename-Item -LiteralPath $fake -NewName 'swapped.ps1';throw 'Executable replacement was not blocked.'}catch{if($_.Exception.Message-eq'Executable replacement was not blocked.'){throw}};$result=Get-TailscaleNodeIdentity -Executable $fakeCommand;if($result.DnsName-cne$dns){throw 'Held executable invocation failed.'};$fakeStream.Dispose();Rename-Item -LiteralPath $fake -NewName 'released.ps1';Rename-Item -LiteralPath (Join-Path $root 'released.ps1') -NewName 'fake-tailscale.ps1';$script:fakeStream=[IO.FileStream]::new($fake,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$script:fakeCommand=[pscustomobject]@{ExecutablePath=[IO.Path]::GetFullPath($fake);Stream=$script:fakeStream;TestScript=$fake}}
    Run 'authorization hint is allowlisted and activation command has bounded timeout' {$watch=[Diagnostics.Stopwatch]::StartNew();$result=Invoke-TailscaleCli $fakeCommand @('ux-timeout') -TimeoutSeconds 1 -ShowAuthorizationHint;$watch.Stop();if(-not$result.TimedOut-or$result.ExitCode-ne-1-or$watch.Elapsed.TotalSeconds-gt3){throw 'Authorization command did not obey its bounded timeout.'};if($result.AuthorizationUrls.Count-ne1-or$result.AuthorizationUrls[0]-cne'https://login.tailscale.com/a/OfficialToken_123'){throw 'Authorization URL allowlist accepted an unsafe URL or missed the official URL.'}}
    Run 'state binds executable node host port path and target' {
        $paths=@{State=$root;FunnelStateFile=(Join-Path $root 'funnel-state.json')};$exe=[pscustomobject]@{ExecutablePath='C:\Program Files\Tailscale\tailscale.exe';ExecutableSha256=('a'*64);ExecutableVersion='1.102.3'};$identity=Get-TailscaleNodeIdentity $fakeCommand;$identity|Add-Member ExecutablePath $exe.ExecutablePath;$identity|Add-Member ExecutableSha256 $exe.ExecutableSha256;$identity|Add-Member ExecutableVersion $exe.ExecutableVersion
        Write-FunnelState -Paths $paths -Status active -Executable $identity -Node $identity -LaunchNonce ('b'*32) -CreatedUtc ([DateTimeOffset]::UtcNow.ToString('o')) -ActivatedUtc ([DateTimeOffset]::UtcNow.ToString('o'));$state=Read-FunnelState $paths;Assert-FunnelIdentityMatchesState $state $identity $identity
        $state.target='http://127.0.0.1:9999';[IO.File]::WriteAllText($paths.FunnelStateFile,($state|ConvertTo-Json));Reject 'transport tuple is invalid' {Read-FunnelState $paths}
    }
    Run 'timeout cleanup removes only the exact pending activation state' {$cleanupRoot=Join-Path $root 'pending-cleanup';New-Item -ItemType Directory -Path $cleanupRoot|Out-Null;$paths=@{State=$cleanupRoot;FunnelStateFile=(Join-Path $cleanupRoot 'funnel-state.json')};$identity=Get-TailscaleNodeIdentity $fakeCommand;$identity|Add-Member ExecutablePath 'C:\Program Files\Tailscale\tailscale.exe';$identity|Add-Member ExecutableSha256 ('a'*64);$identity|Add-Member ExecutableVersion '1.102.3';$nonce='e'*32;Write-FunnelState -Paths $paths -Status pending -Executable $identity -Node $identity -LaunchNonce $nonce -CreatedUtc ([DateTimeOffset]::UtcNow.ToString('o'));Reject 'does not match' {Remove-ExactPendingFunnelState -Paths $paths -LaunchNonce ('f'*32)};if(-not(Test-Path $paths.FunnelStateFile)){throw 'Mismatched cleanup deleted pending state.'};Remove-ExactPendingFunnelState -Paths $paths -LaunchNonce $nonce;if(Test-Path $paths.FunnelStateFile){throw 'Exact pending state was not cleaned up.'}}
    Run 'state rejects scalar confusion timestamps and reparse directory' {
        $stateRoot=Join-Path $root 'state-fixtures';New-Item -ItemType Directory -Path $stateRoot|Out-Null;$paths=@{State=$stateRoot;FunnelStateFile=(Join-Path $stateRoot 'funnel-state.json')};$identity=Get-TailscaleNodeIdentity $fakeCommand;$identity|Add-Member ExecutablePath 'C:\Program Files\Tailscale\tailscale.exe';$identity|Add-Member ExecutableSha256 ('a'*64);$identity|Add-Member ExecutableVersion '1.102.3'
        foreach($case in @(
            @{field='schemaVersion';value='1';error='numeric fields'},@{field='schemaVersion';value=$true;error='numeric fields'},@{field='httpsPort';value='443';error='numeric fields'},@{field='httpsPort';value=$false;error='numeric fields'},
            @{field='status';value=@('active','active');error='JSON string'},@{field='dnsName';value=@{value=$dns};error='JSON string'},@{field='launchNonce';value=$true;error='JSON string'},@{field='activatedUtc';value=$null;error='requires a string activation time'},
            @{field='createdUtc';value='2026-01-01T00:00:00Z';error='canonical UTC timestamp'},@{field='activatedUtc';value='2025-01-01T00:00:00.0000000+00:00';error='valid sequence'}
        )){
            $created=[DateTimeOffset]::UtcNow.AddSeconds(-1).ToString('o');Write-FunnelState -Paths $paths -Status active -Executable $identity -Node $identity -LaunchNonce ('c'*32) -CreatedUtc $created -ActivatedUtc ([DateTimeOffset]::UtcNow.ToString('o'));$doc=Read-FunnelState $paths;$doc[$case.field]=$case.value;[IO.File]::WriteAllText($paths.FunnelStateFile,($doc|ConvertTo-Json -Depth 8));try{Reject $case.error {Read-FunnelState $paths}}catch{throw "$($case.field) fixture failed: $($_.Exception.Message)"}
        }
        $real=Join-Path $root 'real-state';$junction=Join-Path $root 'linked-state';New-Item -ItemType Directory -Path $real|Out-Null;New-Item -ItemType Junction -Path $junction -Target $real|Out-Null;[IO.File]::WriteAllText((Join-Path $real 'funnel-state.json'),'{}');Reject 'reparse point' {Read-FunnelState @{State=$junction;FunnelStateFile=(Join-Path $junction 'funnel-state.json')}}
        $lockedRoot=Join-Path $root 'locked-state';New-Item -ItemType Directory -Path $lockedRoot|Out-Null;$lockedFile=Join-Path $lockedRoot 'funnel-state.json';[IO.File]::WriteAllText($lockedFile,'{}');$exclusive=[IO.FileStream]::new($lockedFile,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);try{try{Read-FunnelState @{State=$lockedRoot;FunnelStateFile=$lockedFile}|Out-Null;throw 'Locked state unexpectedly opened.'}catch{if($_.Exception.Message-eq'Locked state unexpectedly opened.'){throw}}}finally{$exclusive.Dispose()};Rename-Item -LiteralPath $lockedRoot -NewName 'locked-state-renamed'
        $writeRoot=Join-Path $root 'write-reparse';New-Item -ItemType Directory -Path $writeRoot|Out-Null;$outside=Join-Path $root 'outside-directory';New-Item -ItemType Directory -Path $outside|Out-Null;$linkedFile=Join-Path $writeRoot 'funnel-state.json';New-Item -ItemType Junction -Path $linkedFile -Target $outside|Out-Null;Reject 'reparse point' {Write-FunnelState -Paths @{State=$writeRoot;FunnelStateFile=$linkedFile} -Status active -Executable $identity -Node $identity -LaunchNonce ('d'*32) -CreatedUtc ([DateTimeOffset]::UtcNow.ToString('o')) -ActivatedUtc ([DateTimeOffset]::UtcNow.ToString('o'))};Rename-Item -LiteralPath $writeRoot -NewName 'write-reparse-renamed'
    }
    Run 'global mutex excludes another process handles abandonment and covers all entry points' {
        $common=(Resolve-Path (Join-Path $PSScriptRoot '..\Common.ps1')).Path;$child=Join-Path $root 'mutex-child.ps1';[IO.File]::WriteAllText($child,@'
param([string]$Common,[string]$Mode,[string]$Marker)
. $Common
if($Mode-eq 'contend'){try{$null=Enter-PublicTransportLock -TimeoutMilliseconds 200;exit 9}catch{if($_.Exception.Message-notlike '*Timed out*'){exit 8};exit 0}}
$lock=Enter-PublicTransportLock -TimeoutMilliseconds 2000
[IO.File]::WriteAllText($Marker,'ready')
Start-Sleep -Milliseconds 750
[Environment]::Exit(0)
'@)
        $held=Enter-PublicTransportLock -TimeoutMilliseconds 1000;try{if($held.Name-cne'Global\AevryndLineGatewayPublicTransportControl'){throw 'Mutex is not machine-wide.'};& pwsh -NoLogo -NoProfile -File $child -Common $common -Mode contend;if($LASTEXITCODE-ne0){throw "Cross-process exclusion failed: $LASTEXITCODE"}}finally{Exit-PublicTransportLock $held}
        $marker=Join-Path $root 'mutex-ready';$psi=[Diagnostics.ProcessStartInfo]::new('pwsh');$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;foreach($arg in @('-NoLogo','-NoProfile','-File',$child,'-Common',$common,'-Mode','abandon','-Marker',$marker)){$null=$psi.ArgumentList.Add($arg)};$process=[Diagnostics.Process]::Start($psi);$deadline=[DateTime]::UtcNow.AddSeconds(3);while(-not(Test-Path -LiteralPath $marker)){if([DateTime]::UtcNow-ge$deadline){$process.Kill();throw 'Abandon fixture did not acquire the mutex.'};Start-Sleep -Milliseconds 25};$recovered=Enter-PublicTransportLock -TimeoutMilliseconds 3000;try{if(-not$recovered.Abandoned){throw 'Abandoned mutex was not reported.'}}finally{Exit-PublicTransportLock $recovered};$process.WaitForExit();if($process.ExitCode-ne0){throw 'Abandon fixture failed.'}
        foreach($name in @('Install.ps1','Uninstall.ps1','Register-Startup.ps1','Discover-Allowlist.ps1','Start-Tunnel.ps1','Stop-Tunnel.ps1','Start-Funnel.ps1','Stop-Funnel.ps1')){$content=Get-Content -Raw (Join-Path $PSScriptRoot "..\$name");if(-not$content.Contains('Enter-PublicTransportLock')-or-not$content.Contains('Exit-PublicTransportLock')){throw "Public entry point lacks shared lock: $name"}}
        $install=Get-Content -Raw (Join-Path $PSScriptRoot '..\Install.ps1');foreach($providerFile in @('Funnel.Common.ps1','Start-Funnel.ps1','Stop-Funnel.ps1','Test-FunnelConfiguration.ps1')){if(([regex]::Matches($install,[regex]::Escape("'$providerFile'"))).Count-ne1){throw "Install manifest must contain exactly one $providerFile entry."}}
    }
    Run 'start and stop scripts contain only fixed mutation tuples and authoritative checks' {
        $start=Get-Content -Raw (Join-Path $PSScriptRoot '..\Start-Funnel.ps1');$stop=Get-Content -Raw (Join-Path $PSScriptRoot '..\Stop-Funnel.ps1')
        foreach($marker in @("'--https=443'","'--set-path=/webhook'","'http://127.0.0.1:8787'",'Get-FunnelStatusSnapshot')){if(-not$start.Contains($marker)-or-not$stop.Contains($marker)){throw "Missing fixed marker: $marker"}}
        foreach($forbidden in @('--tcp=','--tls-terminated-tcp=','funnel reset','Invoke-WebRequest')){if($start.Contains($forbidden)-or$stop.Contains($forbidden)){throw "Forbidden mutation marker: $forbidden"}}
    }
}finally{
    if($null-ne(Get-Variable fakeStream -ErrorAction SilentlyContinue)-and$null-ne$fakeStream){$fakeStream.Dispose()}
    Remove-Item Env:AEVRYND_FUNNEL_TEST_ARGUMENTS,Env:AEVRYND_FUNNEL_TEST_NODE,Env:AEVRYND_FUNNEL_TEST_STATUS,Env:AEVRYND_FUNNEL_TEST_MODE -ErrorAction SilentlyContinue
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
}
if($failed){throw "$failed Funnel tests failed; $passed passed."}
"Funnel hostile tests passed ($passed/20)."
