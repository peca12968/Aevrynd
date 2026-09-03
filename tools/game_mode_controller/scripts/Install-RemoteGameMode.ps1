[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][ValidateSet('taichung','taoyuan')][string]$NodeId,
    [Parameter(Mandatory)][string]$ReadinessDirectory,
    [string]$PythonExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$readinessRoot = [IO.Path]::GetFullPath($ReadinessDirectory)
$readinessItem = Get-Item -LiteralPath $readinessRoot -Force -ErrorAction Stop
if (-not $readinessItem.PSIsContainer -or ($readinessItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Readiness directory is unsafe.' }
$summaryPath = Join-Path $readinessRoot 'readiness.json'
$summaryItem = Get-Item -LiteralPath $summaryPath -Force -ErrorAction Stop
if ($summaryItem.PSIsContainer -or ($summaryItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $summaryItem.Length -lt 2 -or $summaryItem.Length -gt 8192) { throw 'Readiness summary is unavailable.' }
$summaryDocument = $null
try {
    $summaryText = [Text.UTF8Encoding]::new($false,$true).GetString([IO.File]::ReadAllBytes($summaryPath))
    $summaryDocument = [Text.Json.JsonDocument]::Parse($summaryText)
    $summaryRoot = $summaryDocument.RootElement
    $summaryFields = @($summaryRoot.EnumerateObject() | ForEach-Object Name | Sort-Object)
    if ($summaryRoot.ValueKind -ne [Text.Json.JsonValueKind]::Object -or
        ($summaryFields -join ',') -cne 'managedPid,nodeId,ready,runtimeManifestFile,runtimeManifestSha256,schemaVersion') { throw 'schema' }
    $schemaVersion = $summaryRoot.GetProperty('schemaVersion').GetInt32()
    $managedPid = $summaryRoot.GetProperty('managedPid').GetInt32()
    $summaryNode = $summaryRoot.GetProperty('nodeId').GetString()
    $ready = $summaryRoot.GetProperty('ready').GetBoolean()
    $manifestFile = $summaryRoot.GetProperty('runtimeManifestFile').GetString()
    $manifestSha256 = $summaryRoot.GetProperty('runtimeManifestSha256').GetString()
} catch {
    throw 'Readiness summary is invalid.'
} finally {
    if ($null -ne $summaryDocument) { $summaryDocument.Dispose() }
}
if ($schemaVersion -ne 1 -or $summaryNode -cne $NodeId -or $ready -ne $true -or $managedPid -le 0 -or
    $manifestFile -cne "$NodeId-runtime-manifest.json" -or $manifestSha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Readiness summary is invalid.' }
$manifest = Join-Path $readinessRoot $manifestFile
$manifestItem = Get-Item -LiteralPath $manifest -Force -ErrorAction Stop
if ($manifestItem.PSIsContainer -or ($manifestItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $manifestItem.Length -lt 2 -or $manifestItem.Length -gt 65536 -or (Get-FileHash -Algorithm SHA256 -LiteralPath $manifest).Hash.ToLowerInvariant() -cne $manifestSha256) { throw 'Readiness manifest identity changed.' }
if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand -or $pythonCommand.Source -like '*\WindowsApps\python.exe') { throw 'A real Python 3.11 or newer executable is required.' }
    $PythonExe = $pythonCommand.Source
}
$PythonExe = [IO.Path]::GetFullPath($PythonExe)
if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf) -or
    (((Get-Item -LiteralPath $PythonExe -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) -or
    [IO.Path]::GetFileName($PythonExe) -notin @('python.exe','python3.exe')) { throw 'Python executable is invalid.' }
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$credentialCheck = [Diagnostics.ProcessStartInfo]::new($PythonExe)
$credentialCheck.UseShellExecute = $false
$credentialCheck.CreateNoWindow = $true
$credentialCheck.RedirectStandardOutput = $true
$credentialCheck.RedirectStandardError = $true
$credentialCheck.WorkingDirectory = $sourceRoot
$credentialCheck.Environment.Clear()
foreach ($name in @('SystemRoot','WINDIR','TEMP','TMP','USERPROFILE','LOCALAPPDATA','ProgramData')) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if (-not [string]::IsNullOrEmpty($value)) { $credentialCheck.Environment[$name] = $value }
}
$credentialCheck.Environment['PYTHONPATH'] = $sourceRoot
$credentialCheck.ArgumentList.Add('-B'); $credentialCheck.ArgumentList.Add('-c')
$credentialCheck.ArgumentList.Add("import sys; from tools.game_mode_controller.credential_store import read_node_secret,read_qwen_api_key; n=sys.argv[1]; raise SystemExit(0 if read_node_secret(n) is not None and read_qwen_api_key(n) is not None else 4)")
$credentialCheck.ArgumentList.Add($NodeId)
$credentialProcess = $null
try {
    # 兩份固定 Credential 必須先由本機管理員建立；值不寫入檔案或輸出。
    $credentialProcess = [Diagnostics.Process]::Start($credentialCheck)
    if ($null -eq $credentialProcess -or -not $credentialProcess.WaitForExit(10000)) {
        if ($null -ne $credentialProcess) { $credentialProcess.Kill($true); $credentialProcess.WaitForExit() }
        throw 'Required remote controller credentials are unavailable.'
    }
    [void]$credentialProcess.StandardOutput.ReadToEnd(); [void]$credentialProcess.StandardError.ReadToEnd()
    if ($credentialProcess.ExitCode -ne 0) { throw 'Required remote controller credentials are unavailable.' }
} finally {
    if ($null -ne $credentialProcess) { $credentialProcess.Dispose() }
}
if ($PSCmdlet.ShouldProcess("$env:ProgramData\Aevrynd\GameModeController", "install fixed remote node $NodeId")) {
    & (Join-Path $PSScriptRoot 'Install.ps1') -NodeId $NodeId -PythonExe $PythonExe -RuntimeManifestPath $manifest -RuntimeManifestSha256 $manifestSha256 -InitialManagedPid $managedPid -RegisterStartup
    if ($LASTEXITCODE -ne 0) { throw 'Remote Game Mode installation failed.' }
}
