Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-LightPaths {
    $root = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'AevryndQwenLight'))
    return @{
        Root=$root; Runtime=Join-Path $root 'runtime'; Models=Join-Path $root 'models'; State=Join-Path $root 'state'
        Server=Join-Path $root 'runtime\llama-server.exe'; Model=Join-Path $root 'models\Qwen3-4B-Q4_K_M.gguf'
        StateFile=Join-Path $root 'state\light-state.json'
    }
}

function Assert-LightModel {
    param([hashtable]$Paths)
    if (-not(Test-Path -LiteralPath $Paths.Model -PathType Leaf)){throw 'Light model is missing.'}
    $item=Get-Item -LiteralPath $Paths.Model
    if($item.Length-ne2497280256){throw 'Light model byte count mismatch.'}
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $Paths.Model).Hash.ToLowerInvariant()-ne'7485fe6f11af29433bc51cab58009521f205840f5b4ae3a32fa7f92e8534fdf5'){throw 'Light model SHA-256 mismatch.'}
}

function Assert-PackageRuntime {
    param([string]$PackageRoot)
    $manifestPath=Join-Path $PackageRoot 'runtime-manifest.sha256'
    if(-not(Test-Path -LiteralPath $manifestPath -PathType Leaf)){throw 'Runtime manifest is missing.'}
    $expected=@{}
    foreach($line in Get-Content -LiteralPath $manifestPath){
        if($line-notmatch'^([0-9a-f]{64})  runtime/(.+)$'){throw 'Runtime manifest format is invalid.'}
        $relative=$Matches[2]
        if($expected.ContainsKey($relative)){throw 'Runtime manifest contains a duplicate entry.'}
        $expected[$relative]=$Matches[1]
    }
    $actual=Get-ChildItem -LiteralPath (Join-Path $PackageRoot 'runtime') -File
    if($actual.Count-ne$expected.Count){throw 'Runtime file set does not match the closed manifest.'}
    foreach($file in $actual){
        if(-not$expected.ContainsKey($file.Name)){throw "Unexpected runtime file: $($file.Name)"}
        if((Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()-ne$expected[$file.Name]){throw "Runtime hash mismatch: $($file.Name)"}
    }
}

function Read-LightCredential {
    if (-not ('AevryndLightCredentialReader' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AevryndLightCredentialReader {
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    struct Credential { public UInt32 Flags, Type; public IntPtr Target, Comment; public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten; public UInt32 BlobSize; public IntPtr Blob; public UInt32 Persist, AttributeCount; public IntPtr Attributes, Alias, User; }
    [DllImport("advapi32.dll", EntryPoint="CredReadW", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool ReadNative(string name, UInt32 type, UInt32 flags, out IntPtr credential);
    [DllImport("advapi32.dll")] static extern void CredFree(IntPtr credential);
    public static string Read() {
        IntPtr pointer;
        if (!ReadNative("Aevrynd/QwenLight/ApiKey", 1, 0, out pointer)) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        try { var credential = Marshal.PtrToStructure<Credential>(pointer); return Marshal.PtrToStringUni(credential.Blob, (int)credential.BlobSize / 2); }
        finally { CredFree(pointer); }
    }
}
'@
    }
    return [AevryndLightCredentialReader]::Read()
}
