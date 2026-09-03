param([ValidateRange(10,600)][int]$TimeoutSeconds = 300)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskName = 'Aevrynd-Line-Gateway-Automatic-Deployment'
$resultFile = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\status\result.json'
$protectedRunner = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\Invoke-AutomaticDeployment.ps1'
$sourceRunner = Join-Path $PSScriptRoot 'Invoke-AutomaticDeployment.ps1'
$invokedUtc = [DateTimeOffset]::UtcNow

if (-not (Test-Path -LiteralPath $protectedRunner -PathType Leaf) -or
    -not (Test-Path -LiteralPath $sourceRunner -PathType Leaf) -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $protectedRunner).Hash -cne
    (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceRunner).Hash) {
    throw 'Protected deployment runner is outdated; run the one-time installer once before triggering.'
}
$task = Get-ScheduledTask -TaskName $taskName -ErrorAction Stop
if ($task.State -eq 'Running') { throw 'An automatic deployment is already running.' }
$before = Get-ScheduledTaskInfo -TaskName $taskName -ErrorAction Stop
Start-ScheduledTask -TaskName $taskName

$deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
$observedRun = $false
do {
    Start-Sleep -Milliseconds 500
    $task = Get-ScheduledTask -TaskName $taskName -ErrorAction Stop
    $info = Get-ScheduledTaskInfo -TaskName $taskName -ErrorAction Stop
    if ($info.LastRunTime -gt $before.LastRunTime -or $task.State -eq 'Running') { $observedRun = $true }
    if (Test-Path -LiteralPath $resultFile -PathType Leaf) {
        try {
            $raw = [IO.File]::ReadAllText($resultFile, [Text.UTF8Encoding]::new($false, $true))
            $result = $raw | ConvertFrom-Json
            # ConvertFrom-Json 可能已把 ISO 字串轉成 DateTime；直接轉 DateTimeOffset，避免本地化字串破壞 ParseExact。
            $startedUtc = [DateTimeOffset]$result.startedUtc
            if ($result.schemaVersion -eq 1 -and $startedUtc -ge $invokedUtc.AddSeconds(-2) -and $result.status -in @('succeeded','failed','rolled_back','rollback_failed')) {
                $result
                if ($result.status -ne 'succeeded') { exit 1 }
                exit 0
            }
        } catch { }
    }
    if ($observedRun -and $task.State -ne 'Running' -and $info.LastTaskResult -ne 267009) {
        # 排程已終止卻尚無本次結果時只給原子寫入短暫寬限，不再盲等完整 timeout。
        Start-Sleep -Seconds 1
        if (-not (Test-Path -LiteralPath $resultFile -PathType Leaf) -or
            (Get-Item -LiteralPath $resultFile).LastWriteTimeUtc -lt $invokedUtc.UtcDateTime.AddSeconds(-2)) {
            throw "Automatic deployment task ended without a current result (LastTaskResult=$($info.LastTaskResult))."
        }
        throw "Automatic deployment task produced an unreadable current result (LastTaskResult=$($info.LastTaskResult))."
    }
} while ([DateTimeOffset]::UtcNow -lt $deadline)

throw 'Automatic deployment did not produce a bounded result in time.'
