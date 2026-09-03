param([switch]$Replace)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-PowerShellSeven
Add-PortalCredentialInterop
if(-not$Replace-and[AevryndPortalCredential]::Available($script:NgrokApiCredentialTarget,16,2048)){Write-Output 'Existing ngrok API credential retained.';return}
$secure=Read-Host 'ngrok API key (input is hidden)' -AsSecureString
if($secure.Length-lt16-or$secure.Length-gt2048){$secure.Dispose();throw 'ngrok API key length is outside policy.'}
$pointer=[Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secure)
try{[AevryndPortalCredential]::Store($script:NgrokApiCredentialTarget,$pointer,[uint32]($secure.Length*2))}
finally{[Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer);$secure.Dispose()}
Write-Output 'ngrok API key stored in Windows Credential Manager for the current user.'
