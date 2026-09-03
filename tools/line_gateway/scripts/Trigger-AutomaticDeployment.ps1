param([ValidateRange(10,600)][int]$TimeoutSeconds = 180)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskName = 'Aevrynd-Line-Gateway-Automatic-Deployment'
$resultFile = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\status\result.json'
$startedAfter = [DateTimeOffset]::UtcNow.AddSeconds(-2)

$task = Get-ScheduledTask -TaskName $taskName -ErrorAction Stop
if ($task.State -eq 'Running') { throw 'An automatic deployment is already running.' }
Start-ScheduledTask -TaskName $taskName

$deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
do {
    Start-Sleep -Milliseconds 500
    if (Test-Path -LiteralPath $resultFile -PathType Leaf) {
        try {
            $raw = [IO.File]::ReadAllText($resultFile, [Text.UTF8Encoding]::new($false, $true))
            $result = $raw | ConvertFrom-Json
            $startedUtc = [DateTimeOffset]::ParseExact([string]$result.startedUtc, 'o', [Globalization.CultureInfo]::InvariantCulture)
            if ($result.schemaVersion -eq 1 -and $startedUtc -ge $startedAfter -and $result.status -in @('succeeded','failed','rolled_back','rollback_failed')) {
                $result
                if ($result.status -ne 'succeeded') { exit 1 }
                exit 0
            }
        } catch { }
    }
} while ([DateTimeOffset]::UtcNow -lt $deadline)

throw 'Automatic deployment did not produce a bounded result in time.'
