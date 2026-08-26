$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$engineName = -join ([char[]](0x5F15,0x64CE))
$gamesName = -join ([char[]](0x904A,0x6232))
$gameName = -join ([char[]](0x88C2,0x9699,0x5B88,0x671B,0x8005))
$engineRoot = Join-Path $root $engineName
$gameRoot = Join-Path (Join-Path $root $gamesName) $gameName
$llama = Join-Path $engineRoot 'runtime\llama.cpp\llama.exe'
$model = Join-Path $engineRoot 'runtime\models\Qwen3.8-27B-UD-Q4_K_M.gguf'
$modelArgument = '..\models\Qwen3.8-27B-UD-Q4_K_M.gguf'
$game = Join-Path $gameRoot 'dist\Riftwarden-DX12.exe'
$localKey = 'riftwarden-local-only'
foreach ($required in @($llama,$model,$game)) { if (-not (Test-Path -LiteralPath $required)) { throw "Missing required file: $required" } }
$llm = Get-NetTCPConnection -LocalPort 8081 -State Listen -ErrorAction SilentlyContinue
if (-not $llm) {
  Start-Process -FilePath $llama -WindowStyle Hidden -WorkingDirectory (Split-Path $llama) -ArgumentList @(
    'serve','-m',$modelArgument,'--host','127.0.0.1','--port','8081','-c','4096',
    '-ngl','auto','-fit','on','-fa','on','-t','12','--api-key',$localKey,'--no-webui'
  ) -RedirectStandardOutput (Join-Path $root 'llama-server.log') -RedirectStandardError (Join-Path $root 'llama-server-error.log')
}
Write-Host 'Loading local Qwen3.8-27B...' -ForegroundColor Cyan
for ($i=0;$i -lt 90;$i++) { try { $headers=@{Authorization="Bearer $localKey"};$null=Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri 'http://127.0.0.1:8081/health' -TimeoutSec 2;break } catch { Start-Sleep -Seconds 1 } }
Start-Process -FilePath $game -WorkingDirectory (Split-Path $game)
