[CmdletBinding()]
param([string]$ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if( -not $ProjectRoot){$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path}
$builder = Join-Path $ProjectRoot 'tools\ci\New-EngineLabNetworkReleaseCandidate.ps1'

function Assert-True([bool]$Condition,[string]$Message) { if( -not $Condition){throw $Message} }
function Invoke-ExpectedFailure([string]$Sandbox,[string]$Candidate,[string]$FailPoint) {
    $failed=$false
    try { & $builder -ProjectRoot $ProjectRoot -SandboxRoot $Sandbox -CandidateRoot $Candidate -TestFailPoint $FailPoint } catch { $failed=$true;Write-Host "expectedFailure=$FailPoint message=$($_.Exception.Message)" }
    Assert-True $failed "Fail point was accepted: $FailPoint"
    Assert-True ( -not(Test-Path -LiteralPath $Candidate)) "Failed builder left candidate: $FailPoint"
    Assert-True ( -not(@(Get-ChildItem -LiteralPath $Sandbox -Force|Where-Object Name -like '.a-*').Count)) "Failed builder left temp state: $FailPoint"
}

$tokens=$null;$errors=$null
[Management.Automation.Language.Parser]::ParseFile($builder,[ref]$tokens,[ref]$errors)|Out-Null
if($errors.Count){throw "Builder parser errors: $(@($errors|ForEach-Object Message) -join '; ')"}
$operatorPattern='[^\s]-(eq|ne|lt|le|gt|ge|like|notlike|match|notmatch|contains|notcontains|in|notin|and|or|xor|is|isnot|as|not)\b'
$operatorHits=@(Select-String -LiteralPath $builder -Pattern $operatorPattern -AllMatches)
Assert-True ($operatorHits.Count -eq 0) 'Builder contains no-space PowerShell operators.'
$builderText=[IO.File]::ReadAllText($builder)
foreach($requiredFlag in @('/m:1','/nr:false','/p:UseMultiToolTask=true','/p:CL_MPCount=1','/p:EnforceProcessCountAcrossBuilds=true')){Assert-True ($builderText.Contains("'$requiredFlag'")) "Builder is missing safe MSBuild flag: $requiredFlag"}
$unstableMultiToolSetting='UseMultiToolTask='+'false'
Assert-True ( -not $builderText.Contains($unstableMultiToolSetting)) 'Builder retains the unstable disabled multi-tool setting.'
Assert-True ($builderText.Contains("'MSBuildProperties'")) 'Builder does not record the authoritative compiler process limit evidence.'
Assert-True ($builderText.Contains('configuredMaximumClProcessCount=1')) 'Builder does not record the configured maximum CL process count.'

$sandbox=Join-Path ([IO.Path]::GetTempPath()) ('AevryndCandidateTest-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $sandbox|Out-Null
try {
    foreach($selfTest in @('RunnerSelfTest','VerifierSelfTest')){& $builder -ProjectRoot $ProjectRoot -SandboxRoot $sandbox -CandidateRoot (Join-Path $sandbox ('self-'+$selfTest)) -TestFailPoint $selfTest}
    foreach($point in @('DirtySource','StaleSource','TempReparse')){Invoke-ExpectedFailure $sandbox (Join-Path $sandbox ('candidate-'+$point)) $point}
    $existing=Join-Path $sandbox 'existing';New-Item -ItemType Directory -Path $existing|Out-Null
    $rejected=$false;try{& $builder -ProjectRoot $ProjectRoot -SandboxRoot $sandbox -CandidateRoot $existing}catch{$rejected=$true}
    Assert-True $rejected 'Existing candidate root was accepted.'
    $outside=Join-Path ([IO.Path]::GetTempPath()) ('outside-'+[Guid]::NewGuid().ToString('N'))
    $rejected=$false;try{& $builder -ProjectRoot $ProjectRoot -SandboxRoot $sandbox -CandidateRoot $outside}catch{$rejected=$true}
    Assert-True $rejected 'Candidate outside direct sandbox child was accepted.'
    Write-Host 'EngineLab candidate hostile preflight PASS cases=5'
} finally {
    if(Test-Path -LiteralPath $sandbox){Remove-Item -LiteralPath $sandbox -Recurse -Force}
}
