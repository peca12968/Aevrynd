#requires -Version 7.0
function Read-MistwatchJointReceiptText([string]$Text){
    $Result=@{}
    foreach($Line in $Text -split '\r?\n'){
        if([string]::IsNullOrWhiteSpace($Line)){continue}
        $Pair=$Line.Split('=',2)
        if($Pair.Count -ne 2 -or $Pair[0] -cnotmatch '^[a-z][a-z0-9_]*$'){throw 'Malformed joint receipt line'}
        if($Result.ContainsKey($Pair[0])){throw ('Duplicate joint receipt field: '+$Pair[0])}
        $Result[$Pair[0]]=$Pair[1]
    }
    return $Result
}
function Read-MistwatchJointReceipt([string]$Path){
    return Read-MistwatchJointReceiptText ([IO.File]::ReadAllText($Path))
}
function Assert-MistwatchJointField([Collections.IDictionary]$Fields,[string]$Name,[string]$Expected,[bool]$Numeric){
    if(-not $Fields.Contains($Name) -or $Fields[$Name] -isnot [string]){throw ('Missing/non-string joint field: '+$Name)}
    $Value=$Fields[$Name]
    if($Numeric){
        [uint64]$Parsed=0
        if($Value -cnotmatch '^(0|[1-9][0-9]{0,19})$' -or
            -not [uint64]::TryParse($Value,[Globalization.NumberStyles]::None,[Globalization.CultureInfo]::InvariantCulture,[ref]$Parsed)){
            throw ('Noncanonical joint integer: '+$Name)
        }
    }
    if($Value -cne $Expected){throw ('Joint worksite contract mismatch: '+$Name)}
}
function Get-MistwatchJointRepairedScenes{
    # V176／V174 原 preset 與 Session::bridgeRepaired；不得以收據數值生成期望。
    return @('strike','boar-recovery','boar-profile','settlement-clue','settlement-clue-read',
        'fuse-lock-wrong','fuse-lock-ready','panel','powered','relay-west','relay-aligned','relay-linked',
        'complete','focus-pause','ranger-hut-profile')
}
function Assert-MistwatchJointWorksite([Collections.IDictionary]$Receipt,[Collections.IDictionary]$Cleanup,[string]$Scene,[bool]$Gpu){
    $Known=@('initial','repair-shelter','repair-player-camera','repair-ready','river','river-overlook','strike',
        'boar-recovery','boar-profile','settlement-clue','settlement-clue-read','fuse-lock-wrong','fuse-lock-ready',
        'panel','powered','relay-west','relay-aligned','relay-linked','complete','audio-check','focus-pause',
        'boulder-profile','grass-profile','fern-profile','supply-profile','supply-collected','ranger-hut-profile',
        'trail-profile','shrub-profile','canopy-profile','canopy-root','canopy-detail')
    if($Scene -cnotin $Known){throw 'Unknown or noncanonical joint scene'}
    $Repaired=$Scene -cin @(Get-MistwatchJointRepairedScenes)
    if(-not $Receipt.Contains('mission_stage') -or $Receipt.mission_stage -isnot [string] -or
        $Receipt.mission_stage -cnotmatch '^[0-8]$'){throw 'Invalid joint mission_stage type/range'}
    if(([int]$Receipt.mission_stage -ge 3) -ne $Repaired){throw 'Joint fixture mission/bridge state mismatch'}
    $Unrepaired=if($Repaired){'0'}else{'1'}
    $Submitted=if($Repaired){'0'}else{'2'}
    $Expected=@{
        bridge_worksite_visual='stacked_timber_tool_station';bridge_worksite_mesh_resources='2';
        bridge_worksite_wood_triangles='168';bridge_worksite_metal_triangles='36';
        bridge_worksite_fingerprint='550614854349441485';bridge_worksite_unrepaired=$Unrepaired;
        submitted_bridge_worksite=$Submitted;bridge_worksite_wood_textured='1';bridge_worksite_wood_use_uv='1';
        gpu_validation_requested=$(if($Gpu){'1'}else{'0'})
    }
    foreach($Name in $Expected.Keys){$null=Assert-MistwatchJointField $Receipt $Name $Expected[$Name] ($Name -cne 'bridge_worksite_visual')}
    $null=Assert-MistwatchJointField $Cleanup 'mesh_destroy_calls' '2' $true
    $null=Assert-MistwatchJointField $Cleanup 'cleanup' 'success' $false
    return @{scene=$Scene;repaired=$Repaired;unrepaired=[int]$Unrepaired;submitted=[int]$Submitted;cleanup='success';scope='Worksite fields only; original full validator remains required'}
}
function Assert-MistwatchJointPngBytes([byte[]]$Bytes){
    # 真 PNG 的 IHDR，不把 helper 字串／CPU fixture 當像素或 GPU 證據。
    if($Bytes.Length -le 24 -or [Convert]::ToHexString($Bytes[0..7]) -cne '89504E470D0A1A0A' -or
        [Convert]::ToHexString($Bytes[12..15]) -cne '49484452' -or [Convert]::ToHexString($Bytes[16..23]) -cne '00000500000002D0'){
        throw 'Joint worksite requires actual1280x720 PNG'
    }
}
function Assert-MistwatchJointWorksiteEvidence([string]$Capture,[string]$Scene,[bool]$Gpu){
    $null=Assert-MistwatchJointPngBytes ([IO.File]::ReadAllBytes($Capture))
    $Receipt=Read-MistwatchJointReceipt ($Capture+'.receipt.txt')
    $Cleanup=Read-MistwatchJointReceipt ($Capture+'.bridge-worksite-cleanup.txt')
    return Assert-MistwatchJointWorksite $Receipt $Cleanup $Scene $Gpu
}
