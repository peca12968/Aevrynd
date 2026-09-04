#requires -Version 7.0
[CmdletBinding()]
param([int]$Port = 8792)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$uri = "http://127.0.0.1:$Port/health"
$response = Invoke-RestMethod -Uri $uri -Method Get -TimeoutSec 5
if ($response.status -ne 'ok') { throw 'worker_health_invalid' }
$response | ConvertTo-Json -Compress

