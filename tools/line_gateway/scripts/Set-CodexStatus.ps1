[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('idle','working','blocked','completed')][string]$Status,
    [Parameter(Mandatory)][ValidateLength(1,160)][string]$Task,
    [Parameter(Mandatory)][ValidateLength(1,120)][string]$Phase,
    [Parameter(Mandatory)][ValidateRange(0,100)][int]$Progress,
    [ValidateRange(1,3600)][int]$Ttl = 300
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$nonPublic = '(?i)([A-Za-z]:[\\/]|\\\\|https?://|(?:\d{1,3}\.){3}\d{1,3}|(?:api|access|refresh|channel|client)[_ -]?(?:key|token|secret)|authorization|bearer|password|private[_ -]?key|webhook|secret)'
$unsafeCategory = {
    param([string]$Value)
    for ($index = 0; $index -lt $Value.Length; $index++) {
        $category = [Globalization.CharUnicodeInfo]::GetUnicodeCategory($Value, $index)
        if ($category -in @('Control','Format','Surrogate')) { return $true }
        if ([char]::IsHighSurrogate($Value[$index]) -and $index + 1 -lt $Value.Length -and
            [char]::IsLowSurrogate($Value[$index + 1])) { $index++ }
    }
    return $false
}
if ((& $unsafeCategory $Task) -or (& $unsafeCategory $Phase) -or
    $Task -match $nonPublic -or $Phase -match $nonPublic) {
    throw 'Status text contains non-public data.'
}
$safeTask = $Task.Trim()
$safePhase = $Phase.Trim()
if ([string]::IsNullOrWhiteSpace($safeTask) -or [string]::IsNullOrWhiteSpace($safePhase)) {
    throw 'Status text cannot be empty.'
}
$programData = [IO.Path]::GetFullPath($env:ProgramData)
$aevryndRoot = Join-Path $programData 'Aevrynd'
$root = Join-Path $env:ProgramData 'Aevrynd\Codex'
$target = Join-Path $root 'status.json'
if (-not [IO.Directory]::Exists($root)) {
    throw 'Codex status publisher is not provisioned; run LineGateway Install.ps1 as administrator.'
}
foreach ($path in @($aevryndRoot,$root)) {
    if (((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'Codex status directory cannot be a reparse point.'
    }
}
if (Test-Path -LiteralPath $target) {
    $existing = Get-Item -LiteralPath $target -Force
    if (-not $existing.PSIsContainer -and (($existing.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)) {
        throw 'Codex status file cannot be a reparse point.'
    }
    if ($existing.PSIsContainer) { throw 'Codex status target is not a file.' }
}
$currentSid = [Security.Principal.WindowsIdentity]::GetCurrent().User
$systemSid = [Security.Principal.SecurityIdentifier]::new(
    [Security.Principal.WellKnownSidType]::LocalSystemSid, $null)
$administratorsSid = [Security.Principal.SecurityIdentifier]::new(
    [Security.Principal.WellKnownSidType]::BuiltinAdministratorsSid, $null)
$allowedOwners = @($currentSid.Value, $systemSid.Value, $administratorsSid.Value)
$ownedItems = @([IO.DirectoryInfo]::new($root))
if ([IO.File]::Exists($target)) { $ownedItems += [IO.FileInfo]::new($target) }
foreach ($item in $ownedItems) {
    $ownerAcl = [IO.FileSystemAclExtensions]::GetAccessControl(
        $item, [Security.AccessControl.AccessControlSections]::Owner)
    $ownerSid = $ownerAcl.GetOwner([Security.Principal.SecurityIdentifier]).Value
    if ($ownerSid -notin $allowedOwners) { throw 'Codex status owner is not trusted.' }
}
$document = [ordered]@{
    schemaVersion = 1
    status = $Status
    task = $safeTask
    phase = $safePhase
    progress = $Progress
    updatedUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ss.fffZ')
    ttl = $Ttl
}
$temporary = Join-Path $root ('.status.' + [Guid]::NewGuid().ToString('N') + '.tmp')
try {
    $json = $document | ConvertTo-Json -Compress
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($json)
    if ($bytes.Length -lt 2 -or $bytes.Length -gt 8192) { throw 'Codex status document size is invalid.' }
    [IO.File]::WriteAllBytes($temporary, $bytes)
    [IO.File]::Move($temporary, $target, $true)
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
}
