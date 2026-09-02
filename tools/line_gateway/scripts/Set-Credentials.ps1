param([string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Assert-Administrator
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
if (-not (Test-Path -LiteralPath $paths.InstallFile -PathType Leaf)) {
    throw 'Install the LINE gateway before storing credentials.'
}

if (-not ('AevryndCredentialWriter' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class AevryndCredentialWriter {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct Credential {
        public UInt32 Flags;
        public UInt32 Type;
        public string TargetName;
        public string Comment;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
        public UInt32 CredentialBlobSize;
        public IntPtr CredentialBlob;
        public UInt32 Persist;
        public UInt32 AttributeCount;
        public IntPtr Attributes;
        public string TargetAlias;
        public string UserName;
    }

    [DllImport("advapi32.dll", EntryPoint = "CredWriteW", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CredWrite(ref Credential credential, UInt32 flags);

    public static void Write(string target, IntPtr secret, UInt32 byteCount) {
        Credential credential = new Credential();
        credential.Type = 1;
        credential.TargetName = target;
        credential.CredentialBlob = secret;
        credential.CredentialBlobSize = byteCount;
        credential.Persist = 2;
        credential.UserName = Environment.UserName;
        if (!CredWrite(ref credential, 0)) {
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        }
    }
}
'@
}

function Set-SecureGenericCredential {
    param([string]$Target, [string]$Prompt)
    $secure = Read-Host -Prompt $Prompt -AsSecureString
    if ($secure.Length -lt 16 -or $secure.Length -gt 1,280) {
        throw 'Credential length must be between 16 and 1280 characters.'
    }
    $pointer = [Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($secure)
    try {
        [AevryndCredentialWriter]::Write($Target, $pointer, [uint32]($secure.Length * 2))
    } finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeCoTaskMemUnicode($pointer)
        $secure.Dispose()
    }
}

# 三個值只在 SecureString 與受保護的 Credential Manager 中短暫存在。
Set-SecureGenericCredential -Target 'Aevrynd/LineGateway/ChannelSecret' -Prompt 'LINE Channel Secret'
Set-SecureGenericCredential -Target 'Aevrynd/LineGateway/ChannelAccessToken' -Prompt 'LINE Channel Access Token'
Set-SecureGenericCredential -Target 'Aevrynd/LineGateway/QwenApiKey' -Prompt 'Primary Qwen API Key'
Write-Output 'Credentials stored in Windows Credential Manager for the current user.'
