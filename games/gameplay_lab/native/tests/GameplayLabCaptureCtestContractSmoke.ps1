param([Parameter(Mandatory=$true)][string]$CTestFile)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function ConvertFrom-CMakeCommandLine {
    param([string]$Line,[string]$CommandName)
    $match=[regex]::Match($Line,'^\s*'+[regex]::Escape($CommandName)+'\s*\((?<body>.*)\)\s*$')
    if(-not$match.Success){throw "Malformed $CommandName command"}
    $body=$match.Groups['body'].Value
    $tokens=@()
    $index=0
    while($index-lt$body.Length){
        while($index-lt$body.Length-and[char]::IsWhiteSpace($body[$index])){$index++}
        if($index-ge$body.Length){break}
        if($body[$index]-eq'"'){
            $index++
            $builder=New-Object System.Text.StringBuilder
            $closed=$false
            while($index-lt$body.Length){
                $character=$body[$index]
                if($character-eq'"'){$index++;$closed=$true;break}
                if($character-eq'\'){
                    $index++
                    if($index-ge$body.Length){throw "Malformed quoted argument in $CommandName"}
                    $character=$body[$index]
                    if([char]::IsLetterOrDigit($character)-and$character-cnotin@('t','r','n')){throw "Unknown quoted escape in $CommandName"}
                    if($character-ceq't'){$character="`t"}
                    elseif($character-ceq'r'){$character="`r"}
                    elseif($character-ceq'n'){$character="`n"}
                }
                [void]$builder.Append($character)
                $index++
            }
            if(-not$closed){throw "Malformed quoted argument in $CommandName"}
            if($index-lt$body.Length-and-not[char]::IsWhiteSpace($body[$index])){throw "Concatenated quoted argument in $CommandName"}
            $tokens+=$builder.ToString()
            continue
        }
        if($body[$index]-eq'['){
            $remaining=$body.Substring($index)
            $opening=[regex]::Match($remaining,'^\[(?<equals>=*)\[')
            if(-not$opening.Success){throw "Malformed bracket argument in $CommandName"}
            $closing=']'+$opening.Groups['equals'].Value+']'
            $contentStart=$index+$opening.Length
            $contentEnd=$body.IndexOf($closing,$contentStart,[StringComparison]::Ordinal)
            if($contentEnd-lt0){throw "Malformed bracket argument in $CommandName"}
            $tokens+=$body.Substring($contentStart,$contentEnd-$contentStart)
            $index=$contentEnd+$closing.Length
            if($index-lt$body.Length-and-not[char]::IsWhiteSpace($body[$index])){throw "Concatenated bracket argument in $CommandName"}
            continue
        }
        $start=$index
        while($index-lt$body.Length-and-not[char]::IsWhiteSpace($body[$index])){$index++}
        $token=$body.Substring($start,$index-$start)
        if($token.IndexOfAny([char[]]'();')-ge0){throw "Unsafe unquoted argument in $CommandName"}
        $tokens+=$token
    }
    return $tokens
}

function Test-SamePath {
    param([string]$Actual,[string]$Expected)
    try{return [IO.Path]::GetFullPath($Actual)-ieq[IO.Path]::GetFullPath($Expected)}catch{return $false}
}

if(-not(Test-Path -LiteralPath $CTestFile -PathType Leaf)){throw "Missing generated CTest file: $CTestFile"}
$cTestPath=(Resolve-Path -LiteralPath $CTestFile).Path
$buildRoot=Split-Path -Parent $cTestPath
1..3|ForEach-Object{$buildRoot=Split-Path -Parent $buildRoot}
$captureScript=Join-Path $PSScriptRoot 'GameplayLabCaptureSmoke.ps1'
$testNames=@('GameplayLabCaptureSmoke','GameplayLabGpuValidationCaptureSmoke')
$actualRecords=@{GameplayLabCaptureSmoke=@();GameplayLabGpuValidationCaptureSmoke=@()}
$notAvailableCounts=@{GameplayLabCaptureSmoke=0;GameplayLabGpuValidationCaptureSmoke=0}
$propertyRecords=@{GameplayLabCaptureSmoke=@();GameplayLabGpuValidationCaptureSmoke=@()}

foreach($line in Get-Content -LiteralPath $cTestPath){
    if($line-notmatch'GameplayLab(?:GpuValidation)?CaptureSmoke'){continue}
    if($line-match'^\s*add_test\s*\('){
        $tokens=@(ConvertFrom-CMakeCommandLine -Line $line -CommandName 'add_test')
        if($tokens.Count-lt2-or$tokens[0]-cnotin$testNames){throw 'Unexpected or malformed Gameplay capture test name'}
        $name=$tokens[0]
        if($tokens.Count-eq2-and$tokens[1]-ceq'NOT_AVAILABLE'){$notAvailableCounts[$name]++;continue}
        $actualRecords[$name]=@($actualRecords[$name])+(,$tokens)
        continue
    }
    if($line-match'^\s*set_tests_properties\s*\('){
        $tokens=@(ConvertFrom-CMakeCommandLine -Line $line -CommandName 'set_tests_properties')
        if($tokens.Count-lt4-or$tokens[0]-cnotin$testNames){throw 'Unexpected or malformed Gameplay capture property name'}
        $propertyRecords[$tokens[0]]=@($propertyRecords[$tokens[0]])+(,$tokens)
        continue
    }
    throw 'Unexpected Gameplay capture CTest reference'
}

$seenConfigurations=@{GameplayLabCaptureSmoke=@{};GameplayLabGpuValidationCaptureSmoke=@{}}
$configurations=@('Debug','Release','MinSizeRel','RelWithDebInfo')
foreach($name in $testNames){
    if($actualRecords[$name].Count-lt1){throw "Generated $name command is missing"}
    if($notAvailableCounts[$name]-ne1){throw "Generated $name must contain exactly one NOT_AVAILABLE branch"}
    foreach($tokens in $actualRecords[$name]){
        $isGpu=$name-ceq'GameplayLabGpuValidationCaptureSmoke'
        $expected=@($name,'powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',$captureScript,'-Executable',$null,'-IdentityContractProbe',$null)
        if($isGpu){$expected+=@('-GpuValidation','-SingleRun')}
        $expected+=@('-ProcessTimeoutSeconds',$(if($isGpu){'180'}else{'120'}))
        if($tokens.Count-ne$expected.Count){throw "$name argument contract changed"}
        foreach($position in 0..($expected.Count-1)){
            if($position-in6,8,10){continue}
            if($tokens[$position]-cne$expected[$position]){throw "$name argument contract changed at position $position"}
        }
        if(-not(Test-SamePath $tokens[6] $captureScript)){throw "$name PowerShell path changed"}
        $matchedConfiguration=$null
        foreach($configuration in $configurations){
            $binaryDirectory=Join-Path $buildRoot 'bin'
            if($configuration){$binaryDirectory=Join-Path $binaryDirectory $configuration}
            $expectedExecutable=Join-Path $binaryDirectory 'Aevrynd-Gameplay-Lab.exe'
            $expectedProbe=Join-Path $binaryDirectory 'Aevrynd-Gameplay-Object-Identity-Contract.exe'
            if((Test-SamePath $tokens[8] $expectedExecutable)-and(Test-SamePath $tokens[10] $expectedProbe)){
                $matchedConfiguration=$configuration
                break
            }
        }
        if($null-eq$matchedConfiguration){throw "$name executable or identity probe path changed"}
        if($seenConfigurations[$name].ContainsKey($matchedConfiguration)){throw "$name command is duplicated for $matchedConfiguration"}
        $seenConfigurations[$name][$matchedConfiguration]=$true
    }
    if($propertyRecords[$name].Count-ne$actualRecords[$name].Count){throw "$name timeout property count changed"}
    foreach($tokens in $propertyRecords[$name]){
        if($tokens[0]-cne$name-or$tokens[1]-cne'PROPERTIES'-or(($tokens.Count-2)%2)-ne0){throw "$name property contract changed"}
        $propertyKeys=@()
        $timeoutValue=$null
        $labelsValue=$null
        for($position=2;$position-lt$tokens.Count;$position+=2){
            $key=$tokens[$position]
            if($key-cnotin@('LABELS','TIMEOUT','_BACKTRACE_TRIPLES')){throw "$name contains an unexpected property"}
            if($key-cin$propertyKeys){throw "$name property $key is duplicated"}
            $propertyKeys+=$key
            if($key-ceq'TIMEOUT'){$timeoutValue=$tokens[$position+1]}
            elseif($key-ceq'LABELS'){$labelsValue=$tokens[$position+1]}
        }
        $expectedTimeout=if($name-ceq'GameplayLabCaptureSmoke'){'420'}else{'240'}
        if($null-eq$timeoutValue-or$timeoutValue-cne$expectedTimeout-or$labelsValue-cne'visual-gates'){throw "$name overall timeout/label contract changed"}
    }
    $observedConfigurations=@($seenConfigurations[$name].Keys|Sort-Object)
    $expectedConfigurations=@($configurations|Sort-Object)
    if(($observedConfigurations-join',')-cne($expectedConfigurations-join',')){throw "$name must contain exactly the four Visual Studio configurations"}
}

$normalConfigurations=@($seenConfigurations['GameplayLabCaptureSmoke'].Keys|Sort-Object)
$gpuConfigurations=@($seenConfigurations['GameplayLabGpuValidationCaptureSmoke'].Keys|Sort-Object)
if(($normalConfigurations-join',')-cne($gpuConfigurations-join',')){throw 'Normal and GBV CTest configuration coverage differs'}
Write-Output "GameplayLabCaptureCtestContractSmoke PASS normalCommands=$($actualRecords['GameplayLabCaptureSmoke'].Count) gbvCommands=$($actualRecords['GameplayLabGpuValidationCaptureSmoke'].Count) quoting=double-or-bracket"
