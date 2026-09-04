[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('taichung','taoyuan')][string]$Node,
    [Parameter(Mandatory)][Uri]$Endpoint,[Parameter(Mandatory)][string]$SecretPath,[Parameter(Mandatory)][string]$CodexPath,[Parameter(Mandatory)][string]$SharedModulePath,
    [string]$Workspace='C:\Aevrynd\Games',[string]$StateRoot='C:\ProgramData\Aevrynd\Codex\event-relay',[string]$AuditRoot='C:\AevryndAudit\codex-event-relay'
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'CodexEventRelay.psm1') -Force
Invoke-CodexEventRelay @PSBoundParameters
