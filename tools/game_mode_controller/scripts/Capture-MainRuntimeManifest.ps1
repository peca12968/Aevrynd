param(
    [Parameter(Mandatory)][string]$OutputPath,
    [Parameter(Mandatory)][ValidateLength(1,512)][string]$ModelId,
    [string]$PythonExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $command = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $command -or $command.Source -like '*\WindowsApps\python.exe') { throw 'A real Python 3.11 or newer executable is required.' }
    $PythonExe = $command.Source
}
$PythonExe = [IO.Path]::GetFullPath($PythonExe)
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $OutputPath) { throw 'OutputPath must not already exist.' }
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$previous = $env:PYTHONPATH
try {
    $env:PYTHONPATH = $sourceRoot
    & $PythonExe -B -m tools.game_mode_controller.runner capture-main-manifest --output $OutputPath --model-id $ModelId
    if ($LASTEXITCODE -ne 0) { throw 'Runtime manifest capture failed.' }
} finally {
    if ($null -eq $previous) { Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue } else { $env:PYTHONPATH = $previous }
}
