param(
    [switch]$IncludeSmoke
)

$ErrorActionPreference = 'Stop'
$scriptPath = (Resolve-Path (Join-Path $PSScriptRoot 'invoke-local-qwen.ps1')).Path
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-qwen-wrapper-test-' + [Guid]::NewGuid().ToString('N'))
$results = [Collections.Generic.List[object]]::new()

function Invoke-Wrapper([string]$Name,[string[]]$Arguments,[bool]$ExpectLaunch,[bool]$ExpectSuccess,[string]$ExpectedErrorCode = '') {
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
    if (-not $process.Start()) { throw "Could not start wrapper test case: $Name" }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.GetAwaiter().GetResult().Trim()
    $stderr = $stderrTask.GetAwaiter().GetResult().Trim()
    $exitCode = $process.ExitCode
    $process.Dispose()
    $payload = $null
    try { $payload = $stdout | ConvertFrom-Json -ErrorAction Stop } catch {}
    $passed = $exitCode -eq 0 -and $null -ne $payload -and
        [bool]$payload.resource.launched -eq $ExpectLaunch -and [bool]$payload.ok -eq $ExpectSuccess -and
        [bool]$payload.fallbackToCodex -eq (-not $ExpectSuccess) -and $stderr.Length -eq 0
    if ($ExpectedErrorCode.Length -gt 0) { $passed = $passed -and $payload.error.code -eq $ExpectedErrorCode }
    if ($null -ne $payload) {
        $passed = $passed -and $payload.resource.limits.timeoutSeconds -le 30 -and
            $payload.resource.limits.maxTokens -le 512 -and $payload.resource.limits.cpuThreads -le 4 -and
            $payload.resource.limits.maximumRamBytes -le 24GB
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
