param(
    [Parameter(Mandatory = $true)][string]$Wrapper,
    [Parameter(Mandatory = $true)][string]$Executable,
    [int]$ExpectedExitCode = 37
)

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Wrapper -Executable $Executable
$actual = $LASTEXITCODE
if ($actual -ne $ExpectedExitCode) {
    Write-Error "Expected GUI test exit code $ExpectedExitCode but received $actual."
    exit 1
}
Write-Output "Verified GUI test exit code $actual."
exit 0
