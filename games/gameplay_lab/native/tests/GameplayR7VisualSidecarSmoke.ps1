param([Parameter(Mandatory=$true)][string]$Producer)

$ErrorActionPreference='Stop'
$shots=@('first-day','first-dusk','first-night','third-day','third-dusk','third-night','god-day','god-dusk','god-night','palette-building','melee-combat')
$runId='11111111-2222-4333-8444-555555555555'
$fingerprint=('ab'*32)
$w=384;$h=320
$temp=Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-r7-sidecar-smoke-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $temp 'capture';$null=New-Item -ItemType Directory -Path $root

function Write-Json([string]$Path,$Value){[IO.File]::WriteAllText($Path,(($Value|ConvertTo-Json -Depth 12)+"`n"),[Text.UTF8Encoding]::new($false))}
function Sha([string]$Path){$f=[IO.File]::OpenRead($Path);$s=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($s.ComputeHash($f))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose();$f.Dispose()}}
function Quote-ChildArgument([string]$Value){
  return '"'+[regex]::Replace([regex]::Replace($Value,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')+'"'
}
function Invoke-Producer([string[]]$Extra){
  $all=@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$Producer,'-CaptureRoot',$root,'-ExpectedRunId',$runId,'-ExpectedSourceFingerprint',$fingerprint,'-TestOnly')+$Extra
  # 直接取得子程序退出碼，並同時排空兩個輸出串流；不依賴呼叫範圍的 LASTEXITCODE。
  $info=[Diagnostics.ProcessStartInfo]::new()
  $info.FileName=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
  $info.Arguments=($all|ForEach-Object{Quote-ChildArgument $_})-join ' '
  $info.UseShellExecute=$false;$info.CreateNoWindow=$true
  $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
  $process=[Diagnostics.Process]::new();$process.StartInfo=$info
  try{
    if(-not $process.Start()){throw 'sidecar child process did not start'}
    $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    if(-not $process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();throw 'sidecar child timeout'}
    $text=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
    return [pscustomobject]@{Code=$process.ExitCode;Text=$text}
  }finally{$process.Dispose()}
}
function Must-Fail([string]$Name,[string[]]$Extra,[string]$Pattern){$r=Invoke-Producer $Extra;if($r.Code-eq0-or$r.Text-notmatch$Pattern){throw "$Name hostile failed closed contract failed: code=$($r.Code) output=$($r.Text)"}}
function Sign-Review($Review){
  $r=$Review.rubric;$payload="1|HumanDirectorSameCameraReview|$($Review.reviewer)|$($Review.reviewerOrganization)|$($Review.reviewId)|$($Review.reviewedUtc)|$($Review.directorReferenceScore)|$($r.composition),$($r.lighting),$($r.materials),$($r.worldDensity),$($r.characters),$($r.presentation)|$($Review.evidenceSha256)"
  $s=[Security.Cryptography.SHA256]::Create();try{$Review.signature=([BitConverter]::ToString($s.ComputeHash([Text.Encoding]::UTF8.GetBytes($payload)))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
}
function Write-TestBeauty([string]$Path,[string]$Kind){
  $bitmap=[Drawing.Bitmap]::new($w,$h,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
  try{
    $rectangle=[Drawing.Rectangle]::new(0,0,$w,$h);$data=$bitmap.LockBits($rectangle,[Drawing.Imaging.ImageLockMode]::WriteOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try{
      $bytes=[byte[]]::new($data.Stride*$h)
      for($y=0;$y-lt$h;$y++){for($x=0;$x-lt$w;$x++){
        $nx=(2.0*$x/($w-1))-1.0;$ny=(2.0*$y/($h-1))-1.0
        if($Kind-eq'slope'){$value=45.0+0.22*$x+0.16*$y}
        else{
          $illumination=72.0+36.0*($nx*$nx+$ny*$ny)
          if($Kind-eq'radial'-or$Kind-eq'radial-affine'){$value=$illumination}
          else{
            $period=if($Kind-like'tile32*'){32}else{64};$lx=$x%$period;$ly=$y%$period
            $texture=(($lx*37+$ly*53+$lx*$ly*7)%61)-30;$value=$illumination+$texture
          }
        }
        if($Kind-like'*-affine'){$value=18.0+0.72*$value}
        $channel=[byte][Math]::Max(0,[Math]::Min(255,[Math]::Round($value)))
        $offset=$y*$data.Stride+$x*4;$bytes[$offset]=$channel;$bytes[$offset+1]=$channel;$bytes[$offset+2]=$channel;$bytes[$offset+3]=255
      }}
      [Runtime.InteropServices.Marshal]::Copy($bytes,0,$data.Scan0,$bytes.Length)
    }finally{$bitmap.UnlockBits($data)}
    $bitmap.Save($Path,[Drawing.Imaging.ImageFormat]::Png)
  }finally{$bitmap.Dispose()}
}

try{
  Add-Type -AssemblyName System.Drawing
  $prototype=Join-Path $temp 'beauty.png';Write-TestBeauty $prototype 'slope'
  $tokens=[byte[]]::new($w*$h*4);for($i=0;$i-lt$w*$h;$i++){[BitConverter]::GetBytes([uint32](1+($i%4))).CopyTo($tokens,$i*4)}
  $tokenPrototype=Join-Path $temp 'tokens.bin';[IO.File]::WriteAllBytes($tokenPrototype,$tokens)
  $raw=[byte[]]::new($w*$h*8);for($i=0;$i-lt$w*$h;$i++){for($c=0;$c-lt4;$c++){[BitConverter]::GetBytes([uint16]0x3c00).CopyTo($raw,$i*8+$c*2)}}
  $hdrPrototype=Join-Path $temp 'capture.aevhdr';$stream=[IO.File]::Create($hdrPrototype);try{$prefix=[Text.Encoding]::ASCII.GetBytes('AEVHDR-SMOKE');$stream.Write($prefix,0,$prefix.Length);$count=[BitConverter]::GetBytes([uint64]$raw.Length);$stream.Write($count,0,8);$stream.Write($raw,0,$raw.Length)}finally{$stream.Dispose()}
  $hdrRows=@();$captureRows=@()
  for($shotIndex=0;$shotIndex-lt$shots.Count;$shotIndex++){
    $name=$shots[$shotIndex];$kind=@('slope','radial','tile32','tile64','tile32-affine','radial-affine','slope','slope','slope','slope','slope')[$shotIndex]
    Write-TestBeauty (Join-Path $root "$name.png") $kind
    Copy-Item -LiteralPath $prototype -Destination (Join-Path $root "$name-object-id.png")
    Copy-Item -LiteralPath $prototype -Destination (Join-Path $root "$name-depth.png")
    $tokenPath=Join-Path $root "$name-tokens.bin";Copy-Item -LiteralPath $tokenPrototype -Destination $tokenPath
    if($name-eq'god-day'){
      $roofTokens=[IO.File]::ReadAllBytes($tokenPath)
      for($y=80;$y-lt250;$y++){for($x=80;$x-lt300;$x++){[BitConverter]::GetBytes([uint32]5).CopyTo($roofTokens,($y*$w+$x)*4)}}
      [IO.File]::WriteAllBytes($tokenPath,$roofTokens)
    }
    $hdrName="$name.production-hdr.aevhdr";Copy-Item -LiteralPath $hdrPrototype -Destination (Join-Path $root $hdrName)
    $tokenMap=[ordered]@{runId=$runId;width=$w;height=$h;planeSha256=Sha $tokenPath;identities=@(
      [ordered]@{high='3368770884265705473';low='1'},[ordered]@{high='3368770884265705473';low='2'},
      [ordered]@{high='3368770884265705473';low='33'},[ordered]@{high='12592142912428528129';low='1'},
      [ordered]@{high='3368770884265705473';low='472'})}
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
  if([int]$made.evaluatorVersion-ne3-or[string]$made.algorithms.terrainAutocorrelation-ne'beauty-luma-token-masked-box33-detrended-periodic-peak-lags-32-to-256-v2'-or[string]$made.algorithms.roofFrequency-ne'beauty-srgb-to-linear-rec709-token472-forward-axis-mean-gradient-p95-1px-p90-4px-v1'){throw 'metric analyzer provenance mismatch'}
  $byName=@{};foreach($shot in @($made.shots)){$byName[[string]$shot.name]=$shot}
  $slope=[double]$byName['first-day'].terrain.autocorrelation32To256;$radial=[double]$byName['first-dusk'].terrain.autocorrelation32To256
  $tile32=[double]$byName['first-night'].terrain.autocorrelation32To256;$tile64=[double]$byName['third-day'].terrain.autocorrelation32To256;$tile32Affine=[double]$byName['third-dusk'].terrain.autocorrelation32To256
  if($slope-ge0.65-or$radial-ge0.65){throw "smooth lighting false positive: slope=$slope radial=$radial"}
  if($tile32-lt0.65-or$tile64-lt0.65){throw "tiled terrain escaped gate: tile32=$tile32 tile64=$tile64"}
  if($tile32Affine-lt0.65-or[Math]::Abs($tile32-$tile32Affine)-gt0.03){throw "affine luma instability: base=$tile32 affine=$tile32Affine"}
  $roof=$byName['god-day'].roof
  if([double]$roof.pixelCoverage-le0-or[double]$roof.meanLuma-le0-or[double]$roof.gradient1pxP95-le0-or[double]$roof.gradient4pxP90-le0){throw 'roof metric coverage failed'}

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
  Must-Fail 'all-shot missing water semantics' @('-PrintEvidenceDigestOnly') 'required production water token coverage was zero:\s+third-day'
  foreach($name in $shots){[IO.File]::WriteAllBytes((Join-Path $root "$name-tokens.bin"),$savedTokenPlanes[$name]);[IO.File]::WriteAllText((Join-Path $root "$name-tokens.json"),$savedTokenMaps[$name])}
  $review.reviewer='';Sign-Review $review;Write-Json $reviewPath $review;Must-Fail 'unnamed reviewer' @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',$output) 'reviewer identity missing'
  $review.reviewer='R7 Smoke Reviewer';$review.rubric.composition=21;$review.rubric.total=48;$review.directorReferenceScore=48;Sign-Review $review;Write-Json $reviewPath $review;Must-Fail 'illegal score' @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',$output) 'illegal reviewer score'
  $review.rubric.composition=7;$review.rubric.total=34;$review.directorReferenceScore=34;$review.signature='';Write-Json $reviewPath $review;Must-Fail 'unsigned review' @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',$output) 'review signature mismatch'
  Sign-Review $review;Write-Json $reviewPath $review
  Must-Fail 'output escape' @('-ReviewerInput',$reviewPath,'-HdrValidator',$validator,'-OutputPath',(Join-Path $temp 'escaped.test.json')) 'path escaped capture root'
  Write-Output "GameplayR7VisualSidecarSmoke=PASS evidence=$digest slope=$slope radial=$radial tile32=$tile32 tile64=$tile64 tile32Affine=$tile32Affine roofCoverage=$($roof.pixelCoverage) roofGradient1pxP95=$($roof.gradient1pxP95) roofGradient4pxP90=$($roof.gradient4pxP90)"
}finally{
  if((Test-Path -LiteralPath $temp)-and( [IO.Path]::GetFullPath($temp).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase))){Remove-Item -LiteralPath $temp -Recurse -Force}
}
