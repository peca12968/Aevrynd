param([string]$PythonExe = 'C:\Users\USER\AppData\Local\Programs\Python\Python312\python.exe')

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Portal.Common.ps1')
Assert-PowerShellSeven
Assert-Administrator

$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$python = [IO.Path]::GetFullPath($PythonExe)
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) { throw 'Python executable was not found.' }
$pythonIdentity=Get-VerifiedFileHash $python
try{$pythonItem=Get-Item -LiteralPath $python -Force;$signature=Get-AuthenticodeSignature -LiteralPath $python;if($pythonItem.VersionInfo.FileVersionRaw.Major-lt3-or($pythonItem.VersionInfo.FileVersionRaw.Major-eq3-and$pythonItem.VersionInfo.FileVersionRaw.Minor-lt11)-or$signature.Status-ne'Valid'-or$signature.SignerCertificate.Subject-notmatch'O=Python Software Foundation'){throw 'Signed Python 3.11 or newer is required.'}}finally{$pythonIdentity.Stream.Dispose()}
$ngrokIdentity = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'ngrok-package-identity.json') | ConvertFrom-Json
$package = Get-AppxPackage -Name ([string]$ngrokIdentity.packageId) -ErrorAction Stop
if (@($package).Count -ne 1 -or $package.PackageFullName -cne [string]$ngrokIdentity.packageFullName -or
    $package.Publisher -cne [string]$ngrokIdentity.packagePublisher -or $package.SignatureKind.ToString() -cne 'Store' -or
    $package.Status.ToString() -cne 'Ok') { throw 'Pinned Microsoft Store ngrok package identity is invalid.' }
$ngrok = [IO.Path]::GetFullPath((Join-Path ([string]$package.InstallLocation) 'ngrok.exe'))
$ngrokIdentityFile=Get-VerifiedFileHash $ngrok
try{if ($ngrok -cne [string]$ngrokIdentity.installedExecutablePath -or $ngrokIdentityFile.Hash -cne [string]$ngrokIdentity.installedExecutableSha256) {
    throw 'Pinned ngrok executable identity is invalid.'
}}finally{$ngrokIdentityFile.Stream.Dispose()}

$paths = Get-PortalPaths
foreach ($stateFile in @($paths.PortalState,$paths.NgrokState)) {
    if (Test-Path -LiteralPath $stateFile -PathType Leaf) { throw 'Stop the public portal before installing an update.' }
}
foreach ($directory in @($paths.Root,$paths.App,$paths.Scripts,$paths.State,$paths.Logs,$paths.Privileged)) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    if (((Get-Item -LiteralPath $directory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Installation path cannot contain a reparse point.' }
}
$directoryHandles=[Collections.Generic.List[IDisposable]]::new()
foreach($directory in @($sourceRoot,$PSScriptRoot,$paths.Root,$paths.App,$paths.Scripts,$paths.State,$paths.Logs,$paths.Privileged)){$directoryHandles.Add((Get-VerifiedDirectoryHandle $directory))}
# 程式與部署腳本只允許目前使用者讀取執行；狀態與日誌另給目前使用者修改權。
$currentSid=[Security.Principal.WindowsIdentity]::GetCurrent().User
$adminSid=[Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::BuiltinAdministratorsSid,$null)
$systemSid=[Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::LocalSystemSid,$null)
$rootAcl=[Security.AccessControl.DirectorySecurity]::new();$rootAcl.SetAccessRuleProtection($true,$false)
foreach($sid in @($adminSid,$systemSid)){$rootAcl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))}
$rootAcl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($currentSid,'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow'))
Set-Acl -LiteralPath $paths.Root -AclObject $rootAcl
function Set-CanonicalDirectoryAcl([string]$Path,[bool]$Writable){
    $acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetOwner($adminSid);$acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @($adminSid,$systemSid)){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))}
    $rights=if($Writable){'FullControl'}else{'ReadAndExecute'}
    $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($currentSid,$rights,'ContainerInherit,ObjectInherit','None','Allow'))
    Set-Acl -LiteralPath $Path -AclObject $acl
    Assert-CanonicalPortalAcl $Path $Writable
}
function Set-CanonicalFileAcl([string]$Path,[bool]$Writable){
    $acl=[Security.AccessControl.FileSecurity]::new();$acl.SetOwner($adminSid);$acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @($adminSid,$systemSid)){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','Allow'))}
    $rights=if($Writable){'FullControl'}else{'ReadAndExecute'}
    $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($currentSid,$rights,'Allow'))
    Set-Acl -LiteralPath $Path -AclObject $acl
    Assert-CanonicalPortalAcl $Path $Writable
}
function Assert-CanonicalPortalAcl([string]$Path,[bool]$Writable){
    $acl=Get-Acl -LiteralPath $Path
    $ownerSid=([Security.Principal.NTAccount]$acl.Owner).Translate([Security.Principal.SecurityIdentifier]).Value
    if(-not$acl.AreAccessRulesProtected-or$ownerSid-cne$adminSid.Value){throw 'Installed ACL owner or inheritance is not canonical.'}
    $allowed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);foreach($sid in @($adminSid,$systemSid,$currentSid)){$null=$allowed.Add($sid.Value)}
    $rules=@($acl.GetAccessRules($true,$false,[Security.Principal.SecurityIdentifier]))
    if($rules.Count-ne3-or@($rules|Where-Object{$_.AccessControlType-ne'Allow'-or-not$allowed.Contains($_.IdentityReference.Value)}).Count){throw 'Installed ACL contains a non-canonical principal or rule.'}
    $userRule=@($rules|Where-Object{$_.IdentityReference.Value-ceq$currentSid.Value})
    if($userRule.Count-ne1-or($Writable-and($userRule[0].FileSystemRights-band[Security.AccessControl.FileSystemRights]::FullControl)-ne[Security.AccessControl.FileSystemRights]::FullControl)-or(-not$Writable-and($userRule[0].FileSystemRights-band[Security.AccessControl.FileSystemRights]::WriteData))){throw 'Installed ACL grants incorrect user rights.'}
}
function Set-CanonicalPrivilegedAcl([string]$Path){
    $acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetOwner($adminSid);$acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @($adminSid,$systemSid)){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))}
    Set-Acl -LiteralPath $Path -AclObject $acl
    $readback=Get-Acl -LiteralPath $Path;$owner=([Security.Principal.NTAccount]$readback.Owner).Translate([Security.Principal.SecurityIdentifier]);$actual=@($readback.GetAccessRules($true,$false,[Security.Principal.SecurityIdentifier]))
    if(-not$readback.AreAccessRulesProtected-or$owner.Value-cne$adminSid.Value-or$actual.Count-ne2-or@($actual|Where-Object{$_.IdentityReference.Value-notin@($adminSid.Value,$systemSid.Value)-or$_.AccessControlType-ne[Security.AccessControl.AccessControlType]::Allow-or($_.FileSystemRights-band[Security.AccessControl.FileSystemRights]::FullControl)-ne[Security.AccessControl.FileSystemRights]::FullControl-or$_.InheritanceFlags-ne([Security.AccessControl.InheritanceFlags]::ContainerInherit-bor[Security.AccessControl.InheritanceFlags]::ObjectInherit)-or$_.PropagationFlags-ne[Security.AccessControl.PropagationFlags]::None}).Count){throw 'Privileged journal ACL owner, protection, identities, rights, or inheritance is not canonical.'}
}
foreach($directory in @($paths.Root,$paths.App,$paths.Scripts)){Set-CanonicalDirectoryAcl $directory $false}
foreach($directory in @($paths.State,$paths.Logs)){Set-CanonicalDirectoryAcl $directory $true}
Set-CanonicalPrivilegedAcl $paths.Privileged

$appFiles = @('server.py','index.html','styles.css','app.js','auth_adapter.js','auth_login.js')
$scriptFiles = @('Portal.Common.ps1','Install.ps1','Preflight.ps1','Set-GmailCredential.ps1','Set-NgrokApiCredential.ps1','Start-Portal.ps1','Stop-Portal.ps1','Start-Ngrok.ps1','Stop-Ngrok.ps1','Switch-ToPortal.ps1','Restore-LineGateway.ps1','Register-Startup.ps1','Activate.ps1','Limited-Task.ps1','Admin-Broker.ps1','ngrok-package-identity.json')
$records = [Collections.Generic.List[object]]::new()
function Install-VerifiedFile([string]$Source,[string]$Destination){
    $sourceFull=[IO.Path]::GetFullPath($Source);$destinationFull=[IO.Path]::GetFullPath($Destination)
    if(-not$sourceFull.StartsWith($sourceRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Install source escaped the portal source root.'}
    $sourceIdentity=Get-VerifiedFileHash $sourceFull
    try{$sourceIdentity.Stream.Position=0;$destinationStream=[IO.FileStream]::new($destinationFull,'Create','Write','None',65536,[IO.FileOptions]::WriteThrough);try{$sourceIdentity.Stream.CopyTo($destinationStream);$destinationStream.Flush($true)}finally{$destinationStream.Dispose()}}finally{$sourceIdentity.Stream.Dispose()}
    Set-CanonicalFileAcl $destinationFull $false
    $destinationIdentity=Get-VerifiedFileHash $destinationFull
    try{if($destinationIdentity.Hash-cne$sourceIdentity.Hash-or$destinationIdentity.Length-ne$sourceIdentity.Length){throw 'Installed file differs from the handle-pinned source.'};return [ordered]@{path=[IO.Path]::GetRelativePath($paths.Root,$destinationFull);size=$destinationIdentity.Length;sha256=$destinationIdentity.Hash}}finally{$destinationIdentity.Stream.Dispose()}
}
foreach ($name in $appFiles) {
    $source = Join-Path $sourceRoot $name; $destination = Join-Path $paths.App $name
    $records.Add((Install-VerifiedFile $source $destination))
}
foreach ($name in $scriptFiles) {
    $source = Join-Path $PSScriptRoot $name; $destination = Join-Path $paths.Scripts $name
    $records.Add((Install-VerifiedFile $source $destination))
}
Write-PortalJson -Path $paths.Manifest -Value ([ordered]@{schemaVersion=1;files=$records})
$pythonIdentity=Get-VerifiedFileHash $python;$ngrokIdentityFile=Get-VerifiedFileHash $ngrok;$previewIdentity=Get-VerifiedFileHash (Join-Path $sourceRoot 'server.py')
try{Write-PortalJson -Path $paths.Install -Value ([ordered]@{schemaVersion=1;ownerSid=$currentSid.Value;pythonExe=$python;pythonSha256=$pythonIdentity.Hash;ngrokExe=$ngrok;ngrokSha256=$ngrokIdentityFile.Hash;domain=$script:PortalDomain;port=$script:PortalPort;previewServerPath=(Join-Path $sourceRoot 'server.py');previewServerSha256=$previewIdentity.Hash})}finally{$pythonIdentity.Stream.Dispose();$ngrokIdentityFile.Stream.Dispose();$previewIdentity.Stream.Dispose()}
foreach($metadata in @($paths.Manifest,$paths.Install)){Set-CanonicalFileAcl $metadata $false}
foreach($handle in $directoryHandles){$handle.Dispose()}
Write-Output 'Assistant Portal installed as a closed, hashed file set. No public service was switched.'
Write-Output "Next: run the protected Activate.ps1; existing credentials are reused and only missing secrets are prompted."
