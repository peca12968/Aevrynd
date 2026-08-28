param(
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$DemoRoot = '',
    [switch]$SkipValidation,
    [ValidateSet('','BeforeRename','BeforeCurrentCommit')] [string]$FailPoint = ''
)
$ErrorActionPreference = 'Stop'
if (-not ('Aevrynd.ReleaseNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Aevrynd { public static class ReleaseNative { [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern bool MoveFileEx(string existingName,string newName,uint flags); } }
'@
}
function Test-ReleaseContent([string]$Root,[string]$ManifestPath) {
    foreach($line in [IO.File]::ReadAllLines($ManifestPath)) { if($line -match '^file=([^ ]+) size=([0-9]+) sha256=([0-9a-f]{64})$') { $relative=$Matches[1];if($relative.Contains('..') -or [IO.Path]::IsPathRooted($relative)){return $false};$file=Join-Path $Root $relative;if(-not(Test-Path -LiteralPath $file -PathType Leaf)){return $false};$item=Get-Item -LiteralPath $file;if($item.Length -ne [Int64]$Matches[2]){return $false};if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Matches[3]){return $false} } };return $true
}
if (-not $DemoRoot) { $DemoRoot = Join-Path $ProjectRoot 'Demo' }
$DemoRoot = [IO.Path]::GetFullPath($DemoRoot)
$manifest = Get-Content -LiteralPath (Join-Path $ProjectRoot 'games\character_lab\game.json') -Raw | ConvertFrom-Json
$gitId = (& git -C $ProjectRoot rev-parse --short=8 HEAD).Trim()
$releaseId = "v$($manifest.version)-engine-v$($manifest.engineVersion)-$gitId"
if ($releaseId -notmatch '^[A-Za-z0-9._+-]{1,96}$') { throw 'Invalid release id.' }
$releaseBase = Join-Path $DemoRoot 'releases\character-lab'
$stagingBase = Join-Path $releaseBase '.staging'
$staging = Join-Path $stagingBase ($releaseId + '-' + [Guid]::NewGuid().ToString('N'))
$release = Join-Path $releaseBase $releaseId
$currentDirectory = Join-Path $DemoRoot 'current'
$pending = Join-Path $currentDirectory 'character-lab.pending'
$lockPath = Join-Path $DemoRoot '.character-lab-publish.lock'
New-Item -ItemType Directory -Force -Path $DemoRoot,$releaseBase,$stagingBase,$currentDirectory | Out-Null
$lock = [IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
    # 取得排他鎖後不可能仍有合法 writer；只清除此工具命名且位於 canonical staging root 的殘留。
    $canonicalStaging = [IO.Path]::GetFullPath($stagingBase).TrimEnd('\') + '\'
    foreach ($orphan in Get-ChildItem -LiteralPath $stagingBase -Directory -ErrorAction SilentlyContinue) {
        $canonicalOrphan = [IO.Path]::GetFullPath($orphan.FullName)
        if ($canonicalOrphan.StartsWith($canonicalStaging,[StringComparison]::OrdinalIgnoreCase) -and $orphan.Name -match '^[A-Za-z0-9._+-]{1,96}-[0-9a-f]{32}$') { Remove-Item -LiteralPath $canonicalOrphan -Recurse -Force }
    }
    New-Item -ItemType Directory -Path $staging | Out-Null
    $bin = Join-Path $ProjectRoot 'engine\build\bin\Release'
    Copy-Item -LiteralPath (Join-Path $bin 'Aevrynd-Character-Lab.exe') -Destination (Join-Path $staging 'Aevrynd-Character-Lab.runtime.exe')
    Get-ChildItem -LiteralPath $bin -File -Filter '*.dll' | Copy-Item -Destination $staging
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'games\character_lab\assets') -Destination (Join-Path $staging 'assets') -Recurse
    foreach ($required in @('assets\model-packs\realistic-female\realistic-female.aeskin','assets\model-packs\realistic-male\realistic-male.aeskin','assets\material-packs\original\female.materials.json','assets\material-packs\original\male.materials.json')) { if (-not (Test-Path -LiteralPath (Join-Path $staging $required))) { throw "Missing release asset: $required" } }
    $entries = Get-ChildItem -LiteralPath $staging -File -Recurse | Sort-Object { $_.FullName.Substring($staging.Length + 1) }
    $manifestPath = Join-Path $staging 'release.manifest'
    $lines = @('schema=1',"releaseId=$releaseId","gameVersion=$($manifest.version)","engineVersion=$($manifest.engineVersion)")
    foreach ($file in $entries) { $relative = $file.FullName.Substring($staging.Length + 1).Replace('\','/'); $lines += "file=$relative size=$($file.Length) sha256=$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())" }
    [IO.File]::WriteAllLines($manifestPath,$lines,[Text.UTF8Encoding]::new($false))
    $manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if (-not $SkipValidation) {
        $validationState = Join-Path $stagingBase ('validation-' + [Guid]::NewGuid().ToString('N'))
        $oldReleaseId = $env:AEVRYND_CHARACTER_RELEASE_ID;$oldStateRoot = $env:AEVRYND_CHARACTER_STATE_ROOT
        try {
            $env:AEVRYND_CHARACTER_RELEASE_ID = $releaseId;$env:AEVRYND_CHARACTER_STATE_ROOT = $validationState
            $validation = Start-Process -FilePath (Join-Path $staging 'Aevrynd-Character-Lab.runtime.exe') -ArgumentList '--capture-demo' -WorkingDirectory $staging -PassThru -Wait
            if ($validation.ExitCode -ne 0) { throw "Release validation failed with exit $($validation.ExitCode)." }
        } finally { $env:AEVRYND_CHARACTER_RELEASE_ID=$oldReleaseId;$env:AEVRYND_CHARACTER_STATE_ROOT=$oldStateRoot;if(Test-Path -LiteralPath $validationState){Remove-Item -LiteralPath $validationState -Recurse -Force} }
    }
    if ($FailPoint -eq 'BeforeRename') { throw 'Injected failure before immutable release rename.' }
    if (Test-Path -LiteralPath $release) {
        $existingHash = (Get-FileHash -LiteralPath (Join-Path $release 'release.manifest') -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($existingHash -ne $manifestHash -or -not (Test-ReleaseContent $release (Join-Path $release 'release.manifest'))) { throw 'Release id already exists with different or damaged content.' }
        Remove-Item -LiteralPath $staging -Recurse -Force
    } else { Move-Item -LiteralPath $staging -Destination $release }
    $launcher = Join-Path $bin 'Aevrynd-Character-Lab-Launcher.exe';$latest=Join-Path $DemoRoot 'Aevrynd-Character-Lab.exe';$latestPending=Join-Path $DemoRoot 'Aevrynd-Character-Lab.pending'
    $launcherHash=(Get-FileHash -LiteralPath $launcher -Algorithm SHA256).Hash
    if (-not (Test-Path -LiteralPath $latest)) {
        Copy-Item -LiteralPath $launcher -Destination $latestPending -Force;if(-not [Aevrynd.ReleaseNative]::MoveFileEx($latestPending,$latest,0x8)){throw 'Atomic latest launcher creation failed.'}
    } elseif ((Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash -ne $launcherHash) {
        # 一次性遷移舊runtime；只接受本次build或已發布dist中可驗證的既有二進位。
        $legacyHash=(Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash
        $approvedLegacy=@((Get-FileHash -LiteralPath (Join-Path $bin 'Aevrynd-Character-Lab.exe') -Algorithm SHA256).Hash)
        Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'games\character_lab\dist') -File -Filter '*.exe' -ErrorAction SilentlyContinue | ForEach-Object { $approvedLegacy += (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        if ($legacyHash -notin $approvedLegacy) { throw 'Existing Character Lab latest executable is neither the approved launcher nor a known legacy runtime.' }
        $legacyBackup=Join-Path $DemoRoot ('Aevrynd-Character-Lab-Legacy-'+$legacyHash.Substring(0,8).ToLowerInvariant()+'.exe')
        if (Test-Path -LiteralPath $legacyBackup) { if((Get-FileHash -LiteralPath $legacyBackup -Algorithm SHA256).Hash -ne $legacyHash){throw 'Legacy backup name collision.'} } else { Copy-Item -LiteralPath $latest -Destination $legacyBackup;if((Get-FileHash -LiteralPath $legacyBackup -Algorithm SHA256).Hash -ne $legacyHash){throw 'Legacy backup verification failed.'} }
        Copy-Item -LiteralPath $launcher -Destination $latestPending -Force;if(-not [Aevrynd.ReleaseNative]::MoveFileEx($latestPending,$latest,0x9)){throw 'Atomic legacy launcher migration failed.'}
    }
    $pinned = Join-Path $DemoRoot "Aevrynd-Character-Lab-$releaseId.exe"
    if (Test-Path -LiteralPath $pinned) { if((Get-FileHash $pinned).Hash -ne (Get-FileHash $launcher).Hash){throw 'Pinned launcher exists with different content.'} } else { $pinnedPending=$pinned+'.pending';Copy-Item -LiteralPath $launcher -Destination $pinnedPending;if(-not [Aevrynd.ReleaseNative]::MoveFileEx($pinnedPending,$pinned,0x8)){throw 'Atomic pinned launcher publish failed.'} }
    $current = Join-Path $currentDirectory 'character-lab.current'
    [IO.File]::WriteAllLines($pending,@('schema=1',"releaseId=$releaseId","manifestSha256=$manifestHash"),[Text.UTF8Encoding]::new($false))
    if ($FailPoint -eq 'BeforeCurrentCommit') { throw 'Injected failure before current pointer commit.' }
    if (-not [Aevrynd.ReleaseNative]::MoveFileEx($pending,$current,0x9)) { throw "Atomic current pointer commit failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
    Write-Host "Published immutable Character Lab release $releaseId" -ForegroundColor Green
} finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
    if (Test-Path -LiteralPath $pending) { Remove-Item -LiteralPath $pending -Force }
    $lock.Dispose()
}
