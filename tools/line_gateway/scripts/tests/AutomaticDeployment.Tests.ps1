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

Invoke-Test 'task action is fixed and has no caller supplied command arguments' {
    Assert-ContainsAll $installer @('RunLevel Highest','Invoke-AutomaticDeployment.ps1','Register-ScheduledTask','MultipleInstances IgnoreNew')
    if ($installer -match '(?m)^param\s*\(' -or $runner -match '(?m)^param\s*\(') { throw 'Privileged installer or runner accepts caller input.' }
    Assert-ContainsAll $installer @(
        "`$pwsh = 'C:\Program Files\PowerShell\7\pwsh.exe'",
        '$arguments = ''-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\Invoke-AutomaticDeployment.ps1"''',
        'New-ScheduledTaskAction -Execute $pwsh -Argument $arguments'
    )
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

Invoke-Test 'transaction order and bounded rollback are explicit' {
    $stop = $runner.IndexOf("scripts\Stop-Gateway.ps1",[StringComparison]::Ordinal)
    $install = $runner.IndexOf('& $installScript',[StringComparison]::Ordinal)
    $start = $runner.IndexOf("scripts\Start-Gateway.ps1",$install,[StringComparison]::Ordinal)
    $health = $runner.IndexOf("scripts\Test-Health.ps1",$start,[StringComparison]::Ordinal)
    if ($stop -lt 0 -or $install -lt 0 -or $start -lt 0 -or $health -lt 0 -or $stop -gt $install -or $install -gt $start -or $start -gt $health) { throw 'Deployment lifecycle ordering changed.' }
    Assert-ContainsAll $runner @('Restore-VerifiedRelease','Test-InstallationManifest','rolled_back','rollback_failed',"@('app','scripts','manifests')")
    foreach($forbidden in @('config\gateway.env','queue.sqlite3','config\credentials','handoff\requests','logs\')){if($runner.Contains($forbidden,[StringComparison]::OrdinalIgnoreCase)){throw "Rollback touches forbidden data marker: $forbidden"}}
}

Invoke-Test 'cross process lock and secret safe result schema are fixed' {
    Assert-ContainsAll $runner @('Global\AevryndLineGatewayAutomaticDeployment','WaitOne(0)','Write-SafeResult','ConvertTo-Json -Compress')
    foreach($forbidden in @('Exception.Message','StackTrace','repositoryRoot = $','liveRoot = $')){if($runner.Contains($forbidden,[StringComparison]::Ordinal)){throw "Result could expose internal detail: $forbidden"}}
}

Invoke-Test 'non-admin trigger only starts task and uses bounded polling' {
    Assert-ContainsAll $trigger @('Start-ScheduledTask','TimeoutSeconds = 180','ValidateRange(10,600)','result.json','Start-Sleep -Milliseconds 500')
    foreach($forbidden in @('Register-ScheduledTask','Install.ps1','Invoke-Expression','Start-Process')){if($trigger.Contains($forbidden,[StringComparison]::OrdinalIgnoreCase)){throw "Trigger contains privileged or arbitrary execution marker: $forbidden"}}
}

Invoke-Test 'deployment never waits on a persistent gateway process tree' {
    foreach($content in @($installer,$runner,$trigger)) {
        if($content -match '(?i)Start-Process[^\r\n]*(?:-Verb\s+RunAs|-Wait)'){throw 'Deployment uses process-tree waiting or nested UAC.'}
    }
    if($runner -match 'Start-Gateway\.ps1''\)\s+-InstallRoot\s+\$liveRoot\s+-Wait') { throw 'Runner waits on the persistent gateway child.' }
    Assert-ContainsAll $trigger @('Start-ScheduledTask','result.json')
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
