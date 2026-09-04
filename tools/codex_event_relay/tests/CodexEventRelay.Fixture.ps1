$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$root=Split-Path -Parent $PSScriptRoot
$shared=Join-Path (Split-Path -Parent $root) 'codex_command_relay\CodexCommandRelay.psm1'
Import-Module $shared -Force;Import-Module (Join-Path $root 'CodexEventRelay.psm1') -Force
$script:assertions=0;$script:roots=[Collections.Generic.List[string]]::new()
function Assert-True([bool]$Value,[string]$Message){$script:assertions++;if(-not$Value){throw $Message}}
function Assert-Throws([scriptblock]$Action,[string]$Message){$threw=$false;try{&$Action}catch{$threw=$true};Assert-True $threw $Message}
function New-FixtureRoot([string]$Name){$path=Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-event-'+$Name+'-'+[Guid]::NewGuid().ToString('N'));$null=New-Item -ItemType Directory -Path $path;$script:roots.Add($path);return $path}
function New-CommandJson([string]$Id,[string]$Node='taichung',$Owned=@('engine/native/example.cpp')){[ordered]@{version=1;type='command';commandId=$Id;nodeId=$Node;ownedFiles=@($Owned);task='Inspect the owned fixture';createdAtUtc='2026-09-04T12:00:00.0000000+00:00'}|ConvertTo-Json -Compress}
$boundary={param($phase,$workspace)[pscustomobject]@{head='a'*40;status=@();worktree=@();metadata=@()}}
$pass={param($before,$after,$owned)[pscustomobject]@{passed=$true;violations=@()}}

try{
    # Canonical bytes 的固定向量可偵測換行、編碼、prefix 任一漂移。
    $vectorKey=[Convert]::FromBase64String('AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=')
    $canonical="GET`n/api/codex-relay/connect`ntaichung`n1788537600`nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA`n"
    $vectorResult=Get-HmacHex -Secret $vectorKey -Canonical $canonical
    Assert-True ($vectorResult-ceq'293899972d86bc7520b25138db524744bed63373e3bd3ab4b18ffc0fa0cc82ed') 'Canonical HMAC vector drifted'

    $workspace=New-FixtureRoot 'workspace';$stateRoot=New-FixtureRoot 'state';$audit=New-FixtureRoot 'audit';$secret=Join-Path (New-FixtureRoot 'secret') 'relay.secret'
    [IO.File]::WriteAllText($secret,'AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=',[Text.UTF8Encoding]::new($false))
    $script:launches=0;$script:connectHeaders=$null;$script:applicationSends=0
    $factory={param($attempt)[pscustomobject]@{
        Connect={param($self,$headers)$script:connectHeaders=$headers}
        Send={param($self,$payload)$script:applicationSends++}
        Receive={param($self)throw 'fixture idle disconnect'}
        Dispose={param($self)}
    }}
    Invoke-CodexEventRelay -Node taichung -Endpoint 'ws://127.0.0.1/api/codex-relay/connect' -SecretPath $secret -CodexPath 'C:\fixture\codex.exe' -SharedModulePath $shared -Workspace $workspace -StateRoot $stateRoot -AuditRoot $audit -TransportFactory $factory -RunCodex {$script:launches++;0} -MaximumConnections 1 -RequireSecureAcl:$false
    Assert-True ($script:launches-eq0-and$script:applicationSends-eq0) 'Idle connection launched a model or sent an application message'
    Assert-True ([string]$script:connectHeaders['X-Aevrynd-Node']-ceq'taichung') 'Handshake node header was wrong'
    Assert-True ([string]$script:connectHeaders['X-Aevrynd-Timestamp']-match'^\d{10,}$') 'Handshake timestamp was not Unix seconds'
    Assert-True ([string]$script:connectHeaders['X-Aevrynd-Nonce']-match'^[A-Za-z0-9_-]{43}$') 'Handshake nonce was invalid'
    $dynamic="GET`n/api/codex-relay/connect`ntaichung`n$($script:connectHeaders['X-Aevrynd-Timestamp'])`n$($script:connectHeaders['X-Aevrynd-Nonce'])`n"
    Assert-True ([string]$script:connectHeaders['X-Aevrynd-Signature']-ceq('sha256='+(Get-HmacHex -Secret $vectorKey -Canonical $dynamic))) 'Handshake signature did not cover exact headers'

    $readyRoot=New-FixtureRoot 'readiness';$requestPath=Join-Path $readyRoot 'readiness-request.json';$receiptPath=Join-Path $readyRoot 'readiness.json';$readyNonce=New-SecureNonce;$requestedAt=[DateTimeOffset]::UtcNow
    [IO.File]::WriteAllText($requestPath,(([ordered]@{schemaVersion=1;nodeId='taichung';installNonce=$readyNonce;requestedAtUtc=$requestedAt.ToString('o')}|ConvertTo-Json -Compress)),[Text.UTF8Encoding]::new($false))
    $published=Publish-EventRelayReadiness $requestPath $receiptPath 'taichung' -RequireSecureAcl:$false
    $validated=Test-EventRelayReadinessReceipt $receiptPath 'taichung' $readyNonce $requestedAt -RequireSecureAcl:$false -RequireLiveProcess
    Assert-True ($published.installNonce-ceq$readyNonce-and$validated.nodeId-ceq'taichung'-and[int]$validated.processId-eq$PID) 'Valid readiness receipt was not accepted'
    Assert-Throws {Test-EventRelayReadinessReceipt $receiptPath 'taichung' (New-SecureNonce) $requestedAt -RequireSecureAcl:$false} 'Wrong readiness nonce was accepted'
    foreach($badReceipt in @(
        [ordered]@{schemaVersion=1;nodeId='taoyuan';installNonce=$readyNonce;processId=$PID;startedAtUtc=[DateTimeOffset]::UtcNow.ToString('o')},
        [ordered]@{schemaVersion=1;nodeId='taichung';installNonce=$readyNonce;processId=$PID;startedAtUtc=$requestedAt.AddSeconds(-1).ToString('o')},
        [ordered]@{schemaVersion=1;nodeId='taichung';installNonce=$readyNonce;processId=0;startedAtUtc=[DateTimeOffset]::UtcNow.ToString('o')}
    )){[IO.File]::WriteAllText($receiptPath,($badReceipt|ConvertTo-Json -Compress),[Text.UTF8Encoding]::new($false));Assert-Throws {Test-EventRelayReadinessReceipt $receiptPath 'taichung' $readyNonce $requestedAt -RequireSecureAcl:$false} 'Wrong node/time/pid readiness receipt was accepted'}
    Remove-Item -LiteralPath $receiptPath -Force
    Assert-Throws {Test-EventRelayReadinessReceipt $receiptPath 'taichung' $readyNonce $requestedAt -RequireSecureAcl:$false} 'Transient Running state without a receipt was accepted'
    [IO.File]::WriteAllText($requestPath,(([ordered]@{schemaVersion=1;nodeId='taichung';installNonce=$readyNonce;requestedAtUtc=[DateTimeOffset]::UtcNow.AddMinutes(-11).ToString('o')}|ConvertTo-Json -Compress)),[Text.UTF8Encoding]::new($false))
    $staleResult=Publish-EventRelayReadiness $requestPath $receiptPath 'taichung' -RequireSecureAcl:$false
    Assert-True ($null-eq$staleResult-and-not(Test-Path -LiteralPath $receiptPath)) 'Stale readiness leftover was replayed'
    $script:rebootConnects=0;$rebootFactory={param($attempt)[pscustomobject]@{Connect={param($self,$headers)$script:rebootConnects++};Send={param($self,$payload)};Receive={param($self)throw 'fixture reboot idle'};Dispose={param($self)}}}
    Invoke-CodexEventRelay -Node taichung -Endpoint 'ws://127.0.0.1/api/codex-relay/connect' -SecretPath $secret -CodexPath 'C:\fixture\codex.exe' -SharedModulePath $shared -Workspace $workspace -StateRoot $readyRoot -AuditRoot $audit -TransportFactory $rebootFactory -RunCodex {throw 'stale request launched Codex'} -MaximumConnections 1 -RequireSecureAcl:$false
    Assert-True ($script:rebootConnects-eq1) 'Stale readiness leftover blocked normal reboot startup'

    $rollbackRoot=New-FixtureRoot 'rollback';$oldPath=Join-Path $rollbackRoot 'old.bin';$newPath=Join-Path $rollbackRoot 'new.bin';[IO.File]::WriteAllBytes($oldPath,[byte[]](1,2,3));$snapshots=New-EventRelayFileSnapshots @($oldPath,$newPath);[IO.File]::WriteAllBytes($oldPath,[byte[]](9,9));[IO.File]::WriteAllBytes($newPath,[byte[]](8));Restore-EventRelayFileSnapshots $snapshots
    Assert-True (([BitConverter]::ToString([IO.File]::ReadAllBytes($oldPath))-ceq'01-02-03')-and-not(Test-Path -LiteralPath $newPath)) 'Rollback did not restore prior bytes/existence exactly'

    $command=ConvertFrom-CommandMessage (New-CommandJson 'CMD-1') 'taichung' $workspace
    $state=New-InitialEventState 'taichung';$statePath=Join-Path $stateRoot 'claim-state.json';$script:sent=[Collections.Generic.List[object]]::new();$send={param($payload)$script:sent.Add(($payload|ConvertFrom-Json))};$script:runs=0
    Invoke-ClaimedCommand $command $state $statePath $audit $workspace 'fixture.exe' $send $boundary $pass {$script:runs++;0} -RequireSecureAcl:$false
    Invoke-ClaimedCommand $command $state $statePath $audit $workspace 'fixture.exe' $send $boundary $pass {$script:runs++;0} -RequireSecureAcl:$false
    Assert-True ($script:runs-eq1) 'Duplicate command launched Codex twice'
    Assert-True ($script:sent[0].type-ceq'ack'-and$script:sent[0].state-ceq'running'-and$script:sent[1].state-ceq'completed') 'Running/completed ACK order was invalid'
    Assert-True ((@($script:sent[0].PSObject.Properties.Name|Sort-Object)-join',')-ceq'commandId,exitCode,nodeId,occurredAtUtc,state,summary,type,version') 'ACK schema drifted'

    $beforeRuns=$script:runs
    Assert-Throws {ConvertFrom-CommandMessage (New-CommandJson 'WRONG' 'taoyuan') 'taichung' $workspace} 'Wrong-node command was accepted'
    Assert-Throws {ConvertFrom-CommandMessage (New-CommandJson 'DOTGIT' 'taichung' @('.git/config')) 'taichung' $workspace} '.git path was accepted'
    Assert-Throws {ConvertFrom-CommandMessage (New-CommandJson 'ESCAPE' 'taichung' @('../outside')) 'taichung' $workspace} 'Escaping path was accepted'
    Assert-True ($script:runs-eq$beforeRuns) 'Invalid command crossed the model boundary'

    # terminal ACK 斷線後留在 outbox；server 重送相同 command 時只補 ACK，不重跑。
    $pendingState=New-InitialEventState 'taichung';$pendingPath=Join-Path $stateRoot 'pending-state.json';$script:pendingSends=0;$script:pendingRuns=0
    $disconnecting={param($payload)$script:pendingSends++;if($script:pendingSends-eq2){throw 'fixture disconnect'}}
    Assert-Throws {Invoke-ClaimedCommand $command $pendingState $pendingPath $audit $workspace 'fixture.exe' $disconnecting $boundary $pass {$script:pendingRuns++;0} -RequireSecureAcl:$false} 'Terminal ACK disconnect did not surface'
    Assert-True ($script:pendingRuns-eq1-and@($pendingState.pendingMessages).Count-eq1) 'Disconnected terminal ACK was not durable'
    $script:replayed=[Collections.Generic.List[object]]::new();$reconnect={param($payload)$script:replayed.Add(($payload|ConvertFrom-Json))}
    Invoke-ClaimedCommand $command $pendingState $pendingPath $audit $workspace 'fixture.exe' $reconnect $boundary $pass {$script:pendingRuns++;0} -RequireSecureAcl:$false
    Assert-True ($script:pendingRuns-eq1-and$script:replayed[0].state-ceq'completed'-and@($pendingState.pendingMessages).Count-eq0) 'Reconnect did not drain terminal ACK exactly once'

    $crashState=New-InitialEventState 'taichung';$crashState.operations=@([pscustomobject][ordered]@{commandId='CRASHED';state='running';executionStarted=$true;startedAt=[DateTimeOffset]::UtcNow.ToString('o');finishedAt=$null;exitCode=$null;summary='Command claimed before execution'});$crashCommand=ConvertFrom-CommandMessage (New-CommandJson 'CRASHED') 'taichung' $workspace;$script:crashRuns=0
    Invoke-ClaimedCommand $crashCommand $crashState (Join-Path $stateRoot 'crash-state.json') $audit $workspace 'fixture.exe' {param($payload)} $boundary $pass {$script:crashRuns++;0} -RequireSecureAcl:$false
    Assert-True ($script:crashRuns-eq0-and$crashState.operations[0].state-ceq'failed'-and[int]$crashState.operations[0].exitCode-eq92) 'Interrupted launch was rerun instead of sealed failed'

    $failed=ConvertFrom-CommandMessage (New-CommandJson 'BOUNDARY') 'taichung' $workspace;$failState=New-InitialEventState 'taichung';$bad={param($before,$after,$owned)[pscustomobject]@{passed=$false;violations=@('HEAD changed')}}
    Invoke-ClaimedCommand $failed $failState (Join-Path $stateRoot 'boundary-state.json') $audit $workspace 'fixture.exe' {param($payload)} $boundary $bad {0} -RequireSecureAcl:$false
    Assert-True ($failState.operations[0].state-ceq'failed'-and[int]$failState.operations[0].exitCode-eq90) 'Boundary failure was not terminal failed/90'

    # Installer source是排程契約的一部分：只接受 hidden AtLogOn，且不得重新引入 repetition/once polling。
    $installer=[IO.File]::ReadAllText((Join-Path $root 'Install-CodexEventRelay.ps1'))
    Assert-True ($installer-match'New-ScheduledTaskTrigger\s+-AtLogOn') 'Installer does not create an AtLogOn trigger'
    Assert-True ($installer-notmatch'(?i)RepetitionInterval|New-ScheduledTaskTrigger\s+-Once') 'Installer contains a polling trigger'
    Assert-True ($installer-match'-NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass') 'Scheduled action is not hidden'
    Assert-True ($installer-match'Start-ScheduledTask\s+-TaskName\s+\$taskName') 'Installer does not start the hidden event relay immediately'
    Assert-True ($installer-match'Disable-ScheduledTask[\s\S]+Unregister-ScheduledTask') 'Legacy polling task migration is not disable-before-remove'
    $verifiedIndex=$installer.IndexOf("if(-not`$ready)",[StringComparison]::Ordinal)
    $legacyCutoverIndex=$installer.LastIndexOf("if(`$legacy){if(`$legacyWasRunning)",[StringComparison]::Ordinal)
    Assert-True ($verifiedIndex-ge0-and$legacyCutoverIndex-gt$verifiedIndex) 'Legacy polling task is cut over before the event task remains running'
    Assert-True ($installer-match'Export-ScheduledTask[\s\S]+\$fileSnapshots[\s\S]+Register-ScheduledTask\s+-TaskName\s+\$taskName\s+-TaskPath\s+\$taskPath\s+-Xml\s+\$existingXml') 'Existing event task/payload rollback contract is missing'
    Assert-True ($installer-match'readiness-request\.json'-and$installer-match'New-SecureNonce'-and$installer-match'Test-EventRelayReadinessReceipt') 'Installer readiness nonce/request/receipt gate is missing'
    Assert-True ($installer-match'TotalMilliseconds-ge1500') 'Installer does not enforce a continuous Running dwell'
    Assert-True ($installer-match'\$legacyXml[\s\S]+Restore-EventRelayFileSnapshots[\s\S]+Register-ScheduledTask\s+-TaskName\s+\$legacyName\s+-TaskPath\s+\$taskPath\s+-Xml\s+\$legacyXml') 'Legacy task and readiness/payload rollback transaction is incomplete'
    Assert-True ($installer-match'foreach\(\$path in @\(\$requestPath,\$receiptPath\)\)') 'Successful install does not remove readiness transaction files'

    Write-Output "Codex event relay fixtures passed: $script:assertions assertions"
}finally{
    foreach($path in $script:roots){if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Recurse -Force}}
}
