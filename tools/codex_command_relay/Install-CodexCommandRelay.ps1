[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][ValidateSet('TAICHUNG','TAOYUAN')][string]$Node,
    [string]$Repository='peca12968/Aevrynd',[uint32]$Issue=1,
    [string]$InstallRoot='C:\ProgramData\Aevrynd\Codex\relay',
    [string]$StateRoot='C:\ProgramData\Aevrynd\Codex',
    [string]$AuditRoot='C:\AevryndAudit\codex-relay',
    [string]$Workspace='C:\Aevrynd\Games',[switch]$Force
)
$ErrorActionPreference='Stop'
$sourceModule=Join-Path $PSScriptRoot 'CodexCommandRelay.psm1';$sourceInvoker=Join-Path $PSScriptRoot 'Invoke-CodexCommandRelay.ps1'
Import-Module $sourceModule -Force
$InstallRoot=Assert-RelayPathSafe $InstallRoot;$StateRoot=Assert-RelayPathSafe $StateRoot;$AuditRoot=Assert-RelayPathSafe $AuditRoot;$Workspace=Assert-RelayPathSafe $Workspace
if(-not$env:SystemRoot){throw 'Windows system root is unavailable'}
$windowsPowerShell=Assert-RelayPathSafe (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe')
if(-not(Test-Path -LiteralPath $windowsPowerShell -PathType Leaf)){throw 'Trusted Windows PowerShell executable was not found'}
$taskName="Aevrynd-CodexCommandRelay-$Node";$taskPath='\Aevrynd\';$description="Aevrynd Codex command relay owner=$Node schema=1"
$installedModule=Join-Path $InstallRoot 'CodexCommandRelay.psm1';$installedInvoker=Join-Path $InstallRoot 'Invoke-CodexCommandRelay.ps1'
$argument="-NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$installedInvoker`" -Node $Node -Repository `"$Repository`" -Issue $Issue -StateRoot `"$StateRoot`" -AuditRoot `"$AuditRoot`" -Workspace `"$Workspace`""

# 同名工作只有在所有權指紋完整相符時才能更新；-Force 不得繞過。
$existing=Get-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue
if($existing){
    $actions=@($existing.Actions);$currentUser=[Security.Principal.WindowsIdentity]::GetCurrent().Name
    $existingExecute=if($actions.Count-eq1){[IO.Path]::GetFullPath([string]$actions[0].Execute)}else{''}
    if($existing.Description-cne$description-or$actions.Count-ne1-or-not$existingExecute.Equals($windowsPowerShell,[StringComparison]::OrdinalIgnoreCase)-or[string]$actions[0].Arguments-cne$argument-or[string]$existing.Principal.UserId-cne$currentUser){throw 'Scheduled task exists but is not fully owned by this relay'}
}

if ($PSCmdlet.ShouldProcess("$taskPath$taskName",'Secure directories, baseline cursor, and install one-minute poll task')) {
    foreach($path in @($InstallRoot,$StateRoot,$AuditRoot)){Protect-RelayDirectoryAcl $path;Assert-RelayDirectoryAcl $path}
    Copy-Item -LiteralPath $sourceModule -Destination $installedModule -Force
    Copy-Item -LiteralPath $sourceInvoker -Destination $installedInvoker -Force
    try {
        & $windowsPowerShell -NoProfile -ExecutionPolicy Bypass -File $installedInvoker -Node $Node -Repository $Repository -Issue $Issue -StateRoot $StateRoot -AuditRoot $AuditRoot -Workspace $Workspace -BaselineOnly
        if ($LASTEXITCODE -ne 0) { throw 'Relay baseline failed; scheduled task was not installed' }
    } catch {
        foreach($path in @($installedModule,$installedInvoker)){if(Test-Path -LiteralPath $path -PathType Leaf){Remove-Item -LiteralPath $path -Force}}
        throw
    }
    $action=New-ScheduledTaskAction -Execute $windowsPowerShell -Argument $argument
    $trigger=New-ScheduledTaskTrigger -Once -At ([DateTime]::Now.AddMinutes(1)) -RepetitionInterval ([TimeSpan]::FromMinutes(1)) -RepetitionDuration ([TimeSpan]::FromDays(3650))
    $principal=New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Limited
    $settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit ([TimeSpan]::FromMinutes(55)) -StartWhenAvailable
    Register-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description $description -Force | Out-Null
}
