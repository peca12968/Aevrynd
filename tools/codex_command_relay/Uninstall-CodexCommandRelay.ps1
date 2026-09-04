[CmdletBinding(SupportsShouldProcess,ConfirmImpact='High')]
param(
    [Parameter(Mandatory)][ValidateSet('TAICHUNG','TAOYUAN')][string]$Node,
    [string]$Repository='peca12968/Aevrynd',[uint32]$Issue=1,
    [string]$InstallRoot='C:\ProgramData\Aevrynd\Codex\relay',[string]$StateRoot='C:\ProgramData\Aevrynd\Codex',
    [string]$AuditRoot='C:\AevryndAudit\codex-relay',[string]$Workspace='C:\Aevrynd\Games'
)
$ErrorActionPreference='Stop';$taskName="Aevrynd-CodexCommandRelay-$Node";$taskPath='\Aevrynd\';$expected="Aevrynd Codex command relay owner=$Node schema=2"
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
$manifestPath=Assert-RelayPathSafe (Join-Path $InstallRoot 'install.json');$manifest=&(Get-Module CodexCommandRelay){param($p)Read-RelayJsonStrict $p $null 'install manifest' -VerifyAcl} $manifestPath
$manifestNames=@($manifest.PSObject.Properties.Name|Sort-Object);if(($manifestNames-join',')-cne'arguments,auditRoot,gitHubCliPath,gitPath,issue,node,powerShellPath,repository,schemaVersion,stateRoot,workspace'-or[int]$manifest.schemaVersion-ne1-or[string]$manifest.node-cne$Node-or[string]$manifest.repository-cne$Repository-or[uint32]$manifest.issue-ne$Issue-or[string]$manifest.workspace-cne$Workspace-or[string]$manifest.stateRoot-cne$StateRoot-or[string]$manifest.auditRoot-cne$AuditRoot-or-not([IO.Path]::GetFullPath([string]$manifest.powerShellPath).Equals($windowsPowerShell,[StringComparison]::OrdinalIgnoreCase))){throw 'Install manifest ownership fingerprint is invalid'}
$null=&(Get-Module CodexCommandRelay){param($p)Assert-RelayTrustedTool $p 'gh.exe'} ([string]$manifest.gitHubCliPath);$null=&(Get-Module CodexCommandRelay){param($p)Assert-RelayTrustedTool $p 'git.exe'} ([string]$manifest.gitPath);$expectedArguments=[string]$manifest.arguments
$currentUser=[Security.Principal.WindowsIdentity]::GetCurrent().Name
$existingExecute=[IO.Path]::GetFullPath([string]$actions[0].Execute)
if(-not$existingExecute.Equals($windowsPowerShell,[StringComparison]::OrdinalIgnoreCase)-or$actionArguments-cne$expectedArguments-or[string]$task.Principal.UserId-cne$currentUser){throw 'Scheduled task action is not owned by this relay'}
$payload=[Collections.Generic.List[string]]::new();foreach($name in @('CodexCommandRelay.psm1','Invoke-CodexCommandRelay.ps1','install.json')){$path=Assert-RelayPathSafe (Join-Path $InstallRoot $name);if(Test-Path -LiteralPath $path -PathType Leaf){$null=Assert-RelayFileAcl $path;$payload.Add($path)}}
if ($PSCmdlet.ShouldProcess("$taskPath$taskName",'Stop and unregister owned relay task, then remove its installed payload')) {
    Assert-RelayDirectoryAcl $StateRoot
    $stopPath=Join-Path $StateRoot 'relay-stop.json'
    Write-RelayJsonAtomic $stopPath ([ordered]@{schemaVersion=1;node=$Node;requestedAtUtc=[DateTimeOffset]::UtcNow.ToString('o')}) -VerifyAcl
    $deadline=[DateTimeOffset]::UtcNow.AddSeconds(10)
    do{$current=Get-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue;if($null-eq$current-or[string]$current.State-cne'Running'){break};Start-Sleep -Milliseconds 250}while([DateTimeOffset]::UtcNow-lt$deadline)
    if($current-and[string]$current.State-ceq'Running'){Stop-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction SilentlyContinue}
    Unregister-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Confirm:$false
    foreach($path in $payload){Remove-Item -LiteralPath $path -Force}
}
