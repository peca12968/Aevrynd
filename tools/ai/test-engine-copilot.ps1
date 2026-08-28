param(
    [ValidateRange(0,5)] [int]$ModelCases = 0,
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'
$projectPath = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
$evalPath = Join-Path $projectPath 'engine\ai\evaluation\engine-copilot-eval.jsonl'
$queryPath = Join-Path $projectPath 'tools\ai\query-engine-copilot.ps1'
$knowledgePath = Join-Path $projectPath 'engine\ai\knowledge\knowledge.jsonl'
$manifestPath = Join-Path $projectPath 'engine\ai\knowledge\manifest.json'
$schemaPath = Join-Path $projectPath 'engine\ai\config\knowledge-schema.json'
$routingPolicyPath = Join-Path $projectPath 'engine\ai\config\task-routing-policy.json'
$readmePath = Join-Path $projectPath 'engine\ai\README.md'
$infoPath = [IO.Path]::GetFullPath((Join-Path $projectPath '..\Info\QwenEngineCopilot.md'))

function Get-PackHash([string]$Root) {
    $base = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $entries = Get-ChildItem -LiteralPath $base -File -Recurse | Sort-Object FullName | ForEach-Object {
        $_.FullName.Substring($base.Length + 1).Replace('\','/') + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($entries -join "`n"))))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
}
$cases = @([IO.File]::ReadAllLines($evalPath) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_ | ConvertFrom-Json })
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$schema = Get-Content -LiteralPath $schemaPath -Raw | ConvertFrom-Json
$routingPolicy = Get-Content -LiteralPath $routingPolicyPath -Raw | ConvertFrom-Json
$actualKnowledgeSha = (Get-FileHash -LiteralPath $knowledgePath -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceSnapshotPassed = $true
foreach ($source in @($manifest.sources)) {
    $snapshotContent = ((& git -C $projectPath show ($manifest.snapshotRevision + ':' + $source.path)) -join "`n") + "`n"
    if ($LASTEXITCODE -ne 0) { $sourceSnapshotPassed=$false;break }
    $snapshotSha = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($snapshotContent))).ToLowerInvariant()
    $snapshotRevision = (& git -C $projectPath log -1 --format=%H $manifest.snapshotRevision -- $source.path | Select-Object -First 1).Trim()
    # 每個來源都要能由 manifest 指定的 HEAD 快照重算，不能只驗證產物彼此一致。
    if ($snapshotSha -cne [string]$source.sha256 -or [Text.Encoding]::UTF8.GetByteCount($snapshotContent) -ne [int64]$source.bytes -or $snapshotRevision -cne [string]$source.revision) { $sourceSnapshotPassed=$false;break }
}
$metadataPassed = 0
$largestChunkBytes = 0L
$chunkLines = 0
$chunkParityPassed = $true
foreach ($line in [IO.File]::ReadAllLines($knowledgePath)) {
    $record = $line | ConvertFrom-Json
    $chunkLines++
    $chunkFile = Join-Path $projectPath ('engine\ai\knowledge\chunks\' + $record.id + '.json')
    $size = (Get-Item -LiteralPath $chunkFile).Length
    if ([IO.File]::ReadAllText($chunkFile) -cne $line) { $chunkParityPassed=$false }
    if ($size -gt $largestChunkBytes) { $largestChunkBytes=$size }
    if ($record.id -and $record.sourcePath -and $record.sourceSha256 -match '^[0-9a-f]{64}$' -and $record.sourceRevision -and $record.symbol -and $record.engineVersion -and $record.license -and $null -ne $record.trainingEligible) { $metadataPassed++ }
}
$requiredExclusions = @('/build/','/build-','/bin/','/binary/','/binaries/','/obj/','/out/','/generated/','/private/','/secrets/')
$exclusionsComplete = @($requiredExclusions | Where-Object { $_ -notin @($manifest.exclusions) }).Count -eq 0
$excludedSourceCount = @($manifest.sources | Where-Object {
    $path='/' + ([string]$_.path).TrimStart('/').ToLowerInvariant()
    @($manifest.exclusions | Where-Object { $path.Contains(([string]$_).ToLowerInvariant()) }).Count -gt 0
}).Count
$schemaContractPassed = -not [bool]$schema.'$defs'.chunk.additionalProperties -and -not [bool]$schema.'$defs'.manifest.additionalProperties -and
    @($schema.'$defs'.chunk.required).Count -eq 12 -and @($schema.'$defs'.manifest.required).Count -eq 12 -and
    $schema.'$defs'.manifest.properties.trainingEligible.const -eq $false
$manifestPassed = $sourceSnapshotPassed -and $actualKnowledgeSha -eq $manifest.knowledgeSha256 -and $chunkLines -eq $manifest.chunkCount -and $metadataPassed -eq $chunkLines -and
    $chunkParityPassed -and $largestChunkBytes -lt 8192 -and -not [bool]$manifest.trainingEligible -and
    $manifest.sensitiveContentScan -eq 'passed' -and $exclusionsComplete -and $excludedSourceCount -eq 0 -and $schemaContractPassed
$retrievalPassed = 0
$results = [Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
    $response = (& $queryPath -Question $case.question -TopK 3 -SkipModel -ProjectRoot $projectPath) | ConvertFrom-Json
    $sources = @($response.citations | ForEach-Object sourcePath)
    $matched = @($case.expectedSources | Where-Object { $expected=$_; $sources | Where-Object { $_ -like ('*' + $expected) } }).Count -gt 0
    if ($matched) { $retrievalPassed++ }
    $results.Add([ordered]@{id=$case.id;passed=$matched;sources=$sources})
}

# 未知問題必須安全回退；純檢索查詢前後的知識包雜湊必須完全一致。
$beforeReadOnlySha = (Get-FileHash -LiteralPath $knowledgePath -Algorithm SHA256).Hash
$beforeReadOnlyPackSha = Get-PackHash (Split-Path -Parent $knowledgePath)
$unknown = (& $queryPath -Question 'zzzaevrynd_no_such_symbol_9f61b7' -SkipModel -ProjectRoot $projectPath) | ConvertFrom-Json
$afterReadOnlySha = (Get-FileHash -LiteralPath $knowledgePath -Algorithm SHA256).Hash
$afterReadOnlyPackSha = Get-PackHash (Split-Path -Parent $knowledgePath)
$unknownFallbackPassed = -not [bool]$unknown.ok -and [bool]$unknown.fallbackToCodex -and @($unknown.citations).Count -eq 0
$readOnlyPassed = $beforeReadOnlySha -eq $afterReadOnlySha -and $beforeReadOnlyPackSha -eq $afterReadOnlyPackSha

# 在隔離暫存副本破壞 manifest、JSONL 與 chunk；查詢必須全部 fail closed，且不得啟動模型。
$negativeRoot = Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-ai-negative-' + [guid]::NewGuid().ToString('N'))
$negativeKnowledge = Join-Path $negativeRoot 'engine\ai\knowledge'
New-Item -ItemType Directory -Path $negativeKnowledge -Force | Out-Null
Copy-Item -LiteralPath $knowledgePath -Destination (Join-Path $negativeKnowledge 'knowledge.jsonl')
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $negativeKnowledge 'manifest.json')
Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $knowledgePath) 'chunks') -Destination $negativeKnowledge -Recurse
$negativePassed = 0
try {
    Add-Content -LiteralPath (Join-Path $negativeKnowledge 'knowledge.jsonl') -Value 'tampered' -NoNewline
    $tamperedKnowledge = (& $queryPath -Question 'Aevrynd engine version' -SkipModel -ProjectRoot $negativeRoot) | ConvertFrom-Json
    if (-not [bool]$tamperedKnowledge.ok -and [bool]$tamperedKnowledge.fallbackToCodex -and -not [bool]$tamperedKnowledge.resource.launched -and $tamperedKnowledge.workerError.code -eq 'KnowledgePackIntegrity') { $negativePassed++ }
    Copy-Item -LiteralPath $knowledgePath -Destination (Join-Path $negativeKnowledge 'knowledge.jsonl') -Force
    $firstChunk = Get-ChildItem -LiteralPath (Join-Path $negativeKnowledge 'chunks') -Filter '*.json' -File | Sort-Object Name | Select-Object -First 1
    Add-Content -LiteralPath $firstChunk.FullName -Value ' ' -NoNewline
    $tamperedChunk = (& $queryPath -Question 'Aevrynd engine version' -SkipModel -ProjectRoot $negativeRoot) | ConvertFrom-Json
    if (-not [bool]$tamperedChunk.ok -and [bool]$tamperedChunk.fallbackToCodex -and -not [bool]$tamperedChunk.resource.launched -and $tamperedChunk.workerError.code -eq 'KnowledgePackIntegrity') { $negativePassed++ }
    Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $knowledgePath) ('chunks\' + $firstChunk.Name)) -Destination $firstChunk.FullName -Force
    $badManifest = Get-Content -LiteralPath (Join-Path $negativeKnowledge 'manifest.json') -Raw | ConvertFrom-Json
    $badManifest.trainingEligible = $true
    [IO.File]::WriteAllText((Join-Path $negativeKnowledge 'manifest.json'),($badManifest|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    $trainingBypass = (& $queryPath -Question 'Aevrynd engine version' -SkipModel -ProjectRoot $negativeRoot) | ConvertFrom-Json
    if (-not [bool]$trainingBypass.ok -and [bool]$trainingBypass.fallbackToCodex -and -not [bool]$trainingBypass.resource.launched -and $trainingBypass.workerError.code -eq 'KnowledgePackIntegrity') { $negativePassed++ }
    Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $negativeKnowledge 'manifest.json') -Force
    [IO.File]::WriteAllText((Join-Path $negativeKnowledge 'chunks\aec-99999.json'),'{}',[Text.UTF8Encoding]::new($false))
    $unexpectedChunk = (& $queryPath -Question 'Aevrynd engine version' -SkipModel -ProjectRoot $negativeRoot) | ConvertFrom-Json
    if (-not [bool]$unexpectedChunk.ok -and [bool]$unexpectedChunk.fallbackToCodex -and -not [bool]$unexpectedChunk.resource.launched -and $unexpectedChunk.workerError.code -eq 'KnowledgePackIntegrity') { $negativePassed++ }
    [IO.File]::Delete((Join-Path $negativeKnowledge 'chunks\aec-99999.json'))
    New-Item -ItemType Directory -Path (Join-Path $negativeRoot 'engine\ai\config'),(Join-Path $negativeRoot 'tools\ci') -Force | Out-Null
    $badPolicy = Get-Content -LiteralPath $routingPolicyPath -Raw | ConvertFrom-Json
    $badPolicy.maxInputBytes = 2048
    [IO.File]::WriteAllText((Join-Path $negativeRoot 'engine\ai\config\task-routing-policy.json'),($badPolicy|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath (Join-Path $projectPath 'tools\ci\invoke-local-qwen.ps1') -Destination (Join-Path $negativeRoot 'tools\ci\invoke-local-qwen.ps1')
    $policyTamper = (& $queryPath -Question 'Aevrynd engine version' -ProjectRoot $negativeRoot) | ConvertFrom-Json
    if (-not [bool]$policyTamper.ok -and [bool]$policyTamper.fallbackToCodex -and -not [bool]$policyTamper.resource.launched -and $policyTamper.workerError.code -eq 'RoutingPolicyIntegrity') { $negativePassed++ }
} finally {
    $resolvedNegative = [IO.Path]::GetFullPath($negativeRoot)
    if ($resolvedNegative.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $resolvedNegative)) {
        Remove-Item -LiteralPath $resolvedNegative -Recurse -Force
    }
}
$negativeSafetyPassed = $negativePassed -eq 5

$uniqueSystems = @($cases.system | Sort-Object -Unique).Count
$evaluationCoveragePassed = $cases.Count -eq 38 -and $uniqueSystems -ge 15
$roadmapText = (Get-Content -LiteralPath $readmePath -Raw) + "`n" + (Get-Content -LiteralPath $infoPath -Raw)
$learningGatesPassed = -not [bool]$manifest.trainingEligible -and $roadmapText -match '2,000' -and $roadmapText -match '200' -and
    $roadmapText -match '90%' -and $roadmapText -match '85%' -and $roadmapText -match '2[^\r\n]*(percentage|百分點|個百分點)'
$policyProbe = (& $queryPath -Question 'Classify the BloomSettings3D feature boundary.' -TopK 2 -ProjectRoot $projectPath) | ConvertFrom-Json
$routingPolicyPassed = $routingPolicy.schemaVersion -eq 1 -and @($routingPolicy.allowedTaskTypes).Count -eq 1 -and
    $routingPolicy.allowedTaskTypes[0] -eq 'single-feature-classification' -and $routingPolicy.maxInputBytes -eq 1024 -and
    $routingPolicy.maxTokens -eq 96 -and $routingPolicy.timeoutSeconds -eq 30 -and [bool]$routingPolicy.requiresFixedJsonSchema -and
    [bool]$routingPolicy.fallbackToCodex -and $routingPolicy.measuredEvidence.singleFeatureBloom.inputBytes -eq 528 -and
    $routingPolicy.measuredEvidence.singleFeatureBloom.elapsedMilliseconds -eq 19408 -and $routingPolicy.measuredEvidence.singleFeatureBloom.peakWorkingSetGiB -eq 14.94 -and
    $routingPolicy.measuredEvidence.singleFeatureBloom.result -eq 'passed' -and $routingPolicy.measuredEvidence.threeFeatureBatch.inputBytes -eq 3218 -and
    $routingPolicy.measuredEvidence.threeFeatureBatch.maxTokens -eq 512 -and $routingPolicy.measuredEvidence.threeFeatureBatch.timeoutSeconds -eq 30 -and
    $routingPolicy.measuredEvidence.threeFeatureBatch.result -eq 'timed-out' -and -not [bool]$policyProbe.ok -and
    [bool]$policyProbe.fallbackToCodex -and -not [bool]$policyProbe.resource.launched -and $policyProbe.workerError.code -eq 'LocalPolicyRejected'

$modelResults = [Collections.Generic.List[object]]::new()
$peak = 0L
$elapsed = 0L
$modelPassed = 0
foreach ($case in ($cases | Select-Object -First $ModelCases)) {
    $response = (& $queryPath -Question $case.question -TopK 2 -ProjectRoot $projectPath) | ConvertFrom-Json
    $termMatch = $false
    foreach ($term in $case.expectedTerms) { if ($response.answer -like ('*' + $term + '*')) { $termMatch=$true;break } }
    $passed = [bool]$response.ok -and $termMatch -and -not [bool]$response.fallbackToCodex
    if ($passed) { $modelPassed++ }
    if ($response.resource.peakWorkingSetBytes -gt $peak) { $peak=[int64]$response.resource.peakWorkingSetBytes }
    $elapsed += [int64]$response.resource.elapsedMilliseconds
    $modelResults.Add([ordered]@{id=$case.id;passed=$passed;fallbackToCodex=[bool]$response.fallbackToCodex;elapsedMilliseconds=[int64]$response.resource.elapsedMilliseconds;peakWorkingSetBytes=[int64]$response.resource.peakWorkingSetBytes;answer=$response.answer})
}

$gpuReceipt = 'Not attributable from the current dispatcher receipt; the prior bounded baseline measured 0 MiB.'
[ordered]@{
    ok=($manifestPassed -and $unknownFallbackPassed -and $readOnlyPassed -and $negativeSafetyPassed -and $evaluationCoveragePassed -and $learningGatesPassed -and $routingPolicyPassed -and $retrievalPassed -eq $cases.Count -and ($ModelCases -eq 0 -or $modelPassed -eq $ModelCases))
    knowledge=[ordered]@{passed=$manifestPassed;manifestSha256=$manifest.knowledgeSha256;actualSha256=$actualKnowledgeSha;packSha256=$afterReadOnlyPackSha;chunks=$chunkLines;metadataComplete=$metadataPassed;chunkParity=$chunkParityPassed;sourceSnapshot=$sourceSnapshotPassed;largestChunkBytes=$largestChunkBytes;trainingEligible=[bool]$manifest.trainingEligible;schemaContract=$schemaContractPassed;exclusionsComplete=$exclusionsComplete;excludedSources=$excludedSourceCount}
    safety=[ordered]@{unknownQuestionFallback=$unknownFallbackPassed;readOnlyHashStable=$readOnlyPassed;negativeCasesPassed=$negativePassed;negativeCasesTotal=5;tamperFailClosed=$negativeSafetyPassed;learningGatesDocumented=$learningGatesPassed;routingPolicyPassed=$routingPolicyPassed;policyProbeLaunched=[bool]$policyProbe.resource.launched}
    retrieval=[ordered]@{passed=$retrievalPassed;total=$cases.Count;accuracy=[Math]::Round($retrievalPassed/[double]$cases.Count,4);uniqueSystems=$uniqueSystems;coveragePassed=$evaluationCoveragePassed}
    model=[ordered]@{passed=$modelPassed;total=$ModelCases;accuracy=if($ModelCases){[Math]::Round($modelPassed/[double]$ModelCases,4)}else{0};elapsedMilliseconds=$elapsed;peakWorkingSetBytes=$peak;gpuReceipt=$gpuReceipt;cases=$modelResults}
    cases=$results
}|ConvertTo-Json -Depth 10
