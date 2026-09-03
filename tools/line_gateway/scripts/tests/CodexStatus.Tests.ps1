$ErrorActionPreference = 'Stop'
$script = (Resolve-Path (Join-Path $PSScriptRoot '..\Set-CodexStatus.ps1')).Path
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-codex-status-' + [Guid]::NewGuid().ToString('N'))
$originalProgramData = $env:ProgramData
try {
    [IO.Directory]::CreateDirectory($testRoot) | Out-Null
    $env:ProgramData = $testRoot
    & $script -Status working -Task 'LINE authority' -Phase 'tests' -Progress 80 -Ttl 120
    $directory = Join-Path $testRoot 'Aevrynd\Codex'
    $target = Join-Path $directory 'status.json'
    $document = Get-Content -LiteralPath $target -Raw | ConvertFrom-Json
    if ($document.schemaVersion -ne 1 -or $document.status -ne 'working' -or
        $document.progress -ne 80 -or $document.ttl -ne 120) {
        throw 'Codex status schema mismatch.'
    }
    if ((Get-ChildItem -LiteralPath $directory -Filter '*.tmp').Count -ne 0) {
        throw 'Atomic writer left a temporary file.'
    }
    $firstUpdated = $document.updatedUtc
    Start-Sleep -Milliseconds 5
    & $script -Status completed -Task 'LINE authority ✅' -Phase 'finished' -Progress 100 -Ttl 60
    $replacement = Get-Content -LiteralPath $target -Raw | ConvertFrom-Json
    if ($replacement.status -ne 'completed' -or $replacement.updatedUtc -eq $firstUpdated -or
        (Get-ChildItem -LiteralPath $directory -Filter '*.tmp').Count -ne 0) {
        throw 'Atomic status replacement failed.'
    }
    if ((Get-Item -LiteralPath $target).Length -gt 8192) { throw 'Status writer exceeded the reader byte contract.' }

    foreach ($unsafe in @('C:\private\token.txt', 'client secret rotation', "safe`tphase", "safe$([char]0x202e)exe", '   ')) {
        $rejected = $false
        try {
            & $script -Status working -Task $unsafe -Phase 'tests' -Progress 1
        } catch {
            $rejected = $true
        }
        if (-not $rejected) { throw "Unsafe status text was accepted: $unsafe" }
    }

    $acl = Get-Acl -LiteralPath $directory
    if (-not $acl.AreAccessRulesProtected) { throw 'Codex status directory still inherits ACLs.' }
    $allowed = @(
        [Security.Principal.WindowsIdentity]::GetCurrent().Name,
        'NT AUTHORITY\SYSTEM',
        'BUILTIN\Administrators'
    )
    foreach ($rule in $acl.Access) {
        if ($rule.AccessControlType -eq 'Allow' -and $rule.IdentityReference.Value -notin $allowed) {
            throw "Unexpected writable identity in Codex status ACL: $($rule.IdentityReference.Value)"
        }
    }

    $install = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\Install.ps1') -Raw
    if ($install -notmatch "'Set-CodexStatus\.ps1'") { throw 'Install closed list omits Set-CodexStatus.ps1.' }
    Write-Output 'Codex status writer tests passed (10/10).'
} finally {
    $env:ProgramData = $originalProgramData
    if ([IO.Directory]::Exists($testRoot)) {
        [IO.Directory]::Delete($testRoot, $true)
    }
}
