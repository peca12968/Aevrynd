Set-StrictMode -Version Latest

function Get-GameplaySourceFileSha256([string]$Path) {
  $sha = [Security.Cryptography.SHA256]::Create()
  try { return ([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($Path))).Replace('-', '').ToLowerInvariant()) }
  finally { $sha.Dispose() }
}

function Get-GameplaySourceInventoryPaths([string]$SourceRoot) {
  $root = [IO.Path]::GetFullPath($SourceRoot)
  $paths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  $scopes = @(
    @{ Root = 'engine/native/include'; Extensions = @('.h', '.hpp', '.cpp') },
    @{ Root = 'engine/native/src'; Extensions = @('.h', '.hpp', '.cpp') },
    @{ Root = 'engine/native/shaders'; Extensions = @('.hlsl', '.hlsli') },
    @{ Root = 'engine/native/tests'; Extensions = @('.h', '.hpp', '.cpp', '.ps1') },
    @{ Root = 'games/gameplay_lab/native/include'; Extensions = @('.h', '.hpp', '.cpp') },
    @{ Root = 'games/gameplay_lab/native/src'; Extensions = @('.h', '.hpp', '.cpp') },
    @{ Root = 'games/gameplay_lab/native/tests'; Extensions = @('.h', '.hpp', '.cpp', '.ps1') }
  )
  foreach ($scope in $scopes) {
    $scopePath = Join-Path $root $scope.Root
    if (-not (Test-Path -LiteralPath $scopePath -PathType Container)) { throw "Source inventory scope missing: $($scope.Root)" }
    foreach ($file in Get-ChildItem -LiteralPath $scopePath -Recurse -File) {
      if ($scope.Extensions -contains $file.Extension.ToLowerInvariant()) {
        $relative = $file.FullName.Substring($root.TrimEnd('\', '/').Length + 1).Replace('\', '/')
        if (-not $paths.Add($relative)) { throw "Duplicate source inventory path: $relative" }
      }
    }
  }
  foreach ($relative in @(
      'CMakeLists.txt',
      'engine/native/CMakeLists.txt',
      'games/gameplay_lab/game.json',
      'games/gameplay_lab/native/CMakeLists.txt',
      'games/gameplay_lab/native/app.manifest',
      'engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json')) {
    $full = Join-Path $root $relative
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Required source inventory file missing: $relative" }
    [void]$paths.Add($relative)
  }
  return @($paths) | Sort-Object -CaseSensitive
}

function Get-GameplaySourceInventoryEvidence([string]$SourceRoot, [string]$InventoryPath) {
  if (-not (Test-Path -LiteralPath $InventoryPath -PathType Leaf)) { throw 'Generated source inventory missing' }
  $root = [IO.Path]::GetFullPath($SourceRoot)
  $lines = @(Get-Content -LiteralPath $InventoryPath -Encoding UTF8 | Where-Object { $_.Length -gt 0 })
  if ($lines.Count -eq 0) { throw 'Generated source inventory is empty' }
  $entries = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
  $previous = $null
  foreach ($line in $lines) {
    if ($line -notmatch '^([^:]+):([0-9a-f]{64})$') { throw "Invalid source inventory entry: $line" }
    $relative = $Matches[1]
    $hash = $Matches[2]
    if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains('\') -or $relative.Contains('../') -or $relative.Contains('/../')) { throw "Unsafe source inventory path: $relative" }
    if ($null -ne $previous -and [StringComparer]::Ordinal.Compare($previous, $relative) -ge 0) { throw 'Source inventory paths are duplicate or unsorted' }
    if ($entries.ContainsKey($relative)) { throw "Duplicate source inventory path: $relative" }
    $entries.Add($relative, $hash)
    $previous = $relative
  }
  $actualPaths = @(Get-GameplaySourceInventoryPaths $root)
  if ($entries.Count -ne $actualPaths.Count) { throw "Source inventory path count mismatch: inventory=$($entries.Count) actual=$($actualPaths.Count)" }
  for ($index = 0; $index -lt $actualPaths.Count; ++$index) {
    $relative = $actualPaths[$index]
    if (-not $entries.ContainsKey($relative)) { throw "Source inventory missing path: $relative" }
    $actualHash = Get-GameplaySourceFileSha256 (Join-Path $root $relative)
    if ($entries[$relative] -cne $actualHash) { throw "Source inventory hash mismatch: $relative" }
  }
  $canonical = (($lines -join "`n") + "`n")
  $sha = [Security.Cryptography.SHA256]::Create()
  try { $fingerprint = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical))).Replace('-', '').ToLowerInvariant()) }
  finally { $sha.Dispose() }
  return [pscustomobject]@{ fingerprint = $fingerprint; files = $entries.Count; canonical = $canonical }
}
