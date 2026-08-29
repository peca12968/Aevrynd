param([Parameter(Mandatory=$true)][string]$EngineLab,[Parameter(Mandatory=$true)][string]$FrontierWorld,[Parameter(Mandatory=$true)][string]$Riftwarden,[Parameter(Mandatory=$true)][string]$CharacterLab)
$ErrorActionPreference='Stop'
$optional=@('sl.interposer.dll','sl.common.dll','sl.dlss.dll','nvngx_dlss.dll','NvLowLatencyVk.dll')
foreach($executable in @($EngineLab,$FrontierWorld,$Riftwarden,$CharacterLab)){
  if(-not(Test-Path -LiteralPath $executable -PathType Leaf)){throw "Missing game executable: $executable"}
  $directory=Split-Path -Parent $executable
  foreach($name in $optional){if(Test-Path -LiteralPath (Join-Path $directory $name)){throw "Optional Streamline runtime leaked into OFF output: $name"}}
}
