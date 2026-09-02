param([switch]$Prompt)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
if ($Prompt) {
    $secure=Read-Host 'Light Qwen API Key' -AsSecureString
} else {
    $randomBytes = [byte[]]::new(48)
    [Security.Cryptography.RandomNumberGenerator]::Fill($randomBytes)
    $randomText = [Convert]::ToBase64String($randomBytes)
    $secure = ConvertTo-SecureString $randomText -AsPlainText -Force
    [Array]::Clear($randomBytes, 0, $randomBytes.Length)
    $randomText = $null
}
if($secure.Length-lt24-or$secure.Length-gt1280){throw 'Credential length is outside policy.'}
if(-not('AevryndLightCredentialWriter'-as[type])){Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;public static class AevryndLightCredentialWriter{[StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct C{public UInt32 Flags,Type;public string TargetName,Comment;public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;public UInt32 BlobSize;public IntPtr Blob;public UInt32 Persist,AttributeCount;public IntPtr Attributes;public string Alias,User;}[DllImport("advapi32.dll",EntryPoint="CredWriteW",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool W(ref C c,UInt32 f);public static void Write(IntPtr p,UInt32 n){var c=new C{Type=1,TargetName="Aevrynd/QwenLight/ApiKey",Blob=p,BlobSize=n,Persist=2,User=Environment.UserName};if(!W(ref c,0))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());}}
'@}
$pointer=[Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secure);try{[AevryndLightCredentialWriter]::Write($pointer,[uint32]($secure.Length*2))}finally{[Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer);$secure.Dispose()}
Write-Output 'Light API credential stored in Windows Credential Manager.'
