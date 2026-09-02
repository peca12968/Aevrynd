Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common-Light.ps1')
$paths=Get-LightPaths
$state=Get-Content -Raw -LiteralPath $paths.StateFile|ConvertFrom-Json
$process=Get-Process -Id ([int]$state.pid) -ErrorAction Stop
$base='http://127.0.0.1:18080'
$key=Read-LightCredential
$headers=@{Authorization="Bearer $key"}

function Get-HttpStatus {
    param([string]$Uri,[string]$Method='Get',[object]$Body=$null,[hashtable]$Headers=@{})
    try {
        $parameters=@{Uri=$Uri;Method=$Method;Headers=$Headers;TimeoutSec=30;SkipHttpErrorCheck=$true}
        if($null-ne$Body){$parameters.ContentType='application/json';$parameters.Body=($Body|ConvertTo-Json -Depth 6)}
        return Invoke-WebRequest @parameters
    } catch { throw "HTTP probe failed for $Uri : $($_.Exception.Message)" }
}

$health=Get-HttpStatus "$base/health" -Headers $headers
$models=Get-HttpStatus "$base/v1/models" -Headers $headers
$slots=Get-HttpStatus "$base/slots" -Headers $headers
$unauthSlots=Get-HttpStatus "$base/slots"
$unauthChat=Get-HttpStatus "$base/v1/chat/completions" -Method Post -Body @{model='Qwen3-4B-Q4_K_M.gguf';messages=@(@{role='user';content='auth rejection probe'});max_tokens=1}
if($health.StatusCode-ne200-or$models.StatusCode-ne200-or$slots.StatusCode-ne200){throw 'Authorized health/models/slots gate failed.'}
if($unauthSlots.StatusCode-notin@(401,403)-or$unauthChat.StatusCode-notin@(401,403)){throw 'Unauthorized sensitive endpoint gate failed.'}
$slotData=$slots.Content|ConvertFrom-Json
if(@($slotData).Count-ne1){throw 'Parallel slot count is not exactly one.'}
$chatBody=@{model='Qwen3-4B-Q4_K_M.gguf';messages=@(@{role='user';content='Reply with exactly QWEN_LIGHT_OK'});max_tokens=128;temperature=0}
$chat=Get-HttpStatus "$base/v1/chat/completions" -Method Post -Body $chatBody -Headers $headers
if($chat.StatusCode-ne200){throw 'Authorized chat gate failed.'}
$chatData=$chat.Content|ConvertFrom-Json
$answer=[string]$chatData.choices[0].message.content
if($answer.Trim()-ne'QWEN_LIGHT_OK'){throw "Unexpected chat marker: $answer"}

$gpuBytes=$null
$nvidia=Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
if($nvidia){
    $rows=& $nvidia.Source --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>$null
    foreach($row in @($rows)){if($row-match'^\s*(\d+)\s*,\s*(\d+)'){if([int]$Matches[1]-eq$process.Id){$gpuBytes=[int64]$Matches[2]*1MB}}}
}
[ordered]@{passed=$true;pid=$process.Id;workingSetBytes=$process.WorkingSet64;privateBytes=$process.PrivateMemorySize64;gpuVramBytes=$gpuBytes;gpuMeasurement=if($null-eq$gpuBytes){'not-reported-for-process'}else{'nvidia-smi'};healthStatus=$health.StatusCode;modelsStatus=$models.StatusCode;slotsStatus=$slots.StatusCode;unauthorizedSlotsStatus=$unauthSlots.StatusCode;unauthorizedChatStatus=$unauthChat.StatusCode;slotCount=@($slotData).Count;chatMarker='QWEN_LIGHT_OK';profile=[ordered]@{host=$state.host;port=$state.port;context=$state.context;parallel=$state.parallel;threads=$state.threads;gpuLayers=$state.gpuLayers}}|ConvertTo-Json -Depth 5
$key=$null
