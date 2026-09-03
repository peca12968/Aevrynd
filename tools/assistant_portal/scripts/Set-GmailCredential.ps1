param([switch]$Replace)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-PowerShellSeven
Add-PortalCredentialInterop
if(-not$Replace){if([AevryndPortalCredential]::ValidGmail($script:GmailCredentialTarget)){Write-Output 'Existing Gmail credential retained.';return};if([AevryndPortalCredential]::Available($script:GmailCredentialTarget,1,128)){throw 'Existing Gmail credential is invalid. Run this script again with -Replace to enter a replacement securely.'}}
$secure = Read-Host 'Gmail app password for slime12968@gmail.com (input is hidden)' -AsSecureString
if ($secure.Length -lt 16 -or $secure.Length -gt 64) { $secure.Dispose(); throw 'Gmail app password must contain exactly 16 ASCII letters or digits after spaces are removed.' }
$pointer = [Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secure)
try { [AevryndPortalCredential]::StoreGmail($script:GmailCredentialTarget,$pointer,[uint32]($secure.Length*2)) }
finally { [Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer); $secure.Dispose() }
Write-Output 'Gmail app password stored in Windows Credential Manager for the current user.'
