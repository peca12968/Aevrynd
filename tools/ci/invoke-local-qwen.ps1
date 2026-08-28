param(
    [Parameter(Mandatory)] [string]$Task,
    [string[]]$Files = @(),
    [ValidateRange(1,512)] [int]$MaxTokens = 256,
    [string]$JsonSchema = '{"type":"object"}',
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [ValidateRange(10,30)] [int]$TimeoutSeconds = 30,
    [ValidateRange(1,4)] [int]$CpuThreads = 4,
    [ValidateRange(4,24)] [int]$MaximumRamGiB = 24
)

$ErrorActionPreference = 'Stop'
$expectedProjectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..')).TrimEnd('\')
$projectPath = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
$runtimePath = Join-Path $projectPath 'engine\runtime\llama.cpp\llama-cli.exe'
$modelPath = Join-Path $projectPath 'engine\runtime\models\Qwen3.8-27B-UD-Q4_K_M.gguf'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-qwen-' + [Guid]::NewGuid().ToString('N'))
$process = $null
$launched = $false
$timer = $null
$peakWorkingSetBytes = 0L
$totalBytes = 0L
$promptBytes = 0L
$exitCode = $null
$failurePhase = 'initialization'
$jobHandle = $null
$jobAssigned = $false
$stdoutTask = $null
$stderrTask = $null
$outputBytes = 0L
$outputCandidateCount = 0
$outputJsonCandidateCount = 0
$outputSchemaCandidateCount = 0
$outputLastCandidateStart = -1
$outputLastCandidateEnd = -1
$consoleBytes = 0L
$promptEchoBytes = 0L
$promptEchoPrefixMatched = $false
$promptEchoCommonPrefixChars = 0
$responseMarkerFound = $false
$outputCandidateKeys = @()
$runtimeOutputError = 'none'
$runtimeOutputKind = 'unknown'
$failureCode = $null

if (-not ('AevryndPathSecurity' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

public static class AevryndPathSecurity
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern uint GetFinalPathNameByHandle(
        SafeFileHandle handle,
        StringBuilder path,
        uint pathLength,
        uint flags);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateJobObject(IntPtr attributes, string name);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetInformationJobObject(
        IntPtr job, int informationClass, IntPtr information, uint informationLength);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);

    [StructLayout(LayoutKind.Sequential)]
    private struct BasicLimitInformation
    {
        public long PerProcessUserTimeLimit;
        public long PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize;
        public UIntPtr MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass;
        public uint SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IoCounters
    {
        public ulong ReadOperationCount;
        public ulong WriteOperationCount;
        public ulong OtherOperationCount;
        public ulong ReadTransferCount;
        public ulong WriteTransferCount;
        public ulong OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ExtendedLimitInformation
    {
        public BasicLimitInformation BasicLimitInformation;
        public IoCounters IoInfo;
        public UIntPtr ProcessMemoryLimit;
        public UIntPtr JobMemoryLimit;
        public UIntPtr PeakProcessMemoryUsed;
        public UIntPtr PeakJobMemoryUsed;
    }

    public static string GetFinalPath(SafeFileHandle handle)
    {
        StringBuilder buffer = new StringBuilder(32768);
        uint length = GetFinalPathNameByHandle(handle, buffer, (uint)buffer.Capacity, 0);
        if (length == 0 || length >= buffer.Capacity)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        string value = buffer.ToString();
        if (value.StartsWith(@"\\?\UNC\", StringComparison.OrdinalIgnoreCase))
            return @"\\" + value.Substring(8);
        if (value.StartsWith(@"\\?\", StringComparison.OrdinalIgnoreCase))
            return value.Substring(4);
        return value;
    }

    public static SafeFileHandle CreateBoundedKillOnCloseJob(IntPtr processHandle, ulong maximumMemoryBytes)
    {
        const uint KillOnJobClose = 0x00002000;
        const uint JobMemory = 0x00000200;
        IntPtr job = CreateJobObject(IntPtr.Zero, null);
        if (job == IntPtr.Zero)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        SafeFileHandle safeJob = new SafeFileHandle(job, true);
        IntPtr buffer = IntPtr.Zero;
        try
        {
            ExtendedLimitInformation limits = new ExtendedLimitInformation();
            limits.BasicLimitInformation.LimitFlags = KillOnJobClose | JobMemory;
            limits.JobMemoryLimit = new UIntPtr(maximumMemoryBytes);
            int size = Marshal.SizeOf(typeof(ExtendedLimitInformation));
            buffer = Marshal.AllocHGlobal(size);
            Marshal.StructureToPtr(limits, buffer, false);
            if (!SetInformationJobObject(job, 9, buffer, (uint)size))
                throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            if (!AssignProcessToJobObject(job, processHandle))
                throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            return safeJob;
        }
        catch
        {
            safeJob.Dispose();
            throw;
        }
        finally
        {
            if (buffer != IntPtr.Zero) Marshal.FreeHGlobal(buffer);
        }
    }
}
'@
}

function Test-ContainedPath([string]$Candidate,[string]$Root) {
    return $Candidate.Equals($Root,[StringComparison]::OrdinalIgnoreCase) -or
        $Candidate.StartsWith($Root + '\',[StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparsePath([string]$Path,[string]$Root) {
    if (-not (Test-ContainedPath $Path $Root)) { throw 'A path escaped the repository root.' }
    $cursor = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    while ($null -ne $cursor -and (Test-ContainedPath $cursor.FullName $Root)) {
        if (($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Reparse-point paths are not allowed.'
        }
        if ($cursor.FullName.Equals($Root,[StringComparison]::OrdinalIgnoreCase)) { return }
        # DirectoryInfo 從 .Directory 取得時沒有 PSIsContainer 擴充屬性，必須依 CLR 型別判斷。
        $cursor = if ($cursor -is [IO.DirectoryInfo]) { $cursor.Parent } else { $cursor.Directory }
    }
    throw 'A path could not be proven to remain inside the repository root.'
}

function Write-Fallback([string]$Code,[string]$Message) {
    $elapsed = if ($null -ne $timer) { [int64]$timer.ElapsedMilliseconds } else { 0L }
    [ordered]@{
        ok=$false
        fallbackToCodex=$true
        error=[ordered]@{code=$Code;message=$Message}
        resource=[ordered]@{
            launched=$launched
            elapsedMilliseconds=$elapsed
            peakWorkingSetBytes=$peakWorkingSetBytes
            inputBytes=$totalBytes
            promptBytes=$promptBytes
            exitCode=$exitCode
            jobAssigned=$jobAssigned
            output=[ordered]@{bytes=$outputBytes;consoleBytes=$consoleBytes;promptEchoBytes=$promptEchoBytes;promptEchoPrefixMatched=$promptEchoPrefixMatched;promptEchoCommonPrefixChars=$promptEchoCommonPrefixChars;responseMarkerFound=$responseMarkerFound;runtimeError=$runtimeOutputError;runtimeKind=$runtimeOutputKind;candidates=$outputCandidateCount;jsonCandidates=$outputJsonCandidateCount;schemaCandidates=$outputSchemaCandidateCount;lastCandidateStart=$outputLastCandidateStart;lastCandidateEnd=$outputLastCandidateEnd;candidateKeys=$outputCandidateKeys}
            limits=[ordered]@{timeoutSeconds=$TimeoutSeconds;maxTokens=$MaxTokens;cpuThreads=$CpuThreads;maximumRamBytes=([int64]$MaximumRamGiB * 1GB)}
        }
    } | ConvertTo-Json -Depth 8 -Compress
}

function Find-MatchingJson([string]$Text,[string]$Schema) {
    $starts = [Collections.Generic.Stack[int]]::new()
    $candidates = [Collections.Generic.List[string]]::new()
    $inString = $false;$escaped = $false
    for ($index=0;$index -lt $Text.Length;$index++) {
        $character = $Text[$index]
        if ($inString) {
            if ($escaped) { $escaped=$false;continue }
            if ([int]$character -eq 92) { $escaped=$true;continue }
            if ([int]$character -eq 34) { $inString=$false }
            continue
        }
        if ([int]$character -eq 34) { $inString=$true;continue }
        if ($character -eq '{') { $starts.Push($index);continue }
        if ($character -eq '}' -and $starts.Count) {
            $start=$starts.Pop()
            if (-not $starts.Count) {
                $candidates.Add($Text.Substring($start,$index-$start+1))
                $script:outputLastCandidateStart = $start
                $script:outputLastCandidateEnd = $index
            }
        }
    }
    $script:outputCandidateCount = $candidates.Count
    for ($index=$candidates.Count-1;$index -ge 0;$index--) {
        try {
            $candidateObject=$candidates[$index]|ConvertFrom-Json -ErrorAction Stop
            $script:outputCandidateKeys = @($candidateObject.PSObject.Properties.Name | Select-Object -First 16)
            $script:outputJsonCandidateCount++
            if ($candidates[$index]|Test-Json -Schema $Schema -ErrorAction Stop) {
                $script:outputSchemaCandidateCount++
                return $candidates[$index]
            }
        } catch {}
    }
    return $null
}

function Normalize-ModelConsole([string]$Text) {
    $normalized = $Text.Replace("`r`n","`n").Replace("`r","`n")
    $normalized = [Text.RegularExpressions.Regex]::Replace($normalized,"`e\[[0-?]*[ -/]*[@-~]",'')
    return $normalized.TrimStart("`r","`n")
}

function Add-GenerationBounds([object]$Schema,[int]$StringLimit) {
    if ($null -eq $Schema) { return }
    if ($Schema -is [pscustomobject]) {
        if ($Schema.type -eq 'string' -and $null -eq $Schema.PSObject.Properties['maxLength']) {
            $Schema | Add-Member -NotePropertyName maxLength -NotePropertyValue $StringLimit
        }
        if ($null -ne $Schema.PSObject.Properties['properties']) {
            foreach ($property in $Schema.properties.PSObject.Properties) {
                Add-GenerationBounds $property.Value $StringLimit
            }
        }
        if ($null -ne $Schema.PSObject.Properties['items']) { Add-GenerationBounds $Schema.items $StringLimit }
        foreach ($keyword in @('allOf','anyOf','oneOf')) {
            $entry = $Schema.PSObject.Properties[$keyword]
            if ($null -ne $entry) { foreach ($child in $entry.Value) { Add-GenerationBounds $child $StringLimit } }
        }
    }
}

try {
    $failurePhase = 'repository-boundary'
    if (-not $projectPath.Equals($expectedProjectPath,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'ProjectRoot must be the repository that contains this wrapper.'
    }
    Assert-NoReparsePath $projectPath $projectPath
    if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf) -or -not (Test-Path -LiteralPath $modelPath -PathType Leaf)) {
        Write-Fallback 'RuntimeUnavailable' 'The local QWEN runtime or model is unavailable.'
        exit 0
    }
    Assert-NoReparsePath $runtimePath $projectPath
    Assert-NoReparsePath $modelPath $projectPath
    $failurePhase = 'schema-validation'
    $schemaObject = $JsonSchema | ConvertFrom-Json -ErrorAction Stop
    if ($schemaObject -isnot [pscustomobject] -or $schemaObject.type -ne 'object') {
        throw 'JsonSchema must describe an object at its root.'
    }
    if ($JsonSchema -match '"\$ref"\s*:') { throw 'External or indirect schema references are not allowed.' }
    # 未限制的字串可能耗盡 token 後留下半個 JSON；生成時加上與預算成比例的上限。
    $generationSchemaObject = $JsonSchema | ConvertFrom-Json -ErrorAction Stop
    Add-GenerationBounds $generationSchemaObject ([Math]::Max(8,[Math]::Floor($MaxTokens / 2)))
    $generationSchema = $generationSchemaObject | ConvertTo-Json -Depth 100 -Compress
    $sections = [Collections.Generic.List[string]]::new()
    $failurePhase = 'input-validation'
    foreach ($file in $Files) {
        try { $resolved = (Resolve-Path -LiteralPath (Join-Path $projectPath $file) -ErrorAction Stop).Path } catch { throw 'An input file could not be resolved safely.' }
        if (-not (Test-ContainedPath $resolved $projectPath)) { throw 'An input file escaped the repository root.' }
        $item = Get-Item -LiteralPath $resolved
        if ($item.PSIsContainer) { throw 'Input paths must be files.' }
        if ($item.Extension.ToLowerInvariant() -notin @('.md','.txt','.log','.json','.csv')) { throw 'Local QWEN is limited to document and log classification.' }
        if ($item.Length -gt 16384 -or ($totalBytes + $item.Length) -gt 16384) { throw 'Input files exceed the 16 KiB local-worker policy.' }
        Assert-NoReparsePath $resolved $projectPath

        # 先鎖定檔案，再從 handle 驗證最終路徑，避免檢查與讀取之間被替換。
        $stream = [IO.File]::Open($resolved,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        try {
            $finalPath = [IO.Path]::GetFullPath([AevryndPathSecurity]::GetFinalPath($stream.SafeFileHandle))
            if (-not (Test-ContainedPath $finalPath $projectPath)) { throw 'An opened input file escaped the repository root.' }
            Assert-NoReparsePath $resolved $projectPath
            $remainingBytes = 16384 - $totalBytes
            if ($stream.Length -gt $remainingBytes) { throw 'Input files exceed the 16 KiB local-worker policy.' }
            $buffer = [byte[]]::new($remainingBytes + 1)
            $readBytes = 0
            while ($readBytes -lt $buffer.Length) {
                $count = $stream.Read($buffer,$readBytes,$buffer.Length - $readBytes)
                if ($count -eq 0) { break }
                $readBytes += $count
            }
            if ($readBytes -gt $remainingBytes) { throw 'An input file grew beyond the 16 KiB local-worker policy while being read.' }
            $contents = [Text.UTF8Encoding]::new($false,$true).GetString($buffer,0,$readBytes)
        } finally {
            $stream.Dispose()
        }
        $totalBytes += [Text.Encoding]::UTF8.GetByteCount($contents)
        $relative = $resolved.Substring($projectPath.Length + 1).Replace('\','/')
        $sections.Add("FILE: $relative`n" + $contents)
    }

    # 模型只接收呼叫者明確提供的唯讀內容，不允許自行探索檔案系統。
    $responseMarker = 'AEVRYND_RESPONSE_' + [Guid]::NewGuid().ToString('N')
    $prompt = @"
You are a local offline code-review worker. Use only the task and file contents below. Do not request tools, write files, or invent findings. Return exactly one JSON value matching the supplied schema.

JSON SCHEMA:
$generationSchema

TASK:
$Task

INPUTS:
$($sections -join "`n`n")

Do not repeat the response marker. Emit the JSON response immediately after it.
$responseMarker
"@
    $failurePhase = 'prompt-validation'
    $promptBytes = [Text.Encoding]::UTF8.GetByteCount($prompt)
    if ($promptBytes -gt 16384) { throw 'The complete prompt exceeds the 16 KiB local-worker policy.' }
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    $promptPath = Join-Path $temporaryRoot 'prompt.txt'
    $schemaPath = Join-Path $temporaryRoot 'schema.json'
    $outputPath = Join-Path $temporaryRoot 'response.json'
    $logPath = Join-Path $temporaryRoot 'runtime.log'
    [IO.File]::WriteAllText($promptPath,$prompt,[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($schemaPath,$generationSchema,[Text.UTF8Encoding]::new($false))
    $failurePhase = 'runtime-launch'
    # Chat template 會讓結束回合的特殊 token 經過 JSON grammar 而初始化失敗；completion 模式沒有此衝突。
    # 固定短 prompt 讓 single-turn 進入生成；完整內容與 schema 仍只經過暫存檔傳入。
    $arguments = @('-m',$modelPath,'--prompt','Follow the complete task from the attached prompt file.','--file',$promptPath,'-n',[string]$MaxTokens,'-c','4096','-t',[string]$CpuThreads,'-tb',[string]$CpuThreads,'-ngl','32','--flash-attn','auto','--reasoning','off','--reasoning-budget','0','--json-schema-file',$schemaPath,'--output-file',$outputPath,'--log-file',$logPath,'--no-jinja','--single-turn','--simple-io','--no-display-prompt','--color','off','--no-perf')
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $runtimePath
    $startInfo.WorkingDirectory = $temporaryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in $arguments) { $startInfo.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw 'Local QWEN could not be started.' }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $launched = $true
    $maximumBytes = [int64]$MaximumRamGiB * 1GB
    $jobHandle = [AevryndPathSecurity]::CreateBoundedKillOnCloseJob($process.Handle,[uint64]$maximumBytes)
    $jobAssigned = $true
    try { $process.PriorityClass = [Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $failurePhase = 'runtime-execution'
    while (-not $process.HasExited) {
        if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) { $process.Kill($true);$process.WaitForExit();throw 'Local QWEN timed out.' }
        $process.Refresh()
        if ($process.WorkingSet64 -gt $peakWorkingSetBytes) { $peakWorkingSetBytes=$process.WorkingSet64 }
        if ($process.WorkingSet64 -gt $maximumBytes) { $process.Kill($true);$process.WaitForExit();throw 'Local QWEN exceeded its RAM budget.' }
        Start-Sleep -Milliseconds 100
    }
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    if ($process.ExitCode -ne 0) {
        throw "Local QWEN exited with code $($process.ExitCode)."
    }
    $consoleText = $stdoutTask.GetAwaiter().GetResult()
    $stderrText = $stderrTask.GetAwaiter().GetResult()
    $diagnosticText = $consoleText + "`n" + $stderrText
    if ($diagnosticText -match 'JSON schema to constrain generations') { $runtimeOutputKind = 'help' }
    elseif ($diagnosticText -match 'Loading model') { $runtimeOutputKind = 'model-session' }
    if ($diagnosticText -match 'Failed to initialize samplers') { $runtimeOutputError = 'sampler-initialization' }
    elseif ($diagnosticText -match '(?m)^Error:') { $runtimeOutputError = 'runtime-error' }
    if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
        $failureCode = 'ModelOutputInvalid'
        throw 'Local QWEN did not create its bounded output file.'
    }
    $outputItem = Get-Item -LiteralPath $outputPath -Force
    if (($outputItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $outputItem.Length -gt 65536) {
        $failureCode = 'ModelOutputInvalid'
        throw 'Local QWEN produced an unsafe or oversized output file.'
    }
    $raw = [IO.File]::ReadAllText($outputPath,[Text.UTF8Encoding]::new($false,$true)).Trim()
    $promptEcho = ''
    $promptEchoBytes = [Text.Encoding]::UTF8.GetByteCount($promptEcho)
    $consoleBytes = [Text.Encoding]::UTF8.GetByteCount($consoleText)
    $normalizedEcho = Normalize-ModelConsole $promptEcho
    $normalizedConsole = Normalize-ModelConsole $consoleText
    $prefixLimit = [Math]::Min($normalizedEcho.Length,$normalizedConsole.Length)
    while ($promptEchoCommonPrefixChars -lt $prefixLimit -and $normalizedEcho[$promptEchoCommonPrefixChars] -eq $normalizedConsole[$promptEchoCommonPrefixChars]) {
        $promptEchoCommonPrefixChars++
    }
    $markerIndex = $raw.LastIndexOf($responseMarker,[StringComparison]::Ordinal)
    if ($markerIndex -lt 0) {
        $failureCode = 'ModelOutputInvalid'
        throw 'Local QWEN output did not preserve the response boundary.'
    }
    $responseMarkerFound = $true
    # output-file 可能包含提示內容；只驗證不可預測分隔線之後的實際生成，避免輸入 JSON 被誤認為回答。
    $raw = $raw.Substring($markerIndex + $responseMarker.Length).Trim()
    $outputBytes = [Text.Encoding]::UTF8.GetByteCount($raw)
    $failurePhase = 'output-validation'
    $json = Find-MatchingJson $raw $JsonSchema
    if (-not $json) {
        $failureCode = 'ModelOutputInvalid'
        throw "Local QWEN did not return schema-valid JSON (stdoutBytes=$([Text.Encoding]::UTF8.GetByteCount($raw)))."
    }
    $resultObject=$json|ConvertFrom-Json
    [ordered]@{
        ok=$true
        fallbackToCodex=$false
        result=$resultObject
        resource=[ordered]@{
            launched=$launched
            elapsedMilliseconds=[int64]$timer.ElapsedMilliseconds
            peakWorkingSetBytes=$peakWorkingSetBytes
            inputBytes=$totalBytes
            promptBytes=$promptBytes
            exitCode=$exitCode
            jobAssigned=$jobAssigned
            output=[ordered]@{bytes=$outputBytes;consoleBytes=$consoleBytes;promptEchoBytes=$promptEchoBytes;promptEchoPrefixMatched=$promptEchoPrefixMatched;promptEchoCommonPrefixChars=$promptEchoCommonPrefixChars;responseMarkerFound=$responseMarkerFound;runtimeError=$runtimeOutputError;runtimeKind=$runtimeOutputKind;candidates=$outputCandidateCount;jsonCandidates=$outputJsonCandidateCount;schemaCandidates=$outputSchemaCandidateCount;lastCandidateStart=$outputLastCandidateStart;lastCandidateEnd=$outputLastCandidateEnd;candidateKeys=$outputCandidateKeys}
            requestedGpuLayers=32
            modelFile=[IO.Path]::GetFileName($modelPath)
            limits=[ordered]@{timeoutSeconds=$TimeoutSeconds;maxTokens=$MaxTokens;cpuThreads=$CpuThreads;maximumRamBytes=([int64]$MaximumRamGiB * 1GB)}
        }
    } | ConvertTo-Json -Depth 100 -Compress
} catch {
    # 僅回傳固定階段名稱，讓呼叫端可診斷而不暴露路徑或模型輸出。
    $code = if ($null -ne $failureCode) { $failureCode } else { 'LocalQwenFailed.' + $failurePhase }
    Write-Fallback $code 'Local QWEN could not complete this bounded task; use Codex fallback.'
} finally {
    if ($null -ne $process) { try { if (-not $process.HasExited) {$process.Kill($true);$process.WaitForExit()};$process.Dispose() } catch {} }
    if ($null -ne $jobHandle) { try { $jobHandle.Dispose() } catch {} }
    if (Test-Path -LiteralPath $temporaryRoot) { Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
