param(
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway",
    [string]$PythonExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Assert-Administrator
$paths = Get-GatewayPaths -InstallRoot $InstallRoot
$transportLock = Enter-PublicTransportLock
try {
$sourcePackage = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $command = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $command) { throw 'Python 3.11 or newer was not found.' }
    $PythonExe = $command.Source
}
$PythonExe = [IO.Path]::GetFullPath($PythonExe)
if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf)) { throw 'Python executable was not found.' }
$versionText = (& $PythonExe -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>$null)
if ($LASTEXITCODE -ne 0 -or [version]$versionText -lt [version]'3.11') { throw 'Python 3.11 or newer is required.' }

$coreFiles = @(
    '__init__.py', 'codex_handoff.py', 'config.py', 'credential_store.py',
    'host_tasks.py', 'line_client.py', 'queue_store.py', 'qwen_client.py',
    'routing.py', 'server.py', 'signature.py', 'status_report.py', 'worker.py'
)
$scriptFiles = @(
    'Common.ps1', 'Install.ps1', 'Register-Startup.ps1', 'Set-Configuration.ps1',
    'Set-Credentials.ps1', 'Set-SecondaryQwenCredential.ps1', 'Start-Gateway.ps1', 'Start-Tunnel.ps1',
    'Stop-Gateway.ps1', 'Stop-Tunnel.ps1', 'Test-Health.ps1',
    'Test-TunnelConfiguration.ps1', 'Funnel.Common.ps1', 'Start-Funnel.ps1',
    'Stop-Funnel.ps1', 'Test-FunnelConfiguration.ps1', 'Uninstall.ps1', 'gateway_runner.py',
    'Bootstrap.ps1', 'Enrollment.Common.ps1', 'Discover-Allowlist.ps1',
    'Ngrok.Common.ps1', 'Set-NgrokConfiguration.ps1', 'Set-NgrokCredential.ps1',
    'Start-Ngrok.ps1', 'Stop-Ngrok.ps1', 'Test-NgrokConfiguration.ps1',
    'Remove-NgrokPlaintextCredential.ps1', 'ngrok-package-identity.json'
)
foreach ($name in $coreFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourcePackage $name) -PathType Leaf)) { throw "Required core file is missing: $name" }
}
foreach ($name in $scriptFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $name) -PathType Leaf)) { throw "Required deployment file is missing: $name" }
}

New-Item -ItemType Directory -Force -Path $paths.Root,$paths.Config,$paths.Data,$paths.Logs,$paths.State,
    (Join-Path $paths.Root 'handoff'),(Join-Path $paths.Root 'cloudflared'),(Join-Path $paths.Root 'staging') | Out-Null

# 根目錄不可給排程帳號 DELETE_CHILD；否則可繞過 scripts/manifests 的唯讀 ACL。
$rootSecurity = [Security.AccessControl.DirectorySecurity]::new()
$rootSecurity.SetAccessRuleProtection($true, $false)
$rootInheritance = [Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit'
$rootPropagation = [Security.AccessControl.PropagationFlags]::None
$rootCurrentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$rootSecurity.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new(
    $rootCurrentIdentity, [Security.AccessControl.FileSystemRights]'ReadAndExecute, Synchronize',
    [Security.AccessControl.InheritanceFlags]::None, [Security.AccessControl.PropagationFlags]::None,
    [Security.AccessControl.AccessControlType]::Allow))
foreach ($account in @('NT AUTHORITY\SYSTEM', 'BUILTIN\Administrators')) {
    $rootSecurity.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new(
        $account, [Security.AccessControl.FileSystemRights]::FullControl,
        $rootInheritance, $rootPropagation, [Security.AccessControl.AccessControlType]::Allow))
}
Set-Acl -LiteralPath $paths.Root -AclObject $rootSecurity

# 只有明確列出的資料目錄可由登入使用者寫入；每個目錄都切斷父層繼承。
$mutableSecurity=[Security.AccessControl.DirectorySecurity]::new();$mutableSecurity.SetAccessRuleProtection($true,$false)
foreach($account in @($rootCurrentIdentity,'NT AUTHORITY\SYSTEM','BUILTIN\Administrators')){$mutableSecurity.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($account,[Security.AccessControl.FileSystemRights]::FullControl,$rootInheritance,$rootPropagation,[Security.AccessControl.AccessControlType]::Allow))}
foreach($directory in @($paths.Config,$paths.Data,$paths.Logs,$paths.State,(Join-Path $paths.Root 'handoff'),(Join-Path $paths.Root 'cloudflared'),(Join-Path $paths.Root 'staging'))){Set-Acl -LiteralPath $directory -AclObject $mutableSecurity}
if (Test-Path -LiteralPath $paths.StateFile -PathType Leaf) { throw 'Stop the gateway before installing an update.' }
if (Test-Path -LiteralPath $paths.TunnelStateFile -PathType Leaf) { throw 'Stop the named tunnel before installing an update.' }
if (Test-Path -LiteralPath $paths.FunnelStateFile -PathType Leaf) { throw 'Stop Tailscale Funnel before installing an update.' }
if (Test-Path -LiteralPath $paths.NgrokStateFile -PathType Leaf) { throw 'Stop ngrok before installing an update.' }
if (@(Get-NetTCPConnection -State Listen -LocalPort 8787 -ErrorAction SilentlyContinue).Count -gt 0) {
    throw 'TCP 8787 is active; stop the gateway before installing an update.'
}

# 舊版 runtime 可能留下 bytecode；僅在服務停止後清除精確版本及模組白名單。
$pythonCacheTag = 'cpython-' + $versionText.Replace('.', '')
$pythonModuleNames = @($coreFiles | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_) })
$null = Remove-KnownGatewayPythonCache -Paths $paths -AllowedModuleNames $pythonModuleNames -PythonCacheTag $pythonCacheTag

$releaseId = [Guid]::NewGuid().ToString('N')
$stageRoot = Join-Path $paths.Root "staging\$releaseId"
$stageApp = Join-Path $stageRoot 'app'
$stagePackage = Join-Path $stageApp 'tools\line_gateway'
$stageScripts = Join-Path $stageRoot 'scripts'
$stageManifests = Join-Path $stageRoot 'manifests'
New-Item -ItemType Directory -Force -Path $stagePackage,$stageScripts,$stageManifests | Out-Null

foreach ($name in $coreFiles) { Copy-Item -LiteralPath (Join-Path $sourcePackage $name) -Destination $stagePackage }
foreach ($name in $scriptFiles) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $stageScripts }

$installMetadata = [ordered]@{
    schemaVersion = 1
    releaseId = $releaseId
    installedUtc = [DateTime]::UtcNow.ToString('o')
    pythonExe = $PythonExe
    pythonVersion = $versionText
}
$stageInstallFile = Join-Path $stageManifests 'install.json'
[IO.File]::WriteAllText($stageInstallFile, ($installMetadata | ConvertTo-Json), [Text.UTF8Encoding]::new($false))

# 使用 AST 驗證但不產生 __pycache__，避免未列出的可載入檔案進入 runtime。
$pythonFiles = @((Get-ChildItem -LiteralPath $stagePackage -File -Filter '*.py').FullName) + @(Join-Path $stageScripts 'gateway_runner.py')
$astProbe = 'import ast,pathlib,sys; [ast.parse(pathlib.Path(p).read_text(encoding="utf-8"), filename=p) for p in sys.argv[1:]]'
& $PythonExe -B -c $astProbe @pythonFiles
if ($LASTEXITCODE -ne 0) { throw 'Installed Python source validation failed.' }

$manifestFiles = @(
    Get-ChildItem -LiteralPath $stageApp,$stageScripts -Recurse -File
    Get-Item -LiteralPath $stageInstallFile
) | Sort-Object FullName
$manifest = [ordered]@{
    schemaVersion = 1
    releaseId = $releaseId
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    files = @($manifestFiles | ForEach-Object {
        [ordered]@{
            path = [IO.Path]::GetRelativePath($stageRoot, $_.FullName).Replace('\', '/')
            size = $_.Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
        }
    })
}
$stageManifest = Join-Path $stageManifests 'installation-manifest.json'
$stageManifestHash = Join-Path $stageManifests 'installation-manifest.sha256'
[IO.File]::WriteAllText($stageManifest, ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
$manifestDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $stageManifest).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($stageManifestHash, $manifestDigest, [Text.UTF8Encoding]::new($false))

$stagePaths = @{
    Root = $stageRoot; App = $stageApp; Scripts = $stageScripts; Manifests = $stageManifests
    InstallFile = $stageInstallFile; ManifestFile = $stageManifest; ManifestHashFile = $stageManifestHash
}
$null = Test-InstallationManifest -Paths $stagePaths

# 驗證完成後才切換固定目錄；舊目錄只移到同磁碟備份，失敗時可原子移回。
$backupRoot = Join-Path $paths.Root "staging\backup-$releaseId"
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
$switched = [Collections.Generic.List[string]]::new()
$backedUp = [Collections.Generic.List[string]]::new()
try {
    foreach ($name in @('app', 'scripts', 'manifests')) {
        $destination = Join-Path $paths.Root $name
        $staged = Join-Path $stageRoot $name
        Assert-ExactGatewayPath -Paths $paths -Path $destination -AllowedRelativePaths @($name)
        if (Test-Path -LiteralPath $destination) {
            Move-Item -LiteralPath $destination -Destination (Join-Path $backupRoot $name)
            $backedUp.Add($name)
        }
        Move-Item -LiteralPath $staged -Destination $destination
        $switched.Add($name)
    }
} catch {
    foreach ($name in @($switched.ToArray()) | Sort-Object -Descending) {
        $destination = Join-Path $paths.Root $name
        Assert-ExactGatewayPath -Paths $paths -Path $destination -AllowedRelativePaths @($name)
        if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
        $backup = Join-Path $backupRoot $name
        if (Test-Path -LiteralPath $backup) { Move-Item -LiteralPath $backup -Destination $destination }
    }
    foreach ($name in $backedUp) {
        $destination = Join-Path $paths.Root $name
        $backup = Join-Path $backupRoot $name
        if (-not (Test-Path -LiteralPath $destination) -and (Test-Path -LiteralPath $backup)) {
            Move-Item -LiteralPath $backup -Destination $destination
        }
    }
    throw
}

Copy-Item -LiteralPath (Join-Path $sourcePackage 'gateway.env.example') -Destination $paths.Config -Force
Copy-Item -LiteralPath (Join-Path $sourcePackage 'cloudflared-config.yml.example') -Destination (Join-Path $paths.Root 'cloudflared') -Force
Copy-Item -LiteralPath (Join-Path $sourcePackage 'README.md') -Destination $paths.Root -Force

# runtime 程式目錄對一般執行 token 唯讀；只有提升權限的 Administrators 可更新。
$protectedSecurity = [Security.AccessControl.DirectorySecurity]::new()
$protectedSecurity.SetAccessRuleProtection($true, $false)
$inheritance = [Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit'
$propagation = [Security.AccessControl.PropagationFlags]::None
$currentIdentity = $rootCurrentIdentity
$protectedSecurity.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new(
    $currentIdentity, [Security.AccessControl.FileSystemRights]'ReadAndExecute, Synchronize',
    $inheritance, $propagation, [Security.AccessControl.AccessControlType]::Allow))
foreach ($account in @('NT AUTHORITY\SYSTEM', 'BUILTIN\Administrators')) {
    $protectedSecurity.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new(
        $account, [Security.AccessControl.FileSystemRights]::FullControl,
        $inheritance, $propagation, [Security.AccessControl.AccessControlType]::Allow))
}
foreach ($directory in @($paths.App, $paths.Scripts, $paths.Manifests)) { Set-Acl -LiteralPath $directory -AclObject $protectedSecurity }

# 所有清理由固定 staging 路徑導出，且不會觸及 queue、handoff、config 或 logs。
foreach ($temporaryRoot in @($stageRoot, $backupRoot)) {
    $prefix = [IO.Path]::GetFullPath((Join-Path $paths.Root 'staging')).TrimEnd('\') + '\'
    $candidate = [IO.Path]::GetFullPath($temporaryRoot)
    if (-not $candidate.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe staging cleanup path.' }
    if (Test-Path -LiteralPath $candidate) { Remove-Item -LiteralPath $candidate -Recurse -Force }
}

$null = Test-InstallationManifest -Paths $paths
Write-Output "LINE gateway installed from a closed, verified file set: release=$releaseId"
Write-Output 'Next: Set-Configuration.ps1, Set-Credentials.ps1, then Start-Gateway.ps1.'
} finally { Exit-PublicTransportLock $transportLock }
