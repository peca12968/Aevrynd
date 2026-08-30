[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$CookedRoot
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $CookedRoot).Path
$manifestPath = Join-Path $root 'material-ibl-asset-tree.manifest.json'
# 中文註解：只擁有材質與 IBL；不可把平行 mesh converter 的輸出納入此切片 receipt。
$files = @('materials','ibl' | ForEach-Object { Get-ChildItem -LiteralPath (Join-Path $root $_) -Recurse -File } | Sort-Object FullName)
$entries = @()
[uint64]$total = 0
foreach ($file in $files) {
    if (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Cooked tree contains a reparse point: $($file.FullName)" }
    $relative = [IO.Path]::GetRelativePath($root, $file.FullName).Replace('\','/')
    if ($relative -notmatch '^[A-Za-z0-9._/-]+$' -or $relative -match '(^|/)\.\.(/|$)') { throw "Cooked path is unsafe: $relative" }
    $total += [uint64]$file.Length
    $entries += [ordered]@{path=$relative;bytes=[uint64]$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$document = [ordered]@{schemaVersion=1;fileCount=$entries.Count;totalBytes=$total;files=$entries}
$json = $document | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText($manifestPath, $json + "`n", [Text.UTF8Encoding]::new($false))
$document | ConvertTo-Json -Compress -Depth 8
