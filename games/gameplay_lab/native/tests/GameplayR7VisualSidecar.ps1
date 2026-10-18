param(
  [Parameter(Mandatory=$true)][string]$CaptureRoot,
  [Parameter(Mandatory=$true)][string]$ExpectedRunId,
  [Parameter(Mandatory=$true)][string]$ExpectedSourceFingerprint,
  [string]$ReviewerInput = '',
  [string]$OutputPath = '',
  [string]$HdrValidator = '',
  [switch]$PrintEvidenceDigestOnly,
  [switch]$TestOnly
)

$ErrorActionPreference = 'Stop'
$Evaluator = 'aevrynd.gameplay-r7-visual-sidecar'
$EvaluatorVersion = 3
$EnvironmentIdentityHigh = [uint64]3368770884265705473
$DynamicIdentityHigh = [uint64]12592142912428528129
$RequiredShots = @(
  'first-day','first-dusk','first-night',
  'third-day','third-dusk','third-night',
  'god-day','god-dusk','god-night',
  'palette-building','melee-combat'
)
$RequiredWaterShots = @('third-day','god-day','god-dusk')

function Fail([string]$Message) { throw "R7 visual sidecar rejected: $Message" }
function Full([string]$Path) { return [IO.Path]::GetFullPath($Path) }
function Sha([string]$Path) {
  $stream=[IO.File]::OpenRead($Path);$algorithm=[Security.Cryptography.SHA256]::Create()
  try{return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$algorithm.Dispose();$stream.Dispose()}
}
function Require-File([string]$Path,[string]$Label) {
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "missing $Label`: $Path" }
  return (Get-Item -LiteralPath $Path)
}
function Read-Json([string]$Path,[string]$Label) {
  $null=Require-File $Path $Label
  try { return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json }
  catch { Fail "invalid $Label JSON: $($_.Exception.Message)" }
}
function Assert-UnderRoot([string]$Root,[string]$Path,[string]$Label) {
  $rootPrefix=(Full $Root).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
  $candidate=Full $Path
  if (-not $candidate.StartsWith($rootPrefix,[StringComparison]::OrdinalIgnoreCase)) { Fail "$Label path escaped capture root" }
  return $candidate
}
function Assert-Hex64([string]$Value,[string]$Label) {
  if ([string]::IsNullOrWhiteSpace($Value)-or$value-notmatch'^[0-9a-fA-F]{64}$') { Fail "invalid $Label" }
}
function Exact-Properties($Object,[string[]]$Expected,[string]$Label) {
  $actual=@($Object.PSObject.Properties.Name|Sort-Object);$wanted=@($Expected|Sort-Object)
  if (@(Compare-Object $wanted $actual).Count-ne0) { Fail "$Label property exact-set mismatch" }
}

$root=Full $CaptureRoot
if (-not (Test-Path -LiteralPath $root -PathType Container)) { Fail 'capture root missing' }
if ($root -match '[\\/]fixtures[\\/]r7-visual([\\/]|$)' -or
    (Test-Path -LiteralPath (Join-Path $root 'fixture-manifest.json'))) { Fail 'synthetic fixture is forbidden' }
if ($ExpectedRunId-notmatch'^[0-9a-fA-F]{8}-[0-9a-fA-F-]{27}$') { Fail 'invalid expected runId' }
Assert-Hex64 $ExpectedSourceFingerprint 'expected source fingerprint'

$gameplayPath=Join-Path $root 'gameplay-receipt.json'
$hdrReceiptPath=Join-Path $root 'gameplay-hdr-capture-receipt.json'
$presentationPath=Join-Path $root 'gameplay-presentation-receipt.json'
$photorealPath=Join-Path $root 'gameplay-photoreal-receipt.json'
$palettePath=''
$gameplay=Read-Json $gameplayPath 'gameplay receipt'
$hdrReceipt=Read-Json $hdrReceiptPath 'HDR receipt'
$presentation=Read-Json $presentationPath 'presentation receipt'
$photoreal=Read-Json $photorealPath 'photoreal receipt'
if ($gameplay.source-ne'GameplayLabFramebuffer'-or[int]$gameplay.captures-ne11) { Fail 'non-production gameplay receipt' }
if ([string]$gameplay.runId-ne$ExpectedRunId-or[string]$gameplay.sourceFingerprint-ne$ExpectedSourceFingerprint) { Fail 'gameplay run/fingerprint mismatch' }
foreach($pair in @(@($hdrReceipt,'HDR'),@($presentation,'presentation'),@($photoreal,'photoreal'))){
  if ([string]$pair[0].runId-ne$ExpectedRunId){Fail "$($pair[1]) receipt crossed runId"}
}
if ([int]$hdrReceipt.expectedCaptures-ne11-or[int]$photoreal.expectedCaptures-ne11-or[int]$photoreal.completedCaptures-ne11){Fail 'capture receipt count mismatch'}
if (@($hdrReceipt.captures).Count-ne11-or@($presentation.captures).Count-ne11-or@($photoreal.captures).Count-ne11){Fail 'capture row count mismatch'}
$cleanHud=[int]$presentation.cleanHudPresentationFrames-ge11
if ($null-eq$presentation.PSObject.Properties['cleanHudPresentationFrames']) {
  $palette=Read-Json (Join-Path $root 'gameplay-palette-ui-receipt.json') 'palette UI receipt'
  $palettePath=Join-Path $root 'gameplay-palette-ui-receipt.json'
  if ([string]$palette.runId-ne$ExpectedRunId-or[int]$palette.cleanHudPresentationFrames-lt11){Fail 'clean HUD provenance missing'}
  $cleanHud=$true
}

$hdrByName=@{};foreach($row in @($hdrReceipt.captures)){
  $name=[IO.Path]::GetFileNameWithoutExtension([string]$row.name)
  if (-not($RequiredShots-contains$name)-or$hdrByName.ContainsKey($name)){Fail 'HDR shot name missing, extra or duplicate'}
  if (-not[bool]$row.verified-or[string]$row.state-ne'Verified'){Fail "HDR row not verified: $name"}
  $hdrByName[$name]=$row
}
$photoNames=@($photoreal.captures|ForEach-Object{[IO.Path]::GetFileNameWithoutExtension([string]$_.name)})
$presentationNames=@($presentation.captures|ForEach-Object{[IO.Path]::GetFileNameWithoutExtension([string]$_.name)})
if (@(Compare-Object ($RequiredShots|Sort-Object) ($photoNames|Sort-Object)).Count-ne0-or
    @($photoNames|Group-Object|Where-Object Count-ne1).Count-ne0-or
    @(Compare-Object ($RequiredShots|Sort-Object) ($presentationNames|Sort-Object)).Count-ne0-or
    @($presentationNames|Group-Object|Where-Object Count-ne1).Count-ne0){Fail 'semantic/presentation shot exact-set mismatch'}

$artifactRows=[Collections.Generic.List[object]]::new()
$shotInputs=@{}
foreach($name in $RequiredShots){
  $paths=[ordered]@{
    beauty=Join-Path $root "$name.png";objectId=Join-Path $root "$name-object-id.png";depth=Join-Path $root "$name-depth.png"
    tokenPlane=Join-Path $root "$name-tokens.bin";tokenMap=Join-Path $root "$name-tokens.json"
  }
  $hdrRow=$hdrByName[$name]
  $relative=[string]$hdrRow.path
  if ([IO.Path]::IsPathRooted($relative)-or[string]::IsNullOrWhiteSpace($relative)){Fail "HDR path was not a relative production path: $name"}
  $paths.hdr=Assert-UnderRoot $root (Join-Path $root $relative) "HDR $name"
  foreach($kind in @($paths.Keys)){
    $item=Require-File $paths[$kind] "$name $kind"
    $hash=Sha $item.FullName
    if($kind-eq'hdr'){
      if([uint64]$hdrRow.byteCount-ne[uint64]$item.Length-or[string]$hdrRow.fileHash-ne$hash){Fail "HDR receipt hash/bytes mismatch: $name"}
    }
    $artifactRows.Add([ordered]@{shot=$name;kind=$kind;path=$item.Name;bytes=[uint64]$item.Length;sha256=$hash})
  }
  $token=Read-Json $paths.tokenMap "$name token map"
  $tokenWidth=[int]$token.width;$tokenHeight=[int]$token.height
  if([string]$token.runId-ne$ExpectedRunId-or$tokenWidth-lt8-or$tokenHeight-lt8-or(-not$TestOnly-and($tokenWidth-ne1280-or$tokenHeight-ne720))){Fail "token run/dimensions mismatch: $name"}
  $expectedTokenBytes=[int64]$tokenWidth*[int64]$tokenHeight*4
  if((Get-Item -LiteralPath $paths.tokenPlane).Length-ne$expectedTokenBytes){Fail "token plane byte count mismatch: $name"}
  if([string]$token.planeSha256-ne(Sha $paths.tokenPlane)){Fail "token plane hash mismatch: $name"}
  if($RequiredWaterShots-contains$name){
    $waterCodes=[Collections.Generic.HashSet[uint32]]::new()
    $identityRows=@($token.identities)
    for($identityIndex=0;$identityIndex-lt$identityRows.Count;++$identityIndex){
      $identity=$identityRows[$identityIndex]
      if([uint64]$identity.high-eq$EnvironmentIdentityHigh-and[uint64]$identity.low-eq2){$null=$waterCodes.Add([uint32]($identityIndex+1))}
    }
    $waterPixels=0L
    if($waterCodes.Count-gt0){
      $stream=[IO.File]::OpenRead($paths.tokenPlane);$reader=[IO.BinaryReader]::new($stream)
      try{for($pixel=0L;$pixel-lt([int64]$tokenWidth*$tokenHeight);++$pixel){if($waterCodes.Contains($reader.ReadUInt32())){++$waterPixels}}}finally{$reader.Dispose();$stream.Dispose()}
    }
    if($waterPixels-le0){Fail "required production water token coverage was zero: $name"}
  }
  $shotInputs[$name]=[pscustomobject]@{paths=$paths;token=$token;width=$tokenWidth;height=$tokenHeight}
}
foreach($p in @($gameplayPath,$hdrReceiptPath,$presentationPath,$photorealPath)){
  $i=Require-File $p 'bound receipt';$artifactRows.Add([ordered]@{shot='';kind='receipt';path=$i.Name;bytes=[uint64]$i.Length;sha256=Sha $i.FullName})
}
if(-not[string]::IsNullOrWhiteSpace($palettePath)){
  $i=Require-File $palettePath 'bound palette UI receipt';$artifactRows.Add([ordered]@{shot='';kind='receipt';path=$i.Name;bytes=[uint64]$i.Length;sha256=Sha $i.FullName})
}
$evidenceLines=@($artifactRows|Sort-Object shot,kind,path|ForEach-Object{"$($_.shot)|$($_.kind)|$($_.path)|$($_.bytes)|$($_.sha256)"})
$sha256=[Security.Cryptography.SHA256]::Create()
try{$evidenceDigest=([BitConverter]::ToString($sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes(($evidenceLines-join"`n")+"`n")))).Replace('-','').ToLowerInvariant()}finally{$sha256.Dispose()}
if($PrintEvidenceDigestOnly){Write-Output "R7_EVIDENCE_SHA256=$evidenceDigest";exit 0}

if([string]::IsNullOrWhiteSpace($ReviewerInput)){Fail 'reviewer input missing'}
$reviewPath=Assert-UnderRoot $root (Full $ReviewerInput) 'reviewer input';$review=Read-Json $reviewPath 'reviewer input'
Exact-Properties $review @('schemaVersion','source','reviewer','reviewerOrganization','reviewId','reviewedUtc','directorReferenceScore','rubric','evidenceSha256','signatureAlgorithm','signature') 'reviewer input'
Exact-Properties $review.rubric @('composition','lighting','materials','worldDensity','characters','presentation','total') 'reviewer rubric'
if([int]$review.schemaVersion-ne1-or[string]$review.source-ne'HumanDirectorSameCameraReview'){Fail 'reviewer source/schema'}
if([string]::IsNullOrWhiteSpace([string]$review.reviewer)-or[string]::IsNullOrWhiteSpace([string]$review.reviewerOrganization)-or[string]::IsNullOrWhiteSpace([string]$review.reviewId)){Fail 'reviewer identity missing'}
try{$reviewed=[DateTimeOffset]::Parse([string]$review.reviewedUtc,[Globalization.CultureInfo]::InvariantCulture)}catch{Fail 'review timestamp invalid'}
$bounds=[ordered]@{composition=20;lighting=20;materials=20;worldDensity=15;characters=15;presentation=10}
$sum=0;foreach($key in $bounds.Keys){$v=$review.rubric.$key;if($v-isnot[ValueType]-or$v-is[bool]-or[int]$v-ne[double]$v-or[int]$v-lt0-or[int]$v-gt$bounds[$key]){Fail "illegal reviewer score: $key"};$sum+=[int]$v}
if([int]$review.rubric.total-ne$sum-or[int]$review.directorReferenceScore-ne$sum-or$sum-lt0-or$sum-gt100){Fail 'manual total/director score mismatch'}
if([string]$review.evidenceSha256-ne$evidenceDigest){Fail 'review evidence hash mismatch'}
if([string]$review.signatureAlgorithm-ne'sha256-reviewer-attestation-v1'){Fail 'review signature algorithm'}
$reviewPayload="1|HumanDirectorSameCameraReview|$($review.reviewer)|$($review.reviewerOrganization)|$($review.reviewId)|$($review.reviewedUtc)|$sum|$($review.rubric.composition),$($review.rubric.lighting),$($review.rubric.materials),$($review.rubric.worldDensity),$($review.rubric.characters),$($review.rubric.presentation)|$evidenceDigest"
$sha256=[Security.Cryptography.SHA256]::Create();try{$signature=([BitConverter]::ToString($sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($reviewPayload)))).Replace('-','').ToLowerInvariant()}finally{$sha256.Dispose()}
if([string]$review.signature-ne$signature){Fail 'review signature mismatch'}

if([string]::IsNullOrWhiteSpace($HdrValidator)){Fail 'production HDR validator missing'}
$validator=Full $HdrValidator
if(-not(Test-Path -LiteralPath $validator -PathType Leaf)){Fail 'production HDR validator path invalid'}
if(-not$TestOnly-and[IO.Path]::GetFileName($validator)-ne'Aevrynd-Gameplay-Hdr-Validator.exe'){Fail 'untrusted HDR validator identity'}
foreach($name in $RequiredShots){
  $raw=& $validator $shotInputs[$name].paths.hdr 2>&1;$invocationSucceeded=$?
  $exit=if([IO.Path]::GetExtension($validator)-ieq'.ps1'){if($invocationSucceeded){0}else{1}}else{$LASTEXITCODE}
  if($exit-ne0){Fail "HDR validator rejected $name"}
  try{$validated=($raw-join"`n")|ConvertFrom-Json}catch{Fail "HDR validator output invalid: $name"}
  if([int]$validated.schemaVersion-ne3-or[string]$validated.fileHash-ne(Sha $shotInputs[$name].paths.hdr)){Fail "HDR validator provenance mismatch: $name"}
}

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies 'System.Drawing.dll' -TypeDefinition @'
using System;using System.IO;using System.Drawing;using System.Drawing.Imaging;using System.Runtime.InteropServices;using System.Collections.Generic;
public sealed class R7MetricResult{public double hdrMean,hdrP10,waterCoverage,waterEntropy,waterNormalVariance,waterReflectionVariance,terrainAutocorrelation,vegetationCoverage,vegetationSpatialEntropy,actorCoverage,roofCoverage,roofMeanLuma,roofGradient1pxP95,roofGradient4pxP90;}
public static class R7ProductionMetrics{
 static float Half(ushort h){int s=(h>>15)&1,e=(h>>10)&31,m=h&1023;double v;if(e==0)v=m==0?0:Math.Pow(2,-14)*(m/1024.0);else if(e==31)v=m==0?double.PositiveInfinity:double.NaN;else v=Math.Pow(2,e-15)*(1+m/1024.0);return(float)(s==0?v:-v);}
 static double Entropy(int[] h,int n){double e=0;if(n==0)return 0;foreach(int x in h)if(x>0){double p=(double)x/n;e-=p*Math.Log(p,2);}return e;}
 static double Linear(byte c){double v=c/255.0;return v<=.04045?v/12.92:Math.Pow((v+.055)/1.055,2.4);}
 static double Percentile(List<double> values,double fraction){if(values.Count==0)return 0;values.Sort();return values[(int)Math.Floor((values.Count-1)*fraction)];}
 static float[] LocalResidual(float[] l,int W,int H){const int radius=16,size=33;double[] integral=new double[(W+1)*(H+1)];for(int y=0;y<H;y++){double row=0;for(int x=0;x<W;x++){row+=l[y*W+x];integral[(y+1)*(W+1)+x+1]=integral[y*(W+1)+x+1]+row;}}float[] d=new float[l.Length];double area=size*size;for(int y=radius;y<H-radius;y++)for(int x=radius;x<W-radius;x++){int x0=x-radius,y0=y-radius,x1=x+radius+1,y1=y+radius+1;double sum=integral[y1*(W+1)+x1]-integral[y0*(W+1)+x1]-integral[y1*(W+1)+x0]+integral[y0*(W+1)+x0];d[y*W+x]=(float)(l[y*W+x]-sum/area);}return d;}
 static double ResidualEnergyRatio(float[] l,float[] d,int[] c,int W,int H){const int margin=16;double sl=0,sd=0,sll=0,sdd=0;long n=0;for(int y=margin;y<H-margin;y+=2)for(int x=margin;x<W-margin;x+=2){int i=y*W+x;if(c[i]!=1)continue;double a=l[i],b=d[i];sl+=a;sd+=b;sll+=a*a;sdd+=b*b;n++;}if(n<=32)return 0;double vl=n*sll-sl*sl,vd=n*sdd-sd*sd;return vl>1e-18?Math.Max(0,vd/vl):0;}
 static double TerrainCorrelation(float[] d,int[] c,int W,int H,int q,bool vertical){const int margin=16;double sx=0,sy=0,sxx=0,syy=0,sxy=0;long n=0;for(int y=margin;y<H-margin;y+=2)for(int x=margin;x<W-margin;x+=2){int xx=vertical?x:x+q,yy=vertical?y+q:y;if(xx>=W-margin||yy>=H-margin)continue;int i=y*W+x,j=yy*W+xx;if(c[i]==1&&c[j]==1){double a=d[i],b=d[j];sx+=a;sy+=b;sxx+=a*a;syy+=b*b;sxy+=a*b;n++;}}double vx=n*sxx-sx*sx,vy=n*syy-sy*sy,den=Math.Sqrt(Math.Max(0,vx*vy));return n>32&&den>1e-18?(n*sxy-sx*sy)/den:double.NaN;}
 static double TerrainPeriodicPeak(float[] l,int[] c,int W,int H){float[] d=LocalResidual(l,W,H);if(ResidualEnergyRatio(l,d,c,W,H)<.01)return 0;double best=0;foreach(bool vertical in new[]{false,true}){double[] corr=new double[265];for(int q=24;q<=264;q++)corr[q]=TerrainCorrelation(d,c,W,H,q,vertical);for(int q=32;q<=256;q++){double peak=corr[q],left=corr[q-1],right=corr[q+1];if(double.IsNaN(peak)||double.IsNaN(left)||double.IsNaN(right)||peak<=0||peak+1e-9<left||peak+1e-9<right)continue;double shoulder=double.PositiveInfinity;for(int delta=1;delta<=8;delta++){double a=corr[q-delta],b=corr[q+delta];if(!double.IsNaN(a))shoulder=Math.Min(shoulder,a);if(!double.IsNaN(b))shoulder=Math.Min(shoulder,b);}if(shoulder<double.PositiveInfinity&&peak-shoulder>=.08)best=Math.Max(best,peak);}}return best;}
 public static R7MetricResult Analyze(string beauty,string tokens,string hdr,int[] cat,int W,int H){int N=checked(W*H);byte[] tb=File.ReadAllBytes(tokens);if(tb.Length!=N*4)throw new InvalidDataException("token byte count");int[] c=new int[N];for(int i=0;i<N;i++){uint code=BitConverter.ToUInt32(tb,i*4);if(code>=cat.Length)throw new InvalidDataException("token index");c[i]=cat[code];}
  float[] l=new float[N];double[] linearLuma=new double[N];using(var src=new Bitmap(beauty)){if(src.Width!=W||src.Height!=H)throw new InvalidDataException("image dimensions");using(var b=new Bitmap(W,H,PixelFormat.Format32bppArgb)){using(var g=Graphics.FromImage(b))g.DrawImageUnscaled(src,0,0);var d=b.LockBits(new Rectangle(0,0,W,H),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);byte[] p=new byte[d.Stride*H];Marshal.Copy(d.Scan0,p,0,p.Length);b.UnlockBits(d);for(int y=0;y<H;y++)for(int x=0;x<W;x++){int o=y*d.Stride+x*4,i=y*W+x;l[i]=(float)((.2126*p[o+2]+.7152*p[o+1]+.0722*p[o])/255.0);linearLuma[i]=.2126*Linear(p[o+2])+.7152*Linear(p[o+1])+.0722*Linear(p[o]);}}}
  var r=new R7MetricResult();int water=0,veg=0,actor=0,roof=0;int[] hist=new int[256],grid=new int[16*9];double gs=0,g2=0,rs=0,r2=0,roofSum=0;int gn=0,rn=0;var roofGradient1=new List<double>();var roofGradient4=new List<double>();for(int y=0;y<H;y++)for(int x=0;x<W;x++){int i=y*W+x;if(c[i]==2){water++;hist[Math.Max(0,Math.Min(255,(int)(l[i]*255))) ]++;if(x+1<W&&y+1<H&&c[i+1]==2&&c[i+W]==2){double gx=l[i+1]-l[i],gy=l[i+W]-l[i],v=Math.Sqrt(gx*gx+gy*gy);gs+=v;g2+=v*v;gn++;}if(x>=4&&x+4<W&&c[i-4]==2&&c[i+4]==2){double v=l[i]-(l[i-4]+l[i+4])*.5;rs+=v;r2+=v*v;rn++;}}else if(c[i]==3){veg++;grid[(y*9/H)*16+x*16/W]++;}else if(c[i]==4)actor++;else if(c[i]==5){roof++;roofSum+=linearLuma[i];if(x+1<W&&y+1<H&&c[i+1]==5&&c[i+W]==5){roofGradient1.Add((Math.Abs(linearLuma[i+1]-linearLuma[i])+Math.Abs(linearLuma[i+W]-linearLuma[i]))*.5);}if(x+4<W&&y+4<H&&c[i+4]==5&&c[i+4*W]==5){roofGradient4.Add((Math.Abs(linearLuma[i+4]-linearLuma[i])+Math.Abs(linearLuma[i+4*W]-linearLuma[i]))*.5);}}}
  r.waterCoverage=(double)water/N;r.waterEntropy=Entropy(hist,water);r.waterNormalVariance=gn==0?0:Math.Max(0,g2/gn-(gs/gn)*(gs/gn));r.waterReflectionVariance=rn==0?0:Math.Max(0,r2/rn-(rs/rn)*(rs/rn));r.vegetationCoverage=(double)veg/N;r.actorCoverage=(double)actor/N;r.vegetationSpatialEntropy=Entropy(grid,veg);
  r.terrainAutocorrelation=TerrainPeriodicPeak(l,c,W,H);
  r.roofCoverage=(double)roof/N;r.roofMeanLuma=roof==0?0:roofSum/roof;r.roofGradient1pxP95=Percentile(roofGradient1,.95);r.roofGradient4pxP90=Percentile(roofGradient4,.90);
  int raw=N*8;using(var f=new FileStream(hdr,FileMode.Open,FileAccess.Read,FileShare.Read)){if(f.Length<raw+8)throw new InvalidDataException("HDR length");f.Seek(-raw-8,SeekOrigin.End);byte[] n8=new byte[8];if(f.Read(n8,0,8)!=8||BitConverter.ToUInt64(n8,0)!=(ulong)raw)throw new InvalidDataException("HDR raw trailer");byte[] px=new byte[raw];int off=0,k;while(off<px.Length&&(k=f.Read(px,off,px.Length-off))>0)off+=k;if(off!=px.Length)throw new EndOfStreamException();double sum=0;float[] values=new float[N];for(int i=0;i<N;i++){float rr=Half(BitConverter.ToUInt16(px,i*8)),gg=Half(BitConverter.ToUInt16(px,i*8+2)),bb=Half(BitConverter.ToUInt16(px,i*8+4));float v=(float)(.2126*rr+.7152*gg+.0722*bb);if(float.IsNaN(v)||float.IsInfinity(v))throw new InvalidDataException("HDR nonfinite");values[i]=v;sum+=v;}Array.Sort(values);r.hdrMean=sum/N;r.hdrP10=values[(int)((N-1)*.1)];}return r;}
}
'@

$rows=[Collections.Generic.List[object]]::new()
foreach($name in $RequiredShots){
  $input=$shotInputs[$name];$identities=@($input.token.identities);$categories=[int[]]::new($identities.Count+1)
  for($i=0;$i-lt$identities.Count;++$i){$id=$identities[$i];$high=[uint64]$id.high;$low=[uint64]$id.low;$category=0;if($high-eq$EnvironmentIdentityHigh){if($low-eq1){$category=1}elseif($low-eq2){$category=2}elseif($low-ge33-and$low-le458){$category=3}elseif($low-eq472){$category=5}}elseif($high-eq$DynamicIdentityHigh){$category=4};$categories[$i+1]=$category}
  try{$m=[R7ProductionMetrics]::Analyze($input.paths.beauty,$input.paths.tokenPlane,$input.paths.hdr,$categories,$input.width,$input.height)}catch{Fail "metric analysis failed for $name`: $($_.Exception.Message)"}
  $rows.Add([ordered]@{name=$name;hdr=[ordered]@{meanLuma=$m.hdrMean;p10Luma=$m.hdrP10};water=[ordered]@{pixelCoverage=$m.waterCoverage;entropy=$m.waterEntropy;normalVariance=$m.waterNormalVariance;reflectionVariance=$m.waterReflectionVariance};terrain=[ordered]@{autocorrelation32To256=$m.terrainAutocorrelation};vegetation=[ordered]@{coverage=$m.vegetationCoverage;spatialEntropy=$m.vegetationSpatialEntropy};actor=[ordered]@{coverage=$m.actorCoverage};roof=[ordered]@{pixelCoverage=$m.roofCoverage;meanLuma=$m.roofMeanLuma;gradient1pxP95=$m.roofGradient1pxP95;gradient4pxP90=$m.roofGradient4pxP90};ui=[ordered]@{coverage=if($cleanHud){0}else{1};heroOverlap=if($cleanHud){0}else{1}}})
}

if([string]::IsNullOrWhiteSpace($OutputPath)){$OutputPath=Join-Path $root 'r7-visual-sidecar.json'}
$output=Full $OutputPath
$null=Assert-UnderRoot $root $output 'output'
if($TestOnly){if([IO.Path]::GetFileName($output)-eq'r7-visual-sidecar.json'){Fail 'test-only output cannot use production sidecar name'}}
elseif($output-ne(Full (Join-Path $root 'r7-visual-sidecar.json'))){Fail 'production output path must be capture-root/r7-visual-sidecar.json'}
$algorithms=[ordered]@{hdrLuma='validated-rgba16f-linear-rec709-v1';waterEntropy='beauty-luma-histogram-256-v1';waterNormalVariance='beauty-luma-forward-gradient-variance-v1';waterReflectionVariance='beauty-luma-horizontal-4px-residual-variance-v1';terrainAutocorrelation='beauty-luma-token-masked-box33-detrended-periodic-peak-lags-32-to-256-v2';roofFrequency='beauty-srgb-to-linear-rec709-token472-forward-axis-mean-gradient-p95-1px-p90-4px-v1';semanticCoverage='lossless-token-plane-v1'}
$sidecar=[ordered]@{schemaVersion=1;source=if($TestOnly){'GameplayR7VisualSidecarTest'}else{'GameplayR7VisualSidecarProduction'};runId=$ExpectedRunId;sourceFingerprint=$ExpectedSourceFingerprint;evaluator=$Evaluator;evaluatorVersion=$EvaluatorVersion;algorithms=$algorithms;captureRoot=$root;evidenceSha256=$evidenceDigest;directorReferenceScore=$sum;manualReview=[ordered]@{path=$reviewPath;sha256=Sha $reviewPath;reviewer=[string]$review.reviewer;reviewerOrganization=[string]$review.reviewerOrganization;reviewId=[string]$review.reviewId;reviewedUtc=[string]$review.reviewedUtc;signatureAlgorithm=[string]$review.signatureAlgorithm;signature=[string]$review.signature;rubric=$review.rubric};artifacts=@($artifactRows);shots=@($rows)}
[IO.File]::WriteAllText($output,(($sidecar|ConvertTo-Json -Depth 12)+"`n"),[Text.UTF8Encoding]::new($false))
$written=Read-Json $output 'written sidecar'
if([string]$written.evidenceSha256-ne$evidenceDigest-or@($written.shots).Count-ne11){Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue;Fail 'post-write verification'}
Write-Output "R7_VISUAL_SIDECAR=PASS runId=$ExpectedRunId score=$sum evidence=$evidenceDigest output=$output"
