$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$relayRoot=Split-Path -Parent $PSScriptRoot;Import-Module (Join-Path $relayRoot 'CodexCommandRelay.psm1') -Force
$script:Assertions=0
function Assert-True([bool]$Value,[string]$Message){$script:Assertions++;if(-not$Value){throw $Message}}
function Assert-Throws([scriptblock]$Action,[string]$Message){$threw=$false;try{&$Action}catch{$threw=$true};Assert-True $threw $Message}
function New-Root([string]$Name){$path=Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-relay-'+$Name+'-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $path|Out-Null;$script:Roots.Add($path);$path}
function New-Body([string]$Id,[string]$Target='TAICHUNG',[string]$Owned='engine/native/tests/example.txt'){"[MAIN_COMMAND]`nTARGET: $Target`nCOMMAND_ID: $Id`nOWNED_FILES: $Owned`nTASK: inspect the owned fixture`n[/MAIN_COMMAND]"}
function New-Comment([uint64]$Id,[string]$Body,[string]$Login='peca12968',[string]$Association='OWNER'){$date=[DateTimeOffset]::new(2026,1,1,0,0,0,[TimeSpan]::Zero).AddSeconds($Id).ToString('o');[pscustomobject]@{id=$Id;created_at=$date;body=$Body;user=[pscustomobject]@{login=$Login};author_association=$Association}}
function Read-Json([string]$Path){Get-Content -LiteralPath $Path -Raw -Encoding UTF8|ConvertFrom-Json}
function Test-BadState([string]$Kind){
    $caseRoot = (New-Root -Name $Kind);$caseState=Join-Path $caseRoot state;New-Item -ItemType Directory -Path $caseState|Out-Null;$script:side=0
    if($Kind-ceq'corrupt'){[IO.File]::WriteAllText((Join-Path $caseState relay-state.json),'{bad')}
    elseif($Kind-ceq'oversize'){[IO.File]::WriteAllBytes((Join-Path $caseState relay-state.json),[byte[]]::new(1048577))}
    elseif($Kind-ceq'schema'){Write-RelayJsonAtomic (Join-Path $caseState relay-state.json) ([ordered]@{schemaVersion=1;cursor=0;operations=@()})}
    else{Write-RelayJsonAtomic (Join-Path $caseState relay-state.json) ([ordered]@{schemaVersion=1;node='TAICHUNG';cursor=0;cursorCreatedAt='1970-01-01T00:00:00.0000000+00:00';operations=@()});[IO.File]::WriteAllText((Join-Path $caseState operations-history.json),'{bad')}
    Assert-Throws {Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $caseState -AuditRoot (Join-Path $caseRoot audit) -RequireSecureAcl:$false -GetComments {$script:side++;@()} -SendAck {$script:side++} -RunCodex {$script:side++}} "$Kind did not fail closed"
    Assert-True ($script:side-eq0) "$Kind crossed the side-effect boundary"
}
function Test-InvalidOperation([string]$Kind){
    $caseRoot=(New-Root -Name ('invalid-op-'+$Kind));$caseState=Join-Path $caseRoot state;New-Item -ItemType Directory -Path $caseState|Out-Null;$stamp='2026-01-01T00:00:00.0000000+00:00'
    $operation=[pscustomobject][ordered]@{commandId='HOSTILE';commentId=[uint64]1;node='TAICHUNG';target='TAICHUNG';state='completed';startedAt=$stamp;finishedAt=$stamp;exitCode=0;ackAttempted=$true;ackSucceeded=$true;outputPath='C:\audit\hostile.log';summary='fixture'}
    switch($Kind){
        extra {$operation|Add-Member extra 'smuggled'}
        comment {$operation.commentId='1'}
        target {$operation.target='OTHER'}
        timestamp {$operation.finishedAt='not-a-time'}
        running {$operation.state='running'}
        exit {$operation.state='failed'}
        acktype {$operation.ackAttempted='true'}
        acklogic {$operation.ackAttempted=$false}
        output {$operation.outputPath="C:\audit\bad`npath"}
        summary {$operation.summary='x'*241}
    }
    Write-RelayJsonAtomic (Join-Path $caseState relay-state.json) ([ordered]@{schemaVersion=1;node='TAICHUNG';cursor=1;cursorCreatedAt=$stamp;operations=@($operation)})
    $script:side=0;Assert-Throws {Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $caseState -AuditRoot (Join-Path $caseRoot audit) -RequireSecureAcl:$false -GetComments {$script:side++;@()} -SendAck {$script:side++} -RunCodex {$script:side++}} "Invalid operation $Kind was accepted"
    Assert-True ($script:side-eq0) "Invalid operation $Kind crossed the side-effect boundary"
}
$boundary={param($phase,$workspace)[pscustomobject]@{head='a'*40;status=@();worktree=@();metadata=@()}}
$compare={param($before,$after,$owned)[pscustomobject]@{passed=$true;violations=@()}}
$script:Roots=[Collections.Generic.List[string]]::new()
try{
    $relayModule=Get-Module CodexCommandRelay
    $pagedBody=(New-Body PAGED).Replace("TASK: inspect the owned fixture","TASK: first line`nsecond line")
    $pageOne=(New-Comment 101 $pagedBody)|ConvertTo-Json -Depth 6 -Compress
    $pageTwo=(New-Comment 102 (New-Body PAGE-TWO))|ConvertTo-Json -Depth 6 -Compress
    $pagedJson="[[$pageOne],[$pageTwo]]"
    $paged=@(&$relayModule {param($raw) ConvertFrom-GitHubCommentPages $raw} @($pagedJson.Substring(0,[Math]::Floor($pagedJson.Length/2)),$pagedJson.Substring([Math]::Floor($pagedJson.Length/2))))
    Assert-True ($paged.Count-eq2-and[uint64]$paged[0].id-eq101-and$paged[0].body-ceq$pagedBody-and[uint64]$paged[1].id-eq102) 'Native JSON pagination lost a page or multiline command body'
    Assert-Throws {&$relayModule {param($raw) ConvertFrom-GitHubCommentPages $raw} @('<html>gateway failure</html>')} 'Non-JSON GitHub response was accepted'
    Assert-Throws {&$relayModule {param($raw) ConvertFrom-GitHubCommentPages $raw} @($pagedJson.Substring(0,$pagedJson.Length-3))} 'Truncated GitHub response was accepted'
    $relaySource=Get-Content -LiteralPath (Join-Path $relayRoot 'CodexCommandRelay.psm1') -Raw
    Assert-True ($relaySource-match'--paginate --slurp'-and$relaySource-notmatch'--template') 'GitHub polling still depends on a Go template'

    foreach($body in @("[MAIN_COMMAND]`nTARGET: TAICHUNG`nCOMMAND_ID: X`nOWNED_FILES: a.txt`nTASK: x","[MAIN_COMMAND]`nTARGET: TAICHUNG`nCOMMAND_ID: X`nOWNED_FILES: a.txt`nTASK: x`n[/MAIN_COMMAND]`ntrailing","[MAIN_COMMAND]`n[MAIN_COMMAND]`nTARGET: TAICHUNG`nCOMMAND_ID: X`nOWNED_FILES: a.txt`nTASK: x`n[/MAIN_COMMAND]`n[/MAIN_COMMAND]")){Assert-True ($null-eq(Get-RelayCommand (New-Comment 1 $body) TAICHUNG)) 'Malformed envelope was accepted'}
    $lfCommand=Get-RelayCommand (New-Comment 1 (New-Body LF)) TAICHUNG;$crlfCommand=Get-RelayCommand (New-Comment 2 ((New-Body CRLF).Replace("`n","`r`n"))) TAICHUNG
    Assert-True ($null-ne$lfCommand-and$lfCommand.CommandId-ceq'LF'-and$null-ne$crlfCommand-and$crlfCommand.CommandId-ceq'CRLF') "LF or CRLF command envelope was rejected lf=$($lfCommand.CommandId) crlf=$($crlfCommand.CommandId)"
    Assert-True ($null-eq(Get-RelayCommand (New-Comment 1 (New-Body X) attacker OWNER) TAICHUNG)) 'Wrong author was accepted'
    Assert-True ($null-eq(Get-RelayCommand (New-Comment 1 (New-Body X TAOYUAN)) TAICHUNG)) 'Wrong target was accepted'
    Assert-True ($null-eq(Get-RelayCommand (New-Comment 1 (New-Body X TAICHUNG '.git/config')) TAICHUNG)) 'Git metadata ownership was accepted'

    Set-Variable -Name caseDirectory -Value (New-Root -Name 'reject')
    $state=Join-Path $caseDirectory 'state'
    $audit=Join-Path $caseDirectory 'audit'
    $script:since=$null;$script:runs=0;$script:acks=0
    $comments=@((New-Comment 1 (New-Body BAD) attacker OWNER),(New-Comment 2 (New-Body WRONG TAOYUAN)))
    $output=@(Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot $audit -RequireSecureAcl:$false -GetComments {param($r,$i,$s)$script:since=$s;$comments} -SendAck {$script:acks++} -RunCodex {$script:runs++} -CaptureBoundary $boundary -CompareBoundary $compare 2>&1)
    Assert-True (@($output).Count-eq0-and$script:runs-eq0-and$script:acks-eq0) 'Rejected poll was not silent'
    Assert-True ($script:since-ceq'1970-01-01T00:00:00.0000000+00:00'-and(Read-Json (Join-Path $state relay-state.json)).cursor-eq2) 'Cursor/since behavior was incorrect'

    $root = (New-Root -Name 'valid');$state=Join-Path $root state;$audit=Join-Path $root audit;$script:runs=0;$script:acks=0;$script:active=$null;$script:args=$null;$script:ackObserved=$null
    $longId='A'+('B'*127);$valid=New-Comment 10 (New-Body $longId)
    Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot $audit -RequireSecureAcl:$false -GetComments {@($valid)} -SendAck {$script:acks++;$script:ackObserved=(Read-Json (Join-Path $state relay-state.json)).operations[-1]} -ResolveCodex {'C:\fixture\codex.exe'} -CheckLogin {} -RunCodex {param($p,$a,$prompt,$o)$script:runs++;$script:args=$a;$script:active=Read-Json (Join-Path $state status.json);0} -CaptureBoundary $boundary -CompareBoundary $compare
    $private=Read-Json (Join-Path $state relay-state.json);$history=Read-Json (Join-Path $state operations-history.json)
    Assert-True ($script:runs-eq1-and$script:acks-eq1-and$private.operations[0].state-ceq'completed'-and$private.operations[0].ackSucceeded) "Valid command was not completed once runs=$script:runs acks=$script:acks state=$($private.operations[0].state) ack=$($private.operations[0].ackSucceeded) summary=$($private.operations[0].summary)"
    Assert-True ($script:ackObserved.ackAttempted-and-not$script:ackObserved.ackSucceeded-and$script:ackObserved.state-ceq'running') 'ACK was called before attempted state became durable'
    Assert-True (($script:args-join'|')-ceq'exec|--ephemeral|-m|gpt-5.6-luna|-c|model_reasoning_effort="medium"|-s|workspace-write|--approve-for-me|-C|C:\Aevrynd\Games') 'Codex arguments changed'
    Assert-True ($script:active.task.Length-le80-and$script:active.status-ceq'working'-and$script:active.ttl-eq3600) 'Active telemetry schema/task bound failed'
    Assert-True ($history.schemaVersion-eq1-and$history.events[0].status-ceq'completed'-and$history.events[0].PSObject.Properties.Name.Count-eq7-and-not(Test-Path (Join-Path $state status.json))) 'History schema or Active cleanup failed'
    Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot $audit -RequireSecureAcl:$false -GetComments {@($valid,(New-Comment 11 (New-Body $longId)))} -SendAck {$script:acks++} -RunCodex {$script:runs++} -CaptureBoundary $boundary -CompareBoundary $compare
    Assert-True ($script:runs-eq1-and$script:acks-eq1) 'Duplicate command was retried'

    $ackFail=New-Comment 12 (New-Body ACK-FAIL)
    Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot $audit -RequireSecureAcl:$false -GetComments {@($ackFail)} -SendAck {throw 'offline ACK failure'} -ResolveCodex {'fixture.exe'} -CheckLogin {} -RunCodex {$script:runs++;0} -CaptureBoundary $boundary -CompareBoundary $compare
    $op=(Read-Json (Join-Path $state relay-state.json)).operations[-1]
    Assert-True ($script:runs-eq2-and$op.ackAttempted-and-not$op.ackSucceeded-and$op.state-ceq'completed'-and$op.summary-match'ACK failed') 'ACK failure semantics were incorrect'

    $badCompare={param($b,$a,$o)[pscustomobject]@{passed=$false;violations=@('HEAD changed')}};$boundaryCommand=New-Comment 13 (New-Body BOUNDARY)
    Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot $audit -RequireSecureAcl:$false -GetComments {@($boundaryCommand)} -SendAck {} -ResolveCodex {'fixture.exe'} -CheckLogin {} -RunCodex {0} -CaptureBoundary $boundary -CompareBoundary $badCompare
    $op=(Read-Json (Join-Path $state relay-state.json)).operations[-1]
    Assert-True ($op.state-ceq'failed'-and$op.exitCode-eq90-and(Test-Path (Join-Path (Join-Path $audit taichung) '13-BOUNDARY-boundary.json'))) 'Boundary violation was not retained as failure evidence'
    $collision=New-Comment 14 (New-Body OUTPUT-COLLISION);$collisionPath=Join-Path (Join-Path $audit taichung) '14-OUTPUT-COLLISION.log';[IO.File]::WriteAllText($collisionPath,'preexisting');$script:collisionRuns=0
    Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot $audit -RequireSecureAcl:$false -GetComments {@($collision)} -SendAck {} -ResolveCodex {'fixture.exe'} -CheckLogin {} -RunCodex {$script:collisionRuns++;0} -CaptureBoundary $boundary -CompareBoundary $compare
    $collisionOperation=(Read-Json (Join-Path $state relay-state.json)).operations[-1];Assert-True ($script:collisionRuns-eq0-and$collisionOperation.state-ceq'failed') 'Existing output log was overwritten or handed to Codex'

    $cursor=(Read-Json (Join-Path $state relay-state.json)).cursor;$apiOut=@(Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot $audit -RequireSecureAcl:$false -GetComments {throw 'API failure'} 2>&1)
    Assert-True (@($apiOut).Count-eq0-and(Read-Json (Join-Path $state relay-state.json)).cursor-eq$cursor) 'API failure advanced cursor or emitted output'
    $root = (New-Root -Name 'baseline');$state=Join-Path $root state;$script:runs=0
    Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot (Join-Path $root audit) -RequireSecureAcl:$false -BaselineOnly -GetComments {@((New-Comment 20 (New-Body OLD)),(New-Comment 25 (New-Body OLD2)))} -RunCodex {$script:runs++}
    Assert-True ((Read-Json (Join-Path $state relay-state.json)).cursor-eq25-and$script:runs-eq0) 'Baseline executed old work'
    Assert-Throws {Invoke-CodexCommandRelay -Node TAOYUAN -StateRoot $state -AuditRoot (Join-Path $root audit) -RequireSecureAcl:$false -GetComments {@()}} 'Permanent node binding was bypassed'

    Test-BadState corrupt;Test-BadState oversize;Test-BadState schema;Test-BadState history
    foreach($kind in @('extra','comment','target','timestamp','running','exit','acktype','acklogic','output','summary')){Test-InvalidOperation $kind}

    $root = (New-Root -Name 'lock');$state=Join-Path $root state;New-Item -ItemType Directory -Path $state|Out-Null;$held=[IO.File]::Open((Join-Path $state relay.lock),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    try {
        $out=@(Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot (Join-Path $root audit) -RequireSecureAcl:$false -GetComments {throw 'polled'} 2>&1)
        Assert-True (@($out).Count-eq0) 'Lock contention was not silent'
    } finally { $held.Dispose() }

    $root = (New-Root -Name 'whatif');$state=Join-Path $root absent-state;$audit=Join-Path $root absent-audit;$script:side=0
    $plan=Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $state -AuditRoot $audit -RequireSecureAcl:$false -WhatIf -GetComments {@(New-Comment 30 (New-Body PLAN))} -SendAck {$script:side++} -ResolveCodex {$script:side++} -RunCodex {$script:side++}
    Assert-True ($plan.commandId-ceq'PLAN'-and($plan.codexArguments-join'|')-match'--approve-for-me'-and$script:side-eq0-and-not(Test-Path $state)-and-not(Test-Path $audit)) 'WhatIf caused a side effect or returned a bad plan'

    $root = (New-Root -Name 'reparse');$real=Join-Path $root real;$link=Join-Path $root link;New-Item -ItemType Directory -Path $real|Out-Null;New-Item -ItemType Junction -Path $link -Target $real|Out-Null
    Assert-Throws {Assert-RelayPathSafe (Join-Path $link child)} 'Reparse path was accepted'

    $emptyBoundary=[pscustomobject]@{head='a'*40;status=@();worktree=@();metadata=@()}
    $renameBoundary=[pscustomobject]@{head='a'*40;status=@('R  outside.txt -> allowed.txt');worktree=@();metadata=@()}
    $quotedRenameBoundary=[pscustomobject]@{head='a'*40;status=@('R  "outside file.txt" -> "allowed file.txt"');worktree=@();metadata=@()}
    Assert-True (-not(Compare-GitBoundarySnapshot $emptyBoundary $renameBoundary @('allowed.txt')).passed-and-not(Compare-GitBoundarySnapshot $emptyBoundary $quotedRenameBoundary @('allowed file.txt')).passed) 'Rename source escaped ownership validation'
    $metadataBoundary=[pscustomobject]@{head='a'*40;status=@();worktree=@();metadata=@([pscustomobject]@{path='hooks/post-checkout';sha256='bad'})}
    Assert-True (-not(Compare-GitBoundarySnapshot $emptyBoundary $metadataBoundary @('**')).passed) 'Git metadata change escaped the boundary audit'

    $aclRoot=(New-Root -Name 'acl');$aclState=Join-Path $aclRoot state;$aclAudit=Join-Path $aclRoot audit;Protect-RelayDirectoryAcl $aclState;Protect-RelayDirectoryAcl $aclAudit
    $aclStamp='1970-01-01T00:00:00.0000000+00:00';$aclStatePath=Join-Path $aclState relay-state.json
    Write-RelayJsonAtomic $aclStatePath ([ordered]@{schemaVersion=1;node='TAICHUNG';cursor=0;cursorCreatedAt=$aclStamp;operations=@()}) -VerifyAcl
    $icacls=Join-Path $env:SystemRoot 'System32\icacls.exe';$null=&$icacls $aclStatePath /grant '*S-1-1-0:W' /q;Assert-True ($LASTEXITCODE-eq0) 'Fixture could not add hostile file ACL'
    $script:side=0;Assert-Throws {Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $aclState -AuditRoot $aclAudit -GetComments {$script:side++;@()}} 'Untrusted state-file writer ACL was accepted';Assert-True ($script:side-eq0) 'ACL failure crossed the API boundary'
    Protect-RelayDirectoryAcl $aclState;$null=&$icacls $aclState /grant '*S-1-1-0:(OI)(CI)W' /q;Assert-True ($LASTEXITCODE-eq0) 'Fixture could not add hostile directory ACL'
    Assert-Throws {Assert-RelayDirectoryAcl $aclState} 'Untrusted directory writer ACL was accepted';Protect-RelayDirectoryAcl $aclState

    $lockAclRoot=(New-Root -Name 'lock-acl');$lockState=Join-Path $lockAclRoot state;$lockAudit=Join-Path $lockAclRoot audit;Protect-RelayDirectoryAcl $lockState;Protect-RelayDirectoryAcl $lockAudit;$lockFile=Join-Path $lockState relay.lock;[IO.File]::WriteAllText($lockFile,'')
    $null=&$icacls $lockFile /grant '*S-1-1-0:W' /q;Assert-True ($LASTEXITCODE-eq0) 'Fixture could not add hostile lock ACL';$script:side=0
    Assert-Throws {Invoke-CodexCommandRelay -Node TAICHUNG -StateRoot $lockState -AuditRoot $lockAudit -GetComments {$script:side++;@()}} 'Untrusted lock ACL was accepted';Assert-True ($script:side-eq0) 'Lock ACL failure crossed the API boundary'

    $installerText=Get-Content -LiteralPath (Join-Path $relayRoot 'Install-CodexCommandRelay.ps1') -Raw
    $uninstallerText=Get-Content -LiteralPath (Join-Path $relayRoot 'Uninstall-CodexCommandRelay.ps1') -Raw
    Assert-True ($installerText-match"System32\\WindowsPowerShell\\v1\.0\\powershell\.exe"-and$installerText-match'New-ScheduledTaskAction -Execute \$windowsPowerShell'-and$installerText-match'& \$windowsPowerShell -NoProfile') 'Installer did not pin Windows PowerShell by full path'
    Assert-True ($uninstallerText-match'OrdinalIgnoreCase'-and$uninstallerText-match"System32\\WindowsPowerShell\\v1\.0\\powershell\.exe"-and$uninstallerText-notmatch"notmatch'\(\?i\)\(\^\|\\\\\)powershell") 'Uninstaller ownership did not require the exact executable path'

    $absentRoot=(New-Root -Name 'uninstall-absent');[IO.File]::WriteAllText((Join-Path $absentRoot 'CodexCommandRelay.psm1'),'payload');[IO.File]::WriteAllText((Join-Path $absentRoot 'Invoke-CodexCommandRelay.ps1'),'payload')
    function global:Get-ScheduledTask { return $null }
    try{& (Join-Path $relayRoot 'Uninstall-CodexCommandRelay.ps1') -Node TAICHUNG -InstallRoot $absentRoot -Confirm:$false}finally{Remove-Item Function:\Get-ScheduledTask -Force}
    Assert-True ((Test-Path (Join-Path $absentRoot 'CodexCommandRelay.psm1'))-and(Test-Path (Join-Path $absentRoot 'Invoke-CodexCommandRelay.ps1'))) 'Task-absent uninstall deleted installed payload'

    "CodexCommandRelay fixture PASS assertions=$script:Assertions"
} finally {
    foreach($root in $script:Roots){if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}}
}
