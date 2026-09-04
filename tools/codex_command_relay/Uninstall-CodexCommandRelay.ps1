[CmdletBinding(SupportsShouldProcess,ConfirmImpact='High')]
param(
    [Parameter(Mandatory)][ValidateSet('TAICHUNG','TAOYUAN')][string]$Node,
    [string]$Repository='peca12968/Aevrynd',[uint32]$Issue=1,
    [string]$InstallRoot='C:\ProgramData\Aevrynd\Codex\relay',[string]$StateRoot='C:\ProgramData\Aevrynd\Codex',
    [string]$AuditRoot='C:\AevryndAudit\codex-relay',[string]$Workspace='C:\Aevrynd\Games'
)
$ErrorActionPreference='Stop';$taskName="Aevrynd-CodexCommandRelay-$Node";$taskPath='\Aevrynd\';$expected="Aevrynd Codex command relay owner=$Node schema=1"
$module=Join-Path $PSScriptRoot 'CodexCommandRelay.psm1';Import-Module $module -Force
$InstallRoot=Assert-RelayPathSafe $InstallRoot;$StateRoot=Assert-RelayPathSafe $StateRoot;$AuditRoot=Assert-RelayPathSafe $AuditRoot;$Workspace=Assert-RelayPathSafe $Workspace
if(-not$env:SystemRoot){throw 'Windows system root is unavailable'}
$windowsPowerShell=Assert-RelayPathSafe (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe')
if(-not(Test-Path -LiteralPath $windowsPowerShell -PathType Leaf)){throw 'Trusted Windows PowerShell executable was not found'}
$task=Get-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue
if(-not$task){return}
if ($task.Description -cne $expected) { throw 'Scheduled task exists but is not owned by this relay' }
$expectedInvoker=[IO.Path]::GetFullPath((Join-Path $InstallRoot 'Invoke-CodexCommandRelay.ps1'))
$actions=@($task.Actions)
if($actions.Count-ne1){throw 'Scheduled task action is not owned by this relay'}
$actionArguments=[string]$actions[0].Arguments
$expectedArguments="-NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$expectedInvoker`" -Node $Node -Repository `"$Repository`" -Issue $Issue -StateRoot `"$StateRoot`" -AuditRoot `"$AuditRoot`" -Workspace `"$Workspace`""
$currentUser=[Security.Principal.WindowsIdentity]::GetCurrent().Name
$existingExecute=[IO.Path]::GetFullPath([string]$actions[0].Execute)
if(-not$existingExecute.Equals($windowsPowerShell,[StringComparison]::OrdinalIgnoreCase)-or$actionArguments-cne$expectedArguments-or[string]$task.Principal.UserId-cne$currentUser){throw 'Scheduled task action is not owned by this relay'}
$payload=[Collections.Generic.List[string]]::new();foreach($name in @('CodexCommandRelay.psm1','Invoke-CodexCommandRelay.ps1')){$path=Assert-RelayPathSafe (Join-Path $InstallRoot $name);if(Test-Path -LiteralPath $path -PathType Leaf){$null=Assert-RelayFileAcl $path;$payload.Add($path)}}
if ($PSCmdlet.ShouldProcess("$taskPath$taskName",'Stop and unregister owned relay task, then remove its installed payload')) {
    Stop-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue
    Unregister-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Confirm:$false
    foreach($path in $payload){Remove-Item -LiteralPath $path -Force}
}
