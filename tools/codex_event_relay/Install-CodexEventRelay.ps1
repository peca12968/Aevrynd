[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][ValidateSet('taichung','taoyuan')][string]$Node,
    [Parameter(Mandatory)][Uri]$Endpoint,[Parameter(Mandatory)][string]$SecretPath,
    [string]$InstallRoot='C:\ProgramData\Aevrynd\Codex\event-relay-bin',[string]$StateRoot='C:\ProgramData\Aevrynd\Codex\event-relay',
    [string]$AuditRoot='C:\AevryndAudit\codex-event-relay',[string]$Workspace='C:\Aevrynd\Games'
)
$ErrorActionPreference='Stop'
function Wait-TaskStopped {
    param([string]$Name,[string]$Path,[int]$TimeoutMilliseconds=5000)
    $deadline=[DateTimeOffset]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    do{$task=Get-ScheduledTask -TaskName $Name -TaskPath $Path -ErrorAction SilentlyContinue;if($null-eq$task-or[string]$task.State-cne'Running'){return};Start-Sleep -Milliseconds 100}while([DateTimeOffset]::UtcNow-lt$deadline)
    throw "Scheduled task did not stop: $Path$Name"
}
$sharedModule=Join-Path $Workspace 'tools\codex_command_relay\CodexCommandRelay.psm1'
Import-Module $sharedModule -Force
$sourceEventModule=Join-Path $PSScriptRoot 'CodexEventRelay.psm1';Import-Module $sourceEventModule -Force
$InstallRoot=Assert-RelayPathSafe $InstallRoot;$StateRoot=Assert-RelayPathSafe $StateRoot;$AuditRoot=Assert-RelayPathSafe $AuditRoot;$Workspace=Assert-RelayPathSafe $Workspace;$SecretPath=Assert-RelayPathSafe $SecretPath;$sharedModule=Assert-RelayPathSafe $sharedModule
if($Endpoint.AbsolutePath-cne'/api/codex-relay/connect'-or$Endpoint.Query-or$Endpoint.Fragment-or($Endpoint.Scheme-cne'wss'-and-not($Endpoint.Scheme-ceq'ws'-and$Endpoint.IsLoopback))){throw 'Endpoint must be /api/codex-relay/connect over wss (or loopback ws)'}
if(-not$env:SystemRoot){throw 'Windows system root is unavailable'}
$powerShell=Assert-RelayPathSafe (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe')
if(-not(Test-Path -LiteralPath $powerShell -PathType Leaf)){throw 'Trusted Windows PowerShell was not found'}
$codex=& (Get-Module CodexCommandRelay) { Resolve-DesktopCodex }
$null=& (Get-Module CodexCommandRelay) { param($Path) Test-CodexLogin $Path } $codex
$taskName="Aevrynd-CodexEventRelay-$Node";$taskPath='\Aevrynd\';$description="Aevrynd Codex event relay owner=$Node schema=1"
$installedModule=Join-Path $InstallRoot 'CodexEventRelay.psm1';$installedInvoker=Join-Path $InstallRoot 'Invoke-CodexEventRelay.ps1'
$installManifest=Join-Path $InstallRoot 'install.json'
$argument="-NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$installedInvoker`" -Node $Node -Endpoint `"$($Endpoint.AbsoluteUri)`" -SecretPath `"$SecretPath`" -CodexPath `"$codex`" -SharedModulePath `"$sharedModule`" -Workspace `"$Workspace`" -StateRoot `"$StateRoot`" -AuditRoot `"$AuditRoot`""
$currentUser=[Security.Principal.WindowsIdentity]::GetCurrent().Name
$existing=Get-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue
if($existing){$actions=@($existing.Actions);$execute=if($actions.Count-eq1){[IO.Path]::GetFullPath([string]$actions[0].Execute)}else{''};if($existing.Description-cne$description-or$actions.Count-ne1-or-not$execute.Equals($powerShell,[StringComparison]::OrdinalIgnoreCase)-or[string]$actions[0].Arguments-cne$argument-or[string]$existing.Principal.UserId-cne$currentUser){throw 'Scheduled task exists but is not fully owned by this relay'}}
$legacyNode=$Node.ToUpperInvariant();$legacyName="Aevrynd-CodexCommandRelay-$legacyNode";$legacyDescription="Aevrynd Codex command relay owner=$legacyNode schema=1";$legacyInvoker='C:\ProgramData\Aevrynd\Codex\relay\Invoke-CodexCommandRelay.ps1'
$legacyArguments="-NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$legacyInvoker`" -Node $legacyNode -Repository `"peca12968/Aevrynd`" -Issue 1 -StateRoot `"C:\ProgramData\Aevrynd\Codex`" -AuditRoot `"C:\AevryndAudit\codex-relay`" -Workspace `"$Workspace`""
$legacy=Get-ScheduledTask -TaskName $legacyName -TaskPath $taskPath -ErrorAction SilentlyContinue
if($legacy){$legacyActions=@($legacy.Actions);$legacyExecute=if($legacyActions.Count-eq1){[IO.Path]::GetFullPath([string]$legacyActions[0].Execute)}else{''};if($legacy.Description-cne$legacyDescription-or$legacyActions.Count-ne1-or-not$legacyExecute.Equals($powerShell,[StringComparison]::OrdinalIgnoreCase)-or[string]$legacyActions[0].Arguments-cne$legacyArguments-or[string]$legacy.Principal.UserId-cne([Security.Principal.WindowsIdentity]::GetCurrent().Name)){throw 'Legacy polling task exists but its ownership fingerprint is not exact'}}
if($PSCmdlet.ShouldProcess("$taskPath$taskName",'Install persistent event relay at logon')){
    $requestPath=Join-Path $StateRoot 'readiness-request.json';$receiptPath=Join-Path $StateRoot 'readiness.json'
    $transactionPaths=@($installedModule,$installedInvoker,$installManifest,$requestPath,$receiptPath)
    $fileSnapshots=New-EventRelayFileSnapshots $transactionPaths
    $existingXml=if($existing){Export-ScheduledTask -TaskName $taskName -TaskPath $taskPath}else{$null}
    $existingWasRunning=$existing-and([string]$existing.State-ceq'Running')
    $legacyXml=if($legacy){Export-ScheduledTask -TaskName $legacyName -TaskPath $taskPath}else{$null}
    $legacyWasRunning=$legacy-and([string]$legacy.State-ceq'Running')
    $action=New-ScheduledTaskAction -Execute $powerShell -Argument $argument
    $trigger=New-ScheduledTaskTrigger -AtLogOn -User $currentUser
    $principal=New-ScheduledTaskPrincipal -UserId $currentUser -LogonType Interactive -RunLevel Limited
    $settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit ([TimeSpan]::Zero) -StartWhenAvailable -RestartCount 3 -RestartInterval ([TimeSpan]::FromMinutes(1))
    try{
        if($existingWasRunning){Stop-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction Stop;Wait-TaskStopped $taskName $taskPath}
        foreach($path in @($InstallRoot,$StateRoot,$AuditRoot)){Protect-RelayDirectoryAcl $path}
        $null=Assert-RelayFileAcl $SecretPath
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'CodexEventRelay.psm1') -Destination $installedModule -Force
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Invoke-CodexEventRelay.ps1') -Destination $installedInvoker -Force
        Write-RelayJsonAtomic $installManifest ([ordered]@{schemaVersion=1;node=$Node;endpoint=$Endpoint.AbsoluteUri;secretPath=$SecretPath;codexPath=$codex;sharedModulePath=$sharedModule;workspace=$Workspace;stateRoot=$StateRoot;auditRoot=$AuditRoot;powerShellPath=$powerShell;arguments=$argument}) -VerifyAcl
        $installNonce=New-SecureNonce;$requestedAt=[DateTimeOffset]::UtcNow
        Write-RelayJsonAtomic $requestPath ([ordered]@{schemaVersion=1;nodeId=$Node;installNonce=$installNonce;requestedAtUtc=$requestedAt.ToString('o')}) -VerifyAcl
        if(Test-Path -LiteralPath $receiptPath -PathType Leaf){Remove-Item -LiteralPath $receiptPath -Force}
        Register-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description $description -Force|Out-Null
        # 安裝當下立即啟動單一隱藏常駐程序；不要求登出，也不建立任何 repetition trigger。
        Start-ScheduledTask -TaskName $taskName -TaskPath $taskPath
        $deadline=[DateTimeOffset]::UtcNow.AddSeconds(20);$dwellStart=$null;$ready=$false
        while([DateTimeOffset]::UtcNow-lt$deadline){
            $currentTask=Get-ScheduledTask -TaskName $taskName -TaskPath $taskPath
            if(Test-Path -LiteralPath $receiptPath -PathType Leaf){
                $readiness=Test-EventRelayReadinessReceipt -Path $receiptPath -Node $Node -InstallNonce $installNonce -RequestedAtUtc $requestedAt -RequireSecureAcl -RequireLiveProcess
                if([string]$currentTask.State-cne'Running'){throw 'Event relay stopped after publishing readiness'}
                if($null-eq$dwellStart){$dwellStart=[DateTimeOffset]::UtcNow}
                if(([DateTimeOffset]::UtcNow-$dwellStart).TotalMilliseconds-ge1500){$ready=$true;break}
            }elseif($dwellStart){throw 'Readiness receipt disappeared during dwell'}
            Start-Sleep -Milliseconds 100
        }
        if(-not$ready){throw 'Event relay did not publish valid readiness and remain Running for the dwell interval'}
        # legacy 切換也屬於同一交易；任何 stop/disable/unregister 失敗均走完整 rollback。
        if($legacy){if($legacyWasRunning){Stop-ScheduledTask -TaskName $legacyName -TaskPath $taskPath -ErrorAction Stop;Wait-TaskStopped $legacyName $taskPath};Disable-ScheduledTask -TaskName $legacyName -TaskPath $taskPath -ErrorAction Stop|Out-Null;Unregister-ScheduledTask -TaskName $legacyName -TaskPath $taskPath -Confirm:$false -ErrorAction Stop}
        # readiness 只屬於這次安裝交易；成功後清除，避免重開機重放舊 request/receipt。
        foreach($path in @($requestPath,$receiptPath)){if(Test-Path -LiteralPath $path -PathType Leaf){Remove-Item -LiteralPath $path -Force -ErrorAction Stop}}
    }catch{
        $failure=$_
        $rollbackErrors=[Collections.Generic.List[string]]::new()
        try{Stop-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue;Wait-TaskStopped $taskName $taskPath}catch{$rollbackErrors.Add($_.Exception.Message)}
        try{Unregister-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Confirm:$false -ErrorAction SilentlyContinue}catch{$rollbackErrors.Add($_.Exception.Message)}
        try{Restore-EventRelayFileSnapshots $fileSnapshots}catch{$rollbackErrors.Add($_.Exception.Message)}
        try{if($existingXml){Register-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Xml $existingXml -Force|Out-Null;if($existingWasRunning){Start-ScheduledTask -TaskName $taskName -TaskPath $taskPath}}}catch{$rollbackErrors.Add($_.Exception.Message)}
        try{if($legacyXml){Unregister-ScheduledTask -TaskName $legacyName -TaskPath $taskPath -Confirm:$false -ErrorAction SilentlyContinue;Register-ScheduledTask -TaskName $legacyName -TaskPath $taskPath -Xml $legacyXml -Force|Out-Null;if($legacyWasRunning){Start-ScheduledTask -TaskName $legacyName -TaskPath $taskPath}}}catch{$rollbackErrors.Add($_.Exception.Message)}
        if($rollbackErrors.Count){throw "Install failed: $($failure.Exception.Message); rollback failed: $($rollbackErrors -join '; ')"}
        throw $failure
    }
}
