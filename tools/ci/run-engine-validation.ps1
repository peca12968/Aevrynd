param(
    [string]$BuildDirectory = "build-ci",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$resolvedBuild = (Resolve-Path -LiteralPath $BuildDirectory).Path
$artifactDirectory = Join-Path $resolvedBuild "artifacts"
New-Item -ItemType Directory -Force -Path $artifactDirectory | Out-Null
$ctestCommand = Get-Command ctest.exe -ErrorAction SilentlyContinue
$ctestPath = if ($ctestCommand) { $ctestCommand.Source } else { Join-Path $env:ProgramFiles "CMake\bin\ctest.exe" }
if (-not (Test-Path -LiteralPath $ctestPath)) {
    throw "ctest.exe was not found"
}

$computer = Get-CimInstance Win32_ComputerSystem
$processors = @(Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors)
$graphics = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, AdapterRAM)
[pscustomobject]@{
    os = [System.Environment]::OSVersion.VersionString
    totalPhysicalMemoryBytes = [uint64]$computer.TotalPhysicalMemory
    processors = $processors
    graphics = $graphics
} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $artifactDirectory "environment.json")

$testReport = Join-Path $artifactDirectory "test-results.xml"
& $ctestPath --test-dir $resolvedBuild -C $Configuration --output-on-failure --output-junit $testReport
if ($LASTEXITCODE -ne 0) {
    throw "Engine tests failed with exit code $LASTEXITCODE"
}

$benchmarkResults = [ordered]@{}
foreach ($benchmark in @("AevryndRendererBenchmark", "AevryndScene3DBenchmark")) {
    $executable = Join-Path $resolvedBuild "bin\$Configuration\$benchmark.exe"
    $output = @(& $executable 2>&1)
    $exitCode = $LASTEXITCODE
    $output | Tee-Object -FilePath (Join-Path $artifactDirectory "$benchmark.txt")
    if ($exitCode -ne 0) {
        throw "$benchmark failed with exit code $exitCode"
    }
    $benchmarkResults[$benchmark] = ($output -join [Environment]::NewLine)
}

[pscustomobject]@{
    configuration = $Configuration
    testsPassed = $true
    benchmarksPassed = $true
    benchmarkResults = $benchmarkResults
    completedUtc = [DateTime]::UtcNow.ToString("o")
} | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $artifactDirectory "validation-summary.json")
