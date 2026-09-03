Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# 以下信任邊界全部是 literal，runner 不接受命令、參數、來源路徑或安裝目標輸入。
$repositoryRoot = 'D:\Download\Aevrynd\Games'
$liveRoot = 'C:\ProgramData\Aevrynd\LineGateway'
$deploymentRoot = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment'
$statusRoot = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\status'
$resultFile = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\status\result.json'
$scratchRoot = 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\staging'
$expectedOrigin = 'https://github.com/peca12968/Aevrynd.git'
$expectedBranch = 'refs/heads/main'
$gitExe = 'C:\Program Files\Git\cmd\git.exe'
$startedUtc = [DateTimeOffset]::UtcNow
$release = ''
$oldNoSystem = $null
$oldGlobal = $null
$oldPager = $null

function Write-SafeResult {
    param([Parameter(Mandatory)][ValidateSet('running','succeeded','failed','rolled_back','rollback_failed')][string]$Status,
          [Parameter(Mandatory)][ValidateSet('none','source_rejected','busy','stop_failed','install_failed','start_failed','health_failed','rollback_failed')][string]$Code,
          [string]$Release = '')
    $document = [ordered]@{
        schemaVersion = 1
        status = $Status
        code = $Code
        release = $(if ($Release -match '^[0-9a-f]{7,64}$') { $Release.Substring(0, [Math]::Min(12, $Release.Length)) } else { '' })
        startedUtc = $startedUtc.ToString('o', [Globalization.CultureInfo]::InvariantCulture)
        finishedUtc = $(if ($Status -eq 'running') { $null } else { [DateTimeOffset]::UtcNow.ToString('o', [Globalization.CultureInfo]::InvariantCulture) })
    }
    $temporary = Join-Path $statusRoot ('.result-' + [Guid]::NewGuid().ToString('N') + '.tmp')
    [IO.File]::WriteAllText($temporary, ($document | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $resultFile -Force
}

function Assert-FixedSafePath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Expected, [switch]$AllowMissing)
    if ($Path.StartsWith('\\') -or $Path.Length -gt 220) { throw 'unsafe_path' }
    $candidate = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $literal = [IO.Path]::GetFullPath($Expected).TrimEnd('\')
    if (-not [string]::Equals($candidate, $literal, [StringComparison]::OrdinalIgnoreCase)) { throw 'unsafe_path' }
    if (-not $AllowMissing -and -not (Test-Path -LiteralPath $candidate -PathType Container)) { throw 'unsafe_path' }
    $cursor = $candidate
    while ($cursor -and (Test-Path -LiteralPath $cursor)) {
        if (((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'unsafe_path' }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
}

function Remove-FixedScratch {
    if (-not (Test-Path -LiteralPath $scratchRoot)) { return }
    Assert-FixedSafePath -Path $scratchRoot -Expected 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\staging'
    Remove-Item -LiteralPath $scratchRoot -Recurse -Force
}

function Restore-VerifiedRelease {
    param([Parameter(Mandatory)][string]$BackupRoot)
    try { & (Join-Path $liveRoot 'scripts\Stop-Gateway.ps1') -InstallRoot $liveRoot | Out-Null } catch { }
    foreach ($name in @('app','scripts','manifests')) {
        $destination = Join-Path $liveRoot $name
        $backup = Join-Path $BackupRoot $name
        if (-not (Test-Path -LiteralPath $backup -PathType Container)) { throw 'rollback_missing' }
        if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
        Move-Item -LiteralPath $backup -Destination $destination
    }
    . (Join-Path $liveRoot 'scripts\Common.ps1')
    $paths = Get-GatewayPaths -InstallRoot $liveRoot
    $null = Test-InstallationManifest -Paths $paths
    & (Join-Path $liveRoot 'scripts\Start-Gateway.ps1') -InstallRoot $liveRoot | Out-Null
    & (Join-Path $liveRoot 'scripts\Test-Health.ps1') -InstallRoot $liveRoot | Out-Null
}

New-Item -ItemType Directory -Force -Path $statusRoot | Out-Null
Write-SafeResult -Status running -Code none
$mutex = [Threading.Mutex]::new($false, 'Global\AevryndLineGatewayAutomaticDeployment')
$acquired = $false
try {
    try { $acquired = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $acquired = $true }
    if (-not $acquired) { Write-SafeResult -Status failed -Code busy; exit 2 }

    Assert-FixedSafePath -Path $repositoryRoot -Expected 'D:\Download\Aevrynd\Games'
    Assert-FixedSafePath -Path $liveRoot -Expected 'C:\ProgramData\Aevrynd\LineGateway' -AllowMissing
    Assert-FixedSafePath -Path $deploymentRoot -Expected 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment'
    $gitDirectory = Join-Path $repositoryRoot '.git'
    if (-not (Test-Path -LiteralPath $gitDirectory -PathType Container) -or
        ((Get-Item -LiteralPath $gitDirectory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'source_rejected' }
    if (-not (Test-Path -LiteralPath $gitExe -PathType Leaf) -or $gitExe.Length -gt 220) { throw 'source_rejected' }
    $gitCursor = [IO.Path]::GetDirectoryName($gitExe)
    while ($gitCursor.Length -ge 'C:\Program Files'.Length) {
        if (((Get-Item -LiteralPath $gitCursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'source_rejected' }
        if ([string]::Equals($gitCursor, 'C:\Program Files', [StringComparison]::OrdinalIgnoreCase)) { break }
        $gitCursor = [IO.Path]::GetDirectoryName($gitCursor)
    }
    $gitSignature = Get-AuthenticodeSignature -LiteralPath $gitExe
    if ($gitSignature.Status -ne 'Valid' -or $gitSignature.SignerCertificate.Subject -notmatch '(?i)O=Johannes Schindelin') { throw 'source_rejected' }

    # 只接受遠端 main 的 authoritative commit；本機工作樹與 origin 設定都不是信任來源。
    $oldNoSystem = $env:GIT_CONFIG_NOSYSTEM
    $oldGlobal = $env:GIT_CONFIG_GLOBAL
    $oldPager = $env:GIT_PAGER
    $env:GIT_CONFIG_NOSYSTEM = '1'
    $env:GIT_CONFIG_GLOBAL = 'NUL'
    $env:GIT_PAGER = 'cat'
    $gitSafety = @('-c','protocol.file.allow=never','-c','core.fsmonitor=false','-c','credential.helper=','-c','core.hooksPath=NUL')
    $remoteLines = @(& $gitExe @gitSafety -C $repositoryRoot ls-remote $expectedOrigin $expectedBranch 2>$null)
    if ($LASTEXITCODE -ne 0 -or $remoteLines.Count -ne 1 -or $remoteLines[0] -notmatch '^([0-9a-f]{40})\s+refs/heads/main$') { throw 'source_rejected' }
    $release = $Matches[1]
    $head = (& $gitExe @gitSafety -C $repositoryRoot rev-parse HEAD 2>$null).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cne $release) { throw 'source_rejected' }
    & $gitExe @gitSafety -C $repositoryRoot diff --no-ext-diff --quiet -- tools/line_gateway
    if ($LASTEXITCODE -ne 0) { throw 'source_rejected' }
    & $gitExe @gitSafety -C $repositoryRoot diff --cached --no-ext-diff --quiet -- tools/line_gateway
    if ($LASTEXITCODE -ne 0) { throw 'source_rejected' }

    Remove-FixedScratch
    New-Item -ItemType Directory -Path $scratchRoot | Out-Null
    $archive = Join-Path $scratchRoot 'source.zip'
    & $gitExe @gitSafety -C $repositoryRoot fetch --no-tags $expectedOrigin $release 2>$null
    if ($LASTEXITCODE -ne 0) { throw 'source_rejected' }
    & $gitExe @gitSafety -C $repositoryRoot archive --format=zip --output=$archive $release tools/line_gateway
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw 'source_rejected' }
    $source = Join-Path $scratchRoot 'source'
    Expand-Archive -LiteralPath $archive -DestinationPath $source
    Assert-FixedSafePath -Path $source -Expected 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment\staging\source'
    foreach ($item in Get-ChildItem -LiteralPath $source -Recurse -Force) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'source_rejected' }
    }
    $installScript = Join-Path $source 'tools\line_gateway\scripts\Install.ps1'
    if (-not (Test-Path -LiteralPath $installScript -PathType Leaf)) { throw 'source_rejected' }

    $backup = Join-Path $scratchRoot 'rollback'
    $hasRollback = $false
    if (Test-Path -LiteralPath (Join-Path $liveRoot 'manifests\installation-manifest.json') -PathType Leaf) {
        . (Join-Path $liveRoot 'scripts\Common.ps1')
        $currentPaths = Get-GatewayPaths -InstallRoot $liveRoot
        $null = Test-InstallationManifest -Paths $currentPaths
        foreach ($name in @('app','scripts','manifests')) {
            foreach ($item in Get-ChildItem -LiteralPath (Join-Path $liveRoot $name) -Recurse -Force) {
                if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'source_rejected' }
            }
        }
        New-Item -ItemType Directory -Path $backup | Out-Null
        foreach ($name in @('app','scripts','manifests')) { Copy-Item -LiteralPath (Join-Path $liveRoot $name) -Destination $backup -Recurse }
        $backupPaths = @{
            Root=$backup; App=(Join-Path $backup 'app'); Scripts=(Join-Path $backup 'scripts');
            Manifests=(Join-Path $backup 'manifests'); InstallFile=(Join-Path $backup 'manifests\install.json');
            ManifestFile=(Join-Path $backup 'manifests\installation-manifest.json');
            ManifestHashFile=(Join-Path $backup 'manifests\installation-manifest.sha256')
        }
        $null = Test-InstallationManifest -Paths $backupPaths
        $hasRollback = $true
    }

    try { & (Join-Path $liveRoot 'scripts\Stop-Gateway.ps1') -InstallRoot $liveRoot | Out-Null } catch { Write-SafeResult -Status failed -Code stop_failed -Release $release; exit 3 }
    try { & $installScript -InstallRoot $liveRoot | Out-Null } catch {
        if ($hasRollback) {
            try { Restore-VerifiedRelease -BackupRoot $backup; Write-SafeResult -Status rolled_back -Code install_failed -Release $release; exit 4 }
            catch { Write-SafeResult -Status rollback_failed -Code rollback_failed -Release $release; exit 5 }
        }
        Write-SafeResult -Status failed -Code install_failed -Release $release; exit 4
    }
    try { & (Join-Path $liveRoot 'scripts\Start-Gateway.ps1') -InstallRoot $liveRoot | Out-Null }
    catch {
        if ($hasRollback) { try { Restore-VerifiedRelease -BackupRoot $backup; Write-SafeResult -Status rolled_back -Code start_failed -Release $release; exit 6 } catch { Write-SafeResult -Status rollback_failed -Code rollback_failed -Release $release; exit 5 } }
        Write-SafeResult -Status failed -Code start_failed -Release $release; exit 6
    }
    try { & (Join-Path $liveRoot 'scripts\Test-Health.ps1') -InstallRoot $liveRoot | Out-Null }
    catch {
        if ($hasRollback) { try { Restore-VerifiedRelease -BackupRoot $backup; Write-SafeResult -Status rolled_back -Code health_failed -Release $release; exit 7 } catch { Write-SafeResult -Status rollback_failed -Code rollback_failed -Release $release; exit 5 } }
        Write-SafeResult -Status failed -Code health_failed -Release $release; exit 7
    }
    Write-SafeResult -Status succeeded -Code none -Release $release
} catch {
    Write-SafeResult -Status failed -Code source_rejected -Release $release
    exit 8
} finally {
    try { Remove-FixedScratch } catch { }
    if ($null -eq $oldNoSystem) { Remove-Item Env:GIT_CONFIG_NOSYSTEM -ErrorAction SilentlyContinue } else { $env:GIT_CONFIG_NOSYSTEM = $oldNoSystem }
    if ($null -eq $oldGlobal) { Remove-Item Env:GIT_CONFIG_GLOBAL -ErrorAction SilentlyContinue } else { $env:GIT_CONFIG_GLOBAL = $oldGlobal }
    if ($null -eq $oldPager) { Remove-Item Env:GIT_PAGER -ErrorAction SilentlyContinue } else { $env:GIT_PAGER = $oldPager }
    if ($acquired) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
