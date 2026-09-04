[CmdletBinding(SupportsShouldProcess,ConfirmImpact='High')]
param(
    [Parameter(Mandatory)][ValidateSet('taichung','taoyuan')][string]$Node,
    [Parameter(Mandatory)][Uri]$Endpoint,[Parameter(Mandatory)][string]$SecretPath,
    [string]$InstallRoot='C:\ProgramData\Aevrynd\Codex\event-relay-bin',[string]$StateRoot='C:\ProgramData\Aevrynd\Codex\event-relay',
    [string]$AuditRoot='C:\AevryndAudit\codex-event-relay',[string]$Workspace='C:\Aevrynd\Games'
)
$ErrorActionPreference='Stop';$sharedModule=Join-Path $Workspace 'tools\codex_command_relay\CodexCommandRelay.psm1';Import-Module $sharedModule -Force
$InstallRoot=Assert-RelayPathSafe $InstallRoot;$Workspace=Assert-RelayPathSafe $Workspace;$SecretPath=Assert-RelayPathSafe $SecretPath;$sharedModule=Assert-RelayPathSafe $sharedModule
$powerShell=Assert-RelayPathSafe (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe')
$taskName="Aevrynd-CodexEventRelay-$Node";$taskPath='\Aevrynd\';$description="Aevrynd Codex event relay owner=$Node schema=1"
$manifestPath=Assert-RelayPathSafe (Join-Path $InstallRoot 'install.json')
if(-not(Test-Path -LiteralPath $manifestPath -PathType Leaf)){throw 'Owned install manifest was not found'}
$manifest=&(Get-Module CodexCommandRelay){param($Path)Read-RelayJsonStrict $Path $null 'install manifest' -VerifyAcl}$manifestPath
$names=@($manifest.PSObject.Properties.Name|Sort-Object);if(($names-join',')-cne'arguments,auditRoot,codexPath,endpoint,node,powerShellPath,schemaVersion,secretPath,sharedModulePath,stateRoot,workspace'-or[int]$manifest.schemaVersion-ne1-or[string]$manifest.node-cne$Node-or[string]$manifest.endpoint-cne$Endpoint.AbsoluteUri-or-not([IO.Path]::GetFullPath([string]$manifest.secretPath).Equals($SecretPath,[StringComparison]::OrdinalIgnoreCase))-or-not([IO.Path]::GetFullPath([string]$manifest.workspace).Equals($Workspace,[StringComparison]::OrdinalIgnoreCase))-or[string]$manifest.stateRoot-cne$StateRoot-or[string]$manifest.auditRoot-cne$AuditRoot-or-not([IO.Path]::GetFullPath([string]$manifest.sharedModulePath).Equals($sharedModule,[StringComparison]::OrdinalIgnoreCase))-or-not([IO.Path]::GetFullPath([string]$manifest.powerShellPath).Equals($powerShell,[StringComparison]::OrdinalIgnoreCase))){throw 'Install manifest ownership fingerprint is invalid'}
$argument=[string]$manifest.arguments
$task=Get-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue;if(-not$task){return}
$actions=@($task.Actions);$currentUser=[Security.Principal.WindowsIdentity]::GetCurrent().Name;$execute=if($actions.Count-eq1){[IO.Path]::GetFullPath([string]$actions[0].Execute)}else{''}
if($task.Description-cne$description-or$actions.Count-ne1-or-not$execute.Equals($powerShell,[StringComparison]::OrdinalIgnoreCase)-or[string]$actions[0].Arguments-cne$argument-or[string]$task.Principal.UserId-cne$currentUser){throw 'Scheduled task is not fully owned by this relay'}
$payload=@();foreach($name in @('CodexEventRelay.psm1','Invoke-CodexEventRelay.ps1','install.json')){$path=Assert-RelayPathSafe (Join-Path $InstallRoot $name);if(Test-Path -LiteralPath $path -PathType Leaf){$null=Assert-RelayFileAcl $path;$payload+=,$path}}
if($PSCmdlet.ShouldProcess("$taskPath$taskName",'Remove exactly the owned event relay task and payload')){Stop-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue;Unregister-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Confirm:$false;foreach($path in $payload){Remove-Item -LiteralPath $path -Force}}
