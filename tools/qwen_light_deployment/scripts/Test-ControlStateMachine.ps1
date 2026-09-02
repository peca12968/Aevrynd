Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$resolver=Join-Path $PSScriptRoot 'Resolve-ControlRequest.ps1'
$valid=& $resolver -RequestJson '{"schemaVersion":1,"node":"taichung","action":"switch","mode":"light","requestId":"request_001"}'|ConvertFrom-Json
if(-not$valid.accepted-or$valid.execute-or$valid.steps.Count-ne8){throw 'Valid switch plan test failed.'}
$rejected=$false
try{& $resolver -RequestJson '{"schemaVersion":1,"node":"main","action":"switch","mode":"full","requestId":"request_002"}'|Out-Null}catch{$rejected=$true}
if(-not$rejected){throw 'Main full-mode rejection test failed.'}
$rejected=$false
try{& $resolver -RequestJson '{"schemaVersion":1,"node":"taichung","action":"shell","requestId":"request_003","command":"calc"}'|Out-Null}catch{$rejected=$true}
if(-not$rejected){throw 'Unknown action/field rejection test failed.'}
Write-Output 'Control state-machine static tests passed (3/3).'
