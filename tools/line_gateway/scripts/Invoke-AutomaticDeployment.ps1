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
$gcmExe = 'C:\Program Files\Git\mingw64\bin\git-credential-manager.exe'
$githubCliExe = 'C:\Program Files\GitHub CLI\gh.exe'
$startedUtc = [DateTimeOffset]::UtcNow
$release = ''
$oldNoSystem = $null
$oldGlobal = $null
$oldPager = $null

function Write-SafeResult {
    param([Parameter(Mandatory)][ValidateSet('running','succeeded','failed','rolled_back','rollback_failed')][string]$Status,
          [Parameter(Mandatory)][ValidateSet('none','source_rejected','busy','drain_failed','stop_failed','install_failed','start_failed','health_failed','rollback_failed')][string]$Code,
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
    # 同磁碟 overwrite move，讀取端只會看到完整舊檔或完整新檔。
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        try { [IO.File]::Move($temporary, $resultFile, $true); return }
        catch [IO.IOException] { if ($attempt -eq 19) { throw }; Start-Sleep -Milliseconds 50 }
    }
}

function Invoke-QueueDeploymentControl {
    param([Parameter(Mandatory)][ValidateSet('count','recover')][string]$Mode,
          [Parameter(Mandatory)][string]$PythonExe)
    $database = 'C:\ProgramData\Aevrynd\LineGateway\data\queue.sqlite3'
    $program = @'
import sqlite3,sys,time
mode=sys.argv[1]
database=sys.argv[2]
connection=sqlite3.connect(database,timeout=5.0)
try:
    if mode == "count":
        total=0
        for table in ("jobs","deliveries","reception_notifications"):
            total += int(connection.execute(f"SELECT COUNT(*) FROM {table} WHERE status='running'").fetchone()[0])
        print(total)
    elif mode == "recover":
        now=time.time()
        connection.execute("BEGIN IMMEDIATE")
        changed=0
        changed += connection.execute("UPDATE jobs SET status='queued', attempt_count=MAX(0,attempt_count-1), available_at=?, lease_until=NULL, lease_owner='', updated_at=?, error_code='' WHERE status='running'",(now+1.0,now)).rowcount
        for table in ("deliveries","reception_notifications"):
            changed += connection.execute(f"UPDATE {table} SET status='queued', attempt_count=MAX(0,attempt_count-1), available_at=?, lease_until=NULL, lease_owner='', updated_at=?, error_code='' WHERE status='running'",(now+1.0,now)).rowcount
        connection.commit()
        print(changed)
    else:
        raise RuntimeError("invalid mode")
finally:
    connection.close()
'@
    $output = & $PythonExe -B -c $program $Mode $database 2>$null
    if ($LASTEXITCODE -ne 0 -or @($output).Count -ne 1 -or [string]$output -notmatch '^\d+$') { throw 'queue_control_failed' }
    return [int]$output
}

function Wait-GatewayQueueDrain {
    param([Parameter(Mandatory)][string]$PythonExe,[ValidateRange(1,180)][int]$TimeoutSeconds=120)
    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if ((Invoke-QueueDeploymentControl -Mode count -PythonExe $PythonExe) -eq 0) { return }
        Start-Sleep -Milliseconds 500
    } while ([DateTimeOffset]::UtcNow -lt $deadline)
    # 超時不把正常工作標成失敗；停止後會在 gateway 尚未重啟時立即安全歸還租約。
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

function Assert-TrustedProgramFile {
    param([Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][string]$Expected,[Parameter(Mandatory)][string]$SignerPattern)
    if (-not [string]::Equals([IO.Path]::GetFullPath($Path), $Expected, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $Path -PathType Leaf) -or $Path.Length -gt 220 -or
        ((Get-Item -LiteralPath $Path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'source_rejected' }
    $cursor = [IO.Path]::GetDirectoryName($Path)
    while ($cursor.Length -ge 'C:\Program Files'.Length) {
        if (((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'source_rejected' }
        if ([string]::Equals($cursor, 'C:\Program Files', [StringComparison]::OrdinalIgnoreCase)) { break }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch $SignerPattern) { throw 'source_rejected' }
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

$mutex = $null
$acquired = $false
try {
    New-Item -ItemType Directory -Force -Path $statusRoot | Out-Null
    Write-SafeResult -Status running -Code none
    $mutex = [Threading.Mutex]::new($false, 'Global\AevryndLineGatewayAutomaticDeployment')
    try { $acquired = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $acquired = $true }
    if (-not $acquired) { Write-SafeResult -Status failed -Code busy; exit 2 }

    Assert-FixedSafePath -Path $repositoryRoot -Expected 'D:\Download\Aevrynd\Games'
    Assert-FixedSafePath -Path $liveRoot -Expected 'C:\ProgramData\Aevrynd\LineGateway' -AllowMissing
    Assert-FixedSafePath -Path $deploymentRoot -Expected 'C:\ProgramData\Aevrynd\LineGatewayAutomaticDeployment'
    $gitDirectory = Join-Path $repositoryRoot '.git'
    if (-not (Test-Path -LiteralPath $gitDirectory -PathType Container) -or
        ((Get-Item -LiteralPath $gitDirectory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'source_rejected' }
    Assert-TrustedProgramFile -Path $gitExe -Expected 'C:\Program Files\Git\cmd\git.exe' -SignerPattern '(?i)O=Johannes Schindelin'
    Assert-TrustedProgramFile -Path $gcmExe -Expected 'C:\Program Files\Git\mingw64\bin\git-credential-manager.exe' -SignerPattern '(?i)O=Microsoft Corporation'
    Assert-TrustedProgramFile -Path $githubCliExe -Expected 'C:\Program Files\GitHub CLI\gh.exe' -SignerPattern '(?i)O="?GitHub, Inc\."?'

    # 只接受遠端 main 的 authoritative commit；本機工作樹與 origin 設定都不是信任來源。
    $oldNoSystem = $env:GIT_CONFIG_NOSYSTEM
    $oldGlobal = $env:GIT_CONFIG_GLOBAL
    $oldPager = $env:GIT_PAGER
    $env:GIT_CONFIG_NOSYSTEM = '1'
    $env:GIT_CONFIG_GLOBAL = 'NUL'
    $env:GIT_PAGER = 'cat'
    $gitBaseSafety = @('-c','protocol.file.allow=never','-c','core.fsmonitor=false','-c','credential.helper=','-c','credential.interactive=never','-c','core.hooksPath=NUL')
    # 優先使用 Microsoft 簽章 GCM；若此帳號的 private-origin credential 不在 GCM，才選固定 GitHub CLI helper。
    $helperCandidates = @(
        '!"C:/Program Files/Git/mingw64/bin/git-credential-manager.exe"',
        '!"C:/Program Files/GitHub CLI/gh.exe" auth git-credential'
    )
    $gitSafety = $null
    $remoteLines = @()
    foreach ($helper in $helperCandidates) {
        $candidateSafety = @($gitBaseSafety + @('-c', "credential.helper=$helper"))
        # 網路查詢在受保護且不含 .git 的 ProgramData 根目錄執行，不讀使用者可寫的 local Git config。
        $candidateLines = @(& $gitExe @candidateSafety -C $deploymentRoot ls-remote $expectedOrigin $expectedBranch 2>$null)
        if ($LASTEXITCODE -eq 0 -and $candidateLines.Count -eq 1 -and $candidateLines[0] -match '^([0-9a-f]{40})\s+refs/heads/main$') {
            $gitSafety = $candidateSafety
            $remoteLines = $candidateLines
            break
        }
    }
    if ($null -eq $gitSafety -or $remoteLines.Count -ne 1 -or $remoteLines[0] -notmatch '^([0-9a-f]{40})\s+refs/heads/main$') { throw 'source_rejected' }
    $release = $Matches[1]
    $head = (& $gitExe @gitSafety -C $repositoryRoot rev-parse HEAD 2>$null).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cne $release) { throw 'source_rejected' }
    & $gitExe @gitSafety -C $repositoryRoot diff --no-ext-diff --quiet -- tools/line_gateway
    if ($LASTEXITCODE -ne 0) { throw 'source_rejected' }
    & $gitExe @gitSafety -C $repositoryRoot diff --cached --no-ext-diff --quiet -- tools/line_gateway
    if ($LASTEXITCODE -ne 0) { throw 'source_rejected' }

    Remove-FixedScratch
    New-Item -ItemType Directory -Path $scratchRoot | Out-Null
    $trustedGitDirectory = Join-Path $scratchRoot 'source.git'
    & $gitExe @gitSafety init --bare $trustedGitDirectory 2>$null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $trustedGitDirectory -PathType Container)) { throw 'source_rejected' }
    $archive = Join-Path $scratchRoot 'source.zip'
    & $gitExe @gitSafety --git-dir=$trustedGitDirectory fetch --no-tags $expectedOrigin $release 2>$null
    if ($LASTEXITCODE -ne 0) { throw 'source_rejected' }
    & $gitExe @gitSafety --git-dir=$trustedGitDirectory archive --format=zip --output=$archive FETCH_HEAD tools/line_gateway
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
    $installedPython = $null
    if (Test-Path -LiteralPath (Join-Path $liveRoot 'manifests\installation-manifest.json') -PathType Leaf) {
        . (Join-Path $liveRoot 'scripts\Common.ps1')
        $currentPaths = Get-GatewayPaths -InstallRoot $liveRoot
        $null = Test-InstallationManifest -Paths $currentPaths
        $installedPython = Get-InstalledPython -Paths $currentPaths
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

    if ($null -ne $installedPython) {
        try { Wait-GatewayQueueDrain -PythonExe $installedPython } catch { Write-SafeResult -Status failed -Code drain_failed -Release $release; exit 9 }
    }
    try { & (Join-Path $liveRoot 'scripts\Stop-Gateway.ps1') -InstallRoot $liveRoot | Out-Null } catch { Write-SafeResult -Status failed -Code stop_failed -Release $release; exit 3 }
    if ($null -ne $installedPython) {
        try { $null = Invoke-QueueDeploymentControl -Mode recover -PythonExe $installedPython }
        catch {
            # Gateway 已停止時若無法歸還租約，不繼續切換版本；先恢復原服務，工作仍可由既有 lease expiry 接手。
            try { & (Join-Path $liveRoot 'scripts\Start-Gateway.ps1') -InstallRoot $liveRoot | Out-Null } catch { }
            Write-SafeResult -Status failed -Code drain_failed -Release $release
            exit 9
        }
    }
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
    try { Write-SafeResult -Status failed -Code source_rejected -Release $release } catch { }
    exit 8
} finally {
    try { Remove-FixedScratch } catch { }
    if ($null -eq $oldNoSystem) { Remove-Item Env:GIT_CONFIG_NOSYSTEM -ErrorAction SilentlyContinue } else { $env:GIT_CONFIG_NOSYSTEM = $oldNoSystem }
    if ($null -eq $oldGlobal) { Remove-Item Env:GIT_CONFIG_GLOBAL -ErrorAction SilentlyContinue } else { $env:GIT_CONFIG_GLOBAL = $oldGlobal }
    if ($null -eq $oldPager) { Remove-Item Env:GIT_PAGER -ErrorAction SilentlyContinue } else { $env:GIT_PAGER = $oldPager }
    if ($acquired -and $null -ne $mutex) { $mutex.ReleaseMutex() }
    if ($null -ne $mutex) { $mutex.Dispose() }
}
