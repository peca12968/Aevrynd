param([Parameter(Mandatory=$true)][string]$Producer)

$ErrorActionPreference='Stop'
$shots=@('first-day','first-dusk','first-night','third-day','third-dusk','third-night','god-day','god-dusk','god-night','palette-building','melee-combat')
$runId='11111111-2222-4333-8444-555555555555'
$fingerprint=('ab'*32)
$w=64;$h=36
$temp=Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-r7-sidecar-smoke-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $temp 'capture';$null=New-Item -ItemType Directory -Path $root

function Write-Json([string]$Path,$Value){[IO.File]::WriteAllText($Path,(($Value|ConvertTo-Json -Depth 12)+"`n"),[Text.UTF8Encoding]::new($false))}
function Sha([string]$Path){$f=[IO.File]::OpenRead($Path);$s=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($s.ComputeHash($f))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose();$f.Dispose()}}
function Invoke-Producer([string[]]$Extra){
  $all=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$Producer,'-CaptureRoot',$root,'-ExpectedRunId',$runId,'-ExpectedSourceFingerprint',$fingerprint,'-TestOnly')+$Extra
  $oldPreference=$ErrorActionPreference;$ErrorActionPreference='Continue'
  try{$text=& powershell.exe @all 2>&1;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$oldPreference}
  return [pscustomobject]@{Code=$code;Text=($text-join"`n")}
}
function Must-Fail([string]$Name,[string[]]$Extra,[string]$Pattern){$r=Invoke-Producer $Extra;if($r.Code-eq0-or$r.Text-notmatch$Pattern){throw "$Name hostile failed closed contract failed: code=$($r.Code) output=$($r.Text)"}}
function Sign-Review($Review){
  $r=$Review.rubric;$payload="1|HumanDirectorSameCameraReview|$($Review.reviewer)|$($Review.reviewerOrganization)|$($Review.reviewId)|$($Review.reviewedUtc)|$($Review.directorReferenceScore)|$($r.composition),$($r.lighting),$($r.materials),$($r.worldDensity),$($r.characters),$($r.presentation)|$($Review.evidenceSha256)"
  $s=[Security.Cryptography.SHA256]::Create();try{$Review.signature=([BitConverter]::ToString($s.ComputeHash([Text.Encoding]::UTF8.GetBytes($payload)))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
}

try{
  Add-Type -AssemblyName System.Drawing
  $prototype=Join-Path $temp 'beauty.png';$bitmap=[Drawing.Bitmap]::new($w,$h)
  try{for($y=0;$y-lt$h;$y++){for($x=0;$x-lt$w;$x++){$bitmap.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,(40+$x*2)%256,(70+$y*3)%256,(30+$x+$y)%256))}};$bitmap.Save($prototype,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
  $tokens=[byte[]]::new($w*$h*4);for($i=0;$i-lt$w*$h;$i++){[BitConverter]::GetBytes([uint32](1+($i%4))).CopyTo($tokens,$i*4)}
  $tokenPrototype=Join-Path $temp 'tokens.bin';[IO.File]::WriteAllBytes($tokenPrototype,$tokens)
  $raw=[byte[]]::new($w*$h*8);for($i=0;$i-lt$w*$h;$i++){for($c=0;$c-lt4;$c++){[BitConverter]::GetBytes([uint16]0x3c00).CopyTo($raw,$i*8+$c*2)}}
  $hdrPrototype=Join-Path $temp 'capture.aevhdr';$stream=[IO.File]::Create($hdrPrototype);try{$prefix=[Text.Encoding]::ASCII.GetBytes('AEVHDR-SMOKE');$stream.Write($prefix,0,$prefix.Length);$count=[BitConverter]::GetBytes([uint64]$raw.Length);$stream.Write($count,0,8);$stream.Write($raw,0,$raw.Length)}finally{$stream.Dispose()}
  $hdrRows=@();$captureRows=@()
  foreach($name in $shots){
    Copy-Item -LiteralPath $prototype -Destination (Join-Path $root "$name.png")
    Copy-Item -LiteralPath $prototype -Destination (Join-Path $root "$name-object-id.png")
    Copy-Item -LiteralPath $prototype -Destination (Join-Path $root "$name-depth.png")
    Copy-Item -LiteralPath $tokenPrototype -Destination (Join-Path $root "$name-tokens.bin")
    $hdrName="$name.production-hdr.aevhdr";Copy-Item -LiteralPath $hdrPrototype -Destination (Join-Path $root $hdrName)
    $tokenMap=[ordered]@{runId=$runId;width=$w;height=$h;planeSha256=Sha (Join-Path $root "$name-tokens.bin");identities=@(
      [ordered]@{high='3368770884265705473';low='1'},[ordered]@{high='3368770884265705473';low='2'},
      [ordered]@{high='3368770884265705473';low='33'},[ordered]@{high='12592142912428528129';low='1'})}
    Write-Json (Join-Path $root "$name-tokens.json") $tokenMap
    $hdrFile=Get-Item -LiteralPath (Join-Path $root $hdrName)
    $hdrRows+=,[ordered]@{name="$name.png";path=$hdrName;verified=$true;state='Verified';byteCount=[uint64]$hdrFile.Length;fileHash=Sha $hdrFile.FullName}
    $captureRows+=,[ordered]@{name="$name.png"}
  }
  Write-Json (Join-Path $root 'gameplay-receipt.json') ([ordered]@{source='GameplayLabFramebuffer';runId=$runId;sourceFingerprint=$fingerprint;captures=11})
  Write-Json (Join-Path $root 'gameplay-hdr-capture-receipt.json') ([ordered]@{runId=$runId;expectedCaptures=11;captures=$hdrRows})
  Write-Json (Join-Path $root 'gameplay-presentation-receipt.json') ([ordered]@{runId=$runId;captures=$captureRows})
  Write-Json (Join-Path $root 'gameplay-photoreal-receipt.json') ([ordered]@{runId=$runId;expectedCaptures=11;completedCaptures=11;captures=$captureRows})
  Write-Json (Join-Path $root 'gameplay-palette-ui-receipt.json') ([ordered]@{runId=$runId;cleanHudPresentationFrames=11})
  $validator=Join-Path $temp 'hdr-validator.ps1'
  [IO.File]::WriteAllText($validator,'param([string]$Path);$f=[IO.File]::OpenRead($Path);$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($f))).Replace(''-'','''').ToLowerInvariant()}finally{$s.Dispose();$f.Dispose()};[pscustomobject]@{schemaVersion=3;fileHash=$h}|ConvertTo-Json -Compress',[Text.UTF8Encoding]::new($false))

  # Reproduce the production precondition: no sidecar exists before a signed review is supplied.
  if(Test-Path -LiteralPath (Join-Path $root 'r7-visual-sidecar.json')){throw 'unexpected pre-existing production sidecar'}
  Must-Fail 'sidecar missing review' @('-HdrValidator',$validator,'-OutputPath',(Join-Path $root 'r7-visual-sidecar.test.json')) 'reviewer input missing'
  $digestRun=Invoke-Producer @('-PrintEvidenceDigestOnly');if($digestRun.Code-ne0-or$digestRun.Text-notmatch'R7_EVIDENCE_SHA256=([0-9a-f]{64})'){throw "digest failed: $($digestRun.Text)"};$digest=$Matches[1]
  $reviewPath=Join-Path $root 'director-review.json'
  $review=[ordered]@{schemaVersion=1;source='HumanDirectorSameCameraReview';reviewer='R7 Smoke Reviewer';reviewerOrganization='Aevrynd QA';reviewId='smoke-review-1';reviewedUtc='2026-09-09T00:00:00Z';directorReferenceScore=34;rubric=[ordered]@{composition=7;lighting=7;materials=7;worldDensity=5;characters=5;presentation=3;total=34};evidenceSha256=$digest;signatureAlgorithm='sha256-reviewer-attestation-v1';signature=''}
  Sign-Review $review;Write-Json $reviewPath $review
  $output=Join-Path $root 'r7-visual-sidecar.test.json';$positive=Invoke-Producer @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',$output)
  if($positive.Code-ne0){throw "positive producer failed: $($positive.Text)"};$made=Get-Content -Raw -LiteralPath $output|ConvertFrom-Json
  if(@($made.shots).Count-ne11-or[int]$made.directorReferenceScore-ne34-or[string]$made.runId-ne$runId-or[string]$made.sourceFingerprint-ne$fingerprint){throw 'positive binding mismatch'}

  $marker=Join-Path $root 'fixture-manifest.json';[IO.File]::WriteAllText($marker,'{}');Must-Fail 'synthetic fixture' @('-PrintEvidenceDigestOnly') 'synthetic fixture is forbidden';Remove-Item -LiteralPath $marker
  $gameplayPath=Join-Path $root 'gameplay-receipt.json';$savedGameplay=Get-Content -Raw -LiteralPath $gameplayPath
  $badGameplay=$savedGameplay|ConvertFrom-Json;$badGameplay.runId='aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee';Write-Json $gameplayPath $badGameplay;Must-Fail 'cross run' @('-PrintEvidenceDigestOnly') 'run/fingerprint mismatch';[IO.File]::WriteAllText($gameplayPath,$savedGameplay)
  $badGameplay=$savedGameplay|ConvertFrom-Json;$badGameplay.sourceFingerprint=('cd'*32);Write-Json $gameplayPath $badGameplay;Must-Fail 'cross fingerprint' @('-PrintEvidenceDigestOnly') 'run/fingerprint mismatch';[IO.File]::WriteAllText($gameplayPath,$savedGameplay)
  $missing=Join-Path $root 'first-day.png';Move-Item -LiteralPath $missing -Destination "$missing.hide";Must-Fail 'missing image' @('-PrintEvidenceDigestOnly') 'missing first-day beauty';Move-Item -LiteralPath "$missing.hide" -Destination $missing
  $savedHdr=Get-Content -Raw -LiteralPath (Join-Path $root 'gameplay-hdr-capture-receipt.json');$bad=$savedHdr|ConvertFrom-Json;$bad.captures[1].name=$bad.captures[0].name;Write-Json (Join-Path $root 'gameplay-hdr-capture-receipt.json') $bad;Must-Fail 'duplicate' @('-PrintEvidenceDigestOnly') 'missing, extra or duplicate';[IO.File]::WriteAllText((Join-Path $root 'gameplay-hdr-capture-receipt.json'),$savedHdr)
  $bad=$savedHdr|ConvertFrom-Json;$bad.captures[0].path='..\escape.aevhdr';Write-Json (Join-Path $root 'gameplay-hdr-capture-receipt.json') $bad;Must-Fail 'path escape' @('-PrintEvidenceDigestOnly') 'escaped capture root';[IO.File]::WriteAllText((Join-Path $root 'gameplay-hdr-capture-receipt.json'),$savedHdr)
  $bad=$savedHdr|ConvertFrom-Json;$bad.captures[0].fileHash=('0'*64);Write-Json (Join-Path $root 'gameplay-hdr-capture-receipt.json') $bad;Must-Fail 'hash mismatch' @('-PrintEvidenceDigestOnly') 'hash/bytes mismatch';[IO.File]::WriteAllText((Join-Path $root 'gameplay-hdr-capture-receipt.json'),$savedHdr)
  $savedTokenPlanes=@{};$savedTokenMaps=@{}
  $terrainOnly=[byte[]]::new($w*$h*4);for($i=0;$i-lt$w*$h;$i++){[BitConverter]::GetBytes([uint32]1).CopyTo($terrainOnly,$i*4)}
  foreach($name in $shots){
    $plane=Join-Path $root "$name-tokens.bin";$map=Join-Path $root "$name-tokens.json"
    $savedTokenPlanes[$name]=[IO.File]::ReadAllBytes($plane);$savedTokenMaps[$name]=[IO.File]::ReadAllText($map)
    [IO.File]::WriteAllBytes($plane,$terrainOnly);$tokenJson=Get-Content -Raw -LiteralPath $map|ConvertFrom-Json;$tokenJson.planeSha256=Sha $plane;Write-Json $map $tokenJson
  }
  Must-Fail 'all-shot missing water semantics' @('-PrintEvidenceDigestOnly') 'required production water token coverage was zero: third-day'
  foreach($name in $shots){[IO.File]::WriteAllBytes((Join-Path $root "$name-tokens.bin"),$savedTokenPlanes[$name]);[IO.File]::WriteAllText((Join-Path $root "$name-tokens.json"),$savedTokenMaps[$name])}
  $review.reviewer='';Sign-Review $review;Write-Json $reviewPath $review;Must-Fail 'unnamed reviewer' @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',$output) 'reviewer identity missing'
  $review.reviewer='R7 Smoke Reviewer';$review.rubric.composition=21;$review.rubric.total=48;$review.directorReferenceScore=48;Sign-Review $review;Write-Json $reviewPath $review;Must-Fail 'illegal score' @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',$output) 'illegal reviewer score'
  $review.rubric.composition=7;$review.rubric.total=34;$review.directorReferenceScore=34;$review.signature='';Write-Json $reviewPath $review;Must-Fail 'unsigned review' @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',$output) 'review signature mismatch'
  Sign-Review $review;Write-Json $reviewPath $review
  Must-Fail 'output escape' @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',(Join-Path $temp 'escaped.test.json')) 'path escaped capture root'
  Write-Output "GameplayR7VisualSidecarSmoke=PASS evidence=$digest"
}finally{
  if((Test-Path -LiteralPath $temp)-and( [IO.Path]::GetFullPath($temp).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase))){Remove-Item -LiteralPath $temp -Recurse -Force}
}
