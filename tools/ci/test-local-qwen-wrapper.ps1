param(
    [switch]$IncludeSmoke
)

$ErrorActionPreference = 'Stop'
$scriptPath = (Resolve-Path (Join-Path $PSScriptRoot 'invoke-local-qwen.ps1')).Path
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-qwen-wrapper-test-' + [Guid]::NewGuid().ToString('N'))
$results = [Collections.Generic.List[object]]::new()

function Invoke-Wrapper([string]$Name,[string[]]$Arguments,[bool]$ExpectLaunch,[bool]$ExpectSuccess,[string]$ExpectedErrorCode = '',[hashtable]$Environment = @{},[ValidateRange(1,60)][int]$MaximumWallSeconds = 20) {
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
    foreach ($argument in (@('-NoProfile','-File',$scriptPath) + $Arguments)) { $startInfo.ArgumentList.Add($argument) }
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
    Invoke-Wrapper 'host-hang-after-child-exit' @('-Task','synthetic host hang','-MaxTokens','32','-TimeoutSeconds','10') $true $false 'LocalQwenSupervisorTimeout' @{
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
        $hostHangResult.SupervisorPhase -eq 'worker-exit' -and $hostHangResult.SupervisorTreeKilled -and
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
