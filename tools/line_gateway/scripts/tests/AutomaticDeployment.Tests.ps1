Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$scriptsRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$passed = 0
$failed = 0

function Invoke-Test {
    param([string]$Name,[scriptblock]$Action)
    try { & $Action; $script:passed++; Write-Output "PASS $Name" }
    catch { $script:failed++; Write-Output "FAIL $Name : $($_.Exception.Message)" }
}
function Assert-ContainsAll { param([string]$Text,[string[]]$Values) foreach($value in $Values){if(-not $Text.Contains($value,[StringComparison]::Ordinal)){throw "Missing contract marker: $value"}} }

$installer = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Install-AutomaticDeployment.ps1')
$runner = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Invoke-AutomaticDeployment.ps1')
$trigger = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Trigger-AutomaticDeployment.ps1')
$gatewayInstaller = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Install.ps1')

Invoke-Test 'task action is fixed and has no caller supplied command arguments' {
    Assert-ContainsAll $installer @('RunLevel Highest','Invoke-AutomaticDeployment.ps1','Register-ScheduledTask','MultipleInstances IgnoreNew')
    if ($installer -match '(?m)^param\s*\(' -or $runner -match '(?m)^param\s*\(') { throw 'Privileged installer or runner accepts caller input.' }
    Assert-ContainsAll $installer @(
        "`$pwsh = 'C:\Program Files\PowerShell\7\pwsh.exe'",
        '$arguments = ''-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\Invoke-AutomaticDeployment.ps1"''',
        'New-ScheduledTaskAction -Execute $pwsh -Argument $arguments'
    )
}

Invoke-Test 'verified install refreshes only the fixed protected runner' {
    Assert-ContainsAll $gatewayInstaller @(
        "`$automaticDeploymentRoot = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment'",
        "`$trustedRunner = Join-Path `$paths.Scripts 'Invoke-AutomaticDeployment.ps1'",
        '[IO.File]::Move($temporaryRunner, $fixedRunner, $true)',
        'Automatic deployment runner refresh verification failed.'
    )
    $manifest=$gatewayInstaller.LastIndexOf('Test-InstallationManifest -Paths $paths',[StringComparison]::Ordinal)
    $refresh=$gatewayInstaller.IndexOf('[IO.File]::Move($temporaryRunner',$manifest,[StringComparison]::Ordinal)
    if($manifest-lt0-or$refresh-lt0-or$manifest-gt$refresh){throw 'Protected runner refresh occurs before closed manifest validation.'}
}

Invoke-Test 'automatic update passes only verified installed Python and refuses bootstrap' {
    Assert-ContainsAll $runner @(
        '$installedPython = Get-InstalledPython -Paths $currentPaths',
        '& $installScript -InstallRoot $liveRoot -PythonExe $installedPython',
        'Write-SafeResult -Status failed -Code bootstrap_required',
        "scripts\Common.ps1"
    )
    if($runner -match '(?m)^param\s*\(' -or $runner.Contains('Get-Command python',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Automatic runner accepts external Python input or resolves it from PATH.'
    }
    $manifestGate=$runner.IndexOf('$null = Test-InstallationManifest -Paths $currentPaths',[StringComparison]::Ordinal)
    $pythonRead=$runner.IndexOf('$installedPython = Get-InstalledPython -Paths $currentPaths',[StringComparison]::Ordinal)
    $installCall=$runner.IndexOf('& $installScript -InstallRoot $liveRoot -PythonExe $installedPython',[StringComparison]::Ordinal)
    if($manifestGate-lt0-or$pythonRead-lt0-or$installCall-lt0-or$manifestGate-gt$pythonRead-or$pythonRead-gt$installCall){throw 'Verified Python selection ordering changed.'}
    if(([regex]::Matches($runner,[regex]::Escape('& $installScript -InstallRoot $liveRoot -PythonExe $installedPython'))).Count-ne1){throw 'Install must have one exact verified-Python invocation.'}
}

Invoke-Test 'closed install list contains the full automatic deployment closure' {
    foreach($name in @('Install.ps1','Install-AutomaticDeployment.ps1','Invoke-AutomaticDeployment.ps1','Trigger-AutomaticDeployment.ps1')) {
        if(-not$gatewayInstaller.Contains("'$name'",[StringComparison]::Ordinal)){throw "Closed script list is missing $name"}
    }
}

Invoke-Test 'runner fixes trust roots and rejects untrusted source forms' {
    Assert-ContainsAll $runner @(
        "`$repositoryRoot = 'D:\Download\Aevrynd\Games'",
        "`$liveRoot = 'C:\ProgramData\Aevrynd\LineGateway'",
        "`$expectedOrigin = 'https://github.com/peca12968/Aevrynd.git'",
        "`$expectedBranch = 'refs/heads/main'",
        "StartsWith('\\')",'Length -gt 220','ReparsePoint','ls-remote','diff --no-ext-diff --quiet -- tools/line_gateway','diff --cached --no-ext-diff --quiet -- tools/line_gateway'
    )
}

Invoke-Test 'private origin uses only fixed signed credential helpers and suppresses remote stderr' {
    Assert-ContainsAll $runner @(
        "`$gcmExe = 'C:\Program Files\Git\mingw64\bin\git-credential-manager.exe'",
        "`$githubCliExe = 'C:\Program Files\GitHub CLI\gh.exe'",
        "-SignerPattern '(?i)O=Microsoft Corporation'",
        '-SignerPattern ''(?i)O="?GitHub, Inc\."?''',
        "'-c','credential.helper='",
        "'-c','credential.interactive=never'",
        '!"C:/Program Files/Git/mingw64/bin/git-credential-manager.exe"',
        '!"C:/Program Files/GitHub CLI/gh.exe" auth git-credential',
        '-C $deploymentRoot ls-remote $expectedOrigin $expectedBranch 2>$null',
        'init --bare $trustedGitDirectory 2>$null',
        '--git-dir=$trustedGitDirectory fetch --no-tags $expectedOrigin $release 2>$null',
        '--git-dir=$trustedGitDirectory archive --format=zip --output=$archive FETCH_HEAD'
    )
    if($runner -match '(?i)(Write-(Output|Host|Warning|Error)|Out-File|Tee-Object).*\$(candidateLines|remoteLines)' -or $runner.Contains('2>&1')) {
        throw 'Private-origin output or stderr could be disclosed.'
    }

    # 模擬 private origin：GCM 無此 credential 回 128，固定且已驗簽的 GitHub CLI helper 成功。
    $attempts=@(
        [pscustomobject]@{helper='gcm';exitCode=128;lines=@()},
        [pscustomobject]@{helper='github-cli';exitCode=0;lines=@('0123456789abcdef0123456789abcdef01234567 refs/heads/main')}
    )
    $selected=$null
    foreach($attempt in $attempts){if($attempt.exitCode-eq0-and$attempt.lines.Count-eq1-and$attempt.lines[0]-match '^[0-9a-f]{40}\s+refs/heads/main$'){$selected=$attempt.helper;break}}
    if($selected-cne'github-cli'){throw 'Private-origin fallback did not select the only successful fixed helper.'}
}

Invoke-Test 'transaction order and bounded rollback are explicit' {
    $stop = $runner.IndexOf("scripts\Stop-Gateway.ps1",[StringComparison]::Ordinal)
    $install = $runner.IndexOf('& $installScript',[StringComparison]::Ordinal)
    $start = $runner.IndexOf("scripts\Start-Gateway.ps1",$install,[StringComparison]::Ordinal)
    $health = $runner.IndexOf("scripts\Test-Health.ps1",$start,[StringComparison]::Ordinal)
    if ($stop -lt 0 -or $install -lt 0 -or $start -lt 0 -or $health -lt 0 -or $stop -gt $install -or $install -gt $start -or $start -gt $health) { throw 'Deployment lifecycle ordering changed.' }
    Assert-ContainsAll $runner @('Restore-VerifiedRelease','Test-InstallationManifest','rolled_back','rollback_failed',"@('app','scripts','manifests')")
    foreach($forbidden in @('config\gateway.env','config\credentials','handoff\requests','logs\')){if($runner.Contains($forbidden,[StringComparison]::OrdinalIgnoreCase)){throw "Rollback touches forbidden data marker: $forbidden"}}
}

Invoke-Test 'cross process lock and secret safe result schema are fixed' {
    Assert-ContainsAll $runner @('Global\AevryndLineGatewayAutomaticDeployment','WaitOne(0)','Write-SafeResult','ConvertTo-Json -Compress')
    foreach($forbidden in @('Exception.Message','StackTrace','repositoryRoot = $','liveRoot = $')){if($runner.Contains($forbidden,[StringComparison]::Ordinal)){throw "Result could expose internal detail: $forbidden"}}
}

Invoke-Test 'non-admin trigger only starts task and uses bounded polling' {
    Assert-ContainsAll $trigger @('Start-ScheduledTask','TimeoutSeconds = 300','ValidateRange(10,600)','result.json','Start-Sleep -Milliseconds 500','Get-ScheduledTaskInfo','LastRunTime','LastTaskResult','[DateTimeOffset]$result.startedUtc','Protected deployment runner is outdated','Get-FileHash -Algorithm SHA256')
    if($trigger.Contains('ParseExact([string]$result.startedUtc',[StringComparison]::Ordinal)){throw 'Trigger retains the localized DateTime parsing bug.'}
    foreach($forbidden in @('Register-ScheduledTask','Install.ps1','Invoke-Expression','Start-Process')){if($trigger.Contains($forbidden,[StringComparison]::OrdinalIgnoreCase)){throw "Trigger contains privileged or arbitrary execution marker: $forbidden"}}
}

Invoke-Test 'deployment never waits on a persistent gateway process tree' {
    foreach($content in @($installer,$runner,$trigger)) {
        if($content -match '(?i)Start-Process[^\r\n]*(?:-Verb\s+RunAs|-Wait)'){throw 'Deployment uses process-tree waiting or nested UAC.'}
    }
    if($runner -match 'Start-Gateway\.ps1''\)\s+-InstallRoot\s+\$liveRoot\s+-Wait') { throw 'Runner waits on the persistent gateway child.' }
    Assert-ContainsAll $trigger @('Start-ScheduledTask','result.json')
}

Invoke-Test 'result publication is atomic and queue recovery brackets stop and install' {
    Assert-ContainsAll $runner @('[IO.File]::Move($temporary, $resultFile, $true)','Wait-GatewayQueueDrain','Invoke-QueueDeploymentControl','attempt_count=MAX(0,attempt_count-1)')
    $drain=$runner.LastIndexOf('Wait-GatewayQueueDrain -PythonExe',[StringComparison]::Ordinal)
    $stop=$runner.IndexOf("scripts\Stop-Gateway.ps1",$drain,[StringComparison]::Ordinal)
    $recover=$runner.IndexOf('Invoke-QueueDeploymentControl -Mode recover',$stop,[StringComparison]::Ordinal)
    $install=$runner.IndexOf('& $installScript',$recover,[StringComparison]::Ordinal)
    if($drain-lt0-or$stop-lt0-or$recover-lt0-or$install-lt0-or$drain-gt$stop-or$stop-gt$recover-or$recover-gt$install){throw 'Drain/recovery transaction ordering changed.'}
}

Invoke-Test 'active ngrok participates in the exact deployment transaction order' {
    Assert-ContainsAll $runner @('ngrok_stop_failed','ngrok_start_failed','public_health_failed','$ngrokWasActive = $true')
    $drain=$runner.LastIndexOf('Wait-GatewayQueueDrain -PythonExe',[StringComparison]::Ordinal)
    $stopNgrok=$runner.IndexOf("scripts\Stop-Ngrok.ps1') -InstallRoot `$liveRoot | Out-Null",$drain,[StringComparison]::Ordinal)
    $stopGateway=$runner.IndexOf("scripts\Stop-Gateway.ps1') -InstallRoot `$liveRoot | Out-Null",$stopNgrok,[StringComparison]::Ordinal)
    $recover=$runner.IndexOf('Invoke-QueueDeploymentControl -Mode recover',$stopGateway,[StringComparison]::Ordinal)
    $install=$runner.IndexOf('& $installScript -InstallRoot $liveRoot -PythonExe $installedPython',$recover,[StringComparison]::Ordinal)
    $startGateway=$runner.IndexOf("scripts\Start-Gateway.ps1') -InstallRoot `$liveRoot | Out-Null",$install,[StringComparison]::Ordinal)
    $startNgrok=$runner.IndexOf("scripts\Start-Ngrok.ps1') -InstallRoot `$liveRoot | Out-Null",$startGateway,[StringComparison]::Ordinal)
    $localHealth=$runner.IndexOf("scripts\Test-Health.ps1') -InstallRoot `$liveRoot | Out-Null",$startNgrok,[StringComparison]::Ordinal)
    $publicHealth=$runner.IndexOf("scripts\Test-NgrokConfiguration.ps1') -InstallRoot `$liveRoot",$localHealth,[StringComparison]::Ordinal)
    $ordered=@($drain,$stopNgrok,$stopGateway,$recover,$install,$startGateway,$startNgrok,$localHealth,$publicHealth)
    if($ordered.Where({$_-lt0}).Count-or(($ordered|Sort-Object)-join ',')-cne($ordered-join ',')){throw "Ngrok deployment order changed: $($ordered -join ',')"}
}

Invoke-Test 'rollback restores gateway and only previously active ngrok' {
    $begin=$runner.IndexOf('function Restore-VerifiedRelease',[StringComparison]::Ordinal)
    $end=$runner.IndexOf("`n`$mutex = `$null",$begin,[StringComparison]::Ordinal)
    if($begin-lt0-or$end-lt0){throw 'Rollback function bounds are missing.'}
    $rollback=$runner.Substring($begin,$end-$begin)
    Assert-ContainsAll $rollback @(
        '[Parameter(Mandatory)][bool]$RestartNgrok',
        "scripts\Stop-Ngrok.ps1",
        "scripts\Stop-Gateway.ps1",
        "if (`$RestartNgrok) { & (Join-Path `$liveRoot 'scripts\Start-Ngrok.ps1')",
        "scripts\Test-Health.ps1",
        "scripts\Test-NgrokConfiguration.ps1"
    )
    $stopNgrok=$rollback.IndexOf('Stop-Ngrok.ps1',[StringComparison]::Ordinal)
    $stopGateway=$rollback.IndexOf('Stop-Gateway.ps1',[StringComparison]::Ordinal)
    $startGateway=$rollback.IndexOf('Start-Gateway.ps1',[StringComparison]::Ordinal)
    $startNgrok=$rollback.IndexOf('Start-Ngrok.ps1',[StringComparison]::Ordinal)
    $localHealth=$rollback.IndexOf('Test-Health.ps1',[StringComparison]::Ordinal)
    $publicHealth=$rollback.IndexOf('Test-NgrokConfiguration.ps1',[StringComparison]::Ordinal)
    if($stopNgrok-gt$stopGateway-or$stopGateway-gt$startGateway-or$startGateway-gt$startNgrok-or$startNgrok-gt$localHealth-or$localHealth-gt$publicHealth){throw 'Rollback transport ordering changed.'}
    foreach($call in [regex]::Matches($runner,'Restore-VerifiedRelease\s+-BackupRoot[^;\r\n]*')){if(-not$call.Value.Contains('-RestartNgrok $ngrokWasActive')-and-not$call.Value.Contains('-RestartNgrok $true')){throw 'Rollback omitted prior ngrok state.'}}
}

Invoke-Test 'mock interrupted work is requeued without consuming an attempt' {
    $rows=@(
        [ordered]@{status='running';attempt_count=2;error_code='upstream_error'},
        [ordered]@{status='completed';attempt_count=1;error_code=''}
    )
    foreach($row in $rows){if($row.status-eq'running'){$row.status='queued';$row.attempt_count=[Math]::Max(0,$row.attempt_count-1);$row.error_code=''}}
    if($rows[0].status-ne'queued'-or$rows[0].attempt_count-ne1-or$rows[0].error_code){throw 'Interrupted mock work was not safely recovered.'}
    if($rows[1].status-ne'completed'-or$rows[1].attempt_count-ne1){throw 'Terminal mock work was modified.'}
}

Invoke-Test 'SQLite mock recovery is transactional and preserves terminal rows' {
    $python=$null
    $liveMetadata='C:\ProgramData\Aevrynd\LineGateway\manifests\install.json'
    if(Test-Path -LiteralPath $liveMetadata -PathType Leaf){
        try{$candidate=[string]((Get-Content -Raw -LiteralPath $liveMetadata|ConvertFrom-Json).pythonExe);if(Test-Path -LiteralPath $candidate -PathType Leaf){$python=$candidate}}catch{}
    }
    if($null-eq$python){
        $candidate=(Get-Command python.exe -ErrorAction Stop).Source
        if(Test-Path -LiteralPath $candidate -PathType Leaf){$python=$candidate}
    }
    if($null-eq$python){throw 'A real Python executable is required for the SQLite fixture.'}
    $database=Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-deploy-queue-'+[Guid]::NewGuid().ToString('N')+'.sqlite3')
    $fixturePath=[IO.Path]::ChangeExtension($database,'.py')
    $fixture=@'
import sqlite3,sys,time
p=sys.argv[1]; c=sqlite3.connect(p)
for table in ("jobs","deliveries","reception_notifications"):
 c.execute(f"CREATE TABLE {table}(status TEXT,attempt_count INTEGER,available_at REAL,lease_until REAL,lease_owner TEXT,updated_at REAL,error_code TEXT)")
 c.execute(f"INSERT INTO {table} VALUES('running',2,0,99,'owner',0,'old')")
 c.execute(f"INSERT INTO {table} VALUES('completed',1,0,NULL,'',0,'')")
c.commit(); now=time.time(); c.execute("BEGIN IMMEDIATE")
c.execute("UPDATE jobs SET status='queued', attempt_count=MAX(0,attempt_count-1), available_at=?, lease_until=NULL, lease_owner='', updated_at=?, error_code='' WHERE status='running'",(now+1,now))
for table in ("deliveries","reception_notifications"): c.execute(f"UPDATE {table} SET status='queued', attempt_count=MAX(0,attempt_count-1), available_at=?, lease_until=NULL, lease_owner='', updated_at=?, error_code='' WHERE status='running'",(now+1,now))
c.commit()
for table in ("jobs","deliveries","reception_notifications"): print(table,*c.execute(f"SELECT status,attempt_count,error_code FROM {table} ORDER BY rowid").fetchall())
c.close()
'@
    try {
        [IO.File]::WriteAllText($fixturePath,$fixture,[Text.UTF8Encoding]::new($false))
        $output=@(& $python -B $fixturePath $database)
        if($LASTEXITCODE-ne0-or$output.Count-ne3){throw "SQLite mock did not execute: exit=$LASTEXITCODE count=$($output.Count) output=$($output -join ' | ')"}
        foreach($line in $output){if($line-notmatch "\('queued', 1, ''\).*\('completed', 1, ''\)"){throw "Unexpected recovery rows: $line"}}
    } finally {
        if(Test-Path -LiteralPath $database){Remove-Item -LiteralPath $database -Force}
        if(Test-Path -LiteralPath $fixturePath){Remove-Item -LiteralPath $fixturePath -Force}
    }
}

# mock 狀態機：不呼叫部署腳本、不建立排程，驗證成功與 rollback 結果對外收斂。
Invoke-Test 'mock lifecycle reports success and rollback without leaking exception text' {
    function Invoke-MockedLifecycle { param([scriptblock]$Install,[scriptblock]$Health,[scriptblock]$Rollback)
        try { & $Install; & $Health; return @{status='succeeded';code='none'} }
        catch { try { & $Rollback; return @{status='rolled_back';code='health_failed'} } catch { return @{status='rollback_failed';code='rollback_failed'} } }
    }
    $success=Invoke-MockedLifecycle {} {} {}; if($success.status-ne'succeeded'){throw 'Mock success did not converge.'}
    $rollback=Invoke-MockedLifecycle {} {throw 'SECRET=CANNOT_LEAK'} {}; if($rollback.status-ne'rolled_back'-or($rollback|ConvertTo-Json)-match'SECRET'){throw 'Mock rollback leaked detail.'}
}

Write-Output "Automatic deployment tests: passed=$passed failed=$failed"
if ($failed -gt 0) { exit 1 }
