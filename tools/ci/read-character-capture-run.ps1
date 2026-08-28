param(
    [Parameter(Mandatory)] [string]$StateRoot,
    [Parameter(Mandatory)] [string]$OutputDirectory,
    [int]$HoldMilliseconds = 0
)
$ErrorActionPreference = 'Stop'
if (-not ('Aevrynd.CaptureLeaseNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Aevrynd { public static class CaptureLeaseNative {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr CreateFile(string path,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
} }
'@
}
$captureRoot = [IO.Path]::GetFullPath((Join-Path $StateRoot 'character-captures'))
$runsRoot = [IO.Path]::GetFullPath((Join-Path $captureRoot 'runs')).TrimEnd('\') + '\'
$pointerPath = Join-Path $captureRoot 'capture-current.txt';$lease=[IntPtr]::new(-1);$pointerBytes=$null;$runDirectory=$null;$pointerLines=$null
for($attempt=0;$attempt -lt 8;$attempt++){
    $pointerBytes=[IO.File]::ReadAllBytes($pointerPath);$pointerLines=[Text.Encoding]::UTF8.GetString($pointerBytes).Split("`n",[StringSplitOptions]::RemoveEmptyEntries)|ForEach-Object{$_.TrimEnd("`r")};$runLines=@($pointerLines|Where-Object{$_ -like 'run=*'});if($runLines.Count-ne 1){throw 'Capture pointer has no unique run.'};$runId=$runLines[0].Substring(4);if($runId-notmatch '^[0-9]+-[0-9]+$'){throw 'Capture run id is invalid.'};$runDirectory=[IO.Path]::GetFullPath((Join-Path $runsRoot $runId));if(-not $runDirectory.StartsWith($runsRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Capture run escaped its root.'}
    $lease=[Aevrynd.CaptureLeaseNative]::CreateFile($runDirectory,[uint32]2147483648,[uint32]3,[IntPtr]::Zero,[uint32]3,[uint32]33554432,[IntPtr]::Zero);if($lease-ne [IntPtr]::new(-1)){break};Start-Sleep -Milliseconds 10
}
if($lease-eq [IntPtr]::new(-1)){throw 'Capture run lease could not be acquired.'}
try {
    if(-not(Test-Path -LiteralPath $runDirectory -PathType Container)){throw 'Leased capture run disappeared.'};if((Get-Item -LiteralPath $runDirectory).Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Capture run cannot be a reparse point.'}
    $schema=@($pointerLines|Where-Object{$_ -eq 'schema=1'});$count=@($pointerLines|Where-Object{$_ -eq 'count=12'});if($schema.Count-ne 1-or$count.Count-ne 1){throw 'Capture manifest schema/count is invalid.'};$expectedNames=@('front-pair.png','female-profile.png','back-pair.png','male-profile.png','portrait-pair.png','female-face-front.png','male-face-front.png','equipment-front.png','equipment-swapped.png','locomotion-walk.png','locomotion-run.png','weapon-layer.png');$files=@($pointerLines|Where-Object{$_ -like 'file=*'});if($files.Count-ne 12){throw 'Capture manifest file count is invalid.'};$verified=@{};foreach($entry in $files){if($entry-notmatch '^file=([^ ]+) size=([0-9]+) sha256=([0-9a-f]{64})$'){throw 'Capture manifest entry is invalid.'};$name=$Matches[1];$expectedSize=[Int64]$Matches[2];$expectedHash=$Matches[3];if($name-notin$expectedNames-or$verified.ContainsKey($name)){throw 'Capture filename set is invalid.'};$file=Join-Path $runDirectory $name;$item=Get-Item -LiteralPath $file;if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Capture file cannot be a reparse point.'};if($item.Length-ne $expectedSize){throw 'Capture size mismatch.'};if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()-ne $expectedHash){throw 'Capture hash mismatch.'};$verified[$name]=$file};if(@(Get-ChildItem -LiteralPath $runDirectory -File -Filter '*.png').Count-ne 12){throw 'Capture run contains extra PNG files.'}
    if ($HoldMilliseconds -gt 0) { Start-Sleep -Milliseconds $HoldMilliseconds }
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    foreach($name in $expectedNames){Copy-Item -LiteralPath $verified[$name] -Destination $OutputDirectory}
    [IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'capture-current.txt'),$pointerBytes)
} finally {
    [Aevrynd.CaptureLeaseNative]::CloseHandle($lease)|Out-Null
}
