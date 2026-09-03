$ErrorActionPreference = 'Stop'
$script = (Resolve-Path (Join-Path $PSScriptRoot '..\Set-CodexStatus.ps1')).Path
$installScript = (Resolve-Path (Join-Path $PSScriptRoot '..\Install.ps1')).Path
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('aevrynd-codex-status-' + [Guid]::NewGuid().ToString('N'))
$originalProgramData = $env:ProgramData
try {
    [IO.Directory]::CreateDirectory($testRoot) | Out-Null
    $env:ProgramData = $testRoot
    $bootstrapRejected = $false
    try {
        & $script -Status working -Task 'LINE authority' -Phase 'tests' -Progress 1
    } catch {
        $bootstrapRejected = $_.Exception.Message -match 'not provisioned.*Install\.ps1'
    }
    if (-not $bootstrapRejected) { throw 'Missing Codex directory did not return the fixed bootstrap error.' }

    $null = [IO.Directory]::CreateDirectory((Join-Path $testRoot 'Aevrynd'))
    $tokens = $null; $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($installScript, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count -ne 0) { throw 'Install script did not parse.' }
    $provision = @($ast.FindAll({
        param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq 'Set-CodexStatusProvision'
    }, $true))
    if ($provision.Count -ne 1) { throw 'Codex ACL provision function is missing or ambiguous.' }
    Invoke-Expression $provision[0].Extent.Text
    $publisherSid = [Security.Principal.WindowsIdentity]::GetCurrent().User
    $directory = Join-Path $testRoot 'Aevrynd\Codex'
    Set-CodexStatusProvision -Root $directory -PublisherSid $publisherSid

    & $script -Status working -Task 'LINE authority' -Phase 'tests' -Progress 80 -Ttl 120
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

    $systemSid = [Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::LocalSystemSid, $null)
    $administratorsSid = [Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::BuiltinAdministratorsSid, $null)
    $allowedSids = @($publisherSid.Value, $systemSid.Value, $administratorsSid.Value)
    foreach ($securedPath in @($directory,$target)) {
        $acl = Get-Acl -LiteralPath $securedPath
        $rules = @($acl.Access)
        foreach ($rule in $rules) {
            $sid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
            if ($rule.AccessControlType -eq 'Allow' -and $sid -notin $allowedSids) {
                throw "Unexpected allowed identity in Codex status ACL: $sid"
            }
        }
        foreach ($broadSid in @('S-1-1-0','S-1-5-11','S-1-5-32-545')) {
            if (@($rules | Where-Object { $_.AccessControlType -eq 'Allow' -and
                $_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value -eq $broadSid }).Count) {
                throw "Broad ordinary-user write identity is present: $broadSid"
            }
        }
        $publisherRules = @($rules | Where-Object {
            $_.AccessControlType -eq 'Allow' -and
            $_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value -eq $publisherSid.Value
        })
        if (-not @($publisherRules | Where-Object {
            ($_.FileSystemRights -band [Security.AccessControl.FileSystemRights]::Modify) -eq
                [Security.AccessControl.FileSystemRights]::Modify
        }).Count) { throw 'Interactive publisher does not have Modify.' }
    }
    if ([string]::IsNullOrWhiteSpace((Get-Content -LiteralPath $target -Raw))) {
        throw 'Gateway-compatible read of Codex status failed.'
    }

    $install = Get-Content -LiteralPath $installScript -Raw
    if ($install -notmatch "'Set-CodexStatus\.ps1'") { throw 'Install closed list omits Set-CodexStatus.ps1.' }
    $writerSource = Get-Content -LiteralPath $script -Raw
    foreach ($forbiddenMutation in @('Set-Acl','SetAccessControl','New-Item')) {
        if ($writerSource -match [regex]::Escape($forbiddenMutation)) {
            throw "Writer still mutates provisioning ACL or parent state: $forbiddenMutation"
        }
    }
    Write-Output 'Codex status provisioning/writer tests passed (16/16).'
} finally {
    $env:ProgramData = $originalProgramData
    if ([IO.Directory]::Exists($testRoot)) {
        [IO.Directory]::Delete($testRoot, $true)
    }
}
