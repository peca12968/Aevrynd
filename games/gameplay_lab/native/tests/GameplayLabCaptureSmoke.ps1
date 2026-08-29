param([Parameter(Mandatory=$true)][string]$Executable,[switch]$GpuValidation)
$ErrorActionPreference='Stop'
if($GpuValidation){$env:AEVRYND_GPU_VALIDATION='1'}else{Remove-Item Env:AEVRYND_GPU_VALIDATION -ErrorAction SilentlyContinue}
$directory=Join-Path (Split-Path -Parent $Executable) $(if($GpuValidation){'gameplay-captures-gbv'}else{'gameplay-captures-normal'})
$names=@('first-day.png','third-day.png','god-day.png','first-dusk.png','third-dusk.png','god-dusk.png','first-night.png','third-night.png','god-night.png','palette-building.png','melee-combat.png')
$process=Start-Process -FilePath $Executable -ArgumentList '--capture-demo' -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
if(-not$process.WaitForExit(120000)){Stop-Process -Id $process.Id -Force;throw 'Gameplay Lab capture timed out'}
if($process.ExitCode-ne 0){throw "Gameplay Lab capture failed: $($process.ExitCode)"}
Add-Type -AssemblyName System.Drawing
$hashes=@{}
foreach($name in $names){$path=Join-Path $directory $name;if(-not(Test-Path -LiteralPath $path)-or(Get-Item $path).Length-lt 24000){throw "Missing capture: $name"};$bitmap=[System.Drawing.Bitmap]::FromFile($path);try{if($bitmap.Width-ne1280-or$bitmap.Height-ne720){throw "Bad dimensions: $name"};$minimum=255;$maximum=0;for($y=20;$y-lt720;$y+=60){for($x=20;$x-lt1280;$x+=60){$c=$bitmap.GetPixel($x,$y);$v=[int](($c.R+$c.G+$c.B)/3);$minimum=[Math]::Min($minimum,$v);$maximum=[Math]::Max($maximum,$v)}};if($maximum-$minimum-lt35){throw "Low variance: $name"}}finally{$bitmap.Dispose()};$sha=[System.Security.Cryptography.SHA256]::Create();try{$hashes[[BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($path)))]=$true}finally{$sha.Dispose()}}
if($hashes.Count-lt11){throw 'Gameplay captures were not unique'}
$receipt=Get-Content -Raw -LiteralPath (Join-Path $directory 'gameplay-receipt.json')|ConvertFrom-Json
$poseReceipt=Get-Content -Raw -LiteralPath (Join-Path $directory 'gameplay-pose-receipt.json')|ConvertFrom-Json
if($poseReceipt.runId-ne$receipt.runId-or$poseReceipt.source-ne'GameplayLabSemanticSkinning'-or$poseReceipt.female.Count-ne11-or$poseReceipt.male.Count-ne11-or@($poseReceipt.female|Select-Object -Unique).Count-ne11-or@($poseReceipt.male|Select-Object -Unique).Count-ne11){throw 'Semantic F2/M2 pose receipt failed'}
if($receipt.source-ne'GameplayLabFramebuffer'-or$receipt.entry-ne'--capture-demo'-or$receipt.runId-notmatch'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'-or$receipt.captures-ne11-or$receipt.entities-lt18-or$receipt.physicsBodies-lt19-or$receipt.physicsSteps-ne$receipt.ticks-or$receipt.cameraSwitches-lt3-or$receipt.jumps-lt1-or$receipt.crouches-lt1-or$receipt.palettePreviews-lt1-or$receipt.paletteValidPreviews-lt1-or$receipt.paletteInvalidPreviews-lt1-or$receipt.selectedCategory-ne5-or$receipt.paletteCommits-lt1-or$receipt.combatSweeps-lt3-or$receipt.combatHits-ne3-or$receipt.combatKills-ne1-or$receipt.monsterHealth-ne0-or$receipt.animationState-ne8-or$receipt.monsterAnimationState-ne10-or$receipt.corpseVisible-ne1-or$receipt.femaleGpuDraws-lt1-or$receipt.maleGpuDraws-lt1-or$receipt.femaleModelIdentity-eq0-or$receipt.maleModelIdentity-eq0-or$receipt.femaleModelIdentity-eq$receipt.maleModelIdentity-or$receipt.femalePoseHash-eq0-or$receipt.malePoseHash-eq0-or[Math]::Abs($receipt.minimumControllerHeight-1.15)-gt0.001-or$receipt.headroomRejected-lt1-or$receipt.crouchFeetInvariant-ne1-or$receipt.weaponAttachmentPassed-ne1-or$receipt.weaponHitWindowEvidence-ne1-or$receipt.lighting.Count-ne3-or$receipt.gpuErrors-ne0-or$receipt.gpuWarnings-ne0){throw 'Gameplay receipt gate failed'}
if($GpuValidation-and($receipt.gpuValidationRequested-ne1-or$receipt.gpuValidationAvailable-ne1-or$receipt.gpuValidationExecuted-ne1-or$receipt.debugLayerEnabled-ne1-or$receipt.gpuValidationPassed-ne1)){throw 'Gameplay GPU validation was requested but did not execute and pass'}
if(-not$GpuValidation-and($receipt.gpuValidationRequested-ne0-or$receipt.gpuValidationAvailable-ne0-or$receipt.gpuValidationExecuted-ne0-or$receipt.debugLayerEnabled-ne0-or$receipt.gpuValidationPassed-ne0)){throw 'Normal Gameplay Lab capture unexpectedly enabled GPU validation'}
$first=$receipt
$process=Start-Process -FilePath $Executable -ArgumentList '--capture-demo' -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
if(-not$process.WaitForExit(120000)){Stop-Process -Id $process.Id -Force;throw 'Second deterministic capture timed out'}
if($process.ExitCode-ne0){throw "Second deterministic capture failed: $($process.ExitCode)"}
$second=Get-Content -Raw -LiteralPath (Join-Path $directory 'gameplay-receipt.json')|ConvertFrom-Json
if($first.runId-eq$second.runId){throw 'Capture run identifier was reused'}
$fields=@('captures','entities','physicsBodies','physicsSteps','cameraSwitches','jumps','crouches','palettePreviews','paletteCommits','combatSweeps','combatHits','combatKills','monsterHealth','animationState')
foreach($field in $fields){if($first.$field-ne$second.$field){throw "Nondeterministic gameplay telemetry: $field"}}
if([Math]::Abs([int64]$first.ticks-[int64]$second.ticks)-gt1){throw 'Capture completion varied by more than one presentation frame'}
$captureEntries=@();foreach($name in $names){$path=Join-Path $directory $name;$sha=[System.Security.Cryptography.SHA256]::Create();try{$captureEntries+=@{name=$name;sha256=([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($path))).Replace('-','').ToLowerInvariant())}}finally{$sha.Dispose()}}
@{runId=$second.runId;source='GameplayLabFramebuffer';captures=$captureEntries;telemetry=$second}|ConvertTo-Json -Depth 5|Set-Content -Encoding utf8 -LiteralPath (Join-Path $directory 'gameplay-capture-receipt.json')
Write-Output "captures=11 unique=11 deterministic=2 entities=$($receipt.entities) physics=$($receipt.physicsBodies) sweeps=$($receipt.combatSweeps) hits=$($receipt.combatHits) kills=$($receipt.combatKills)"
