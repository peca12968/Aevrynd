#requires -Version 7.0
#requires -RunAsAdministrator
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('taichung', 'taoyuan')]
    [string]$NodeId,
    [string]$SourceRoot = (Join-Path $PSScriptRoot '..')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$destination = 'C:\ProgramData\Aevrynd\LunaWorker\App'
if (-not (Test-Path -LiteralPath (Join-Path $source 'worker.py') -PathType Leaf)) { throw 'worker_payload_missing' }
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $source '__init__.py') -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $source 'worker.py') -Destination $destination -Force
New-Item -ItemType Directory -Path 'C:\ProgramData\Aevrynd\LunaWorker\inbox' -Force | Out-Null
New-Item -ItemType Directory -Path 'C:\ProgramData\Aevrynd\LunaWorker\records' -Force | Out-Null
Set-Content -LiteralPath 'C:\ProgramData\Aevrynd\LunaWorker\node-id.txt' -Value $NodeId -Encoding ASCII
Write-Output "installed node=$NodeId; no API key was written"

