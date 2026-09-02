param([Parameter(Mandatory=$true)][string]$CTestFile)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

if(-not(Test-Path -LiteralPath $CTestFile -PathType Leaf)){throw "Missing generated CTest file: $CTestFile"}
$text=Get-Content -Raw -LiteralPath $CTestFile
$gbv=@([regex]::Matches($text,'(?m)^\s*add_test\("GameplayLabGpuValidationCaptureSmoke"\s+(.+)\)\r?$')|Where-Object{$_.Groups[1].Value-ne'NOT_AVAILABLE'})
$normal=@([regex]::Matches($text,'(?m)^\s*add_test\("GameplayLabCaptureSmoke"\s+(.+)\)\r?$')|Where-Object{$_.Groups[1].Value-ne'NOT_AVAILABLE'})
if($gbv.Count-lt1-or$normal.Count-lt1){throw 'Generated Gameplay capture CTest commands are missing'}
foreach($command in $gbv){if($command.Groups[1].Value-notmatch'(?i)"-GpuValidation"'-or$command.Groups[1].Value-notmatch'(?i)"-SingleRun"'){throw 'GBV CTest must request GPU validation and exactly one run'}}
foreach($command in $normal){if($command.Groups[1].Value-match'(?i)"-SingleRun"'-or$command.Groups[1].Value-match'(?i)"-GpuValidation"'){throw 'Normal CTest must preserve the explicit three-run non-GBV contract'}}
foreach($command in $normal){if($command.Groups[1].Value-notmatch'(?i)"-ProcessTimeoutSeconds"\s+"120"'){throw 'Normal CTest must preserve the 120-second process timeout'}}
foreach($command in $gbv){if($command.Groups[1].Value-notmatch'(?i)"-ProcessTimeoutSeconds"\s+"180"'){throw 'GBV CTest must use the bounded 180-second process timeout'}}
$normalTimeout=@([regex]::Matches($text,'(?m)^\s*set_tests_properties\("GameplayLabCaptureSmoke"\s+PROPERTIES\s+TIMEOUT\s+"([0-9]+)".+\)\r?$'))
$gbvTimeout=@([regex]::Matches($text,'(?m)^\s*set_tests_properties\("GameplayLabGpuValidationCaptureSmoke"\s+PROPERTIES\s+TIMEOUT\s+"([0-9]+)".+\)\r?$'))
if($normalTimeout.Count-lt1-or@($normalTimeout|Where-Object{[int]$_.Groups[1].Value-ne300}).Count){throw 'Normal CTest overall timeout contract changed'}
if($gbvTimeout.Count-lt1-or@($gbvTimeout|Where-Object{[int]$_.Groups[1].Value-lt210-or[int]$_.Groups[1].Value-gt300}).Count){throw 'GBV CTest overall timeout must remain between 210 and 300 seconds'}
Write-Output "GameplayLabCaptureCtestContractSmoke PASS normalCommands=$($normal.Count) gbvCommands=$($gbv.Count)"
