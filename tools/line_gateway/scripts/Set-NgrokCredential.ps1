param([string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Assert-Administrator
$paths=Get-GatewayPaths -InstallRoot $InstallRoot
$transportLock=Enter-PublicTransportLock
try{
if(Test-Path -LiteralPath $paths.NgrokStateFile){throw 'Stop the active ngrok transport before changing its credential.'}
if(-not(Test-Path -LiteralPath $paths.InstallFile -PathType Leaf)){throw 'Install the LINE gateway before storing the ngrok credential.'}
if(-not('AevryndNgrokCredentialWriter'-as[type])){Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AevryndNgrokCredentialWriter {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Credential { public uint Flags,Type; public string TargetName,Comment; public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten; public uint CredentialBlobSize; public IntPtr CredentialBlob; public uint Persist,AttributeCount; public IntPtr Attributes; public string TargetAlias,UserName; }
 [DllImport("advapi32.dll",EntryPoint="CredWriteW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CredWrite(ref Credential value,uint flags);
 public static void Write(IntPtr secret,uint bytes){var value=new Credential{Type=1,TargetName="Aevrynd/LineGateway/NgrokAuthtoken",CredentialBlob=secret,CredentialBlobSize=bytes,Persist=2,UserName=Environment.UserName};if(!CredWrite(ref value,0))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());}
}
'@}
# Authtoken 只經由 SecureString 與 Credential Manager，不接受命令列參數或明文設定檔。
$secure=Read-Host 'ngrok authtoken (input is hidden)' -AsSecureString
if($secure.Length-lt20-or$secure.Length-gt512){$secure.Dispose();throw 'Ngrok authtoken length is outside policy.'}
$pointer=[Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secure)
try{[AevryndNgrokCredentialWriter]::Write($pointer,[uint32]($secure.Length*2))}finally{[Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer);$secure.Dispose()}
Write-Output 'Ngrok authtoken stored in Windows Credential Manager for the current user.'
}finally{Exit-PublicTransportLock $transportLock}
