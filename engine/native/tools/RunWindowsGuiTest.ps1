param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExecutableArguments
)

$startInfo = @{
    FilePath    = $Executable
    Wait        = $true
    PassThru    = $true
    NoNewWindow = $true
}

if ($ExecutableArguments.Count -gt 0) {
    $startInfo.ArgumentList = $ExecutableArguments
}

try {
    $process = Start-Process @startInfo
    exit [int]$process.ExitCode
}
catch {
    Write-Error $_
    exit 1
}
