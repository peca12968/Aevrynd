param([string]$Baseline,[string]$Candidate,[switch]$SelfTest,[string]$NativeBuildDirectory)
$ErrorActionPreference='Stop'
# 只比較已記錄契約，不重播、不改鏡位、不取代正式 production gates。
function Compare-Contract($a,$b) {
  $fields=@('schemaVersion','replaySupported','shot','collisionClear','cameraRawBits','actorRawBits','clipPhaseRaw','frozenPoseHashes','footPlacementHash','controllerHash','companionRawBits','viewmodelHash','exposureRawBits')
  foreach($value in @($a,$b)) {
    if($null-eq$value-or@($value.PSObject.Properties).Count-ne$fields.Count){return 'schema-fields'}
    foreach($field in $fields){if($null-eq$value.PSObject.Properties[$field]-or$null-eq$value.$field){return "missing-$field"}}
    if($value.schemaVersion-ne1-or$value.replaySupported-cne$false){return 'schema-version'}
    if($value.collisionClear-cne$true){return 'collision'}
    foreach($pair in @(@('cameraRawBits',14),@('actorRawBits',17),@('clipPhaseRaw',6),@('frozenPoseHashes',3),@('companionRawBits',17))){if(@($value.($pair[0])).Count-ne$pair[1]){return "dimension-$($pair[0])"}}
    foreach($field in @('cameraRawBits','actorRawBits','clipPhaseRaw','companionRawBits','exposureRawBits')){
      foreach($raw in @($value.$field)){if($raw-is[string]-or$raw-is[bool]-or[double]$raw-lt0-or[double]$raw-gt4294967295-or[double]$raw-ne[math]::Floor([double]$raw)){return "invalid-$field"}}
    }
    foreach($field in @('frozenPoseHashes','footPlacementHash','controllerHash','viewmodelHash')){foreach($hash in @($value.$field)){[uint64]$parsed=0;if($hash-isnot[string]-or-not[uint64]::TryParse($hash,[ref]$parsed)){return "invalid-$field"}}}
    $floatBits=@($value.cameraRawBits[0..8]) + @($value.cameraRawBits[10..13]) + @($value.actorRawBits[0..15]) + @($value.clipPhaseRaw[1,3,5]) + @($value.companionRawBits[0..15]) + @($value.exposureRawBits)
    foreach($raw in $floatBits){if(([uint64]$raw-band2139095040)-eq2139095040){return 'nonfinite-float'}}
  }
  foreach($field in $fields){if(($a.$field|ConvertTo-Json -Compress)-cne($b.$field|ConvertTo-Json -Compress)){return "mismatch-$field"}}
  return ''
}
if($SelfTest){
  if($NativeBuildDirectory){
    # 擷取正式函式本體編譯，不在測試重製碰撞公式。
    $root=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    $source=Get-Content (Join-Path $root 'games/gameplay_lab/native/src/GameplayLabGame.cpp') -Raw -Encoding UTF8
    $body=[regex]::Match($source,'(?s)// COMPARE_COLLISION_CPU_BEGIN\s*(.*?)// COMPARE_COLLISION_CPU_END').Groups[1].Value
    if(-not$body){throw 'native collision seam missing'}
    $directory=Join-Path ([IO.Path]::GetTempPath()) ('compare-collision-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $directory|Out-Null
    $test=@'
#include <Aevrynd/CollisionWorld3D.h>
#include <iostream>
'@ + "`n" + $body + @'

int main(){using namespace Aevrynd;CollisionWorld3D world;
const Vec3 camera{0,2,-4},center{0,.9f,0},extents{.45f,.9f,.45f};
if(!CompareCaptureCollisionClear(world,camera,center,extents,true))return 1;
const auto floor=world.CreateBox({{{0,-.5f,0},{10,.5f,10}},1,0xffffffffu});
if(!floor||!CompareCaptureCollisionClear(world,camera,center,extents,true))return 2;
const auto wall=world.CreateBox({{center,{.1f,.2f,.1f}},1,0xffffffffu});
if(!wall||CompareCaptureCollisionClear(world,camera,center,extents,true))return 3;
world.Destroy(wall);const auto obstruction=world.CreateBox({{camera,{.2f,.2f,.2f}},1,0xffffffffu});
if(!obstruction||CompareCaptureCollisionClear(world,camera,center,extents,true))return 4;
world.Destroy(obstruction);if(CompareCaptureCollisionClear(world,camera,center,extents,false))return 5;
std::cout<<"COMPARE_COLLISION_CPU=PASS empty/floor/actor-wall/camera-wall/terrain\n";}
'@
    [IO.File]::WriteAllText((Join-Path $directory 'test.cpp'),$test,[Text.UTF8Encoding]::new($true))
    $vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $command='call "'+$vs+'\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul && cl /nologo /std:c++20 /EHsc /MD /utf-8 /I"'+$root+'\engine\native\include" "'+$directory+'\test.cpp" /Fo:"'+$directory+'\test.obj" /Fe:"'+$directory+'\test.exe" "'+$NativeBuildDirectory+'\engine\native\Release\AevryndEngine.lib" && "'+$directory+'\test.exe"'
    & cmd /c $command
    if($LASTEXITCODE-ne0){throw "native collision CPU failed: $LASTEXITCODE artifacts=$directory"}
    Write-Output "collisionEvidence=$directory"
  }
  $a=[pscustomobject]@{schemaVersion=1;replaySupported=$false;shot='first-day.png';collisionClear=$true;cameraRawBits=@(0)*14;actorRawBits=@(0)*17;clipPhaseRaw=@(0)*6;frozenPoseHashes=@('1','2','3');footPlacementHash='4';controllerHash='5';companionRawBits=@(0)*17;viewmodelHash='6';exposureRawBits=0}
  function Clone($x){return ($x|ConvertTo-Json -Depth 10|ConvertFrom-Json)}
  if((Compare-Contract $a (Clone $a))-ne''){throw 'equal failed'}
  foreach($field in @('cameraRawBits','actorRawBits','clipPhaseRaw','companionRawBits','frozenPoseHashes')){$b=Clone $a;$b.$field[0]=if($field-eq'frozenPoseHashes'){'9'}else{1};if((Compare-Contract $a $b)-eq''){throw "mutation accepted $field"}}
  foreach($field in @('controllerHash','footPlacementHash','viewmodelHash','shot','exposureRawBits','schemaVersion')){$b=Clone $a;$b.$field=if($field-in@('exposureRawBits','schemaVersion')){2}else{'9'};if((Compare-Contract $a $b)-eq''){throw "mutation accepted $field"}}
  $b=Clone $a;$b.collisionClear=$false;if((Compare-Contract $a $b)-ne'collision'){throw 'collision accepted'}
  $b=Clone $a;$b.cameraRawBits=@(0);if((Compare-Contract $a $b)-notlike'dimension-*'){throw 'dimension accepted'}
  $b=Clone $a;$b.PSObject.Properties.Remove('controllerHash');if((Compare-Contract $a $b)-eq''){throw 'missing accepted'}
  $b=Clone $a;$b.cameraRawBits[0]=2143289344;if((Compare-Contract $b $b)-ne'nonfinite-float'){throw 'NaN accepted'}
  Write-Output 'COMPARE_CONTRACT_CPU=PASS replaySupported=false';exit 0
}
$shots=@('first-day','third-day','god-day','first-dusk','third-dusk','god-dusk','first-night','third-night','god-night','palette-building','melee-combat')
$comparable=$true
foreach($shot in $shots){
  try{$a=Get-Content -LiteralPath (Join-Path $Baseline "$shot.compare-v1.json") -Raw|ConvertFrom-Json;$b=Get-Content -LiteralPath (Join-Path $Candidate "$shot.compare-v1.json") -Raw|ConvertFrom-Json;$reason=Compare-Contract $a $b;if($a.shot-cne"$shot.png"-or$b.shot-cne"$shot.png"){$reason='shot-identity'}}catch{$reason='missing-or-invalid-input'}
  $ok=$reason-eq'';$comparable=$comparable-and$ok
  [pscustomobject]@{schemaVersion=1;shot=$shot;comparable=$ok;reason=$reason;replaySupported=$false}|ConvertTo-Json -Compress
}
if(-not$comparable){exit 10}
