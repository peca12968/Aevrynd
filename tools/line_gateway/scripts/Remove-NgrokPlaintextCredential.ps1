param([switch]$ConfirmRotated,[string]$InstallRoot="$env:ProgramData\Aevrynd\LineGateway")
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'Funnel.Common.ps1')
Assert-Administrator
$paths=Get-GatewayPaths -InstallRoot $InstallRoot
if(-not('AevryndFileDisposition'-as[type])){Add-Type @'
using System;using System.ComponentModel;using System.Runtime.InteropServices;using Microsoft.Win32.SafeHandles;
public static class AevryndFileDisposition {
 const uint GENERIC_READ=0x80000000,GENERIC_WRITE=0x40000000,DELETE=0x00010000,OPEN_EXISTING=3,FILE_ATTRIBUTE_NORMAL=0x80,FILE_FLAG_WRITE_THROUGH=0x80000000;
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern SafeFileHandle CreateFileW(string p,uint a,uint s,IntPtr sa,uint c,uint f,IntPtr t);
 [StructLayout(LayoutKind.Sequential)] struct FILE_DISPOSITION_INFO { [MarshalAs(UnmanagedType.Bool)] public bool DeleteFile; }
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetFileInformationByHandle(SafeFileHandle h,int c,ref FILE_DISPOSITION_INFO i,uint s);
 public static SafeFileHandle OpenExclusive(string path){var h=CreateFileW(path,GENERIC_READ|GENERIC_WRITE|DELETE,0,IntPtr.Zero,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,IntPtr.Zero);if(h.IsInvalid){int e=Marshal.GetLastWin32Error();h.Dispose();throw new Win32Exception(e);}return h;}
 public static void DeleteOnClose(SafeFileHandle handle){var info=new FILE_DISPOSITION_INFO{DeleteFile=true};if(!SetFileInformationByHandle(handle,4,ref info,(uint)Marshal.SizeOf<FILE_DISPOSITION_INFO>()))throw new Win32Exception(Marshal.GetLastWin32Error());}
}
'@}
$transportLock=Enter-PublicTransportLock
try{
    if(Test-Path -LiteralPath $paths.NgrokStateFile){throw 'Stop the active ngrok transport before removing plaintext credentials.'}
    $path=[IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'ngrok\ngrok.yml'))
    if(-not(Test-Path -LiteralPath $path -PathType Leaf)){Write-Output 'No user ngrok.yml plaintext credential file exists.';exit 0}
    Assert-NoReparseAncestors $path
    $directoryHandle=Open-VerifiedDirectoryHandle ([IO.Path]::GetDirectoryName($path));$stream=$null;$bytes=$null
    try{
        $deleteHandle=[AevryndFileDisposition]::OpenExclusive($path)
        try{$stream=[IO.FileStream]::new($deleteHandle,[IO.FileAccess]::ReadWrite,4096,$false)}catch{$deleteHandle.Dispose();throw}
        if([Aevrynd.SafePath]::FinalPath($stream.SafeFileHandle)-cne$path){throw 'Ngrok configuration final handle path is invalid.'}
        if($stream.Length-gt65536){throw 'Ngrok configuration size is outside cleanup policy.'}
        if($stream.Length-eq0){
            [AevryndFileDisposition]::DeleteOnClose($stream.SafeFileHandle)
        }else{
        $bytes=[byte[]]::new([int]$stream.Length);$offset=0
        while($offset-lt$bytes.Length){$read=$stream.Read($bytes,$offset,$bytes.Length-$offset);if($read-eq0){throw 'Ngrok configuration was truncated during read.'};$offset+=$read}
        $needle=[Text.Encoding]::ASCII.GetBytes('authtoken');$found=$false
        for($i=0;$i-le$bytes.Length-$needle.Length-and-not$found;$i++){$match=$true;for($j=0;$j-lt$needle.Length;$j++){if(([byte][char]([char]$bytes[$i+$j]).ToString().ToLowerInvariant())-ne$needle[$j]){$match=$false;break}};if($match){$found=$true}}
        if(-not$found){Write-Output 'User ngrok.yml contains no authtoken field.';exit 0}
        if(-not$ConfirmRotated){throw 'Rotate the exposed authtoken in the ngrok dashboard, then rerun with -ConfirmRotated.'}
        $stream.Position=0;$zeros=[byte[]]::new($bytes.Length);try{$stream.Write($zeros,0,$zeros.Length);$stream.SetLength(0);$stream.Flush($true);[AevryndFileDisposition]::DeleteOnClose($stream.SafeFileHandle)}finally{[Array]::Clear($zeros,0,$zeros.Length)}
        }
    }finally{if($null-ne$bytes){[Array]::Clear($bytes,0,$bytes.Length)};if($null-ne$stream){$stream.Dispose()};$directoryHandle.Dispose()}
    if(Test-Path -LiteralPath $path){throw 'Ngrok plaintext configuration remained after handle-bound deletion.'}
    Write-Output 'Removed the rotated plaintext ngrok configuration without creating a managed secret string or backup.'
}finally{Exit-PublicTransportLock $transportLock}
