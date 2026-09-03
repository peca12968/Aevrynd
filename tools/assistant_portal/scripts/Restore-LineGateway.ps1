Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-Administrator
# Restore 只依賴 durable journal 與受保護 LINE broker；不要求 portal、ngrok、SMTP 或 credential 健康。
& (Join-Path $PSScriptRoot 'Admin-Broker.ps1') -Operation Restore
Write-Output 'Previous LINE baseline restored and portal startup disabled.'
