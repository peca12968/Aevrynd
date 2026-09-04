#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('taichung', 'taoyuan')]
    [string]$NodeId,
    [string]$Python = 'python.exe',
    [int]$Port = 8792
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([string]::IsNullOrWhiteSpace($env:AEVRYND_OPENAI_API_KEY)) {
    throw 'AEVRYND_OPENAI_API_KEY_not_set'
}
$app = 'C:\ProgramData\Aevrynd\LunaWorker\App'
if (-not (Test-Path -LiteralPath (Join-Path $app 'worker.py') -PathType Leaf)) { throw 'worker_not_installed' }
& $Python (Join-Path $app 'worker.py') --node-id $NodeId --port $Port --work-dir 'C:\ProgramData\Aevrynd\LunaWorker'
exit $LASTEXITCODE

