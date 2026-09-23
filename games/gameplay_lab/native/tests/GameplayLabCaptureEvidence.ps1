$ErrorActionPreference = 'Stop'

function Assert-GameplayReadbackGpuProvenance($Capture) {
  $phases=@($Capture.phases)
  if($phases.Count-lt2){throw "Readback GPU provenance phase count failed: $($Capture.name)"}
  foreach($passName in @('atmosphere','temporalResolve')){
    $baseline=$null
    for($phaseIndex=0;$phaseIndex-lt$phases.Count;++$phaseIndex){
      $gpu=$phases[$phaseIndex].gpuProvenance
      if($null-eq$gpu-or$null-eq$gpu.PSObject.Properties[$passName]){throw "Readback GPU provenance missing: $($Capture.name) phase=$phaseIndex pass=$passName"}
      $pass=$gpu.$passName
      $identity=$pass.outputIdentity
      if($pass.complete-ne1-or$pass.fingerprint-eq0-or$pass.submissionFence-eq0-or$pass.historyGeneration-eq0-or
         $pass.outputWidth-eq0-or$pass.outputHeight-eq0-or$pass.rejection-ne0-or$null-eq$identity-or
         $identity.generation-eq0-or$identity.width-eq0-or$identity.height-eq0-or$identity.format-eq0-or$identity.sampleCount-eq0-or
         $pass.outputWidth-ne$identity.width-or$pass.outputHeight-ne$identity.height){
        throw "Readback GPU provenance completeness failed: $($Capture.name) phase=$phaseIndex pass=$passName"
      }
      if($phaseIndex-eq0){
        if($pass.dispatched-ne1-or$pass.reused-ne0){throw "Readback GPU phase-zero dispatch failed: $($Capture.name) pass=$passName"}
        $baseline=$pass
      } else {
        if($pass.dispatched-ne0-or$pass.reused-ne1){throw "Readback GPU phase reuse failed: $($Capture.name) phase=$phaseIndex pass=$passName"}
        foreach($field in @('fingerprint','resourceCreates','resourceReleases','submissionFence','historyGeneration','outputWidth','outputHeight','historyIndex')){
          if($pass.$field-ne$baseline.$field){throw "Readback GPU locked provenance failed: $($Capture.name) phase=$phaseIndex pass=$passName field=$field"}
        }
        foreach($field in @('generation','width','height','format','sampleCount','state')){
          if($pass.outputIdentity.$field-ne$baseline.outputIdentity.$field){throw "Readback GPU output identity drifted: $($Capture.name) phase=$phaseIndex pass=$passName field=$field"}
        }
      }
    }
  }
}

function ConvertFrom-GameplayObjectIdentityContractProbeOutput([object[]]$Output,[int]$ExitCode) {
  if($ExitCode-ne0-or$Output.Count-ne1){throw 'Gameplay Object-ID compiler contract probe failed or emitted trailing output'}
  $line=[string]$Output[0];if([string]::IsNullOrWhiteSpace($line)-or$line.Contains("`r")-or$line.Contains("`n")){throw 'Gameplay Object-ID compiler contract probe output was not one strict line'}
  try{$receipt=$line|ConvertFrom-Json -ErrorAction Stop}catch{throw 'Gameplay Object-ID compiler contract probe JSON was invalid'}
  $expected=@('schemaVersion','source','environmentHigh','terrainLow','generation','hlodProxyBit','proxyHigh','contractPassed');$actual=@($receipt.PSObject.Properties.Name)
  if($actual.Count-ne$expected.Count-or@($expected|Where-Object{$_-cnotin$actual}).Count-ne0){throw 'Gameplay Object-ID compiler contract probe schema fields were invalid'}
  if($receipt.schemaVersion.GetType().Name-cnotin@('Int32','Int64')-or$receipt.terrainLow.GetType().Name-cnotin@('Int32','Int64')-or$receipt.generation.GetType().Name-cnotin@('Int32','Int64')-or$receipt.contractPassed.GetType().Name-cnotin@('Int32','Int64')-or$receipt.source-isnot[string]-or$receipt.environmentHigh-isnot[string]-or$receipt.hlodProxyBit-isnot[string]-or$receipt.proxyHigh-isnot[string]){throw 'Gameplay Object-ID compiler contract probe field types were invalid'}
  if($receipt.schemaVersion-ne1-or$receipt.source-cne'GameplayObjectIdentityContractProbe'-or$receipt.terrainLow-ne1-or$receipt.generation-ne1-or$receipt.contractPassed-ne1-or$receipt.environmentHigh-notmatch'^[1-9][0-9]{0,19}$'-or$receipt.hlodProxyBit-notmatch'^[1-9][0-9]{0,19}$'-or$receipt.proxyHigh-notmatch'^[1-9][0-9]{0,19}$'){throw 'Gameplay Object-ID compiler contract probe identity was invalid'}
  try{$environmentHigh=[uint64]::Parse($receipt.environmentHigh,[Globalization.CultureInfo]::InvariantCulture);$hlodProxyBit=[uint64]::Parse($receipt.hlodProxyBit,[Globalization.CultureInfo]::InvariantCulture);$proxyHigh=[uint64]::Parse($receipt.proxyHigh,[Globalization.CultureInfo]::InvariantCulture)}catch{throw 'Gameplay Object-ID compiler contract probe uint64 fields were invalid'}
  if($hlodProxyBit-ne([uint64]1-shl63)-or$environmentHigh-eq0-or($environmentHigh-band$hlodProxyBit)-ne0-or$proxyHigh-ne($environmentHigh-bor$hlodProxyBit)){throw 'Gameplay Object-ID compiler contract probe relationship was invalid'}
  return [pscustomobject]@{environmentHigh=$environmentHigh;terrainLow=[uint64]$receipt.terrainLow;generation=[uint64]$receipt.generation;hlodProxyBit=$hlodProxyBit;proxyHigh=$proxyHigh}
}

function Get-GameplayObjectIdentityContract([string]$ProbePath='') {
  $cached=Get-Variable -Name GameplayObjectIdentityContract -Scope Script -ErrorAction SilentlyContinue
  if($cached){return $cached.Value}
  if([string]::IsNullOrWhiteSpace($ProbePath)){throw 'Gameplay Object-ID compiler contract probe path was not initialized'}
  try{$full=(Resolve-Path -LiteralPath $ProbePath -ErrorAction Stop).Path}catch{throw 'Gameplay Object-ID compiler contract probe executable was missing'}
  if(-not(Test-Path -LiteralPath $full -PathType Leaf)-or[IO.Path]::GetExtension($full)-cne'.exe'-or[IO.Path]::GetFileName($full)-cne'Aevrynd-Gameplay-Object-Identity-Contract.exe'){throw 'Gameplay Object-ID compiler contract probe path was invalid'}
  # 使用該次程序的退出碼，不依賴呼叫端是否曾建立原生命令的自動變數。 (native exit).
  $startInfo=[Diagnostics.ProcessStartInfo]::new()
  $startInfo.FileName=$full;$startInfo.UseShellExecute=$false;$startInfo.CreateNoWindow=$true
  $startInfo.RedirectStandardOutput=$true;$startInfo.RedirectStandardError=$true
  $process=[Diagnostics.Process]::new();$process.StartInfo=$startInfo
  try {
    if(-not$process.Start()){throw 'Gameplay Object-ID compiler contract probe did not start'}
    $stdoutTask=$process.StandardOutput.ReadToEndAsync();$stderrTask=$process.StandardError.ReadToEndAsync()
    if(-not$process.WaitForExit(10000)){$process.Kill();$process.WaitForExit();throw 'Gameplay Object-ID compiler contract probe timed out'}
    $exitCode=$process.ExitCode
    $stdout=$stdoutTask.GetAwaiter().GetResult();$stderr=$stderrTask.GetAwaiter().GetResult()
    if(-not[string]::IsNullOrEmpty($stderr)){throw "Gameplay Object-ID compiler contract probe emitted stderr: exit=$exitCode"}
    # 保留額外空行，讓既有單行契約拒絕尾隨輸出。 (strict output).
    $output=@([regex]::Split($stdout,'\r\n|\n|\r'))
    if($output.Count-gt1-and$output[-1]-ceq''){$output=@($output[0..($output.Count-2)])}
  } finally {$process.Dispose()}
  $script:GameplayObjectIdentityContract=ConvertFrom-GameplayObjectIdentityContractProbeOutput $output $exitCode
  return $script:GameplayObjectIdentityContract
}

function Get-GameplayGodCoverageContract {
  $cached=Get-Variable -Name GameplayGodCoverageContract -Scope Script -ErrorAction SilentlyContinue
  if($cached){return $cached.Value}
  $headerPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../include/GameplayLabGame.h'))
  $header=[IO.File]::ReadAllText($headerPath)
  $coverageMatch=[regex]::Match($header,'GameplayGodMinimumPlayerCoverage3D\s*=\s*([0-9]*\.[0-9]+)f')
  $widthMatch=[regex]::Match($header,'GameplayCaptureViewportWidth3D\s*=\s*([0-9]+)')
  $heightMatch=[regex]::Match($header,'GameplayCaptureViewportHeight3D\s*=\s*([0-9]+)')
  if(-not$coverageMatch.Success-or-not$widthMatch.Success-or-not$heightMatch.Success){throw 'Gameplay God coverage source contract was missing'}
  $culture=[Globalization.CultureInfo]::InvariantCulture
  $coverage=[double]::Parse($coverageMatch.Groups[1].Value,$culture);$width=[uint32]::Parse($widthMatch.Groups[1].Value,$culture);$height=[uint32]::Parse($heightMatch.Groups[1].Value,$culture)
  if($coverage-le0-or$coverage-ge1-or$width-lt1-or$height-lt1){throw 'Gameplay God coverage source contract was invalid'}
  $script:GameplayGodCoverageContract=[pscustomobject]@{coverage=$coverage;width=$width;height=$height;minimumPixels=[uint64][Math]::Ceiling($coverage*$width*$height)}
  return $script:GameplayGodCoverageContract
}

function Assert-GameplayCameraPolicyReceipt($Receipt,[string]$RunId,[string[]]$ExpectedNames){
  if($Receipt.runId-cne$RunId-or$Receipt.source-cne'GameplayLabCameraPolicy'-or@($Receipt.captures).Count-ne$ExpectedNames.Count){throw 'Gameplay camera policy receipt identity failed'}
  $paletteMinimum=[double][single]0.08;$runtimeMinimum=[double][single]0.05
  for($index=0;$index-lt$ExpectedNames.Count;++$index){$capture=$Receipt.captures[$index];$palette=$ExpectedNames[$index]-ceq'palette-building.png';$minimum=if($palette){$paletteMinimum}else{$runtimeMinimum};if($capture.name-cne$ExpectedNames[$index]-or[uint64]$capture.paletteOpen-ne[uint64]$palette-or[single]$capture.minimumSubjectScreenArea-ne[single]$minimum-or($palette-and[single]$capture.actualSubjectScreenArea-lt[single]$paletteMinimum)){throw "Gameplay camera palette open/close/minimum-area contract failed: $($ExpectedNames[$index])"}}
}

if(-not('AevryndGameplayWindowsPath' -as [type])){
  Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

public sealed class AevryndGameplayPinnedPath : IDisposable
{
    public SafeFileHandle Handle { get; private set; }
    public uint Attributes { get; private set; }
    public uint NumberOfLinks { get; private set; }

    internal AevryndGameplayPinnedPath(SafeFileHandle handle, uint attributes, uint numberOfLinks)
    {
        Handle = handle;
        Attributes = attributes;
        NumberOfLinks = numberOfLinks;
    }

    public void Dispose()
    {
        if (Handle != null) Handle.Dispose();
    }
}

public static class AevryndGameplayWindowsPath
{
    private const uint FileShareRead = 0x00000001;
    private const uint FileShareWrite = 0x00000002;
    private const uint OpenExisting = 3;
    private const uint FileAttributeDirectory = 0x00000010;
    private const uint FileAttributeReparsePoint = 0x00000400;
    private const uint FileFlagOpenReparsePoint = 0x00200000;
    private const uint FileFlagBackupSemantics = 0x02000000;

    [StructLayout(LayoutKind.Sequential)]
    private struct ByHandleFileInformation
    {
        public uint FileAttributes;
        public System.Runtime.InteropServices.ComTypes.FILETIME CreationTime;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastAccessTime;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWriteTime;
        public uint VolumeSerialNumber;
        public uint FileSizeHigh;
        public uint FileSizeLow;
        public uint NumberOfLinks;
        public uint FileIndexHigh;
        public uint FileIndexLow;
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFileW(string fileName, uint desiredAccess, uint shareMode,
        IntPtr securityAttributes, uint creationDisposition, uint flagsAndAttributes, IntPtr templateFile);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetFileInformationByHandle(SafeFileHandle handle, out ByHandleFileInformation information);

    public static AevryndGameplayPinnedPath Open(string path, bool directory)
    {
        uint flags = FileFlagOpenReparsePoint | (directory ? FileFlagBackupSemantics : 0u);
        uint sharing = FileShareRead | (directory ? FileShareWrite : 0u);
        SafeFileHandle handle = CreateFileW(path, 0u, sharing, IntPtr.Zero, OpenExisting, flags, IntPtr.Zero);
        if (handle.IsInvalid)
        {
            int error = Marshal.GetLastWin32Error();
            handle.Dispose();
            throw new Win32Exception(error, "Could not pin capture provenance path");
        }
        ByHandleFileInformation information;
        if (!GetFileInformationByHandle(handle, out information))
        {
            int error = Marshal.GetLastWin32Error();
            handle.Dispose();
            throw new Win32Exception(error, "Could not query capture provenance path");
        }
        bool isDirectory = (information.FileAttributes & FileAttributeDirectory) != 0u;
        if (isDirectory != directory)
        {
            handle.Dispose();
            throw new InvalidOperationException("Capture provenance path type changed during validation");
        }
        return new AevryndGameplayPinnedPath(handle, information.FileAttributes, information.NumberOfLinks);
    }

    public static bool IsReparsePoint(AevryndGameplayPinnedPath path)
    {
        return (path.Attributes & FileAttributeReparsePoint) != 0u;
    }
}
'@
}

function Close-GameplayCapturePathPins {
  $existing=Get-Variable -Name GameplayCapturePathPins -Scope Script -ErrorAction SilentlyContinue
  if($existing){foreach($pin in @($existing.Value)){if($null-ne$pin){$pin.Dispose()}};Remove-Variable -Name GameplayCapturePathPins -Scope Script -ErrorAction SilentlyContinue}
}

function Resolve-GameplayCaptureExecutablePath([string]$Executable,[string]$AllowedRoot){
  if([string]::IsNullOrWhiteSpace($Executable)-or$Executable-match'(^|[\\/])\.\.([\\/]|$)'){throw 'Gameplay capture executable path is empty or contains traversal'}
  if([string]::IsNullOrWhiteSpace($AllowedRoot)-or$AllowedRoot-match'(^|[\\/])\.\.([\\/]|$)'){throw 'Gameplay capture source root is empty or contains traversal'}
  try{$sourceRoot=(Resolve-Path -LiteralPath $AllowedRoot -ErrorAction Stop).Path}catch{throw 'Gameplay capture source root does not exist'}
  if(-not(Test-Path -LiteralPath $sourceRoot -PathType Container)){throw 'Gameplay capture source root is not a directory'}
  $sourceRoot=[IO.Path]::GetFullPath($sourceRoot).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
  try{$resolved=(Resolve-Path -LiteralPath $Executable -ErrorAction Stop).Path}catch{throw 'Gameplay capture executable does not exist'}
  $full=[IO.Path]::GetFullPath($resolved);$releaseRoot=Split-Path -Parent $full;$binRoot=Split-Path -Parent $releaseRoot;$buildRoot=Split-Path -Parent $binRoot
  $expected=[IO.Path]::GetFullPath((Join-Path $buildRoot 'bin\Release\Aevrynd-Gameplay-Lab.exe'));$cachePath=Join-Path $buildRoot 'CMakeCache.txt'
  if(-not(Test-Path -LiteralPath $full -PathType Leaf)-or[IO.Path]::GetExtension($full)-cne'.exe'-or-not[string]::Equals($full,$expected,[StringComparison]::OrdinalIgnoreCase)-or-not(Test-Path -LiteralPath $cachePath -PathType Leaf)-or-not[string]::Equals([IO.Path]::GetPathRoot($buildRoot),[IO.Path]::GetPathRoot($full),[StringComparison]::OrdinalIgnoreCase)){throw 'Gameplay capture executable was not the exact fresh build bin/Release target'}
  $pins=[Collections.Generic.List[IDisposable]]::new()
  try{
    $directories=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($leaf in @($sourceRoot,$buildRoot,$binRoot,$releaseRoot)){
      $current=[IO.Path]::GetFullPath($leaf).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar);$volumeRoot=[IO.Path]::GetPathRoot($current).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
      while(-not[string]::IsNullOrWhiteSpace($current)-and-not[string]::Equals($current,$volumeRoot,[StringComparison]::OrdinalIgnoreCase)){
        $null=$directories.Add($current);$parent=[IO.Path]::GetDirectoryName($current);if([string]::IsNullOrWhiteSpace($parent)-or[string]::Equals($parent,$current,[StringComparison]::OrdinalIgnoreCase)){break};$current=$parent.TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
      }
    }
    foreach($directory in $directories){$pin=[AevryndGameplayWindowsPath]::Open($directory,$true);$pins.Add($pin);if([AevryndGameplayWindowsPath]::IsReparsePoint($pin)){throw "Gameplay capture provenance directory was a reparse point: $directory"}}
    $executablePin=[AevryndGameplayWindowsPath]::Open($full,$false);$pins.Add($executablePin);if([AevryndGameplayWindowsPath]::IsReparsePoint($executablePin)-or$executablePin.NumberOfLinks-ne1){throw 'Gameplay capture executable was a reparse point or hardlink'}
    $cachePin=[AevryndGameplayWindowsPath]::Open($cachePath,$false);$pins.Add($cachePin);if([AevryndGameplayWindowsPath]::IsReparsePoint($cachePin)-or$cachePin.NumberOfLinks-ne1){throw 'Gameplay capture CMake cache was a reparse point or hardlink'}
    try{$cache=[IO.File]::ReadAllText($cachePath)}catch{throw 'Gameplay capture build root CMake cache was unreadable'}
    $sourceIdentityDeclarations=[regex]::Matches($cache,'(?mi)^[ \t]*CMAKE_HOME_DIRECTORY[^\r\n]*\r?$')
    $sourceIdentityMatches=[regex]::Matches($cache,'(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=([^\r\n]+)\r?$')
    if($sourceIdentityDeclarations.Count-ne1-or$sourceIdentityMatches.Count-ne1){throw 'Gameplay capture build root must declare exactly one well-formed CMake source identity'}
    $declaredSourceText=$sourceIdentityMatches[0].Groups[1].Value.Trim();if([string]::IsNullOrWhiteSpace($declaredSourceText)-or$declaredSourceText-match'(^|[\\/])\.\.([\\/]|$)'){throw 'Gameplay capture build root declared an invalid CMake source identity'}
    try{$declaredSource=[IO.Path]::GetFullPath($declaredSourceText).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)}catch{throw 'Gameplay capture build root declared an invalid CMake source identity'}
    if(-not[string]::Equals($declaredSource,$sourceRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Gameplay capture build root belonged to a different source tree'}
    Close-GameplayCapturePathPins;$script:GameplayCapturePathPins=@($pins);$pins=$null
    return $full
  } finally {if($null-ne$pins){foreach($pin in $pins){if($null-ne$pin){$pin.Dispose()}}}}
}
function Get-GameplayEvidenceBgra32($Bitmap) {
  $converted=[Drawing.Bitmap]::new($Bitmap.Width,$Bitmap.Height,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $graphics=[Drawing.Graphics]::FromImage($converted);$data=$null
  try {
    $graphics.DrawImageUnscaled($Bitmap,0,0);$rectangle=[Drawing.Rectangle]::new(0,0,$converted.Width,$converted.Height);$data=$converted.LockBits($rectangle,[Drawing.Imaging.ImageLockMode]::ReadOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bytes=[byte[]]::new([Math]::Abs($data.Stride)*$converted.Height);[Runtime.InteropServices.Marshal]::Copy($data.Scan0,$bytes,0,$bytes.Length);return [pscustomobject]@{bytes=$bytes}
  } finally {if($data){$converted.UnlockBits($data)};$graphics.Dispose();$converted.Dispose()}
}

function Assert-GameplayUltraDiagnosticSchema($Value) {
  $numbers=@('epoch','downgradeCount','requested','actual','reason','phase','firstFalseReason','gpu_epoch','gpu_observationIndex','gpu_submittedFrameSerial','gpu_completedFrameSerial','gpu_timingSampleSerial','gpu_lastGpuFrameNanoseconds','gpu_telemetryAgeNanoseconds','gpu_dedicatedVideoMemoryBytes','gpu_currentVideoMemoryUsageBytes','gpu_rendererAdapterLuidLow','gpu_rendererAdapterLuidHigh','gpu_telemetryAdapterLuidLow','gpu_telemetryAdapterLuidHigh','gpu_hardVramBudgetBytes','gpu_pendingGpuReleaseBytes','gpu_debugErrorCount','gpu_debugWarningCount','gpu_stableFrames','gpu_requiredStableFrames','gpu_timingProvenance','terminalOutcome','terminalReason','terminalEpoch');$booleans=@('requestActive','noOp','applyAttempted','applySucceeded','postApplyPassed','rollbackAttempted','rollbackSucceeded','rendererApplied','transitionPolicyApplied','shadowResourcesApplied','volumetricResolutionApplied','cloudSamplesApplied','cloudShadowSamplesApplied','lightShaftSamplesApplied','vegetationDensityApplied','textureBudgetApplied','meshBudgetApplied','gpu_observed','gpu_resourcesTrue','gpu_telemetryTrue','gpu_rendererAdapterIdentityValid','gpu_adapterIdentityTrue','gpu_validationTrue','gpu_timingRecorded','gpu_timingSerialAligned','gpu_productionFrameBudgetApplied','gpu_frameBudgetTrue','gpu_fresh','gpu_framePassed','gpu_committed');$strings=@('requestedFingerprint','normalizedFingerprint','actualFingerprint')
  $numbers+=@('renderer_completedFrameSerial','renderer_totalNanoseconds','renderer_scalarSerial','renderer_lastVolumetricAtmosphereGpuNanoseconds','renderer_lastHzbPrepassGpuNanoseconds','renderer_lastHzbBuildGpuNanoseconds','renderer_lastHzbCullGpuNanoseconds','renderer_lastHzbColorGpuNanoseconds','renderer_shadow_serial','renderer_shadow_beginTick','renderer_shadow_endTick','renderer_shadow_frequency','renderer_shadow_nanoseconds','renderer_captureCopy_serial','renderer_captureCopy_beginTick','renderer_captureCopy_endTick','renderer_captureCopy_frequency','renderer_captureCopy_nanoseconds');$booleans+=@('renderer_scalarAlignedValid','renderer_shadow_recorded','renderer_shadow_valid','renderer_shadow_alignedValid','renderer_captureCopy_recorded','renderer_captureCopy_valid','renderer_captureCopy_alignedValid')
  $expected=@('schemaVersion')+$numbers+$booleans+$strings
  if($null-eq$Value-or@($Value.PSObject.Properties).Count-ne$expected.Count-or$Value.schemaVersion-ne2){throw 'Ultra diagnostic schema missing or invalid'}
  foreach($name in $expected){if($null-eq$Value.PSObject.Properties[$name]){throw "Ultra diagnostic field missing: $name"}}
  foreach($name in $numbers){$v=$Value.$name;if($v-is[string]-or$v-is[bool]-or$null-eq$v){throw "Ultra diagnostic integer type invalid: $name"};try{$n=[decimal]$v}catch{throw "Ultra diagnostic integer invalid: $name"};if($n-ne[decimal]::Truncate($n)-or$n-gt[decimal][uint64]::MaxValue-or($n-lt0-and$name-notin@('gpu_rendererAdapterLuidHigh','gpu_telemetryAdapterLuidHigh'))){throw "Ultra diagnostic integer range invalid: $name"}}
  foreach($name in $booleans){if($Value.$name-isnot[bool]){throw "Ultra diagnostic boolean invalid: $name"}}
  foreach($name in $strings){if($Value.$name-isnot[string]-or$Value.$name-notmatch'^(0|[1-9][0-9]{0,19})$'){throw "Ultra diagnostic uint64 text invalid: $name"};$null=[uint64]::Parse($Value.$name)}
  if($Value.gpu_timingProvenance-lt0-or$Value.gpu_timingProvenance-gt2){throw 'Ultra timing provenance invalid'}
  if($Value.gpu_timingRecorded-ne($Value.gpu_lastGpuFrameNanoseconds-gt0-and$Value.renderer_totalNanoseconds-eq$Value.gpu_lastGpuFrameNanoseconds)){throw 'Ultra timing recorded evidence mismatch'}
  if($Value.gpu_timingSerialAligned-ne($Value.gpu_completedFrameSerial-gt0-and$Value.renderer_scalarSerial-eq$Value.gpu_completedFrameSerial)){throw 'Ultra timing snapshot serial alignment mismatch'}
  if($Value.gpu_productionFrameBudgetApplied-ne($Value.gpu_timingProvenance-eq1)){throw 'Ultra production timing provenance mismatch'}
  if($Value.gpu_timingProvenance-eq2-and$Value.gpu_productionFrameBudgetApplied){throw 'GBV timing cannot be production performance evidence'}
  $completed=$Value.renderer_completedFrameSerial
  if($Value.renderer_scalarAlignedValid-ne($completed-gt0-and$Value.renderer_scalarSerial-eq$completed)){throw 'Ultra scalar timing serial alignment mismatch'}
  foreach($prefix in @('renderer_shadow','renderer_captureCopy')){
    $serial=$Value."${prefix}_serial";$valid=$Value."${prefix}_valid";$recorded=$Value."${prefix}_recorded";$ns=$Value."${prefix}_nanoseconds"
    if($Value."${prefix}_alignedValid"-ne($valid-and$serial-eq$completed)){throw 'Ultra pass timing serial alignment mismatch'}
    if($valid-and($serial-eq0-or$Value."${prefix}_frequency"-eq0-or$Value."${prefix}_endTick"-lt$Value."${prefix}_beginTick"-or(-not$recorded-and$ns-ne0))){throw 'Ultra raw pass timing validity contradiction'}
  }
  return $true
}

function Get-GameplayTemporalTokenCompatibility($Raw,$Effective) {
  if($null-eq$Raw-or$null-eq$Effective){return $null-eq$Raw-and$null-eq$Effective}
  return $Raw.class-ceq'static'-and$Effective.class-ceq'static'-and-not$Raw.opacityAffected-and-not$Effective.opacityAffected-and$Raw.high-ceq$Effective.high-and$Raw.low-ceq$Effective.low-and$Raw.generation-ceq$Effective.generation
}

function Read-GameplayTemporalTokenPair([string]$Directory,[string]$Stem,[int]$Width,[int]$Height) {
  $character=Get-Content -Raw -LiteralPath (Join-Path $Directory 'gameplay-character-capture-receipt.json')|ConvertFrom-Json
  $sequence=Get-Content -Raw -LiteralPath (Join-Path $Directory 'gameplay-readback-sequence-receipt.json')|ConvertFrom-Json
  $c=@($character.captures|Where-Object name -CEQ "$Stem.png");$s=@($sequence.captures|Where-Object name -CEQ "$Stem.png")
  if($c.Count-ne1-or$s.Count-ne1-or$character.runId-cne$sequence.runId-or$s[0].complete-ne1){throw 'Temporal token receipt identity mismatch'}
  $planes=@();$tables=@();$metadataHashes=@();$planeHashes=@();$keys=[Collections.Generic.Dictionary[string,int]]::new();$nextKey=1
  for($phase=0;$phase-lt2;++$phase){
    $suffix=if($phase-eq0){'-raw'}else{''};$name="$Stem$suffix-tokens.json";$snapshot=Get-GameplayEvidenceFileSnapshot (Join-Path $Directory $name)
    $sha=if($phase-eq0){$c[0].rawTokenMetadataSha256}else{$c[0].effectiveTokenMetadataSha256}
    if($sha-notmatch'^[0-9a-f]{64}$'-or$snapshot.sha256-cne$sha){throw 'Temporal token metadata SHA mismatch'}
    $m=[Text.Encoding]::UTF8.GetString($snapshot.bytes)|ConvertFrom-Json;$provenance=$s[0].phases[$phase]
    if($m.source-cne'GameplayLosslessTokens'-or$m.version-ne1-or$m.runId-cne$character.runId-or$m.width-ne$Width-or$m.height-ne$Height-or$m.phase-ne$phase-or$m.frameSerial-cne[string]$provenance.frameSerial-or$m.tokenGeneration-cne[string]$provenance.tokenGeneration-or$m.plane-cne"$Stem$suffix-tokens.bin"){throw 'Temporal token frame/size/name provenance mismatch'}
    $plane=Get-GameplayEvidenceFileSnapshot (Join-Path $Directory $m.plane)
    if($plane.sha256-cne$m.planeSha256-or$plane.length-ne([int64]$Width*$Height*4)){throw 'Temporal token plane size/SHA mismatch'}
    $metadataHashes+=$snapshot.sha256;$planeHashes+=$plane.sha256
    $table=[int[]]::new(@($m.identities).Count+1);$seen=[Collections.Generic.HashSet[string]]::new();$token=0
    foreach($identity in $m.identities){
      ++$token
      foreach($field in @('high','low','generation')){if($identity.$field-isnot[string]-or$identity.$field-notmatch'^[1-9][0-9]{0,19}$'){throw 'Temporal token identity encoding invalid'}; $null=[uint64]::Parse($identity.$field)}
      if($identity.class-cnotin@('static','dynamic')-or$identity.opacityAffected-isnot[bool]){throw 'Temporal token class invalid'}
      $key="$($identity.high)/$($identity.low)/$($identity.generation)";if(-not$seen.Add($key)){throw 'Temporal token duplicate identity'}
      if(-not$keys.ContainsKey($key)){$keys.Add($key,$nextKey);++$nextKey}
      $table[$token]=if(Get-GameplayTemporalTokenCompatibility $identity $identity){$keys[$key]}else{-1}
    }
    $tokens=[uint32[]]::new($Width*$Height);[Buffer]::BlockCopy([byte[]]$plane.bytes,0,$tokens,0,$plane.bytes.Length)
    $planes+=,$tokens;$tables+=,$table
  }
  $compatible=[bool[]]::new($Width*$Height);$identityKeys=[int[]]::new($Width*$Height)
  for($i=0;$i-lt$compatible.Length;++$i){$a=$planes[0][$i];$b=$planes[1][$i];if($a-ge$tables[0].Length-or$b-ge$tables[1].Length){throw 'Temporal pixel token out of range'};$ka=$tables[0][$a];$kb=$tables[1][$b];$compatible[$i]=$ka-ge0-and$ka-eq$kb;$identityKeys[$i]=$ka}
  return [pscustomobject]@{compatible=$compatible;keys=$identityKeys;metadataSha256=$metadataHashes;planeSha256=$planeHashes}
}

function Get-GameplayTemporalTokenStableMetadataSha([string]$Path) {
  $snapshot=Get-GameplayEvidenceFileSnapshot $Path;$m=[Text.Encoding]::UTF8.GetString($snapshot.bytes)|ConvertFrom-Json
  # 僅排除每輪時間身份；完整物件身份、分類、opacity 與平面雜湊仍逐輪一致。 (canonical).
  $stable=[ordered]@{source=$m.source;version=$m.version;width=$m.width;height=$m.height;phase=$m.phase;plane=$m.plane;planeSha256=$m.planeSha256;identities=$m.identities}
  return Get-GameplayEvidenceTextSha256Hex ($stable|ConvertTo-Json -Depth 8 -Compress)
}

if(-not('AevryndGameplayTemporalGhostKernel' -as [type])){
  Add-Type -TypeDefinition @'
using System;

public sealed class AevryndGameplayTemporalGhostKernelResult
{
    public byte[] DifferenceMap;
    public bool[] EligibleMap;
    public bool[] RingMap;
    public int[] Histogram;
    public int[] RingHistogram;
    public int[] TilePixels;
    public int[] TileStablePixels;
    public long[] TileEnergy;
    public long[] TileDifferenceSum;
    public long[] TileDifferenceSquaredSum;
    public int[] TileHistogram;
    public long[] TileSignedSum;
    public int[] TileSignedHistogram;
    public long AbsoluteChannelEnergy;
    public long RingChannelEnergy;
    public int Eligible;
    public int StablePixels;
    public int RingPixels;
    public int Excluded;
    public int MaximumDifference;
    public int RingMaximum;
}

public static class AevryndGameplayTemporalGhostKernel
{
    public static bool[] BuildSemanticEmptyMap(
        byte[] rawObjectId, byte[] effectiveObjectId,
        int imageWidth, int x, int y, int width, int height)
    {
        var semanticEmpty = new bool[checked(width * height)];
        for (int row = 0; row < height; ++row) {
            int sourceIndex = (y + row) * imageWidth + x;
            int mapIndex = row * width;
            for (int column = 0; column < width; ++column) {
                int offset = (sourceIndex + column) * 4;
                semanticEmpty[mapIndex + column] =
                    rawObjectId[offset] <= 8 && rawObjectId[offset + 1] <= 8 &&
                    rawObjectId[offset + 2] <= 8 &&
                    effectiveObjectId[offset] <= 8 && effectiveObjectId[offset + 1] <= 8 &&
                    effectiveObjectId[offset + 2] <= 8;
            }
        }
        return semanticEmpty;
    }

    public static AevryndGameplayTemporalGhostKernelResult MeasureWithoutTokens(
        byte[] rawBeauty, byte[] effectiveBeauty, bool[] semanticEmpty,
        int imageWidth, int x, int y, int width, int height)
    {
        return Measure(rawBeauty, effectiveBeauty, semanticEmpty, new int[0], false, imageWidth, x, y, width, height);
    }

    public static AevryndGameplayTemporalGhostKernelResult MeasureWithoutTokensFromObjectIds(
        byte[] rawBeauty, byte[] effectiveBeauty, byte[] rawObjectId, byte[] effectiveObjectId,
        int imageWidth, int x, int y, int width, int height)
    {
        return MeasureWithoutTokens(
            rawBeauty, effectiveBeauty,
            BuildSemanticEmptyMap(rawObjectId, effectiveObjectId, imageWidth, x, y, width, height),
            imageWidth, x, y, width, height);
    }

    public static AevryndGameplayTemporalGhostKernelResult MeasureWithTokens(
        byte[] rawBeauty, byte[] effectiveBeauty, bool[] semanticEmpty, int[] tokenKeys,
        int imageWidth, int x, int y, int width, int height)
    {
        return Measure(rawBeauty, effectiveBeauty, semanticEmpty, tokenKeys, true, imageWidth, x, y, width, height);
    }

    public static AevryndGameplayTemporalGhostKernelResult Measure(
        byte[] rawBeauty, byte[] effectiveBeauty, bool[] semanticEmpty,
        int[] tokenKeys, bool useTokenKeys, int imageWidth, int x, int y, int width, int height)
    {
        const int tileColumns = 8;
        const int tileRows = 4;
        const int tileCount = tileColumns * tileRows;
        int pixelCount = checked(width * height);
        var result = new AevryndGameplayTemporalGhostKernelResult {
            DifferenceMap = new byte[pixelCount], EligibleMap = new bool[pixelCount],
            RingMap = new bool[pixelCount], Histogram = new int[256],
            RingHistogram = new int[256], TilePixels = new int[tileCount],
            TileStablePixels = new int[tileCount], TileEnergy = new long[tileCount],
            TileDifferenceSum = new long[tileCount], TileDifferenceSquaredSum = new long[tileCount],
            TileHistogram = new int[tileCount * 256], TileSignedSum = new long[tileCount * 3],
            TileSignedHistogram = new int[tileCount * 3 * 511]
        };
        for (int row = 0; row < height; ++row) {
            int py = y + row;
            int mapRow = row * width;
            for (int column = 0; column < width; ++column) {
                int mapIndex = mapRow + column;
                if (!semanticEmpty[mapIndex]) { ++result.Excluded; continue; }
                ++result.Eligible;
                bool adjacentObject = false;
                for (int oy = -1; oy <= 1 && !adjacentObject; ++oy) {
                    int neighborRow = row + oy;
                    if (neighborRow < 0 || neighborRow >= height) continue;
                    int neighborMapRow = neighborRow * width;
                    for (int ox = -1; ox <= 1; ++ox) {
                        int neighborColumn = column + ox;
                        if (neighborColumn < 0 || neighborColumn >= width) continue;
                        if (!semanticEmpty[neighborMapRow + neighborColumn]) { adjacentObject = true; break; }
                    }
                }
                int px = x + column;
                int sourceIndex = py * imageWidth + px;
                int rawOffset = sourceIndex * 4;
                int difference = 0;
                int channelDifference = 0;
                if (!adjacentObject) {
                    ++result.StablePixels;
                    for (int channel = 0; channel < 3; ++channel) {
                        int delta = Math.Abs((int)rawBeauty[rawOffset + channel] - effectiveBeauty[rawOffset + channel]);
                        channelDifference += delta;
                        difference = Math.Max(difference, delta);
                    }
                    result.AbsoluteChannelEnergy += channelDifference;
                    ++result.Histogram[difference];
                } else {
                    ++result.RingPixels;
                    result.RingMap[mapIndex] = true;
                    int forward = 255, backward = 255;
                    for (int oy = -1; oy <= 1; ++oy) {
                        int neighborRow = row + oy;
                        if (neighborRow < 0 || neighborRow >= height) continue;
                        int sy = y + neighborRow;
                        int neighborMapRow = neighborRow * width;
                        for (int ox = -1; ox <= 1; ++ox) {
                            int neighborColumn = column + ox;
                            if (neighborColumn < 0 || neighborColumn >= width) continue;
                            int neighborMapIndex = neighborMapRow + neighborColumn;
                            if (!semanticEmpty[neighborMapIndex]) continue;
                            int neighborSourceIndex = sy * imageWidth + x + neighborColumn;
                            if (useTokenKeys && tokenKeys[neighborSourceIndex] != tokenKeys[sourceIndex]) continue;
                            int neighbor = neighborSourceIndex * 4;
                            int fd = 0, bd = 0;
                            for (int channel = 0; channel < 3; ++channel) {
                                fd = Math.Max(fd, Math.Abs((int)rawBeauty[rawOffset + channel] - effectiveBeauty[neighbor + channel]));
                                bd = Math.Max(bd, Math.Abs((int)effectiveBeauty[rawOffset + channel] - rawBeauty[neighbor + channel]));
                            }
                            forward = Math.Min(forward, fd);
                            backward = Math.Min(backward, bd);
                        }
                    }
                    difference = Math.Max(forward, backward);
                    channelDifference = 3 * difference;
                    result.RingChannelEnergy += channelDifference;
                    ++result.RingHistogram[difference];
                    result.RingMaximum = Math.Max(result.RingMaximum, difference);
                }
                result.EligibleMap[mapIndex] = true;
                result.DifferenceMap[mapIndex] = (byte)difference;
                result.MaximumDifference = Math.Max(result.MaximumDifference, difference);
                int tile = ((row * tileRows) / height) * tileColumns + ((column * tileColumns) / width);
                ++result.TilePixels[tile];
                if (!adjacentObject) {
                    ++result.TileStablePixels[tile];
                    result.TileEnergy[tile] += channelDifference;
                    result.TileDifferenceSum[tile] += difference;
                    result.TileDifferenceSquaredSum[tile] += difference * difference;
                    ++result.TileHistogram[tile * 256 + difference];
                    for (int channel = 0; channel < 3; ++channel) {
                        int signed = (int)effectiveBeauty[rawOffset + channel] - rawBeauty[rawOffset + channel];
                        result.TileSignedSum[tile * 3 + channel] += signed;
                        ++result.TileSignedHistogram[((tile * 3 + channel) * 511) + signed + 255];
                    }
                }
            }
        }
        return result;
    }
}
'@
}

function Get-GameplayTemporalGhostEvidence(
  [string]$RawBeautyPath,[string]$EffectiveBeautyPath,
  [string]$RawObjectIdPath,[string]$EffectiveObjectIdPath,
  [string]$RawDepthPath,[string]$EffectiveDepthPath,
  [int]$X,[int]$Y,[int]$Width,[int]$Height,[hashtable]$ExpectedSha256=$null) {
  Add-Type -AssemblyName System.Drawing
  $paths=[ordered]@{rawBeauty=$RawBeautyPath;effectiveBeauty=$EffectiveBeautyPath;rawObjectId=$RawObjectIdPath;effectiveObjectId=$EffectiveObjectIdPath;rawDepth=$RawDepthPath;effectiveDepth=$EffectiveDepthPath}
  $snapshots=[ordered]@{};$streams=@();$bitmaps=@()
  try {
    foreach($key in $paths.Keys){
      if([string]::IsNullOrWhiteSpace($paths[$key])-or-not(Test-Path -LiteralPath $paths[$key] -PathType Leaf)){throw "Temporal ghost artifact was missing: $key"}
      $snapshot=Get-GameplayEvidenceFileSnapshot $paths[$key]
      if($snapshot.length-lt1-or$snapshot.sha256-notmatch'^[0-9a-f]{64}$'){throw "Temporal ghost artifact SHA was empty: $key"}
      if($ExpectedSha256){if(-not$ExpectedSha256.ContainsKey($key)-or$ExpectedSha256[$key]-notmatch'^[0-9a-f]{64}$'-or$ExpectedSha256[$key]-cne$snapshot.sha256){throw "Temporal ghost artifact SHA mismatch: $key"}}
      $snapshots[$key]=$snapshot
    }
    foreach($key in $paths.Keys){$stream=[IO.MemoryStream]::new([byte[]]$snapshots[$key].bytes,$false);$streams+=$stream;$bitmaps+=[Drawing.Bitmap]::FromStream($stream)}
    if($bitmaps.Count-ne6-or@($bitmaps|Where-Object{$null-eq$_}).Count){throw 'Temporal ghost bitmap decode was incomplete'}
    $rawBeauty=$bitmaps[0]
    $effectiveBeauty=$bitmaps[1]
    $rawObjectId=$bitmaps[2]
    $effectiveObjectId=$bitmaps[3]
    $rawDepth=$bitmaps[4]
    $effectiveDepth=$bitmaps[5]
    $tokenPair=$null
    if($ExpectedSha256){$stem=[IO.Path]::GetFileName($RawObjectIdPath)-replace '-raw-object-id.png$','';$tokenPair=Read-GameplayTemporalTokenPair ([IO.Path]::GetDirectoryName($RawObjectIdPath)) $stem $rawBeauty.Width $rawBeauty.Height}
    foreach($bitmap in $bitmaps){if($bitmap.Width-ne$rawBeauty.Width-or$bitmap.Height-ne$rawBeauty.Height){throw 'Temporal ghost artifact dimensions did not match'}}
    if($X-lt0-or$Y-lt0-or$Width-lt16-or$Height-lt16-or$X+$Width-gt$rawBeauty.Width-or$Y+$Height-gt$rawBeauty.Height){throw 'Temporal ghost world-only bounds were invalid'}

    $bgraBuffers=@($(Get-GameplayEvidenceBgra32 $rawBeauty);$(Get-GameplayEvidenceBgra32 $effectiveBeauty);$(Get-GameplayEvidenceBgra32 $rawObjectId);$(Get-GameplayEvidenceBgra32 $effectiveObjectId))
    # 正式擷取以無損身份判斷靜態背景；未附來源證明的舊合成測試只接受空白 OID。
    if($bgraBuffers.Count-ne4){throw 'Temporal ghost BGRA buffer collection was incomplete'}
    $expectedBgraLength=$rawBeauty.Width*$rawBeauty.Height*4
    if($bgraBuffers[0].bytes.Count-ne$expectedBgraLength-or$bgraBuffers[1].bytes.Count-ne$expectedBgraLength-or$bgraBuffers[2].bytes.Count-ne$expectedBgraLength-or$bgraBuffers[3].bytes.Count-ne$expectedBgraLength){throw 'Temporal ghost BGRA conversion was incomplete'}
    $imageWidth=$rawBeauty.Width
    $pixelCount=$Width*$Height;$tileColumns=8;$tileRows=4;$tileCount=$tileColumns*$tileRows
    # 每個 3x3 查詢的 OID／token 合格性與格狀歸屬皆固定；逐像素只解碼一次，避免在兩次鄰域掃描重複處理屬性與位元組陣列。
    # 單一已編譯 kernel 逐像素產生所有 map、ring、histogram、tile 與 signed 統計；後段 gate 保持原樣。
    $kernel=[AevryndGameplayTemporalGhostKernelResult]::new()
    if($tokenPair){$kernel=[AevryndGameplayTemporalGhostKernel]::MeasureWithTokens($bgraBuffers[0].bytes,$bgraBuffers[1].bytes,$tokenPair.compatible,[int[]]($tokenPair.keys),$imageWidth,$X,$Y,$Width,$Height)}else{$kernel=[AevryndGameplayTemporalGhostKernel]::MeasureWithoutTokensFromObjectIds($bgraBuffers[0].bytes,$bgraBuffers[1].bytes,$bgraBuffers[2].bytes,$bgraBuffers[3].bytes,$imageWidth,$X,$Y,$Width,$Height)}
    if($null-eq$kernel){throw 'Temporal ghost compiled kernel returned no result'}
    $differenceMap=$kernel.DifferenceMap;$eligibleMap=$kernel.EligibleMap;$ringMap=$kernel.RingMap;$histogram=$kernel.Histogram;$ringHistogram=$kernel.RingHistogram;$tilePixels=$kernel.TilePixels;$tileStablePixels=$kernel.TileStablePixels;$tileEnergy=$kernel.TileEnergy;$tileDifferenceSum=$kernel.TileDifferenceSum;$tileDifferenceSquaredSum=$kernel.TileDifferenceSquaredSum;$tileHistogram=$kernel.TileHistogram;$tileSignedSum=$kernel.TileSignedSum;$tileSignedHistogram=$kernel.TileSignedHistogram
    [int64]$absoluteChannelEnergy=$kernel.AbsoluteChannelEnergy;[int64]$ringChannelEnergy=$kernel.RingChannelEnergy;$eligible=$kernel.Eligible;$stablePixels=$kernel.StablePixels;$ringPixels=$kernel.RingPixels;$excluded=$kernel.Excluded;$maximumDifference=$kernel.MaximumDifference;$ringMaximum=$kernel.RingMaximum
    $minimumEligible=[Math]::Min($pixelCount,[Math]::Max(129,[int][Math]::Ceiling($pixelCount*.05)));if($eligible-lt$minimumEligible-or$stablePixels-lt129-or($excluded-gt0-and$ringPixels-lt1)-or$stablePixels+$ringPixels-ne$eligible){throw "Temporal ghost full-frame eligible coverage was insufficient: eligible=$eligible stable=$stablePixels ring=$ringPixels required=$minimumEligible"}
    $getP99={param([int[]]$Values,[int]$Start,[int]$Count)$rank=[int][Math]::Ceiling($Count*.99);$running=0;for($value=0;$value-lt256;++$value){$running+=$Values[$Start+$value];if($running-ge$rank){return $value}};return 255}
    $p99=& $getP99 $histogram 0 $stablePixels;$ringP99=& $getP99 $ringHistogram 0 $ringPixels
    $tileMinimumStablePixels=129;$tileFallbackCoveragePixels=256;$tileMeanThreshold=2.25;$tileP99Threshold=8;$tileSignedBiasThreshold=.75;$tileMedianBiasThreshold=1;$tileCoherenceThreshold=.75
    $tiles=@();$eligibleTiles=0;$worstTileMean=0.0;$worstTileP99=0;$worstTileMaximum=0;$worstTileSignedBias=0.0;$worstTileMedianBias=0.0;$worstTileCoherence=0.0
    for($tile=0;$tile-lt$tileCount;++$tile){
      $coverageCount=$tilePixels[$tile];$count=$tileStablePixels[$tile];if($coverageCount){++$eligibleTiles}
      $tileMean=0.0;$tileP99=0;$tileMax=0;$signedMeans=@(0.0,0.0,0.0);$signedMedians=@(0.0,0.0,0.0);$signedBias=0.0;$medianBias=0.0;$coherence=0.0;$tileDifferenceMeanValue=0.0;$tileDifferenceSecondMomentValue=0.0;$tileVarianceValue=0.0
      if($count){
        $tileMean=[double]$tileEnergy[$tile]/(3.0*$count);$tileP99=& $getP99 $tileHistogram ($tile*256) $count
        for($value=255;$value-ge0;--$value){if($tileHistogram[$tile*256+$value]){$tileMax=$value;break}}
        $signedMeans=@();$signedMedians=@()
        for($channel=0;$channel-lt3;++$channel){
          $signedMeans+=[double]$tileSignedSum[$tile*3+$channel]/$count
          $lowerRank=[int][Math]::Floor(($count+1)/2);$upperRank=[int][Math]::Ceiling(($count+1)/2);$running=0;$lowerMedian=$null;$upperMedian=$null
          for($value=-255;$value-le255;++$value){$running+=$tileSignedHistogram[(($tile*3+$channel)*511)+$value+255];if($null-eq$lowerMedian-and$running-ge$lowerRank){$lowerMedian=$value};if($running-ge$upperRank){$upperMedian=$value;break}}
          $signedMedians+=([double]($lowerMedian+$upperMedian)/2.0)
        }
        $signedBias=[double]((@($signedMeans|ForEach-Object{[Math]::Abs($_)})|Measure-Object -Average).Average);$medianBias=[double]((@($signedMedians|ForEach-Object{[Math]::Abs($_)})|Measure-Object -Maximum).Maximum);$coherence=if($tileMean-gt.000001){$signedBias/$tileMean}else{0.0}
        $tileDifferenceMeanValue=[double]$tileDifferenceSum[$tile]/[double]$count;$tileDifferenceSecondMomentValue=[double]$tileDifferenceSquaredSum[$tile]/[double]$count;$tileVarianceValue=[double]($tileDifferenceSecondMomentValue-$tileDifferenceMeanValue*$tileDifferenceMeanValue);if($tileVarianceValue-lt0){$tileVarianceValue=0.0}
      }
      $tileGate=$count-ge$tileMinimumStablePixels;$fallbackFail=$count-lt$tileMinimumStablePixels-and$coverageCount-ge$tileFallbackCoveragePixels
      $tileMetricPass=$tileMean-le$tileMeanThreshold-and$tileP99-le$tileP99Threshold-and$signedBias-le$tileSignedBiasThreshold-and$medianBias-le$tileMedianBiasThreshold-and$coherence-le$tileCoherenceThreshold
      $tilePass=if($tileGate){$tileMetricPass}elseif($fallbackFail){$false}else{$true}
      if($tileGate){$worstTileMean=[Math]::Max($worstTileMean,$tileMean);$worstTileP99=[Math]::Max($worstTileP99,$tileP99);$worstTileSignedBias=[Math]::Max($worstTileSignedBias,$signedBias);$worstTileMedianBias=[Math]::Max($worstTileMedianBias,$medianBias);$worstTileCoherence=[Math]::Max($worstTileCoherence,$coherence)}
      $worstTileMaximum=[Math]::Max($worstTileMaximum,$tileMax)
      $tiles+=[pscustomobject]@{index=$tile;x=$tile%$tileColumns;y=[int][Math]::Floor($tile/$tileColumns);eligiblePixels=$coverageCount;stablePixels=$count;ringPixels=$coverageCount-$count;gateApplied=if($tileGate){1}else{0};coverageFallbackFailed=if($fallbackFail){1}else{0};meanAbsoluteChannelDifference=$tileMean;p99MaximumChannelDifference=$tileP99;maximumChannelDifference=$tileMax;signedMeanB=$signedMeans[0];signedMeanG=$signedMeans[1];signedMeanR=$signedMeans[2];signedMedianB=$signedMedians[0];signedMedianG=$signedMedians[1];signedMedianR=$signedMedians[2];signedBias=$signedBias;medianBias=$medianBias;absoluteDifferenceMean=$tileDifferenceMeanValue;absoluteDifferenceSecondMoment=$tileDifferenceSecondMomentValue;absoluteDifferenceVariance=$tileVarianceValue;coherence=$coherence;pass=if($tilePass){1}else{0}}
    }
    if($eligibleTiles-lt4){throw "Temporal ghost full-frame tile coverage was insufficient: eligibleTiles=$eligibleTiles"}
    $getLevel={param([int]$Threshold,[bool]$RingOnly)$count=0;[int64]$energy=0;$mask=[bool[]]::new([int]$pixelCount);for($index=0;$index-lt$pixelCount;++$index){if(-not$eligibleMap[$index]-or$ringMap[$index]-ne$RingOnly-or$differenceMap[$index]-le$Threshold){continue};$mask[$index]=$true;++$count;$energy+=$differenceMap[$index]-$Threshold};$maximumComponent=0;$queue=[Collections.Generic.Queue[int]]::new();for($index=0;$index-lt$pixelCount;++$index){if(-not$mask[$index]){continue};$mask[$index]=$false;$queue.Enqueue($index);$component=0;while($queue.Count){$current=$queue.Dequeue();++$component;$cx=$current%$Width;$cy=[Math]::Floor($current/$Width);foreach($next in @($(if($cx){$current-1}else{-1}),$(if($cx+1-lt$Width){$current+1}else{-1}),$(if($cy){$current-$Width}else{-1}),$(if($cy+1-lt$Height){$current+$Width}else{-1}))){if($next-ge0-and$mask[$next]){$mask[$next]=$false;$queue.Enqueue($next)}}};$maximumComponent=[Math]::Max($maximumComponent,$component)};$population=if($RingOnly){$ringPixels}else{$stablePixels};return [pscustomobject]@{threshold=$Threshold;pixels=$count;fraction=if($population){[double]$count/$population}else{0.0};maximumConnectedPixels=$maximumComponent;normalizedExcessEnergy=if($population){[double]$energy/$population}else{0.0}}}
    $stableLevels=@((& $getLevel 8 $false),(& $getLevel 12 $false),(& $getLevel 16 $false));$ringLevels=@((& $getLevel 8 $true),(& $getLevel 12 $true),(& $getLevel 16 $true));$meanAbs=[double]$absoluteChannelEnergy/(3.0*$stablePixels);$ringMean=if($ringPixels){[double]$ringChannelEnergy/(3.0*$ringPixels)}else{0.0}
    # stable與ring以面積、cluster、energy共同判定，孤立量化差不主導結果。 (multi-level contract).
    $stablePass=$meanAbs-le1.25-and$p99-le4-and($stableLevels[0].pixels-le2-or$stableLevels[0].fraction-le.002)-and$stableLevels[0].maximumConnectedPixels-le12-and$stableLevels[0].normalizedExcessEnergy-le.02-and($stableLevels[1].pixels-le2-or$stableLevels[1].fraction-le.00025)-and$stableLevels[1].maximumConnectedPixels-le4-and$stableLevels[1].normalizedExcessEnergy-le.005-and$stableLevels[2].fraction-le.00002-and$stableLevels[2].maximumConnectedPixels-le2-and$stableLevels[2].normalizedExcessEnergy-le.0005
    $ringPass=$ringMean-le2.0-and$ringP99-le8-and($ringLevels[0].pixels-le4-or$ringLevels[0].fraction-le.025)-and$ringLevels[0].maximumConnectedPixels-le12-and$ringLevels[0].normalizedExcessEnergy-le.08-and($ringLevels[1].pixels-le2-or$ringLevels[1].fraction-le.004)-and$ringLevels[1].maximumConnectedPixels-le6-and$ringLevels[1].normalizedExcessEnergy-le.02-and(($ringLevels[2].pixels-le1-and$ringLevels[2].maximumConnectedPixels-le1)-or($ringLevels[2].fraction-le.001-and$ringLevels[2].maximumConnectedPixels-le3-and$ringLevels[2].normalizedExcessEnergy-le.005))
    $tilePass=@($tiles|Where-Object { $_.pass -ne 1 }).Count -eq 0;$pass=$stablePass-and$ringPass-and$tilePass
    $artifactSha=[ordered]@{};$artifactName=[ordered]@{};foreach($key in $paths.Keys){$artifactSha[$key]=$snapshots[$key].sha256;$artifactName[$key]=[IO.Path]::GetFileName($paths[$key])}
    $coverageText="$X,$Y,$Width,$Height|$eligible,$stablePixels,$ringPixels,$excluded|"+(@($tiles|ForEach-Object{"$($_.index):$($_.eligiblePixels)"})-join',')
    $tokenMetadataSha256=if($tokenPair){@($tokenPair.metadataSha256)}else{@()};$tokenPlaneSha256=if($tokenPair){@($tokenPair.planeSha256)}else{@()}
    return [pscustomobject]@{pass=if($pass){1}else{0};contract='raw-effective-full-frame-v3';losslessIdentityVerified=($null-ne$tokenPair);tokenMetadataSha256=$tokenMetadataSha256;tokenPlaneSha256=$tokenPlaneSha256;stablePass=if($stablePass){1}else{0};ringPass=if($ringPass){1}else{0};tilePass=if($tilePass){1}else{0};boundsX=$X;boundsY=$Y;boundsWidth=$Width;boundsHeight=$Height;eligiblePixels=$eligible;coveredPixels=$stablePixels+$ringPixels;excludedPixels=$excluded;stableEmptyPixels=$stablePixels;objectAdjacentRingPixels=$ringPixels;eligibleTiles=$eligibleTiles;tileColumns=$tileColumns;tileRows=$tileRows;meanAbsoluteChannelDifference=$meanAbs;p99MaximumChannelDifference=$p99;maximumChannelDifference=$maximumDifference;ringMeanAbsoluteChannelDifference=$ringMean;ringP99MaximumChannelDifference=$ringP99;ringMaximumChannelDifference=$ringMaximum;stableLevels=$stableLevels;ringLevels=$ringLevels;worstTileMeanAbsoluteChannelDifference=$worstTileMean;worstTileP99MaximumChannelDifference=$worstTileP99;worstTileMaximumChannelDifference=$worstTileMaximum;worstTileSignedBias=$worstTileSignedBias;worstTileMedianBias=$worstTileMedianBias;worstTileCoherence=$worstTileCoherence;tiles=$tiles;meanThreshold=1.25;ringMeanThreshold=2.0;p99Threshold=4;ringP99Threshold=8;tileMinimumStablePixels=$tileMinimumStablePixels;tileFallbackCoveragePixels=$tileFallbackCoveragePixels;tileMeanThreshold=$tileMeanThreshold;tileP99Threshold=$tileP99Threshold;tileSignedBiasThreshold=$tileSignedBiasThreshold;tileMedianBiasThreshold=$tileMedianBiasThreshold;tileCoherenceThreshold=$tileCoherenceThreshold;artifactName=[pscustomobject]$artifactName;artifactSha256=[pscustomobject]$artifactSha;coverageSha256=Get-GameplayEvidenceTextSha256Hex $coverageText}
  } finally {foreach($bitmap in $bitmaps){if($bitmap){$bitmap.Dispose()}};foreach($stream in $streams){if($stream){$stream.Dispose()}}}
}

function Assert-GameplayTemporalGhostPairProvenance([string]$CaptureName,$CharacterCapture,$PhotorealCapture,$SequenceCapture,$Evidence,$ArtifactCapture) {
  if($CharacterCapture.PSObject.Properties.Name-contains'rawTokenMetadataSha256'){
    if(-not$Evidence.losslessIdentityVerified-or@($Evidence.tokenMetadataSha256).Count-ne2-or$Evidence.tokenMetadataSha256[0]-cne$CharacterCapture.rawTokenMetadataSha256-or$Evidence.tokenMetadataSha256[1]-cne$CharacterCapture.effectiveTokenMetadataSha256){throw 'Temporal lossless identity evidence provenance mismatch'}
  }
  if($CharacterCapture.name-ne$CaptureName-or$PhotorealCapture.name-ne$CaptureName-or$SequenceCapture.name-ne$CaptureName-or$SequenceCapture.complete-ne1-or@($SequenceCapture.phases).Count-lt2-or$Evidence.contract-ne'raw-effective-full-frame-v3'){throw "Temporal ghost pair identity failed: $CaptureName"}
  $raw=@($SequenceCapture.phases)[0];$effective=@($SequenceCapture.phases)[1]
  if($raw.index-ne0-or$effective.index-ne1-or$raw.frameSerial-lt1-or$effective.frameSerial-ne$raw.frameSerial+1-or$effective.tokenGeneration-ne$raw.tokenGeneration+1-or$raw.viewProjectionHash-eq0-or$raw.viewProjectionHash-ne$effective.viewProjectionHash-or$raw.cameraHash-ne$effective.cameraHash-or$raw.invariantHash-ne$effective.invariantHash-or$raw.jitterIndex-ne$effective.jitterIndex-or$raw.jitterX-ne$effective.jitterX-or$raw.jitterY-ne$effective.jitterY-or$raw.historyGeneration-ne$effective.historyGeneration-or$raw.historyIndex-ne$effective.historyIndex){throw "Temporal ghost raw/effective frame/VP/history provenance mismatch: $CaptureName"}
  if($PhotorealCapture.effectiveFrameSerial-ne$effective.frameSerial-or$PhotorealCapture.semanticFrameSerial-ne$effective.frameSerial){throw "Temporal ghost effective semantic frame mismatch: $CaptureName"}
  $diagnosticBeauty=([IO.Path]::GetFileNameWithoutExtension($CaptureName)+'-diagnostic.png')
  $effectiveBeauty=if($CharacterCapture.PSObject.Properties.Name -contains 'effectiveColor'){$CharacterCapture.effectiveColor}else{$diagnosticBeauty}
  $expectedNames=[ordered]@{rawBeauty=$CharacterCapture.rawColor;effectiveBeauty=$effectiveBeauty;rawObjectId=$CharacterCapture.rawObjectIdMask;effectiveObjectId=$CharacterCapture.objectIdMask;rawDepth=$CharacterCapture.rawDepth;effectiveDepth=$CharacterCapture.effectiveDepth}
  foreach($key in $expectedNames.Keys){$sha=$Evidence.artifactSha256.$key;$actualName=$Evidence.artifactName.$key;if([string]::IsNullOrWhiteSpace($expectedNames[$key])-or$actualName-cne$expectedNames[$key]-or$sha-notmatch'^[0-9a-f]{64}$'){throw "Temporal ghost artifact name/SHA provenance missing or mismatched: $CaptureName/$key"}}
  if($CharacterCapture.rawDepthFinalFileSha256-notmatch'^[0-9a-f]{64}$'-or$CharacterCapture.effectiveDepthFinalFileSha256-notmatch'^[0-9a-f]{64}$'-or$Evidence.artifactSha256.rawDepth-cne$CharacterCapture.rawDepthFinalFileSha256-or$Evidence.artifactSha256.effectiveDepth-cne$CharacterCapture.effectiveDepthFinalFileSha256){throw "Temporal ghost pair depth final-file SHA mismatch: $CaptureName"}
  if($ArtifactCapture.name-cne$CaptureName-or$ArtifactCapture.rawFrameSerial-ne$raw.frameSerial-or$ArtifactCapture.effectiveFrameSerial-ne$effective.frameSerial-or$ArtifactCapture.rawTokenGeneration-ne$raw.tokenGeneration-or$ArtifactCapture.effectiveTokenGeneration-ne$effective.tokenGeneration-or@($ArtifactCapture.artifacts).Count-ne6){throw "Temporal ghost receipt-owned frame/token provenance mismatch: $CaptureName"}
  foreach($key in $expectedNames.Keys){$owned=@($ArtifactCapture.artifacts|Where-Object key -eq $key);if($owned.Count-ne1-or$owned[0].name-cne$Evidence.artifactName.$key-or$owned[0].sha256-cne$Evidence.artifactSha256.$key-or[uint64]$owned[0].bytes-lt1){throw "Temporal ghost receipt-owned artifact mismatch: $CaptureName/$key"}}
  return $true
}

function Write-GameplayTemporalGhostArtifactReceipt([string]$Directory,[string]$RunId,$CharacterReceipt,$SequenceReceipt) {
  if($RunId-notmatch'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'){throw 'Temporal ghost artifact receipt runId was invalid'}
  $captures=@();foreach($stem in @('third-day','third-dusk')){$name="$stem.png";$character=@($CharacterReceipt.captures|Where-Object name -eq $name);$sequence=@($SequenceReceipt.captures|Where-Object name -eq $name);if($character.Count-ne1-or$sequence.Count-ne1-or@($sequence[0].phases).Count-lt2){throw "Temporal ghost artifact receipt source capture missing: $name"};$raw=$sequence[0].phases[0];$effective=$sequence[0].phases[1];$names=[ordered]@{rawBeauty=$character[0].rawColor;effectiveBeauty="$stem-diagnostic.png";rawObjectId=$character[0].rawObjectIdMask;effectiveObjectId=$character[0].objectIdMask;rawDepth=$character[0].rawDepth;effectiveDepth=$character[0].effectiveDepth};$artifacts=@();foreach($key in $names.Keys){$path=Join-Path $Directory $names[$key];$snapshot=Get-GameplayEvidenceFileSnapshot $path;$artifacts+=[ordered]@{key=$key;name=$names[$key];bytes=[uint64]$snapshot.length;sha256=$snapshot.sha256}};if($artifacts[4].sha256-cne$character[0].rawDepthFinalFileSha256-or$artifacts[5].sha256-cne$character[0].effectiveDepthFinalFileSha256){throw "Temporal ghost artifact receipt depth ownership mismatch: $name"};$captures+=[ordered]@{name=$name;rawFrameSerial=[uint64]$raw.frameSerial;effectiveFrameSerial=[uint64]$effective.frameSerial;rawTokenGeneration=[uint64]$raw.tokenGeneration;effectiveTokenGeneration=[uint64]$effective.tokenGeneration;artifacts=$artifacts}}
  $payload=[ordered]@{source='GameplayTemporalGhostArtifactReceipt';runId=$RunId;captures=$captures};$payloadJson=$payload|ConvertTo-Json -Depth 8 -Compress;$receipt=[ordered]@{source=$payload.source;runId=$payload.runId;captures=$payload.captures;payloadSha256=Get-GameplayEvidenceTextSha256Hex $payloadJson};$path=Join-Path $Directory 'gameplay-temporal-ghost-artifact-receipt.json';$temporary="$path.tmp";[IO.File]::WriteAllText($temporary,($receipt|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false));Move-Item -LiteralPath $temporary -Destination $path -Force
  $loaded=Get-Content -Raw -LiteralPath $path|ConvertFrom-Json;$verify=[ordered]@{source=$loaded.source;runId=$loaded.runId;captures=$loaded.captures};if($loaded.payloadSha256-cne(Get-GameplayEvidenceTextSha256Hex ($verify|ConvertTo-Json -Depth 8 -Compress))){throw 'Temporal ghost artifact receipt self hash mismatch'};return $loaded
}

function Get-GameplayTemporalGhostExpectedSha($ArtifactCapture) {$expected=@{};foreach($artifact in @($ArtifactCapture.artifacts)){if($expected.ContainsKey([string]$artifact.key)-or$artifact.sha256-notmatch'^[0-9a-f]{64}$'){throw "Temporal ghost receipt artifact key/SHA invalid: $($artifact.key)"};$expected[[string]$artifact.key]=[string]$artifact.sha256};if($expected.Count-ne6){throw 'Temporal ghost receipt artifact set was incomplete'};return $expected}

function Assert-GameplayTemporalGhostDepthProvenance([string]$DepthPath,[string]$CaptureName,[int]$PhaseIndex,$CharacterCapture,$PhotorealCapture,$SequenceCapture) {
  if($PhaseIndex-notin@(0,1)-or$CharacterCapture.name-ne$CaptureName-or$PhotorealCapture.name-ne$CaptureName-or$SequenceCapture.name-ne$CaptureName-or$CharacterCapture.depthCaptureSupported-ne1-or$CharacterCapture.depthGatePass-ne1-or$CharacterCapture.depthGeneration-lt1-or$SequenceCapture.complete-ne1-or@($SequenceCapture.phases).Count-lt2){throw "Temporal ghost typed depth provenance identity failed: $CaptureName"}
  $phase=@($SequenceCapture.phases)[$PhaseIndex];$raw=@($SequenceCapture.phases)[0];$effective=@($SequenceCapture.phases)[1]
  $prefix=if($PhaseIndex-eq0){'raw'}else{'effective'};$expectedName=$CharacterCapture."${prefix}Depth";$pixelSha=$CharacterCapture."${prefix}DepthPixelSha256";$legacyPixelSha=$CharacterCapture."${prefix}DepthSha256";$finalSha=$CharacterCapture."${prefix}DepthFinalFileSha256";$finalBytes=$CharacterCapture."${prefix}DepthFinalFileBytes";$snapshot=Get-GameplayEvidenceFileSnapshot $DepthPath
  if([IO.Path]::GetFileName($DepthPath)-cne$expectedName-or$pixelSha-notmatch'^[0-9a-f]{64}$'-or$legacyPixelSha-cne$pixelSha-or$finalSha-notmatch'^[0-9a-f]{64}$'-or$snapshot.sha256-cne$finalSha-or$snapshot.length-ne[uint64]$finalBytes-or$phase.index-ne$PhaseIndex-or$phase.frameSerial-lt1-or$phase.viewProjectionHash-eq0-or$raw.viewProjectionHash-ne$effective.viewProjectionHash-or$raw.cameraHash-ne$effective.cameraHash-or$raw.invariantHash-ne$effective.invariantHash-or$raw.jitterIndex-ne$effective.jitterIndex-or$raw.jitterX-ne$effective.jitterX-or$raw.jitterY-ne$effective.jitterY-or$raw.historyGeneration-ne$effective.historyGeneration-or$raw.historyIndex-ne$effective.historyIndex){throw "Temporal ghost typed depth pixel/final-file SHA/frame/VP provenance mismatch: $CaptureName phase=$PhaseIndex"}
  if($PhaseIndex-eq1-and($PhotorealCapture.effectiveFrameSerial-ne$phase.frameSerial-or$PhotorealCapture.semanticFrameSerial-ne$phase.frameSerial)){throw "Temporal ghost effective semantic/depth frame provenance mismatch: $CaptureName"}
  return $true
}

function Assert-GameplayDepthFileReceipts([string]$Directory,$CharacterReceipt) {
  foreach($capture in @($CharacterReceipt.captures)){
    foreach($prefix in @('raw','effective','reference')){
      $name=$capture."${prefix}Depth";$pixelSha=$capture."${prefix}DepthPixelSha256";$legacyPixelSha=$capture."${prefix}DepthSha256";$finalSha=$capture."${prefix}DepthFinalFileSha256";$finalBytes=$capture."${prefix}DepthFinalFileBytes"
      $required=$prefix-ne'reference'-or$capture.camera-eq'third'
      if(-not$required){if($finalSha-notmatch'^0{64}$'-or[uint64]$finalBytes-ne0){throw "Non-produced depth artifact carried final-file receipt: $($capture.name)/$prefix"};continue}
      if([string]::IsNullOrWhiteSpace($name)-or$pixelSha-notmatch'^[0-9a-f]{64}$'-or$legacyPixelSha-cne$pixelSha-or$finalSha-notmatch'^[0-9a-f]{64}$'-or$finalSha-match'^0{64}$'-or[uint64]$finalBytes-lt1){throw "Depth pixel/final-file receipt schema failed: $($capture.name)/$prefix"}
      $path=Join-Path $Directory $name;if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Depth final artifact missing: $($capture.name)/$prefix"};$snapshot=Get-GameplayEvidenceFileSnapshot $path
      if($snapshot.length-ne[uint64]$finalBytes-or$snapshot.sha256-cne$finalSha){throw "Depth final artifact was replaced or tampered: $($capture.name)/$prefix"}
    }
  }
  return $true
}

function Get-GameplayKindDerivedBuildingStableLows($Receipt) {
  if($Receipt.buildingIdentitySource-cne'GameplayShowcaseEnvironment3D.Descriptors'-or$Receipt.buildingIdentityDomain-cne'3368770884265705473'-or$Receipt.buildingIdentityGeneration-ne1){throw 'Building descriptor identity provenance failed'}
  $kinds=@($Receipt.descriptorKinds)
  if($kinds.Count-lt6-or$kinds.Count-gt480){throw 'Building descriptor capacity failed'}
  $result=@()
  for($index=0;$index-lt$kinds.Count;++$index){
    $kind=$kinds[$index]
    if($kind-is[string]-or$kind-is[bool]-or$null-eq$kind-or[double]$kind-ne[math]::Truncate([double]$kind)-or$kind-lt0-or$kind-gt5){throw 'Building descriptor kind invalid'}
    if($kind-eq5){$result+=([uint64]$index+1)}
  }
  if($result.Count-ne6-or@($result|Select-Object -Unique).Count-ne6){throw 'Building descriptor set incomplete or duplicated'}
  return $result
}

function Assert-GameplayReferenceOpacityEvidence($ReferenceReceipt,$CharacterReceipt,$SequenceReceipt=$null) {
  $buildingStableLows=@(Get-GameplayKindDerivedBuildingStableLows $ReferenceReceipt)
  if($ReferenceReceipt.runId-ne$CharacterReceipt.runId-or$ReferenceReceipt.source-ne'GameplayLabReferenceOpacity'-or@($ReferenceReceipt.captures).Count-ne11){throw 'Gameplay reference opacity receipt identity failed'}
  foreach($characterCapture in $CharacterReceipt.captures){
    $evidence=@($ReferenceReceipt.captures|Where-Object name -eq $characterCapture.name);if($evidence.Count-ne1){throw "Reference opacity capture identity missing: $($characterCapture.name)"};$evidence=$evidence[0]
    foreach($field in @('referencePhase','overrideMode','removalThreshold','requestedReferenceCount','matchedReferenceCount','removedReferenceCount','removedCoverage','effectiveBuildingUnion','effectiveFrameSerial','referenceFrameSerial','effectiveSequenceIndex','referenceSequenceIndex','effectiveIncludingOpacity','referenceIncludingOpacity','effectiveExcludingOpacity','referenceExcludingOpacity','effectiveIdentityOrder','referenceIdentityOrder','parts')){if($null-eq$evidence.PSObject.Properties[$field]){throw "Reference opacity typed field missing: $($characterCapture.name)/$field"}}
    if([math]::Abs([double]$evidence.removalThreshold-.08)-gt.0000001){throw "Reference opacity removal threshold drifted: $($characterCapture.name)"}
    if(@($evidence.parts).Count-ne6){throw "Reference opacity per-ID receipt count failed: $($characterCapture.name)"}
    for($partIndex=0;$partIndex-lt6;++$partIndex){$part=$evidence.parts[$partIndex];foreach($field in @('stableLow','requested','matched','missing','generationConflict','appliedOpacity')){if($null-eq$part.PSObject.Properties[$field]){throw "Reference opacity per-ID field missing: $($characterCapture.name)/$partIndex/$field"}};if($part.stableLow-ne$buildingStableLows[$partIndex]){throw "Reference opacity per-ID order drifted: $($characterCapture.name)/$partIndex"}}
    if($characterCapture.camera-ne'third'){if($evidence.referencePhase-ne0-or$evidence.overrideMode-ne'NoOverride'-or$evidence.requestedReferenceCount-ne0-or$evidence.matchedReferenceCount-ne0-or$evidence.removedReferenceCount-ne0-or[math]::Abs([double]$evidence.removedCoverage)-gt.000001-or@($evidence.parts|Where-Object{$_.requested-ne0-or$_.matched-ne0-or$_.missing-ne0-or$_.generationConflict-ne0-or$null-ne$_.appliedOpacity}).Count){throw "Non-reference capture declared opacity removal: $($characterCapture.name)"};continue}
    if($evidence.referencePhase-ne1-or[math]::Abs([double]$evidence.effectiveBuildingUnion-[double]$characterCapture.effectiveBuildingUnion)-gt.000001-or$evidence.effectiveExcludingOpacity-ne$evidence.referenceExcludingOpacity-or$evidence.effectiveIdentityOrder-ne$evidence.referenceIdentityOrder){throw "Reference opacity phase contract failed: $($characterCapture.name)"}
    $removed=$evidence.removedReferenceCount-gt0;$fingerprintChanged=$evidence.referenceIncludingOpacity-ne$evidence.effectiveIncludingOpacity
    if($removed){if($evidence.overrideMode-ne'RemoveAll'-or$evidence.requestedReferenceCount-ne6-or$evidence.removedReferenceCount-ne$evidence.matchedReferenceCount-or$evidence.removedReferenceCount-ne6-or$evidence.removedCoverage-le$evidence.removalThreshold-or[math]::Abs([double]$evidence.removedCoverage-[double]$evidence.effectiveBuildingUnion)-gt.000001-or-not$fingerprintChanged-or@($evidence.parts|Where-Object{$_.requested-ne1-or$_.matched-ne1-or$_.missing-ne0-or$_.generationConflict-ne0-or[math]::Abs([double]$_.appliedOpacity)-gt.000001}).Count){throw "Removed reference opacity evidence was inconsistent: $($characterCapture.name)"}}
    elseif($evidence.overrideMode-ne'NoOverride'-or$evidence.requestedReferenceCount-ne0-or$evidence.matchedReferenceCount-ne0-or$evidence.removedReferenceCount-ne0-or[math]::Abs([double]$evidence.removedCoverage)-gt.000001-or$evidence.effectiveBuildingUnion-gt$evidence.removalThreshold-or$fingerprintChanged-or@($evidence.parts|Where-Object{$_.requested-ne0-or$_.matched-ne0-or$_.missing-ne0-or$_.generationConflict-ne0-or$null-ne$_.appliedOpacity}).Count){throw "Retained reference opacity evidence was inconsistent: $($characterCapture.name)"}
    if($SequenceReceipt){$sequence=@($SequenceReceipt.captures|Where-Object name -eq $characterCapture.name);if($sequence.Count-ne1-or$sequence[0].phases.Count-ne3-or$evidence.effectiveSequenceIndex-ne1-or$evidence.referenceSequenceIndex-ne2-or$evidence.effectiveFrameSerial-ne$sequence[0].phases[1].frameSerial-or$evidence.referenceFrameSerial-ne$sequence[0].phases[2].frameSerial-or$evidence.effectiveIncludingOpacity-ne$sequence[0].phases[1].includingOpacity-or$evidence.referenceIncludingOpacity-ne$sequence[0].phases[2].includingOpacity-or$evidence.effectiveExcludingOpacity-ne$sequence[0].phases[1].excludingOpacity-or$evidence.referenceExcludingOpacity-ne$sequence[0].phases[2].excludingOpacity-or$evidence.effectiveIdentityOrder-ne$sequence[0].phases[1].identityOrder-or$evidence.referenceIdentityOrder-ne$sequence[0].phases[2].identityOrder){throw "Reference opacity sequence provenance mismatch: $($characterCapture.name)"}}
  }
  return $true
}

function Get-GameplayEvidenceSha256Hex([string]$Path) {
  $sha = [Security.Cryptography.SHA256]::Create()
  try {
    return ([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($Path)))).Replace('-', '').ToLowerInvariant()
  } finally {
    $sha.Dispose()
  }
}

function Get-GameplayEvidenceFileSnapshot([string]$Path) {
  $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
  try {
    if ($stream.Length -gt [int]::MaxValue) { throw "Evidence file is too large: $Path" }
    $bytes = New-Object byte[] ([int]$stream.Length)
    $offset = 0
    while ($offset -lt $bytes.Length) {
      $read = $stream.Read($bytes, $offset, $bytes.Length - $offset)
      if ($read -le 0) { throw "Evidence file was truncated while pinned: $Path" }
      $offset += $read
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
      $hash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    } finally {
      $sha.Dispose()
    }
    return [pscustomobject]@{ bytes = $bytes; length = [int64]$bytes.LongLength; sha256 = $hash }
  } finally {
    $stream.Dispose()
  }
}

function Get-GameplayEvidenceTextSha256Hex([string]$Text) {
  $sha = [Security.Cryptography.SHA256]::Create()
  try {
    return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-', '').ToLowerInvariant()
  } finally {
    $sha.Dispose()
  }
}

function Get-GameplayCanonicalSingleRawBits($Value) {
  $single=[single]$Value
  if([single]::IsNaN($single)-or[single]::IsInfinity($single)){throw 'Gameplay canonical single was not finite'}
  if($single-eq[single]0){return [uint32]0}
  return [BitConverter]::ToUInt32([BitConverter]::GetBytes($single),0)
}

function Get-GameplayGitSealEvidence([string]$SourceRoot) {
  if ([string]::IsNullOrWhiteSpace($SourceRoot) -or -not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw 'Final evidence Git source root is missing'
  }
  [string[]]$gitHeadOutput = @()
  $gitHeadOutput = @(& git -C $SourceRoot rev-parse --verify HEAD 2>$null | ForEach-Object { [string]$_ })
  $gitHeadExitCode = [int]$LASTEXITCODE
  if ($gitHeadExitCode -ne 0 -or $gitHeadOutput.Count -ne 1 -or $gitHeadOutput[0] -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$') {
    throw "Final evidence Git HEAD query failed closed: exit=$gitHeadExitCode"
  }
  [string[]]$gitStatus = @()
  [string[]]$nativeStatusLines = @()
  # tracked變更逐檔記錄；untracked只記錄tree存在，逐檔內容由inventory SHA負責。 (source inventory).
  $nativeStatusLines = @(& git -C $SourceRoot status --porcelain=v1 --untracked-files=normal 2>$null | ForEach-Object { [string]$_ })
  $gitStatusExitCode = [int]$LASTEXITCODE
  if ($gitStatusExitCode -ne 0) {
    throw "Final evidence Git status query failed closed: exit=$gitStatusExitCode"
  }
  $gitStatus = [string[]]@($nativeStatusLines)
  $gitStatusText = $gitStatus -join "`n"
  return [pscustomobject]@{
    head = $gitHeadOutput[0].ToLowerInvariant()
    statusLines = $gitStatus
    statusText = $gitStatusText
    statusSha256 = Get-GameplayEvidenceTextSha256Hex $gitStatusText
  }
}

function Assert-GameplayFrameBoundRenderProvenance {
  param(
    [Parameter(Mandatory=$true)]$Value,
    [Parameter(Mandatory=$true)][uint64]$ExpectedFrameSerial,
    [Parameter(Mandatory=$true)][uint64]$ExpectedSceneDrawRevision,
    [Parameter(Mandatory=$true)][string]$Context
  )
  $required=@('frameSerial','sceneDrawRevision','worldRevision','submittedMeshInstances','submittedSkinnedInstances','submittedTerrainInstances','submittedWorldVertices','submittedWorldIndices')
  if($null-eq$Value){throw "Frame-bound render provenance missing: $Context"}
  foreach($field in $required){if($null-eq$Value.PSObject.Properties[$field]){throw "Frame-bound render provenance field missing: $Context field=$field"}}
  $submittedInstances=[uint64]$Value.submittedMeshInstances+[uint64]$Value.submittedSkinnedInstances+[uint64]$Value.submittedTerrainInstances
  $worldPayloadPresent=[uint64]$Value.submittedWorldVertices-gt0-and[uint64]$Value.submittedWorldIndices-gt0-and[uint64]$Value.worldRevision-gt0
  $worldPayloadAbsent=[uint64]$Value.submittedWorldVertices-eq0-and[uint64]$Value.submittedWorldIndices-eq0-and[uint64]$Value.worldRevision-eq0
  if([uint64]$Value.frameSerial-ne$ExpectedFrameSerial-or[uint64]$Value.sceneDrawRevision-ne$ExpectedSceneDrawRevision-or$submittedInstances-lt1-or(-not$worldPayloadPresent-and-not$worldPayloadAbsent)){throw "Frame-bound render provenance gate failed: $Context"}
  return $true
}

function Assert-GameplayDeterminismIntegrityEvidence {
  param(
    [Parameter(Mandatory=$true)]$Receipt,
    [Parameter(Mandatory=$true)][ValidateRange(1,1024)][int]$ExpectedCaptures,
    $SequenceReceipt=@(),
    $CharacterReceipt=@()
  )
  if($null-eq$Receipt-or$Receipt.source-ne'GameplayLabDeterminismIntegrity'){throw 'Determinism integrity sidecar missing'}
  $captures=@($Receipt.captures);if($captures.Count-ne$ExpectedCaptures){throw 'Determinism integrity capture count mismatch'}
  $names=@{};foreach($capture in $captures){if([string]::IsNullOrWhiteSpace([string]$capture.name)-or$names.ContainsKey([string]$capture.name)){throw 'Determinism integrity capture name was missing or duplicated'};$names[[string]$capture.name]=$true;$fingerprint=$capture.canonicalDrawFingerprint;if($capture.determinismFrameSerial-lt1-or$capture.determinismIntegrityRequested-ne'SealedCapture'-or$capture.determinismIntegrityActual-ne'SealedCapture'-or$capture.determinismIntegrityReason-ne'None'-or$capture.determinismIntegrityRejected-ne0-or$capture.determinismIntegritySampled-ne$capture.determinismIntegrityVerified-or$capture.determinismIntegritySampled-lt1-or$capture.determinismIntegrityAcceptedFrame-ne1-or$null-eq$fingerprint-or$fingerprint.schemaVersion-ne1-or$fingerprint.topology-eq0-or$fingerprint.material-eq0-or$fingerprint.transform-eq0-or$fingerprint.palette-eq0-or$fingerprint.identity-eq0-or$fingerprint.order-eq0){throw "Determinism integrity sealed capture gate failed: $($capture.name)"};if(@($SequenceReceipt).Count){$sequence=@($SequenceReceipt.captures|Where-Object name -eq $capture.name);if($sequence.Count-ne1-or@($sequence[0].phases).Count-lt2){throw "Frame-bound determinism sequence evidence missing: $($capture.name)"};$phases=@($sequence[0].phases);foreach($phase in $phases){$phaseFingerprint=$phase.canonicalDrawFingerprint;if($phase.determinismFrameSerial-ne$phase.frameSerial-or$phase.determinismIntegrityRequested-ne'SealedCapture'-or$phase.determinismIntegrityActual-ne'SealedCapture'-or$phase.determinismIntegrityReason-ne'None'-or$phase.determinismIntegrityRejected-ne0-or$phase.determinismIntegritySampled-ne$phase.determinismIntegrityVerified-or$phase.determinismIntegritySampled-lt1-or$phase.determinismIntegrityAcceptedFrame-ne1-or$null-eq$phaseFingerprint-or$phaseFingerprint.schemaVersion-ne1-or$phaseFingerprint.topology-eq0-or$phaseFingerprint.material-eq0-or$phaseFingerprint.transform-eq0-or$phaseFingerprint.palette-eq0-or$phaseFingerprint.identity-eq0-or$phaseFingerprint.order-eq0){throw "Frame-bound determinism phase gate failed: $($capture.name) phase=$($phase.index)"};$null = Assert-GameplayFrameBoundRenderProvenance -Value $phase.renderProvenance -ExpectedFrameSerial ([uint64]$phase.frameSerial) -ExpectedSceneDrawRevision ([uint64]$phase.drawListRevision) -Context "$($capture.name) phase=$($phase.index)"};$effective=$phases[1];foreach($field in @('determinismFrameSerial','determinismIntegrityRequested','determinismIntegrityActual','determinismIntegrityReason','determinismIntegritySampled','determinismIntegrityVerified','determinismIntegrityRejected','determinismIntegrityAcceptedFrame')){if($capture.$field-ne$effective.$field){throw "Effective frame-bound integrity drifted: $($capture.name) field=$field"}};if(($capture.canonicalDrawFingerprint|ConvertTo-Json -Compress)-cne($effective.canonicalDrawFingerprint|ConvertTo-Json -Compress)){throw "Effective canonical fingerprint drifted: $($capture.name)"};if(($capture.renderProvenance|ConvertTo-Json -Compress)-cne($effective.renderProvenance|ConvertTo-Json -Compress)){throw "Effective render provenance drifted: $($capture.name)"};if(@($CharacterReceipt).Count){$character=@($CharacterReceipt.captures|Where-Object name -eq $capture.name);if($character.Count-ne1){throw "Character frame-bound integrity missing: $($capture.name)"};foreach($field in @('determinismFrameSerial','determinismIntegrityRequested','determinismIntegrityActual','determinismIntegrityReason','determinismIntegritySampled','determinismIntegrityVerified','determinismIntegrityRejected','determinismIntegrityAcceptedFrame')){if($character[0].$field-ne$effective.$field){throw "Character frame-bound integrity drifted: $($capture.name) field=$field"}};if(($character[0].canonicalDrawFingerprint|ConvertTo-Json -Compress)-cne($effective.canonicalDrawFingerprint|ConvertTo-Json -Compress)){throw "Character canonical fingerprint drifted: $($capture.name)"};if(($character[0].renderProvenance|ConvertTo-Json -Compress)-cne($effective.renderProvenance|ConvertTo-Json -Compress)){throw "Character render provenance drifted: $($capture.name)"}}}}
  if($ExpectedCaptures-eq11){$expected=@(Get-GameplayCaptureViewExpectations|ForEach-Object name|Sort-Object);$actual=@($captures|ForEach-Object name|Sort-Object);if(@(Compare-Object -CaseSensitive $expected $actual).Count){throw 'Determinism integrity capture names did not match the exact capture set'}}
  return $true
}

function Get-GameplayCanonicalStateSha($Receipt,$Environment,$Collision,$Palette,$Character,$Camera,$Sequence,$Photoreal) {
  $copyWithoutRunId={param($value)$result=[ordered]@{};foreach($property in $value.PSObject.Properties){if($property.Name-ne'runId'){$result[$property.Name]=$property.Value}};return $result}
  $receiptFields=@('captures','entities','physicsBodies','authoritativePhysicsTick','authoritativePhysicsSteps','cameraSwitches','jumps','crouches','palettePreviews','paletteCommits','combatSweeps','combatHits','combatKills','monsterHealth','animationState','weaponAnimationSyncPassed','firstPersonBodyHidden','femaleFirstPersonArms','maleFirstPersonArms','femaleCharacterMaterialCount','maleCharacterMaterialCount','characterMaterialCount','characterMaterialLoadFailures','gpuValidationRequested','gpuValidationAvailable','gpuValidationExecuted','debugLayerEnabled','gpuValidationPassed','authoritativeTrajectoryHash','authoritativeAttackTrajectoryHashes','authoritativeTrajectoryCheckpointCount','authoritativeAttackCheckpointCounts')
  $receiptState=[ordered]@{};foreach($field in $receiptFields){$receiptState[$field]=$Receipt.$field}
  $environmentState=[ordered]@{source=$Environment.source;worldHash=$Environment.worldHash;descriptors=$Environment.descriptors;submittedDescriptors=$Environment.submittedDescriptors;executionObserved=$Environment.executionObserved;counterCount=@($Environment.counterDeltas).Count;counterAllPositive=@($Environment.counterDeltas|Where-Object{$_-le0}).Count-eq0;normalizedAssets=$Environment.normalizedAssets;normalizationRejected=$Environment.normalizationRejected;meshChainsSubmitted=$Environment.meshChainsSubmitted;meshChainsRejected=$Environment.meshChainsRejected;releasedDescriptors=$Environment.releasedDescriptors;retainedBytesAfterRelease=$Environment.retainedBytesAfterRelease}
  $paletteState=&$copyWithoutRunId $Palette
  # These are process-local presentation totals. Their minimums are validated by
  # Assert-GameplayRunEvidence, but wall-clock-dependent extra frames must not
  # masquerade as an authoritative world-state change between fresh processes.
  $paletteState.Remove('drawnFrames')
  $paletteState.Remove('cleanHudPresentationFrames')
  $sequenceCaptures=@($Sequence.captures)
  $characterCaptures=@($Character.captures);$objectGenerationBase=if($characterCaptures.Count){[int64]$characterCaptures[0].objectIdGeneration}else{0};$depthGenerationBase=if($characterCaptures.Count){[int64]$characterCaptures[0].depthGeneration}else{0};$characterCaptureState=@()
  for($captureIndex=0;$captureIndex-lt$characterCaptures.Count;++$captureIndex){$capture=$characterCaptures[$captureIndex];$sequencePhases=if($captureIndex-lt$sequenceCaptures.Count){@($sequenceCaptures[$captureIndex].phases)}else{@()};$basePhase=if($sequencePhases.Count){$sequencePhases[0]}else{$null};$effectivePhase=if($sequencePhases.Count-gt1){$sequencePhases[1]}else{$null};$state=[ordered]@{};foreach($property in $capture.PSObject.Properties){if($property.Name-notin@('rawTokenMetadataSha256','effectiveTokenMetadataSha256','objectIdGeneration','depthGeneration','contactFrameSerial','determinismFrameSerial','renderProvenance')){$state[$property.Name]=$property.Value}};$render=[ordered]@{};foreach($property in $capture.renderProvenance.PSObject.Properties){if($property.Name-ne'frameSerial'){$render[$property.Name]=$property.Value}};$render.frameSerialDelta=if($null-ne$basePhase){[int64]$capture.renderProvenance.frameSerial-[int64]$basePhase.frameSerial}else{0};$render.matchesEffective=$null-ne$effectivePhase-and$capture.renderProvenance.frameSerial-eq$effectivePhase.frameSerial;$render.afterBase=$null-ne$basePhase-and$capture.renderProvenance.frameSerial-gt$basePhase.frameSerial;$state.renderProvenance=$render;$state.objectIdGenerationDelta=[int64]$capture.objectIdGeneration-$objectGenerationBase;$state.depthGenerationDelta=[int64]$capture.depthGeneration-$depthGenerationBase;$state.objectDepthGenerationDelta=[int64]$capture.depthGeneration-[int64]$capture.objectIdGeneration;$state.contactMatchesEffective=$null-ne$effectivePhase-and$capture.contactFrameSerial-eq$effectivePhase.frameSerial;$state.determinismMatchesEffective=$null-ne$effectivePhase-and$capture.determinismFrameSerial-eq$effectivePhase.frameSerial;$characterCaptureState+=$state}
  $characterState=[ordered]@{};foreach($property in $Character.PSObject.Properties){if($property.Name-notin@('runId','captures')){$characterState[$property.Name]=$property.Value}};$characterState.captures=$characterCaptureState
  $sequenceBaseToken=if($sequenceCaptures.Count){[int64]$sequenceCaptures[0].token}else{0};$sequenceState=@()
  foreach($capture in $sequenceCaptures){$phases=@($capture.phases);$first=if($phases.Count){$phases[0]}else{$null};$phaseState=@();foreach($phase in $phases){$phaseState+=[ordered]@{index=$phase.index;sequenceIndex=$phase.sequenceIndex;tokenMatchesCapture=($phase.sequenceToken-eq$capture.token);drawListRevision=$phase.drawListRevision;cameraHash=$phase.cameraHash;stateHash=$phase.stateHash;invariantHash=$phase.invariantHash;frameDelta=if($first){[int64]$phase.frameSerial-[int64]$first.frameSerial}else{0};viewProjectionHash=$phase.viewProjectionHash;tokenGenerationDelta=if($first){[int64]$phase.tokenGeneration-[int64]$first.tokenGeneration}else{0};historyGenerationDelta=if($first){[int64]$phase.historyGeneration-[int64]$first.historyGeneration}else{0};historyIndexStable=if($first){$phase.historyIndex-eq$first.historyIndex}else{$false};jitterIndex=$phase.jitterIndex;jitterX=$phase.jitterX;jitterY=$phase.jitterY;excludingOpacity=$phase.excludingOpacity;includingOpacity=$phase.includingOpacity;identityOrder=$phase.identityOrder}}
    $sequenceState+=[ordered]@{name=$capture.name;complete=$capture.complete;expectedPhaseCount=$capture.expectedPhaseCount;tokenDelta=[int64]$capture.token-$sequenceBaseToken;jitterPolicy=$capture.jitterPolicy;fixedSampleIndex=$capture.fixedSampleIndex;phases=$phaseState}
  }
  $photorealCaptures=@($Photoreal.captures);$shadowGenerationBase=if($photorealCaptures.Count){[int64]$photorealCaptures[0].shadowResourceGeneration}else{0};$photorealCaptureState=@()
  for($index=0;$index-lt$photorealCaptures.Count;++$index){$capture=$photorealCaptures[$index];$sequenceCapture=$sequenceCaptures[$index];$sequencePhases=@($sequenceCapture.phases);$frameBase=if($sequencePhases.Count){[int64]$sequencePhases[0].frameSerial}else{0};$state=[ordered]@{};foreach($property in $capture.PSObject.Properties){if($property.Name-notin@('effectiveFrameSerial','semanticFrameSerial','sceneSubmissionSerial','shadowFrameSerial','lodFrameSerial','shadowResourceGeneration')){$state[$property.Name]=$property.Value}};$state.effectiveFrameDelta=[int64]$capture.effectiveFrameSerial-$frameBase;$state.semanticMatchesEffective=$capture.semanticFrameSerial-eq$capture.effectiveFrameSerial;$state.sceneSubmissionDelta=[int64]$capture.sceneSubmissionSerial-$frameBase;$state.shadowMatchesEffective=$capture.shadowFrameSerial-eq$capture.effectiveFrameSerial;$state.lodMatchesEffective=$capture.lodFrameSerial-eq$capture.effectiveFrameSerial;$state.shadowResourceGenerationDelta=[int64]$capture.shadowResourceGeneration-$shadowGenerationBase;$state.shadowResourcePresent=[int64]$capture.shadowResourceGeneration-gt0;$photorealCaptureState+=$state}
  $photorealState=[ordered]@{};foreach($property in $Photoreal.PSObject.Properties){if($property.Name-notin@('runId','captures')){$photorealState[$property.Name]=$property.Value}};$photorealState.captures=$photorealCaptureState
  $payload=[ordered]@{receipt=$receiptState;environment=$environmentState;collision=(&$copyWithoutRunId $Collision);palette=$paletteState;character=$characterState;camera=(&$copyWithoutRunId $Camera);sequence=[ordered]@{captures=$sequenceState};photoreal=$photorealState}
  return Get-GameplayEvidenceTextSha256Hex ($payload|ConvertTo-Json -Depth 30 -Compress)
}

function Get-GameplayPhotorealPackageEvidence([string]$ManifestPath) {
  $resolvedManifest = (Resolve-Path -LiteralPath $ManifestPath).Path
  $root = [IO.Path]::GetFullPath((Split-Path -Parent $resolvedManifest))
  $rootPrefix = $root.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
  $manifestSnapshot = Get-GameplayEvidenceFileSnapshot $resolvedManifest
  $manifestBytes = $manifestSnapshot.bytes
  $utf8 = New-Object Text.UTF8Encoding($false, $true)
  try {
    $manifest = $utf8.GetString($manifestBytes) | ConvertFrom-Json
  } catch {
    throw "Photoreal manifest is not strict UTF-8 JSON: $resolvedManifest"
  }

  if ($manifest.schemaVersion -ne 1 -or $manifest.manifestType -ne 'aevrynd-runtime-render-package' -or
      $manifest.exactAssetSet.count -ne 12 -or @($manifest.exactAssetSet.assetIds).Count -ne 12 -or
      @($manifest.assets).Count -ne 12 -or @($manifest.files).Count -ne 96 -or
      @($manifest.materials).Count -ne 18 -or @($manifest.meshParts).Count -ne 13 -or
      $manifest.package.payloadFileCount -ne 96 -or $manifest.package.packageFileCount -ne 97 -or
      $manifest.package.manifestPath -ne 'runtime-render-manifest.json' -or
      $manifest.package.manifestHashPolicy -ne 'external-envelope') {
    throw 'Photoreal manifest exact schema/count gate failed'
  }
  if ([int64]$manifest.package.manifestBytes -ne $manifestBytes.LongLength) {
    throw 'Photoreal manifest byte count does not describe the loaded bytes'
  }

  $declared = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
  $payloadBytes = [int64]0
  $payloadSnapshotLines = @()
  foreach ($file in @($manifest.files)) {
    $relative = [string]$file.path
    if ([string]::IsNullOrWhiteSpace($relative) -or [IO.Path]::IsPathRooted($relative) -or
        -not $declared.Add($relative.Replace('\', '/')) -or $file.sha256 -notmatch '^[0-9a-f]{64}$' -or
        [int64]$file.bytes -lt 1) {
      throw "Photoreal manifest invalid or duplicate payload entry: $relative"
    }
    $payloadPath = [IO.Path]::GetFullPath((Join-Path $root $relative))
    if (-not $payloadPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $payloadPath -PathType Leaf)) {
      throw "Photoreal payload escapes or is missing: $relative"
    }
    $snapshot = Get-GameplayEvidenceFileSnapshot $payloadPath
    if ($snapshot.length -ne [int64]$file.bytes -or $snapshot.sha256 -ne [string]$file.sha256) {
      throw "Photoreal payload bytes/SHA mismatch: $relative"
    }
    $payloadBytes += $snapshot.length
    $payloadSnapshotLines += "$($relative.Replace('\', '/')):$($snapshot.length):$($snapshot.sha256)"
  }
  if ($payloadBytes -ne [int64]$manifest.package.payloadBytes -or
      $payloadBytes + $manifestBytes.LongLength -ne [int64]$manifest.package.packageBytes) {
    throw 'Photoreal package aggregate byte gate failed'
  }

  $referenced = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
  foreach ($material in @($manifest.materials)) {
    foreach ($property in @('baseColorPath', 'surfacePath', 'normalPath')) {
      $value = [string]$material.$property
      if ([string]::IsNullOrWhiteSpace($value) -or -not $referenced.Add($value.Replace('\', '/'))) {
        throw "Photoreal material reference is missing or duplicated: $($material.id)/$property"
      }
    }
  }
  foreach ($part in @($manifest.meshParts)) {
    if (@($part.lodPaths).Count -ne 3) { throw "Photoreal mesh part LOD count mismatch: $($part.id)" }
    foreach ($value in @($part.lodPaths)) {
      if (-not $referenced.Add(([string]$value).Replace('\', '/'))) {
        throw "Photoreal mesh reference is duplicated: $value"
      }
    }
  }
  foreach ($value in @($manifest.ibl.specularPath, $manifest.ibl.irradiancePath, $manifest.ibl.brdfLutPath)) {
    if (-not $referenced.Add(([string]$value).Replace('\', '/'))) {
      throw "Photoreal IBL reference is missing or duplicated: $value"
    }
  }
  if ($referenced.Count -ne 96) { throw 'Photoreal referenced payload count is not exactly 96' }
  foreach ($value in $declared) {
    if (-not $referenced.Contains($value)) { throw "Photoreal payload is not schema-referenced: $value" }
  }

  $actual = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
  foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse) {
    $relative = $file.FullName.Substring($rootPrefix.Length).Replace('\', '/')
    if (-not $actual.Add($relative)) { throw "Photoreal package has duplicate normalized path: $relative" }
  }
  if ($actual.Count -ne 97 -or -not $actual.Contains('runtime-render-manifest.json')) {
    throw 'Photoreal staging package is not the exact clean 97-file set'
  }
  foreach ($value in $declared) {
    if (-not $actual.Contains($value)) { throw "Photoreal staged payload is missing: $value" }
  }

  $payloadSha256 = Get-GameplayEvidenceTextSha256Hex ($payloadSnapshotLines -join "`n")
  $snapshotEpochSha256 = Get-GameplayEvidenceTextSha256Hex "manifest=$($manifestSnapshot.sha256):$($manifestBytes.LongLength)|payload=$payloadSha256|files=97|bytes=$($payloadBytes + $manifestBytes.LongLength)"
  return [pscustomobject]@{
    manifestPath = $resolvedManifest
    manifestSha256 = $manifestSnapshot.sha256
    manifestBytes = $manifestBytes.LongLength
    payloadFiles = 96
    materials = 18
    meshParts = 13
    referencedFiles = 96
    packageFiles = 97
    payloadBytes = $payloadBytes
    packageBytes = $payloadBytes + $manifestBytes.LongLength
    payloadSha256 = $payloadSha256
    snapshotEpochSha256 = $snapshotEpochSha256
  }
}

function Assert-GameplayPhotorealPackageSnapshot($Expected, [string]$ManifestPath) {
  $actual = Get-GameplayPhotorealPackageEvidence $ManifestPath
  foreach ($field in @('manifestSha256','manifestBytes','payloadFiles','payloadBytes','packageFiles','packageBytes','payloadSha256','snapshotEpochSha256')) {
    if ($actual.$field -ne $Expected.$field) { throw "Photoreal package snapshot changed: $field" }
  }
  return $actual
}

function Assert-GameplayNoFatalMarker([string]$Directory) {
  if (Test-Path -LiteralPath (Join-Path $Directory 'gameplay-fatal-error.txt') -PathType Leaf) {
    throw 'Successful capture artifact contains a fatal marker'
  }
}

function Get-GameplayExpectedRunArtifactNames([string[]]$ExpectedNames) {
  $result = @()
  foreach ($name in $ExpectedNames) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($name)
    $result += @($name, "$stem-object-id.png", "$stem-raw.png", "$stem-raw-object-id.png", "$stem-depth.png", "$stem-raw-depth.png", "$stem-tokens.bin", "$stem-tokens.json", "$stem-raw-tokens.bin", "$stem-raw-tokens.json")
    if ($name -like 'third-*' -or $name -in @('palette-building.png','melee-combat.png')) {
      $result += @("$stem-reference.png", "$stem-reference-object-id.png", "$stem-reference-depth.png", "$stem-contact.png", "$stem-reference-contact.png")
    }
  }
  return $result
}

function Assert-GameplayRunArchiveSchema($Run, [string[]]$ExpectedNames, [string[]]$ExpectedReceiptNames) {
  if ($Run.fatalAbsent -ne 1) { throw "Run archive did not seal fatal-marker absence: run=$($Run.run)" }
  $artifacts = @($Run.artifacts); $expectedArtifacts = @(Get-GameplayExpectedRunArtifactNames $ExpectedNames)
  $optionalArtifacts = @()
  foreach ($name in $ExpectedNames) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($name)
    $optionalArtifacts += @("$stem-atmosphere-rgba16f.bin", "$stem-atmosphere-output.json")
  }
  $allowedArtifacts = @($expectedArtifacts + $optionalArtifacts)
  $artifactNames = @($artifacts | ForEach-Object { [string]$_.name })
  if (@($artifactNames | Select-Object -Unique).Count -ne $artifacts.Count) { throw "Run archive artifact unique-name mismatch: run=$($Run.run)" }
  foreach ($artifact in $artifacts) {
    if ([string]$artifact.name -cnotin $allowedArtifacts -or [string]$artifact.sha256 -notmatch '^[0-9a-f]{64}$') { throw "Run archive contains an invalid or unexpected artifact: run=$($Run.run) name=$($artifact.name)" }
  }
  foreach ($name in $expectedArtifacts) {
    $entry = @($artifacts | Where-Object { $_.name -ceq $name })
    if ($entry.Count -ne 1 -or $entry[0].sha256 -notmatch '^[0-9a-f]{64}$') { throw "Run archive missing or duplicate artifact: run=$($Run.run) name=$name" }
  }
  foreach ($name in $ExpectedNames) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($name)
    $payload = @($artifacts | Where-Object { $_.name -ceq "$stem-atmosphere-rgba16f.bin" })
    $receipt = @($artifacts | Where-Object { $_.name -ceq "$stem-atmosphere-output.json" })
    if ($payload.Count -ne $receipt.Count -or $payload.Count -gt 1) { throw "Run archive atmosphere diagnostic pair mismatch: run=$($Run.run) name=$name" }
  }
  $receipts = @($Run.receipts); $receiptNames = @($receipts | ForEach-Object { [string]$_.name })
  if ($receipts.Count -ne $ExpectedReceiptNames.Count -or @($receiptNames | Select-Object -Unique).Count -ne $ExpectedReceiptNames.Count) { throw "Run archive receipt count/unique-name mismatch: run=$($Run.run)" }
  foreach ($name in $ExpectedReceiptNames) {
    $entry = @($receipts | Where-Object { $_.name -ceq $name })
    if ($entry.Count -ne 1 -or $entry[0].sha256 -notmatch '^[0-9a-f]{64}$') { throw "Run archive missing or duplicate receipt: run=$($Run.run) name=$name" }
  }
}

function Assert-GameplayOrderedCaptureNames($Captures, [string[]]$ExpectedNames, [string]$ReceiptName) {
  $items = @($Captures)
  if ($items.Count -ne $ExpectedNames.Count) { throw "$ReceiptName capture count mismatch" }
  $unique = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
  for ($index = 0; $index -lt $ExpectedNames.Count; ++$index) {
    $name = [string]$items[$index].name
    if ($name -ne $ExpectedNames[$index] -or -not $unique.Add($name)) {
      throw "$ReceiptName ordered/unique capture identity mismatch at index $index"
    }
  }
}

function Get-GameplayThirdPersonOcclusionPathEvidence($Capture) {
  $required=@('rawOccluderCoverage','policyFadeOpacity','fadedStableLow','rawAcceptedIncludingOpacity','effectiveAcceptedIncludingOpacity')
  foreach($field in $required){
    if($null-eq$Capture-or$null-eq$Capture.PSObject.Properties[$field]){throw "Third-person occlusion path evidence field is missing: $field"}
    $value=$Capture.PSObject.Properties[$field].Value
    if($null-eq$value-or($value-is[string]-and[string]::IsNullOrWhiteSpace($value))){throw "Third-person occlusion path evidence field is empty: $field"}
  }
  $rawCoverage=[double]$Capture.rawOccluderCoverage;$policyOpacity=[double]$Capture.policyFadeOpacity
  if([double]::IsNaN($rawCoverage)-or[double]::IsInfinity($rawCoverage)-or$rawCoverage-lt0-or$rawCoverage-gt1-or
     [double]::IsNaN($policyOpacity)-or[double]::IsInfinity($policyOpacity)-or$policyOpacity-lt0-or$policyOpacity-gt1){throw 'Third-person occlusion path evidence is non-finite or out of range'}
  return [pscustomobject]@{
    rawPathOccluded=($rawCoverage-gt.08)
    clearPathOpacityInvalid=($policyOpacity-ne1)-or([uint64]$Capture.fadedStableLow-ne0)-or([uint64]$Capture.rawAcceptedIncludingOpacity-ne[uint64]$Capture.effectiveAcceptedIncludingOpacity)
  }
}

function Get-GameplayCaptureViewExpectations {
  return @(
    [pscustomobject]@{name='first-day.png';camera='first';lighting='day';requireLodTransition=$true},
    [pscustomobject]@{name='third-day.png';camera='third';lighting='day';requireLodTransition=$true},
    [pscustomobject]@{name='god-day.png';camera='god';lighting='day';requireLodTransition=$true},
    [pscustomobject]@{name='first-dusk.png';camera='first';lighting='dusk';requireLodTransition=$true},
    [pscustomobject]@{name='third-dusk.png';camera='third';lighting='dusk';requireLodTransition=$true},
    [pscustomobject]@{name='god-dusk.png';camera='god';lighting='dusk';requireLodTransition=$true},
    [pscustomobject]@{name='first-night.png';camera='first';lighting='night';requireLodTransition=$true},
    [pscustomobject]@{name='third-night.png';camera='third';lighting='night';requireLodTransition=$true},
    [pscustomobject]@{name='god-night.png';camera='god';lighting='night';requireLodTransition=$true},
    [pscustomobject]@{name='palette-building.png';camera='third';lighting='day';requireLodTransition=$true},
    [pscustomobject]@{name='melee-combat.png';camera='third';lighting='dusk';requireLodTransition=$true}
  )
}

function Invoke-GameplayHdrValidator([string]$ValidatorPath,[string]$SidecarPath) {
  if(-not[IO.Path]::IsPathRooted($ValidatorPath)-or-not(Test-Path -LiteralPath $ValidatorPath -PathType Leaf)){throw 'Gameplay HDR validator must be an existing absolute file'}
  $validator=[IO.Path]::GetFullPath($ValidatorPath);$sidecar=[IO.Path]::GetFullPath($SidecarPath)
  # 測試可使用 PowerShell stub；正式 gate 固定傳入相鄰的原生 exe。
  $launchFile='';$launchArguments=''
  if([IO.Path]::GetExtension($validator)-ieq'.ps1'){
    $windowsPowerShell=Join-Path $PSHOME 'powershell.exe';$powerShellCore=Join-Path $PSHOME 'pwsh.exe'
    if(Test-Path -LiteralPath $windowsPowerShell -PathType Leaf){$launchFile=$windowsPowerShell}elseif(Test-Path -LiteralPath $powerShellCore -PathType Leaf){$launchFile=$powerShellCore}else{$launchFile=(Get-Process -Id $PID).Path}
    $launchArguments='-NoProfile -ExecutionPolicy Bypass -File "'+$validator+'" "'+$sidecar+'"'
  }else{$launchFile=$validator;$launchArguments='"'+$sidecar+'"'}
  if([string]::IsNullOrWhiteSpace($launchFile)){throw "Gameplay HDR validator launch filename was empty: validator=$validator extension=$([IO.Path]::GetExtension($validator))"}
  $info=[Diagnostics.ProcessStartInfo]::new($launchFile,$launchArguments);$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
  $process=[Diagnostics.Process]::new();$process.StartInfo=$info
  try{
    if(-not$process.Start()){throw 'Gameplay HDR validator process did not start'}
    $stdoutTask=$process.StandardOutput.ReadToEndAsync();$stderrTask=$process.StandardError.ReadToEndAsync()
    if(-not$process.WaitForExit(30000)){try{$process.Kill()}catch{};throw 'Gameplay HDR validator timed out'}
    return [pscustomobject]@{exitCode=$process.ExitCode;stdout=$stdoutTask.GetAwaiter().GetResult();stderr=$stderrTask.GetAwaiter().GetResult()}
  }finally{$process.Dispose()}
}

function Assert-GameplayHdrValidatorExactEvidence([string]$ValidatorPath,[string]$SidecarPath,$ExpectedEntry,[string]$Context) {
  if(-not(Test-Path -LiteralPath $SidecarPath -PathType Leaf)){throw "Gameplay HDR validator input was missing: $Context"}
  $snapshot=Get-GameplayEvidenceFileSnapshot $SidecarPath
  $validation=Invoke-GameplayHdrValidator $ValidatorPath $SidecarPath
  if([int]$validation.exitCode-ne0){throw "Gameplay HDR validator rejected sidecar: $Context exit=$($validation.exitCode)"}
  $lines=@(([string]$validation.stdout-split'\r?\n')|Where-Object{-not[string]::IsNullOrWhiteSpace($_)})
  if($lines.Count-ne1){throw "Gameplay HDR validator output was not exactly one JSON line: $Context"}
  $jsonLine=$lines[0].Trim();if(-not$jsonLine.StartsWith('{',[StringComparison]::Ordinal)-or-not$jsonLine.EndsWith('}',[StringComparison]::Ordinal)){throw "Gameplay HDR validator output JSON was not an object: $Context"}
  try{$validated=$jsonLine|ConvertFrom-Json -ErrorAction Stop}catch{throw "Gameplay HDR validator output JSON was invalid: $Context"}
  if($validated-is[Array]){throw "Gameplay HDR validator output JSON was not an object: $Context"}
  foreach($field in @('schemaVersion','byteCount','fileHash','stableContentHash','payloadHash','rootHash')){if($null-eq$validated.PSObject.Properties[$field]){throw "Gameplay HDR validator field missing: $Context/$field"}}
  if([uint32]$validated.schemaVersion-ne3-or[uint64]$validated.byteCount-ne[uint64]$snapshot.length-or[uint64]$validated.byteCount-ne[uint64]$ExpectedEntry.byteCount){throw "Gameplay HDR validator schema/byte count mismatch: $Context"}
  foreach($field in @('fileHash','stableContentHash','payloadHash','rootHash')){$hash=[string]$validated.$field;if($hash-notmatch'^[0-9a-f]{64}$'-or$hash-match'^0{64}$'-or$hash-cne[string]$ExpectedEntry.$field){throw "Gameplay HDR validator/expected hash mismatch: $Context/$field"}}
  if([string]$validated.fileHash-cne$snapshot.sha256){throw "Gameplay HDR validator/file hash mismatch: $Context"}
  return $validated
}

function Assert-GameplayHdrCaptureEvidence(
    [string]$CaptureDirectory, $Receipt, $SequenceReceipt, [string[]]$ExpectedNames,
    [string]$ValidatorPath, [string]$ExpectedRunId) {
  if(-not[IO.Path]::IsPathRooted($ValidatorPath)-or-not(Test-Path -LiteralPath $ValidatorPath -PathType Leaf)){throw 'Gameplay HDR validator is missing or is not absolute'}
  $parsedRunId=[Guid]::Empty
  if(-not[Guid]::TryParseExact($ExpectedRunId,'D',[ref]$parsedRunId)-or$parsedRunId.ToString('D')-cne$ExpectedRunId-or[string]$Receipt.runId-cne$ExpectedRunId-or[string]$SequenceReceipt.runId-cne$ExpectedRunId){throw 'Gameplay HDR run identifier mismatch'}
  if([string]$Receipt.source-cne'GameplayLabProductionHdr'-or[uint32]$Receipt.expectedCaptures-ne11-or[uint64]$Receipt.engineRawPeakBytes-lt1-or[uint64]$Receipt.engineRawPeakBytes-gt16MB-or[uint32]$Receipt.maximumInFlight-ne1){throw 'Gameplay HDR production receipt header failed'}
  $rows=@($Receipt.captures)
  if($rows.Count-ne11-or$ExpectedNames.Count-ne11-or@($rows.name|Select-Object -Unique).Count-ne11-or@($rows.path|Select-Object -Unique).Count-ne11){throw 'Gameplay HDR sidecar receipt count/uniqueness failed'}
  $root=[IO.Path]::GetFullPath($CaptureDirectory).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
  $result=@()
  for($index=0;$index-lt11;++$index){
    $row=$rows[$index];$sequence=@($SequenceReceipt.captures|Where-Object name -CEQ $ExpectedNames[$index])
    if($row.name-cne$ExpectedNames[$index]-or[string]::IsNullOrWhiteSpace([string]$row.path)-or[IO.Path]::IsPathRooted([string]$row.path)-or[string]$row.path-match'(^|[\/])\.\.([\/]|$)'){throw "Gameplay HDR sidecar order/path failed: index=$index"}
    $path=[IO.Path]::GetFullPath((Join-Path $CaptureDirectory ([string]$row.path)))
    if(-not$path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)-or-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Gameplay HDR sidecar file missing or escaped root: $($row.name)"}
    if([IO.Path]::GetExtension($path)-ine'.aevhdr'){throw "Gameplay HDR sidecar extension failed: $($row.name)"}
    $snapshot=Get-GameplayEvidenceFileSnapshot $path
    foreach($field in @('fileHash','payloadHash','rootHash','stableContentHash')){$hash=[string]$row.$field;if($hash-notmatch'^[0-9a-f]{64}$'-or$hash-match'^0{64}$'){throw "Gameplay HDR sidecar hash invalid: $($row.name)/$field"}}
    if([uint64]$row.byteCount-ne[uint64]$snapshot.length-or[string]$row.fileHash-cne$snapshot.sha256-or[string]$row.state-cne'Verified'-or$row.verified-ne$true){throw "Gameplay HDR sidecar file/hash/verified failed: $($row.name)"}
    $null=Assert-GameplayHdrValidatorExactEvidence $ValidatorPath $path $row ([string]$row.name)
    if($sequence.Count-ne1-or@($sequence[0].phases).Count-lt2){throw "Gameplay HDR sidecar sequence capture missing: $($row.name)"}
    $phase=$sequence[0].phases[1]
    if([uint32]$row.sequenceIndex-ne1-or[uint32]$phase.index-ne1-or[uint32]$phase.sequenceIndex-ne1-or[uint64]$row.frameSerial-ne[uint64]$phase.frameSerial-or[uint64]$row.sequenceToken-ne[uint64]$phase.sequenceToken-or[uint64]$row.sequenceToken-ne[uint64]$sequence[0].token){throw "Gameplay HDR sidecar sequence provenance mismatch: $($row.name)"}
    $result+=[pscustomobject]@{name=[string]$row.name;path=[string]$row.path;byteCount=[uint64]$snapshot.length;fileHash=$snapshot.sha256;payloadHash=[string]$row.payloadHash;rootHash=[string]$row.rootHash;stableContentHash=[string]$row.stableContentHash;frameSerial=[uint64]$row.frameSerial;sequenceToken=[uint64]$row.sequenceToken;sequenceIndex=1}
  }
  return $result
}

# Cross-run evidence excludes frame/fence/resource/history identity, while visual/canonical/material content remains sealed by stableContentHash.
function Assert-GameplayHdrStableContentDeterminism($Baseline,$Candidate,$ExpectedNames) {
  $left=@($Baseline)
  $right=@($Candidate)
  if($left.Count -ne 11 -or $right.Count -ne 11 -or $ExpectedNames.Count -ne 11){throw 'Gameplay HDR stable content row count failed'}
  for($index=0;$index -lt 11;$index++){
    $leftRow=$left[$index]
    $rightRow=$right[$index]
    $expectedName=$ExpectedNames[$index]
    if($leftRow.name -cne $expectedName -or $rightRow.name -cne $expectedName){throw "Three-run HDR stable content name mismatch: $index"}
    if($leftRow.path -cne $rightRow.path){throw "Three-run HDR stable content path mismatch: $index"}
    $leftHash=$leftRow.stableContentHash
    $rightHash=$rightRow.stableContentHash
    if($leftHash -notmatch '^[0-9a-f]{64}$' -or $rightHash -notmatch '^[0-9a-f]{64}$' -or $leftHash -cne $rightHash){throw "Three-run HDR stable content hash mismatch: $index"}
  }
}

function Assert-GameplayRunEvidence(
    $Receipt, $Environment, $Collision, $Palette, $Character, $Camera, $Sequence,
    $Photoreal, $PackageEvidence, [string[]]$ExpectedNames) {
  $viewExpectations=@(Get-GameplayCaptureViewExpectations)
  $godCoverageContract=Get-GameplayGodCoverageContract
  if($ExpectedNames.Count-ne$viewExpectations.Count){throw 'Gameplay typed view expectation count mismatch'}
  for($viewIndex=0;$viewIndex-lt$viewExpectations.Count;++$viewIndex){if($ExpectedNames[$viewIndex]-cne$viewExpectations[$viewIndex].name){throw "Gameplay typed view expectation name mismatch: index=$viewIndex"}}
  if ($Receipt.runId -notmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$') {
    throw 'Gameplay run identifier is invalid'
  }
  $attackHashes=@($Receipt.authoritativeAttackTrajectoryHashes);$trajectoryHashes=@([uint64]$Receipt.authoritativeTrajectoryHash)+@($attackHashes|ForEach-Object{[uint64]$_});$attackCounts=@($Receipt.authoritativeAttackCheckpointCounts)
  if($attackHashes.Count-ne3-or$trajectoryHashes.Count-ne4-or@($trajectoryHashes|Where-Object{$_-eq0}).Count-ne0-or@($trajectoryHashes|Select-Object -Unique).Count-ne4-or$attackCounts.Count-ne3-or@($attackCounts|Where-Object{$_-le0}).Count-ne0-or[uint64]$Receipt.authoritativeTrajectoryCheckpointCount-ne[uint64](($attackCounts|Measure-Object -Sum).Sum)){
    throw 'Gameplay authoritative trajectory hash/checkpoint receipt failed'
  }
  foreach ($typed in @($Environment, $Collision, $Palette, $Character, $Camera, $Sequence, $Photoreal)) {
    if ($typed.runId -ne $Receipt.runId) { throw 'Gameplay typed receipt run identifier mismatch' }
  }
  $armFields=@('firstPersonArmSubmeshCount','firstPersonArmTriangleCount','firstPersonLeftArmTriangleCount','firstPersonRightArmTriangleCount')
  foreach($sex in @('female','male')){
    $mainArms=$Receipt."${sex}FirstPersonArms";$typedArms=$Character."${sex}FirstPersonArms"
    foreach($field in $armFields){if([uint64]$mainArms.$field-lt1-or[uint64]$typedArms.$field-lt1-or[uint64]$mainArms.$field-ne[uint64]$typedArms.$field){throw "Gameplay $sex first-person arms main/typed receipt mismatch: $field"}}
    if([uint64]$mainArms.firstPersonArmTriangleCount-ne([uint64]$mainArms.firstPersonLeftArmTriangleCount+[uint64]$mainArms.firstPersonRightArmTriangleCount)){throw "Gameplay $sex first-person arms triangle accounting mismatch"}
  }
  if([uint64]$Receipt.femaleCharacterMaterialCount-lt1-or[uint64]$Receipt.maleCharacterMaterialCount-lt1-or
     [uint64]$Character.femaleCharacterMaterialCount-lt1-or[uint64]$Character.maleCharacterMaterialCount-lt1-or
     [uint64]$Receipt.femaleCharacterMaterialCount-ne[uint64]$Character.femaleCharacterMaterialCount-or
     [uint64]$Receipt.maleCharacterMaterialCount-ne[uint64]$Character.maleCharacterMaterialCount-or
     [uint64]$Receipt.characterMaterialCount-ne([uint64]$Receipt.femaleCharacterMaterialCount+[uint64]$Receipt.maleCharacterMaterialCount)){
    throw 'Gameplay female/male character material main/typed count mismatch'
  }
  $paletteReplay=@($Character.paletteReplayDiagnostics)
  if($paletteReplay.Count-ne3){throw 'Gameplay palette replay diagnostic count mismatch'}
  $fixedDelta=[single](1.0/60.0);$fixedDeltaBits=[BitConverter]::ToUInt32([BitConverter]::GetBytes($fixedDelta),0)
  $authorityCameraHash=[uint64]$paletteReplay[0].cameraHash;$authorityInputHash=[uint64]$paletteReplay[0].cameraAuthorityInputHash;$authorityCameraRaw=(@($paletteReplay[0].cameraRawBits)|ConvertTo-Json -Compress)
  for($replayIndex=0;$replayIndex-lt3;++$replayIndex){$row=$paletteReplay[$replayIndex];$pointer=@($row.normalizedPointer);$origin=@($row.rayOrigin);$direction=@($row.rayDirection);$hit=@($row.projectedHit);$target=@($row.snappedTarget);$cameraRaw=@($row.cameraRawBits);if($row.observed-ne1-or$row.inputSource-cne'FixedTickReplayAuthoritativeGodCamera'-or$row.liveSuppressed-ne1-or$row.deltaSource-cne'CaptureFixed60Hz'-or[single]$row.gameplayDelta-ne$fixedDelta-or[uint32]$row.gameplayDeltaRawBits-ne$fixedDeltaBits-or[uint64]$row.cameraHash-lt1-or[uint64]$row.cameraAuthorityInputHash-lt1-or[uint64]$row.cameraAuthorityInputHash-ne$authorityInputHash-or[uint64]$row.cameraHash-ne$authorityCameraHash-or$cameraRaw.Count-ne13-or($cameraRaw|ConvertTo-Json -Compress)-cne$authorityCameraRaw-or[uint64]$row.cameraProjectionMode-ne1-or$row.cameraProjectionModeName-cne'Orthographic'-or$row.tick-ne24+$replayIndex-or$pointer.Count-ne2-or$origin.Count-ne3-or$direction.Count-ne3-or$hit.Count-ne3-or$target.Count-ne3-or$row.hitValid-notin@(0,1)-or$row.previewActive-notin@(0,1)-or$row.previewValid-notin@(0,1)-or$row.undoDepth-lt18-or$row.redoDepth-lt0-or@($pointer+$origin+$direction+$hit+$target|Where-Object{[single]::IsNaN([single]$_)-or[single]::IsInfinity([single]$_)}).Count-or$pointer[0]-lt0-or$pointer[0]-gt1-or$pointer[1]-lt0-or$pointer[1]-gt1){throw "Gameplay palette replay typed diagnostic failed: tick=$($row.tick)"}}
  $placement=$paletteReplay[2];$rayOrigin=@($placement.rayOrigin);$rayDirection=@($placement.rayDirection);$projected=@($placement.projectedHit);$snapped=@($placement.snappedTarget)
  if($placement.hitValid-ne1-or$placement.previewValid-ne1-or$placement.placementRaySource-cne'ScreenPointToRayAuthoritativeGodCamera'-or$placement.placementPlaneSource-cne'WorldYPlane'-or$placement.placementGridSource-cne'ObjectPalette2mGrid'-or[uint32]$placement.gridStepRawBits-ne(Get-GameplayCanonicalSingleRawBits $placement.gridStep)-or[uint32]$placement.placementPlaneHeightRawBits-ne(Get-GameplayCanonicalSingleRawBits $placement.placementPlaneHeight)-or$placement.footPlacementCameraDistanceSource-cne'PreviousFixedCamera'-or[uint32]$placement.footPlacementCameraDistanceRawBits-ne(Get-GameplayCanonicalSingleRawBits $placement.footPlacementCameraDistance)-or[uint64]$placement.cameraSubjectPoseHash-lt1-or[math]::Abs([single]$rayDirection[1])-lt.0001){throw 'Gameplay authoritative God palette ray/plane/grid provenance failed'}
  $rayT=([single]$placement.placementPlaneHeight-[single]$rayOrigin[1])/[single]$rayDirection[1];$expectedProjected=@(([single]$rayOrigin[0]+[single]$rayDirection[0]*$rayT),([single]$placement.placementPlaneHeight),([single]$rayOrigin[2]+[single]$rayDirection[2]*$rayT));$grid=[single]$placement.gridStep;$expectedSnapped=@(([math]::Round($expectedProjected[0]/$grid,[MidpointRounding]::AwayFromZero)*$grid),([single]$placement.placementPlaneHeight+.5),([math]::Round($expectedProjected[2]/$grid,[MidpointRounding]::AwayFromZero)*$grid))
  for($axis=0;$axis-lt3;++$axis){if([math]::Abs([single]$projected[$axis]-[single]$expectedProjected[$axis])-gt.00001-or[math]::Abs([single]$snapped[$axis]-[single]$expectedSnapped[$axis])-gt.00001){throw "Gameplay authoritative God palette ray/plane/grid recompute failed: axis=$axis"}}
  Assert-GameplayOrderedCaptureNames $Character.captures $ExpectedNames 'character'
  Assert-GameplayOrderedCaptureNames $Camera.captures $ExpectedNames 'camera'
  Assert-GameplayOrderedCaptureNames $Sequence.captures $ExpectedNames 'sequence'
  Assert-GameplayOrderedCaptureNames $Photoreal.captures $ExpectedNames 'photoreal'
  $expectedFirstNames=@($viewExpectations|Where-Object{$_.camera-eq'first'}|ForEach-Object{$_.name})
  $viewmodels=@($Character.firstPersonViewmodels)
  if($viewmodels.Count-ne$expectedFirstNames.Count-or@($viewmodels.name|Select-Object -Unique).Count-ne$expectedFirstNames.Count){throw 'Gameplay first-person viewmodel evidence count/uniqueness mismatch'}
  for($viewmodelIndex=0;$viewmodelIndex-lt$expectedFirstNames.Count;++$viewmodelIndex){
    $viewmodel=$viewmodels[$viewmodelIndex];$armsIdentity=@($viewmodel.armsIdentity);$weaponIdentity=@($viewmodel.weaponIdentity);$armsBounds=@($viewmodel.armsBounds);$weaponBounds=@($viewmodel.weaponBounds)
    if($viewmodel.name-cne$expectedFirstNames[$viewmodelIndex]-or$armsIdentity.Count-ne3-or$weaponIdentity.Count-ne3-or
       [string]$armsIdentity[0]-ne'12592142912428528129'-or[uint64]$armsIdentity[1]-ne3-or[uint64]$armsIdentity[2]-ne1-or
       [string]$weaponIdentity[0]-ne'12592142912428528129'-or[uint64]$weaponIdentity[1]-ne4-or[uint64]$weaponIdentity[2]-ne1-or
       [uint64]$viewmodel.armsToken-lt1-or[uint64]$viewmodel.weaponToken-lt1-or[uint64]$viewmodel.armsToken-eq[uint64]$viewmodel.weaponToken-or
       [uint64]$viewmodel.armsPixels-lt1-or[uint64]$viewmodel.weaponPixels-lt1-or$armsBounds.Count-ne4-or$weaponBounds.Count-ne4){throw "Gameplay first-person viewmodel identity/token evidence failed: $($viewmodel.name)"}
    # 手臂的肩部切口必須藏在右／下畫面外；武器本體則必須維持四邊完整安全距離。
    if([uint64]$armsBounds[0]-lt640-or[uint64]$armsBounds[1]-lt66-or[uint64]$armsBounds[2]-gt1279-or[uint64]$armsBounds[3]-gt719-or[uint64]$armsBounds[0]-ge[uint64]$armsBounds[2]-or[uint64]$armsBounds[1]-ge[uint64]$armsBounds[3]){throw "Gameplay first-person arms edge-entry evidence failed: $($viewmodel.name)"}
    if([uint64]$weaponBounds[0]-lt16-or[uint64]$weaponBounds[1]-lt32-or[uint64]$weaponBounds[2]-gt1264-or[uint64]$weaponBounds[3]-gt704-or[uint64]$weaponBounds[0]-ge[uint64]$weaponBounds[2]-or[uint64]$weaponBounds[1]-ge[uint64]$weaponBounds[3]){throw "Gameplay first-person weapon screen-margin evidence failed: $($viewmodel.name)"}
  }
  if ($Receipt.captures -ne $ExpectedNames.Count -or $Photoreal.expectedCaptures -ne $ExpectedNames.Count -or
      $Photoreal.actualCaptureIndex -ne $ExpectedNames.Count -or
      $Photoreal.completedCaptures -ne $ExpectedNames.Count -or
      $Photoreal.uniqueCaptureNames -ne $ExpectedNames.Count) {
    throw 'Gameplay actual/typed capture count mismatch'
  }
  if ($Photoreal.source -ne 'GameplayPhotorealTyped' -or
      $Photoreal.manifestSha256 -ne $PackageEvidence.manifestSha256 -or
      $Photoreal.payloadFiles -ne 96 -or $Photoreal.validatedPayloadFiles -ne 96 -or
      $Photoreal.profileAssets -ne 12 -or
      $Photoreal.visualFirReplacements -ne 46 -or
      $Photoreal.materials -ne 18 -or $Photoreal.validatedMaterials -ne 18 -or
      $Photoreal.meshParts -ne 13 -or $Photoreal.validatedMeshParts -ne 13 -or
      $Photoreal.validatedReferencedFiles -ne 96) {
    throw 'Gameplay typed manifest SHA/exact-count package cross-check failed'
  }
  if ($Photoreal.objectIdSupported -ne 1 -or $Photoreal.objectIdRequested -ne 1 -or
      $Photoreal.objectIdPass -ne 1 -or $Photoreal.semanticPass -ne 1 -or
      $Photoreal.depthCaptureSupported -ne 1 -or $Photoreal.depthPass -ne 1 -or
      $Photoreal.shadowPass -ne 1 -or $Photoreal.lodPass -ne 1 -or
      $Photoreal.shadowCasters -lt $ExpectedNames.Count -or
      $Photoreal.shadowDrawCalls -lt $ExpectedNames.Count -or
      $Photoreal.shadowCascadeDrawCalls -lt $ExpectedNames.Count -or
      $Photoreal.lodInstances -lt 1 -or $Photoreal.lodTransitions -lt 1 -or
      $Photoreal.lodDualSubmissions -lt 1 -or $Photoreal.lodBudgetFallbacks -ne 0) {
    throw 'Gameplay typed semantic/shadow/LOD evidence failed'
  }
  $expectedWorld = [uint64]$Environment.terrainPatches + [uint64]$Environment.waterSegments + [uint64]$Environment.treeInstances + [uint64]$Environment.grassInstances + [uint64]$Environment.rockInstances + [uint64]$Environment.buildingComponents
  $expectedSolid = [uint64]$Environment.terrainPatches + [uint64]$Environment.treeInstances + [uint64]$Environment.rockInstances + [uint64]$Environment.buildingComponents
  $expectedNonSolid = [uint64]$Environment.waterSegments + [uint64]$Environment.grassInstances
  if ($Environment.source -ne 'GameplayShowcaseEnvironment3D' -or $Environment.worldHash -eq 0 -or
      $Environment.descriptors -ne $expectedWorld -or $expectedWorld -gt 480 -or $Environment.submittedDescriptors -ne $expectedWorld -or
      $Environment.executionObserved -ne 1 -or @($Environment.counterDeltas).Count -ne 10 -or
      @($Environment.counterDeltas | Where-Object { $_ -le 0 }).Count -ne 0 -or
      $Environment.releasedDescriptors -ne $expectedWorld -or $Environment.retainedBytesAfterRelease -ne 0) {
    throw 'Gameplay environment typed execution/release evidence failed'
  }
  if ($Collision.source -ne 'EnvironmentCollisionStreaming3D' -or $Collision.stateHash -eq 0 -or
      $Collision.descriptors -ne $expectedWorld -or $Collision.solidDescriptors -ne $expectedSolid -or
      $Collision.nonSolidDescriptors -ne $expectedNonSolid -or $Collision.solidDescriptors + $Collision.nonSolidDescriptors -ne $expectedWorld -or $Collision.activePartitions -ne 1 -or
      $Collision.activeColliders -ne $expectedSolid -or $Collision.activePhysicsBodies -ne $expectedSolid -or
      $Collision.activations -lt 2 -or $Collision.deactivations -lt 1 -or
      $Collision.failedActivations -ne 0 -or $Collision.rollbacks -ne 0 -or
      $Collision.stableIdEvidence -ne 1 -or $Collision.rayEvidence -ne 1 -or
      $Collision.meleeQueryEvidence -ne 1 -or $Collision.reloadStable -ne 1 -or
      $Collision.releasedDescriptors -ne $expectedWorld -or $Collision.releasedColliders -ne $expectedSolid -or
      $Collision.releasedPhysicsBodies -ne $expectedSolid -or $Collision.retainedBytesAfterRelease -ne 0 -or
      $Collision.activeCollidersAfterRelease -ne 0 -or $Collision.activePhysicsBodiesAfterRelease -ne 0) {
    throw 'Gameplay collision typed execution/release evidence failed'
  }
  if ($Palette.source -ne 'GameplayObjectPaletteUi3D' -or $Palette.categories -ne 6 -or
      $Palette.drawnFrames -lt 11 -or $Palette.pointerDowns -lt 1 -or $Palette.drags -lt 1 -or
      $Palette.previews -lt 1 -or $Palette.commits -lt 1 -or $Palette.deterministicHash -eq 0) {
    throw 'Gameplay palette typed transaction evidence failed'
  }
  for ($index = 0; $index -lt $ExpectedNames.Count; ++$index) {
    $capture = @($Character.captures)[$index]
    $cameraCapture = @($Camera.captures)[$index]
    $sequenceCapture = @($Sequence.captures)[$index]
    $photorealCapture = @($Photoreal.captures)[$index]
    $viewExpectation=$viewExpectations[$index]
    $expectedCharacter=if($index-lt5-or$index-eq10){'female-f2'}else{'male-m2'}
    $expectedArms=if($expectedCharacter-eq'female-f2'){$Character.femaleFirstPersonArms}else{$Character.maleFirstPersonArms}
    $first=$viewExpectation.camera-eq'first'
    $third = $ExpectedNames[$index] -like 'third-*' -or $ExpectedNames[$index] -in @('palette-building.png', 'melee-combat.png')
    $horizonFields=@('topCenterRayElevation','minimumTopCenterRayElevation','rejectedHorizon')
    foreach($field in $horizonFields){if($null-eq$capture.PSObject.Properties[$field]-or$null-eq$cameraCapture.PSObject.Properties[$field]-or$capture.$field-ne$cameraCapture.$field){throw "Gameplay character/camera analytic horizon provenance mismatch: $($capture.name) $field"};if($capture.$field-isnot[ValueType]-or$capture.$field-is[bool]-or$cameraCapture.$field-isnot[ValueType]-or$cameraCapture.$field-is[bool]){throw "Gameplay analytic horizon field is not numeric: $($capture.name) $field"}}
    foreach($field in @('topCenterRayElevation','minimumTopCenterRayElevation')){$bitsField="${field}RawBits";if($null-eq$capture.PSObject.Properties[$bitsField]-or$null-eq$cameraCapture.PSObject.Properties[$bitsField]-or[uint32]$capture.$bitsField-ne[uint32]$cameraCapture.$bitsField){throw "Gameplay character/camera analytic horizon raw-bit provenance mismatch: $($capture.name) $field"};$value=[single]$capture.$field;$valueBits=[BitConverter]::ToUInt32([BitConverter]::GetBytes($value),0);if([single]::IsNaN($value)-or[single]::IsInfinity($value)-or$valueBits-ne[uint32]$capture.$bitsField){throw "Gameplay analytic horizon text/raw-bit round-trip mismatch: $($capture.name) $field"}}
    $actualHorizon=[single]$capture.topCenterRayElevation;$minimumHorizon=[single]$capture.minimumTopCenterRayElevation;$rejectedHorizon=[double]$capture.rejectedHorizon
    $minimumHorizonContract=[double][single]0.03;$minimumHorizonBits=[BitConverter]::ToUInt32([BitConverter]::GetBytes([single]0.03),0)
    if($third){if([double]::IsNaN($actualHorizon)-or[double]::IsInfinity($actualHorizon)-or[double]::IsNaN($minimumHorizon)-or[double]::IsInfinity($minimumHorizon)-or$minimumHorizon-lt$minimumHorizonContract-or[uint32]$capture.minimumTopCenterRayElevationRawBits-ne$minimumHorizonBits-or$actualHorizon-lt$minimumHorizon-or[double]::IsNaN($rejectedHorizon)-or[double]::IsInfinity($rejectedHorizon)-or$rejectedHorizon-lt1-or$rejectedHorizon-ne[Math]::Floor($rejectedHorizon)){throw "Gameplay third-person analytic horizon gate failed: $($capture.name) actual=$actualHorizon minimum=$minimumHorizon rejected=$rejectedHorizon"}}
    elseif($actualHorizon-ne0-or$minimumHorizon-ne0-or$rejectedHorizon-ne0){throw "Gameplay non-third capture carried analytic horizon evidence: $($capture.name)"}
    if($capture.camera-ne$viewExpectation.camera-or$capture.character-ne$expectedCharacter-or$capture.weaponMaterialRanges-ne2-or
       [double]::IsNaN([double]$capture.weaponAnchorError)-or[double]::IsInfinity([double]$capture.weaponAnchorError)-or$capture.weaponAnchorError-lt0-or$capture.weaponAnchorError-gt.001){throw "Gameplay character material/socket capture gate failed: $($capture.name)"}
    if($first){
      foreach($field in $armFields){if([uint64]$capture.$field-lt1-or[uint64]$capture.$field-ne[uint64]$expectedArms.$field){throw "Gameplay first-person arms capture typed count mismatch: $($capture.name) $field"}}
      if([uint64]$capture.bodyMaterialSlots-ne[uint64]$expectedArms.firstPersonArmSubmeshCount){throw "Gameplay first-person arms material range mismatch: $($capture.name)"}
    }else{
      foreach($field in $armFields){if([uint64]$capture.$field-ne0){throw "Gameplay non-first capture carried first-person arms evidence: $($capture.name) $field"}}
      $expectedMaterialCount=if($expectedCharacter-eq'female-f2'){if($null-ne$Character.femaleBodyMaterialSlotCount){$Character.femaleBodyMaterialSlotCount}else{$Character.femaleCharacterMaterialCount}}else{if($null-ne$Character.maleBodyMaterialSlotCount){$Character.maleBodyMaterialSlotCount}else{$Character.maleCharacterMaterialCount}}
      if([uint64]$expectedMaterialCount-lt1-or[uint64]$capture.bodyMaterialSlots-ne[uint64]$expectedMaterialCount){throw "Gameplay non-first character material range mismatch: $($capture.name)"}
    }
    if ($capture.objectIdOpaqueComplete -ne 1 -or $capture.objectIdMaskedComplete -ne 1 -or
        $capture.objectIdSkinnedComplete -ne 1 -or $capture.depthCaptureSupported -ne 1 -or
        $capture.depthGatePass -ne 1 -or $capture.depthGeneration -lt 1 -or
        $capture.rawDepthSha256 -notmatch '^[0-9a-f]{64}$' -or
        $capture.effectiveDepthSha256 -notmatch '^[0-9a-f]{64}$' -or
        $capture.rawDepthPixelSha256 -cne $capture.rawDepthSha256 -or
        $capture.effectiveDepthPixelSha256 -cne $capture.effectiveDepthSha256 -or
        $capture.rawDepthFinalFileSha256 -notmatch '^[0-9a-f]{64}$' -or
        $capture.effectiveDepthFinalFileSha256 -notmatch '^[0-9a-f]{64}$' -or
        $capture.rawDepthFinalFileSha256 -match '^0{64}$' -or
        $capture.effectiveDepthFinalFileSha256 -match '^0{64}$' -or
        [uint64]$capture.rawDepthFinalFileBytes -lt 1 -or [uint64]$capture.effectiveDepthFinalFileBytes -lt 1 -or
        $capture.feetContactPassed -ne 1 -or $capture.groundSemantic -ne 'terrain') {
      throw "Gameplay per-view semantic/depth/contact evidence failed: $($capture.name)"
    }
    if ($capture.rawFemalePoseHash -eq 0 -or $capture.rawMalePoseHash -eq 0 -or
        $capture.rawMonsterPoseHash -eq 0 -or $capture.rawFootPlacementHash -eq 0 -or
        $capture.rawFemalePoseHash -ne $capture.effectiveFemalePoseHash -or
        $capture.rawMalePoseHash -ne $capture.effectiveMalePoseHash -or
        $capture.rawMonsterPoseHash -ne $capture.effectiveMonsterPoseHash -or
        $capture.rawFootPlacementHash -ne $capture.effectiveFootPlacementHash) {
      throw "Gameplay per-view frozen pose evidence failed: $($capture.name)"
    }
    $entityDiagnostics=@($capture.captureStateEntityDiagnostics)
    if($capture.captureStateValid-ne1-or$capture.captureStateEntityCount-lt1-or$entityDiagnostics.Count-ne$capture.captureStateEntityCount){throw "Gameplay canonical entity diagnostic count/validity failed: $($capture.name)"}
    $previousStableId=[uint64]0
    foreach($entity in $entityDiagnostics){
      $position=@($entity.position);$positionBits=@($entity.positionRawBits);$bodyPosition=@($entity.bodyPosition);$bodyVelocity=@($entity.bodyVelocity);$scenePosition=@($entity.scenePosition)
      if([uint64]$entity.stableId-le$previousStableId-or$entity.source-notin@('InitialSeed','PaletteReplay')-or$entity.category-lt0-or$entity.category-gt5-or$entity.dynamic-notin@(0,1)-or$entity.alive-notin@(0,1)-or$position.Count-ne3-or$positionBits.Count-ne3-or$entity.topologyHash-eq0-or$entity.transformHash-eq0-or$entity.rotationRawBits-lt0){throw "Gameplay canonical entity typed identity failed: $($capture.name) id=$($entity.stableId)"}
      for($component=0;$component-lt3;++$component){$value=[single]$position[$component];$bits=[BitConverter]::ToUInt32([BitConverter]::GetBytes($value),0);if([single]::IsNaN($value)-or[single]::IsInfinity($value)-or$bits-ne[uint32]$positionBits[$component]-or($value-eq0-and$bits-ne0)){throw "Gameplay canonical entity position bits failed: $($capture.name) id=$($entity.stableId) component=$component"}}
      $rotation=[single]$entity.rotation;$rotationBits=[BitConverter]::ToUInt32([BitConverter]::GetBytes($rotation),0);if([single]::IsNaN($rotation)-or[single]::IsInfinity($rotation)-or$rotationBits-ne[uint32]$entity.rotationRawBits-or($rotation-eq0-and$rotationBits-ne0)){throw "Gameplay canonical entity rotation bits failed: $($capture.name) id=$($entity.stableId)"}
      foreach($relation in @(@($entity.bodyAvailable,$bodyPosition+$bodyVelocity),@($entity.sceneAvailable,$scenePosition))){$invalidRuntimeValues=@(@($relation[1])|Where-Object{[single]::IsNaN([single]$_)-or[single]::IsInfinity([single]$_)});if($relation[0]-notin@(0,1)-or($relation[0]-eq1-and$invalidRuntimeValues.Count)){throw "Gameplay canonical entity runtime transform diagnostic failed: $($capture.name) id=$($entity.stableId)"}}
      $previousStableId=[uint64]$entity.stableId
    }
    $expectedPhases = if ($third) { 3 } else { 2 }
    if ($sequenceCapture.complete -ne 1 -or $sequenceCapture.expectedPhaseCount -ne $expectedPhases -or
        @($sequenceCapture.phases).Count -ne $expectedPhases -or $sequenceCapture.token -eq 0 -or
        $sequenceCapture.jitterPolicy -ne 1 -or $sequenceCapture.fixedSampleIndex -ne 0) {
      throw "Gameplay sequence completeness evidence failed: $($capture.name)"
    }
    Assert-GameplayReadbackGpuProvenance $sequenceCapture
    $firstPhase = @($sequenceCapture.phases)[0]
    for ($phaseIndex = 0; $phaseIndex -lt $expectedPhases; ++$phaseIndex) {
      $phase = @($sequenceCapture.phases)[$phaseIndex]
      if ($phase.index -ne $phaseIndex -or $phase.sequenceIndex -ne $phaseIndex -or
          $phase.sequenceToken -ne $sequenceCapture.token -or $phase.drawListRevision -eq 0 -or
          $phase.cameraHash -eq 0 -or $phase.stateHash -eq 0 -or $phase.invariantHash -eq 0 -or
          $phase.frameSerial -eq 0 -or $phase.viewProjectionHash -eq 0 -or
          $phase.tokenGeneration -eq 0 -or $phase.historyGeneration -eq 0 -or
          $phase.excludingOpacity -eq 0 -or $phase.includingOpacity -eq 0 -or $phase.identityOrder -eq 0 -or
          $phase.cameraHash -ne $firstPhase.cameraHash -or $phase.invariantHash -ne $firstPhase.invariantHash -or
          $phase.viewProjectionHash -ne $firstPhase.viewProjectionHash -or $phase.jitterIndex -ne $firstPhase.jitterIndex -or
          $phase.jitterX -ne $firstPhase.jitterX -or $phase.jitterY -ne $firstPhase.jitterY -or
          $phase.historyGeneration -ne $firstPhase.historyGeneration -or $phase.historyIndex -ne $firstPhase.historyIndex -or
          $phase.excludingOpacity -ne $firstPhase.excludingOpacity -or $phase.identityOrder -ne $firstPhase.identityOrder) {
        throw "Gameplay sequence phase/provenance evidence failed: $($capture.name) phase=$phaseIndex"
      }
      if ($phaseIndex -gt 0) {
        $previous = @($sequenceCapture.phases)[$phaseIndex - 1]
        if ($phase.frameSerial -le $previous.frameSerial -or $phase.tokenGeneration -le $previous.tokenGeneration) {
          throw "Gameplay sequence monotonic evidence failed: $($capture.name) phase=$phaseIndex"
        }
      }
    }
    $expectedCamera = $viewExpectation.camera
    $expectedLighting = $viewExpectation.lighting
    $projectionFields=@('projectionMode','projectionModeValue','verticalFovRadians','orthographicHeight','godActualTriangleCoverage','godVisibleWorldDescriptors','godBuildingUnion','godMaximumBuildingPart','godRoofCoverage')
    foreach($field in $projectionFields){if($null-eq$cameraCapture.PSObject.Properties[$field]-or$null-eq$photorealCapture.PSObject.Properties[$field]-or$cameraCapture.$field-ne$photorealCapture.$field){throw "Gameplay camera/photoreal projection provenance mismatch: $($capture.name) $field"}}
    foreach($field in @('verticalFovRadians','orthographicHeight','godActualTriangleCoverage','godVisibleWorldDescriptors','godBuildingUnion','godMaximumBuildingPart','godRoofCoverage')){if($null-eq$photorealCapture.PSObject.Properties[$field]-or$photorealCapture.$field-isnot[ValueType]-or$photorealCapture.$field-is[bool]){throw "Gameplay projection/composition field is not numeric: $($capture.name) $field"}}
    if($null-eq$cameraCapture.PSObject.Properties['godLodTransitions']-or$cameraCapture.godLodTransitions-isnot[ValueType]-or$cameraCapture.godLodTransitions-is[bool]){throw "Gameplay camera actual God LOD field missing/non-numeric: $($capture.name)"}
    $photorealGodLodFields=@('godPredictedLodTransitions','godActualLodTransitions','godActualLodDualSubmissions','godLodPredictionMatched','godLodContractMissing','godLodContractExtra','godLodContractWrongLevel','godLodContractWrongBlend')
    foreach($field in $photorealGodLodFields){if($null-eq$photorealCapture.PSObject.Properties[$field]-or$photorealCapture.$field-isnot[ValueType]-or$photorealCapture.$field-is[bool]){throw "Gameplay Photoreal typed God LOD field missing/non-numeric: $($capture.name) $field"}}
    if($expectedCamera-eq'god'-and[uint64]$cameraCapture.godLodTransitions-ne[uint64]$photorealCapture.godActualLodTransitions){throw "Gameplay Camera actual/Photoreal actual God LOD mapping mismatch: $($capture.name)"}
    $verticalFov=[double]$photorealCapture.verticalFovRadians;$orthographicHeight=[double]$photorealCapture.orthographicHeight
    if($expectedCamera-eq'god'){
      if($photorealCapture.projectionMode-cne'Orthographic'-or[uint64]$photorealCapture.projectionModeValue-ne1-or[double]::IsNaN($orthographicHeight)-or[double]::IsInfinity($orthographicHeight)-or$orthographicHeight-lt4-or$orthographicHeight-gt180){throw "Gameplay production God orthographic projection evidence failed: $($capture.name)"}
    }elseif($photorealCapture.projectionMode-cne'Perspective'-or[uint64]$photorealCapture.projectionModeValue-ne0-or[double]::IsNaN($verticalFov)-or[double]::IsInfinity($verticalFov)-or$verticalFov-le.1-or$verticalFov-ge3-or[double]::IsNaN($orthographicHeight)-or[double]::IsInfinity($orthographicHeight)-or$orthographicHeight-ne0){
      throw "Gameplay production first/third perspective projection evidence failed: $($capture.name)"
    }
    foreach($field in @('godActualTriangleCoverage','godBuildingUnion','godMaximumBuildingPart','godRoofCoverage')){$number=[double]$photorealCapture.$field;if([double]::IsNaN($number)-or[double]::IsInfinity($number)){throw "Gameplay God composition field is non-finite: $($capture.name) $field"}}
    if($expectedCamera-eq'god'){
      # These values originate as float telemetry.  Keep the authored composition
      # limits exact while tolerating only float-to-double representation noise.
      $godCompositionEpsilon=1e-6
      if([double]$photorealCapture.godActualTriangleCoverage-lt$godCoverageContract.coverage-or[uint64]$photorealCapture.godVisibleWorldDescriptors-lt20-or[double]$photorealCapture.godBuildingUnion-lt0-or[double]$photorealCapture.godBuildingUnion-gt(.22+$godCompositionEpsilon)-or[double]$photorealCapture.godMaximumBuildingPart-lt0-or[double]$photorealCapture.godMaximumBuildingPart-gt(.08+$godCompositionEpsilon)-or[double]$photorealCapture.godRoofCoverage-lt0-or[double]$photorealCapture.godRoofCoverage-gt(.05+$godCompositionEpsilon)-or[uint64]$photorealCapture.godActualLodTransitions-lt1-or[uint64]$photorealCapture.godPredictedLodTransitions-lt1-or[uint64]$photorealCapture.godPredictedLodTransitions-ne[uint64]$photorealCapture.godActualLodTransitions-or[uint64]$photorealCapture.godActualLodDualSubmissions-ne[uint64]$photorealCapture.godActualLodTransitions-or[uint64]$photorealCapture.godLodPredictionMatched-ne1-or[uint64]$photorealCapture.godLodContractMissing-ne0-or[uint64]$photorealCapture.godLodContractExtra-ne0-or[uint64]$photorealCapture.godLodContractWrongLevel-ne0-or[uint64]$photorealCapture.godLodContractWrongBlend-ne0){throw "Gameplay God production composition/predicted-actual LOD evidence failed: $($capture.name)"}
    }elseif([double]$photorealCapture.godActualTriangleCoverage-ne0-or[uint64]$photorealCapture.godVisibleWorldDescriptors-ne0-or[double]$photorealCapture.godBuildingUnion-ne0-or[double]$photorealCapture.godMaximumBuildingPart-ne0-or[double]$photorealCapture.godRoofCoverage-ne0-or[uint64]$cameraCapture.godLodTransitions-ne0-or[uint64]$photorealCapture.godPredictedLodTransitions-ne0-or[uint64]$photorealCapture.godActualLodTransitions-ne[uint64]$photorealCapture.lodTransitions-or[uint64]$photorealCapture.godActualLodDualSubmissions-ne[uint64]$photorealCapture.lodDualSubmissions-or[uint64]$photorealCapture.godLodPredictionMatched-ne1-or[uint64]$photorealCapture.godLodContractMissing-ne0-or[uint64]$photorealCapture.godLodContractExtra-ne0-or[uint64]$photorealCapture.godLodContractWrongLevel-ne0-or[uint64]$photorealCapture.godLodContractWrongBlend-ne0){throw "Gameplay non-God capture carried God composition/LOD evidence or mismatched actual LOD counters: $($capture.name)"}
    $expectedRequiredCount = 2
    $expectedForbiddenCount = if ($expectedCamera -eq 'first') { 2 } else { 1 }
    $required = @($photorealCapture.requiredIdentities)
    $forbidden = @($photorealCapture.forbiddenIdentities)
    $activeFingerprints = @($photorealCapture.shadowCascadeFingerprints | Select-Object -First ([int]$photorealCapture.shadowActiveCascades))
    $requestedPath = [string]$photorealCapture.requestedPath; $committedPath = [string]$photorealCapture.committedPath
    $pathsNormalized = -not [string]::IsNullOrWhiteSpace($requestedPath) -and -not [string]::IsNullOrWhiteSpace($committedPath) -and [IO.Path]::IsPathRooted($requestedPath) -and [IO.Path]::IsPathRooted($committedPath) -and $requestedPath -notmatch '(^|[\\/])\.\.([\\/]|$)' -and [IO.Path]::GetFullPath($requestedPath) -ceq [IO.Path]::GetFullPath($committedPath)
    if ($photorealCapture.camera -ne $expectedCamera -or $photorealCapture.lighting -ne $expectedLighting -or
        $photorealCapture.requestedName -cne $ExpectedNames[$index] -or $photorealCapture.committedName -cne $ExpectedNames[$index] -or
        -not $pathsNormalized -or $requestedPath -cne $committedPath -or [IO.Path]::GetFileName($requestedPath) -cne $ExpectedNames[$index] -or
        $photorealCapture.requiredIdentityCount -ne $expectedRequiredCount -or
        $photorealCapture.requiredMatched -ne $expectedRequiredCount -or
        $photorealCapture.forbiddenIdentityCount -ne $expectedForbiddenCount -or $photorealCapture.forbiddenMatched -ne 0 -or
        $photorealCapture.generationConflicts -ne 0 -or $photorealCapture.semanticPass -ne 1 -or $required.Count -ne $expectedRequiredCount -or
        $forbidden.Count -ne $expectedForbiddenCount -or
        $photorealCapture.effectiveFrameSerial -ne @($sequenceCapture.phases)[1].frameSerial -or
        $photorealCapture.semanticFrameSerial -ne $photorealCapture.effectiveFrameSerial -or
        $photorealCapture.sceneSubmissionSerial -lt 1 -or $photorealCapture.sceneDrawRevision -ne @($sequenceCapture.phases)[1].drawListRevision -or $photorealCapture.sceneIdentityOrderFingerprint -lt 1 -or
        $photorealCapture.shadowFrameSerial -ne $photorealCapture.effectiveFrameSerial -or
        $photorealCapture.shadowResourceGeneration -lt 1 -or $activeFingerprints.Count -ne $photorealCapture.shadowActiveCascades -or @($activeFingerprints | Where-Object { $_ -eq 0 }).Count -ne 0 -or
        $photorealCapture.shadowCasters -lt 1 -or $photorealCapture.shadowDrawCalls -lt 1 -or
        $photorealCapture.shadowCascadeDrawCalls -lt 1 -or $photorealCapture.shadowActiveCascades -lt 1 -or
        $photorealCapture.shadowCascadeRendered + $photorealCapture.shadowCascadeReused -lt $photorealCapture.shadowActiveCascades -or
        $photorealCapture.shadowPass -ne 1 -or
        $photorealCapture.lodFrameSerial -ne $photorealCapture.effectiveFrameSerial -or
        $photorealCapture.lodInstances -lt 1 -or $photorealCapture.lodBudgetFallbacks -ne 0 -or
        ($viewExpectation.requireLodTransition -and ($photorealCapture.lodTransitions -lt 1 -or $photorealCapture.lodDualSubmissions -lt 1)) -or
        (-not $viewExpectation.requireLodTransition -and ($photorealCapture.lodTransitions -ne 0 -or $photorealCapture.lodDualSubmissions -ne 0)) -or
        $photorealCapture.lodPass -ne 1) {
      throw "Gameplay photoreal per-view semantic/shadow/LOD evidence failed: $($capture.name)"
    }
    for($identityIndex=0;$identityIndex-lt$required.Count;++$identityIndex){if($required[$identityIndex].nonzeroPixels-lt1-or$required[$identityIndex].coverage-le0-or$required[$identityIndex].coverage-gt1-or$required[$identityIndex].generation-ne1){throw "Gameplay required identity pixel/generation evidence failed: $($capture.name)"}}
    if($expectedCamera-eq'first'){
      if([string]$required[0].high-ne'12592142912428528129'-or$required[0].low-ne3-or[string]$required[1].high-ne'12592142912428528129'-or$required[1].low-ne4-or
         [string]$forbidden[0].high-ne'12592142912428528129'-or$forbidden[0].low-ne3-or$forbidden[0].generation-ne2-or[string]$forbidden[1].high-ne'12592142912428528129'-or$forbidden[1].low-ne4-or$forbidden[1].generation-ne2){throw "Gameplay first-person arms/weapon semantic identity evidence failed: $($capture.name)"}
      $viewmodelMatch=@($viewmodels|Where-Object{$_.name-ceq$capture.name})
      if($viewmodelMatch.Count-ne1-or[uint64]$viewmodelMatch[0].armsPixels-ne[uint64]$required[0].nonzeroPixels-or[uint64]$viewmodelMatch[0].weaponPixels-ne[uint64]$required[1].nonzeroPixels){throw "Gameplay first-person viewmodel/readback semantic-pixel cross-check failed: $($capture.name)"}
    }elseif([string]$required[0].high-ne'12592142912428528129'-or$required[0].low-ne1-or[uint64]$required[1].high-ne[uint64](Get-GameplayObjectIdentityContract).environmentHigh-or$required[1].low-ne1-or([uint64]$required[1].high-band[uint64](Get-GameplayObjectIdentityContract).hlodProxyBit)-ne0-or[string]$forbidden[0].high-ne'12592142912428528129'-or$forbidden[0].low-ne1-or$forbidden[0].generation-ne2){throw "Gameplay world character/terrain semantic identity evidence failed: $($capture.name)"}
    if($expectedCamera-eq'god'-and([uint64]$required[0].nonzeroPixels-lt$godCoverageContract.minimumPixels-or[double]$required[0].coverage-lt$godCoverageContract.coverage)){throw "Gameplay God primary semantic triangle coverage evidence failed: $($capture.name)"}
    if ($third -and ($capture.characterReferenceSupported -ne 1 -or
        $capture.referenceDepthSha256 -notmatch '^[0-9a-f]{64}$' -or
        $capture.referenceDepthPixelSha256 -cne $capture.referenceDepthSha256 -or
        $capture.referenceDepthFinalFileSha256 -notmatch '^[0-9a-f]{64}$' -or
        $capture.referenceDepthFinalFileSha256 -match '^0{64}$' -or
        [uint64]$capture.referenceDepthFinalFileBytes -lt 1 -or
        $capture.gpuFeetContactPassed -ne 1 -or
        $capture.contactArtifactSha256 -notmatch '^[0-9a-f]{64}$' -or
        $capture.contactArtifactSha256 -match '^0{64}$' -or
        $capture.contactLeftFootPixels -lt 1 -or $capture.contactRightFootPixels -lt 1 -or
        $capture.contactGroundSupportPixels -lt 1 -or $capture.contactShadowPixels -lt 1 -or
        $capture.contactLeftFootPixels + $capture.contactRightFootPixels + $capture.contactGroundSupportPixels + $capture.contactShadowPixels -ge 46080 -or
        $capture.contactFrameSerial -lt 1 -or $capture.contactFrameSerial -ne @($sequenceCapture.phases)[1].frameSerial -or
        $capture.contactPoseHash -eq 0 -or $capture.contactPoseHash -ne $capture.effectiveFootPlacementHash -or
        $capture.rawFemalePoseHash -ne $capture.referenceFemalePoseHash -or
        $capture.rawMalePoseHash -ne $capture.referenceMalePoseHash -or
        $capture.rawMonsterPoseHash -ne $capture.referenceMonsterPoseHash -or
        $capture.rawFootPlacementHash -ne $capture.referenceFootPlacementHash -or
        $capture.requestDrawRevision -eq 0 -or $capture.requestCameraHash -eq 0 -or
        $capture.requestStateHash -eq 0 -or $capture.captureStateSchemaVersion -ne 2 -or
        $capture.captureStateCanonical -ne $capture.requestStateHash -or $capture.captureStateLegacy -eq 0 -or
        $capture.captureStateEntities -eq 0 -or $capture.captureStateTopology -eq 0 -or $capture.captureStateTransform -eq 0 -or
        $capture.captureStateController -eq 0 -or $capture.captureStateYawPitch -eq 0 -or $capture.captureStateRules -eq 0 -or
        $capture.captureStateCamera -eq 0 -or $capture.captureStatePalette -eq 0 -or $capture.captureStateAnimation -eq 0 -or $capture.captureStatePhysics -eq 0 -or
        $capture.rawAcceptedExcludingOpacity -ne $capture.effectiveAcceptedExcludingOpacity -or
        $capture.rawIdentityOrderHash -ne $capture.effectiveIdentityOrderHash -or
        $capture.referenceAcceptedExcludingOpacity -ne $capture.effectiveAcceptedExcludingOpacity -or
        $capture.referenceIdentityOrderHash -ne $capture.effectiveIdentityOrderHash -or
        @($capture.rawBuildingCoverage151To156).Count -ne 6 -or
        @($capture.effectiveBuildingCoverage151To156).Count -ne 6 -or
        $capture.characterReferenceCoverage -lt 0.02 -or $capture.characterReferenceCoverage -gt 0.20 -or
        $capture.characterVisibility -lt 0.995 -or $capture.characterSilhouetteIou -lt 0.995 -or
        $cameraCapture.builderValid -ne 1 -or $cameraCapture.policyValid -ne 1 -or
        $cameraCapture.matrixExact -ne 1 -or $cameraCapture.candidateLow -eq 0 -or
        $cameraCapture.expectedCameraHash -eq 0 -or
        $cameraCapture.expectedCameraHash -ne $cameraCapture.actualCameraHash)) {
      throw "Gameplay per-view reference/contact evidence failed: $($capture.name)"
    }
    if($expectedCamera-eq'god'-and(
       $cameraCapture.builderValid-ne1-or$cameraCapture.policyValid-ne1-or
       $cameraCapture.matrixExact-ne1-or[uint64]$cameraCapture.candidateLow-eq0-or
       [uint64]$cameraCapture.expectedCameraHash-eq0-or
       [uint64]$cameraCapture.expectedCameraHash-ne[uint64]$cameraCapture.actualCameraHash)){
      throw "Gameplay God camera diagnostic evidence failed: $($capture.name)"
    }
  }
  $photorealCaptures = @($Photoreal.captures)
  if ($Photoreal.shadowCasters -ne ($photorealCaptures | Measure-Object shadowCasters -Sum).Sum -or
      $Photoreal.shadowDrawCalls -ne ($photorealCaptures | Measure-Object shadowDrawCalls -Sum).Sum -or
      $Photoreal.shadowCascadeDrawCalls -ne ($photorealCaptures | Measure-Object shadowCascadeDrawCalls -Sum).Sum -or
      $Photoreal.lodInstances -ne ($photorealCaptures | Measure-Object lodInstances -Sum).Sum -or
      $Photoreal.lodTransitions -ne ($photorealCaptures | Measure-Object lodTransitions -Sum).Sum -or
      $Photoreal.lodDualSubmissions -ne ($photorealCaptures | Measure-Object lodDualSubmissions -Sum).Sum -or
      $Photoreal.lodBudgetFallbacks -ne ($photorealCaptures | Measure-Object lodBudgetFallbacks -Sum).Sum) {
    throw 'Gameplay photoreal aggregate/per-view count cross-check failed'
  }
}

function Get-GameplayRunReceiptEvidence([string]$Directory, [string[]]$ReceiptNames) {
  $entries = @()
  foreach ($name in $ReceiptNames) {
    $path = Join-Path $Directory $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing run receipt: $name" }
    $entries += [pscustomobject]@{ name = $name; sha256 = Get-GameplayEvidenceSha256Hex $path }
  }
  return $entries
}

function Write-GameplayFinalEvidenceReceipt(
    [string]$Directory, [string]$Executable, [string]$SourceRoot, $PackageEvidence,
    $Telemetry, [object[]]$Runs, [string[]]$ExpectedNames, [double]$MinimumSsim,
    [string]$SourceFingerprint) {
  Assert-GameplayNoFatalMarker $Directory
  $null = Assert-GameplayPhotorealPackageSnapshot $PackageEvidence $PackageEvidence.manifestPath
  if (@($Runs | ForEach-Object { $_.stateSha256 } | Select-Object -Unique).Count -ne 1) {
    throw 'Final capture determinism run state SHA mismatch'
  }
  $expectedReceiptNames=@('gameplay-receipt.json','gameplay-character-capture-receipt.json','gameplay-camera-depth-diagnostic-receipt.json','gameplay-camera-policy-receipt.json','gameplay-determinism-integrity-receipt.json','gameplay-readback-sequence-receipt.json','gameplay-hdr-capture-receipt.json','gameplay-temporal-history-receipt.json','gameplay-presentation-receipt.json','gameplay-temporal-ghost-artifact-receipt.json','gameplay-reference-opacity-receipt.json','gameplay-environment-receipt.json','gameplay-environment-collision-receipt.json','gameplay-palette-ui-receipt.json','gameplay-photoreal-receipt.json')
  foreach($run in $Runs){Assert-GameplayRunArchiveSchema $run $ExpectedNames $expectedReceiptNames}
  $captureEntries = @()
  foreach ($name in $ExpectedNames) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($name)
    $artifacts = @($name, "$stem-object-id.png", "$stem-raw.png", "$stem-raw-object-id.png", "$stem-depth.png", "$stem-raw-depth.png", "$stem-tokens.bin", "$stem-tokens.json", "$stem-raw-tokens.bin", "$stem-raw-tokens.json")
    if ($name -like 'third-*' -or $name -in @('palette-building.png', 'melee-combat.png')) {
      $artifacts += @("$stem-reference.png", "$stem-reference-object-id.png", "$stem-reference-depth.png", "$stem-contact.png", "$stem-reference-contact.png")
    }
    foreach ($artifact in $artifacts) {
      $path = Join-Path $Directory $artifact
      if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing final capture artifact: $artifact" }
      $captureEntries += [pscustomobject]@{ name = $artifact; sha256 = Get-GameplayEvidenceSha256Hex $path }
    }
  }
  $gitSeal = Get-GameplayGitSealEvidence $SourceRoot
  $runEvidence = @($Runs | ForEach-Object {
    [ordered]@{ run = $_.run; runId = $_.runId; stateSha256 = $_.stateSha256; packageSnapshotEpochSha256 = $_.packageSnapshotEpochSha256; fatalAbsent = $_.fatalAbsent; temporalGhostEvidence = $_.temporalGhostEvidence; artifacts = @($_.artifacts); receipts = @($_.receipts) }
  })
  $payload = [ordered]@{
    runId = $Telemetry.runId
    runIds = @($Runs | ForEach-Object { $_.runId })
    source = 'GameplayLabFramebuffer'
    captures = $captureEntries
    telemetry = $Telemetry
    executableSha256 = Get-GameplayEvidenceSha256Hex $Executable
    manifestSha256 = $PackageEvidence.manifestSha256
    manifestBytes = $PackageEvidence.manifestBytes
    packageFiles = $PackageEvidence.packageFiles
    packageBytes = $PackageEvidence.packageBytes
    packagePayloadSha256 = $PackageEvidence.payloadSha256
    packageSnapshotEpochSha256 = $PackageEvidence.snapshotEpochSha256
    gitHead = $gitSeal.head
    gitStatusSha256 = $gitSeal.statusSha256
    sourceFingerprint = $SourceFingerprint
    authoritativeStateSha256 = $Runs[-1].stateSha256
    minimumSsim = $MinimumSsim
    determinismRuns = $runEvidence
  }
  $payloadJson = $payload | ConvertTo-Json -Depth 12 -Compress
  $final = [ordered]@{}
  foreach ($key in $payload.Keys) { $final[$key] = $payload[$key] }
  $final.evidencePayloadSha256 = Get-GameplayEvidenceTextSha256Hex $payloadJson
  $path = Join-Path $Directory 'gameplay-capture-receipt.json'
  $final | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 -LiteralPath $path

  $loaded = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json
  $loadedPayload = [ordered]@{}
  foreach ($property in $loaded.PSObject.Properties) {
    if ($property.Name -ne 'evidencePayloadSha256') { $loadedPayload[$property.Name] = $property.Value }
  }
  if ($loaded.evidencePayloadSha256 -notmatch '^[0-9a-f]{64}$' -or
      (Get-GameplayEvidenceTextSha256Hex ($loadedPayload | ConvertTo-Json -Depth 12 -Compress)) -ne $loaded.evidencePayloadSha256) {
    throw 'Final capture receipt evidence payload self-hash mismatch'
  }
  foreach ($capture in @($loaded.captures)) {
    $capturePath = Join-Path $Directory $capture.name
    if (-not (Test-Path -LiteralPath $capturePath -PathType Leaf) -or
        (Get-GameplayEvidenceSha256Hex $capturePath) -ne $capture.sha256) {
      throw "Final capture receipt artifact SHA mismatch: $($capture.name)"
    }
  }
  foreach ($run in @($loaded.determinismRuns)) {
    if ($run.packageSnapshotEpochSha256 -ne $PackageEvidence.snapshotEpochSha256) {
      throw "Final capture run package snapshot mismatch: run=$($run.run)"
    }
    $runDirectory = Join-Path $Directory "determinism-runs/run-$($run.run)"
    foreach ($artifact in @($run.artifacts)) {
      $artifactPath = Join-Path $runDirectory $artifact.name
      if (-not (Test-Path -LiteralPath $artifactPath -PathType Leaf) -or
          (Get-GameplayEvidenceSha256Hex $artifactPath) -ne $artifact.sha256) {
        throw "Final capture run artifact SHA mismatch: run=$($run.run) $($artifact.name)"
      }
    }
    foreach ($receipt in @($run.receipts)) {
      $receiptPath = Join-Path $runDirectory $receipt.name
      if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf) -or
          (Get-GameplayEvidenceSha256Hex $receiptPath) -ne $receipt.sha256) {
        throw "Final capture run receipt SHA mismatch: run=$($run.run) $($receipt.name)"
      }
    }
  }
  return $loaded
}
