param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$OutputRoot,
    [string]$Gltfpack = 'D:\Download\Aevrynd\ToolCache\meshoptimizer-1.2-build\Release\gltfpack.exe'
)
$ErrorActionPreference = 'Stop'
$expectedToolHash = '1517A5E6FD3AF9FC8A07B57E9BC018D882FEC1058E77120777DB9C79D938288B'
$limit = 2GB
if ((Get-FileHash -LiteralPath $Gltfpack -Algorithm SHA256).Hash -ne $expectedToolHash) { throw 'gltfpack executable provenance hash mismatch.' }

if (-not ('AevryndProcessMemoryJob' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class AevryndProcessMemoryJob {
    [StructLayout(LayoutKind.Sequential)] public struct IoCounters { public ulong ReadOperationCount, WriteOperationCount, OtherOperationCount, ReadTransferCount, WriteTransferCount, OtherTransferCount; }
    [StructLayout(LayoutKind.Sequential)] public struct BasicLimitInformation { public long PerProcessUserTimeLimit, PerJobUserTimeLimit; public uint LimitFlags; public UIntPtr MinimumWorkingSetSize, MaximumWorkingSetSize; public uint ActiveProcessLimit; public UIntPtr Affinity; public uint PriorityClass, SchedulingClass; }
    [StructLayout(LayoutKind.Sequential)] public struct ExtendedLimitInformation { public BasicLimitInformation BasicLimitInformation; public IoCounters IoInfo; public UIntPtr ProcessMemoryLimit, JobMemoryLimit, PeakProcessMemoryUsed, PeakJobMemoryUsed; }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr CreateJobObject(IntPtr attributes, string name);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool QueryInformationJobObject(IntPtr job, int informationClass, out ExtendedLimitInformation information, uint length, IntPtr returnLength);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    public static IntPtr Create() { IntPtr job=CreateJobObject(IntPtr.Zero, null); if(job==IntPtr.Zero) throw new Win32Exception(); return job; }
    public static void Assign(IntPtr job, IntPtr process) { if(!AssignProcessToJobObject(job, process)) throw new Win32Exception(); }
    public static ulong PeakBytes(IntPtr job) { ExtendedLimitInformation value; uint length=(uint)Marshal.SizeOf<ExtendedLimitInformation>(); if(!QueryInformationJobObject(job, 9, out value, length, IntPtr.Zero)) throw new Win32Exception(); return value.PeakJobMemoryUsed.ToUInt64(); }
    public static void Close(IntPtr job) { if(job!=IntPtr.Zero) CloseHandle(job); }
}
'@
}

function Get-ProcessTreeWorkingSet([int]$RootId) {
    $root = Get-Process -Id $RootId -ErrorAction SilentlyContinue
    if (-not $root) { return [pscustomobject]@{Bytes=0L;ProcessCount=0} }
    $processes = @(Get-Process -ErrorAction SilentlyContinue)
    $ids = [Collections.Generic.HashSet[int]]::new()
    [void]$ids.Add($RootId)
    $changed = $true
    while ($changed) {
        $changed = $false
        foreach ($candidate in $processes) {
            try { $parentId = $candidate.Parent.Id } catch { continue }
            if ($ids.Contains($parentId) -and $ids.Add($candidate.Id)) { $changed = $true }
        }
    }
    try { $root.Refresh(); $bytes = [int64]$root.WorkingSet64; $count = 1 } catch { return [pscustomobject]@{Bytes=0L;ProcessCount=0} }
    foreach ($candidate in $processes) {
        if ($candidate.Id -eq $RootId -or -not $ids.Contains([int]$candidate.Id)) { continue }
        try { $candidate.Refresh(); $workingSet = [int64]$candidate.WorkingSet64 } catch { continue }
        if ($workingSet -gt [int64]::MaxValue - $bytes) { throw 'Process-tree working set overflow.' }
        $bytes += $workingSet
        ++$count
    }
    return [pscustomobject]@{Bytes=$bytes;ProcessCount=$count}
}

[IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
$levels = @()
$peakWorkingSet = 0L
$peakJobMemory = 0L
$sampleCount = 0
$maxProcessCount = 0
foreach ($level in 0..2) {
    $ratio = @(1.0, 0.5, 0.15)[$level].ToString([Globalization.CultureInfo]::InvariantCulture)
    $output = Join-Path $OutputRoot "island-tree-01-lod$level.glb"
    $report = Join-Path $OutputRoot "island-tree-01-lod$level-report.json"
    $job = [AevryndProcessMemoryJob]::Create()
    try {
        $process = Start-Process -FilePath $Gltfpack -WindowStyle Hidden -ArgumentList @('-i',$Source,'-o',$output,'-si',$ratio,'-se','0.01','-kn','-km','-kv','-tr','-r',$report) -PassThru
        try { [AevryndProcessMemoryJob]::Assign($job, $process.Handle) } catch {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw 'Unable to assign gltfpack to the process-tree memory job.'
        }
        do {
            $sample = Get-ProcessTreeWorkingSet $process.Id
            $peakWorkingSet = [Math]::Max($peakWorkingSet, $sample.Bytes)
            $maxProcessCount = [Math]::Max($maxProcessCount, $sample.ProcessCount)
            ++$sampleCount
            if (-not $process.HasExited) { Start-Sleep -Milliseconds 10 }
        } while (-not $process.HasExited)
        $process.WaitForExit()
        $peakJobMemory = [Math]::Max($peakJobMemory, [int64][AevryndProcessMemoryJob]::PeakBytes($job))
    } finally {
        [AevryndProcessMemoryJob]::Close($job)
    }
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $output)) { throw "gltfpack failed for LOD $level." }
    $document = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
    $levels += [ordered]@{level=$level;ratio=[double]$ratio;triangles=[uint64]$document.render.triangleCount;materials=$document.scene.materialCount;textures=$document.scene.textureCount;bytes=(Get-Item $output).Length;sha256=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash}
}
$peakProcessTree = [Math]::Max($peakWorkingSet, $peakJobMemory)
if ($levels[1].triangles -ge $levels[0].triangles -or $levels[2].triangles -ge $levels[1].triangles) { throw 'gltfpack triangle counts did not strictly decrease.' }
$memoryGate = 'pass'
$memoryGateReason = $null
if ($sampleCount -lt 3 -or $maxProcessCount -lt 1 -or $peakWorkingSet -le 0 -or $peakJobMemory -le 0) {
    $memoryGate = 'fail'
    $memoryGateReason = "incomplete-telemetry:samples=$sampleCount,processes=$maxProcessCount,workingSet=$peakWorkingSet,jobMemory=$peakJobMemory"
} elseif ($peakProcessTree -ge $limit) {
    $memoryGate = 'fail'
    $memoryGateReason = "peak-limit-exceeded:peak=$peakProcessTree,limit=$limit"
}
$receipt = [ordered]@{schemaVersion=2;tool='gltfpack';tag='v1.2';commit='9d9890c73011d75920af614485296d1e03e95448';archiveSha256='E40F71B809CDF3361B9A4DEF85FD44534E8733CE29D4B943C145B76859E4C2B4';executableSha256=$expectedToolHash;license='MIT';memoryGate=$memoryGate;memoryGateMethod='gltfpack-process-tree';memoryGateReason=$memoryGateReason;peakWorkingSetBytes=$peakWorkingSet;peakJobMemoryBytes=$peakJobMemory;peakProcessTreeBytes=$peakProcessTree;peakLimitBytes=$limit;memorySampleCount=$sampleCount;maxProcessCount=$maxProcessCount;levels=$levels}
[IO.File]::WriteAllText((Join-Path $OutputRoot 'gltfpack-import-receipt.json'), (($receipt | ConvertTo-Json -Depth 8) + "`n"), [Text.UTF8Encoding]::new($false))
if ($memoryGate -ne 'pass') { throw "gltfpack process-tree memory gate failed: $memoryGateReason" }
Write-Output "GLTFPACK_IMPORT=PASS PEAK_PROCESS_TREE_BYTES=$peakProcessTree LEVELS=3"
