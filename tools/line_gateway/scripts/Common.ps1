Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-PowerShellSeven {
    if ($PSVersionTable.PSVersion.Major -lt 7) {
        throw 'PowerShell 7 or newer is required.'
    }
}

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Administrator privileges are required.'
    }
}

function Enter-PublicTransportLock {
    param([ValidateRange(1,60000)][int]$TimeoutMilliseconds=30000)
    $name='Global\AevryndLineGatewayPublicTransportControl'
    $security=[Security.AccessControl.MutexSecurity]::new()
    $security.SetAccessRuleProtection($true,$false)
    $rights=[Security.AccessControl.MutexRights]::FullControl
    foreach($sid in @(
        [Security.Principal.WindowsIdentity]::GetCurrent().User,
        [Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::LocalSystemSid,$null),
        [Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::BuiltinAdministratorsSid,$null)
    )){$security.AddAccessRule([Security.AccessControl.MutexAccessRule]::new($sid,$rights,[Security.AccessControl.AccessControlType]::Allow))}
    $created=$false
    try{$mutex=[Threading.MutexAcl]::Create($false,$name,[ref]$created,$security)}catch{throw "Unable to open the machine-wide public transport lock: $($_.Exception.Message)"}
    $acquired=$false;$abandoned=$false
    try{
        try{$acquired=$mutex.WaitOne($TimeoutMilliseconds)}catch [Threading.AbandonedMutexException]{$acquired=$true;$abandoned=$true}
        if(-not$acquired){throw 'Timed out waiting for the machine-wide public transport lock.'}
        return [pscustomobject]@{Mutex=$mutex;Acquired=$true;Abandoned=$abandoned;Name=$name;CreatedNew=$created}
    }catch{$mutex.Dispose();throw}
}

function Exit-PublicTransportLock {
    param([Parameter(Mandatory)][object]$Lock)
    try{if($Lock.Acquired){$Lock.Mutex.ReleaseMutex()}}finally{$Lock.Mutex.Dispose()}
}

function Get-GatewayPaths {
    param([string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway")
    $root = [IO.Path]::GetFullPath($InstallRoot)
    $expected = [IO.Path]::GetFullPath("$env:ProgramData\Aevrynd\LineGateway")
    if (-not [string]::Equals($root.TrimEnd('\'), $expected.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) {
        throw 'InstallRoot must remain below ProgramData at Aevrynd\LineGateway.'
    }
    return @{
        Root = $root
        App = Join-Path $root 'app'
        Scripts = Join-Path $root 'scripts'
        Config = Join-Path $root 'config'
        Data = Join-Path $root 'data'
        Logs = Join-Path $root 'logs'
        State = Join-Path $root 'state'
        StateFile = Join-Path $root 'state\gateway-state.json'
        TunnelStateFile = Join-Path $root 'state\tunnel-state.json'
        FunnelStateFile = Join-Path $root 'state\funnel-state.json'
        NgrokStateFile = Join-Path $root 'state\ngrok-state.json'
        NgrokPendingFile = Join-Path $root 'state\ngrok-pending.json'
        NgrokDomainFile = Join-Path $root 'config\ngrok-domain.json'
        Manifests = Join-Path $root 'manifests'
        InstallFile = Join-Path $root 'manifests\install.json'
        ManifestFile = Join-Path $root 'manifests\installation-manifest.json'
        ManifestHashFile = Join-Path $root 'manifests\installation-manifest.sha256'
        SettingsFile = Join-Path $root 'config\gateway.env'
        Runner = Join-Path $root 'scripts\gateway_runner.py'
        TunnelConfigFile = Join-Path $root 'cloudflared\config.yml'
    }
}

function Assert-ExactGatewayPath {
    param([hashtable]$Paths, [string]$Path, [string[]]$AllowedRelativePaths)
    $candidate = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    foreach ($relative in $AllowedRelativePaths) {
        $allowed = [IO.Path]::GetFullPath((Join-Path $Paths.Root $relative)).TrimEnd('\')
        if ([string]::Equals($candidate, $allowed, [StringComparison]::OrdinalIgnoreCase)) { return }
    }
    throw 'Filesystem target is outside the exact gateway path allowlist.'
}

function Get-InstalledPython {
    param([hashtable]$Paths)
    if (-not (Test-Path -LiteralPath $Paths.InstallFile -PathType Leaf)) {
        throw 'Installation metadata is missing.'
    }
    $metadata = Get-Content -Raw -LiteralPath $Paths.InstallFile | ConvertFrom-Json
    $python = [string]$metadata.pythonExe
    if ([string]::IsNullOrWhiteSpace($python) -or -not (Test-Path -LiteralPath $python -PathType Leaf)) {
        throw 'The installed Python executable is unavailable.'
    }
    return [IO.Path]::GetFullPath($python)
}

function Test-InstallationManifest {
    param([hashtable]$Paths)
    if (-not (Test-Path -LiteralPath $Paths.ManifestFile -PathType Leaf) -or
        -not (Test-Path -LiteralPath $Paths.ManifestHashFile -PathType Leaf)) {
        throw 'Installation manifest is missing.'
    }
    $hashBytes=[IO.File]::ReadAllBytes($Paths.ManifestHashFile);$manifestBytes=[IO.File]::ReadAllBytes($Paths.ManifestFile)
    try{$expectedManifestHash=[Text.UTF8Encoding]::new($false,$true).GetString($hashBytes).Trim().ToLowerInvariant();$sha=[Security.Cryptography.SHA256]::Create();try{$actualManifestHash=([Convert]::ToHexString($sha.ComputeHash($manifestBytes))).ToLowerInvariant()}finally{$sha.Dispose()}
    if ($expectedManifestHash -notmatch '^[0-9a-f]{64}$' -or $actualManifestHash -cne $expectedManifestHash) {
        throw 'Installation manifest hash validation failed.'
    }
    $manifest = ([Text.UTF8Encoding]::new($false,$true).GetString($manifestBytes)) | ConvertFrom-Json
    }finally{[Array]::Clear($hashBytes,0,$hashBytes.Length);[Array]::Clear($manifestBytes,0,$manifestBytes.Length)}
    if ($manifest.schemaVersion -ne 1 -or @($manifest.files).Count -lt 1) {
        throw 'Installation manifest is invalid.'
    }
    $expectedPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in @($manifest.files)) {
        $relative = ([string]$entry.path).Replace('/', '\')
        if ([string]::IsNullOrWhiteSpace($relative) -or [IO.Path]::IsPathRooted($relative)) {
            throw 'Installation manifest contains an unsafe path.'
        }
        if (-not $expectedPaths.Add($relative)) { throw 'Installation manifest contains a duplicate path.' }
        $candidate = [IO.Path]::GetFullPath((Join-Path $Paths.Root $relative))
        $rootPrefix = $Paths.Root.TrimEnd('\') + '\'
        if (-not $candidate.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Installation manifest path escaped the installation root.'
        }
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            throw 'An installed gateway file is missing.'
        }
        $item = Get-Item -LiteralPath $candidate
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Installed gateway files must not be links or reparse points.'
        }
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash.ToLowerInvariant()
        if ($item.Length -ne [long]$entry.size -or $hash -cne [string]$entry.sha256) {
            throw 'Installed gateway file integrity validation failed.'
        }
    }
    $actualPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($directory in @($Paths.App, $Paths.Scripts)) {
        if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw 'Installed executable directory is missing.' }
        if (((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Installed executable directories must not be links or reparse points.'
        }
        foreach ($file in Get-ChildItem -LiteralPath $directory -Recurse -File) {
            [void]$actualPaths.Add([IO.Path]::GetRelativePath($Paths.Root, $file.FullName))
        }
    }
    [void]$actualPaths.Add([IO.Path]::GetRelativePath($Paths.Root, $Paths.InstallFile))
    if (-not $actualPaths.SetEquals($expectedPaths)) {
        throw 'Installed executable file set differs from the closed manifest.'
    }
    return $manifest
}

function Read-NamedTunnelConfiguration {
    param([Parameter(Mandatory)][string]$ConfigPath)
    $fullPath = [IO.Path]::GetFullPath($ConfigPath)
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) { throw 'Named tunnel configuration is missing.' }
    $content = Get-Content -Raw -LiteralPath $fullPath
    if ($content.Contains("`t")) { throw 'Named tunnel configuration must not contain tabs.' }
    if ($content -match '(?i)trycloudflare\.com|quick[ -]?tunnel') { throw 'Quick Tunnel configuration is forbidden.' }
    $lines = @($content -split "`r?`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
    if ($lines.Count -ne 6) { throw 'Named tunnel configuration must use the exact six-line ingress schema.' }
    if ($lines[0] -notmatch '^tunnel:\s*([0-9a-fA-F-]{36})$') { throw 'Named tunnel UUID is missing.' }
    try { $tunnelId = [Guid]::Parse($Matches[1]).ToString() } catch { throw 'Named tunnel UUID is invalid.' }
    if ($lines[1] -notmatch '^credentials-file:\s*(.+)$') { throw 'Named tunnel credential file is missing.' }
    $credentialPath = $Matches[1].Trim().Trim('"').Trim("'")
    if (-not [IO.Path]::IsPathRooted($credentialPath)) {
        $credentialPath = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $fullPath) $credentialPath))
    }
    if ($credentialPath -match '(?i)replace_with' -or -not (Test-Path -LiteralPath $credentialPath -PathType Leaf)) {
        throw 'Named tunnel credential file is unavailable.'
    }
    if ($lines[2] -ne 'ingress:') { throw 'Named tunnel ingress section is invalid.' }
    if ($lines[3] -notmatch '^- hostname:\s*([A-Za-z0-9.-]+)$') { throw 'Named tunnel hostname is missing.' }
    $hostname = $Matches[1].ToLowerInvariant()
    if ($hostname.Length -gt 253 -or $hostname -notmatch '^[a-z0-9](?:[a-z0-9.-]*[a-z0-9])?$' -or
        $hostname -notmatch '\.' -or $hostname -match '(?i)localhost|replace_with|\.\.') {
        throw 'Named tunnel hostname is invalid.'
    }
    foreach ($label in $hostname.Split('.')) {
        if ($label.Length -lt 1 -or $label.Length -gt 63 -or $label.StartsWith('-') -or $label.EndsWith('-')) {
            throw 'Named tunnel hostname is invalid.'
        }
    }
    if ($lines[4] -ne 'service: http://127.0.0.1:8787') {
        throw 'Named tunnel may only target http://127.0.0.1:8787.'
    }
    if ($lines[5] -ne '- service: http_status:404') {
        throw 'Named tunnel must end with exactly one 404 catch-all rule.'
    }
    return [pscustomobject]@{
        ConfigPath = $fullPath
        ConfigSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $fullPath).Hash.ToLowerInvariant()
        CredentialPath = $credentialPath
        TunnelId = $tunnelId
        Hostname = $hostname
    }
}

function Import-GatewaySettings {
    param([hashtable]$Paths)
    if (-not (Test-Path -LiteralPath $Paths.SettingsFile -PathType Leaf)) {
        throw 'Non-secret gateway configuration is missing.'
    }
    $allowedNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    @(
        'AEVRYND_LINE_ALLOWED_USER_IDS',
        'AEVRYND_LINE_ALLOWED_GROUP_IDS',
        'AEVRYND_LINE_BIND_HOST',
        'AEVRYND_LINE_PORT',
        'AEVRYND_LINE_MAX_BODY_BYTES',
        'AEVRYND_LINE_REQUEST_TIMEOUT_SECONDS',
        'AEVRYND_LINE_MAX_EVENTS_PER_WEBHOOK',
        'AEVRYND_QWEN_BASE_URL',
        'AEVRYND_QWEN_MODEL',
        'AEVRYND_QWEN_SECONDARY_BASE_URL'
        'AEVRYND_ASSISTANT_TAICHUNG_QWEN_ROLE'
        'AEVRYND_ASSISTANT_TAOYUAN_QWEN_ROLE'
    ) | ForEach-Object { [void]$allowedNames.Add($_) }

    foreach ($line in Get-Content -LiteralPath $Paths.SettingsFile) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith('#')) { continue }
        if ($trimmed -notmatch '^([A-Z0-9_]+)=(.*)$') {
            throw 'Gateway configuration contains an invalid line.'
        }
        $name = $Matches[1]
        $value = $Matches[2].Trim()
        if (-not $allowedNames.Contains($name) -or [string]::IsNullOrWhiteSpace($value)) {
            throw 'Gateway configuration contains an unsupported or empty value.'
        }
        Set-Item -LiteralPath "Env:$name" -Value $value
    }
    if ($env:AEVRYND_LINE_BIND_HOST -ne '127.0.0.1' -or $env:AEVRYND_LINE_PORT -ne '8787') {
        throw 'Gateway must remain bound to 127.0.0.1:8787.'
    }
    $env:AEVRYND_LINE_INSTALL_ROOT = $Paths.Root
}

function Rotate-GatewayLogs {
    param([hashtable]$Paths, [long]$MaximumBytes = 20MB, [int]$Keep = 5)
    New-Item -ItemType Directory -Force -Path $Paths.Logs | Out-Null
    foreach ($name in @('gateway.stdout.log', 'gateway.stderr.log', 'cloudflared.stdout.log', 'cloudflared.stderr.log')) {
        $path = Join-Path $Paths.Logs $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        if ((Get-Item -LiteralPath $path).Length -lt $MaximumBytes) { continue }
        for ($index = $Keep - 1; $index -ge 1; $index--) {
            $source = "$path.$index"
            $destination = "$path.$($index + 1)"
            Assert-ExactGatewayPath -Paths $Paths -Path $destination -AllowedRelativePaths @("logs\$name.$($index + 1)")
            if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Force }
            if (Test-Path -LiteralPath $source) { Move-Item -LiteralPath $source -Destination $destination }
        }
        Move-Item -LiteralPath $path -Destination "$path.1" -Force
    }
}

function Read-GatewayState {
    param([hashtable]$Paths)
    if (-not (Test-Path -LiteralPath $Paths.StateFile -PathType Leaf)) { return $null }
    $state = Get-Content -Raw -LiteralPath $Paths.StateFile | ConvertFrom-Json
    if ($null -eq $state.pid -or $null -eq $state.startedUtc) {
        throw 'Gateway state file is invalid.'
    }
    return $state
}

function Test-GatewayProcess {
    param([hashtable]$Paths, [object]$State)
    if ($null -eq $State) { return $null }
    $process = Get-Process -Id ([int]$State.pid) -ErrorAction SilentlyContinue
    if ($null -eq $process) { return $null }
    $python = Get-InstalledPython -Paths $Paths
    try { $actual = [IO.Path]::GetFullPath($process.Path) } catch { return $null }
    if (-not [string]::Equals($actual, $python, [StringComparison]::OrdinalIgnoreCase)) { return $null }
    $processDetails = Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)" -ErrorAction SilentlyContinue
    if ($null -eq $processDetails -or [string]::IsNullOrWhiteSpace([string]$processDetails.CommandLine)) { return $null }
    if (-not ([string]$processDetails.CommandLine).Contains($Paths.Runner, [StringComparison]::OrdinalIgnoreCase)) { return $null }
    $expectedStart = [DateTimeOffset]::Parse([string]$State.startedUtc).UtcDateTime
    if ([Math]::Abs(($process.StartTime.ToUniversalTime() - $expectedStart).TotalSeconds) -gt 2) { return $null }
    return $process
}

function Write-GatewayState {
    param([hashtable]$Paths, [Diagnostics.Process]$Process)
    New-Item -ItemType Directory -Force -Path $Paths.State | Out-Null
    $document = [ordered]@{
        schemaVersion = 1
        pid = $Process.Id
        startedUtc = $Process.StartTime.ToUniversalTime().ToString('o')
        bindHost = '127.0.0.1'
        port = 8787
    }
    $temporary = "$($Paths.StateFile).$([Guid]::NewGuid().ToString('N')).tmp"
    [IO.File]::WriteAllText($temporary, ($document | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $Paths.StateFile -Force
}

function Read-TunnelState {
    param([hashtable]$Paths)
    if (-not (Test-Path -LiteralPath $Paths.TunnelStateFile -PathType Leaf)) { return $null }
    $state = Get-Content -Raw -LiteralPath $Paths.TunnelStateFile | ConvertFrom-Json
    if ($state.schemaVersion -ne 1 -or $state.status -notin @('pending', 'active') -or
        [string]::IsNullOrWhiteSpace([string]$state.executablePath) -or
        [string]::IsNullOrWhiteSpace([string]$state.configPath) -or
        [string]$state.configSha256 -notmatch '^[0-9a-f]{64}$' -or
        [string]$state.launchNonce -notmatch '^[0-9a-f]{32}$' -or
        $null -eq $state.createdUtc) {
        throw 'Tunnel state file is invalid.'
    }
    if ($state.status -eq 'active' -and ($null -eq $state.pid -or $null -eq $state.startedUtc)) {
        throw 'Active tunnel state is incomplete.'
    }
    return $state
}

function Invoke-GatewayEmergencyStops {
    param([string]$ScriptsRoot, [string]$InstallRoot)
    $failed = [Collections.Generic.List[string]]::new()
    foreach ($component in @(
        @{ Name = 'gateway'; Script = 'Stop-Gateway.ps1' },
        @{ Name = 'tunnel'; Script = 'Stop-Tunnel.ps1' },
        @{ Name = 'funnel'; Script = 'Stop-Funnel.ps1' }
        @{ Name = 'ngrok'; Script = 'Stop-Ngrok.ps1' }
    )) {
        try {
            & (Join-Path $ScriptsRoot $component.Script) -InstallRoot $InstallRoot | Out-Null
        } catch {
            $failed.Add($component.Name)
        }
    }
    return @($failed)
}

function Test-ExactTunnelCommandLine {
    param([string]$CommandLine, [string]$ExecutablePath, [string]$ConfigPath)
    if ([string]::IsNullOrWhiteSpace($CommandLine)) { return $false }
    $escapedExecutable = [regex]::Escape([IO.Path]::GetFullPath($ExecutablePath))
    $escapedConfig = [regex]::Escape([IO.Path]::GetFullPath($ConfigPath))
    $pattern = '(?i)^\s*"?' + $escapedExecutable + '"?\s+--config\s+"?' + $escapedConfig + '"?\s+tunnel\s+run\s*$'
    return $CommandLine -match $pattern
}

function Select-ExactTunnelRecord {
    param([object[]]$Records, [string]$ExecutablePath, [string]$ConfigPath)
    $all = @($Records)
    if ($all.Count -eq 0) { return $null }
    $expectedExecutable = [IO.Path]::GetFullPath($ExecutablePath)
    $exact = @($all | Where-Object {
        -not [string]::IsNullOrWhiteSpace([string]$_.ExecutablePath) -and
        [string]::Equals([IO.Path]::GetFullPath([string]$_.ExecutablePath), $expectedExecutable, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-ExactTunnelCommandLine -CommandLine ([string]$_.CommandLine) -ExecutablePath $expectedExecutable -ConfigPath $ConfigPath)
    })
    if ($all.Count -ne 1 -or $exact.Count -ne 1) {
        throw 'Tunnel process enumeration is ambiguous; no process was stopped.'
    }
    return $exact[0]
}

function Test-TunnelProcess {
    param([hashtable]$Paths, [object]$State)
    if ($null -eq $State -or $State.status -ne 'active') { return $null }
    $process = Get-Process -Id ([int]$State.pid) -ErrorAction SilentlyContinue
    if ($null -eq $process) { return $null }
    try {
        $actualExecutable = [IO.Path]::GetFullPath($process.Path)
        $expectedExecutable = [IO.Path]::GetFullPath([string]$State.executablePath)
        $expectedConfig = [IO.Path]::GetFullPath([string]$State.configPath)
    } catch { return $null }
    if ([IO.Path]::GetFileName($expectedExecutable) -ne 'cloudflared.exe' -or
        -not [string]::Equals($actualExecutable, $expectedExecutable, [StringComparison]::OrdinalIgnoreCase)) {
        return $null
    }
    if (-not [string]::Equals($expectedConfig, [IO.Path]::GetFullPath($Paths.TunnelConfigFile), [StringComparison]::OrdinalIgnoreCase)) {
        return $null
    }
    $details = Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)" -ErrorAction SilentlyContinue
    if ($null -eq $details) { return $null }
    $commandLine = [string]$details.CommandLine
    if (-not (Test-ExactTunnelCommandLine -CommandLine $commandLine -ExecutablePath $expectedExecutable -ConfigPath $expectedConfig)) { return $null }
    $expectedStart = [DateTimeOffset]::Parse([string]$State.startedUtc).UtcDateTime
    if ([Math]::Abs(($process.StartTime.ToUniversalTime() - $expectedStart).TotalSeconds) -gt 2) { return $null }
    return $process
}

function Write-PendingTunnelState {
    param(
        [hashtable]$Paths,
        [string]$ExecutablePath,
        [string]$ConfigPath,
        [string]$ConfigSha256,
        [string]$LaunchNonce
    )
    New-Item -ItemType Directory -Force -Path $Paths.State | Out-Null
    $document = [ordered]@{
        schemaVersion = 1
        status = 'pending'
        createdUtc = [DateTime]::UtcNow.ToString('o')
        launchNonce = $LaunchNonce
        executablePath = [IO.Path]::GetFullPath($ExecutablePath)
        configPath = [IO.Path]::GetFullPath($ConfigPath)
        configSha256 = $ConfigSha256
    }
    $temporary = "$($Paths.TunnelStateFile).$([Guid]::NewGuid().ToString('N')).tmp"
    [IO.File]::WriteAllText($temporary, ($document | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $Paths.TunnelStateFile -Force
}

function Activate-TunnelState {
    param([hashtable]$Paths, [object]$PendingState, [Diagnostics.Process]$Process)
    if ($PendingState.status -ne 'pending') { throw 'Tunnel state activation requires pending state.' }
    $document = [ordered]@{
        schemaVersion = 1
        status = 'active'
        createdUtc = [string]$PendingState.createdUtc
        launchNonce = [string]$PendingState.launchNonce
        pid = $Process.Id
        startedUtc = $Process.StartTime.ToUniversalTime().ToString('o')
        executablePath = [string]$PendingState.executablePath
        configPath = [string]$PendingState.configPath
        configSha256 = [string]$PendingState.configSha256
    }
    $temporary = "$($Paths.TunnelStateFile).$([Guid]::NewGuid().ToString('N')).tmp"
    [IO.File]::WriteAllText($temporary, ($document | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $Paths.TunnelStateFile -Force
}

function Find-RecoverableTunnelProcess {
    param([hashtable]$Paths, [object]$PendingState = $null)
    $records = @(Get-CimInstance Win32_Process -Filter "Name = 'cloudflared.exe'" -ErrorAction SilentlyContinue)
    if ($records.Count -eq 0) { return $null }
    if ($null -eq $PendingState) {
        $commands = @(Get-Command cloudflared.exe -All -ErrorAction SilentlyContinue)
        $resolvedPaths = @($commands | ForEach-Object { [IO.Path]::GetFullPath($_.Source) } | Sort-Object -Unique)
        if ($resolvedPaths.Count -ne 1) { throw 'A unique cloudflared executable cannot be resolved for emergency process discovery.' }
        $executable = $resolvedPaths[0]
        $config = $Paths.TunnelConfigFile
        $createdUtc = $null
    } else {
        if ($PendingState.status -ne 'pending') { throw 'Pending tunnel recovery state is invalid.' }
        $executable = [IO.Path]::GetFullPath([string]$PendingState.executablePath)
        $config = [IO.Path]::GetFullPath([string]$PendingState.configPath)
        $createdUtc = [DateTimeOffset]::Parse([string]$PendingState.createdUtc).UtcDateTime
    }
    if (-not [string]::Equals([IO.Path]::GetFullPath($config), [IO.Path]::GetFullPath($Paths.TunnelConfigFile), [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Tunnel recovery config path is outside the fixed allowlist.'
    }
    $selected = Select-ExactTunnelRecord -Records $records -ExecutablePath $executable -ConfigPath $config
    if ($null -eq $selected) { return $null }
    $process = Get-Process -Id ([int]$selected.ProcessId) -ErrorAction SilentlyContinue
    if ($null -eq $process) { return $null }
    if ($null -ne $createdUtc -and $process.StartTime.ToUniversalTime() -lt $createdUtc.AddSeconds(-2)) {
        throw 'Recovered tunnel process predates pending launch intent.'
    }
    return $process
}

Assert-PowerShellSeven
