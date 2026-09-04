[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Node,
    [string]$Repository='peca12968/Aevrynd',[uint32]$Issue=1,
    [string]$StateRoot='C:\ProgramData\Aevrynd\Codex',
    [string]$AuditRoot='C:\AevryndAudit\codex-relay',
    [string]$Workspace='C:\Aevrynd\Games',[switch]$BaselineOnly,[switch]$WhatIf
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'CodexCommandRelay.psm1') -Force
Invoke-CodexCommandRelay @PSBoundParameters
