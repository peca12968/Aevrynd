param(
    [Parameter(Mandatory)] [ValidateLength(3,1024)] [string]$Question,
    [ValidateRange(1,3)] [int]$TopK = 2,
    [switch]$SkipModel,
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'
$projectPath = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
$knowledgePath = Join-Path $projectPath 'engine\ai\knowledge\knowledge.jsonl'
$manifestPath = Join-Path $projectPath 'engine\ai\knowledge\manifest.json'
$wrapperPath = Join-Path $projectPath 'tools\ci\invoke-local-qwen.ps1'

function Write-SafeFallback([string]$Code,[string]$Answer) {
    [ordered]@{
        ok=$false
        fallbackToCodex=$true
        answer=$Answer
        citations=@()
        resource=@{launched=$false}
        workerError=@{code=$Code;message=$Answer}
    } | ConvertTo-Json -Depth 8
    exit 0
}

function Get-Sha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Test-ExactProperties([object]$Value,[string[]]$Expected) {
    $actual = @($Value.PSObject.Properties.Name | Sort-Object)
    $wanted = @($Expected | Sort-Object)
    return $actual.Count -eq $wanted.Count -and (($actual -join "`n") -ceq ($wanted -join "`n"))
}

function Test-SafeSourcePath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or $Path.Contains('\') -or $Path.Contains(':')) { return $false }
    if ($Path.StartsWith('../Info/',[StringComparison]::Ordinal)) {
        return $Path.Substring(8) -notmatch '[/\\]' -and $Path.EndsWith('.md',[StringComparison]::OrdinalIgnoreCase)
    }
    return -not $Path.StartsWith('/') -and -not ($Path.Split('/') -contains '..')
}

if (-not (Test-Path -LiteralPath $knowledgePath -PathType Leaf) -or -not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    Write-SafeFallback 'KnowledgePackMissing' 'The knowledge pack is missing; send the question to Codex.'
}

# 查詢前驗證完整知識包，避免部分損毀、竄改區塊或不安全路徑成為模型證據。
$validatedRecords = [Collections.Generic.List[object]]::new()
try {
    $chunkDirectory = Join-Path $projectPath 'engine\ai\knowledge\chunks'
    foreach ($requiredFile in @($knowledgePath,$manifestPath,$chunkDirectory)) {
        $item = Get-Item -LiteralPath $requiredFile -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'KnowledgeReparsePoint' }
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $manifestProperties = @('schemaVersion','engineVersion','snapshotRevision','chunkCharacters','chunkCount','sourceCount','defaultLicense','trainingEligible','sensitiveContentScan','knowledgeSha256','sources','exclusions')
    if (-not (Test-ExactProperties $manifest $manifestProperties)) { throw 'ManifestSchema' }
    if ($manifest.schemaVersion -ne 1 -or $manifest.engineVersion -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$' -or
        $manifest.snapshotRevision -notmatch '^[0-9a-f]{40}$' -or $manifest.chunkCharacters -lt 256 -or $manifest.chunkCharacters -gt 4096 -or
        $manifest.chunkCount -lt 1 -or $manifest.sourceCount -lt 1 -or [bool]$manifest.trainingEligible -or
        $manifest.sensitiveContentScan -cne 'passed' -or $manifest.knowledgeSha256 -notmatch '^[0-9a-f]{64}$' -or
        (Get-Sha256 $knowledgePath) -cne [string]$manifest.knowledgeSha256) { throw 'ManifestIntegrity' }
    if (@($manifest.sources).Count -ne [int]$manifest.sourceCount) { throw 'ManifestSourceCount' }
    $requiredExclusions = @('/build/','/build-','/bin/','/binary/','/binaries/','/obj/','/out/','/generated/','/private/','/secrets/')
    foreach ($required in $requiredExclusions) { if ($required -notin @($manifest.exclusions)) { throw 'ManifestExclusions' } }
    $sourceMap = @{}
    foreach ($source in @($manifest.sources)) {
        if (-not (Test-ExactProperties $source @('path','kind','sha256','revision','bytes')) -or
            -not (Test-SafeSourcePath ([string]$source.path)) -or $source.kind -notin @('info','public-header','test','tool','build') -or
            $source.sha256 -notmatch '^[0-9a-f]{64}$' -or [string]::IsNullOrWhiteSpace([string]$source.revision) -or
            [int64]$source.bytes -lt 1 -or $sourceMap.ContainsKey([string]$source.path)) { throw 'ManifestSourceSchema' }
        foreach ($excluded in @($manifest.exclusions)) {
            if (('/' + ([string]$source.path).TrimStart('/')).ToLowerInvariant().Contains(([string]$excluded).ToLowerInvariant())) { throw 'ExcludedSource' }
        }
        $sourceMap[[string]$source.path] = $source
    }
    $recordProperties = @('schemaVersion','id','kind','sourcePath','sourceSha256','sourceRevision','license','trainingEligible','engineVersion','offset','symbol','content')
    $seenIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($line in [IO.File]::ReadAllLines($knowledgePath)) {
        if ([string]::IsNullOrWhiteSpace($line)) { throw 'BlankKnowledgeLine' }
        $record = $line | ConvertFrom-Json
        if (-not (Test-ExactProperties $record $recordProperties) -or $record.schemaVersion -ne 1 -or
            $record.id -notmatch '^aec-[0-9]{5}$' -or -not $seenIds.Add([string]$record.id) -or
            $record.kind -notin @('info','public-header','test','tool','build','git-commit') -or
            -not (Test-SafeSourcePath ([string]$record.sourcePath)) -or $record.sourceSha256 -notmatch '^[0-9a-f]{64}$' -or
            [string]::IsNullOrWhiteSpace([string]$record.sourceRevision) -or [string]::IsNullOrWhiteSpace([string]$record.symbol) -or
            [string]::IsNullOrWhiteSpace([string]$record.content) -or [int64]$record.offset -lt 0 -or
            [bool]$record.trainingEligible -or $record.engineVersion -cne $manifest.engineVersion -or
            $record.license -cne $manifest.defaultLicense) { throw 'KnowledgeRecordSchema' }
        if ($record.kind -ne 'git-commit') {
            if (-not $sourceMap.ContainsKey([string]$record.sourcePath)) { throw 'KnowledgeSourceMissing' }
            $source = $sourceMap[[string]$record.sourcePath]
            if ($record.sourceSha256 -cne $source.sha256 -or $record.sourceRevision -cne $source.revision -or $record.kind -cne $source.kind) { throw 'KnowledgeProvenance' }
        } elseif ($record.sourcePath -cne 'git/history' -or $record.sourceRevision -notmatch '^[0-9a-f]{40}$') { throw 'CommitProvenance' }
        $chunkFile = Join-Path $projectPath ('engine\ai\knowledge\chunks\' + $record.id + '.json')
        if (-not (Test-Path -LiteralPath $chunkFile -PathType Leaf) -or [IO.File]::ReadAllText($chunkFile) -cne $line) { throw 'ChunkIntegrity' }
        $validatedRecords.Add($record)
    }
    if ($validatedRecords.Count -ne [int]$manifest.chunkCount) { throw 'KnowledgeChunkCount' }
    $chunkFiles = @(Get-ChildItem -LiteralPath $chunkDirectory -File -Filter '*.json')
    if ($chunkFiles.Count -ne $validatedRecords.Count) { throw 'ChunkFileCount' }
    foreach ($chunkFile in $chunkFiles) {
        if (($chunkFile.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or -not $seenIds.Contains($chunkFile.BaseName)) { throw 'UnexpectedChunkFile' }
    }
} catch {
    Write-SafeFallback 'KnowledgePackIntegrity' 'The knowledge pack failed integrity validation; send the question to Codex.'
}

function Get-Terms([string]$Text) {
    $terms = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($match in [regex]::Matches($Text, '[A-Za-z_][A-Za-z0-9_:.\-]*|[0-9]+(?:\.[0-9]+)+')) {
        if ($match.Value.Length -ge 2) { $null = $terms.Add($match.Value.ToLowerInvariant()) }
    }
    $cjk = -join ([regex]::Matches($Text, '[\p{IsCJKUnifiedIdeographs}]') | ForEach-Object Value)
    for ($index=0; $index -lt $cjk.Length-1; $index++) { $null=$terms.Add($cjk.Substring($index,2)) }
    @($terms)
}

$questionTerms = Get-Terms $Question
$sourceHints = @()
$termAliases = [ordered]@{}
$termAliases.Add(([string][char]0x7248 + [char]0x672C),@('2.86.0','EngineVersion','CMakeLists'))
$termAliases.Add(([string][char]0x8A18 + [char]0x61B6 + [char]0x9AD4),@('Memory','EngineMemoryStats','workingSet'))
$termAliases.Add(([string][char]0x552F + [char]0x8B80),@('read-only','fallback','Copilot'))
foreach ($alias in $termAliases.GetEnumerator()) {
    if ($Question.Contains($alias.Key)) { $questionTerms += $alias.Value }
}
if ($Question -match '(?i)qwen') {
    $questionTerms += @('invoke-local-qwen','fallbackToCodex','JsonSchema','MaximumRamGiB')
    $sourceHints += @('tools/ci/invoke-local-qwen.ps1','tools/ci/test-local-qwen-wrapper.ps1')
}
$questionTerms = @($questionTerms | ForEach-Object { ([string]$_).ToLowerInvariant() } | Select-Object -Unique)
$ranked = foreach ($record in $validatedRecords) {
    $haystack = (($record.symbol + ' ' + $record.sourcePath + ' ' + $record.content).ToLowerInvariant())
    $score = if ($record.sourcePath -in $sourceHints) { 100 } else { 0 }
    foreach ($term in $questionTerms) {
        if ($haystack.Contains($term)) {
            $score += if ($record.symbol -like "*$term*") { 8 } elseif ($record.sourcePath -like "*$term*") { 6 } elseif ($term.Length -ge 5) { 4 } else { 1 }
        }
    }
    if ($score -gt 0) { [pscustomobject]@{score=$score;record=$record} }
}
# 同一大型測試可能有許多高分區塊；先取每個來源最高分，避免單一檔案淹沒公開 API 與文件證據。
$selected = @($ranked | Group-Object {$_.record.sourcePath} | ForEach-Object {
    $_.Group | Sort-Object @{Expression='score';Descending=$true}, @{Expression={$_.record.id};Descending=$false} | Select-Object -First 1
} | Sort-Object @{Expression='score';Descending=$true}, @{Expression={$_.record.id};Descending=$false} | Select-Object -First $TopK)
if ($selected.Count -eq 0) {
    [ordered]@{ok=$false;fallbackToCodex=$true;answer='No grounded evidence was found.';citations=@();resource=@{launched=$false}}|ConvertTo-Json -Depth 8
    exit 0
}

$citations = @($selected | ForEach-Object {
    [ordered]@{chunkId=$_.record.id;sourcePath=$_.record.sourcePath;symbol=$_.record.symbol;sourceSha256=$_.record.sourceSha256;sourceRevision=$_.record.sourceRevision;score=$_.score}
})
if ($SkipModel) {
    [ordered]@{ok=$true;fallbackToCodex=$true;answer='Retrieval-only mode; send the evidence to Codex for synthesis.';citations=$citations;resource=@{launched=$false}}|ConvertTo-Json -Depth 8
    exit 0
}
if (-not (Test-Path -LiteralPath $wrapperPath -PathType Leaf)) { throw 'The bounded local QWEN wrapper is unavailable.' }

$relativeFiles = @($selected | ForEach-Object { 'engine/ai/knowledge/chunks/' + $_.record.id + '.json' })
$schema = '{"type":"object","additionalProperties":false,"properties":{"answer":{"type":"string","minLength":1,"maxLength":1200},"citations":{"type":"array","minItems":1,"maxItems":2,"items":{"type":"string","pattern":"^aec-[0-9]{5}$"}},"confidence":{"type":"number","minimum":0,"maximum":1},"needsCodex":{"type":"boolean"}},"required":["answer","citations","confidence","needsCodex"]}'
$task = "Answer this factual Aevrynd engine documentation question using only the supplied evidence: $Question Cite only supplied chunk ids. Set needsCodex=true if evidence is insufficient. Do not judge code correctness or suggest repository changes."
$routingPolicyPath = Join-Path $projectPath 'engine\ai\config\task-routing-policy.json'
if (-not (Test-Path -LiteralPath $routingPolicyPath -PathType Leaf)) { Write-SafeFallback 'RoutingPolicyMissing' 'The local QWEN routing policy is missing; send the question to Codex.' }
try {
    $routingPolicyItem = Get-Item -LiteralPath $routingPolicyPath -Force
    if (($routingPolicyItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'RoutingPolicyReparsePoint' }
    $routingPolicy = Get-Content -LiteralPath $routingPolicyPath -Raw | ConvertFrom-Json
    if (-not (Test-ExactProperties $routingPolicy @('schemaVersion','policyName','allowedTaskTypes','maxInputBytes','maxTokens','timeoutSeconds','requiresFixedJsonSchema','fallbackToCodex','measuredEvidence')) -or
        $routingPolicy.schemaVersion -ne 1 -or $routingPolicy.policyName -cne 'bounded-local-qwen-routing' -or
        @($routingPolicy.allowedTaskTypes).Count -ne 1 -or $routingPolicy.allowedTaskTypes[0] -cne 'single-feature-classification' -or
        [int]$routingPolicy.maxInputBytes -ne 1024 -or [int]$routingPolicy.maxTokens -ne 96 -or [int]$routingPolicy.timeoutSeconds -ne 30 -or
        -not [bool]$routingPolicy.requiresFixedJsonSchema -or -not [bool]$routingPolicy.fallbackToCodex -or
        $routingPolicy.measuredEvidence.singleFeatureBloom.inputBytes -ne 528 -or $routingPolicy.measuredEvidence.singleFeatureBloom.maxTokens -ne 96 -or
        $routingPolicy.measuredEvidence.singleFeatureBloom.elapsedMilliseconds -ne 19408 -or $routingPolicy.measuredEvidence.singleFeatureBloom.peakWorkingSetGiB -ne 14.94 -or
        $routingPolicy.measuredEvidence.singleFeatureBloom.result -cne 'passed' -or $routingPolicy.measuredEvidence.threeFeatureBatch.inputBytes -ne 3218 -or
        $routingPolicy.measuredEvidence.threeFeatureBatch.maxTokens -ne 512 -or $routingPolicy.measuredEvidence.threeFeatureBatch.timeoutSeconds -ne 30 -or
        $routingPolicy.measuredEvidence.threeFeatureBatch.result -cne 'timed-out') { throw 'RoutingPolicyIntegrity' }
} catch {
    Write-SafeFallback 'RoutingPolicyIntegrity' 'The local QWEN routing policy failed integrity validation; send the question to Codex.'
}
$estimatedInputBytes = [Text.Encoding]::UTF8.GetByteCount($task) + [Text.Encoding]::UTF8.GetByteCount($schema)
foreach ($relativeFile in $relativeFiles) { $estimatedInputBytes += (Get-Item -LiteralPath (Join-Path $projectPath $relativeFile)).Length }
if ('grounded-engine-question' -notin @($routingPolicy.allowedTaskTypes) -or $estimatedInputBytes -gt [int]$routingPolicy.maxInputBytes) {
    [ordered]@{ok=$false;fallbackToCodex=$true;answer='This task is outside the measured local QWEN routing envelope; send the selected evidence to Codex.';citations=$citations;resource=@{launched=$false};workerError=@{code='LocalPolicyRejected';message='Only measured single-feature classification tasks may launch local QWEN.'};routing=@{estimatedInputBytes=$estimatedInputBytes;maxInputBytes=[int]$routingPolicy.maxInputBytes}}|ConvertTo-Json -Depth 10
    exit 0
}
$raw = & $wrapperPath -Task $task -Files $relativeFiles -MaxTokens ([int]$routingPolicy.maxTokens) -JsonSchema $schema -ProjectRoot $projectPath -TimeoutSeconds ([int]$routingPolicy.timeoutSeconds) -CpuThreads 4 -MaximumRamGiB 24
$worker = $raw | ConvertFrom-Json
if (-not $worker.ok) {
    [ordered]@{ok=$false;fallbackToCodex=$true;answer='Local QWEN was unavailable; send the selected evidence to Codex.';citations=$citations;resource=$worker.resource;workerError=$worker.error}|ConvertTo-Json -Depth 8
    exit 0
}
$allowedChunkIds = @($selected | ForEach-Object {$_.record.id})
$modelChunkIds = @($worker.result.citations)
$citationsValid = $modelChunkIds.Count -gt 0 -and @($modelChunkIds | Where-Object { $_ -notin $allowedChunkIds }).Count -eq 0
if (-not $citationsValid) {
    [ordered]@{ok=$false;fallbackToCodex=$true;answer='Local QWEN returned an ungrounded citation; send the selected evidence to Codex.';citations=$citations;resource=$worker.resource;workerError=@{code='UngroundedCitation';message='A model citation was not present in the selected evidence.'}}|ConvertTo-Json -Depth 8
    exit 0
}
[ordered]@{ok=$true;fallbackToCodex=[bool]$worker.result.needsCodex;answer=$worker.result.answer;modelCitations=$modelChunkIds;citations=$citations;confidence=$worker.result.confidence;resource=$worker.resource}|ConvertTo-Json -Depth 10
