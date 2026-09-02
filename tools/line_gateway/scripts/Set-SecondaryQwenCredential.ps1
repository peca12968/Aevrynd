param([string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Assert-Administrator
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
if (-not (Test-Path -LiteralPath $paths.InstallFile -PathType Leaf)) { throw 'Install the LINE gateway before storing the secondary Qwen credential.' }
if (-not ('AevryndSecondaryQwenCredentialWriter' -as [type])) {
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AevryndSecondaryQwenCredentialWriter {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Credential { public uint Flags,Type; public string TargetName,Comment; public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten; public uint CredentialBlobSize; public IntPtr CredentialBlob; public uint Persist,AttributeCount; public IntPtr Attributes; public string TargetAlias,UserName; }
 [DllImport("advapi32.dll",EntryPoint="CredWriteW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CredWrite(ref Credential value,uint flags);
 public static void Write(IntPtr secret,uint bytes){var value=new Credential{Type=1,TargetName="Aevrynd/LineGateway/SecondaryQwenApiKey",CredentialBlob=secret,CredentialBlobSize=bytes,Persist=2,UserName=Environment.UserName};if(!CredWrite(ref value,0))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());}
}
'@
}
# 密鑰只存在 SecureString 與 Credential Manager，不接受命令列或明文檔案輸入。
$secure = Read-Host 'Secondary Qwen API key (input is hidden)' -AsSecureString
if ($secure.Length -lt 16 -or $secure.Length -gt 1280) { $secure.Dispose(); throw 'Credential length must be between 16 and 1280 characters.' }
$pointer = [Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secure)
try { [AevryndSecondaryQwenCredentialWriter]::Write($pointer,[uint32]($secure.Length*2)) }
finally { [Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer); $secure.Dispose() }
Write-Output 'Secondary Qwen credential stored in Windows Credential Manager for the current user.'
