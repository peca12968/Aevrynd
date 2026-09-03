param(
    [Parameter(Mandatory)][ValidateSet('taichung','taoyuan')][string]$NodeId,
    [switch]$ControllerOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not ('AevryndGameModeCredentialWriter' -as [type])) {
    Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class AevryndGameModeCredentialWriter {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct C { public UInt32 Flags,Type; public string TargetName,Comment; public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten; public UInt32 BlobSize; public IntPtr Blob; public UInt32 Persist,AttributeCount; public IntPtr Attributes; public string Alias,User; }
 [DllImport("advapi32.dll",EntryPoint="CredWriteW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool W(ref C c,UInt32 f);
 public static void Write(string target,IntPtr p,UInt32 n) { var c=new C{Type=1,TargetName=target,Blob=p,BlobSize=n,Persist=2,User=Environment.UserName}; if(!W(ref c,0))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()); }
}
'@
}
function Set-FixedCredential([string]$Target,[string]$Prompt) {
    $secure = Read-Host $Prompt -AsSecureString
    if ($secure.Length -lt 32 -or $secure.Length -gt 1280) { $secure.Dispose(); throw 'Credential length is outside policy.' }
    $pointer = [Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secure)
    try { [AevryndGameModeCredentialWriter]::Write($Target,$pointer,[uint32]($secure.Length*2)) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer); $secure.Dispose() }
}
# 同一 node secret 必須分別在主機與該遠端以互動方式輸入；絕不放入 ZIP 或命令列。
Set-FixedCredential "Aevrynd/GameModeController/$NodeId" "Shared controller secret for $NodeId"
if (-not $ControllerOnly) { Set-FixedCredential "Aevrynd/GameModeController/$NodeId/QwenApiKey" "Local Qwen API key for $NodeId" }
Write-Output 'Credentials stored in Windows Credential Manager for the current user.'
