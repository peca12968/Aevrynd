Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
$result=Invoke-PortalPreflight
Write-Output "PREFLIGHT_OK line_gateway=$($result.LineGatewayRunning) line_ngrok=$($result.LineNgrokRunning) preview_pid=$($result.PreviewPid)"
