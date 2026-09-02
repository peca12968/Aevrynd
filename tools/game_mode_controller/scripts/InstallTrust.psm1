Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-NoReparseAncestorChain {
    param([Parameter(Mandatory)][string]$TrustedRoot,[Parameter(Mandatory)][string]$Target)
    $root = [IO.Path]::GetFullPath($TrustedRoot).TrimEnd('\')
    $targetPath = [IO.Path]::GetFullPath($Target).TrimEnd('\')
    if (-not [string]::Equals($root,$targetPath,[StringComparison]::OrdinalIgnoreCase) -and -not $targetPath.StartsWith($root + '\',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Trust target escaped the fixed ancestor root.'
    }
    $current = $root
    $relative = $targetPath.Substring($root.Length).TrimStart('\')
    foreach ($part in @($relative.Split('\',[StringSplitOptions]::RemoveEmptyEntries))) {
        $current = Join-Path $current $part
        if (-not (Test-Path -LiteralPath $current)) { break }
        $item = Get-Item -LiteralPath $current -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Installation ancestor chain contains a reparse point.' }
        if (-not $item.PSIsContainer) { throw 'Installation ancestor chain contains a non-directory.' }
    }
}

function Assert-FixedProgramDataRoot {
    param([Parameter(Mandatory)][string]$ProgramDataRoot)
    $fixed = [IO.Path]::GetFullPath([Environment]::GetFolderPath('CommonApplicationData')).TrimEnd('\')
    $supplied = [IO.Path]::GetFullPath($ProgramDataRoot).TrimEnd('\')
    if (-not [string]::Equals($fixed,$supplied,[StringComparison]::OrdinalIgnoreCase)) { throw 'ProgramData root is not the fixed Windows common application-data directory.' }
    $item = Get-Item -LiteralPath $supplied -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'ProgramData root is not a trusted physical directory.' }
    $owner = (Get-Acl -LiteralPath $supplied).Owner
    $ownerSid = ([Security.Principal.NTAccount]$owner).Translate([Security.Principal.SecurityIdentifier]).Value
    if ($ownerSid -notin @('S-1-5-18','S-1-5-32-544')) { throw 'ProgramData root owner is invalid.' }
    return $supplied
}

function Assert-ClosedDirectoryTrust {
    param([Parameter(Mandatory)][string]$Path,[Security.Principal.SecurityIdentifier]$WritableUserSid)
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Controller trust directory is not physical.' }
    $acl = Get-Acl -LiteralPath $Path
    $ownerSid = ([Security.Principal.NTAccount]$acl.Owner).Translate([Security.Principal.SecurityIdentifier]).Value
    if ($ownerSid -notin @('S-1-5-18','S-1-5-32-544')) { throw 'Controller trust directory owner is invalid.' }
    $trustedWriters = @('S-1-5-18','S-1-5-32-544')
    if ($null -ne $WritableUserSid) { $trustedWriters += $WritableUserSid.Value }
    Assert-NoUntrustedWriter -Acl $acl -TrustedWriterSids $trustedWriters
}

function Assert-NoUntrustedWriter {
    param([Parameter(Mandatory)][Security.AccessControl.DirectorySecurity]$Acl,[Parameter(Mandatory)][string[]]$TrustedWriterSids)
    # 不可用 Write/Modify/FullControl 聚合值；這些值含 ReadAndExecute 的共同讀取位元。
    $writeMask = [Security.AccessControl.FileSystemRights]::WriteData `
        -bor [Security.AccessControl.FileSystemRights]::AppendData `
        -bor [Security.AccessControl.FileSystemRights]::WriteExtendedAttributes `
        -bor [Security.AccessControl.FileSystemRights]::WriteAttributes `
        -bor [Security.AccessControl.FileSystemRights]::DeleteSubdirectoriesAndFiles `
        -bor [Security.AccessControl.FileSystemRights]::Delete `
        -bor [Security.AccessControl.FileSystemRights]::ChangePermissions `
        -bor [Security.AccessControl.FileSystemRights]::TakeOwnership
    foreach ($rule in $acl.Access) {
        $sid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
        if ($rule.AccessControlType -eq [Security.AccessControl.AccessControlType]::Allow -and $sid -notin $TrustedWriterSids -and ($rule.FileSystemRights -band $writeMask)) {
            throw 'Controller trust directory has an untrusted writer.'
        }
    }
}

function Set-ClosedDirectoryTrust {
    param([Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][Security.Principal.SecurityIdentifier]$CurrentUserSid,[switch]$AllowCurrentUserWrite)
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Cannot secure a reparse or non-directory trust path.' }
    $acl = Get-Acl -LiteralPath $Path
    $acl.SetAccessRuleProtection($true,$false)
    $administrators = [Security.Principal.SecurityIdentifier]::new('S-1-5-32-544')
    $acl.SetOwner($administrators)
    foreach ($existingSid in @($acl.Access | ForEach-Object { $_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]) } | Select-Object -Unique)) { $acl.PurgeAccessRules($existingSid) }
    $userRights = if ($AllowCurrentUserWrite) { [Security.AccessControl.FileSystemRights]::Modify } else { [Security.AccessControl.FileSystemRights]::ReadAndExecute }
    foreach ($entry in @(
        @([Security.Principal.SecurityIdentifier]::new('S-1-5-18'),[Security.AccessControl.FileSystemRights]::FullControl),
        @($administrators,[Security.AccessControl.FileSystemRights]::FullControl),
        @($CurrentUserSid,$userRights)
    )) {
        $rule = [Security.AccessControl.FileSystemAccessRule]::new($entry[0],$entry[1],[Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit',[Security.AccessControl.PropagationFlags]::None,[Security.AccessControl.AccessControlType]::Allow)
        $acl.AddAccessRule($rule) | Out-Null
    }
    Set-Acl -LiteralPath $Path -AclObject $acl
    Assert-ClosedDirectoryTrust -Path $Path -WritableUserSid $(if($AllowCurrentUserWrite){$CurrentUserSid}else{$null})
}

function Initialize-ControllerTrustRoot {
    param([Parameter(Mandatory)][string]$ProgramDataRoot,[Parameter(Mandatory)][string]$InstallRoot,[Parameter(Mandatory)][Security.Principal.SecurityIdentifier]$CurrentUserSid)
    $fixed = Assert-FixedProgramDataRoot $ProgramDataRoot
    $aevryndRoot = Join-Path $fixed 'Aevrynd'
    $expectedInstall = Join-Path $aevryndRoot 'GameModeController'
    if (-not [string]::Equals([IO.Path]::GetFullPath($InstallRoot).TrimEnd('\'),[IO.Path]::GetFullPath($expectedInstall).TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase)) { throw 'InstallRoot escaped the fixed ProgramData hierarchy.' }
    Assert-NoReparseAncestorChain -TrustedRoot $fixed -Target $expectedInstall
    if (Test-Path -LiteralPath $aevryndRoot) { Assert-ClosedDirectoryTrust $aevryndRoot }
    else {
        New-Item -ItemType Directory -Path $aevryndRoot | Out-Null
        Assert-NoReparseAncestorChain -TrustedRoot $fixed -Target $aevryndRoot
        Set-ClosedDirectoryTrust -Path $aevryndRoot -CurrentUserSid $CurrentUserSid
    }
    if (Test-Path -LiteralPath $expectedInstall) { Assert-ClosedDirectoryTrust $expectedInstall }
    else {
        New-Item -ItemType Directory -Path $expectedInstall | Out-Null
        Assert-NoReparseAncestorChain -TrustedRoot $fixed -Target $expectedInstall
        Set-ClosedDirectoryTrust -Path $expectedInstall -CurrentUserSid $CurrentUserSid
    }
    return $expectedInstall
}

Export-ModuleMember -Function Assert-NoReparseAncestorChain,Assert-FixedProgramDataRoot,Assert-ClosedDirectoryTrust,Set-ClosedDirectoryTrust,Initialize-ControllerTrustRoot
