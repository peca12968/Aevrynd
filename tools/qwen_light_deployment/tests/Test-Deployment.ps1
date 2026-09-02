Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$failures=@()
Get-ChildItem -LiteralPath $root -Recurse -Filter '*.ps1'|ForEach-Object{
    $tokens=$null;$errors=$null
    [Management.Automation.Language.Parser]::ParseFile($_.FullName,[ref]$tokens,[ref]$errors)|Out-Null
    if($errors){$failures+="Parser: $($_.Name)"}
}
$manifest=Get-Content -Raw -LiteralPath (Join-Path $root 'manifest.json')|ConvertFrom-Json
if($manifest.model.revision-ne'bc640142c66e1fdd12af0bd68f40445458f3869b'){$failures+='Model revision pin'}
if($manifest.model.sha256-ne'7485fe6f11af29433bc51cab58009521f205840f5b4ae3a32fa7f92e8534fdf5'){$failures+='Model SHA pin'}
if([int64]$manifest.model.bytes-ne2497280256){$failures+='Model byte pin'}
& (Join-Path $root 'scripts\Test-ControlStateMachine.ps1')|Out-Null
$badRequests=@(
    '{"schemaVersion":1,"node":"main","action":"switch","mode":"full","requestId":"request_001"}',
    '{"schemaVersion":1,"node":"taichung","action":"shell","requestId":"request_002"}',
    '{"schemaVersion":1,"node":"../main","action":"status","requestId":"request_003"}',
    '{"schemaVersion":1,"node":"taichung","action":"status","requestId":"request_004","command":"calc"}'
)
foreach($request in $badRequests){$rejected=$false;try{& (Join-Path $root 'scripts\Resolve-ControlRequest.ps1') -RequestJson $request|Out-Null}catch{$rejected=$true};if(-not$rejected){$failures+='Hostile control request accepted'}}
$download=Get-Content -Raw -LiteralPath (Join-Path $root 'scripts\Download-LightModel.ps1')
foreach($pin in @($manifest.model.revision,$manifest.model.sha256,[string]$manifest.model.bytes)){if(-not$download.Contains($pin)){$failures+='Download script missing immutable pin'}}
$start=Get-Content -Raw -LiteralPath (Join-Path $root 'scripts\Start-Light.ps1')
foreach($setting in @("'127.0.0.1'","'18080'","'2048'","'1'","'4'","'0'")){if(-not$start.Contains($setting)){$failures+="Start setting missing: $setting"}}
if($start-match'--api-key'){ $failures+='API key appears in process arguments' }
if($failures.Count){throw ($failures-join'; ')}
Write-Output 'Qwen light deployment tests passed (parser, pins, profile, credential transport, hostile requests).'
