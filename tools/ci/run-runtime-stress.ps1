param(
    [string]$BuildDirectory = "build-ci",
    [string]$Configuration = "Release",
    [int]$DurationSeconds = 30,
    [int]$WarmupSeconds = 5,
    [int]$SampleIntervalSeconds = 2,
    [double]$MaxPrivateMemoryMB = 256,
    [double]$MaxPrivateGrowthMBPerMinute = 24
)

$ErrorActionPreference = "Stop"
if ($DurationSeconds -le $WarmupSeconds -or $SampleIntervalSeconds -lt 1) {
    throw "Invalid runtime stress duration or sample interval"
}

$resolvedBuild = (Resolve-Path -LiteralPath $BuildDirectory).Path
$binaryDirectory = Join-Path $resolvedBuild "bin\$Configuration"
$artifactDirectory = Join-Path $resolvedBuild "artifacts"
New-Item -ItemType Directory -Force -Path $artifactDirectory | Out-Null
$env:AEVRYND_AUTOMATION = "1"
Remove-Item Env:AEVRYND_ENABLE_DLSS -ErrorAction SilentlyContinue

$targets = @(
    [pscustomobject]@{ name = "character_lab"; executable = "Aevrynd-Character-Lab.exe" },
    [pscustomobject]@{ name = "engine_lab"; executable = "Aevrynd-Engine-Lab.exe" },
    [pscustomobject]@{ name = "frontier_world"; executable = "Aevrynd-Frontier-World.exe" },
    [pscustomobject]@{ name = "riftwarden"; executable = "Riftwarden-DX12.exe" }
)

$results = @()
foreach ($target in $targets) {
    $executable = Join-Path $binaryDirectory $target.executable
    if (-not (Test-Path -LiteralPath $executable)) {
        throw "Missing runtime executable: $executable"
    }

    $samples = @()
    $process = $null
    $failure = $null
    try {
        $process = Start-Process -FilePath $executable -WorkingDirectory $binaryDirectory -WindowStyle Hidden -PassThru
        $clock = [Diagnostics.Stopwatch]::StartNew()
        while ($clock.Elapsed.TotalSeconds -lt $DurationSeconds) {
            Start-Sleep -Seconds $SampleIntervalSeconds
            $current = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
            if (-not $current) {
                $failure = "Runtime exited before the stress duration completed"
                break
            }
            $samples += [pscustomobject]@{
                elapsedSeconds = [math]::Round($clock.Elapsed.TotalSeconds, 3)
                privateMemoryMB = [math]::Round($current.PrivateMemorySize64 / 1MB, 3)
                workingSetMB = [math]::Round($current.WorkingSet64 / 1MB, 3)
                responding = $current.Responding
            }
            if (-not $current.Responding) {
                $failure = "Runtime stopped responding"
                break
            }
        }
    } catch {
        $failure = $_.Exception.Message
    } finally {
        if ($process) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }

    $analysisSamples = @($samples | Where-Object { $_.elapsedSeconds -ge $WarmupSeconds })
    $growth = 0.0
    if ($analysisSamples.Count -ge 2) {
        $meanTime = ($analysisSamples | Measure-Object elapsedSeconds -Average).Average
        $meanPrivate = ($analysisSamples | Measure-Object privateMemoryMB -Average).Average
        $numerator = 0.0
        $denominator = 0.0
        foreach ($sample in $analysisSamples) {
            $timeDelta = $sample.elapsedSeconds - $meanTime
            $numerator += $timeDelta * ($sample.privateMemoryMB - $meanPrivate)
            $denominator += $timeDelta * $timeDelta
        }
        if ($denominator -gt 0) {
            $growth = $numerator / $denominator * 60.0
        }
    } else {
        $failure = if ($failure) { $failure } else { "Not enough post-warmup memory samples" }
    }

    $peakPrivate = if ($samples.Count) { ($samples | Measure-Object privateMemoryMB -Maximum).Maximum } else { 0.0 }
    $peakWorkingSet = if ($samples.Count) { ($samples | Measure-Object workingSetMB -Maximum).Maximum } else { 0.0 }
    $passed = -not $failure -and $peakPrivate -le $MaxPrivateMemoryMB -and $growth -le $MaxPrivateGrowthMBPerMinute
    $results += [pscustomobject]@{
        name = $target.name
        passed = $passed
        failure = $failure
        sampleCount = $samples.Count
        peakPrivateMemoryMB = [math]::Round($peakPrivate, 3)
        peakWorkingSetMB = [math]::Round($peakWorkingSet, 3)
        privateGrowthMBPerMinute = [math]::Round($growth, 3)
        samples = $samples
    }
}

$summary = [pscustomobject]@{
    configuration = $Configuration
    durationSeconds = $DurationSeconds
    warmupSeconds = $WarmupSeconds
    sampleIntervalSeconds = $SampleIntervalSeconds
    maxPrivateMemoryMB = $MaxPrivateMemoryMB
    maxPrivateGrowthMBPerMinute = $MaxPrivateGrowthMBPerMinute
    passed = -not ($results | Where-Object { -not $_.passed })
    results = $results
    completedUtc = [DateTime]::UtcNow.ToString("o")
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $artifactDirectory "runtime-stress.json")
$results | Select-Object name, passed, sampleCount, peakPrivateMemoryMB, peakWorkingSetMB, privateGrowthMBPerMinute, failure | Format-Table -AutoSize
if (-not $summary.passed) {
    throw "Runtime memory stress validation failed"
}
