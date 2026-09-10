param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'GameplayLabCaptureEvidence.ps1')
# 同色不是身份證據；此測試直接使用正式 oracle 的無損身份分類函式。 (identity).
if(-not(Get-Command Get-GameplayTemporalTokenCompatibility -ErrorAction SilentlyContinue)){throw 'Lossless temporal identity classifier is missing'}
$static=[pscustomobject]@{high='3368770884265705473';low='1';generation='1';class='static';opacityAffected=$false}
$other=[pscustomobject]@{high=$static.high;low='2';generation='1';class='static';opacityAffected=$false}
$dynamic=[pscustomobject]@{high='42';low='1';generation='1';class='dynamic';opacityAffected=$false}
if(-not(Get-GameplayTemporalTokenCompatibility $static $static)){throw 'Stable terrain rejected'}
if(Get-GameplayTemporalTokenCompatibility $static $other){throw 'Same diagnostic color/different identity accepted'}
if(Get-GameplayTemporalTokenCompatibility $dynamic $dynamic){throw 'Dynamic identity accepted as stable'}
$affected=[pscustomobject]@{high=$static.high;low=$static.low;generation='1';class='static';opacityAffected=$true}
if(Get-GameplayTemporalTokenCompatibility $affected $static){throw 'Opacity-affected identity accepted as stable'}
Write-Output 'Lossless temporal token classification PASS'
if(-not$OutputDirectory){$OutputDirectory=Join-Path ([IO.Path]::GetTempPath()) ('gameplay-token-'+[guid]::NewGuid().ToString('N'))}
$null=New-Item -ItemType Directory -Path $OutputDirectory -Force
Add-Type -AssemblyName System.Drawing
$width=192;$height=128;$runId=[guid]::NewGuid().ToString();$stem='third-day'
$names=@('third-day-raw.png','third-day-diagnostic.png','third-day-raw-object-id.png','third-day-object-id.png','third-day-raw-depth.png','third-day-depth.png')
foreach($name in $names){$bitmap=[Drawing.Bitmap]::new($width,$height);$g=[Drawing.Graphics]::FromImage($bitmap);try{$g.Clear([Drawing.Color]::FromArgb(255,90,110,130));$bitmap.Save((Join-Path $OutputDirectory $name))}finally{$g.Dispose();$bitmap.Dispose()}}
function Save-TokenFixture([string]$Mode){
  $captures=[ordered]@{name='third-day.png'}
  for($phase=0;$phase-lt2;++$phase){
    $suffix=if($phase-eq0){'-raw'}else{''};$tokens=[uint32[]]::new($width*$height)
    for($i=0;$i-lt$tokens.Length;++$i){$tokens[$i]=1}
    if($Mode-eq'invalid-token'){$tokens[0]=99}
    if($Mode-eq'dynamic'){for($y=48;$y-lt80;++$y){for($x=80;$x-lt112;++$x){$tokens[$y*$width+$x]=2}}}
    $bytes=[byte[]]::new($tokens.Length*4);[Buffer]::BlockCopy($tokens,0,$bytes,0,$bytes.Length)
    $plane="$stem$suffix-tokens.bin";[IO.File]::WriteAllBytes((Join-Path $OutputDirectory $plane),$bytes)
    $first=if($Mode-eq'mismatch'-and$phase-eq1){$other}else{$static}
    $m=[ordered]@{source='GameplayLosslessTokens';version=1;runId=$runId;width=$width;height=$height;frameSerial=[string](10+$phase);tokenGeneration=[string](20+$phase);phase=$phase;plane=$plane;planeSha256=(Get-GameplayEvidenceFileSnapshot (Join-Path $OutputDirectory $plane)).sha256;identities=@($first,$dynamic)}
    $path=Join-Path $OutputDirectory "$stem$suffix-tokens.json";[IO.File]::WriteAllText($path,($m|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    $field=if($phase-eq0){'rawTokenMetadataSha256'}else{'effectiveTokenMetadataSha256'};$captures[$field]=(Get-GameplayEvidenceFileSnapshot $path).sha256
  }
  [IO.File]::WriteAllText((Join-Path $OutputDirectory 'gameplay-character-capture-receipt.json'),([ordered]@{runId=$runId;captures=@($captures)}|ConvertTo-Json -Depth 6))
  [IO.File]::WriteAllText((Join-Path $OutputDirectory 'gameplay-readback-sequence-receipt.json'),([ordered]@{runId=$runId;captures=@(@{name='third-day.png';complete=1;phases=@(@{frameSerial=10;tokenGeneration=20},@{frameSerial=11;tokenGeneration=21})})}|ConvertTo-Json -Depth 6))
}
function Invoke-Fixture{
  $paths=@($names|ForEach-Object{Join-Path $OutputDirectory $_});$expected=@{};$keys=@('rawBeauty','effectiveBeauty','rawObjectId','effectiveObjectId','rawDepth','effectiveDepth')
  for($i=0;$i-lt6;++$i){$expected[$keys[$i]]=(Get-GameplayEvidenceFileSnapshot $paths[$i]).sha256}
  Get-GameplayTemporalGhostEvidence $paths[0] $paths[1] $paths[2] $paths[3] $paths[4] $paths[5] 0 0 $width $height $expected
}
Save-TokenFixture 'static';$clean=Invoke-Fixture
if($clean.pass-ne1-or$clean.eligiblePixels-ne($width*$height)){throw 'Fully populated stable static frame failed'}
$staticEvidence=$clean
Save-TokenFixture 'mismatch';$rejected=$false;try{$null=Invoke-Fixture}catch{if($_-match'eligible coverage'){$rejected=$true}else{throw}}
if(-not$rejected){throw 'Identical RGB masks with different full identities passed'}
Save-TokenFixture 'dynamic';$clean=Invoke-Fixture
if($clean.pass-ne1-or$clean.excludedPixels-ne1024-or$clean.objectAdjacentRingPixels-lt1){throw 'Dynamic exclusion/ring contract failed'}
$dynamicEvidence=$clean
# 在動態區外的靜態 ring 植入連續 trail；不能因鄰接動態物件而掩蓋。 (ghost).
$path=Join-Path $OutputDirectory 'third-day-diagnostic.png';$b=[Drawing.Bitmap]::FromFile($path);$copy=[Drawing.Bitmap]::new($b);$b.Dispose()
try{for($y=48;$y-lt80;++$y){$copy.SetPixel(79,$y,[Drawing.Color]::White)};$copy.Save($path)}finally{$copy.Dispose()}
$ghost=Invoke-Fixture;if($ghost.pass-ne0-or$ghost.ringPass-ne0){throw 'Dynamic-boundary ghost was not rejected'}
$planePath=Join-Path $OutputDirectory 'third-day-tokens.bin';$bytes=[IO.File]::ReadAllBytes($planePath);$bytes[0]=$bytes[0]-bxor1;[IO.File]::WriteAllBytes($planePath,$bytes)
$rejected=$false;try{$null=Invoke-Fixture}catch{if($_-match'plane size/SHA'){$rejected=$true}else{throw}}
if(-not$rejected){throw 'Tampered token plane passed'}
Save-TokenFixture 'static'
$metadataPath=Join-Path $OutputDirectory 'third-day-tokens.json';$stableSha=Get-GameplayTemporalTokenStableMetadataSha $metadataPath
$m=Get-Content -Raw $metadataPath|ConvertFrom-Json;$m.frameSerial='999';[IO.File]::WriteAllText($metadataPath,($m|ConvertTo-Json -Depth 6))
if((Get-GameplayTemporalTokenStableMetadataSha $metadataPath)-cne$stableSha){throw 'Canonical metadata included frame-local serial'}
$m.identities[0].class='dynamic';[IO.File]::WriteAllText($metadataPath,($m|ConvertTo-Json -Depth 6))
if((Get-GameplayTemporalTokenStableMetadataSha $metadataPath)-ceq$stableSha){throw 'Canonical metadata ignored class mutation'}
$m.identities[0].class='static';[IO.File]::WriteAllText($metadataPath,($m|ConvertTo-Json -Depth 6))
$cpath=Join-Path $OutputDirectory 'gameplay-character-capture-receipt.json';$c=Get-Content -Raw $cpath|ConvertFrom-Json;$c.captures[0].effectiveTokenMetadataSha256=(Get-GameplayEvidenceFileSnapshot $metadataPath).sha256;[IO.File]::WriteAllText($cpath,($c|ConvertTo-Json -Depth 6))
$rejected=$false;try{$null=Invoke-Fixture}catch{if($_-match'frame/size/name provenance'){$rejected=$true}else{throw}}
if(-not$rejected){throw 'Rehashed wrong-frame metadata passed'}
Save-TokenFixture 'invalid-token';$rejected=$false;try{$null=Invoke-Fixture}catch{if($_-match'token out of range'){$rejected=$true}else{throw}}
if(-not$rejected){throw 'Rehashed out-of-range pixel token passed'}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'cpu-receipt.json'),([ordered]@{source='GameplayTemporalTokenEligibilitySmoke';pass=1;static=$staticEvidence;dynamic=$dynamicEvidence;ghost=$ghost;sameColorDifferentIdentityRejected=$true;planeTamperRejected=$true;wrongFrameRejected=$true}|ConvertTo-Json -Depth 12))
Write-Output "Synthetic full-frame static/mismatched identity/dynamic ring/ghost PASS artifacts=$OutputDirectory"
