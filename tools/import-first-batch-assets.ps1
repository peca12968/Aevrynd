param(
    [string]$Catalog = (Join-Path $PSScriptRoot '../engine/assets/asset-catalogs/realistic-cc0-assets.json'),
    [Parameter(Mandatory = $true)][string]$InputRoot,
    [Parameter(Mandatory = $true)][string]$OutputRoot,
    [string]$Blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe',
    [switch]$PlanOnly
)
$ErrorActionPreference = 'Stop'
$memoryLimitBytes = 2GB

function ConvertTo-EnglishAssetName([string]$Value) {
    $result = [regex]::Replace($Value, '([a-z0-9])([A-Z])', '$1-$2').ToLowerInvariant()
    $result = [regex]::Replace($result, '[^a-z0-9]+', '-').Trim('-')
    if (-not $result) { throw "Asset id '$Value' has no valid English name." }
    return $result
}

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

$catalogPath = (Resolve-Path -LiteralPath $Catalog).Path
$inputPath = [IO.Path]::GetFullPath($InputRoot)
$outputPath = [IO.Path]::GetFullPath($OutputRoot)
$inputPrefix = $inputPath.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
$outputPrefix = $outputPath.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if ($inputPrefix.StartsWith($outputPrefix, [StringComparison]::OrdinalIgnoreCase) -or $outputPrefix.StartsWith($inputPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'InputRoot and OutputRoot must not contain one another.' }
$document = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
$assets = @($document.assets | Where-Object firstBatch)
if ($document.policy.allowedLicense -ne 'CC0-1.0' -or -not $document.policy.commercialUseRequired -or $assets.Count -ne 10) {
    throw 'Catalog must define exactly ten commercial-use CC0-1.0 first-batch assets.'
}
$plan = foreach ($asset in $assets) {
    $name = ConvertTo-EnglishAssetName $asset.id
    $assetInput = Join-Path $inputPath $name
    $candidates = if (Test-Path -LiteralPath $assetInput) { @(Get-ChildItem -LiteralPath $assetInput -File -Recurse) } else { @() }
    $source = if ($asset.kind -eq 'mesh-pbr') {
        $candidates | Where-Object Extension -In '.gltf','.glb','.fbx','.obj','.blend' | Sort-Object FullName | Select-Object -First 1
    } elseif ($asset.kind -eq 'hdri') {
        $candidates | Where-Object Extension -In '.hdr','.exr' | Sort-Object FullName | Select-Object -First 1
    } else {
        $candidates | Where-Object Extension -In '.jpg','.jpeg','.png','.tif','.tiff','.exr' | Sort-Object FullName | Select-Object -First 1
    }
    [ordered]@{
        id = $asset.id; assetName = $name; source = $asset.source; kind = $asset.kind
        license = $document.sources.($asset.source).license; officialPage = $asset.officialPage
        inputDirectory = $assetInput; sourceFile = if ($source) { $source.FullName } else { $null }
        outputDirectory = (Join-Path $outputPath $name); status = if ($source) { 'ready' } else { 'missing-source' }
    }
}
if ($PlanOnly) {
    $plan | ConvertTo-Json -Depth 6
    if (@($plan | Where-Object status -eq 'missing-source').Count) { exit 2 }
    exit 0
}
if (-not (Test-Path -LiteralPath $Blender -PathType Leaf)) { throw "Blender executable not found: $Blender" }
if (@($plan | Where-Object status -eq 'missing-source').Count) { throw 'One or more first-batch source directories are missing or contain no supported file.' }
[IO.Directory]::CreateDirectory($outputPath) | Out-Null
$peakWorkingSetBytes = 0L
$peakJobMemoryBytes = 0L
$memorySampleCount = 0
$maxProcessCount = 0
$receipts = foreach ($entry in $plan) {
    [IO.Directory]::CreateDirectory($entry.outputDirectory) | Out-Null
    if ($entry.kind -eq 'mesh-pbr') {
        $lodReceipt = Join-Path $entry.outputDirectory "$($entry.assetName)-lod-receipt.json"
        $levels = @()
        for ($level = 0; $level -lt 3; ++$level) {
            $ratio = @(1.0, 0.5, 0.15)[$level].ToString([Globalization.CultureInfo]::InvariantCulture)
            $log = Join-Path $entry.outputDirectory "$($entry.assetName)-lod$level-blender.log"
            $errorLog = Join-Path $entry.outputDirectory "$($entry.assetName)-lod$level-blender-error.log"
            $arguments = @('--background','--python',(Join-Path $PSScriptRoot 'blender/build-static-mesh-lods.py'),'--','--source',$entry.sourceFile,'--output',$entry.outputDirectory,'--asset-name',$entry.assetName,'--ratios',$ratio,'--level-offset',[string]$level)
            $job = [AevryndProcessMemoryJob]::Create()
            try {
                $process = Start-Process -FilePath $Blender -ArgumentList $arguments -WindowStyle Hidden -RedirectStandardOutput $log -RedirectStandardError $errorLog -PassThru
                try { [AevryndProcessMemoryJob]::Assign($job, $process.Handle) } catch {
                    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                    throw 'Unable to assign Blender to the process-tree memory job.'
                }
                do {
                    $sample = Get-ProcessTreeWorkingSet $process.Id
                    $peakWorkingSetBytes = [Math]::Max($peakWorkingSetBytes, $sample.Bytes)
                    $maxProcessCount = [Math]::Max($maxProcessCount, $sample.ProcessCount)
                    ++$memorySampleCount
                    if (-not $process.HasExited) { Start-Sleep -Milliseconds 50 }
                } while (-not $process.HasExited)
                $process.WaitForExit()
                $peakJobMemoryBytes = [Math]::Max($peakJobMemoryBytes, [int64][AevryndProcessMemoryJob]::PeakBytes($job))
            } finally {
                [AevryndProcessMemoryJob]::Close($job)
            }
            $blenderOutput = @(Get-Content -LiteralPath $log)
            if ($process.ExitCode -ne 0 -or @($blenderOutput | Where-Object { $_ -match '^LOD_RECEIPT=.+ LEVELS=1 SOURCE_TRIANGLES=\d+$' }).Count -ne 1) {
                throw "Blender single-process LOD conversion failed for $($entry.assetName) level $level."
            }
            $single = Get-Content -LiteralPath $lodReceipt -Raw | ConvertFrom-Json
            $levels += $single.levels[0]
        }
        if ($levels[1].triangles -ge $levels[0].triangles -or $levels[2].triangles -ge $levels[1].triangles) {
            throw "Blender LOD receipt failed strict triangle reduction for $($entry.assetName)."
        }
        $peakProcessTreeBytes = [Math]::Max($peakWorkingSetBytes, $peakJobMemoryBytes)
        $memoryGate = 'pass'
        $memoryGateReason = $null
        if ($memorySampleCount -lt 3 -or $maxProcessCount -lt 1 -or $peakWorkingSetBytes -le 0 -or $peakJobMemoryBytes -le 0) {
            $memoryGate = 'fail'
            $memoryGateReason = "incomplete-telemetry:samples=$memorySampleCount,processes=$maxProcessCount,workingSet=$peakWorkingSetBytes,jobMemory=$peakJobMemoryBytes"
        } elseif ($peakProcessTreeBytes -ge $memoryLimitBytes) {
            $memoryGate = 'fail'
            $memoryGateReason = "peak-limit-exceeded:peak=$peakProcessTreeBytes,limit=$memoryLimitBytes"
        }
        $combined = [ordered]@{schemaVersion=2;assetName=$entry.assetName;source=$entry.sourceFile;sourceSha256=(Get-FileHash -LiteralPath $entry.sourceFile -Algorithm SHA256).Hash;blenderVersion='5.2.1 LTS';memoryGate=$memoryGate;memoryGateMethod='blender-process-tree';memoryGateReason=$memoryGateReason;peakWorkingSetBytes=$peakWorkingSetBytes;peakJobMemoryBytes=$peakJobMemoryBytes;peakProcessTreeBytes=$peakProcessTreeBytes;peakLimitBytes=$memoryLimitBytes;memorySampleCount=$memorySampleCount;maxProcessCount=$maxProcessCount;levels=$levels}
        [IO.File]::WriteAllText($lodReceipt, (($combined | ConvertTo-Json -Depth 8) + "`n"), [Text.UTF8Encoding]::new($false))
        if ($memoryGate -ne 'pass') { throw "Blender process-tree memory gate failed: $memoryGateReason" }

        $textureFiles = @(Get-ChildItem -LiteralPath $entry.inputDirectory -File -Recurse | Where-Object Extension -In '.jpg','.jpeg','.png','.tif','.tiff','.exr')
        $baseColor = $textureFiles | Where-Object Name -Match '(?i)(?:_diff_|color)' | Sort-Object FullName | Select-Object -First 1
        $normal = $textureFiles | Where-Object Name -Match '(?i)(?:nor_gl|normalgl|normal_gl)' | Sort-Object FullName | Select-Object -First 1
        $surface = $textureFiles | Where-Object Name -Match '(?i)(?:_arm_|_orm_|rough)' | Sort-Object @{Expression={if($_.Name -match '(?i)(?:_arm_|_orm_)'){0}else{1}}},FullName | Select-Object -First 1
        if (-not $baseColor -or -not $normal -or -not $surface) { throw "Mesh PBR channels are incomplete for $($entry.assetName)." }
        $textureOutput = Join-Path $entry.outputDirectory 'textures'
        [IO.Directory]::CreateDirectory($textureOutput) | Out-Null
        foreach ($texture in @($baseColor,$normal,$surface)) { Copy-Item -LiteralPath $texture.FullName -Destination (Join-Path $textureOutput $texture.Name) -Force }
        $material = [ordered]@{schemaVersion=1;binding='runtime-sidecar';meshMaterialBinding='integration-required';materials=@([ordered]@{slot=0;baseColor="textures/$($baseColor.Name)";normalGl="textures/$($normal.Name)";surface="textures/$($surface.Name)"})}
        [IO.File]::WriteAllText((Join-Path $entry.outputDirectory "$($entry.assetName).materials.json"), (($material | ConvertTo-Json -Depth 6) + "`n"), [Text.UTF8Encoding]::new($false))
    }
    $sourceFiles = @(Get-ChildItem -LiteralPath $entry.inputDirectory -File -Recurse | Sort-Object FullName | ForEach-Object {
        [ordered]@{ file = $_.FullName; bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
    $hash = ($sourceFiles | Where-Object file -eq $entry.sourceFile | Select-Object -First 1).sha256
    [ordered]@{
        schemaVersion = 1; assetName = $entry.assetName; catalogId = $document.catalogId
        sourceId = $entry.id; sourceProvider = $entry.source; officialPage = $entry.officialPage
        license = $entry.license; commercialUse = $true; sourceFile = $entry.sourceFile
        sourceSha256 = $hash; sourceFiles = $sourceFiles; kind = $entry.kind
        status = if ($entry.kind -eq 'mesh-pbr') { 'lod-generated' } else { 'validated' }
    }
}
$receiptPath = Join-Path $outputPath 'first-batch10-import-receipt.json'
[IO.File]::WriteAllText($receiptPath, (($receipts | ConvertTo-Json -Depth 8) + "`n"), [Text.UTF8Encoding]::new($false))
$peakProcessTreeBytes = [Math]::Max($peakWorkingSetBytes, $peakJobMemoryBytes)
Write-Output "IMPORT_RECEIPT=$receiptPath ASSETS=$($receipts.Count) PEAK_PROCESS_TREE_BYTES=$peakProcessTreeBytes"
