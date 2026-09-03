Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$scriptsRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $scriptsRoot 'Common.ps1')

$script:passed = 0
$script:failed = 0

function Assert-Equal {
    param([object]$Actual, [object]$Expected, [string]$Name)
    if ($Actual -ne $Expected) { throw "$Name expected '$Expected' but received '$Actual'." }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Name)
    try { & $Action; throw "$Name did not reject the hostile fixture." }
    catch {
        if ($_.Exception.Message -eq "$Name did not reject the hostile fixture.") { throw }
    }
}

function Invoke-Test {
    param([string]$Name, [scriptblock]$Action)
    try {
        & $Action
        $script:passed++
        Write-Output "PASS $Name"
    } catch {
        $script:failed++
        Write-Output "FAIL $Name : $($_.Exception.GetType().FullName) : $($_.Exception.Message)"
    }
}

$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) "aevrynd-line-gateway-tests-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
try {
    Invoke-Test 'secondary Qwen credential setter has no plaintext input path' {
        $content = Get-Content -Raw (Join-Path $scriptsRoot 'Set-SecondaryQwenCredential.ps1')
        foreach ($required in @('Assert-Administrator','InstallFile','Read-Host','AsSecureString','CredWriteW','Aevrynd/LineGateway/SecondaryQwenApiKey','ZeroFreeCoTaskMemUnicode','1280')) { if (-not $content.Contains($required)) { throw "secondary credential setter lacks $required" } }
        if ($content -match '(?i)param\([^)]*(ApiKey|Secret|Token)' -or $content -match '(?i)(Set-Content|WriteAllText|WriteAllBytes).*secure') { throw 'secondary credential setter exposes plaintext input or persistence' }
    }
    Invoke-Test 'local light router is fixed to loopback and credential manager' {
        $configuration = Get-Content -Raw (Join-Path $scriptsRoot 'Set-Configuration.ps1')
        foreach ($required in @('http://127.0.0.1:18080/v1','Qwen3-4B-Q4_K_M.gguf','AEVRYND_QWEN_LOCAL_BASE_URL','AEVRYND_QWEN_LOCAL_MODEL')) {
            if (-not $configuration.Contains($required)) { throw "local router configuration lacks $required" }
        }
        if ($configuration -match '(?i)param\([^)]*(LocalQwenApiKey|LocalSecret)') { throw 'local router exposes a plaintext key parameter' }
        $pythonConfig = Get-Content -Raw (Join-Path $scriptsRoot '..\config.py')
        if (-not $pythonConfig.Contains('Aevrynd/QwenLight/ApiKey')) { throw 'local router does not use the existing credential target' }
    }
    $credentialPath = Join-Path $temporaryRoot '11111111-1111-1111-1111-111111111111.json'
    [IO.File]::WriteAllText($credentialPath, '{}', [Text.UTF8Encoding]::new($false))
    $validLines = @(
        'tunnel: 11111111-1111-1111-1111-111111111111',
        "credentials-file: $credentialPath",
        'ingress:',
        '- hostname: line.example.com',
        'service: http://127.0.0.1:8787',
        '- service: http_status:404'
    )

    Invoke-Test 'strict named tunnel accepts exact schema' {
        $path = Join-Path $temporaryRoot 'valid.yml'
        [IO.File]::WriteAllLines($path, $validLines, [Text.UTF8Encoding]::new($false))
        $result = Read-NamedTunnelConfiguration -ConfigPath $path
        Assert-Equal $result.Hostname 'line.example.com' 'hostname'
        Assert-Equal $result.TunnelId '11111111-1111-1111-1111-111111111111' 'tunnel id'
    }

    $hostile = [ordered]@{
        'second hostname' = @($validLines[0..4] + '- hostname: other.example.com' + '- service: http_status:404')
        'ssh service' = @($validLines | ForEach-Object { if ($_ -eq 'service: http://127.0.0.1:8787') { 'service: ssh://localhost:22' } else { $_ } })
        'tcp service' = @($validLines | ForEach-Object { if ($_ -eq 'service: http://127.0.0.1:8787') { 'service: tcp://127.0.0.1:8787' } else { $_ } })
        'other http service' = @($validLines | ForEach-Object { if ($_ -eq 'service: http://127.0.0.1:8787') { 'service: http://127.0.0.1:9999' } else { $_ } })
        'wildcard bind service' = @($validLines | ForEach-Object { if ($_ -eq 'service: http://127.0.0.1:8787') { 'service: http://0.0.0.0:8787' } else { $_ } })
        'missing catch all' = @($validLines[0..4])
        'rule after catch all' = @($validLines + 'originRequest:')
        'quick tunnel marker' = @($validLines + '# trycloudflare.com')
    }
    foreach ($fixture in $hostile.GetEnumerator()) {
        Invoke-Test "reject $($fixture.Key)" {
            $path = Join-Path $temporaryRoot (($fixture.Key -replace '[^A-Za-z0-9]', '-') + '.yml')
            [IO.File]::WriteAllLines($path, [string[]]$fixture.Value, [Text.UTF8Encoding]::new($false))
            Assert-Throws { Read-NamedTunnelConfiguration -ConfigPath $path | Out-Null } $fixture.Key
        }
    }

    Invoke-Test 'closed manifest rejects injected Python package' {
        $root = Join-Path $temporaryRoot 'manifest-fixture'
        $app = Join-Path $root 'app'
        $package = Join-Path $app 'tools\line_gateway'
        $scripts = Join-Path $root 'scripts'
        $manifests = Join-Path $root 'manifests'
        New-Item -ItemType Directory -Path $package,$scripts,$manifests | Out-Null
        [IO.File]::WriteAllText((Join-Path $package '__init__.py'), '# fixture', [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $scripts 'gateway_runner.py'), '# fixture', [Text.UTF8Encoding]::new($false))
        $installFile = Join-Path $manifests 'install.json'
        [IO.File]::WriteAllText($installFile, '{"schemaVersion":1}', [Text.UTF8Encoding]::new($false))
        $files = @(Get-ChildItem -LiteralPath $app,$scripts -Recurse -File) + @(Get-Item -LiteralPath $installFile)
        $manifestDocument = [ordered]@{
            schemaVersion = 1
            files = @($files | ForEach-Object {
                [ordered]@{
                    path = [IO.Path]::GetRelativePath($root, $_.FullName).Replace('\', '/')
                    size = $_.Length
                    sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
                }
            })
        }
        $manifestFile = Join-Path $manifests 'installation-manifest.json'
        $manifestHashFile = Join-Path $manifests 'installation-manifest.sha256'
        [IO.File]::WriteAllText($manifestFile, ($manifestDocument | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($manifestHashFile, (Get-FileHash -Algorithm SHA256 -LiteralPath $manifestFile).Hash.ToLowerInvariant(), [Text.UTF8Encoding]::new($false))
        $fixturePaths = @{
            Root = $root; App = $app; Scripts = $scripts; Manifests = $manifests
            InstallFile = $installFile; ManifestFile = $manifestFile; ManifestHashFile = $manifestHashFile
        }
        $null = Test-InstallationManifest -Paths $fixturePaths
        [IO.File]::WriteAllText((Join-Path $package 'injected.py'), 'raise SystemExit()', [Text.UTF8Encoding]::new($false))
        Assert-Throws { Test-InstallationManifest -Paths $fixturePaths | Out-Null } 'injected Python package'
    }

    Invoke-Test 'known Python cache cleanup removes only the closed module set' {
        $root = Join-Path $temporaryRoot 'known-cache-fixture'
        $app = Join-Path $root 'app'
        $cache = Join-Path $app 'tools\line_gateway\__pycache__'
        New-Item -ItemType Directory -Path $cache -Force | Out-Null
        $modules = @('__init__', 'config', 'routing')
        foreach ($module in $modules) {
            [IO.File]::WriteAllBytes((Join-Path $cache "$module.cpython-312.pyc"), [byte[]](1,2,3))
        }
        $removed = Remove-KnownGatewayPythonCache -Paths @{Root=$root;App=$app} -AllowedModuleNames $modules -PythonCacheTag 'cpython-312'
        if (-not $removed -or (Test-Path -LiteralPath $cache)) {
            throw 'Known Python cache was not removed exactly.'
        }
    }

    Invoke-Test 'hostile Python cache rejects unknown entries without partial deletion' {
        $root = Join-Path $temporaryRoot 'hostile-cache-fixture'
        $app = Join-Path $root 'app'
        $cache = Join-Path $app 'tools\line_gateway\__pycache__'
        New-Item -ItemType Directory -Path $cache -Force | Out-Null
        $known = Join-Path $cache 'config.cpython-312.pyc'
        $unknown = Join-Path $cache 'payload.cpython-312.pyc'
        [IO.File]::WriteAllBytes($known, [byte[]](1,2,3))
        [IO.File]::WriteAllBytes($unknown, [byte[]](4,5,6))
        Assert-Throws {
            Remove-KnownGatewayPythonCache -Paths @{Root=$root;App=$app} -AllowedModuleNames @('config') -PythonCacheTag 'cpython-312'
        } 'unknown Python cache entry'
        if (-not (Test-Path -LiteralPath $known -PathType Leaf) -or -not (Test-Path -LiteralPath $unknown -PathType Leaf)) {
            throw 'Hostile cache validation partially deleted files before failing closed.'
        }
    }

    Invoke-Test 'hostile Python cache rejects nested directories without deletion' {
        $root = Join-Path $temporaryRoot 'nested-cache-fixture'
        $app = Join-Path $root 'app'
        $cache = Join-Path $app 'tools\line_gateway\__pycache__'
        New-Item -ItemType Directory -Path (Join-Path $cache 'nested') -Force | Out-Null
        Assert-Throws {
            Remove-KnownGatewayPythonCache -Paths @{Root=$root;App=$app} -AllowedModuleNames @('config') -PythonCacheTag 'cpython-312'
        } 'nested Python cache entry'
        if (-not (Test-Path -LiteralPath (Join-Path $cache 'nested') -PathType Container)) {
            throw 'Hostile cache directory was deleted before validation completed.'
        }
    }

    Invoke-Test 'gateway launch disables bytecode before application imports' {
        $start = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Start-Gateway.ps1')
        $runner = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'gateway_runner.py')
        if (([regex]::Matches($start, "PYTHONDONTWRITEBYTECODE = '1'")).Count -lt 2 -or
            -not $start.Contains('& $python -B -c $probe') -or
            -not $start.Contains("-ArgumentList @('-B', `$paths.Runner)")) {
            throw 'Gateway probe and runtime do not both enforce bytecode suppression.'
        }
        $guardIndex = $runner.IndexOf('sys.dont_write_bytecode = True', [StringComparison]::Ordinal)
        $importIndex = $runner.IndexOf('from tools.line_gateway.codex_handoff import CodexHandoff', [StringComparison]::Ordinal)
        if ($guardIndex -lt 0 -or $importIndex -lt 0 -or $guardIndex -gt $importIndex) {
            throw 'Runner bytecode guard does not precede application imports.'
        }
    }

    Invoke-Test 'closed manifest rejects content replacement' {
        $root = Join-Path $temporaryRoot 'tamper-fixture'
        $app = Join-Path $root 'app'
        $scripts = Join-Path $root 'scripts'
        $manifests = Join-Path $root 'manifests'
        New-Item -ItemType Directory -Path $app,$scripts,$manifests | Out-Null
        $appFile = Join-Path $app 'module.py'
        $runner = Join-Path $scripts 'gateway_runner.py'
        $installFile = Join-Path $manifests 'install.json'
        [IO.File]::WriteAllText($appFile, 'safe = True', [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($runner, 'safe = True', [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($installFile, '{"schemaVersion":1}', [Text.UTF8Encoding]::new($false))
        $files = @(Get-Item -LiteralPath $appFile,$runner,$installFile)
        $document = [ordered]@{ schemaVersion = 1; files = @($files | ForEach-Object {
            [ordered]@{ path = [IO.Path]::GetRelativePath($root, $_.FullName).Replace('\', '/'); size = $_.Length; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant() }
        }) }
        $manifestFile = Join-Path $manifests 'installation-manifest.json'
        $manifestHashFile = Join-Path $manifests 'installation-manifest.sha256'
        [IO.File]::WriteAllText($manifestFile, ($document | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($manifestHashFile, (Get-FileHash -Algorithm SHA256 -LiteralPath $manifestFile).Hash.ToLowerInvariant(), [Text.UTF8Encoding]::new($false))
        [IO.File]::AppendAllText($appFile, '; safe = False')
        $fixturePaths = @{ Root=$root; App=$app; Scripts=$scripts; Manifests=$manifests; InstallFile=$installFile; ManifestFile=$manifestFile; ManifestHashFile=$manifestHashFile }
        Assert-Throws { Test-InstallationManifest -Paths $fixturePaths | Out-Null } 'content replacement'
    }

    Invoke-Test 'corrupt manifest hash is rejected' {
        $root = Join-Path $temporaryRoot 'corrupt-manifest-fixture'
        $app = Join-Path $root 'app'
        $scripts = Join-Path $root 'scripts'
        $manifests = Join-Path $root 'manifests'
        New-Item -ItemType Directory -Path $app,$scripts,$manifests | Out-Null
        $appFile = Join-Path $app 'module.py'
        $runner = Join-Path $scripts 'gateway_runner.py'
        $installFile = Join-Path $manifests 'install.json'
        [IO.File]::WriteAllText($appFile, 'safe = True', [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($runner, 'safe = True', [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($installFile, '{"schemaVersion":1}', [Text.UTF8Encoding]::new($false))
        $files = @(Get-Item -LiteralPath $appFile,$runner,$installFile)
        $document = [ordered]@{ schemaVersion = 1; files = @($files | ForEach-Object {
            [ordered]@{ path = [IO.Path]::GetRelativePath($root, $_.FullName).Replace('\', '/'); size = $_.Length; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant() }
        }) }
        $manifestFile = Join-Path $manifests 'installation-manifest.json'
        $manifestHashFile = Join-Path $manifests 'installation-manifest.sha256'
        [IO.File]::WriteAllText($manifestFile, ($document | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($manifestHashFile, ('0' * 64), [Text.UTF8Encoding]::new($false))
        $fixturePaths = @{ Root=$root; App=$app; Scripts=$scripts; Manifests=$manifests; InstallFile=$installFile; ManifestFile=$manifestFile; ManifestHashFile=$manifestHashFile }
        Assert-Throws { Test-InstallationManifest -Paths $fixturePaths | Out-Null } 'corrupt manifest hash'
    }

    Invoke-Test 'tunnel process identity uses complete state tuple' {
        $common = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Common.ps1')
        $start = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Start-Tunnel.ps1')
        foreach ($marker in @('pending', 'active', 'pid', 'startedUtc', 'executablePath', 'configPath', 'configSha256', 'CommandLine')) {
            if (-not $common.Contains($marker, [StringComparison]::Ordinal)) { throw "Tunnel state marker is missing: $marker" }
        }
        $pendingIndex = $start.IndexOf('Write-PendingTunnelState', [StringComparison]::Ordinal)
        $processIndex = $start.IndexOf('Start-Process', [StringComparison]::Ordinal)
        $activeIndex = $start.LastIndexOf('Activate-TunnelState', [StringComparison]::Ordinal)
        if ($pendingIndex -lt 0 -or $processIndex -lt 0 -or $activeIndex -lt 0 -or
            $pendingIndex -gt $processIndex -or $processIndex -gt $activeIndex) {
            throw 'Tunnel pending/launch/active handshake ordering is unsafe.'
        }
    }

    Invoke-Test 'pending crash window recovers only exact command' {
        $executable = 'C:\Tools\cloudflared.exe'
        $config = 'C:\ProgramData\Aevrynd\LineGateway\cloudflared\config.yml'
        $command = '"C:\Tools\cloudflared.exe" --config "C:\ProgramData\Aevrynd\LineGateway\cloudflared\config.yml" tunnel run'
        $record = [pscustomobject]@{ ExecutablePath=$executable; CommandLine=$command; ProcessId=123 }
        $selected = Select-ExactTunnelRecord -Records @($record) -ExecutablePath $executable -ConfigPath $config
        Assert-Equal $selected.ProcessId 123 'recovered process id'
        $wrong = [pscustomobject]@{ ExecutablePath=$executable; CommandLine=($command + ' --extra'); ProcessId=124 }
        Assert-Throws { Select-ExactTunnelRecord -Records @($wrong) -ExecutablePath $executable -ConfigPath $config | Out-Null } 'ambiguous command'
        Assert-Throws { Select-ExactTunnelRecord -Records @($record,$record) -ExecutablePath $executable -ConfigPath $config | Out-Null } 'multiple exact processes'
    }

    Invoke-Test 'config deletion does not erase process identity' {
        $executable = 'C:\Tools\cloudflared.exe'
        $missingConfig = Join-Path $temporaryRoot 'already-deleted-config.yml'
        $command = '"C:\Tools\cloudflared.exe" --config "' + $missingConfig + '" tunnel run'
        if (Test-Path -LiteralPath $missingConfig) { throw 'Config deletion fixture must not exist.' }
        if (-not (Test-ExactTunnelCommandLine -CommandLine $command -ExecutablePath $executable -ConfigPath $missingConfig)) {
            throw 'Exact process identity incorrectly depends on config file existence.'
        }
    }

    Invoke-Test 'corrupt tunnel state enters exact emergency discovery' {
        $fixtureProgramData = Join-Path $temporaryRoot 'corrupt-state-program-data'
        $fixtureRoot = Join-Path $fixtureProgramData 'Aevrynd\LineGateway'
        $fixtureState = Join-Path $fixtureRoot 'state'
        New-Item -ItemType Directory -Path $fixtureState -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $fixtureState 'tunnel-state.json'), '{"schemaVersion":1,"status":', [Text.UTF8Encoding]::new($false))
        function Get-CimInstance {
            [CmdletBinding()]
            param([string]$ClassName,[string]$Filter)
            if ($ClassName -ne 'Win32_Process' -or $Filter -ne "Name = 'cloudflared.exe'") {
                throw 'Emergency discovery fixture received an unexpected process query.'
            }
            return @()
        }
        $savedProgramData = $env:ProgramData
        try {
            $env:ProgramData = $fixtureProgramData
            $output = @(& (Join-Path $scriptsRoot 'Stop-Tunnel.ps1'))
        } finally {
            $env:ProgramData = $savedProgramData
        }
        if (Test-Path -LiteralPath (Join-Path $fixtureState 'tunnel-state.json')) { throw 'Corrupt state was not safely converged.' }
        if (($output -join "`n") -notmatch 'exact emergency discovery') { throw 'Corrupt state did not use the emergency discovery path.' }
    }

    Invoke-Test 'uninstall emergency stops are independent and aggregated' {
        $stopScripts = Join-Path $temporaryRoot 'stop-fixture'
        $marker = Join-Path $temporaryRoot 'tunnel-stop-called.txt'
        $funnelMarker = Join-Path $temporaryRoot 'funnel-stop-called.txt'
        $ngrokMarker = Join-Path $temporaryRoot 'ngrok-stop-called.txt'
        New-Item -ItemType Directory -Path $stopScripts | Out-Null
        [IO.File]::WriteAllText((Join-Path $stopScripts 'Stop-Gateway.ps1'), "param([string]`$InstallRoot)`nthrow 'gateway fixture failure'", [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $stopScripts 'Stop-Tunnel.ps1'), "param([string]`$InstallRoot)`n[IO.File]::WriteAllText('$($marker.Replace("'", "''"))','called')", [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $stopScripts 'Stop-Funnel.ps1'), "param([string]`$InstallRoot)`n[IO.File]::WriteAllText('$($funnelMarker.Replace("'", "''"))','called')", [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $stopScripts 'Stop-Ngrok.ps1'), "param([string]`$InstallRoot)`n[IO.File]::WriteAllText('$($ngrokMarker.Replace("'", "''"))','called')", [Text.UTF8Encoding]::new($false))
        $failures = @(Invoke-GatewayEmergencyStops -ScriptsRoot $stopScripts -InstallRoot 'fixture')
        if (-not (Test-Path -LiteralPath $marker)) { throw 'Tunnel stop was skipped after gateway stop failed.' }
        if (-not (Test-Path -LiteralPath $funnelMarker)) { throw 'Funnel stop was skipped after gateway stop failed.' }
        if (-not (Test-Path -LiteralPath $ngrokMarker)) { throw 'Ngrok stop was skipped after gateway stop failed.' }
        Assert-Equal $failures.Count 1 'aggregated stop failure count'
        Assert-Equal $failures[0] 'gateway' 'aggregated stop component'
    }

    Invoke-Test 'corrupt manifest is independent from emergency stop ordering' {
        $content = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Uninstall.ps1')
        $disableIndex = $content.IndexOf('Disable-ScheduledTask', [StringComparison]::Ordinal)
        $stopIndex = $content.IndexOf('Invoke-GatewayEmergencyStops', [StringComparison]::Ordinal)
        $stopFailureIndex = $content.IndexOf('Gateway emergency stop failed', [StringComparison]::Ordinal)
        $manifestIndex = $content.IndexOf('Test-InstallationManifest', [StringComparison]::Ordinal)
        if ($disableIndex -lt 0 -or $stopIndex -lt 0 -or $stopFailureIndex -lt 0 -or $manifestIndex -lt 0 -or
            $disableIndex -gt $stopIndex -or $stopIndex -gt $stopFailureIndex -or $stopFailureIndex -gt $manifestIndex) {
            throw 'Manifest gate can still block emergency process stop.'
        }
    }

    Invoke-Test 'uninstall stops tunnel before deleting files' {
        $content = Get-Content -Raw -LiteralPath (Join-Path $scriptsRoot 'Uninstall.ps1')
        $manifestIndex = $content.IndexOf('Test-InstallationManifest', [StringComparison]::Ordinal)
        $stopIndex = $content.IndexOf('Invoke-GatewayEmergencyStops', [StringComparison]::Ordinal)
        $removeIndex = $content.IndexOf('Remove-Item', $manifestIndex, [StringComparison]::Ordinal)
        if ($manifestIndex -lt 0 -or $stopIndex -lt 0 -or $removeIndex -lt 0 -or
            $stopIndex -gt $manifestIndex -or $manifestIndex -gt $removeIndex) {
            throw 'Uninstall ordering is unsafe.'
        }
    }

    Invoke-Test 'startup transport transitions remove the conflicting task' {
        $fixtureRoot = Join-Path $temporaryRoot 'startup-transition'
        $fixtureScripts = Join-Path $fixtureRoot 'scripts'
        New-Item -ItemType Directory -Path $fixtureScripts | Out-Null
        Copy-Item -LiteralPath (Join-Path $scriptsRoot 'Register-Startup.ps1') -Destination $fixtureScripts
        [IO.File]::WriteAllText((Join-Path $fixtureScripts 'Start-Gateway.ps1'), '', [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $fixtureScripts 'Test-TunnelConfiguration.ps1'),
            "param([string]`$ConfigPath)`n[pscustomobject]@{ConfigPath=`$ConfigPath}", [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $fixtureScripts 'Test-NgrokConfiguration.ps1'),
            "param([string]`$InstallRoot)`n[pscustomobject]@{Mode='active'}", [Text.UTF8Encoding]::new($false))
        $escapedRoot = $fixtureRoot.Replace("'", "''")
        $escapedScripts = $fixtureScripts.Replace("'", "''")
        [IO.File]::WriteAllText((Join-Path $fixtureScripts 'Common.ps1'), @"
function Assert-Administrator {}
function Get-GatewayPaths { param([string]`$InstallRoot) return @{Root='$escapedRoot';Scripts='$escapedScripts'} }
function Get-TrustedSystemPowerShell { return 'C:\Program Files\PowerShell\7\pwsh.exe' }
function Enter-PublicTransportLock { return [pscustomobject]@{Acquired=`$true} }
function Exit-PublicTransportLock { param([object]`$Lock) }
"@, [Text.UTF8Encoding]::new($false))

        $scheduled = @{
            'Aevrynd-Line-Gateway-Tunnel' = [pscustomobject]@{Kind='old-tunnel'}
            'Aevrynd-Line-Gateway-Ngrok' = [pscustomobject]@{Kind='old-ngrok'}
        }
        function Get-ScheduledTask { param([string]$TaskName,[object]$ErrorAction) return $scheduled[$TaskName] }
        function Unregister-ScheduledTask { param([string]$TaskName,[switch]$Confirm) [void]$scheduled.Remove($TaskName) }
        function Register-ScheduledTask { param([string]$TaskName,[object]$InputObject,[switch]$Force) $scheduled[$TaskName]=$InputObject }
        function New-ScheduledTaskTrigger { param([switch]$AtLogOn,[string]$User) return [pscustomobject]@{Kind='trigger'} }
        function New-ScheduledTaskPrincipal { param([string]$UserId,[string]$LogonType,[string]$RunLevel) return [pscustomobject]@{Kind='principal'} }
        function New-ScheduledTaskSettingsSet { param([string]$MultipleInstances,[int]$RestartCount,[TimeSpan]$RestartInterval,[TimeSpan]$ExecutionTimeLimit,[switch]$StartWhenAvailable,[switch]$AllowStartIfOnBatteries,[switch]$DontStopIfGoingOnBatteries) return [pscustomobject]@{Kind='settings'} }
        function New-ScheduledTaskAction { param([string]$Execute,[string]$Argument,[string]$WorkingDirectory) return [pscustomobject]@{Kind='action';Argument=$Argument} }
        function New-ScheduledTask { param([object]$Action,[object]$Trigger,[object]$Principal,[object]$Settings) return [pscustomobject]@{Kind='task';Action=$Action} }

        & (Join-Path $fixtureScripts 'Register-Startup.ps1') -InstallRoot $fixtureRoot -PublicTransport Cloudflare -CloudflaredConfigPath (Join-Path $fixtureRoot 'cloudflared\config.yml') | Out-Null
        if ($scheduled.ContainsKey('Aevrynd-Line-Gateway-Ngrok') -or -not $scheduled.ContainsKey('Aevrynd-Line-Gateway-Tunnel')) {
            throw 'Cloudflare transition retained the conflicting ngrok startup task.'
        }
        & (Join-Path $fixtureScripts 'Register-Startup.ps1') -InstallRoot $fixtureRoot -PublicTransport Ngrok | Out-Null
        if ($scheduled.ContainsKey('Aevrynd-Line-Gateway-Tunnel') -or -not $scheduled.ContainsKey('Aevrynd-Line-Gateway-Ngrok')) {
            throw 'Ngrok transition retained the conflicting Cloudflare startup task.'
        }
        if (-not $scheduled.ContainsKey('Aevrynd-Line-Gateway')) { throw 'Transport transition omitted the gateway startup task.' }
    }
} finally {
    $tempPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $candidate = [IO.Path]::GetFullPath($temporaryRoot)
    if (-not $candidate.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing unsafe fixture cleanup.'
    }
    if (Test-Path -LiteralPath $candidate) { Remove-Item -LiteralPath $candidate -Recurse -Force }
}

Invoke-Test 'gateway timestamp writes canonical UTC normalizes exact legacy and rejects variants' {
    $current=ConvertTo-GatewayProcessTimestamp (Get-Process -Id $PID).StartTime
    if($current-notmatch'\+00:00$'){throw 'Current process timestamp is not canonical UTC.'}
    $legacy=[DateTime]::UtcNow.ToString('o',[Globalization.CultureInfo]::InvariantCulture)
    $normalized=(ConvertFrom-GatewayStateTimestamp $legacy).ToString('o',[Globalization.CultureInfo]::InvariantCulture)
    if($normalized-notmatch'\+00:00$'){throw 'Exact legacy Z timestamp was not normalized.'}
    foreach($bad in @($legacy.ToLowerInvariant(),$legacy.Replace('Z','+0000'),'2026-01-01T00:00:00Z')){Assert-Throws {ConvertFrom-GatewayStateTimestamp $bad} 'hostile gateway timestamp'}
}

Invoke-Test 'gateway state preserves and normalizes fixed legacy timestamp text' {
    $fixtureRoot=Join-Path ([IO.Path]::GetTempPath()) "aevrynd-gateway-state-$([Guid]::NewGuid().ToString('N'))";New-Item -ItemType Directory $fixtureRoot|Out-Null;$fixture=@{StateFile=(Join-Path $fixtureRoot 'gateway-state.json')}
    try{[IO.File]::WriteAllText($fixture.StateFile,'{"schemaVersion":1,"pid":154244,"startedUtc":"2026-09-02T18:04:49.6200313Z","bindHost":"127.0.0.1","port":8787}');$state=Read-GatewayState $fixture;if($state.startedUtc-cne'2026-09-02T18:04:49.6200313+00:00'){throw "Fixed legacy timestamp normalized incorrectly: $($state.startedUtc)"}}finally{Remove-Item $fixtureRoot -Recurse -Force}
}

Invoke-Test 'startup rejects PATH shadowing and selects only signed system PowerShell' {
    $register=Get-Content -Raw (Join-Path $scriptsRoot 'Register-Startup.ps1');$common=Get-Content -Raw (Join-Path $scriptsRoot 'Common.ps1')
    if($register-match'Get-Command\s+pwsh' -or$register-match'\.Source'){throw 'Startup still trusts PATH command discovery.'}
    foreach($required in @('Get-TrustedSystemPowerShell','SpecialFolder]::ProgramFiles','HKLM:\SOFTWARE\Microsoft\PowerShellCore\InstalledVersions','StartsWith($programFiles+','ReparsePoint','FileVersionRaw.Major-lt7','Get-AuthenticodeSignature','O=Microsoft Corporation')){if(-not($register.Contains($required)-or$common.Contains($required))){throw "Trusted PowerShell discovery lacks $required"}}
    if($common.Contains('.cache\codex-runtimes')-or$common.Contains('WindowsApps\pwsh.exe')){throw 'User-scoped PowerShell path entered the trusted candidates.'}
}

Write-Output "Deployment tests: passed=$script:passed failed=$script:failed"
if ($script:failed -gt 0) { exit 1 }
