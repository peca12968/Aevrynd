param(
 [Parameter(Mandatory=$true)][string]$ExecutablePath,
 [Parameter(Mandatory=$true)][string]$EvidenceDirectory
)
$ErrorActionPreference='Stop'
$exe=(Resolve-Path -LiteralPath $ExecutablePath).Path
$root=[IO.Path]::GetFullPath($EvidenceDirectory)
if(Test-Path -LiteralPath $root){throw '保留既有草距離證據'}
New-Item -ItemType Directory -Path $root | Out-Null
$hash=(Get-FileHash -LiteralPath $exe).Hash
$names=@(@(Get-ChildItem Env: | Where-Object Name -like 'AEVRYND_*' | Select-Object -ExpandProperty Name)+@(
 'AEVRYND_MISTWATCH_CAPTURE','AEVRYND_MISTWATCH_SCENE','AEVRYND_MISTWATCH_GRASS_FIXTURE','AEVRYND_MISTWATCH_GRASS_FIXTURE_ENABLED') | Sort-Object -Unique)
$saved=@{};foreach($n in $names){$saved[$n]=[Environment]::GetEnvironmentVariable($n,'Process');[Environment]::SetEnvironmentVariable($n,$null,'Process')}
$rows=@()
$plan=@(
 @{mode='player-near';label='player-near';player=2;camera=12;selected=1},
 @{mode='player-inside';label='player-inside';player=34.99;camera=12;selected=1},
 @{mode='player-edge';label='player-edge';player=35;camera=12;selected=1},
 @{mode='player-outside';label='player-outside';player=35.01;camera=12;selected=0},
 @{mode='image-near';label='image-2';player=2;camera=2;selected=1},
 @{mode='image-middle';label='image-12';player=2;camera=12;selected=1},
 @{mode='image-far';label='image-35';player=2;camera=35;selected=1})
try {
 $env:AEVRYND_MISTWATCH_SCENE='grass-distance'
 foreach($item in $plan){
  $modes=if($item.label.StartsWith('image-')){@('on','off','repeat')}else{@('on')}
  foreach($toggle in $modes){
   $case=$item.label+'-'+$toggle
   Write-Progress -Activity '草株選擇與正式影像貢獻驗證' -Status $case -PercentComplete ([int](100*$rows.Count/13))
   $capture=Join-Path $root ($case+'.png')
   $env:AEVRYND_MISTWATCH_CAPTURE=$capture
   $env:AEVRYND_MISTWATCH_GRASS_FIXTURE=$item.mode
   $env:AEVRYND_MISTWATCH_GRASS_FIXTURE_ENABLED=if($toggle-eq 'off'){'0'}else{'1'}
   $start=[DateTime]::UtcNow
   $p=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -PassThru
   $deadline=$start.AddSeconds(45)
   while(-not $p.WaitForExit(1000)-and [DateTime]::UtcNow-lt $deadline){}
   if(-not $p.HasExited){throw ('草距離觀測逾時；檢查同一執行中 PID='+$p.Id+' case='+$case)}
   if($p.ExitCode-ne 0){throw ('草距離場景失敗: '+$case+' exit='+$p.ExitCode)}
   $r=@{};foreach($line in Get-Content -LiteralPath ($capture+'.receipt.txt')){$pair=$line.Split('=',2);if($pair.Count-eq 2){if($r.ContainsKey($pair[0])){throw '重複收據欄位'};$r[$pair[0]]=$pair[1]}}
   foreach($flag in @('quality_shadows','quality_ao','quality_post_aa','ground_textured')){if($r[$flag]-ne '1'){throw ('正式畫質缺項: '+$flag)}}
   foreach($flag in @('debug_errors','debug_warnings','diagnostic_no_shadows','diagnostic_no_msaa','diagnostic_no_bloom','diagnostic_hdr_stage')){if($r[$flag]-ne '0'){throw ('診斷污染或GPU錯誤: '+$flag)}}
   if($r.capture-ne 'success'-or $r.multisample_count-ne '4'-or $r.grass_source-ne 'poly-haven-grass-medium-02'-or
    $r.grass_alpha_cutoff-ne '0.5'-or $r.grass_fixture_schema-ne '1'-or $r.grass_fixture_feet_mode-ne 'synthetic_same_height'-or
    $r.grass_fixture_enabled-ne $env:AEVRYND_MISTWATCH_GRASS_FIXTURE_ENABLED-or $r.grass_fixture_selection_limit-ne '35'-or
    $r.grass_fixture_gate_selected-ne [string]$item.selected-or $r.camera_source-ne 'diagnostic') {throw ('場景契約不符: '+$case)}
   # 合成同高選擇座標與實際玩家分開；玩家仍通過正式固定更新鏡頭檢查。
   if($r.grass_fixture_selection_source-ne 'isolated_diagnostic_reference'-or $r.grass_fixture_actor_isolated-ne '1'-or
      $r.camera_motion_source-ne 'fixed_update'-or $r.camera_motion_accepted-ne '1'){throw ('診斷污染玩家或鏡頭未通過: '+$case)}
   $actor=@($r.grass_fixture_actor_feet.Split(',')|ForEach-Object{[double]::Parse($_,[Globalization.CultureInfo]::InvariantCulture)})
   if($actor.Count-ne 3-or @($actor|Where-Object{[double]::IsNaN($_)-or [double]::IsInfinity($_)-or $_-ne 0}).Count){throw ('實際玩家位置不是合法原點: '+$case)}
   foreach($numeric in @('grass_fixture_scale','grass_fixture_yaw','grass_fixture_player_distance','grass_fixture_camera_distance')){
    $v=[double]::Parse($r[$numeric],[Globalization.CultureInfo]::InvariantCulture)
    if([double]::IsNaN($v)-or [double]::IsInfinity($v)){throw ('非有限場景數字: '+$numeric)}
   }
   if([Math]::Abs([double]$r.grass_fixture_player_distance-$item.player)-gt .00005-or
      [Math]::Abs([double]$r.grass_fixture_camera_distance-$item.camera)-gt .00005){throw ('實際距離不符: '+$case)}
   $expectedDraw=if($item.selected-eq 1-and $toggle-ne 'off'){'1'}else{'0'}
   if($r.grass_fixture_scene_accepted-ne $expectedDraw-or $r.grass_fixture_opaque_draw_instances-ne $expectedDraw){throw ('目標未走預期色彩draw: '+$case)}
   foreach($kind in @('grass','fern','broadleaf','boulder','audio')){
    $cleanup=@{};foreach($line in Get-Content -LiteralPath ($capture+'.'+$kind+'-cleanup.txt')){$pair=$line.Split('=',2);if($pair.Count-eq 2){$cleanup[$pair[0]]=$pair[1]}}
    if($cleanup.cleanup-ne 'success'){throw ('資源釋放失敗: '+$kind)}
   }
   $rows+=[pscustomobject]@{case=$case;capture=$capture;capture_sha256=(Get-FileHash -LiteralPath $capture).Hash;receipt=$r;exit_code=$p.ExitCode;started_utc=$start.ToString('o');completed_utc=[DateTime]::UtcNow.ToString('o')}
   [pscustomobject]@{executable_sha256=$hash;acceptance='synthetic same-height player selection and production image contribution; not gameplay reachability/direct visible pixels';cases=$rows}|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $root 'grass-distance-validation.json') -Encoding UTF8
  }
 }
 if($rows.Count-ne 13-or (Get-FileHash -LiteralPath $exe).Hash-ne $hash){throw '場景未完成或執行檔漂移'}
 $identityFields=@('grass_fixture_index','grass_fixture_variant','grass_fixture_root','grass_fixture_yaw','grass_fixture_scale')
 foreach($field in $identityFields){if(@($rows.receipt|ForEach-Object{$_[$field]}|Sort-Object -Unique).Count-ne 1){throw ('同株契約漂移: '+$field)}}
 Write-Progress -Activity '草株選擇與正式影像貢獻驗證' -Completed
 Write-Output ('GRASS_DISTANCE_CAPTURE_PASS; CASES='+$rows.Count+'; image contribution analysis still required')
}finally{foreach($n in $names){[Environment]::SetEnvironmentVariable($n,$saved[$n],'Process')}}
