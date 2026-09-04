Set-StrictMode -Version Latest

$script:RelaySchemaVersion = 1
$script:MaximumOperations = 100
$script:MaximumCommandBytes = 32768
$script:MaximumStateBytes = 1048576
$script:EpochCursor = '1970-01-01T00:00:00.0000000+00:00'

function Assert-RelayPathSafe {
    param([Parameter(Mandatory)][string]$Path)
    if([string]::IsNullOrWhiteSpace($Path)-or$Path.IndexOfAny([char[]]@('"',"`r","`n"))-ge0){throw 'Relay path is invalid'}
    $full=[IO.Path]::GetFullPath($Path)
    if(-not[IO.Path]::IsPathRooted($full)-or$full.StartsWith('\\',[StringComparison]::Ordinal)){throw 'Relay path must be a local absolute path'}
    $current=$full
    while(-not[string]::IsNullOrWhiteSpace($current)){
        if(Test-Path -LiteralPath $current){$item=Get-Item -LiteralPath $current -Force;if(($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw "Relay path contains a reparse point: $current"}}
        $parent=Split-Path -Parent $current;if([string]::IsNullOrWhiteSpace($parent)-or$parent-ceq$current){break};$current=$parent
    }
    return $full
}

function Protect-RelayDirectoryAcl {
    param([Parameter(Mandatory)][string]$Path)
    $full=Assert-RelayPathSafe $Path
    if(-not(Test-Path -LiteralPath $full -PathType Container)){New-Item -ItemType Directory -Path $full -Force|Out-Null}
    if(-not$env:SystemRoot){throw 'Windows system root is unavailable'};$icacls=Assert-RelayPathSafe (Join-Path $env:SystemRoot 'System32\icacls.exe');if(-not(Test-Path -LiteralPath $icacls -PathType Leaf)){throw 'Trusted ACL utility was not found'}
    $currentSid=[Security.Principal.WindowsIdentity]::GetCurrent().User;$ownerSid=([Security.Principal.NTAccount](Get-Acl -LiteralPath $full).Owner).Translate([Security.Principal.SecurityIdentifier])
    if($ownerSid.Value-notin@($currentSid.Value,'S-1-5-18','S-1-5-32-544')){$null=&$icacls $full /setowner ("*$($currentSid.Value)") /q;if($LASTEXITCODE-ne0){throw 'Relay directory owner could not be secured'}}
    $null=&$icacls $full /inheritance:r /q;if($LASTEXITCODE-ne0){throw 'Relay directory inheritance could not be disabled'}
    $present=@((Get-Acl -LiteralPath $full).Access|ForEach-Object{$_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value}|Sort-Object -Unique)
    foreach($sid in $present){$null=&$icacls $full /remove ("*$sid") /q;if($LASTEXITCODE-ne0){throw 'Relay directory ACL entry could not be removed'}}
    $null=&$icacls $full /grant:r ("*$($currentSid.Value):(OI)(CI)F") '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F' /q
    if($LASTEXITCODE-ne0){throw 'Relay directory ACL could not be secured'}
    Assert-RelayDirectoryAcl $full
}

function Assert-RelayDirectoryAcl {
    param([Parameter(Mandatory)][string]$Path)
    $full=Assert-RelayPathSafe $Path;$acl=Get-Acl -LiteralPath $full
    if(-not$acl.AreAccessRulesProtected){throw 'Relay directory ACL inheritance is enabled'}
    $allowed=@([Security.Principal.WindowsIdentity]::GetCurrent().User.Value,'S-1-5-18','S-1-5-32-544')
    $owner=$acl.Owner;if($owner){$owner=([Security.Principal.NTAccount]$owner).Translate([Security.Principal.SecurityIdentifier]).Value};if($owner-notin$allowed){throw 'Relay directory owner is unauthorized'}
    foreach($rule in @($acl.Access)){
        $sid=$rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
        if($rule.AccessControlType-ne[Security.AccessControl.AccessControlType]::Allow-or$sid-notin$allowed){throw "Relay directory ACL contains an unauthorized principal: $sid"}
    }
    foreach($sid in $allowed){if(-not(@($acl.Access|Where-Object{$_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value-ceq$sid-and($_.FileSystemRights-band[Security.AccessControl.FileSystemRights]::FullControl)}).Count)){throw 'Relay directory ACL is missing an owner principal'}}
}

function Assert-RelayFileAcl {
    param([Parameter(Mandatory)][string]$Path)
    $full=Assert-RelayPathSafe $Path;if(-not(Test-Path -LiteralPath $full -PathType Leaf)){throw 'Relay file does not exist'}
    $acl=Get-Acl -LiteralPath $full;$allowed=@([Security.Principal.WindowsIdentity]::GetCurrent().User.Value,'S-1-5-18','S-1-5-32-544')
    $owner=$acl.Owner;if($owner){$owner=([Security.Principal.NTAccount]$owner).Translate([Security.Principal.SecurityIdentifier]).Value};if($owner-notin$allowed){throw 'Relay file owner is unauthorized'}
    $writeMask=[Security.AccessControl.FileSystemRights]([int64][Security.AccessControl.FileSystemRights]::Write-bor[int64][Security.AccessControl.FileSystemRights]::Modify-bor[int64][Security.AccessControl.FileSystemRights]::FullControl-bor[int64][Security.AccessControl.FileSystemRights]::Delete-bor[int64][Security.AccessControl.FileSystemRights]::ChangePermissions-bor[int64][Security.AccessControl.FileSystemRights]::TakeOwnership)
    foreach($rule in @($acl.Access)){$sid=$rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value;if($rule.AccessControlType-eq[Security.AccessControl.AccessControlType]::Allow-and$sid-notin$allowed-and($rule.FileSystemRights-band$writeMask)){throw 'Relay file ACL grants write access to an unauthorized principal'}}
    return $full
}

function Write-RelayJsonAtomic {
    param([Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)]$Value,[switch]$VerifyAcl)
    $directory = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    $temporary = Join-Path $directory ('.{0}.{1}.tmp' -f ([IO.Path]::GetFileName($Path)),[Guid]::NewGuid().ToString('N'))
    $backup = Join-Path $directory ('.{0}.{1}.bak' -f ([IO.Path]::GetFileName($Path)),[Guid]::NewGuid().ToString('N'))
    try {
        $json = $Value | ConvertTo-Json -Depth 16 -Compress
        [IO.File]::WriteAllText($temporary,$json,[Text.UTF8Encoding]::new($false))
        if(Test-Path -LiteralPath $Path -PathType Leaf){[IO.File]::Replace($temporary,$Path,$backup,$true);Remove-Item -LiteralPath $backup -Force}
        else{Move-Item -LiteralPath $temporary -Destination $Path}
        if($VerifyAcl){$null=Assert-RelayFileAcl $Path}
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
        if (Test-Path -LiteralPath $backup) { Remove-Item -LiteralPath $backup -Force }
    }
}

function Read-RelayJsonStrict {
    param([string]$Path,$Default,[string]$Kind='state',[switch]$VerifyAcl)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $Default }
    $file=Get-Item -LiteralPath $Path -Force
    if(($file.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0-or$file.Length-lt2-or$file.Length-gt$script:MaximumStateBytes){throw "Relay $Kind file is unsafe or oversized"}
    if($VerifyAcl){$null=Assert-RelayFileAcl $Path}
    try {
        $json=Get-Content -LiteralPath $Path -Raw -Encoding UTF8
        if((Get-Command ConvertFrom-Json).Parameters.ContainsKey('DateKind')){return $json|ConvertFrom-Json -DateKind String}
        return $json|ConvertFrom-Json
    }
    catch { throw "Relay $Kind JSON is invalid" }
}

function Get-RelayCommand {
    param([Parameter(Mandatory)]$Comment,[Parameter(Mandatory)][string]$Node)
    if ([string]$Comment.user.login -cne 'peca12968' -or [string]$Comment.author_association -cne 'OWNER') { return $null }
    $body = [string]$Comment.body
    if ([Text.Encoding]::UTF8.GetByteCount($body) -gt $script:MaximumCommandBytes) { return $null }
    $envelope=[regex]::Match($body,'\A\[MAIN_COMMAND\]\r?\n(?<content>[\s\S]+)\r?\n\[/MAIN_COMMAND\]\z')
    if(-not$envelope.Success-or[regex]::Matches($body,'(?m)^\[/?MAIN_COMMAND\]\r?$').Count-ne2){return $null}
    $content=$envelope.Groups['content'].Value
    $targetMatches = [regex]::Matches($content,'(?m)^TARGET:[ \t]*([A-Z][A-Z0-9_-]{0,31})[ \t]*\r?$')
    $idMatches = [regex]::Matches($content,'(?m)^COMMAND_ID:[ \t]*([A-Z0-9][A-Z0-9._-]{0,127})[ \t]*\r?$')
    $ownedMatches=[regex]::Matches($content,'(?m)^OWNED_FILES:[ \t]*(\S[^\r\n]*?)[ \t]*\r?$')
    if ($targetMatches.Count -ne 1 -or $idMatches.Count -ne 1-or$ownedMatches.Count-ne1) { return $null }
    $target = $targetMatches[0].Groups[1].Value
    if ($target -cne 'ALL' -and $target -cne $Node) { return $null }
    $commandId = $idMatches[0].Groups[1].Value
    $headerEnd = $targetMatches[0].Index + $targetMatches[0].Length
    $idEnd = $idMatches[0].Index + $idMatches[0].Length
    $ownedEnd=$ownedMatches[0].Index+$ownedMatches[0].Length
    if ($content.Substring([Math]::Max([Math]::Max($headerEnd,$idEnd),$ownedEnd)).Trim().Length -eq 0) { return $null }
    $owned=[Collections.Generic.List[string]]::new();foreach($value in @($ownedMatches[0].Groups[1].Value-split'[,;]')){$path=$value.Trim().Replace('\','/');if(-not$path-or[IO.Path]::IsPathRooted($path)-or$path-match'(^|/)\.\.(/|$)|(^|/)\.git(/|$)' -or $path-notmatch'^[A-Za-z0-9._/*?\[\]-]+$'){return $null};$owned.Add($path)}
    if(-not$owned.Count){return $null}
    [pscustomobject]@{ CommentId=[uint64]$Comment.id; CommandId=$commandId; Target=$target; Body=$body;OwnedFiles=@($owned) }
}

function ConvertFrom-GitHubCommentPages {
    param([Parameter(Mandatory)][AllowEmptyCollection()][string[]]$Raw)
    $json=(@($Raw)-join"`n").Trim()
    if(-not$json-or$json[0]-cne'['-or$json[$json.Length-1]-cne']'){throw 'GitHub comments response was not a complete JSON document'}
    try{$document=$json|ConvertFrom-Json}catch{throw 'GitHub comments response was invalid JSON'}
    $comments=[Collections.Generic.List[object]]::new()
    foreach($page in @($document)){if($page-isnot[Array]){throw 'GitHub comments response page was invalid'};foreach($comment in @($page)){if($null-eq$comment-or$comment-is[string]-or$comment-is[Array]){throw 'GitHub comments response page was invalid'};$comments.Add($comment)}}
    return @($comments)
}

function Get-DefaultComments {
    param([string]$Repository,[uint32]$Issue,[string]$Since)
    # --slurp 只把逐頁原生 JSON 陣列包成一個外層陣列，不依賴不同 gh 版本的 Go template 函式集合。
    $raw = & gh api --method GET ("repos/{0}/issues/{1}/comments" -f $Repository,$Issue) -f since=$Since -f per_page=100 --paginate --slurp 2>$null
    if ($LASTEXITCODE -ne 0) { throw 'GitHub comments API failed' }
    return @(ConvertFrom-GitHubCommentPages @($raw))
}

function Send-DefaultAck {
    param([string]$Repository,[uint32]$Issue,[string]$Body)
    $null = & gh api ("repos/{0}/issues/{1}/comments" -f $Repository,$Issue) -f body=$Body 2>$null
    if ($LASTEXITCODE -ne 0) { throw 'GitHub ACK API failed' }
}

function Resolve-DesktopCodex {
    $candidates = [Collections.Generic.List[string]]::new()
    if(-not$env:LOCALAPPDATA){throw 'Codex desktop root is unavailable'}
    $desktopBin=Assert-RelayPathSafe (Join-Path $env:LOCALAPPDATA 'OpenAI\Codex\bin')
    if(Test-Path -LiteralPath $desktopBin -PathType Container){foreach($directory in @(Get-ChildItem -LiteralPath $desktopBin -Directory -Force -ErrorAction Stop|Sort-Object LastWriteTime -Descending)){$safeDirectory=Assert-RelayPathSafe $directory.FullName;$candidate=Join-Path $safeDirectory 'codex.exe';if(Test-Path -LiteralPath $candidate -PathType Leaf){$candidates.Add($candidate)}}}
    foreach ($candidate in $candidates) {
        $safe=Assert-RelayPathSafe $candidate
        if ($safe -and (Test-Path -LiteralPath $safe -PathType Leaf)) {$signature=Get-AuthenticodeSignature -LiteralPath $safe;if($signature.Status-eq[Management.Automation.SignatureStatus]::Valid-and$signature.SignerCertificate.Subject-match'(?i)\bO="?OpenAI OpCo, LLC"?'){return $safe}}
    }
    throw 'A trusted OpenAI-signed Codex desktop executable was not found'
}

function Test-CodexLogin {
    param([Parameter(Mandatory)][string]$CodexPath)
    $text = & $CodexPath login status 2>&1
    if ($LASTEXITCODE -ne 0 -or (($text -join "`n") -notmatch '(?i)logged in|authenticated')) { throw 'Codex is not logged in' }
}

function New-RelayPrompt {
    param([Parameter(Mandatory)]$Command,[Parameter(Mandatory)][string]$Node)
    @"
你是 Aevrynd $Node 副機的受限執行代理。以下命令已由 GitHub repository owner 驗證。

強制政策：
- 只處理命令明確指定的工作與檔案；不得擴張範圍。
- 禁止任何 Git 寫操作，包括 commit、push、merge、rebase、reset、clean、checkout 與建立分支。
- 不得讀取、輸出或傳送 token、密碼、Cookie、憑證、環境秘密或私人金鑰。
- 不得修改排程、relay 狀態、系統安全設定或 tools/codex_command_relay。
- 遇到缺少授權、路徑越界或秘密時立即停止並以一般文字回報。
- 可在 C:\Aevrynd\Games workspace 內完成命令授權的來源修改與局部驗證。

NODE: $Node
COMMAND_ID: $($Command.CommandId)
VERIFIED_COMMAND:
$($Command.Body)
"@
}

function Invoke-DefaultCodex {
    param([string]$CodexPath,[string[]]$Arguments,[string]$Prompt,[string]$OutputPath)
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $Prompt | & $CodexPath @Arguments *> $OutputPath
        return $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousErrorAction }
}

function Get-RelayFileHash {
    param([string]$Path)
    if(-not(Test-Path -LiteralPath $Path -PathType Leaf)){return ''};return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function ConvertFrom-RelayGitPath {
    param([string]$Value)
    $path=$Value.Trim()
    if($path.Length-ge2-and$path[0]-eq'"'-and$path[$path.Length-1]-eq'"'){
        try{$path=$path|ConvertFrom-Json}catch{throw 'Git status contained an invalid quoted path'}
    }
    return ([string]$path).Replace('\','/')
}

function Get-RelayPorcelainPaths {
    param([string]$Row)
    if($Row.Length-lt4){return @()}
    $payload=$Row.Substring(3)
    if($Row[0]-in@('R','C')-or$Row[1]-in@('R','C')){
        $rename=[regex]::Match($payload,'\A(?<source>"(?:\\.|[^"])*"|.*?) -> (?<destination>"(?:\\.|[^"])*"|.*)\z')
        if(-not$rename.Success){throw 'Git status contained an invalid rename'}
        return @((ConvertFrom-RelayGitPath $rename.Groups['source'].Value),(ConvertFrom-RelayGitPath $rename.Groups['destination'].Value))
    }
    return @((ConvertFrom-RelayGitPath $payload))
}

function Get-GitBoundarySnapshot {
    param([string]$Workspace)
    $root=Assert-RelayPathSafe $Workspace
    $head=(& git -C $root rev-parse HEAD 2>$null)-join'';if($LASTEXITCODE-ne0-or$head-notmatch'^[0-9a-fA-F]{40,64}$'){throw 'Git HEAD audit failed'}
    $status=@(& git -C $root status --porcelain=v1 --untracked-files=all 2>$null);if($LASTEXITCODE-ne0){throw 'Git worktree audit failed'}
    $gitPath=(& git -C $root rev-parse --absolute-git-dir 2>$null)-join'';if($LASTEXITCODE-ne0){throw 'Git metadata audit failed'};$gitPath=Assert-RelayPathSafe $gitPath
    # hooks、objects、logs、info 等整個 metadata tree 都屬於禁止變更面。
    $metadata=[Collections.Generic.List[object]]::new();$pending=[Collections.Generic.Queue[string]]::new();$pending.Enqueue($gitPath)
    while($pending.Count){$directory=$pending.Dequeue();$null=Assert-RelayPathSafe $directory;foreach($item in @(Get-ChildItem -LiteralPath $directory -Force -ErrorAction Stop)){$null=Assert-RelayPathSafe $item.FullName;if(($item.Attributes-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'Git metadata contains a reparse point'};if($item.PSIsContainer){$pending.Enqueue($item.FullName)}else{$metadata.Add([pscustomobject]@{path=$item.FullName.Substring($gitPath.Length).TrimStart('\').Replace('\','/');sha256=Get-RelayFileHash $item.FullName})}}}
    $metadata=@($metadata|Sort-Object path)
    $details=[Collections.Generic.List[object]]::new();foreach($row in $status){foreach($path in @(Get-RelayPorcelainPaths $row)){$full=[IO.Path]::GetFullPath((Join-Path $root $path));if(-not$full.StartsWith(($root.TrimEnd('\')+'\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Git status returned an out-of-root path'};$hash=if(Test-Path -LiteralPath $full -PathType Leaf){Get-RelayFileHash $full}else{''};$details.Add([pscustomobject]@{path=$path;status=$row.Substring(0,2);sha256=$hash})}}
    return [pscustomobject]@{head=$head.ToLowerInvariant();status=@($status);worktree=@($details);metadata=@($metadata)}
}

function Test-RelayOwnedPath {
    param([string]$Path,[string[]]$OwnedFiles)
    $value=$Path.Replace('\','/');if($value.Contains(' -> ')){$value=($value-split' -> ')[-1]};$value=$value.Trim('"')
    foreach($pattern in $OwnedFiles){$regex=[regex]::Escape($pattern).Replace('\*\*','.*').Replace('\*','[^/]*').Replace('\?','[^/]');if($value-match("\A$regex\z")){return $true}}
    return $false
}

function Compare-GitBoundarySnapshot {
    param($Before,$After,[string[]]$OwnedFiles)
    $violations=[Collections.Generic.List[string]]::new();if($Before.head-cne$After.head){$violations.Add('HEAD changed')}
    if((ConvertTo-Json $Before.metadata -Depth 5 -Compress)-cne(ConvertTo-Json $After.metadata -Depth 5 -Compress)){$violations.Add('Git metadata changed')}
    $beforeRows=@{};foreach($row in @($Before.status)){foreach($path in @(Get-RelayPorcelainPaths $row)){$beforeRows[$path]=$row.Substring(0,2)}}
    $afterRows=@{};foreach($row in @($After.status)){foreach($path in @(Get-RelayPorcelainPaths $row)){$afterRows[$path]=$row.Substring(0,2)}};foreach($path in @($beforeRows.Keys+$afterRows.Keys|Sort-Object -Unique)){$old=if($beforeRows.ContainsKey($path)){$beforeRows[$path]}else{$null};$current=if($afterRows.ContainsKey($path)){$afterRows[$path]}else{$null};if($old-cne$current-and-not(Test-RelayOwnedPath $path $OwnedFiles)){$violations.Add("Out-of-scope path changed: $path")}}
    $beforeDetails=@{};$afterDetails=@{};foreach($item in @($Before.worktree)){$beforeDetails[[string]$item.path]="$( $item.status)|$( $item.sha256)"};foreach($item in @($After.worktree)){$afterDetails[[string]$item.path]="$( $item.status)|$( $item.sha256)"};foreach($path in @($beforeDetails.Keys+$afterDetails.Keys|Sort-Object -Unique)){$old=if($beforeDetails.ContainsKey($path)){$beforeDetails[$path]}else{$null};$current=if($afterDetails.ContainsKey($path)){$afterDetails[$path]}else{$null};if($old-cne$current-and-not(Test-RelayOwnedPath $path $OwnedFiles)){$message="Out-of-scope content changed: $path";if($message-notin$violations){$violations.Add($message)}}}
    return [pscustomobject]@{passed=$violations.Count-eq0;violations=@($violations)}
}

function New-InitialRelayState([string]$Node) { [pscustomobject]@{ schemaVersion=$script:RelaySchemaVersion; node=$Node; cursor=[uint64]0; cursorCreatedAt=$script:EpochCursor; operations=@() } }

function Test-RelayUtcTimestamp {
    param($Value)
    if($Value-isnot[string]){return $false}
    try{$parsed=[DateTimeOffset]::ParseExact($Value,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind);return $parsed.Offset-eq[TimeSpan]::Zero}catch{return $false}
}

function Assert-RelayState {
    param($State,[string]$Node)
    if($null-eq$State){throw 'Relay state schema or permanent node binding is invalid'}
    $names=@($State.PSObject.Properties.Name|Sort-Object)
    if(($names-join',')-cne'cursor,cursorCreatedAt,node,operations,schemaVersion'){throw 'Relay state schema or permanent node binding is invalid'}
    if([int]$State.schemaVersion-ne1-or[string]$State.node-cne$Node-or$State.cursorCreatedAt-isnot[string]-or@($State.operations).Count-gt$script:MaximumOperations){throw 'Relay state schema or permanent node binding is invalid'}
    try{$null=[uint64]$State.cursor}catch{throw 'Relay state cursor is invalid'}
    try{$null=[DateTimeOffset]::ParseExact([string]$State.cursorCreatedAt,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw 'Relay state cursor timestamp is invalid'}
    $ids=@{};foreach($item in @($State.operations)){
        $propertyNames=@($item.PSObject.Properties.Name|Sort-Object)
        if(($propertyNames-join',')-cne'ackAttempted,ackSucceeded,commandId,commentId,exitCode,finishedAt,node,outputPath,startedAt,state,summary,target'){throw 'Relay state operation schema is invalid'}
        if($item.commandId-isnot[string]-or$item.commandId-notmatch'^[A-Z0-9][A-Z0-9._-]{0,127}$'-or$ids.ContainsKey($item.commandId)-or$item.node-isnot[string]-or$item.node-cne$Node-or$item.target-isnot[string]-or$item.target-notin@($Node,'ALL')-or$item.state-isnot[string]-or$item.state-notin@('running','completed','failed')){throw 'Relay state operation identity is invalid'}
        if($item.commentId-is[bool]-or$item.commentId-is[string]){throw 'Relay state comment id is invalid'};try{$commentId=[uint64]$item.commentId;if($commentId-eq0-or[decimal]$commentId-ne[decimal]$item.commentId){throw 'invalid'}}catch{throw 'Relay state comment id is invalid'}
        if(-not(Test-RelayUtcTimestamp $item.startedAt)){throw 'Relay state start timestamp is invalid'}
        if($item.ackAttempted-isnot[bool]-or$item.ackSucceeded-isnot[bool]-or($item.ackSucceeded-and-not$item.ackAttempted)){throw 'Relay state ACK flags are invalid'}
        if($item.summary-isnot[string]-or$item.summary-notmatch'^[\x20-\x7E]{1,240}$'){throw 'Relay state summary is invalid'}
        if($null-ne$item.outputPath-and($item.outputPath-isnot[string]-or$item.outputPath.Length-gt1024-or$item.outputPath-notmatch'^[^\x00-\x1F]+$'-or-not[IO.Path]::IsPathRooted($item.outputPath))){throw 'Relay state output path is invalid'}
        if($item.state-ceq'running'){
            if($null-ne$item.finishedAt-or$null-ne$item.exitCode){throw 'Relay running state is inconsistent'}
        }else{
            if(-not(Test-RelayUtcTimestamp $item.finishedAt)-or$item.outputPath-isnot[string]-or[string]::IsNullOrWhiteSpace($item.outputPath)){throw 'Relay terminal state is inconsistent'}
            if($item.exitCode-is[bool]-or$item.exitCode-is[string]){throw 'Relay exit code is invalid'};try{$exitCode=[int]$item.exitCode;if($exitCode-lt0-or$exitCode-gt65535-or[decimal]$exitCode-ne[decimal]$item.exitCode){throw 'invalid'}}catch{throw 'Relay exit code is invalid'}
            if(($item.state-ceq'completed'-and$exitCode-ne0)-or($item.state-ceq'failed'-and$exitCode-eq0)){throw 'Relay terminal exit code is inconsistent'}
        }
        $ids[$item.commandId]=$true
    }
    return $State
}

function Read-PublisherHistory {
    param([string]$Path,[switch]$VerifyAcl)
    $value=Read-RelayJsonStrict $Path ([pscustomobject]@{schemaVersion=1;events=@()}) 'history' -VerifyAcl:$VerifyAcl
    $names=@($value.PSObject.Properties.Name|Sort-Object)
    if(($names-join',')-cne'events,schemaVersion'-or$value.schemaVersion-ne1-or@($value.events).Count-gt$script:MaximumOperations){throw 'Relay history schema is invalid'}
    foreach($event in @($value.events)){
        $required=@('name','type','startedAt','updatedAt','endedAt','status','evidenceSummary');foreach($name in $required){if($null-eq$event.PSObject.Properties[$name]){throw 'Relay history event is invalid'}}
        if((@($event.PSObject.Properties.Name|Sort-Object)-join',')-cne'endedAt,evidenceSummary,name,startedAt,status,type,updatedAt'){throw 'Relay history event schema is invalid'}
        if([string]$event.name-notmatch'^[\x20-\x7E]{1,80}$'-or[string]$event.type-notmatch'^[A-Za-z0-9._-]{1,40}$'-or[string]$event.evidenceSummary-notmatch'^[\x20-\x7E]{1,240}$'-or$event.status-notin@('completed','failed','cancelled')){throw 'Relay history event fields are invalid'}
        foreach($stamp in @($event.startedAt,$event.updatedAt,$event.endedAt)){try{$null=[DateTimeOffset]::ParseExact([string]$stamp,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw 'Relay history timestamp is invalid'}}
    }
    return $value
}

function New-RelayTaskName {
    param([string]$Node,[string]$CommandId)
    $value="Codex relay $Node $CommandId";if($value.Length-le80){return $value}
    $sha=[Security.Cryptography.SHA256]::Create();try{$hash=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($CommandId)))).Replace('-','').Substring(0,12).ToLowerInvariant()}finally{$sha.Dispose()}
    return ($value.Substring(0,67)+'-'+$hash)
}

function ConvertTo-RelaySafeFileId {
    param([string]$CommandId)
    $value=$CommandId-replace'[^A-Za-z0-9._-]','_';if($value.Length-le48){return $value}
    $sha=[Security.Cryptography.SHA256]::Create();try{$hash=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($CommandId)))).Replace('-','').Substring(0,12).ToLowerInvariant()}finally{$sha.Dispose()}
    return ($value.Substring(0,35)+'-'+$hash)
}

function ConvertTo-RelaySafeSummary {
    param([string]$Value)
    $safe=[regex]::Replace([string]$Value,'[^\x20-\x7E]','?');if($safe.Length-gt240){$safe=$safe.Substring(0,240)};if(-not$safe){$safe='Relay result unavailable'};return $safe
}

function Get-RelayNewComments {
    param($State,[string]$Repository,[uint32]$Issue,[scriptblock]$GetComments)
    try{$cursorTime=[DateTimeOffset]::ParseExact([string]$State.cursorCreatedAt,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind);$source=@($(if($GetComments){&$GetComments $Repository $Issue $State.cursorCreatedAt}else{Get-DefaultComments $Repository $Issue $State.cursorCreatedAt}))}catch{throw 'GitHub comments API failed'}
    $rows=[Collections.Generic.List[object]]::new()
    foreach($comment in $source){try{$id=[uint64]$comment.id;$created=[DateTimeOffset]::Parse([string]$comment.created_at,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{throw 'GitHub comments response was invalid'};if($created-gt$cursorTime-or($created-eq$cursorTime-and$id-gt[uint64]$State.cursor)){$comment|Add-Member -NotePropertyName _relayCreated -NotePropertyValue $created -Force;$rows.Add($comment)}}
    return @($rows|Sort-Object @{Expression={$_._relayCreated}},@{Expression={[uint64]$_.id}})
}

function Invoke-CodexCommandRelay {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][ValidatePattern('^[A-Z][A-Z0-9_-]{0,31}$')][string]$Node,
        [string]$Repository='peca12968/Aevrynd',[uint32]$Issue=1,
        [string]$StateRoot='C:\ProgramData\Aevrynd\Codex',
        [string]$AuditRoot='C:\AevryndAudit\codex-relay',
        [string]$Workspace='C:\Aevrynd\Games',
        [switch]$BaselineOnly,[switch]$WhatIf,[bool]$RequireSecureAcl=$true,
        [scriptblock]$GetComments,[scriptblock]$SendAck,[scriptblock]$ResolveCodex,[scriptblock]$CheckLogin,[scriptblock]$RunCodex,[scriptblock]$InspectCodexPlan,[scriptblock]$CaptureBoundary,[scriptblock]$CompareBoundary
    )
    $StateRoot=Assert-RelayPathSafe $StateRoot;$AuditRoot=Assert-RelayPathSafe $AuditRoot
    $statePath = Join-Path $StateRoot 'relay-state.json'
    $statusPath = Join-Path $StateRoot 'status.json'
    $historyPath = Join-Path $StateRoot 'operations-history.json'
    # 所有節點共用鎖；relay-state 的永久 node binding 才不會在首次建立時競態。
    $lockPath = Join-Path $StateRoot 'relay.lock'
    if($WhatIf){
        $state=Assert-RelayState (Read-RelayJsonStrict $statePath (New-InitialRelayState $Node) 'state' -VerifyAcl:$RequireSecureAcl) $Node
        $null=Read-PublisherHistory $historyPath -VerifyAcl:$RequireSecureAcl
        try{$comments=@(Get-RelayNewComments $state $Repository $Issue $GetComments)}catch{return}
        $known=@{};foreach($item in @($state.operations)){$known[[string]$item.commandId]=$true};$command=$null
        foreach($comment in $comments){$candidate=Get-RelayCommand $comment $Node;if($candidate-and-not$known.ContainsKey($candidate.CommandId)){$command=$candidate;break}}
        if(-not$command){return}
        $safeId=ConvertTo-RelaySafeFileId $command.CommandId;$outputPath=Join-Path (Join-Path $AuditRoot $Node.ToLowerInvariant()) ("{0}-{1}.log"-f$command.CommentId,$safeId)
        $arguments=@('exec','--ephemeral','-m','gpt-5.6-luna','-c','model_reasoning_effort="medium"','-s','workspace-write','--approve-for-me','-C',$Workspace);$prompt=New-RelayPrompt $command $Node
        if($InspectCodexPlan){&$InspectCodexPlan $arguments $prompt $outputPath}
        return [pscustomobject][ordered]@{commandId=$command.CommandId;commentId=$command.CommentId;codexArguments=$arguments;outputPath=$outputPath;ackBody="[NODE_ACK]`nNODE: $Node`nSTATUS: RUNNING`nCOMMAND_ID: $($command.CommandId)"}
    }
    if($RequireSecureAcl){if(-not(Test-Path -LiteralPath $StateRoot -PathType Container)-or-not(Test-Path -LiteralPath $AuditRoot -PathType Container)){throw 'Secure relay directories must already exist'};Assert-RelayDirectoryAcl $StateRoot;Assert-RelayDirectoryAcl $AuditRoot}
    else{New-Item -ItemType Directory -Path $StateRoot -Force|Out-Null}
    $lock = $null
    if(Test-Path -LiteralPath $lockPath){$null=Assert-RelayPathSafe $lockPath;if($RequireSecureAcl){$null=Assert-RelayFileAcl $lockPath}}
    try { $lock = [IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None) }
    catch [IO.IOException] { return }
    try {
        if($RequireSecureAcl){$null=Assert-RelayFileAcl $lockPath}
        $state = Assert-RelayState (Read-RelayJsonStrict $statePath (New-InitialRelayState $Node) 'state' -VerifyAcl:$RequireSecureAcl) $Node
        # 歷史檔也是信任邊界；若損毀，必須在 claim/ACK/模型前停止。
        $publisherHistory=Read-PublisherHistory $historyPath -VerifyAcl:$RequireSecureAcl
        if(Test-Path -LiteralPath $statusPath -PathType Leaf){$null=Read-RelayJsonStrict $statusPath $null 'status' -VerifyAcl:$RequireSecureAcl}
        $operations = [Collections.Generic.List[object]]::new()
        foreach ($item in @($state.operations)) { $operations.Add($item) }
        try { $comments = @(Get-RelayNewComments $state $Repository $Issue $GetComments) }
        catch { if ($BaselineOnly) { throw }; return }
        $maximumComment=if(@($comments).Count){@($comments)[-1]}else{$null}
        if ($BaselineOnly) {
            if($maximumComment){$state.cursor=[uint64]$maximumComment.id;$state.cursorCreatedAt=$maximumComment._relayCreated.ToString('o')};$state.operations=@($operations)
            Write-RelayJsonAtomic $statePath $state -VerifyAcl:$RequireSecureAcl
            return
        }
        $known = @{}; foreach ($item in $operations) { $known[[string]$item.commandId]=$true }
        $commands = [Collections.Generic.List[object]]::new()
        foreach ($comment in $comments) {
            $command = Get-RelayCommand $comment $Node
            if ($command -and -not $known.ContainsKey($command.CommandId)) { $commands.Add($command);break }
        }
        # 成功讀取後才前移游標；API 失敗時上方直接返回。
        # 每輪只 claim 最舊的一筆合法命令，避免程序崩潰時較新的未執行命令被游標跨過。
        $selectedComment=if($commands.Count){$comments|Where-Object{[uint64]$_.id-eq[uint64]$commands[0].CommentId}|Select-Object -First 1}else{$maximumComment}
        if($selectedComment){$state.cursor=[uint64]$selectedComment.id;$state.cursorCreatedAt=$selectedComment._relayCreated.ToString('o')}
        if ($commands.Count -eq 0) { $state.operations=@($operations | Select-Object -Last $script:MaximumOperations);Write-RelayJsonAtomic $statePath $state -VerifyAcl:$RequireSecureAcl;return }
        foreach ($command in $commands) {
            $now=[DateTimeOffset]::UtcNow.ToString('o')
            $operation=[pscustomobject][ordered]@{commandId=$command.CommandId;commentId=$command.CommentId;node=$Node;target=$command.Target;state='running';startedAt=$now;finishedAt=$null;exitCode=$null;ackAttempted=$false;ackSucceeded=$false;outputPath=$null;summary='Command claimed before execution'}
            $operations.Add($operation)
            while ($operations.Count -gt $script:MaximumOperations) { $operations.RemoveAt(0) }
            $state.operations=@($operations);Write-RelayJsonAtomic $statePath $state -VerifyAcl:$RequireSecureAcl
            $taskName=New-RelayTaskName $Node $command.CommandId
            Write-RelayJsonAtomic $statusPath ([ordered]@{schemaVersion=1;task=$taskName;status='working';startedUtc=$now;updatedUtc=$now;ttl=3600}) -VerifyAcl:$RequireSecureAcl
            $ack="[NODE_ACK]`nNODE: $Node`nSTATUS: RUNNING`nCOMMAND_ID: $($command.CommandId)"
            $operation.ackAttempted=$true
            $state.operations=@($operations);Write-RelayJsonAtomic $statePath $state -VerifyAcl:$RequireSecureAcl
            try { if ($SendAck) { & $SendAck $Repository $Issue $ack } else { Send-DefaultAck $Repository $Issue $ack };$operation.ackSucceeded=$true } catch {$operation.summary='ACK failed before Codex execution'}
            $state.operations=@($operations);Write-RelayJsonAtomic $statePath $state -VerifyAcl:$RequireSecureAcl
            $outputDirectory=Join-Path $AuditRoot $Node.ToLowerInvariant();New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null;if($RequireSecureAcl){Assert-RelayDirectoryAcl $outputDirectory}
            $safeId=ConvertTo-RelaySafeFileId $command.CommandId;$outputPath=Join-Path $outputDirectory ("{0}-{1}.log" -f $command.CommentId,$safeId);$boundaryPath=Join-Path $outputDirectory ("{0}-{1}-boundary.json"-f$command.CommentId,$safeId);$operation.outputPath=$outputPath
            if(Test-Path -LiteralPath $boundaryPath -PathType Leaf){$null=Read-RelayJsonStrict $boundaryPath $null 'boundary' -VerifyAcl:$RequireSecureAcl}
            $arguments=@('exec','--ephemeral','-m','gpt-5.6-luna','-c','model_reasoning_effort="medium"','-s','workspace-write','--approve-for-me','-C',$Workspace)
            $prompt=New-RelayPrompt $command $Node
            $exitCode=0;$beforeBoundary=$null;$afterBoundary=$null;$boundaryResult=$null
            try {
                $null=Assert-RelayPathSafe $outputPath
                if(Test-Path -LiteralPath $outputPath){if($RequireSecureAcl){$null=Assert-RelayFileAcl $outputPath};throw 'Output log collision detected'}
                $beforeBoundary=if($CaptureBoundary){&$CaptureBoundary 'before' $Workspace}else{Get-GitBoundarySnapshot $Workspace}
                $codexPath = if ($ResolveCodex) { & $ResolveCodex } else { Resolve-DesktopCodex }
                if ($CheckLogin) { & $CheckLogin $codexPath } else { Test-CodexLogin $codexPath }
                $exitCode = if ($RunCodex) { & $RunCodex $codexPath $arguments $prompt $outputPath } else { Invoke-DefaultCodex $codexPath $arguments $prompt $outputPath }
                if ($null -eq $exitCode) { $exitCode=0 }
            } catch { $exitCode=1;$operation.state='failed';$operation.summary='Relay execution failed' }
            if($beforeBoundary){
                try{
                    $afterBoundary=if($CaptureBoundary){&$CaptureBoundary 'after' $Workspace}else{Get-GitBoundarySnapshot $Workspace}
                    $boundaryResult=if($CompareBoundary){&$CompareBoundary $beforeBoundary $afterBoundary $command.OwnedFiles}else{Compare-GitBoundarySnapshot $beforeBoundary $afterBoundary $command.OwnedFiles}
                    if($null-eq$boundaryResult-or$null-eq$boundaryResult.passed){throw 'Boundary comparer returned an invalid result'}
                    Write-RelayJsonAtomic $boundaryPath ([ordered]@{schemaVersion=1;commandId=$command.CommandId;node=$Node;passed=[bool]$boundaryResult.passed;violations=@($boundaryResult.violations);beforeHead=[string]$beforeBoundary.head;afterHead=[string]$afterBoundary.head}) -VerifyAcl:$RequireSecureAcl
                    if(-not$boundaryResult.passed){$exitCode=90;$operation.state='failed';$operation.summary=('Boundary audit failed: '+(@($boundaryResult.violations)-join'; '))}
                }catch{$exitCode=91;$operation.state='failed';$operation.summary=('Boundary audit could not be completed: '+$_.Exception.Message)}
            }
            if($operation.state-ceq'running'){$operation.state=if([int]$exitCode-eq0){'completed'}else{'failed'};$operation.summary=if([int]$exitCode-eq0){if($operation.ackSucceeded){'Codex completed'}else{'ACK failed; Codex completed'}}else{if($operation.ackSucceeded){'Codex failed'}else{'ACK failed; Codex failed'}}}
            $operation.summary=ConvertTo-RelaySafeSummary $operation.summary
            $operation.exitCode=[int]$exitCode;$operation.finishedAt=[DateTimeOffset]::UtcNow.ToString('o')
            $state.operations=@($operations);Write-RelayJsonAtomic $statePath $state -VerifyAcl:$RequireSecureAcl
            $events=[Collections.Generic.List[object]]::new();foreach($event in @($publisherHistory.events)){$events.Add($event)}
            $publisherStatus=if($operation.state -ceq 'completed'){'completed'}else{'failed'}
            $events.Add([pscustomobject][ordered]@{name=$taskName;type='codex-command-relay';startedAt=$operation.startedAt;updatedAt=$operation.finishedAt;endedAt=$operation.finishedAt;status=$publisherStatus;evidenceSummary=$operation.summary})
            while($events.Count -gt $script:MaximumOperations){$events.RemoveAt(0)}
            Write-RelayJsonAtomic $historyPath ([ordered]@{schemaVersion=1;events=@($events)}) -VerifyAcl:$RequireSecureAcl
            if(Test-Path -LiteralPath $statusPath -PathType Leaf){Remove-Item -LiteralPath $statusPath -Force}
        }
    } finally { if ($lock) {$lock.Dispose()} }
}

Export-ModuleMember -Function Invoke-CodexCommandRelay,Get-RelayCommand,Write-RelayJsonAtomic,Assert-RelayPathSafe,Protect-RelayDirectoryAcl,Assert-RelayDirectoryAcl,Assert-RelayFileAcl,Compare-GitBoundarySnapshot
