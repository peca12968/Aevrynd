param(
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'
$projectPath = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
$configPath = Join-Path $projectPath 'engine\ai\config\knowledge-sources.json'
$knowledgePath = Join-Path $projectPath 'engine\ai\knowledge'
$chunkPath = Join-Path $knowledgePath 'chunks'
$jsonlPath = Join-Path $knowledgePath 'knowledge.jsonl'
$manifestPath = Join-Path $knowledgePath 'manifest.json'
$utf8 = [Text.UTF8Encoding]::new($false)
$config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
$snapshotRevision = (& git -C $projectPath rev-parse HEAD).Trim()
if ($snapshotRevision -notmatch '^[0-9a-f]{40}$') { throw 'A valid Git HEAD snapshot is required.' }
if ([bool]$config.trainingEligible) { throw 'Knowledge cannot become training-eligible without the documented review gates.' }
$trackedPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($trackedPath in (& git -C $projectPath ls-files)) { $null=$trackedPaths.Add(([string]$trackedPath).Replace('\','/')) }

function Get-Sha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-TextSha256([string]$Text) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
}

function Get-EngineVersion {
    $versionPath = ([string]$config.engineVersionFile).Replace('\','/')
    $cmake = ((& git -C $projectPath show ($snapshotRevision + ':' + $versionPath)) -join "`n") + "`n"
    if ($LASTEXITCODE -ne 0) { throw 'The engine version file could not be read from the Git snapshot.' }
    $match = [regex]::Match($cmake, 'project\s*\(\s*Aevrynd\s+VERSION\s+([^\s\)]+)', 'IgnoreCase')
    if (-not $match.Success) { throw 'The engine version could not be read from CMakeLists.txt.' }
    $match.Groups[1].Value
}

function Get-SourceRevision([string]$Path) {
    if (-not $Path.StartsWith($projectPath + '\', [StringComparison]::OrdinalIgnoreCase)) { return 'external-info' }
    $relative = $Path.Substring($projectPath.Length + 1).Replace('\','/')
    $revision = (& git -C $projectPath log -1 --format=%H $snapshotRevision -- $relative 2>$null | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($revision)) { return 'working-tree' }
    $revision.Trim()
}

function Test-ReparsePath([string]$Path,[string]$Boundary) {
    $cursor = Get-Item -LiteralPath $Path -Force
    $boundaryPath = [IO.Path]::GetFullPath($Boundary).TrimEnd('\')
    while ($null -ne $cursor) {
        if (($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { return $true }
        if ($cursor.FullName.TrimEnd('\').Equals($boundaryPath,[StringComparison]::OrdinalIgnoreCase)) { break }
        $cursor = $cursor.Parent
    }
    return $false
}

function Get-Symbol([string]$Text,[string]$Fallback) {
    $patterns = @(
        '\b(?:class|struct|enum\s+class)\s+([A-Za-z_][A-Za-z0-9_]*)',
        '\b([A-Za-z_][A-Za-z0-9_]*(?:3D|2D|25D))\b',
        '^#\s+(.+)$'
    )
    foreach ($pattern in $patterns) {
        $match = [regex]::Match($Text, $pattern, 'Multiline')
        if ($match.Success) { return $match.Groups[1].Value.Trim() }
    }
    $Fallback
}

function Test-Excluded([string]$Path) {
    $normalized = ('/' + $Path.Replace('\','/').TrimStart('/')).ToLowerInvariant()
    foreach ($fragment in $config.excludedPathFragments) {
        if ($normalized.Contains(([string]$fragment).ToLowerInvariant())) { return $true }
    }
    return $false
}

New-Item -ItemType Directory -Path $chunkPath -Force | Out-Null
# 這個目錄只包含本腳本產生的可重建區塊，重建前移除舊區塊可避免殘留資料。
Get-ChildItem -LiteralPath $chunkPath -File -Filter '*.json' -ErrorAction SilentlyContinue | Remove-Item -Force

$engineVersion = Get-EngineVersion
$records = [Collections.Generic.List[object]]::new()
$sourceRecords = [Collections.Generic.List[object]]::new()
$paths = [Collections.Generic.List[object]]::new()
foreach ($source in $config.sources) {
    $root = [IO.Path]::GetFullPath((Join-Path $projectPath ([string]$source.root)))
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
    if ((Test-ReparsePath -Path $root -Boundary $root)) { throw "A knowledge source root cannot be a reparse point: $root" }
    foreach ($pattern in $source.patterns) {
        foreach ($file in @(Get-ChildItem -LiteralPath $root -File -Recurse -Filter ([string]$pattern) | Sort-Object FullName)) {
            if ((Test-ReparsePath -Path $file.FullName -Boundary $root)) { throw "A knowledge source cannot cross a reparse point: $($file.FullName)" }
            $isInternal = $file.FullName.StartsWith($projectPath + '\',[StringComparison]::OrdinalIgnoreCase)
            $candidateRelative = if ($isInternal) { $file.FullName.Substring($projectPath.Length + 1).Replace('\','/') } else { '' }
            # Repo 來源固定讀取 HEAD，避免把其他工作線尚未完成的 working-tree 內容混入知識包。
            if (-not (Test-Excluded $file.FullName) -and (-not $isInternal -or $trackedPaths.Contains($candidateRelative))) {
                $paths.Add([pscustomobject]@{ kind=[string]$source.kind; file=$file })
            }
        }
    }
}

$sequence = 0
foreach ($item in ($paths | Sort-Object @{Expression={$_.file.FullName}},kind -Unique)) {
    $file = $item.file
    $relative = if ($file.FullName.StartsWith($projectPath + '\',[StringComparison]::OrdinalIgnoreCase)) {
        $file.FullName.Substring($projectPath.Length + 1).Replace('\','/')
    } else {
        '../Info/' + $file.Name
    }
    if ($file.FullName.StartsWith($projectPath + '\',[StringComparison]::OrdinalIgnoreCase)) {
        $content = ((& git -C $projectPath show ($snapshotRevision + ':' + $relative)) -join "`n") + "`n"
        if ($LASTEXITCODE -ne 0) { throw "A tracked knowledge source could not be read from HEAD: $relative" }
        $sourceSha = Get-TextSha256 $content
        $sourceBytes = [Text.Encoding]::UTF8.GetByteCount($content)
    } else {
        $content = [IO.File]::ReadAllText($file.FullName)
        $sourceSha = Get-Sha256 $file.FullName
        $sourceBytes = $file.Length
    }
    foreach ($sensitivePattern in $config.sensitivePatterns) {
        if ([regex]::IsMatch($content,[string]$sensitivePattern,'IgnoreCase')) { throw "A knowledge source failed the sensitive-content gate: $relative" }
    }
    $revision = Get-SourceRevision $file.FullName
    $sourceRecords.Add([ordered]@{path=$relative;kind=$item.kind;sha256=$sourceSha;revision=$revision;bytes=$sourceBytes})
    for ($offset = 0; $offset -lt $content.Length; $offset += [int]$config.chunkCharacters) {
        $length = [Math]::Min([int]$config.chunkCharacters, $content.Length - $offset)
        $excerpt = $content.Substring($offset, $length)
        if ([string]::IsNullOrWhiteSpace($excerpt)) { continue }
        $sequence++
        $id = 'aec-' + $sequence.ToString('D5')
        $record = [ordered]@{
            schemaVersion = 1
            id = $id
            kind = [string]$item.kind
            sourcePath = $relative
            sourceSha256 = $sourceSha
            sourceRevision = $revision
            license = [string]$config.defaultLicense
            trainingEligible = [bool]$config.trainingEligible
            engineVersion = $engineVersion
            offset = $offset
            symbol = Get-Symbol $excerpt ([IO.Path]::GetFileNameWithoutExtension($file.Name))
            content = $excerpt
        }
        $records.Add($record)
        [IO.File]::WriteAllText((Join-Path $chunkPath ($id + '.json')), ($record | ConvertTo-Json -Depth 5 -Compress), $utf8)
    }
}

# 中文提交訊息是引擎決策的補充知識來源，保留 commit SHA 以便追溯。
$logLines = & git -C $projectPath log -100 --pretty=format:%H%x09%s
foreach ($line in $logLines) {
    if ($line -notmatch '^([0-9a-f]{40})\t(.+)$') { continue }
    $commitRevision = $Matches[1]
    $commitMessage = $Matches[2]
    if (-not [regex]::IsMatch($commitMessage,'[\p{IsCJKUnifiedIdeographs}]')) { continue }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $commitSha256 = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($commitMessage)))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
    $sequence++
    $record = [ordered]@{
        schemaVersion=1; id=('aec-' + $sequence.ToString('D5')); kind='git-commit'; sourcePath='git/history'
        sourceSha256=$commitSha256; sourceRevision=$commitRevision; license=[string]$config.defaultLicense
        trainingEligible=[bool]$config.trainingEligible; engineVersion=$engineVersion; offset=0
        symbol='Commit'; content=$commitMessage
    }
    $records.Add($record)
    [IO.File]::WriteAllText((Join-Path $chunkPath ($record.id + '.json')), ($record | ConvertTo-Json -Depth 5 -Compress), $utf8)
}

$lines = $records | ForEach-Object { $_ | ConvertTo-Json -Depth 5 -Compress }
[IO.File]::WriteAllLines($jsonlPath, [string[]]$lines, $utf8)
$manifest = [ordered]@{
    schemaVersion=1
    engineVersion=$engineVersion
    snapshotRevision=$snapshotRevision
    chunkCharacters=[int]$config.chunkCharacters
    chunkCount=$records.Count
    sourceCount=$sourceRecords.Count
    defaultLicense=[string]$config.defaultLicense
    trainingEligible=[bool]$config.trainingEligible
    sensitiveContentScan='passed'
    knowledgeSha256=Get-Sha256 $jsonlPath
    sources=$sourceRecords
    exclusions=$config.excludedPathFragments
}
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 8), $utf8)
[ordered]@{ok=$true;engineVersion=$engineVersion;sources=$sourceRecords.Count;chunks=$records.Count;knowledgeSha256=$manifest.knowledgeSha256}|ConvertTo-Json -Compress
