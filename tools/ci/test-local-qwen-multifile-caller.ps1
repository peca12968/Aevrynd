$Mode = if ($args.Count -gt 0) { [string]$args[0] } else { 'Valid' }
$ErrorActionPreference = 'Stop'
$wrapper = (Resolve-Path (Join-Path $PSScriptRoot 'invoke-local-qwen.ps1')).Path
$files = switch ($Mode) {
    'Valid' { @('engine/assets/material-packs/material-packs.json','engine/assets/material-packs/LICENSE.md') }
    'TooMany' { @(0..64 | ForEach-Object { "synthetic-$_.txt" }) }
    'TooLong' { @(('x' * 1025)) }
    'DuplicateFinal' { @('engine/assets/material-packs/LICENSE.md','engine/assets/material-packs/../material-packs/LICENSE.md') }
    default { throw 'Unknown multi-file test mode.' }
}

# 從同一 PowerShell 程序傳入真正的 string[]，驗證 supervisor 到 worker 的多檔 transport。
& $wrapper -Task 'synthetic multi-file protocol' -Files $files -MaxTokens 32 -TimeoutSeconds 10 -CpuThreads 1 -MaximumRamGiB 4
