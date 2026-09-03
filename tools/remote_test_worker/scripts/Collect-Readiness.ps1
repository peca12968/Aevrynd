[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('taichung', 'taoyuan')]
    [string]$NodeId,

    [string]$OutputFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([string]::IsNullOrWhiteSpace($OutputFile)) {
    $OutputFile = Join-Path $PSScriptRoot '..\artifacts\readiness.json'
}

# 僅收集執行能力；輸出不包含路徑、IP、使用者或認證資料。
function Get-ToolVersion {
    param([Parameter(Mandatory)][ValidateSet('git', 'cmake', 'ctest', 'python')][string]$Name)
    try {
        $lines = switch ($Name) {
            'git' { & git --version }
            'cmake' { & cmake --version }
            'ctest' { & ctest --version }
            'python' { & python --version }
        }
        $line = @($lines) | Select-Object -First 1
        if ($LASTEXITCODE -ne 0) { return $null }
        $match = [regex]::Match([string]$line, '\d+(?:\.\d+){1,3}')
        if ($match.Success) { return $match.Value }
    } catch { }
    return $null
}

$repoRoot = 'C:\ProgramData\Aevrynd\RemoteTestWorker\Repository'
$presetFile = Join-Path $repoRoot 'CMakePresets.json'
$rootCmake = Join-Path $repoRoot 'CMakeLists.txt'
$presetReady = $false
$labelReady = $false
$originReady = $false
if (Test-Path -LiteralPath (Join-Path $repoRoot '.git')) {
    try {
        $observedOrigin = (& git -C $repoRoot remote get-url origin 2>$null | Select-Object -First 1)
        $originReady = ([string]$observedOrigin).Trim() -ceq 'https://github.com/peca12968/Aevrynd.git'
    } catch { }
}
if (Test-Path -LiteralPath $presetFile -PathType Leaf) {
    $presetReady = (Get-Content -LiteralPath $presetFile -Raw) -match 'windows-release-gates'
}
if (Test-Path -LiteralPath $rootCmake -PathType Leaf) {
    $labelReady = (Get-Content -LiteralPath $rootCmake -Raw) -match 'release-gates'
}
$gitVersion = Get-ToolVersion -Name 'git'
$cmakeVersion = Get-ToolVersion -Name 'cmake'
$ctestVersion = Get-ToolVersion -Name 'ctest'
$pythonVersion = Get-ToolVersion -Name 'python'
$ready = [bool]($gitVersion -and $cmakeVersion -and $ctestVersion -and $pythonVersion -and $originReady -and $presetReady -and $labelReady)
$manifest = [ordered]@{
    schemaVersion = 1
    nodeId = $NodeId
    ready = $ready
    capabilities = [ordered]@{
        git = $gitVersion
        cmake = $cmakeVersion
        ctest = $ctestVersion
        python = $pythonVersion
        fixedRepository = (Test-Path -LiteralPath (Join-Path $repoRoot '.git'))
        originContract = $originReady
        presetContract = $presetReady
        labelContract = $labelReady
        noWindowBelowNormal = ($env:OS -eq 'Windows_NT')
    }
    policy = [ordered]@{
        originId = 'github-peca12968-aevrynd'
        presets = @('windows-release-gates')
        labels = @('release-gates')
        timeoutsSeconds = @(300, 900, 1800)
        maxConcurrentJobs = 1
    }
}
$parent = Split-Path -Parent $OutputFile
New-Item -ItemType Directory -Path $parent -Force | Out-Null
$temporary = "$OutputFile.tmp"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $temporary -Encoding UTF8
Move-Item -LiteralPath $temporary -Destination $OutputFile -Force
# Readiness 僅含能力布林/版本；仍限制為系統與系統管理員可修改，避免安裝前被低權限偽造。
if ($env:OS -eq 'Windows_NT') {
    & icacls.exe $OutputFile /inheritance:r /grant:r '*S-1-5-18:(F)' '*S-1-5-32-544:(F)' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'readiness_acl_failed' }
}
Write-Output ($manifest | ConvertTo-Json -Depth 5 -Compress)
