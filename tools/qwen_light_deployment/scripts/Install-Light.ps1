Set-StrictMode -Version Latest;$ErrorActionPreference='Stop';. (Join-Path $PSScriptRoot 'Common-Light.ps1')
$paths=Get-LightPaths;$package=Split-Path $PSScriptRoot -Parent;Assert-PackageRuntime $package
New-Item -ItemType Directory -Force -Path $paths.Root,$paths.Runtime,$paths.Models,$paths.State|Out-Null
Get-ChildItem -LiteralPath (Join-Path $package 'runtime') -File|Copy-Item -Destination $paths.Runtime -Force
if(-not(Test-Path -LiteralPath $paths.Server -PathType Leaf)){throw 'llama-server installation failed.'}
Write-Output "Installed user-scope runtime at $($paths.Root). Existing 27B services were not modified."
