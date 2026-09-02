Set-StrictMode -Version Latest;$ErrorActionPreference='Stop';. (Join-Path $PSScriptRoot 'Common-Light.ps1');$paths=Get-LightPaths
New-Item -ItemType Directory -Force -Path $paths.Models|Out-Null
$url='https://huggingface.co/Qwen/Qwen3-4B-GGUF/resolve/bc640142c66e1fdd12af0bd68f40445458f3869b/Qwen3-4B-Q4_K_M.gguf'
$partial="$($paths.Model).partial"
$curl=Get-Command curl.exe -ErrorAction Stop
& $curl.Source '--fail' '--location' '--retry' '5' '--retry-delay' '3' '--continue-at' '-' '--output' $partial $url
if($LASTEXITCODE-ne0){throw "Model download failed with curl exit code $LASTEXITCODE."}
if((Get-Item -LiteralPath $partial).Length-ne2497280256-or(Get-FileHash -Algorithm SHA256 -LiteralPath $partial).Hash.ToLowerInvariant()-ne'7485fe6f11af29433bc51cab58009521f205840f5b4ae3a32fa7f92e8534fdf5'){throw 'Downloaded model integrity mismatch.'}
Move-Item -LiteralPath $partial -Destination $paths.Model -Force;Write-Output 'Light model download and SHA-256 verification passed.'
