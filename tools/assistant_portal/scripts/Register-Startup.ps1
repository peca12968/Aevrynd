Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-Administrator
$legacy=Get-ScheduledTask -TaskName 'Aevrynd Assistant Portal' -TaskPath '\Aevrynd\' -ErrorAction SilentlyContinue
if($null-ne$legacy){Stop-ScheduledTask -InputObject $legacy -ErrorAction SilentlyContinue;Disable-ScheduledTask -InputObject $legacy -ErrorAction SilentlyContinue|Out-Null;Unregister-ScheduledTask -InputObject $legacy -Confirm:$false}
$pwsh=[IO.Path]::GetFullPath((Join-Path $PSHOME 'pwsh.exe'))
if(-not(Test-Path -LiteralPath $pwsh -PathType Leaf)){throw 'The protected PowerShell runtime is unavailable.'}
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$limited=New-ScheduledTaskPrincipal -UserId $identity.Name -LogonType Interactive -RunLevel Limited
$highest=New-ScheduledTaskPrincipal -UserId $identity.Name -LogonType Interactive -RunLevel Highest
$settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 5) -RestartCount 0 -MultipleInstances IgnoreNew
$future=New-ScheduledTaskTrigger -Once -At (Get-Date).AddYears(10)
$limitedScript=Join-Path (Get-PortalPaths).Scripts 'Limited-Task.ps1'
foreach($operation in @('Preflight','Start','Stop','RestorePreview','ResumePortal')){
    $action=New-ScheduledTaskAction -Execute $pwsh -Argument "-NoProfile -NonInteractive -WindowStyle Hidden -File `"$limitedScript`" -Operation $operation"
    $triggers=if($operation-eq'Start'){$trigger=New-ScheduledTaskTrigger -AtLogOn -User $identity.Name;$trigger.Delay='PT30S';@($trigger)}else{@($future)}
    Register-ScheduledTask -TaskName "Aevrynd Assistant Portal $operation" -TaskPath '\Aevrynd\' -Action $action -Trigger $triggers -Principal $limited -Settings $settings -Force|Out-Null
}
$broker=Join-Path (Get-PortalPaths).Scripts 'Admin-Broker.ps1'
$recoveryAction=New-ScheduledTaskAction -Execute $pwsh -Argument "-NoProfile -NonInteractive -WindowStyle Hidden -File `"$broker`" -Operation Recover"
$recoveryTrigger=New-ScheduledTaskTrigger -AtLogOn -User $identity.Name
Register-ScheduledTask -TaskName 'Aevrynd Assistant Portal Recovery' -TaskPath '\Aevrynd\' -Action $recoveryAction -Trigger $recoveryTrigger -Principal $highest -Settings $settings -Force|Out-Null
Write-Output 'Separated Highest recovery broker and Limited steady-state tasks registered.'
