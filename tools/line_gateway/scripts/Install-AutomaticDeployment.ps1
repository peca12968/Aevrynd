Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# 此安裝器只需以系統管理員身分執行一次；之後一般權限帳號只能啟動固定工作。
$repositoryRoot = 'D:\Download\Aevrynd\Games'
$deploymentRoot = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment'
$runnerSource = Join-Path $PSScriptRoot 'Invoke-AutomaticDeployment.ps1'
$runnerDestination = Join-Path $deploymentRoot 'Invoke-AutomaticDeployment.ps1'
$resultRoot = Join-Path $deploymentRoot 'status'
$taskName = 'Aevrynd-Line-Gateway-Automatic-Deployment'

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Administrator privileges are required for the one-time installer.'
    }
}

function Assert-FixedDirectory {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Expected, [switch]$AllowMissing)
    if ($Path.StartsWith('\\') -or $Path.Length -gt 220) { throw 'Fixed deployment path is unsafe.' }
    $candidate = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $literal = [IO.Path]::GetFullPath($Expected).TrimEnd('\')
    if (-not [string]::Equals($candidate, $literal, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixed deployment path changed.' }
    if (-not $AllowMissing -and -not (Test-Path -LiteralPath $candidate -PathType Container)) { throw 'Fixed directory is missing.' }
    $cursor = $candidate
    while ($cursor -and (Test-Path -LiteralPath $cursor)) {
        if (((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Fixed deployment path contains a reparse point.'
        }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
}

Assert-Administrator
Assert-FixedDirectory -Path $repositoryRoot -Expected 'D:\Download\Aevrynd\Games'
Assert-FixedDirectory -Path $PSScriptRoot -Expected 'D:\Download\Aevrynd\Games\tools\line_gateway\scripts'
Assert-FixedDirectory -Path $deploymentRoot -Expected 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment' -AllowMissing
$expectedRunnerSource = 'D:\Download\Aevrynd\Games\tools\line_gateway\scripts\Invoke-AutomaticDeployment.ps1'
if (-not [string]::Equals([IO.Path]::GetFullPath($runnerSource), $expectedRunnerSource, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The one-time installer must run from the fixed repository.'
}
if (-not (Test-Path -LiteralPath $runnerSource -PathType Leaf) -or
    ((Get-Item -LiteralPath $runnerSource -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
    throw 'Automatic deployment runner source is missing or unsafe.'
}

$pwsh = 'C:\Program Files\PowerShell\7\pwsh.exe'
if (-not (Test-Path -LiteralPath $pwsh -PathType Leaf)) { throw 'System-wide PowerShell 7 is required.' }
$signature = Get-AuthenticodeSignature -LiteralPath $pwsh
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(?i)O=Microsoft Corporation') {
    throw 'System-wide PowerShell signature validation failed.'
}

New-Item -ItemType Directory -Force -Path $deploymentRoot,$resultRoot | Out-Null
Assert-FixedDirectory -Path $deploymentRoot -Expected 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment'
Assert-FixedDirectory -Path $resultRoot -Expected 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\status'
if (Test-Path -LiteralPath $runnerDestination) {
    if (-not (Test-Path -LiteralPath $runnerDestination -PathType Leaf) -or
        ((Get-Item -LiteralPath $runnerDestination -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'Existing automatic deployment runner target is unsafe.'
    }
}
Copy-Item -LiteralPath $runnerSource -Destination $runnerDestination -Force

# runner 根目錄切斷繼承；一般使用者只能讀取與執行，無法替換提升權限的程式。
$identityName = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$inheritance = [Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit'
$propagation = [Security.AccessControl.PropagationFlags]::None
$protected = [Security.AccessControl.DirectorySecurity]::new()
$protected.SetAccessRuleProtection($true, $false)
$protected.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new(
    $identityName, [Security.AccessControl.FileSystemRights]'ReadAndExecute, Synchronize',
    $inheritance, $propagation, [Security.AccessControl.AccessControlType]::Allow))
foreach ($account in @('NT AUTHORITY\SYSTEM','BUILTIN\Administrators')) {
    $protected.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new(
        $account, [Security.AccessControl.FileSystemRights]::FullControl,
        $inheritance, $propagation, [Security.AccessControl.AccessControlType]::Allow))
}
Set-Acl -LiteralPath $deploymentRoot -AclObject $protected

# 結果目錄只額外允許登入帳號讀取；結果本身不含路徑、錯誤內容或機密。
$statusAcl = [Security.AccessControl.DirectorySecurity]::new()
$statusAcl.SetAccessRuleProtection($true, $false)
$statusAcl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new(
    $identityName, [Security.AccessControl.FileSystemRights]'ReadAndExecute, Synchronize',
    $inheritance, $propagation, [Security.AccessControl.AccessControlType]::Allow))
foreach ($account in @('NT AUTHORITY\SYSTEM','BUILTIN\Administrators')) {
    $statusAcl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new(
        $account, [Security.AccessControl.FileSystemRights]::FullControl,
        $inheritance, $propagation, [Security.AccessControl.AccessControlType]::Allow))
}
Set-Acl -LiteralPath $resultRoot -AclObject $statusAcl

$arguments = '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\Invoke-AutomaticDeployment.ps1"'
$action = New-ScheduledTaskAction -Execute $pwsh -Argument $arguments -WorkingDirectory $deploymentRoot
$principal = New-ScheduledTaskPrincipal -UserId $identityName -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 10) `
    -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
$task = New-ScheduledTask -Action $action -Principal $principal -Settings $settings
Register-ScheduledTask -TaskName $taskName -InputObject $task -Force | Out-Null

Write-Output 'Automatic LINE gateway deployment is installed. Future deployments do not require another UAC prompt.'
