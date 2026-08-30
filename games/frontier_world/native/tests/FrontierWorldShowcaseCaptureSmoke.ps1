param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
$directory=Join-Path (Split-Path -Parent $Executable) 'frontier-captures'
$names=@('day-first-terrain.png','day-third-forest.png','day-god-map.png','day-river-water.png','dusk-first-road.png','dusk-third-settlement.png','dusk-god-map.png','dusk-river-reflection.png','night-first-settlement.png','night-third-forest.png','night-god-map.png','night-river-reflection.png','feature-off.png','feature-on.png')
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public sealed class ShowcaseImageMetrics {
  public int Width,Height; public byte[] Luma; public double Mean,Variance,Entropy,NonBackgroundCoverage,EdgeDensity,MeanEdge;
  public bool ContentValid { get { return Variance>=100.0&&Entropy>=2.0&&NonBackgroundCoverage>=0.15&&EdgeDensity>=0.015&&MeanEdge>=6.0; } }
}
public sealed class ShowcaseImageComparison {
  public double FullLumaMae,EdgeMae,BestShiftMae; public int AlignmentShiftX,AlignmentShiftY;
  public bool Passed { get { return FullLumaMae<=4.0&&EdgeMae<=12.0&&AlignmentShiftX==0&&AlignmentShiftY==0; } }
}
public static class ShowcaseImageEvidence {
  static ShowcaseImageMetrics AnalyzeLuma(byte[] luma,int width,int height) {
    var result=new ShowcaseImageMetrics{Width=width,Height=height,Luma=luma}; long sum=0; var histogram=new long[16];
    int background=(luma[0]+luma[width-1]+luma[(height-1)*width]+luma[height*width-1])/4,covered=0,edges=0; double edgeSum=0;
    for(int i=0;i<luma.Length;++i){int value=luma[i];sum+=value;++histogram[value>>4];if(Math.Abs(value-background)>8)++covered;}
    result.Mean=(double)sum/luma.Length;double variance=0;for(int i=0;i<luma.Length;++i){double delta=luma[i]-result.Mean;variance+=delta*delta;}result.Variance=variance/luma.Length;
    for(int i=0;i<histogram.Length;++i)if(histogram[i]>0){double p=(double)histogram[i]/luma.Length;result.Entropy-=p*Math.Log(p,2);}
    for(int y=1;y<height-1;++y)for(int x=1;x<width-1;++x){int i=y*width+x;int gx=-luma[i-width-1]+luma[i-width+1]-2*luma[i-1]+2*luma[i+1]-luma[i+width-1]+luma[i+width+1];int gy=-luma[i-width-1]-2*luma[i-width]-luma[i-width+1]+luma[i+width-1]+2*luma[i+width]+luma[i+width+1];double magnitude=Math.Sqrt((double)gx*gx+(double)gy*gy);edgeSum+=magnitude;if(magnitude>=32)++edges;}
    int interior=(width-2)*(height-2);result.NonBackgroundCoverage=(double)covered/luma.Length;result.EdgeDensity=(double)edges/interior;result.MeanEdge=edgeSum/interior;return result;
  }
  public static ShowcaseImageMetrics Analyze(string path) {
    using(var source=new Bitmap(path))using(var image=new Bitmap(source.Width,source.Height,PixelFormat.Format32bppArgb)){using(var graphics=Graphics.FromImage(image))graphics.DrawImageUnscaled(source,0,0);var rectangle=new Rectangle(0,0,image.Width,image.Height);var data=image.LockBits(rectangle,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);try{int bytes=Math.Abs(data.Stride)*image.Height;var pixels=new byte[bytes];Marshal.Copy(data.Scan0,pixels,0,bytes);var luma=new byte[image.Width*image.Height];for(int y=0;y<image.Height;++y)for(int x=0;x<image.Width;++x){int pixel=y*data.Stride+x*4;luma[y*image.Width+x]=(byte)((pixels[pixel+2]*77+pixels[pixel+1]*150+pixels[pixel]*29)>>8);}return AnalyzeLuma(luma,image.Width,image.Height);}finally{image.UnlockBits(data);}}
  }
  static double ShiftMae(ShowcaseImageMetrics a,ShowcaseImageMetrics b,int dx,int dy){long sum=0,count=0;for(int y=1;y<a.Height-1;++y)for(int x=1;x<a.Width-1;++x){int bx=x+dx,by=y+dy;if(bx<0||bx>=b.Width||by<0||by>=b.Height)continue;sum+=Math.Abs(a.Luma[y*a.Width+x]-b.Luma[by*b.Width+bx]);++count;}return (double)sum/count;}
  static double EdgeMae(ShowcaseImageMetrics a,ShowcaseImageMetrics b){long sum=0,count=0;for(int y=1;y<a.Height-1;++y)for(int x=1;x<a.Width-1;++x){int i=y*a.Width+x;int agx=a.Luma[i+1]-a.Luma[i-1],agy=a.Luma[i+a.Width]-a.Luma[i-a.Width],bgx=b.Luma[i+1]-b.Luma[i-1],bgy=b.Luma[i+b.Width]-b.Luma[i-b.Width];sum+=Math.Abs(agx-bgx)+Math.Abs(agy-bgy);count+=2;}return (double)sum/count;}
  public static ShowcaseImageComparison Compare(ShowcaseImageMetrics a,ShowcaseImageMetrics b){if(a.Width!=b.Width||a.Height!=b.Height)throw new ArgumentException("Image dimensions differ");var result=new ShowcaseImageComparison();result.FullLumaMae=ShiftMae(a,b,0,0);result.EdgeMae=EdgeMae(a,b);result.BestShiftMae=double.MaxValue;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){double error=ShiftMae(a,b,dx,dy);if(error<result.BestShiftMae){result.BestShiftMae=error;result.AlignmentShiftX=dx;result.AlignmentShiftY=dy;}}return result;}
  public static bool RunHostileSelfTests(){int width=64,height=64;var patterned=new byte[width*height];for(int y=0;y<height;++y)for(int x=0;x<width;++x)patterned[y*width+x]=(byte)(25+((x/8+y/8)%4)*65);var same=(byte[])patterned.Clone();var shiftedX=new byte[patterned.Length];var shiftedY=new byte[patterned.Length];for(int y=0;y<height;++y)for(int x=0;x<width;++x){shiftedX[y*width+x]=patterned[y*width+Math.Max(0,x-1)];shiftedY[y*width+x]=patterned[Math.Max(0,y-1)*width+x];}var blank=new byte[patterned.Length];var sparse=new byte[patterned.Length];sparse[(height/2)*width+width/2]=255;var a=AnalyzeLuma(patterned,width,height);return a.ContentValid&&Compare(a,AnalyzeLuma(same,width,height)).Passed&&!Compare(a,AnalyzeLuma(shiftedX,width,height)).Passed&&!Compare(a,AnalyzeLuma(shiftedY,width,height)).Passed&&!AnalyzeLuma(blank,width,height).ContentValid&&!AnalyzeLuma(sparse,width,height).ContentValid;}
}
'@ -ReferencedAssemblies System.Drawing
if(-not[ShowcaseImageEvidence]::RunHostileSelfTests()){throw 'Synthetic blank/sparse/one-pixel-shift hostile self-test failed'}
function Invoke-ShowcaseRun {
  $assetRoot=Join-Path (Split-Path -Parent $Executable) 'assets'
  $materials=Get-Content -Raw -LiteralPath (Join-Path $assetRoot 'material-packs/material-packs.json')|ConvertFrom-Json
  $models=Get-Content -Raw -LiteralPath (Join-Path $assetRoot 'model-packs/model-packs.json')|ConvertFrom-Json
  if($materials.license-ne'CC0-1.0'-or-not$materials.commercialUse-or$materials.resolutionPolicy.distant-ne512-or$materials.resolutionPolicy.environment-ne1024-or$materials.resolutionPolicy.near-ne2048-or$materials.resolutionPolicy.hero-ne4096-or$models.license-ne'CC0-1.0'-or-not$models.commercialUse){throw 'Packaged CC0 license or texture resolution policy evidence failed'}
  foreach($id in @('aerial-ground-rock','bark-brown-02','wood-siding-006','ground-037')){if(-not($materials.packs|Where-Object{$_.id-eq$id-and$_.sourceUrl-and$_.licenseUrl})){throw "Missing material license provenance: $id"}}
  foreach($id in @('quiver-tree-02','rock-09')){if(-not($models.packs|Where-Object{$_.id-eq$id-and$_.sourceUrl-and$_.licenseUrl})){throw "Missing model license provenance: $id"}}
  $process=Start-Process -FilePath $Executable -ArgumentList '--capture-demo' -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
  if(-not$process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw 'Frontier showcase capture timed out'}
  if($process.ExitCode-ne0){throw "Frontier showcase failed: $($process.ExitCode)"}
  $hashes=@{};$hashByName=@{};$metricsByName=@{}
  foreach($name in $names){
    $path=Join-Path $directory $name
    if(-not(Test-Path -LiteralPath $path)-or(Get-Item $path).Length-lt24000){throw "Missing showcase capture: $name"}
    $metrics=[ShowcaseImageEvidence]::Analyze($path);if($metrics.Width-ne1280-or$metrics.Height-ne720){throw "Bad capture dimensions: $name"};if(-not$metrics.ContentValid){throw "Blank/sparse showcase content: $name variance=$($metrics.Variance) entropy=$($metrics.Entropy) coverage=$($metrics.NonBackgroundCoverage) edges=$($metrics.EdgeDensity)"};$metricsByName[$name]=$metrics
    $sha=[System.Security.Cryptography.SHA256]::Create();try{$value=([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($path)))).Replace('-','').ToLowerInvariant();$hashes[$value]=$true;$hashByName[$name]=$value}finally{$sha.Dispose()}
  }
  if($hashes.Count-ne14){throw "Showcase captures must all be unique: $($hashes.Count)"}
  $sha=[System.Security.Cryptography.SHA256]::Create()
  try{$off=[BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes((Join-Path $directory 'feature-off.png'))));$sha.Initialize();$on=[BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes((Join-Path $directory 'feature-on.png'))))}finally{$sha.Dispose()}
  if($off-eq$on){throw 'Feature A/B framebuffer evidence did not differ'}
  $receipt=Get-Content -Raw -LiteralPath (Join-Path $directory 'frontier-showcase-receipt.json')|ConvertFrom-Json
  if($receipt.source-ne'FrontierWorldUltraShowcase'-or$receipt.entry-ne'--capture-demo'-or$receipt.worldCaptures-ne12-or$receipt.diagnosticCaptures-ne2-or$receipt.profiles.Count-ne3-or$receipt.directionalShadowCascades-ne4-or$receipt.contactShadowPasses-lt1-or$receipt.ambientOcclusionPasses-lt1-or$receipt.screenSpaceReflectionPasses-lt1-or$receipt.bloomPasses-lt1-or$receipt.volumetricCloudFrames-lt1-or$receipt.cloudShadowFrames-lt1-or$receipt.lightShaftFrames-lt1-or$receipt.waterRefractionPasses-lt1-or$receipt.featuresExecuted-ne1-or$receipt.pendingGpuReleaseBytes-ne0){throw 'Showcase production feature execution receipt failed'}
  if($receipt.memoryGatePassed-ne1){throw "Showcase memory gate failed: WS=$($receipt.maximumWorkingSetBytes) VRAM=$($receipt.maximumTrackedVramBytes)"}
  if($receipt.gpuGatePassed-ne1-or$receipt.cpuGatePassed-ne1){throw "Showcase frame-time gate failed: GPU p95=$($receipt.gpuP95Nanoseconds) CPU p95=$($receipt.cpuP95Nanoseconds)"}
  if($receipt.slots.Count-ne14){throw 'Showcase slot receipt count failed'}
  $counterNames=@('cascades','softSamples','contact','ao','ssr','bloom','cloud','cloudShadow','shafts','water')
  for($index=0;$index-lt14;++$index){
    $slot=$receipt.slots[$index];$expectedProfile=$(if($index-lt4){'day'}elseif($index-lt8){'dusk'}elseif($index-lt12){'night'}else{'day'});$viewIndex=$(if($index-lt12){$index%4}else{1});$expectedView=@('first','third','god','third')[$viewIndex]
    if($slot.index-ne$index-or$slot.name-ne$names[$index]-or$slot.profile-ne$expectedProfile-or$slot.view-ne$expectedView){throw "Showcase slot identity mismatch: $index"}
    $enabled=$index-ne12
    if([bool]$slot.featureRequested-ne$enabled-or[bool]$slot.featureNormalized-ne$enabled-or[bool]$slot.softShadowNormalized-ne$enabled){throw "Showcase normalized feature state mismatch: $index"}
    foreach($counter in $counterNames){$delta=[uint64]$slot.counters.$counter.delta;if($enabled-and$delta-lt1){throw "Enabled slot did not execute $counter at $index"};if(-not$enabled-and$delta-ne0){throw "Disabled slot executed $counter at $index"}}
  }
  if($receipt.slots[12].worldStateHash-ne$receipt.slots[13].worldStateHash){throw 'Feature A/B world snapshot hash differed'}
  return [pscustomobject]@{receipt=$receipt;hashes=$hashByName;metrics=$metricsByName}
}
$first=Invoke-ShowcaseRun
$second=Invoke-ShowcaseRun
if($first.receipt.runId-eq$second.receipt.runId){throw 'Showcase run identifier was reused'}
foreach($field in @('worldCaptures','diagnosticCaptures','directionalShadowCascades','featuresExecuted','memoryGatePassed')){if($first.receipt.$field-ne$second.receipt.$field){throw "Nondeterministic showcase receipt: $field"}}
function Get-CanonicalSlot($slot){$deltas=[ordered]@{};foreach($counter in @('cascades','softSamples','contact','ao','ssr','bloom','cloud','cloudShadow','shafts','water')){$deltas[$counter]=[uint64]$slot.counters.$counter.delta};return [ordered]@{index=[int]$slot.index;name=[string]$slot.name;profile=[string]$slot.profile;view=[string]$slot.view;featureRequested=[int]$slot.featureRequested;featureNormalized=[int]$slot.featureNormalized;softShadowNormalized=[int]$slot.softShadowNormalized;worldStateHash=[string]$slot.worldStateHash;counterDeltas=$deltas}}
$firstCanonical=@($first.receipt.slots|ForEach-Object{Get-CanonicalSlot $_});$secondCanonical=@($second.receipt.slots|ForEach-Object{Get-CanonicalSlot $_})
if(($firstCanonical|ConvertTo-Json -Depth 5 -Compress)-ne($secondCanonical|ConvertTo-Json -Depth 5 -Compress)){throw 'Two-run canonical 14-slot projection differed'}
$exactShaMatches=0;$captureSlots=@()
foreach($name in $names){$comparison=[ShowcaseImageEvidence]::Compare($first.metrics[$name],$second.metrics[$name]);if(-not$comparison.Passed){throw "Framebuffer determinism failed: $name luma=$($comparison.FullLumaMae) edge=$($comparison.EdgeMae) shift=$($comparison.AlignmentShiftX),$($comparison.AlignmentShiftY)"};if($first.hashes[$name]-eq$second.hashes[$name]){++$exactShaMatches};$captureSlots+=@{name=$name;firstSha256=$first.hashes[$name];secondSha256=$second.hashes[$name];exactShaMatch=($first.hashes[$name]-eq$second.hashes[$name]);fullLumaMae=$comparison.FullLumaMae;edgeMae=$comparison.EdgeMae;alignmentShiftX=$comparison.AlignmentShiftX;alignmentShiftY=$comparison.AlignmentShiftY;bestShiftMae=$comparison.BestShiftMae;firstContent=@{variance=$first.metrics[$name].Variance;entropy=$first.metrics[$name].Entropy;nonBackgroundCoverage=$first.metrics[$name].NonBackgroundCoverage;edgeDensity=$first.metrics[$name].EdgeDensity;meanEdge=$first.metrics[$name].MeanEdge};secondContent=@{variance=$second.metrics[$name].Variance;entropy=$second.metrics[$name].Entropy;nonBackgroundCoverage=$second.metrics[$name].NonBackgroundCoverage;edgeDensity=$second.metrics[$name].EdgeDensity;meanEdge=$second.metrics[$name].MeanEdge}}}
$firstSummary=[ordered]@{worldCaptures=[int]$first.receipt.worldCaptures;diagnosticCaptures=[int]$first.receipt.diagnosticCaptures;featuresExecuted=[int]$first.receipt.featuresExecuted;memoryGatePassed=[int]$first.receipt.memoryGatePassed;gpuGatePassed=[int]$first.receipt.gpuGatePassed;cpuGatePassed=[int]$first.receipt.cpuGatePassed}
$secondSummary=[ordered]@{worldCaptures=[int]$second.receipt.worldCaptures;diagnosticCaptures=[int]$second.receipt.diagnosticCaptures;featuresExecuted=[int]$second.receipt.featuresExecuted;memoryGatePassed=[int]$second.receipt.memoryGatePassed;gpuGatePassed=[int]$second.receipt.gpuGatePassed;cpuGatePassed=[int]$second.receipt.cpuGatePassed}
@{source='FrontierWorldShowcaseDeterminism';hostileSelfTestsPassed=$true;firstRunId=$first.receipt.runId;secondRunId=$second.receipt.runId;exactShaMatches=$exactShaMatches;firstSummary=$firstSummary;secondSummary=$secondSummary;firstCanonicalSlots=$firstCanonical;secondCanonicalSlots=$secondCanonical;slots=$captureSlots}|ConvertTo-Json -Depth 8|Set-Content -Encoding utf8 -LiteralPath (Join-Path $directory 'frontier-showcase-capture-receipt.json')
Write-Output "runs=2 world_captures=24 diagnostics=4 features=$($second.receipt.featuresExecuted) gpu_p95_ns=$($second.receipt.gpuP95Nanoseconds) cpu_p95_ns=$($second.receipt.cpuP95Nanoseconds) vram=$($second.receipt.maximumTrackedVramBytes) working_set=$($second.receipt.maximumWorkingSetBytes)"
