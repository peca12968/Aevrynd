param([Parameter(Mandatory=$true)][string]$Executable,[switch]$SkipUnavailableGate)
$ErrorActionPreference = 'Stop'
$captureDirectory = Join-Path (Split-Path -Parent $Executable) 'captures'
$names = @('first-person.png','third-person.png','god-view.png','isometric-2-5d.png','network-replication-overview.png','network-interest-durability.png')

function Invoke-CaptureRun {
  if (Test-Path -LiteralPath $captureDirectory) { Remove-Item -LiteralPath $captureDirectory -Recurse -Force }
  $process = Start-Process -FilePath $Executable -ArgumentList '--capture-demo' -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
  if (-not $process.WaitForExit(120000)) { Stop-Process -Id $process.Id -Force; throw 'EngineLab capture timed out' }
  if ($process.ExitCode -ne 0) { throw "EngineLab capture failed: $($process.ExitCode)" }
  foreach ($name in $names) { $path=Join-Path $captureDirectory $name; if (-not (Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -lt 65536) { throw "Missing or empty capture: $name" } }
}

Add-Type -AssemblyName System.Drawing
function Get-VisualReceipt([string]$Path) {
  $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
  try {
    if ($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720) { throw "Unexpected dimensions: $Path" }
    $samples = New-Object System.Collections.Generic.List[int]
    $bits = New-Object System.Text.StringBuilder
    for ($y=22; $y -lt $bitmap.Height; $y+=45) { for ($x=20; $x -lt $bitmap.Width; $x+=40) { $c=$bitmap.GetPixel($x,$y); $l=[int](($c.R*3+$c.G*6+$c.B)/10); $samples.Add($l); [void]$bits.Append($(if($l -ge 96){'1'}else{'0'})) } }
    $minimum=($samples|Measure-Object -Minimum).Minimum; $maximum=($samples|Measure-Object -Maximum).Maximum
    if ($maximum-$minimum -lt 48) { throw "Capture variance too low: $Path" }
    return $bits.ToString()
  } finally { $bitmap.Dispose() }
}

Invoke-CaptureRun
$firstRuntime=Get-Content -Raw -LiteralPath (Join-Path $captureDirectory 'network-runtime-receipt.json')|ConvertFrom-Json
if($firstRuntime.gpuValidationRequested-ne 0-or$firstRuntime.gpuValidationAvailable-ne 0-or$firstRuntime.gpuValidationExecuted-ne 0-or$firstRuntime.gpuValidationPassed-ne 0-or$firstRuntime.debugLayerEnabled-ne 0-or$firstRuntime.gpuValidationErrors-ne 0-or$firstRuntime.gpuValidationWarnings-ne 0-or$firstRuntime.gpuValidationFirstErrorId-ne 0){throw 'Normal capture unexpectedly created GPU validation resources'}
if($firstRuntime.clients-ne 2-or$firstRuntime.visible1-lt 1-or$firstRuntime.visible2-lt 1-or$firstRuntime.interestEntered-lt 1-or$firstRuntime.interestExited-lt 1-or$firstRuntime.reentries-lt 1-or$firstRuntime.despawns-lt 1-or$firstRuntime.authorized-ne 3-or$firstRuntime.durable-ne 3-or$firstRuntime.committed-ne 3-or$firstRuntime.aborted-ne 0-or$firstRuntime.denied-lt 1-or$firstRuntime.restart-ne 1-or$firstRuntime.pending-ne 0-or$firstRuntime.loss-lt 1-or$firstRuntime.duplicate-lt 1-or$firstRuntime.reorder-lt 1){throw 'Runtime telemetry gate failed'}
$firstOverview=Get-VisualReceipt (Join-Path $captureDirectory 'network-replication-overview.png')
$firstDurability=Get-VisualReceipt (Join-Path $captureDirectory 'network-interest-durability.png')
Invoke-CaptureRun
$secondRuntime=Get-Content -Raw -LiteralPath (Join-Path $captureDirectory 'network-runtime-receipt.json')|ConvertFrom-Json
foreach($field in @('seed','serverTick','clients','snapshots','full','delta','acks','loss','duplicate','reorder','client1Interest','client2Interest','visible1','visible2','interestEntered','interestExited','reentries','despawns','authorized','durable','denied','committed','aborted','restart','pending','queuePackets','packetCapacity','queueBytes','byteCapacity','auth1ChunkX','auth1OffsetX','pred1ChunkX','pred1OffsetX','auth2ChunkX','auth2OffsetX','pred2ChunkX','pred2OffsetX','gpuValidationRequested','gpuValidationAvailable','gpuValidationExecuted','gpuValidationPassed','debugLayerEnabled','gpuValidationErrors','gpuValidationWarnings','gpuValidationSource','gpuValidationEntry')){if($firstRuntime.$field-ne$secondRuntime.$field){throw "Runtime telemetry was not deterministic: $field"}}
$secondOverview=Get-VisualReceipt (Join-Path $captureDirectory 'network-replication-overview.png')
$secondDurability=Get-VisualReceipt (Join-Path $captureDirectory 'network-interest-durability.png')
function Get-Hamming([string]$A,[string]$B){$distance=0;for($i=0;$i-lt$A.Length;$i++){if($A[$i]-ne$B[$i]){$distance++}};return $distance}
function Get-Sha256([string]$Path){$stream=[System.IO.File]::OpenRead($Path);try{$sha=[System.Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}}finally{$stream.Dispose()}}
$overviewDistance=Get-Hamming $firstOverview $secondOverview
$durabilityDistance=Get-Hamming $firstDurability $secondDurability
if ($overviewDistance -gt 90 -or $durabilityDistance -gt 90) { throw "Capture structure was not deterministic: $overviewDistance/$durabilityDistance" }
$overviewPath=Join-Path $captureDirectory 'network-replication-overview.png';$durabilityPath=Join-Path $captureDirectory 'network-interest-durability.png'
$overviewSha=Get-Sha256 $overviewPath;$durabilitySha=Get-Sha256 $durabilityPath
if($overviewSha-eq$durabilitySha){throw 'Phase captures must have distinct image hashes'}
$overviewTelemetry=@{serverTick=$secondRuntime.serverTick;snapshots=$secondRuntime.snapshots;full=$secondRuntime.full;delta=$secondRuntime.delta;acks=$secondRuntime.acks;loss=$secondRuntime.loss;duplicate=$secondRuntime.duplicate;reorder=$secondRuntime.reorder;auth1ChunkX=$secondRuntime.auth1ChunkX;auth1OffsetX=$secondRuntime.auth1OffsetX;pred1ChunkX=$secondRuntime.pred1ChunkX;pred1OffsetX=$secondRuntime.pred1OffsetX;auth2ChunkX=$secondRuntime.auth2ChunkX;auth2OffsetX=$secondRuntime.auth2OffsetX;pred2ChunkX=$secondRuntime.pred2ChunkX;pred2OffsetX=$secondRuntime.pred2OffsetX}
$durabilityTelemetry=@{visible1=$secondRuntime.visible1;visible2=$secondRuntime.visible2;interestEntered=$secondRuntime.interestEntered;interestExited=$secondRuntime.interestExited;reentries=$secondRuntime.reentries;despawns=$secondRuntime.despawns;authorized=$secondRuntime.authorized;durable=$secondRuntime.durable;denied=$secondRuntime.denied;committed=$secondRuntime.committed;aborted=$secondRuntime.aborted;restart=$secondRuntime.restart;pending=$secondRuntime.pending;queuePackets=$secondRuntime.queuePackets;packetCapacity=$secondRuntime.packetCapacity;queueBytes=$secondRuntime.queueBytes;byteCapacity=$secondRuntime.byteCapacity}
$captures=@(@{file='network-replication-overview.png';sha256=$overviewSha;width=1280;height=720;variance='pass';phase='replication-overview';telemetry=$overviewTelemetry},@{file='network-interest-durability.png';sha256=$durabilitySha;width=1280;height=720;variance='pass';phase='interest-durability';telemetry=$durabilityTelemetry})
@{schema=1;seed=$secondRuntime.seed;runtime=$secondRuntime;captures=$captures}|ConvertTo-Json -Depth 5|Set-Content -Encoding utf8 -LiteralPath (Join-Path $captureDirectory 'network-capture-receipt.json')
if(-not $SkipUnavailableGate){$env:AEVRYND_GPU_VALIDATION='1';$env:AEVRYND_GPU_VALIDATION_FORCE_UNAVAILABLE='1'
try{
  if(Test-Path -LiteralPath $captureDirectory){Remove-Item -LiteralPath $captureDirectory -Recurse -Force}
  $process=Start-Process -FilePath $Executable -ArgumentList '--capture-demo' -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
  if(-not $process.WaitForExit(120000)){Stop-Process -Id $process.Id -Force;throw 'Unavailable validation capture timed out'}
  if($process.ExitCode-ne 86){throw "Unavailable validation gate returned $($process.ExitCode), expected 86"}
  $unavailable=Get-Content -Raw -LiteralPath (Join-Path $captureDirectory 'network-runtime-receipt.json')|ConvertFrom-Json
  if($unavailable.gpuValidationRequested-ne 1-or$unavailable.gpuValidationAvailable-ne 0-or$unavailable.gpuValidationExecuted-ne 0-or$unavailable.gpuValidationPassed-ne 0-or$unavailable.debugLayerEnabled-ne 0-or$unavailable.gpuValidationErrors-ne 0){throw 'Unavailable validation receipt was not fail-closed'}
}finally{Remove-Item Env:AEVRYND_GPU_VALIDATION -ErrorAction SilentlyContinue;Remove-Item Env:AEVRYND_GPU_VALIDATION_FORCE_UNAVAILABLE -ErrorAction SilentlyContinue}}
Write-Output "captures=6 dimensions=1280x720 overviewHamming=$overviewDistance durabilityHamming=$durabilityDistance variance=pass"
