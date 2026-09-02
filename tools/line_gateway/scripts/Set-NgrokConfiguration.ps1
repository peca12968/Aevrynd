param(
    [Parameter(Mandatory)][string]$Domain,
    [string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway"
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Ngrok.Common.ps1')
Assert-Administrator
$paths=Get-GatewayPaths -InstallRoot $InstallRoot
$transportLock=Enter-PublicTransportLock
try{
if(Test-Path -LiteralPath $paths.NgrokStateFile){throw 'Stop the active ngrok transport before changing its domain.'}
if(-not(Test-NgrokDevelopmentDomain $Domain)){throw 'Domain must be the exact lowercase ngrok-free.dev development domain assigned by the ngrok dashboard.'}
$temporary="$($paths.NgrokDomainFile).$([Guid]::NewGuid().ToString('N')).tmp";$bytes=[Text.UTF8Encoding]::new($false).GetBytes(([ordered]@{schemaVersion=1;domain=$Domain}|ConvertTo-Json -Compress))
try{$stream=[IO.FileStream]::new($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);try{$stream.Write($bytes);$stream.Flush($true)}finally{$stream.Dispose()};if(Test-Path $paths.NgrokDomainFile){$backup="$temporary.bak";[IO.File]::Replace($temporary,$paths.NgrokDomainFile,$backup,$true);Remove-Item $backup -Force}else{[IO.File]::Move($temporary,$paths.NgrokDomainFile)};$verified=Read-NgrokDomainConfiguration $paths;if($verified.Domain-cne$Domain){throw 'Ngrok domain durable read-back failed.'}}finally{[Array]::Clear($bytes,0,$bytes.Length);if(Test-Path $temporary){Remove-Item $temporary -Force}}
Write-Output "Ngrok development domain configured: https://$Domain/webhook"
}finally{Exit-PublicTransportLock $transportLock}
