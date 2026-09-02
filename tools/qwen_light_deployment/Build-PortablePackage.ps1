param(
    [string]$RuntimeSource='',
    [string]$OutputDirectory=''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$sourceRoot=[IO.Path]::GetFullPath($PSScriptRoot)
$workspaceRoot=[IO.Path]::GetFullPath((Join-Path $sourceRoot '..\..\..'))
if([string]::IsNullOrWhiteSpace($RuntimeSource)){$RuntimeSource=Join-Path $workspaceRoot 'output\packages\Aevrynd-Qwen-Remote-Host-192.168.50.98\runtime'}
if([string]::IsNullOrWhiteSpace($OutputDirectory)){$OutputDirectory=Join-Path $workspaceRoot 'output\packages\qwen-light-build'}
$runtimeRoot=[IO.Path]::GetFullPath($RuntimeSource)
$outputRoot=[IO.Path]::GetFullPath($OutputDirectory)
if(-not(Test-Path -LiteralPath $runtimeRoot -PathType Container)){throw 'Runtime source directory is missing.'}
$manifestLines=Get-Content -LiteralPath (Join-Path $sourceRoot 'runtime-manifest.sha256')
$expected=@{}
foreach($line in $manifestLines){
    if($line-notmatch'^([0-9a-f]{64})  runtime/([A-Za-z0-9_.-]+)$'){throw 'Runtime manifest contains an invalid or unsafe path.'}
    if($expected.ContainsKey($Matches[2])){throw 'Runtime manifest contains a duplicate path.'}
    $expected[$Matches[2]]=$Matches[1]
}
$runtimeFiles=Get-ChildItem -LiteralPath $runtimeRoot -File
if($runtimeFiles.Count-ne$expected.Count){throw 'Runtime source does not match the closed file set.'}
foreach($file in $runtimeFiles){
    if(-not$expected.ContainsKey($file.Name)){throw "Unexpected runtime file: $($file.Name)"}
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()-ne$expected[$file.Name]){throw "Runtime hash mismatch: $($file.Name)"}
}
if(-not(Test-Path -LiteralPath $outputRoot)){New-Item -ItemType Directory -Path $outputRoot|Out-Null}
$staging=Join-Path $outputRoot 'Aevrynd-Qwen-Light-Portable.staging'
$zipPath=Join-Path $outputRoot 'Aevrynd-Qwen-Light-Portable.zip'
foreach($path in @($staging,$zipPath)){
    $full=[IO.Path]::GetFullPath($path)
    if(-not$full.StartsWith($outputRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Build target escaped the output directory.'}
    if(Test-Path -LiteralPath $full){Remove-Item -LiteralPath $full -Recurse -Force}
}
New-Item -ItemType Directory -Path $staging,(Join-Path $staging 'runtime'),(Join-Path $staging 'scripts')|Out-Null
foreach($name in @('README.md','manifest.json','control-policy.json','runtime-manifest.sha256')){Copy-Item -LiteralPath (Join-Path $sourceRoot $name) -Destination (Join-Path $staging $name)}
Get-ChildItem -LiteralPath (Join-Path $sourceRoot 'scripts') -Filter '*.ps1' -File|Copy-Item -Destination (Join-Path $staging 'scripts')
$runtimeFiles|Copy-Item -Destination (Join-Path $staging 'runtime')
$packageLines=Get-ChildItem -LiteralPath $staging -Recurse -File|Sort-Object FullName|ForEach-Object{
    $relative=$_.FullName.Substring($staging.Length+1).Replace('\','/')
    '{0}  {1}'-f(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant(),$relative
}
[IO.File]::WriteAllLines((Join-Path $staging 'package-manifest.sha256'),$packageLines,[Text.UTF8Encoding]::new($false))
Add-Type -AssemblyName System.IO.Compression
$stream=[IO.File]::Open($zipPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try{
    $archive=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$true)
    try{
        foreach($file in Get-ChildItem -LiteralPath $staging -Recurse -File|Sort-Object FullName){
            $relative='Aevrynd-Qwen-Light-Portable/'+$file.FullName.Substring($staging.Length+1).Replace('\','/')
            $entry=$archive.CreateEntry($relative,[IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime=[DateTimeOffset]::new(2026,9,2,0,0,0,[TimeSpan]::Zero)
            $input=[IO.File]::OpenRead($file.FullName);$output=$entry.Open();try{$input.CopyTo($output)}finally{$output.Dispose();$input.Dispose()}
        }
    }finally{$archive.Dispose()}
}finally{$stream.Dispose()}
[ordered]@{zipPath=$zipPath;bytes=(Get-Item -LiteralPath $zipPath).Length;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash.ToLowerInvariant();modelRevision='bc640142c66e1fdd12af0bd68f40445458f3869b';modelSha256='7485fe6f11af29433bc51cab58009521f205840f5b4ae3a32fa7f92e8534fdf5'}|ConvertTo-Json
