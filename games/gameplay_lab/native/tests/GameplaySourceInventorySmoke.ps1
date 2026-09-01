param(
  [Parameter(Mandatory=$true)][string]$SourceRoot,
  [Parameter(Mandatory=$true)][string]$InventoryPath)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GameplaySourceInventory.ps1')

function Expect-Failure([scriptblock]$Action, [string]$Label) {
  $failed = $false
  try { & $Action | Out-Null } catch { $failed = $true }
  if (-not $failed) { throw "Expected source inventory failure: $Label" }
}

$positive = Get-GameplaySourceInventoryEvidence $SourceRoot $InventoryPath
if ($positive.files -lt 100 -or $positive.fingerprint -notmatch '^[0-9a-f]{64}$') { throw 'Positive source inventory evidence is incomplete' }
$lines = @(Get-Content -LiteralPath $InventoryPath -Encoding UTF8 | Where-Object { $_.Length -gt 0 })
$scratch = Join-Path ([IO.Path]::GetTempPath()) ("aevrynd-source-inventory-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
try {
  $omitted = Join-Path $scratch 'omitted.txt'
  @($lines | Where-Object { -not $_.StartsWith('engine/native/src/D3D12Renderer.cpp:', [StringComparison]::Ordinal) }) | Set-Content -LiteralPath $omitted -Encoding UTF8
  Expect-Failure { Get-GameplaySourceInventoryEvidence $SourceRoot $omitted } 'omitted production file'

  $mutated = Join-Path $scratch 'mutated.txt'
  @($lines | ForEach-Object { if ($_.StartsWith('games/gameplay_lab/native/src/GameplayLabGame.cpp:', [StringComparison]::Ordinal)) { $_ -replace ':[0-9a-f]{64}$', (':' + ('0' * 64)) } else { $_ } }) | Set-Content -LiteralPath $mutated -Encoding UTF8
  Expect-Failure { Get-GameplaySourceInventoryEvidence $SourceRoot $mutated } 'mutated production file hash'

  $extra = Join-Path $scratch 'extra.txt'
  @($lines + ('games/gameplay_lab/native/src/NotARealProductionFile.cpp:' + ('0' * 64))) | Sort-Object -CaseSensitive | Set-Content -LiteralPath $extra -Encoding UTF8
  Expect-Failure { Get-GameplaySourceInventoryEvidence $SourceRoot $extra } 'extra inventory path'
}
finally { Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue }

Write-Output "GameplaySourceInventorySmoke PASS files=$($positive.files) fingerprint=$($positive.fingerprint)"
