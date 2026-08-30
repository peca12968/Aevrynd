param(
    [Parameter(Mandatory)] [string]$Task,
    [string[]]$Files = @(),
    [ValidateRange(1,512)] [int]$MaxTokens = 256,
    [string]$JsonSchema = '{"type":"object"}',
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [ValidateRange(10,30)] [int]$TimeoutSeconds = 30,
    [ValidateRange(1,4)] [int]$CpuThreads = 4,
    [ValidateRange(4,24)] [int]$MaximumRamGiB = 24,
    [long]$ContextSize = 4096,
    [long]$SafetyReserveGiB = 4
)

$ErrorActionPreference = 'Stop'
$internalWorker = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_INTERNAL_WORKER') -eq '1'
$workerBootstrapError = $null
$maximumFilesJsonBytes = 24570
$maximumFilesBase64Bytes = 32760

if (-not ('AevryndBoundedTextReader' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.Threading.Tasks;

public sealed class AevryndBoundedTextResult
{
    public string Text { get; set; }
    public long Utf8Bytes { get; set; }
    public bool LimitExceeded { get; set; }
}

public static class AevryndBoundedTextReader
{
    public static async Task<AevryndBoundedTextResult> ReadAsync(StreamReader reader, long maximumUtf8Bytes)
    {
        char[] buffer = new char[4096];
        StringBuilder text = new StringBuilder();
        long bytes = 0;
        while (true)
        {
            int count = await reader.ReadAsync(buffer, 0, buffer.Length).ConfigureAwait(false);
            if (count == 0) break;
            bytes = checked(bytes + Encoding.UTF8.GetByteCount(buffer, 0, count));
            if (bytes > maximumUtf8Bytes)
                return new AevryndBoundedTextResult { Text = text.ToString(), Utf8Bytes = bytes, LimitExceeded = true };
            text.Append(buffer, 0, count);
        }
        return new AevryndBoundedTextResult { Text = text.ToString(), Utf8Bytes = bytes, LimitExceeded = false };
    }
}
'@
}

if ($internalWorker) {
    # PowerShell -File 無法可靠地把多個離散引數繫結到 string[]；監督器改用有界 JSON 跨程序傳遞。
    $encodedFiles = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_WORKER_FILES_B64')
    if ([string]::IsNullOrEmpty($encodedFiles)) {
        $workerBootstrapError = 'The worker file payload is missing.'
        $Files = @()
    } else {
        try {
            if ([Text.Encoding]::ASCII.GetByteCount($encodedFiles) -gt $maximumFilesBase64Bytes) { throw 'The worker file payload is oversized.' }
            $filePayload = [Text.UTF8Encoding]::new($false,$true).GetString([Convert]::FromBase64String($encodedFiles))
            if ([Text.Encoding]::UTF8.GetByteCount($filePayload) -gt $maximumFilesJsonBytes) { throw 'The worker file JSON is oversized.' }
            $decodedValue = ConvertFrom-Json -InputObject $filePayload -NoEnumerate -ErrorAction Stop
            if ($null -eq $decodedValue -or $decodedValue -isnot [array]) { throw 'The worker file payload must be a JSON array.' }
            $decodedFiles = @($decodedValue)
            if ($decodedFiles.Count -gt 64) { throw 'The worker file list is oversized.' }
            foreach ($file in $decodedFiles) {
                if ($file -isnot [string] -or [string]::IsNullOrWhiteSpace($file) -or $file.Length -gt 1024) {
                    throw 'The worker file list is invalid.'
                }
            }
            $Files = [string[]]$decodedFiles
        } catch {
            $workerBootstrapError = 'The worker file payload could not be decoded safely.'
            $Files = @()
        }
    }

    # 這些 stub 只驗證 worker/supervisor 協定，不會讀模型或啟動 llama runtime。
    $protocolStub = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_WORKER_PROTOCOL_STUB')
    if ($null -eq $workerBootstrapError -and [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1' -and -not [string]::IsNullOrEmpty($protocolStub)) {
        $stubLimits = [ordered]@{timeoutSeconds=$TimeoutSeconds;maxTokens=$MaxTokens;cpuThreads=$CpuThreads;maximumRamBytes=([int64]$MaximumRamGiB * 1GB);contextSize=$ContextSize;safetyReserveBytes=($SafetyReserveGiB * 1GB)}
        $stubModelBytes = 1GB
        $stubKvCacheBytes = [int64]([decimal]$ContextSize * 393216)
        $stubRuntimeOverheadBytes = [Math]::Max(1GB,[int64][Math]::Ceiling([decimal]$stubModelBytes / 16))
        $stubAllocatorMarginBytes = 512MB
        $stubEstimatedPeakBytes = $stubModelBytes + $stubKvCacheBytes + $stubRuntimeOverheadBytes + $stubAllocatorMarginBytes
        $stubRequiredAvailableBytes = $stubEstimatedPeakBytes + ($SafetyReserveGiB * 1GB)
        $stubPreflight = [ordered]@{evaluated=$true;passed=$true;reason='ok';modelBytes=$stubModelBytes;contextSize=$ContextSize;kvCacheBytes=$stubKvCacheBytes;runtimeOverheadBytes=$stubRuntimeOverheadBytes;allocatorMarginBytes=$stubAllocatorMarginBytes;estimatedPeakBytes=$stubEstimatedPeakBytes;safetyReserveBytes=($SafetyReserveGiB * 1GB);requiredAvailableBytes=$stubRequiredAvailableBytes;observed=[ordered]@{availablePhysicalBytes=64GB;totalPhysicalBytes=64GB;availableCommitBytes=64GB;commitLimitBytes=64GB;source='synthetic-runtime-only'};gates=[ordered]@{jobBudget=$true;physical=$true;commit=$true}}
        switch ($protocolStub) {
            'Valid' {
                [ordered]@{
                    ok=$true;fallbackToCodex=$false;result=[ordered]@{synthetic=$true;receivedFiles=$Files.Count}
                    resource=[ordered]@{launched=$true;elapsedMilliseconds=1L;peakWorkingSetBytes=1L;inputBytes=0L;promptBytes=1L;exitCode=0;jobAssigned=$true;termination=[ordered]@{graceMilliseconds=5000;timedOut=$false;streamDrainTimedOut=$false};output=[ordered]@{bytes=1L;responseMarkerFound=$true;schemaCandidates=1};requestedGpuLayers=32;modelFile='Qwen3.8-27B-UD-Q4_K_M.gguf';memoryPreflight=$stubPreflight;limits=$stubLimits}
                } | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            'BadJson' { [Console]::Out.Write('{bad-json'); exit 0 }
            'BadReceipt' { [Console]::Out.Write('{"ok":true,"fallbackToCodex":false}'); exit 0 }
            'Stderr' { [Console]::Error.Write('synthetic worker stderr'); exit 0 }
            'LargeStdout' { [Console]::Out.Write(('x' * 300000)); Start-Sleep -Seconds 300; exit 0 }
            'LargeStderr' { [Console]::Error.Write(('x' * 100000)); Start-Sleep -Seconds 300; exit 0 }
            'SuccessNotLaunched' {
                [ordered]@{ok=$true;fallbackToCodex=$false;result=[ordered]@{synthetic=$true};resource=[ordered]@{launched=$false;elapsedMilliseconds=1L;peakWorkingSetBytes=1L;inputBytes=0L;promptBytes=1L;exitCode=0;jobAssigned=$true;output=[ordered]@{bytes=1L;responseMarkerFound=$true;schemaCandidates=1};requestedGpuLayers=0;modelFile='stub.gguf';limits=$stubLimits}} | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            'SuccessBadExit' {
                [ordered]@{ok=$true;fallbackToCodex=$false;result=[ordered]@{synthetic=$true};resource=[ordered]@{launched=$true;elapsedMilliseconds=1L;peakWorkingSetBytes=1L;inputBytes=0L;promptBytes=1L;exitCode='0';jobAssigned=$true;output=[ordered]@{bytes=1L;responseMarkerFound=$true;schemaCandidates=1};requestedGpuLayers=0;modelFile='stub.gguf';limits=$stubLimits}} | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            'SuccessNoJob' {
                [ordered]@{ok=$true;fallbackToCodex=$false;result=[ordered]@{synthetic=$true};resource=[ordered]@{launched=$true;elapsedMilliseconds=1L;peakWorkingSetBytes=1L;inputBytes=0L;promptBytes=1L;exitCode=0;jobAssigned=$false;output=[ordered]@{bytes=1L;responseMarkerFound=$true;schemaCandidates=1};requestedGpuLayers=0;modelFile='stub.gguf';limits=$stubLimits}} | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            'FallbackPretendsSuccess' {
                [ordered]@{ok=$false;fallbackToCodex=$true;error=[ordered]@{code='SyntheticFailure'};resource=[ordered]@{launched=$true;elapsedMilliseconds=1L;peakWorkingSetBytes=1L;inputBytes=0L;promptBytes=1L;exitCode=0;jobAssigned=$true;output=[ordered]@{bytes=1L;responseMarkerFound=$true;schemaCandidates=1};requestedGpuLayers=32;modelFile='Qwen3.8-27B-UD-Q4_K_M.gguf';limits=$stubLimits}} | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            'BadLimits' {
                $stubLimits.timeoutSeconds = [string]$TimeoutSeconds
                [ordered]@{ok=$false;fallbackToCodex=$true;error=[ordered]@{code='SyntheticFailure'};resource=[ordered]@{launched=$false;elapsedMilliseconds=0L;peakWorkingSetBytes=0L;inputBytes=0L;promptBytes=0L;exitCode=$null;jobAssigned=$false;output=[ordered]@{bytes=0L;responseMarkerFound=$false;schemaCandidates=0};limits=$stubLimits}} | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            'BadPreflight' {
                $stubPreflight.gates.physical = $false
                [ordered]@{ok=$true;fallbackToCodex=$false;result=[ordered]@{synthetic=$true};resource=[ordered]@{launched=$true;elapsedMilliseconds=1L;peakWorkingSetBytes=1L;inputBytes=0L;promptBytes=1L;exitCode=0;jobAssigned=$true;output=[ordered]@{bytes=1L;responseMarkerFound=$true;schemaCandidates=1};requestedGpuLayers=32;modelFile='Qwen3.8-27B-UD-Q4_K_M.gguf';memoryPreflight=$stubPreflight;limits=$stubLimits}} | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            'BadPreflightFormula' {
                $stubPreflight.kvCacheBytes++
                $stubPreflight.estimatedPeakBytes++
                $stubPreflight.requiredAvailableBytes++
                [ordered]@{ok=$true;fallbackToCodex=$false;result=[ordered]@{synthetic=$true};resource=[ordered]@{launched=$true;elapsedMilliseconds=1L;peakWorkingSetBytes=1L;inputBytes=0L;promptBytes=1L;exitCode=0;jobAssigned=$true;output=[ordered]@{bytes=1L;responseMarkerFound=$true;schemaCandidates=1};requestedGpuLayers=32;modelFile='Qwen3.8-27B-UD-Q4_K_M.gguf';memoryPreflight=$stubPreflight;limits=$stubLimits}} | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            'PeakOverBudget' {
                [ordered]@{ok=$true;fallbackToCodex=$false;result=[ordered]@{synthetic=$true};resource=[ordered]@{launched=$true;elapsedMilliseconds=1L;peakWorkingSetBytes=(([int64]$MaximumRamGiB * 1GB) + 1L);inputBytes=0L;promptBytes=1L;exitCode=0;jobAssigned=$true;output=[ordered]@{bytes=1L;responseMarkerFound=$true;schemaCandidates=1};requestedGpuLayers=32;modelFile='Qwen3.8-27B-UD-Q4_K_M.gguf';memoryPreflight=$stubPreflight;limits=$stubLimits}} | ConvertTo-Json -Depth 8 -Compress
                exit 0
            }
            default { [Console]::Error.Write('unknown synthetic protocol stub'); exit 2 }
        }
    }
}

if (-not $internalWorker) {
    # 外層監督器不載入模型，也不參與 worker 清理；worker 即使卡在 PowerShell 關閉階段，仍可準時回傳 fallback。
    $supervisorTimer = [Diagnostics.Stopwatch]::StartNew()
    $workerProcess = $null
    $workerStarted = $false
    $workerStdoutTask = $null
    $workerStderrTask = $null
    $workerExitCode = $null
    $workerStdoutBytes = 0
    $workerStderrBytes = 0
    $workerReceiptParsed = $false
    $workerProtocolFailure = 'none'
    $maximumWorkerStdoutBytes = 262144L
    $maximumWorkerStderrBytes = 65536L
    $hardLimitMilliseconds = ($TimeoutSeconds * 1000) + 5000
    $workerArguments = [Collections.Generic.List[string]]::new()
    foreach ($argument in @('-NoProfile','-File',$PSCommandPath,'-Task',$Task)) { $workerArguments.Add([string]$argument) }
    foreach ($argument in @(
        '-MaxTokens',[string]$MaxTokens,
        '-JsonSchema',$JsonSchema,
        '-ProjectRoot',$ProjectRoot,
        '-TimeoutSeconds',[string]$TimeoutSeconds,
        '-CpuThreads',[string]$CpuThreads,
        '-MaximumRamGiB',[string]$MaximumRamGiB,
        '-ContextSize',[string]$ContextSize,
        '-SafetyReserveGiB',[string]$SafetyReserveGiB
    )) { $workerArguments.Add([string]$argument) }

    function Test-ProtocolInteger([object]$Value,[long]$Minimum,[long]$Maximum) {
        return $Value -is [long] -and $Value -ge $Minimum -and $Value -le $Maximum
    }

    function Write-SupervisorFallback([string]$Code,[bool]$WasLaunched,[bool]$TreeKilled,[string]$Phase) {
        [ordered]@{
            ok=$false
            fallbackToCodex=$true
            error=[ordered]@{code=$Code;message='Local QWEN worker did not return a valid receipt; use Codex fallback.'}
            resource=[ordered]@{
                # 協定失敗時無法證明模型 runtime 曾啟動；worker 啟動狀態另列，避免冒充本機推論。
                launched=$false
                elapsedMilliseconds=[int64]$supervisorTimer.ElapsedMilliseconds
                peakWorkingSetBytes=0L
                inputBytes=0L
                promptBytes=0L
                exitCode=$null
                jobAssigned=$false
                supervisor=[ordered]@{phase=$Phase;hardLimitMilliseconds=$hardLimitMilliseconds;treeKilled=$TreeKilled}
                workerProtocol=[ordered]@{workerLaunched=$WasLaunched;exitCode=$workerExitCode;stdoutBytes=$workerStdoutBytes;stderrBytes=$workerStderrBytes;receiptParsed=$workerReceiptParsed;failureReason=$workerProtocolFailure}
                termination=[ordered]@{graceMilliseconds=5000;timedOut=($Code -eq 'LocalQwenSupervisorTimeout');streamDrainTimedOut=$false}
                output=[ordered]@{bytes=0;consoleBytes=0;promptEchoBytes=0;promptEchoPrefixMatched=$false;promptEchoCommonPrefixChars=0;responseMarkerFound=$false;runtimeError='none';runtimeKind='unknown';candidates=0;jsonCandidates=0;schemaCandidates=0;lastCandidateStart=-1;lastCandidateEnd=-1;candidateKeys=@()}
                limits=[ordered]@{timeoutSeconds=$TimeoutSeconds;maxTokens=$MaxTokens;cpuThreads=$CpuThreads;maximumRamBytes=([int64]$MaximumRamGiB * 1GB);contextSize=$ContextSize;safetyReserveBytes=($SafetyReserveGiB * 1GB)}
            }
        } | ConvertTo-Json -Depth 8 -Compress
    }

    try {
        if ($ContextSize -lt 512 -or $ContextSize -gt 32768 -or $SafetyReserveGiB -lt 4 -or $SafetyReserveGiB -gt 16) {
            Write-SupervisorFallback 'LocalQwenConfigurationInvalid' $false $false 'configuration-validation'
            exit 0
        }
        # sender 在建立子程序前先驗證 transport，避免超大環境變數或模糊的陣列繫結進入 worker。
        if ($Files.Count -gt 64) { throw 'The sender file list is oversized.' }
        foreach ($file in $Files) {
            if ($null -eq $file -or [string]::IsNullOrWhiteSpace($file) -or $file.Length -gt 1024) {
                throw 'The sender file list is invalid.'
            }
        }
        $workerFilesJson = ConvertTo-Json -InputObject @($Files) -Compress
        $workerFilesJsonBytes = [Text.Encoding]::UTF8.GetByteCount($workerFilesJson)
        if ($workerFilesJsonBytes -gt $maximumFilesJsonBytes) { throw 'The sender file JSON is oversized.' }
        $workerFilesBase64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($workerFilesJson))
        if ([Text.Encoding]::ASCII.GetByteCount($workerFilesBase64) -gt $maximumFilesBase64Bytes) { throw 'The sender file payload is oversized.' }

        $workerStartInfo = [Diagnostics.ProcessStartInfo]::new()
        $workerStartInfo.FileName = (Get-Command pwsh -ErrorAction Stop).Source
        $workerStartInfo.WorkingDirectory = [IO.Path]::GetFullPath($ProjectRoot)
        $workerStartInfo.UseShellExecute = $false
        $workerStartInfo.CreateNoWindow = $true
        $workerStartInfo.RedirectStandardOutput = $true
        $workerStartInfo.RedirectStandardError = $true
        foreach ($argument in $workerArguments) { $workerStartInfo.ArgumentList.Add($argument) }
        $workerStartInfo.Environment['AEVRYND_QWEN_INTERNAL_WORKER'] = '1'
        $workerStartInfo.Environment['AEVRYND_QWEN_WORKER_FILES_B64'] = $workerFilesBase64
        $workerProcess = [Diagnostics.Process]::new()
        $workerProcess.StartInfo = $workerStartInfo
        if (-not $workerProcess.Start()) { throw 'Worker launch failed.' }
        $workerStarted = $true
        $workerStdoutTask = [AevryndBoundedTextReader]::ReadAsync($workerProcess.StandardOutput,$maximumWorkerStdoutBytes)
        $workerStderrTask = [AevryndBoundedTextReader]::ReadAsync($workerProcess.StandardError,$maximumWorkerStderrBytes)
        while (-not $workerProcess.HasExited) {
            $overflowStream = $null
            if ($workerStdoutTask.IsCompletedSuccessfully -and $workerStdoutTask.Result.LimitExceeded) { $overflowStream = 'stdout' }
            elseif ($workerStderrTask.IsCompletedSuccessfully -and $workerStderrTask.Result.LimitExceeded) { $overflowStream = 'stderr' }
            if ($null -ne $overflowStream) {
                $workerProtocolFailure = 'worker-' + $overflowStream + '-limit'
                if ($workerStdoutTask.IsCompletedSuccessfully) { $workerStdoutBytes = $workerStdoutTask.Result.Utf8Bytes }
                if ($workerStderrTask.IsCompletedSuccessfully) { $workerStderrBytes = $workerStderrTask.Result.Utf8Bytes }
                try { $workerProcess.Kill($true) } catch {}
                try { $workerProcess.WaitForExit(500) | Out-Null } catch {}
                Write-SupervisorFallback 'LocalQwenWorkerProtocolInvalid' $true $true 'worker-stream-limit'
                exit 0
            }
            if ($supervisorTimer.ElapsedMilliseconds -ge $hardLimitMilliseconds) {
                try { $workerProcess.Kill($true) } catch {}
                try { $workerProcess.WaitForExit(500) | Out-Null } catch {}
                Write-SupervisorFallback 'LocalQwenSupervisorTimeout' $true $true 'worker-exit'
                exit 0
            }
            Start-Sleep -Milliseconds 20
        }
        $remainingMilliseconds = [Math]::Max(0,$hardLimitMilliseconds - [int]$supervisorTimer.ElapsedMilliseconds)
        $streamTasks = [Threading.Tasks.Task[]]@($workerStdoutTask,$workerStderrTask)
        $streamsCompleted = try { [Threading.Tasks.Task]::WaitAll($streamTasks,$remainingMilliseconds) } catch { $false }
        if (-not $streamsCompleted) {
            try { $workerProcess.Kill($true) } catch {}
            Write-SupervisorFallback 'LocalQwenSupervisorTimeout' $true $true 'worker-streams'
            exit 0
        }
        $workerStdoutResult = $workerStdoutTask.GetAwaiter().GetResult()
        $workerStderrResult = $workerStderrTask.GetAwaiter().GetResult()
        if ($workerStdoutResult.LimitExceeded -or $workerStderrResult.LimitExceeded) {
            $workerProtocolFailure = if ($workerStdoutResult.LimitExceeded) { 'worker-stdout-limit' } else { 'worker-stderr-limit' }
            $workerStdoutBytes = $workerStdoutResult.Utf8Bytes
            $workerStderrBytes = $workerStderrResult.Utf8Bytes
            Write-SupervisorFallback 'LocalQwenWorkerProtocolInvalid' $true $false 'worker-stream-limit'
            exit 0
        }
        $workerStdout = $workerStdoutResult.Text.Trim()
        $workerStderr = $workerStderrResult.Text.Trim()
        $workerExitCode = $workerProcess.ExitCode
        $workerStdoutBytes = $workerStdoutResult.Utf8Bytes
        $workerStderrBytes = $workerStderrResult.Utf8Bytes
        $workerReceipt = $null
        try { $workerReceipt = $workerStdout | ConvertFrom-Json -ErrorAction Stop; $workerReceiptParsed = $true } catch {}
        if ($workerExitCode -ne 0) { $workerProtocolFailure = 'worker-nonzero-exit' }
        elseif ($workerStderrBytes -ne 0) { $workerProtocolFailure = 'worker-stderr' }
        elseif (-not $workerReceiptParsed) { $workerProtocolFailure = 'receipt-json' }
        else {
            $resource = $workerReceipt.resource
            $receiptInvalid = $workerReceipt -isnot [pscustomobject] -or $workerReceipt.ok -isnot [bool] -or
                $workerReceipt.fallbackToCodex -isnot [bool] -or $workerReceipt.ok -eq $workerReceipt.fallbackToCodex -or
                $resource -isnot [pscustomobject] -or $resource.launched -isnot [bool] -or
                -not (Test-ProtocolInteger $resource.elapsedMilliseconds 0 ([long]::MaxValue)) -or
                -not (Test-ProtocolInteger $resource.peakWorkingSetBytes 0 ([long]::MaxValue)) -or
                -not (Test-ProtocolInteger $resource.inputBytes 0 16384) -or
                -not (Test-ProtocolInteger $resource.promptBytes 0 ([long]::MaxValue)) -or
                $resource.jobAssigned -isnot [bool] -or $resource.output -isnot [pscustomobject] -or
                -not (Test-ProtocolInteger $resource.output.bytes 0 65536) -or $resource.limits -isnot [pscustomobject] -or
                $resource.memoryPreflight -isnot [pscustomobject]
            if (-not $receiptInvalid) {
                $preflight = $resource.memoryPreflight
                $receiptInvalid = -not (Test-ProtocolInteger $resource.limits.timeoutSeconds 10 30) -or $resource.limits.timeoutSeconds -ne $TimeoutSeconds -or
                    -not (Test-ProtocolInteger $resource.limits.maxTokens 1 512) -or $resource.limits.maxTokens -ne $MaxTokens -or
                    -not (Test-ProtocolInteger $resource.limits.cpuThreads 1 4) -or $resource.limits.cpuThreads -ne $CpuThreads -or
                    -not (Test-ProtocolInteger $resource.limits.maximumRamBytes (4GB) (24GB)) -or $resource.limits.maximumRamBytes -ne ([int64]$MaximumRamGiB * 1GB) -or
                    -not (Test-ProtocolInteger $resource.limits.contextSize 512 32768) -or $resource.limits.contextSize -ne $ContextSize -or
                    -not (Test-ProtocolInteger $resource.limits.safetyReserveBytes (4GB) (16GB)) -or $resource.limits.safetyReserveBytes -ne ($SafetyReserveGiB * 1GB) -or
                    $preflight.evaluated -isnot [bool] -or $preflight.passed -isnot [bool] -or [string]::IsNullOrWhiteSpace([string]$preflight.reason) -or
                    -not (Test-ProtocolInteger $preflight.modelBytes 0 (1TB)) -or -not (Test-ProtocolInteger $preflight.contextSize 512 32768) -or
                    $preflight.contextSize -ne $ContextSize -or -not (Test-ProtocolInteger $preflight.kvCacheBytes 0 ([long]::MaxValue)) -or
                    -not (Test-ProtocolInteger $preflight.runtimeOverheadBytes 0 ([long]::MaxValue)) -or -not (Test-ProtocolInteger $preflight.allocatorMarginBytes 0 ([long]::MaxValue)) -or
                    -not (Test-ProtocolInteger $preflight.estimatedPeakBytes 0 ([long]::MaxValue)) -or -not (Test-ProtocolInteger $preflight.safetyReserveBytes 0 ([long]::MaxValue)) -or
                    -not (Test-ProtocolInteger $preflight.requiredAvailableBytes 0 ([long]::MaxValue)) -or $preflight.observed -isnot [pscustomobject] -or $preflight.gates -isnot [pscustomobject] -or
                    -not (Test-ProtocolInteger $preflight.observed.availablePhysicalBytes 0 ([long]::MaxValue)) -or -not (Test-ProtocolInteger $preflight.observed.totalPhysicalBytes 0 ([long]::MaxValue)) -or
                    -not (Test-ProtocolInteger $preflight.observed.availableCommitBytes 0 ([long]::MaxValue)) -or -not (Test-ProtocolInteger $preflight.observed.commitLimitBytes 0 ([long]::MaxValue)) -or
                    $preflight.gates.jobBudget -isnot [bool] -or $preflight.gates.physical -isnot [bool] -or $preflight.gates.commit -isnot [bool]
            }
            if (-not $receiptInvalid -and $preflight.evaluated) {
                $expectedKvCache = [decimal]$ContextSize * 393216
                $expectedRuntimeOverhead = [Math]::Max(1GB,[int64][Math]::Ceiling([decimal]$preflight.modelBytes / 16))
                $expectedAllocatorMargin = 512MB
                $expectedSafetyReserve = [decimal]$SafetyReserveGiB * 1GB
                $expectedPeak = [decimal]$preflight.modelBytes + $expectedKvCache + $expectedRuntimeOverhead + $expectedAllocatorMargin
                $expectedRequired = $expectedPeak + $preflight.safetyReserveBytes
                $expectedJobGate = ([int64]$MaximumRamGiB * 1GB) -ge $preflight.estimatedPeakBytes
                $expectedPhysicalGate = $preflight.observed.availablePhysicalBytes -ge $preflight.requiredAvailableBytes
                $expectedCommitGate = $preflight.observed.availableCommitBytes -ge $preflight.requiredAvailableBytes
                $expectedPassed = $expectedJobGate -and $expectedPhysicalGate -and $expectedCommitGate
                $expectedReason = if (-not $expectedJobGate) {'job-budget-insufficient'} elseif (-not $expectedPhysicalGate) {'physical-memory-insufficient'} elseif (-not $expectedCommitGate) {'commit-insufficient'} else {'ok'}
                $receiptInvalid = $expectedKvCache -gt [long]::MaxValue -or $expectedPeak -gt [long]::MaxValue -or $expectedRequired -gt [long]::MaxValue -or
                    $preflight.kvCacheBytes -ne [int64]$expectedKvCache -or $preflight.runtimeOverheadBytes -ne [int64]$expectedRuntimeOverhead -or
                    $preflight.allocatorMarginBytes -ne [int64]$expectedAllocatorMargin -or $preflight.safetyReserveBytes -ne [int64]$expectedSafetyReserve -or
                    $preflight.estimatedPeakBytes -ne [int64]$expectedPeak -or $preflight.requiredAvailableBytes -ne [int64]$expectedRequired -or
                    $preflight.observed.availablePhysicalBytes -gt $preflight.observed.totalPhysicalBytes -or $preflight.observed.availableCommitBytes -gt $preflight.observed.commitLimitBytes -or
                    $preflight.gates.jobBudget -ne $expectedJobGate -or $preflight.gates.physical -ne $expectedPhysicalGate -or $preflight.gates.commit -ne $expectedCommitGate -or
                    $preflight.passed -ne $expectedPassed -or $preflight.reason -ne $expectedReason
            } elseif (-not $receiptInvalid) {
                $receiptInvalid = $preflight.passed -or $preflight.reason -ne 'not-evaluated' -or $preflight.modelBytes -ne 0 -or
                    $preflight.kvCacheBytes -ne 0 -or $preflight.runtimeOverheadBytes -ne 0 -or $preflight.allocatorMarginBytes -ne 0 -or
                    $preflight.estimatedPeakBytes -ne 0 -or $preflight.requiredAvailableBytes -ne 0 -or
                    $preflight.gates.jobBudget -or $preflight.gates.physical -or $preflight.gates.commit
            }
            if (-not $receiptInvalid -and $workerReceipt.ok) {
                $receiptInvalid = $workerReceipt.result -isnot [pscustomobject] -or $null -ne $workerReceipt.PSObject.Properties['error'] -or
                    -not $resource.launched -or -not $resource.jobAssigned -or -not $preflight.evaluated -or -not $preflight.passed -or -not (Test-ProtocolInteger $resource.exitCode 0 0) -or
                    $resource.peakWorkingSetBytes -lt 1 -or $resource.peakWorkingSetBytes -gt ([int64]$MaximumRamGiB * 1GB) -or $resource.promptBytes -lt 1 -or $resource.promptBytes -gt 16384 -or
                    $resource.output.bytes -lt 1 -or
                    $resource.output.responseMarkerFound -isnot [bool] -or -not $resource.output.responseMarkerFound -or
                    -not (Test-ProtocolInteger $resource.output.schemaCandidates 1 ([long]::MaxValue)) -or
                    -not (Test-ProtocolInteger $resource.requestedGpuLayers 32 32) -or
                    $resource.modelFile -ne 'Qwen3.8-27B-UD-Q4_K_M.gguf'
            } elseif (-not $receiptInvalid) {
                $receiptInvalid = $workerReceipt.error -isnot [pscustomobject] -or [string]::IsNullOrWhiteSpace([string]$workerReceipt.error.code) -or
                    $null -ne $workerReceipt.PSObject.Properties['result']
                if (-not $receiptInvalid -and $resource.launched) {
                    $receiptInvalid = -not $preflight.evaluated -or -not $preflight.passed -or ($null -ne $resource.exitCode -and
                        -not (Test-ProtocolInteger $resource.exitCode ([int]::MinValue) ([int]::MaxValue))
                    )
                    if (-not $receiptInvalid -and $resource.jobAssigned -and (Test-ProtocolInteger $resource.exitCode 0 0) -and
                        $resource.output.responseMarkerFound -is [bool] -and $resource.output.responseMarkerFound -and
                        (Test-ProtocolInteger $resource.output.schemaCandidates 1 ([long]::MaxValue))) {
                        $receiptInvalid = $true
                    }
                } elseif (-not $receiptInvalid) {
                    $receiptInvalid = $resource.jobAssigned -or $null -ne $resource.exitCode
                }
            }
            if ($receiptInvalid) { $workerProtocolFailure = 'receipt-schema' }
        }
        if ($workerProtocolFailure -ne 'none') {
            Write-SupervisorFallback 'LocalQwenWorkerProtocolInvalid' $true $false 'receipt-validation'
            exit 0
        }
        # 重新序列化已解析的單一物件，避免 worker 將額外 console 文字帶給呼叫端。
        $workerReceipt | ConvertTo-Json -Depth 100 -Compress
    } catch {
        $treeKilled = $false
        if ($null -ne $workerProcess) {
            try { if ($workerStarted -and -not $workerProcess.HasExited) { $workerProcess.Kill($true); $treeKilled = $true } } catch {}
        }
        Write-SupervisorFallback 'LocalQwenSupervisorFailed' $workerStarted $treeKilled 'supervisor-exception'
    } finally {
        if ($null -ne $workerProcess) { try { $workerProcess.Dispose() } catch {} }
    }
    exit 0
}

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
$maximumRuntimeStdoutBytes = 262144L
$maximumRuntimeStderrBytes = 262144L
$terminationGraceMilliseconds = 5000
$terminationTimedOut = $false
$streamDrainTimedOut = $false
$syntheticMode = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_SYNTHETIC_MODE')
$terminationTimer = $null
$tracePath = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TEST_TRACE_PATH')
$modelLockStream = $null
$memoryPreflight = [ordered]@{evaluated=$false;passed=$false;reason='not-evaluated';modelBytes=0L;contextSize=$ContextSize;kvCacheBytes=0L;runtimeOverheadBytes=0L;allocatorMarginBytes=0L;estimatedPeakBytes=0L;safetyReserveBytes=0L;requiredAvailableBytes=0L;observed=[ordered]@{availablePhysicalBytes=0L;totalPhysicalBytes=0L;availableCommitBytes=0L;commitLimitBytes=0L;source='none'};gates=[ordered]@{jobBudget=$false;physical=$false;commit=$false}}

function Write-WorkerTrace([string]$Phase) {
    if ([Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -ne '1' -or [string]::IsNullOrWhiteSpace($script:tracePath)) { return }
    try {
        $fullTracePath = [IO.Path]::GetFullPath($script:tracePath)
        $systemTemporaryPath = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
        if ($fullTracePath.StartsWith($systemTemporaryPath + '\',[StringComparison]::OrdinalIgnoreCase)) {
            [IO.File]::AppendAllText($fullTracePath,"$Phase`n",[Text.UTF8Encoding]::new($false))
        }
    } catch {}
}

Write-WorkerTrace 'worker-start'

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

if (-not ('AevryndMemoryProbe' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

public sealed class AevryndMemorySnapshot
{
    public long TotalPhysicalBytes { get; set; }
    public long AvailablePhysicalBytes { get; set; }
    public long CommitLimitBytes { get; set; }
    public long AvailableCommitBytes { get; set; }
}

public static class AevryndMemoryProbe
{
    [StructLayout(LayoutKind.Sequential)]
    private struct PerformanceInformation
    {
        public uint Size;
        public UIntPtr CommitTotal;
        public UIntPtr CommitLimit;
        public UIntPtr CommitPeak;
        public UIntPtr PhysicalTotal;
        public UIntPtr PhysicalAvailable;
        public UIntPtr SystemCache;
        public UIntPtr KernelTotal;
        public UIntPtr KernelPaged;
        public UIntPtr KernelNonpaged;
        public UIntPtr PageSize;
        public uint HandleCount;
        public uint ProcessCount;
        public uint ThreadCount;
    }

    [DllImport("psapi.dll", SetLastError = true)]
    private static extern bool GetPerformanceInfo(out PerformanceInformation value, uint size);

    private static long PagesToBytes(UIntPtr pages, ulong pageSize)
    {
        ulong count = pages.ToUInt64();
        if (pageSize == 0 || count > ((ulong)long.MaxValue / pageSize))
            throw new OverflowException("Memory counter overflow.");
        return checked((long)(count * pageSize));
    }

    public static AevryndMemorySnapshot Read()
    {
        PerformanceInformation value;
        uint size = (uint)Marshal.SizeOf(typeof(PerformanceInformation));
        if (!GetPerformanceInfo(out value, size))
            throw new Win32Exception(Marshal.GetLastWin32Error());
        ulong pageSize = value.PageSize.ToUInt64();
        long commitLimit = PagesToBytes(value.CommitLimit, pageSize);
        long commitTotal = PagesToBytes(value.CommitTotal, pageSize);
        if (commitTotal < 0 || commitTotal > commitLimit)
            throw new InvalidOperationException("Invalid commit counters.");
        return new AevryndMemorySnapshot {
            TotalPhysicalBytes = PagesToBytes(value.PhysicalTotal, pageSize),
            AvailablePhysicalBytes = PagesToBytes(value.PhysicalAvailable, pageSize),
            CommitLimitBytes = commitLimit,
            AvailableCommitBytes = commitLimit - commitTotal
        };
    }
}
'@
}

function Convert-TestMemoryValue([string]$Name) {
    $raw = [Environment]::GetEnvironmentVariable($Name)
    $value = 0L
    if ([string]::IsNullOrWhiteSpace($raw) -or -not [long]::TryParse($raw,[Globalization.NumberStyles]::None,[Globalization.CultureInfo]::InvariantCulture,[ref]$value) -or $value -lt 0) {
        throw "Synthetic memory value is invalid: $Name"
    }
    return $value
}

function Get-MemoryPreflight([long]$ModelBytes) {
    if ($ContextSize -lt 512 -or $ContextSize -gt 32768 -or $SafetyReserveGiB -lt 4 -or $SafetyReserveGiB -gt 16) {
        throw 'Local QWEN memory configuration is outside the safe policy.'
    }
    if ($ModelBytes -lt 1 -or $ModelBytes -gt 1TB) { throw 'The model size is outside the safe policy.' }

    # 以每 token 384 KiB 保守涵蓋 27B GQA KV cache，再加入模型解壓/映射與 allocator 餘量。
    $kvCacheBytes = [int64]([decimal]$ContextSize * 393216)
    $runtimeOverheadBytes = [Math]::Max(1GB,[int64][Math]::Ceiling([decimal]$ModelBytes / 16))
    $allocatorMarginBytes = 512MB
    $estimatedPeakDecimal = [decimal]$ModelBytes + $kvCacheBytes + $runtimeOverheadBytes + $allocatorMarginBytes
    $safetyReserveBytes = [int64]([decimal]$SafetyReserveGiB * 1GB)
    $requiredAvailableDecimal = $estimatedPeakDecimal + $safetyReserveBytes
    if ($estimatedPeakDecimal -gt [long]::MaxValue -or $requiredAvailableDecimal -gt [long]::MaxValue) { throw 'The memory estimate overflowed.' }
    $estimatedPeakBytes = [int64]$estimatedPeakDecimal
    $requiredAvailableBytes = [int64]$requiredAvailableDecimal

    $recognizedSyntheticModes = @('StubSuccess','StubBadMarker','StubBadSchema','StubRamGate','StubRuntimeLargeStdout','StubRuntimeLargeStderr','HungStream','HostHangAfterChildExit')
    $isSyntheticRuntime = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1' -and $syntheticMode -in $recognizedSyntheticModes
    $overrideNames = @('AEVRYND_QWEN_TEST_TOTAL_PHYSICAL_BYTES','AEVRYND_QWEN_TEST_AVAILABLE_PHYSICAL_BYTES','AEVRYND_QWEN_TEST_COMMIT_LIMIT_BYTES','AEVRYND_QWEN_TEST_AVAILABLE_COMMIT_BYTES')
    $provided = @($overrideNames | Where-Object { -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($_)) })
    if (-not $isSyntheticRuntime -and $provided.Count -gt 0) { throw 'Synthetic memory counters require a synthetic runtime.' }
    $source = 'operating-system'
    if ($isSyntheticRuntime) {
        # 測試覆寫只能配合不可能選到 llama-cli.exe 的合成 runtime，且四個計數必須成組提供。
        if ($provided.Count -eq 0) {
            $totalPhysicalBytes=64GB;$availablePhysicalBytes=64GB;$commitLimitBytes=64GB;$availableCommitBytes=64GB
        } elseif ($provided.Count -eq $overrideNames.Count) {
            $totalPhysicalBytes=Convert-TestMemoryValue $overrideNames[0]
            $availablePhysicalBytes=Convert-TestMemoryValue $overrideNames[1]
            $commitLimitBytes=Convert-TestMemoryValue $overrideNames[2]
            $availableCommitBytes=Convert-TestMemoryValue $overrideNames[3]
        } else { throw 'Synthetic memory counters must be supplied as one complete set.' }
        $source = 'synthetic-runtime-only'
    } else {
        $snapshot = [AevryndMemoryProbe]::Read()
        $totalPhysicalBytes=$snapshot.TotalPhysicalBytes;$availablePhysicalBytes=$snapshot.AvailablePhysicalBytes
        $commitLimitBytes=$snapshot.CommitLimitBytes;$availableCommitBytes=$snapshot.AvailableCommitBytes
    }
    if ($totalPhysicalBytes -lt 1 -or $availablePhysicalBytes -gt $totalPhysicalBytes -or $commitLimitBytes -lt 1 -or $availableCommitBytes -gt $commitLimitBytes) {
        throw 'Memory counters are inconsistent.'
    }
    $maximumBytes = [int64]$MaximumRamGiB * 1GB
    $jobGate = $maximumBytes -ge $estimatedPeakBytes
    $physicalGate = $availablePhysicalBytes -ge $requiredAvailableBytes
    $commitGate = $availableCommitBytes -ge $requiredAvailableBytes
    $passed = $jobGate -and $physicalGate -and $commitGate
    $reason = if (-not $jobGate) {'job-budget-insufficient'} elseif (-not $physicalGate) {'physical-memory-insufficient'} elseif (-not $commitGate) {'commit-insufficient'} else {'ok'}
    return [ordered]@{evaluated=$true;passed=$passed;reason=$reason;modelBytes=$ModelBytes;contextSize=$ContextSize;kvCacheBytes=$kvCacheBytes;runtimeOverheadBytes=$runtimeOverheadBytes;allocatorMarginBytes=$allocatorMarginBytes;estimatedPeakBytes=$estimatedPeakBytes;safetyReserveBytes=$safetyReserveBytes;requiredAvailableBytes=$requiredAvailableBytes;observed=[ordered]@{availablePhysicalBytes=$availablePhysicalBytes;totalPhysicalBytes=$totalPhysicalBytes;availableCommitBytes=$availableCommitBytes;commitLimitBytes=$commitLimitBytes;source=$source};gates=[ordered]@{jobBudget=$jobGate;physical=$physicalGate;commit=$commitGate}}
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
            termination=[ordered]@{graceMilliseconds=$terminationGraceMilliseconds;timedOut=$terminationTimedOut;streamDrainTimedOut=$streamDrainTimedOut}
            output=[ordered]@{bytes=$outputBytes;consoleBytes=$consoleBytes;promptEchoBytes=$promptEchoBytes;promptEchoPrefixMatched=$promptEchoPrefixMatched;promptEchoCommonPrefixChars=$promptEchoCommonPrefixChars;responseMarkerFound=$responseMarkerFound;runtimeError=$runtimeOutputError;runtimeKind=$runtimeOutputKind;candidates=$outputCandidateCount;jsonCandidates=$outputJsonCandidateCount;schemaCandidates=$outputSchemaCandidateCount;lastCandidateStart=$outputLastCandidateStart;lastCandidateEnd=$outputLastCandidateEnd;candidateKeys=$outputCandidateKeys}
            memoryPreflight=$memoryPreflight
            limits=[ordered]@{timeoutSeconds=$TimeoutSeconds;maxTokens=$MaxTokens;cpuThreads=$CpuThreads;maximumRamBytes=([int64]$MaximumRamGiB * 1GB);contextSize=$ContextSize;safetyReserveBytes=($SafetyReserveGiB * 1GB)}
        }
    } | ConvertTo-Json -Depth 8 -Compress
}

function Wait-RedirectedStreams([int]$Milliseconds) {
    $tasks = @($script:stdoutTask,$script:stderrTask) | Where-Object { $null -ne $_ }
    if ($tasks.Count -eq 0) { return $true }
    try { return [Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]$tasks,$Milliseconds) } catch { return $false }
}

function Stop-BoundedRuntime {
    # 先關閉 Job，讓 KILL_ON_JOB_CLOSE 終止整棵程序樹；不可先對主程序做無限期 WaitForExit。
    if ($null -eq $script:terminationTimer) { $script:terminationTimer = [Diagnostics.Stopwatch]::StartNew() }
    if ($null -ne $script:jobHandle) {
        try { $script:jobHandle.Dispose() } catch {}
        $script:jobHandle = $null
    }
    if ($null -ne $script:process) {
        try {
            if (-not $script:process.HasExited) { $script:process.Kill($false) }
            $remaining = [Math]::Max(0,$script:terminationGraceMilliseconds - [int]$script:terminationTimer.ElapsedMilliseconds)
            if (-not $script:process.WaitForExit($remaining)) {
                $script:terminationTimedOut = $true
            } elseif ($script:exitCode -eq $null) {
                try { $script:exitCode = $script:process.ExitCode } catch {}
            }
        } catch { $script:terminationTimedOut = $true }
    }
    $streamRemaining = [Math]::Max(0,$script:terminationGraceMilliseconds - [int]$script:terminationTimer.ElapsedMilliseconds)
    if (-not (Wait-RedirectedStreams ([Math]::Min(500,$streamRemaining)))) { $script:streamDrainTimedOut = $true }
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
    if ($null -ne $workerBootstrapError) {
        $failurePhase = 'worker-bootstrap'
        throw $workerBootstrapError
    }
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
    $canonicalInputPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
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
            if (-not $canonicalInputPaths.Add($finalPath)) { throw 'Duplicate input files are not allowed.' }
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
    $failurePhase = 'memory-preflight'
    $recognizedSyntheticRuntime = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1' -and $syntheticMode -in @('StubSuccess','StubBadMarker','StubBadSchema','StubRamGate','StubRuntimeLargeStdout','StubRuntimeLargeStderr','HungStream','HostHangAfterChildExit')
    if ($recognizedSyntheticRuntime) {
        $modelBytes = (Get-Item -LiteralPath $modelPath -Force).Length
        $syntheticModelBytes = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TEST_MODEL_BYTES')
        if (-not [string]::IsNullOrWhiteSpace($syntheticModelBytes)) { $modelBytes = Convert-TestMemoryValue 'AEVRYND_QWEN_TEST_MODEL_BYTES' }
    } else {
        # 持有不允許寫入、刪除或重新命名的唯讀 handle，封閉長度檢查到 runtime 開檔之間的 TOCTOU。
        $modelLockStream = [IO.File]::Open($modelPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        $lockedModelPath = [IO.Path]::GetFullPath([AevryndPathSecurity]::GetFinalPath($modelLockStream.SafeFileHandle))
        if (-not $lockedModelPath.Equals([IO.Path]::GetFullPath($modelPath),[StringComparison]::OrdinalIgnoreCase)) { throw 'The model path changed while it was opened.' }
        $modelBytes = $modelLockStream.Length
    }
    $memoryPreflight = Get-MemoryPreflight $modelBytes
    if (-not $memoryPreflight.passed) {
        $failureCode = 'LocalQwenPreflightRejected'
        throw ('Local QWEN pre-start memory gate rejected launch: ' + $memoryPreflight.reason)
    }
    $failurePhase = 'runtime-launch'
    Write-WorkerTrace 'runtime-launch-before'
    # Chat template 會讓結束回合的特殊 token 經過 JSON grammar 而初始化失敗；completion 模式沒有此衝突。
    # 固定短 prompt 讓 single-turn 進入生成；完整內容與 schema 仍只經過暫存檔傳入。
    $arguments = @('-m',$modelPath,'--prompt','Follow the complete task from the attached prompt file.','--file',$promptPath,'-n',[string]$MaxTokens,'-c',[string]$ContextSize,'-t',[string]$CpuThreads,'-tb',[string]$CpuThreads,'-ngl','32','--flash-attn','auto','--reasoning','off','--reasoning-budget','0','--json-schema-file',$schemaPath,'--output-file',$outputPath,'--log-file',$logPath,'--no-jinja','--single-turn','--simple-io','--no-display-prompt','--color','off','--no-perf')
    if ($syntheticMode -in @('StubSuccess','StubBadMarker','StubBadSchema') -and [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1') {
        # 以本機短命 PowerShell stub 驗證 marker/schema 協定，完全不載入真實模型。
        $syntheticResponse = if ($syntheticMode -eq 'StubBadSchema') { '{"answer":3}' } else { '{"answer":2}' }
        $syntheticContents = if ($syntheticMode -eq 'StubBadMarker') { $syntheticResponse } else { $responseMarker + $syntheticResponse }
        $escapedOutputPath = $outputPath.Replace("'","''")
        $escapedSyntheticOutput = $syntheticContents.Replace("'","''")
        $startInfoFileName = (Get-Command pwsh -ErrorAction Stop).Source
        $childCommand = "[IO.File]::WriteAllText('$escapedOutputPath','$escapedSyntheticOutput',[Text.UTF8Encoding]::new(`$false)); exit 0"
        $arguments = @('-NoProfile','-Command',$childCommand)
    } elseif ($syntheticMode -eq 'StubRamGate' -and [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1') {
        # RAM gate 測試由 worker 主迴圈注入超限收據；stub 本身只等待被安全終止。
        $startInfoFileName = (Get-Command pwsh -ErrorAction Stop).Source
        $arguments = @('-NoProfile','-Command','Start-Sleep -Seconds 300')
    } elseif ($syntheticMode -in @('StubRuntimeLargeStdout','StubRuntimeLargeStderr') -and [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1') {
        # 大量輸出 stub 驗證 worker 也會限制 runtime pipe，而非只限制外層收據 pipe。
        $startInfoFileName = (Get-Command pwsh -ErrorAction Stop).Source
        $streamExpression = if ($syntheticMode -eq 'StubRuntimeLargeStdout') { '[Console]::Out' } else { '[Console]::Error' }
        $arguments = @('-NoProfile','-Command',"$streamExpression.Write(('x' * 300000)); Start-Sleep -Seconds 300")
    } elseif ($syntheticMode -eq 'HungStream' -and [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1') {
        # 測試模式建立會持有輸出 pipe 的子孫程序，驗證 timeout 不會被 stream EOF 卡住。
        $syntheticMarker = [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TEST_MARKER')
        if ($syntheticMarker -notmatch '^[a-f0-9]{32}$') { throw 'Synthetic test marker is invalid.' }
        $startInfoFileName = (Get-Command pwsh -ErrorAction Stop).Source
        $childCommand = '$nested=Start-Process pwsh -ArgumentList @(''-NoProfile'',''-Command'',''Start-Sleep -Seconds 300'',''{0}'') -NoNewWindow -PassThru; [Console]::Out.Write((''x''*65536)); Start-Sleep -Seconds 300' -f $syntheticMarker
        $arguments = @('-NoProfile','-Command',$childCommand)
    } elseif ($syntheticMode -eq 'HostHangAfterChildExit' -and [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1') {
        # 子程序正常退出並建立有效結果，但 worker 在 finally 後掛住，用來驗證外層監督器的硬界線。
        $escapedOutputPath = $outputPath.Replace("'","''")
        $escapedSyntheticOutput = ($responseMarker + '{"synthetic":true}').Replace("'","''")
        $startInfoFileName = (Get-Command pwsh -ErrorAction Stop).Source
        $childCommand = "[IO.File]::WriteAllText('$escapedOutputPath','$escapedSyntheticOutput',[Text.UTF8Encoding]::new(`$false)); exit 0"
        $arguments = @('-NoProfile','-Command',$childCommand)
    } else {
        $startInfoFileName = $runtimePath
    }
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $startInfoFileName
    $startInfo.WorkingDirectory = $temporaryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in $arguments) { $startInfo.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw 'Local QWEN could not be started.' }
    Write-WorkerTrace 'runtime-process-started'
    $stdoutTask = [AevryndBoundedTextReader]::ReadAsync($process.StandardOutput,$maximumRuntimeStdoutBytes)
    $stderrTask = [AevryndBoundedTextReader]::ReadAsync($process.StandardError,$maximumRuntimeStderrBytes)
    $launched = $true
    $maximumBytes = [int64]$MaximumRamGiB * 1GB
    $jobHandle = [AevryndPathSecurity]::CreateBoundedKillOnCloseJob($process.Handle,[uint64]$maximumBytes)
    $jobAssigned = $true
    if ($null -ne $modelLockStream -and $modelLockStream.Length -ne $memoryPreflight.modelBytes) { Stop-BoundedRuntime;throw 'The locked model length changed unexpectedly.' }
    try { $process.PriorityClass = [Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $failurePhase = 'runtime-execution'
    if ($syntheticMode -eq 'StubRamGate' -and [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1') {
        $peakWorkingSetBytes = $maximumBytes + 1
        $failureCode = 'LocalQwenRamBudgetExceeded'
        Stop-BoundedRuntime
        throw 'Synthetic runtime exceeded its RAM budget.'
    }
    while (-not $process.HasExited) {
        if (($stdoutTask.IsCompletedSuccessfully -and $stdoutTask.Result.LimitExceeded) -or
            ($stderrTask.IsCompletedSuccessfully -and $stderrTask.Result.LimitExceeded)) {
            $failureCode = 'LocalQwenRuntimeOutputExceeded'
            $runtimeOutputError = 'output-limit'
            if ($stdoutTask.IsCompletedSuccessfully) { $consoleBytes = $stdoutTask.Result.Utf8Bytes }
            Stop-BoundedRuntime
            throw 'Local QWEN exceeded its redirected output budget.'
        }
        if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) { Stop-BoundedRuntime;throw 'Local QWEN timed out.' }
        $process.Refresh()
        if ($process.WorkingSet64 -gt $peakWorkingSetBytes) { $peakWorkingSetBytes=$process.WorkingSet64 }
        if ($process.WorkingSet64 -gt $maximumBytes) { $failureCode = 'LocalQwenRamBudgetExceeded';Stop-BoundedRuntime;throw 'Local QWEN exceeded its RAM budget.' }
        Start-Sleep -Milliseconds 100
    }
    Write-WorkerTrace 'runtime-process-exited'
    if (-not $process.WaitForExit($terminationGraceMilliseconds)) { Stop-BoundedRuntime;throw 'Local QWEN did not complete process shutdown.' }
    $exitCode = $process.ExitCode
    if ($process.ExitCode -ne 0) {
        throw "Local QWEN exited with code $($process.ExitCode)."
    }
    if (-not (Wait-RedirectedStreams $terminationGraceMilliseconds)) {
        $streamDrainTimedOut = $true
        Stop-BoundedRuntime
        throw 'Local QWEN did not close redirected output streams.'
    }
    Write-WorkerTrace 'runtime-streams-drained'
    $runtimeStdoutResult = $stdoutTask.GetAwaiter().GetResult()
    $runtimeStderrResult = $stderrTask.GetAwaiter().GetResult()
    if ($runtimeStdoutResult.LimitExceeded -or $runtimeStderrResult.LimitExceeded) {
        $failureCode = 'LocalQwenRuntimeOutputExceeded'
        throw 'Local QWEN exceeded its redirected output budget.'
    }
    $consoleText = $runtimeStdoutResult.Text
    $stderrText = $runtimeStderrResult.Text
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
    Write-WorkerTrace 'receipt-serialization-before'
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
            memoryPreflight=$memoryPreflight
            limits=[ordered]@{timeoutSeconds=$TimeoutSeconds;maxTokens=$MaxTokens;cpuThreads=$CpuThreads;maximumRamBytes=([int64]$MaximumRamGiB * 1GB);contextSize=$ContextSize;safetyReserveBytes=($SafetyReserveGiB * 1GB)}
        }
    } | ConvertTo-Json -Depth 100 -Compress
    Write-WorkerTrace 'receipt-serialization-after'
} catch {
    # 僅回傳固定階段名稱，讓呼叫端可診斷而不暴露路徑或模型輸出。
    $code = if ($null -ne $failureCode) { $failureCode } else { 'LocalQwenFailed.' + $failurePhase }
    Write-WorkerTrace ('fallback-before-' + $failurePhase)
    Write-Fallback $code 'Local QWEN could not complete this bounded task; use Codex fallback.'
    Write-WorkerTrace ('fallback-after-' + $failurePhase)
} finally {
    Write-WorkerTrace 'finally-stop-before'
    Stop-BoundedRuntime
    Write-WorkerTrace 'finally-stop-after'
    if ($null -ne $process) { try { $process.Dispose() } catch {} }
    if ($null -ne $modelLockStream) { try { $modelLockStream.Dispose() } catch {} }
    Write-WorkerTrace 'finally-process-disposed'
    if (Test-Path -LiteralPath $temporaryRoot) { Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue }
    Write-WorkerTrace 'finally-temp-removed'
    if ($syntheticMode -eq 'HostHangAfterChildExit' -and [Environment]::GetEnvironmentVariable('AEVRYND_QWEN_TESTING') -eq '1') {
        Write-WorkerTrace 'synthetic-host-hang'
        Start-Sleep -Seconds 300
    }
}
