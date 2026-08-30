param(
    [switch]$IncludeSmoke
)

$ErrorActionPreference = 'Stop'
$scriptPath = (Resolve-Path (Join-Path $PSScriptRoot 'invoke-local-qwen.ps1')).Path
$multiFileCallerPath = (Resolve-Path (Join-Path $PSScriptRoot 'test-local-qwen-multifile-caller.ps1')).Path
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-qwen-wrapper-test-' + [Guid]::NewGuid().ToString('N'))
$results = [Collections.Generic.List[object]]::new()

function Invoke-Wrapper([string]$Name,[string[]]$Arguments,[bool]$ExpectLaunch,[bool]$ExpectSuccess,[string]$ExpectedErrorCode = '',[hashtable]$Environment = @{},[ValidateRange(1,60)][int]$MaximumWallSeconds = 20,[string]$EntryScript = $scriptPath) {
    $caseRoot = Join-Path $temporaryRoot $Name
    New-Item -ItemType Directory -Path $caseRoot | Out-Null
    $stdoutPath = Join-Path $caseRoot 'stdout.json'
    $stderrPath = Join-Path $caseRoot 'stderr.txt'
    # ProcessStartInfo.ArgumentList 可保留 JSON schema 與空白參數的原始邊界。
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = (Get-Command pwsh -ErrorAction Stop).Source
    $startInfo.WorkingDirectory = $caseRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in (@('-NoProfile','-File',$EntryScript) + $Arguments)) { $startInfo.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    foreach ($entry in $Environment.GetEnumerator()) { $process.StartInfo.Environment[$entry.Key] = [string]$entry.Value }
    $wallTimer = [Diagnostics.Stopwatch]::StartNew()
    if (-not $process.Start()) { throw "Could not start wrapper test case: $Name" }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $completedInTime = $process.WaitForExit($MaximumWallSeconds * 1000)
    if (-not $completedInTime) { try { $process.Kill($true) } catch {}; $process.WaitForExit(2000) | Out-Null }
    $streams = [Threading.Tasks.Task[]]@($stdoutTask,$stderrTask)
    $streamsCompleted = try { [Threading.Tasks.Task]::WaitAll($streams,2000) } catch { $false }
    $stdout = if ($streamsCompleted) { $stdoutTask.GetAwaiter().GetResult().Trim() } else { '' }
    $stderr = if ($streamsCompleted) { $stderrTask.GetAwaiter().GetResult().Trim() } else { '' }
    $exitCode = try { if ($process.HasExited) { $process.ExitCode } else { -1 } } catch { -1 }
    $process.Dispose()
    $payload = $null
    try { $payload = $stdout | ConvertFrom-Json -ErrorAction Stop } catch {}
    $passed = $completedInTime -and $streamsCompleted -and $exitCode -eq 0 -and $null -ne $payload -and
        [bool]$payload.resource.launched -eq $ExpectLaunch -and [bool]$payload.ok -eq $ExpectSuccess -and
        [bool]$payload.fallbackToCodex -eq (-not $ExpectSuccess) -and $stderr.Length -eq 0
    if ($ExpectedErrorCode.Length -gt 0) { $passed = $passed -and $payload.error.code -eq $ExpectedErrorCode }
    if ($null -ne $payload) {
        $passed = $passed -and $payload.resource.limits.timeoutSeconds -le 30 -and
            $payload.resource.limits.maxTokens -le 512 -and $payload.resource.limits.cpuThreads -le 4 -and
            $payload.resource.limits.maximumRamBytes -le 24GB
        if ($null -ne $payload.resource.termination) {
            $passed = $passed -and $payload.resource.termination.graceMilliseconds -le 5000
        }
    }
    $results.Add([pscustomobject]@{
        Name = $Name
        Passed = $passed
        ExitCode = $exitCode
        Launched = if ($null -ne $payload) { [bool]$payload.resource.launched } else { $null }
        Ok = if ($null -ne $payload) { [bool]$payload.ok } else { $null }
        Fallback = if ($null -ne $payload) { [bool]$payload.fallbackToCodex } else { $null }
        ErrorCode = if ($null -ne $payload) { $payload.error.code } else { $null }
        PromptBytes = if ($null -ne $payload) { $payload.resource.promptBytes } else { $null }
        ElapsedMilliseconds = if ($null -ne $payload) { $payload.resource.elapsedMilliseconds } else { $null }
        PeakWorkingSetBytes = if ($null -ne $payload) { $payload.resource.peakWorkingSetBytes } else { $null }
        WallMilliseconds = [int64]$wallTimer.ElapsedMilliseconds
        CompletedInTime = $completedInTime
        StreamsCompleted = $streamsCompleted
        TerminationTimedOut = if ($null -ne $payload) { [bool]$payload.resource.termination.timedOut } else { $null }
        StreamDrainTimedOut = if ($null -ne $payload) { [bool]$payload.resource.termination.streamDrainTimedOut } else { $null }
        SupervisorPhase = if ($null -ne $payload) { $payload.resource.supervisor.phase } else { $null }
        SupervisorTreeKilled = if ($null -ne $payload) { [bool]$payload.resource.supervisor.treeKilled } else { $null }
        WorkerLaunched = if ($null -ne $payload) { [bool]$payload.resource.workerProtocol.workerLaunched } else { $null }
        WorkerProtocolFailure = if ($null -ne $payload) { $payload.resource.workerProtocol.failureReason } else { $null }
        WorkerProtocolStdoutBytes = if ($null -ne $payload) { $payload.resource.workerProtocol.stdoutBytes } else { $null }
        WorkerProtocolStderrBytes = if ($null -ne $payload) { $payload.resource.workerProtocol.stderrBytes } else { $null }
        ResultReceivedFiles = if ($null -ne $payload) { $payload.result.receivedFiles } else { $null }
        PreflightEvaluated = if ($null -ne $payload.resource.memoryPreflight) { [bool]$payload.resource.memoryPreflight.evaluated } else { $null }
        PreflightPassed = if ($null -ne $payload.resource.memoryPreflight) { [bool]$payload.resource.memoryPreflight.passed } else { $null }
        PreflightReason = if ($null -ne $payload.resource.memoryPreflight) { $payload.resource.memoryPreflight.reason } else { $null }
        PreflightModelBytes = if ($null -ne $payload.resource.memoryPreflight) { $payload.resource.memoryPreflight.modelBytes } else { $null }
        PreflightEstimatedPeakBytes = if ($null -ne $payload.resource.memoryPreflight) { $payload.resource.memoryPreflight.estimatedPeakBytes } else { $null }
        PreflightRequiredAvailableBytes = if ($null -ne $payload.resource.memoryPreflight) { $payload.resource.memoryPreflight.requiredAvailableBytes } else { $null }
        ContextSize = if ($null -ne $payload) { $payload.resource.limits.contextSize } else { $null }
        LeakedSyntheticStderr = $stdout.Contains('synthetic worker stderr',[StringComparison]::Ordinal)
        StderrBytes = [Text.Encoding]::UTF8.GetByteCount($stderr)
    })
}

try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    $tokens = $null
    $parseErrors = $null
    [Management.Automation.Language.Parser]::ParseFile($scriptPath,[ref]$tokens,[ref]$parseErrors) | Out-Null
    $results.Add([pscustomobject]@{Name='ps7-parse';Passed=$parseErrors.Count -eq 0;ExitCode=0;Launched=$false;Ok=$true;Fallback=$false;ErrorCode=$null;PromptBytes=0;ElapsedMilliseconds=0;PeakWorkingSetBytes=0;StderrBytes=0})

    Invoke-Wrapper 'disallowed-extension' @('-Task','classify','-Files','tools/ci/invoke-local-qwen.ps1') $false $false 'LocalQwenFailed.input-validation'
    Invoke-Wrapper 'outside-root' @('-Task','classify','-Files','..\outside.txt') $false $false
    Invoke-Wrapper 'non-object-schema' @('-Task','classify','-JsonSchema','{"type":"array"}') $false $false
    Invoke-Wrapper 'oversized-prompt' @('-Task',('x' * 17000)) $false $false

    $stubSchema = '{"type":"object","required":["answer"],"properties":{"answer":{"type":"integer","const":2}},"additionalProperties":false}'
    Invoke-Wrapper 'worker-protocol-valid-multifile' @() $true $true '' @{
        AEVRYND_QWEN_TESTING='1'
        AEVRYND_QWEN_WORKER_PROTOCOL_STUB='Valid'
    } 20 $multiFileCallerPath
    $multiFileResult = $results[$results.Count - 1]
    $multiFileResult.Passed = $multiFileResult.Passed -and $multiFileResult.ResultReceivedFiles -eq 2
    foreach ($senderCase in @(
        @{Name='sender-too-many-files';Mode='TooMany'},
        @{Name='sender-path-too-long';Mode='TooLong'}
    )) {
        Invoke-Wrapper $senderCase.Name @($senderCase.Mode) $false $false 'LocalQwenSupervisorFailed' @{} 10 $multiFileCallerPath
        $senderResult = $results[$results.Count - 1]
        $senderResult.Passed = $senderResult.Passed -and -not $senderResult.WorkerLaunched
    }
    Invoke-Wrapper 'duplicate-final-path' @('DuplicateFinal') $false $false 'LocalQwenFailed.input-validation' @{} 10 $multiFileCallerPath

    foreach ($bootstrapCase in @(
        @{Name='worker-bootstrap-empty';Payload=''},
        @{Name='worker-bootstrap-bad-base64';Payload='not-base64'},
        @{Name='worker-bootstrap-null-json';Payload='bnVsbA=='}
    )) {
        Invoke-Wrapper $bootstrapCase.Name @('-Task','synthetic bootstrap','-MaxTokens','32') $false $false 'LocalQwenFailed.worker-bootstrap' @{
            AEVRYND_QWEN_INTERNAL_WORKER='1'
            AEVRYND_QWEN_TESTING='1'
            AEVRYND_QWEN_WORKER_PROTOCOL_STUB='Valid'
            AEVRYND_QWEN_WORKER_FILES_B64=$bootstrapCase.Payload
        }
    }
    foreach ($protocolCase in @(
        @{Name='worker-protocol-bad-json';Mode='BadJson';Reason='receipt-json'},
        @{Name='worker-protocol-bad-receipt';Mode='BadReceipt';Reason='receipt-schema'},
        @{Name='worker-protocol-stderr';Mode='Stderr';Reason='worker-stderr'},
        @{Name='worker-success-not-launched';Mode='SuccessNotLaunched';Reason='receipt-schema'},
        @{Name='worker-success-bad-exit';Mode='SuccessBadExit';Reason='receipt-schema'},
        @{Name='worker-success-no-job';Mode='SuccessNoJob';Reason='receipt-schema'},
        @{Name='worker-fallback-pretends-success';Mode='FallbackPretendsSuccess';Reason='receipt-schema'},
        @{Name='worker-bad-limits';Mode='BadLimits';Reason='receipt-schema'},
        @{Name='worker-bad-memory-preflight';Mode='BadPreflight';Reason='receipt-schema'},
        @{Name='worker-bad-memory-formula';Mode='BadPreflightFormula';Reason='receipt-schema'},
        @{Name='worker-peak-over-budget';Mode='PeakOverBudget';Reason='receipt-schema'},
        @{Name='worker-large-stdout';Mode='LargeStdout';Reason='worker-stdout-limit'},
        @{Name='worker-large-stderr';Mode='LargeStderr';Reason='worker-stderr-limit'}
    )) {
        Invoke-Wrapper $protocolCase.Name @('-Task','synthetic protocol','-MaxTokens','32') $false $false 'LocalQwenWorkerProtocolInvalid' @{
            AEVRYND_QWEN_TESTING='1'
            AEVRYND_QWEN_WORKER_PROTOCOL_STUB=$protocolCase.Mode
        }
        $protocolResult = $results[$results.Count - 1]
        $protocolResult.Passed = $protocolResult.Passed -and $protocolResult.WorkerLaunched -and
            $protocolResult.WorkerProtocolFailure -eq $protocolCase.Reason -and -not $protocolResult.LeakedSyntheticStderr
        if ($protocolCase.Mode -eq 'LargeStdout') {
            $protocolResult.Passed = $protocolResult.Passed -and $protocolResult.SupervisorTreeKilled -and
                $protocolResult.WorkerProtocolStdoutBytes -gt 262144 -and $protocolResult.WallMilliseconds -lt 5000
        } elseif ($protocolCase.Mode -eq 'LargeStderr') {
            $protocolResult.Passed = $protocolResult.Passed -and $protocolResult.SupervisorTreeKilled -and
                $protocolResult.WorkerProtocolStderrBytes -gt 65536 -and $protocolResult.WallMilliseconds -lt 5000
        }
    }
    Invoke-Wrapper 'runtime-stub-success' @('-Task','synthetic success','-MaxTokens','32','-JsonSchema',$stubSchema) $true $true '' @{
        AEVRYND_QWEN_TESTING='1';AEVRYND_QWEN_SYNTHETIC_MODE='StubSuccess'
    }
    Invoke-Wrapper 'runtime-stub-bad-marker' @('-Task','synthetic bad marker','-MaxTokens','32','-JsonSchema',$stubSchema) $true $false 'ModelOutputInvalid' @{
        AEVRYND_QWEN_TESTING='1';AEVRYND_QWEN_SYNTHETIC_MODE='StubBadMarker'
    }
    Invoke-Wrapper 'runtime-stub-bad-schema' @('-Task','synthetic bad schema','-MaxTokens','32','-JsonSchema',$stubSchema) $true $false 'ModelOutputInvalid' @{
        AEVRYND_QWEN_TESTING='1';AEVRYND_QWEN_SYNTHETIC_MODE='StubBadSchema'
    }
    Invoke-Wrapper 'runtime-stub-ram-gate' @('-Task','synthetic RAM gate','-MaxTokens','32') $true $false 'LocalQwenRamBudgetExceeded' @{
        AEVRYND_QWEN_TESTING='1';AEVRYND_QWEN_SYNTHETIC_MODE='StubRamGate'
    }

    # 所有覆寫都只配合 StubSuccess，保證測試永遠不會選到真實 llama-cli.exe。
    $modelBytes = 8GB
    $contextSize = 512
    $estimatedPeakBytes = $modelBytes + ($contextSize * 393216L) + 1GB + 512MB
    $requiredAvailableBytes = $estimatedPeakBytes + 4GB
    function New-SyntheticMemoryEnvironment([long]$AvailablePhysical,[long]$AvailableCommit,[long]$SyntheticModelBytes=$modelBytes) {
        return @{
            AEVRYND_QWEN_TESTING='1';AEVRYND_QWEN_SYNTHETIC_MODE='StubSuccess'
            AEVRYND_QWEN_TEST_MODEL_BYTES=[string]$SyntheticModelBytes
            AEVRYND_QWEN_TEST_TOTAL_PHYSICAL_BYTES=[string](64GB)
            AEVRYND_QWEN_TEST_AVAILABLE_PHYSICAL_BYTES=[string]$AvailablePhysical
            AEVRYND_QWEN_TEST_COMMIT_LIMIT_BYTES=[string](64GB)
            AEVRYND_QWEN_TEST_AVAILABLE_COMMIT_BYTES=[string]$AvailableCommit
        }
    }
    Invoke-Wrapper 'preflight-exact-threshold' @('-Task','synthetic threshold','-MaxTokens','32','-ContextSize',[string]$contextSize,'-JsonSchema',$stubSchema) $true $true '' (New-SyntheticMemoryEnvironment $requiredAvailableBytes $requiredAvailableBytes)
    $thresholdResult = $results[$results.Count - 1]
    $thresholdResult.Passed = $thresholdResult.Passed -and $thresholdResult.PreflightEvaluated -and $thresholdResult.PreflightPassed -and
        $thresholdResult.PreflightReason -eq 'ok' -and $thresholdResult.PreflightEstimatedPeakBytes -eq $estimatedPeakBytes -and
        $thresholdResult.PreflightRequiredAvailableBytes -eq $requiredAvailableBytes -and $thresholdResult.ContextSize -eq $contextSize

    Invoke-Wrapper 'preflight-physical-one-byte-short' @('-Task','synthetic physical gate','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenPreflightRejected' (New-SyntheticMemoryEnvironment ($requiredAvailableBytes - 1) $requiredAvailableBytes)
    $physicalResult = $results[$results.Count - 1]
    $physicalResult.Passed = $physicalResult.Passed -and $physicalResult.PreflightEvaluated -and -not $physicalResult.PreflightPassed -and $physicalResult.PreflightReason -eq 'physical-memory-insufficient'

    Invoke-Wrapper 'preflight-commit-one-byte-short' @('-Task','synthetic commit gate','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenPreflightRejected' (New-SyntheticMemoryEnvironment $requiredAvailableBytes ($requiredAvailableBytes - 1))
    $commitResult = $results[$results.Count - 1]
    $commitResult.Passed = $commitResult.Passed -and $commitResult.PreflightEvaluated -and -not $commitResult.PreflightPassed -and $commitResult.PreflightReason -eq 'commit-insufficient'

    Invoke-Wrapper 'preflight-model-size-growth' @('-Task','synthetic model growth','-MaxTokens','32','-ContextSize',[string]$contextSize) $true $true '' (New-SyntheticMemoryEnvironment 40GB 40GB 16GB)
    $modelGrowthResult = $results[$results.Count - 1]
    $modelGrowthResult.Passed = $modelGrowthResult.Passed -and $modelGrowthResult.PreflightModelBytes -eq 16GB -and $modelGrowthResult.PreflightEstimatedPeakBytes -gt $thresholdResult.PreflightEstimatedPeakBytes

    Invoke-Wrapper 'preflight-job-budget' @('-Task','synthetic job gate','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenPreflightRejected' (New-SyntheticMemoryEnvironment 64GB 64GB 23GB)
    $jobResult = $results[$results.Count - 1]
    $jobResult.Passed = $jobResult.Passed -and $jobResult.PreflightReason -eq 'job-budget-insufficient' -and -not $jobResult.Launched

    foreach ($configurationCase in @(
        @{Name='context-negative';Arguments=@('-ContextSize','-1')},
        @{Name='context-too-large';Arguments=@('-ContextSize','32769')},
        @{Name='context-overflow-policy';Arguments=@('-ContextSize',[string][long]::MaxValue)},
        @{Name='reserve-negative';Arguments=@('-SafetyReserveGiB','-1')},
        @{Name='reserve-too-large';Arguments=@('-SafetyReserveGiB','17')}
    )) {
        Invoke-Wrapper $configurationCase.Name (@('-Task','synthetic invalid configuration','-MaxTokens','32') + $configurationCase.Arguments) $false $false 'LocalQwenConfigurationInvalid'
        $configurationResult = $results[$results.Count - 1]
        $configurationResult.Passed = $configurationResult.Passed -and -not $configurationResult.WorkerLaunched
    }

    Invoke-Wrapper 'preflight-partial-fake-free-memory' @('-Task','synthetic partial memory','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenFailed.memory-preflight' @{
        AEVRYND_QWEN_TESTING='1';AEVRYND_QWEN_SYNTHETIC_MODE='StubSuccess';AEVRYND_QWEN_TEST_MODEL_BYTES=[string]$modelBytes
        AEVRYND_QWEN_TEST_AVAILABLE_PHYSICAL_BYTES=[string](64GB)
    }
    $partialOverrideResult = $results[$results.Count - 1]
    $partialOverrideResult.Passed = $partialOverrideResult.Passed -and -not $partialOverrideResult.Launched

    Invoke-Wrapper 'preflight-inconsistent-fake-free-memory' @('-Task','synthetic inconsistent memory','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenFailed.memory-preflight' (New-SyntheticMemoryEnvironment 65GB 64GB)
    $inconsistentResult = $results[$results.Count - 1]
    $inconsistentResult.Passed = $inconsistentResult.Passed -and -not $inconsistentResult.Launched

    foreach ($modelBoundaryCase in @(
        @{Name='preflight-model-zero';Bytes=0L},
        @{Name='preflight-model-too-large';Bytes=(1TB + 1L)}
    )) {
        Invoke-Wrapper $modelBoundaryCase.Name @('-Task','synthetic model boundary','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenFailed.memory-preflight' (New-SyntheticMemoryEnvironment 64GB 64GB $modelBoundaryCase.Bytes)
        $modelBoundaryResult = $results[$results.Count - 1]
        $modelBoundaryResult.Passed = $modelBoundaryResult.Passed -and -not $modelBoundaryResult.Launched
    }

    # 即使偽造四個極高計數，只要沒有合成 runtime，就必須直接拒絕而非讀取覆寫或啟動 llama。
    Invoke-Wrapper 'preflight-fake-free-without-synthetic-runtime' @('-Task','reject fake free memory','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenFailed.memory-preflight' @{
        AEVRYND_QWEN_TESTING='1'
        AEVRYND_QWEN_TEST_TOTAL_PHYSICAL_BYTES=[string](64GB)
        AEVRYND_QWEN_TEST_AVAILABLE_PHYSICAL_BYTES=[string](64GB)
        AEVRYND_QWEN_TEST_COMMIT_LIMIT_BYTES=[string](64GB)
        AEVRYND_QWEN_TEST_AVAILABLE_COMMIT_BYTES=[string](64GB)
    }
    $noSyntheticOverrideResult = $results[$results.Count - 1]
    $noSyntheticOverrideResult.Passed = $noSyntheticOverrideResult.Passed -and -not $noSyntheticOverrideResult.Launched

    # 重複拒絕必須得到相同估算，且兩次都不建立 runtime。
    Invoke-Wrapper 'preflight-repeat-a' @('-Task','synthetic repeat','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenPreflightRejected' (New-SyntheticMemoryEnvironment ($requiredAvailableBytes - 1) $requiredAvailableBytes)
    Invoke-Wrapper 'preflight-repeat-b' @('-Task','synthetic repeat','-MaxTokens','32','-ContextSize',[string]$contextSize) $false $false 'LocalQwenPreflightRejected' (New-SyntheticMemoryEnvironment ($requiredAvailableBytes - 1) $requiredAvailableBytes)
    $repeatA = $results[$results.Count - 2];$repeatB = $results[$results.Count - 1]
    $repeatA.Passed = $repeatA.Passed -and -not $repeatA.Launched
    $repeatB.Passed = $repeatB.Passed -and -not $repeatB.Launched -and $repeatA.PreflightEstimatedPeakBytes -eq $repeatB.PreflightEstimatedPeakBytes -and $repeatA.PreflightRequiredAvailableBytes -eq $repeatB.PreflightRequiredAvailableBytes
    foreach ($runtimeOutputCase in @('StubRuntimeLargeStdout','StubRuntimeLargeStderr')) {
        Invoke-Wrapper ('runtime-output-limit-' + $runtimeOutputCase.ToLowerInvariant()) @('-Task','synthetic runtime output limit','-MaxTokens','32') $true $false 'LocalQwenRuntimeOutputExceeded' @{
            AEVRYND_QWEN_TESTING='1';AEVRYND_QWEN_SYNTHETIC_MODE=$runtimeOutputCase
        } 10
        $runtimeOutputResult = $results[$results.Count - 1]
        $runtimeOutputResult.Passed = $runtimeOutputResult.Passed -and $runtimeOutputResult.WallMilliseconds -lt 5000
    }

    $syntheticMarker = [Guid]::NewGuid().ToString('N')
    Invoke-Wrapper 'hung-stream-timeout' @('-Task','synthetic timeout','-MaxTokens','32','-TimeoutSeconds','10') $true $false 'LocalQwenFailed.runtime-execution' @{
        AEVRYND_QWEN_TESTING='1'
        AEVRYND_QWEN_SYNTHETIC_MODE='HungStream'
        AEVRYND_QWEN_TEST_MARKER=$syntheticMarker
    } 15
    Start-Sleep -Milliseconds 300
    $orphanCount = @(Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*$syntheticMarker*" }).Count
    $syntheticResult = $results[$results.Count - 1]
    $syntheticResult.Passed = $syntheticResult.Passed -and $orphanCount -eq 0 -and
        $syntheticResult.ElapsedMilliseconds -ge 9500 -and $syntheticResult.WallMilliseconds -ge 9500 -and
        $syntheticResult.WallMilliseconds -le 15000 -and -not $syntheticResult.TerminationTimedOut -and
        -not $syntheticResult.StreamDrainTimedOut
    $syntheticResult | Add-Member -NotePropertyName OrphanCount -NotePropertyValue $orphanCount

    $hostHangTracePath = Join-Path $temporaryRoot 'host-hang-trace.log'
    Invoke-Wrapper 'host-hang-after-child-exit' @('-Task','synthetic host hang','-MaxTokens','32','-TimeoutSeconds','10') $false $false 'LocalQwenSupervisorTimeout' @{
        AEVRYND_QWEN_TESTING='1'
        AEVRYND_QWEN_SYNTHETIC_MODE='HostHangAfterChildExit'
        AEVRYND_QWEN_TEST_TRACE_PATH=$hostHangTracePath
    } 16
    $hostHangTrace = if (Test-Path -LiteralPath $hostHangTracePath) { Get-Content -LiteralPath $hostHangTracePath } else { @() }
    $hostHangResult = $results[$results.Count - 1]
    $remainingQwenTemporaryDirectories = @(
        Get-ChildItem -LiteralPath ([IO.Path]::GetTempPath()) -Directory -Filter 'aevrynd-qwen-*' -ErrorAction SilentlyContinue |
            Where-Object { -not $_.FullName.Equals($temporaryRoot,[StringComparison]::OrdinalIgnoreCase) }
    ).Count
    $requiredTracePhases = @(
        'runtime-process-exited','runtime-streams-drained','receipt-serialization-after',
        'finally-stop-after','finally-process-disposed','finally-temp-removed','synthetic-host-hang'
    )
    $hostHangResult.Passed = $hostHangResult.Passed -and
        $hostHangResult.WallMilliseconds -ge 14500 -and $hostHangResult.WallMilliseconds -le 16000 -and
        $hostHangResult.SupervisorPhase -eq 'worker-exit' -and $hostHangResult.SupervisorTreeKilled -and $hostHangResult.WorkerLaunched -and
        @($requiredTracePhases | Where-Object { $_ -notin $hostHangTrace }).Count -eq 0 -and
        $remainingQwenTemporaryDirectories -eq 0
    $hostHangResult | Add-Member -NotePropertyName TracePhases -NotePropertyValue ($hostHangTrace -join ',')
    $hostHangResult | Add-Member -NotePropertyName RemainingQwenTemporaryDirectories -NotePropertyValue $remainingQwenTemporaryDirectories

    $largeInputPath = Join-Path (Split-Path -Parent $PSScriptRoot) '.qwen-wrapper-test-large.txt'
    try {
        [IO.File]::WriteAllText($largeInputPath,('x' * 16385),[Text.UTF8Encoding]::new($false))
        Invoke-Wrapper 'oversized-file' @('-Task','classify','-Files','tools/.qwen-wrapper-test-large.txt') $false $false 'LocalQwenFailed.input-validation'
    } finally {
        if (Test-Path -LiteralPath $largeInputPath) { Remove-Item -LiteralPath $largeInputPath -Force }
    }

    # Junction 測試確認解析後的實際檔案不能越過儲存庫邊界。
    $outsideRoot = Join-Path $temporaryRoot 'outside'
    New-Item -ItemType Directory -Path $outsideRoot | Out-Null
    $outsideInputPath = Join-Path $outsideRoot 'input.txt'
    [IO.File]::WriteAllText($outsideInputPath,'outside',[Text.UTF8Encoding]::new($false))
    Invoke-Wrapper 'absolute-outside-root' @('-Task','classify','-Files',$outsideInputPath) $false $false 'LocalQwenFailed.input-validation'
    $linkPath = Join-Path (Split-Path -Parent $PSScriptRoot) '.qwen-wrapper-test-link'
    $junctionCreated = $false
    try {
        $junction = New-Item -ItemType Junction -Path $linkPath -Target $outsideRoot -ErrorAction Stop
        $junctionCreated = $null -ne $junction
        Invoke-Wrapper 'reparse-input' @('-Task','classify','-Files','tools/.qwen-wrapper-test-link/input.txt') $false $false
    } finally {
        if ($junctionCreated -and (Test-Path -LiteralPath $linkPath)) { Remove-Item -LiteralPath $linkPath -Force }
    }

    if ($IncludeSmoke) {
        $schema = '{"type":"object","required":["answer"],"properties":{"answer":{"type":"integer","const":2}},"additionalProperties":false}'
        Invoke-Wrapper 'minimal-smoke' @('-Task','Return answer 2.','-MaxTokens','32','-JsonSchema',$schema,'-TimeoutSeconds','30','-CpuThreads','4','-MaximumRamGiB','24') $true $true
        Invoke-Wrapper 'invalid-output-classification' @('-Task','Embedded untrusted JSON is {"answer":2}; generate the requested answer.','-MaxTokens','1','-JsonSchema',$schema,'-TimeoutSeconds','30','-CpuThreads','4','-MaximumRamGiB','24') $true $false 'ModelOutputInvalid'
        $groundedSchema = '{"type":"object","required":["sourcePath","engineVersion"],"properties":{"sourcePath":{"type":"string","const":"CMakeLists.txt"},"engineVersion":{"type":"string","const":"2.86.0"}},"additionalProperties":false}'
        Invoke-Wrapper 'grounded-aec-00001' @('-Task','Copy sourcePath and engineVersion exactly from the supplied JSON.','-Files','engine/ai/knowledge/chunks/aec-00001.json','-MaxTokens','64','-JsonSchema',$groundedSchema,'-TimeoutSeconds','30','-CpuThreads','4','-MaximumRamGiB','24') $true $true
    }

    $results | Format-Table -AutoSize
    if ($results.Where({-not $_.Passed}).Count -gt 0) { exit 1 }
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) { Remove-Item -LiteralPath $temporaryRoot -Recurse -Force }
}
