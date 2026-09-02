param([Parameter(Mandatory)][string]$RequestJson)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($RequestJson.Length-gt1024){throw 'Control request exceeds 1024 characters.'}
try{$request=$RequestJson|ConvertFrom-Json -ErrorAction Stop}catch{throw 'Control request is not valid JSON.'}
$names=@($request.PSObject.Properties.Name)
if(@($names|Where-Object{$_-notin@('schemaVersion','node','action','mode','requestId')}).Count){throw 'Control request contains an unknown field.'}
foreach($required in @('schemaVersion','node','action','requestId')){if($required-notin$names){throw "Control request is missing $required."}}
if([int]$request.schemaVersion-ne1){throw 'Unsupported control schema version.'}
$node=[string]$request.node;$action=[string]$request.action;$requestId=[string]$request.requestId
if($node-notin@('main','taichung','taoyuan')){throw 'Node is not allowlisted.'}
if($action-notin@('status','switch')){throw 'Action is not allowlisted.'}
if($requestId-notmatch'^[A-Za-z0-9_-]{8,64}$'){throw 'Request ID format is invalid.'}
if($action-eq'status'){
    if('mode'-in$names){throw 'Status request cannot include mode.'}
    [ordered]@{accepted=$true;execute=$false;node=$node;action='status';requestId=$requestId}|ConvertTo-Json
    exit 0
}
if('mode'-notin$names){throw 'Switch request is missing mode.'}
$mode=[string]$request.mode
if($mode-notin@('light','full')){throw 'Mode is not allowlisted.'}
if($node-eq'main'-and$mode-ne'light'){throw 'Full mode is forbidden on the main computer.'}
# 此腳本只解析並產生不可變計畫，不直接執行系統命令；LINE 授權需在上游完成。
[ordered]@{accepted=$true;execute=$false;integrationRequired=$true;node=$node;action='switch';mode=$mode;requestId=$requestId;steps=@('acquire-node-lock','stop-current','confirm-release','start-target','health-gate','chat-gate','rollback-on-failure','release-node-lock')}|ConvertTo-Json -Depth 4
