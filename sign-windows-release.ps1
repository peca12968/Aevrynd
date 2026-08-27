param(
    [Parameter(Mandatory = $true)]
    [string[]] $Path,
    [string] $CertificateThumbprint = $env:AEVRYND_SIGNING_THUMBPRINT,
    [string] $TimestampUrl = 'http://timestamp.acs.microsoft.com',
    [switch] $VerifyOnly
)

$ErrorActionPreference = 'Stop'

$signTool = Get-ChildItem -LiteralPath 'C:\Program Files (x86)\Windows Kits\10\bin' -Filter 'signtool.exe' -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $signTool) { throw 'Windows SDK SignTool was not found.' }

$files = foreach ($item in $Path) {
    $resolved = Get-Item -LiteralPath $item -ErrorAction Stop
    if ($resolved.Extension -notin '.exe', '.dll') { throw "Unsupported signing target: $($resolved.FullName)" }
    $resolved
}

if (-not $VerifyOnly) {
    $thumbprint = ($CertificateThumbprint -replace '\s', '').ToUpperInvariant()
    if (-not $thumbprint) { throw 'AEVRYND_SIGNING_THUMBPRINT is not configured.' }

    $certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$thumbprint" -ErrorAction SilentlyContinue
    $machineStore = $false
    if (-not $certificate) {
        $certificate = Get-Item -LiteralPath "Cert:\LocalMachine\My\$thumbprint" -ErrorAction SilentlyContinue
        $machineStore = $null -ne $certificate
    }
    if (-not $certificate) { throw "Signing certificate was not found: $thumbprint" }
    if (-not $certificate.HasPrivateKey) { throw 'The signing certificate does not have an accessible private key.' }
    if ($certificate.PublicKey.Oid.Value -ne '1.2.840.113549.1.1.1') { throw 'Smart App Control release signing requires an RSA certificate.' }
    $codeSigningUsage = $certificate.EnhancedKeyUsageList | Where-Object { $_.ObjectId.Value -eq '1.3.6.1.5.5.7.3.3' }
    if (-not $codeSigningUsage) { throw 'The certificate is not valid for code signing.' }

    $chain = [System.Security.Cryptography.X509Certificates.X509Chain]::new()
    try {
        if (-not $chain.Build($certificate)) {
            $errors = ($chain.ChainStatus | ForEach-Object StatusInformation) -join '; '
            throw "The signing certificate is not trusted: $errors"
        }
    } finally {
        $chain.Dispose()
    }

    foreach ($file in $files) {
        $arguments = @('sign', '/v', '/fd', 'SHA256', '/sha1', $thumbprint, '/tr', $TimestampUrl, '/td', 'SHA256', '/d', 'Aevrynd')
        if ($machineStore) { $arguments += '/sm' }
        $arguments += $file.FullName
        & $signTool.FullName @arguments
        if ($LASTEXITCODE -ne 0) { throw "SignTool failed for $($file.FullName)." }
    }
}

foreach ($file in $files) {
    & $signTool.FullName verify /pa /v $file.FullName
    if ($LASTEXITCODE -ne 0) { throw "Authenticode verification failed for $($file.FullName)." }
    $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
    if ($signature.Status -ne 'Valid') { throw "Invalid Authenticode status for $($file.FullName): $($signature.Status)" }
    Write-Host "Verified signed release: $($file.FullName)" -ForegroundColor Green
}
