[CmdletBinding()]
param(
    [switch]$Live,
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $Live) {
    throw 'Live acceptance is disabled by default. Rerun explicitly with -Live.'
}

# 中文：本測試不呼叫 LINE API、不傳送真實訊息，只送出 LINE 官方驗證所用的空 events webhook。
$root = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
$configPath = Join-Path $root 'config\ngrok-domain.json'
$statePath = Join-Path $root 'state\ngrok-state.json'
$identityPath = Join-Path $root 'scripts\ngrok-package-identity.json'
$transportMutex = $null
$transportLockHeld = $false
$secret = $null
$secretPointer = [IntPtr]::Zero
$secretChars = $null
$secretBytes = $null
$bodyBytes = $null
$executableStream = $null
$http = $null
$config = $null
$state = $null
$identity = $null

function Read-StrictJsonObject {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string[]]$ExpectedKeys,
        [ValidateRange(2,65536)][int]$MaximumBytes = 8192
    )
    $fullPath = [IO.Path]::GetFullPath($Path)
    $item = Get-Item -LiteralPath $fullPath -ErrorAction Stop
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $item.Length -lt 2 -or $item.Length -gt $MaximumBytes) {
        throw "JSON file path or size is invalid: $fullPath"
    }
    $stream = [IO.FileStream]::new($fullPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $bytes = [byte[]]::new([int]$stream.Length)
        try {
            $offset = 0
            while ($offset -lt $bytes.Length) {
                $count = $stream.Read($bytes, $offset, $bytes.Length - $offset)
                if ($count -eq 0) { throw "JSON file was truncated: $fullPath" }
                $offset += $count
            }
            if ($stream.ReadByte() -ne -1) { throw "JSON file changed during read: $fullPath" }
            $options = [Text.Json.JsonDocumentOptions]::new()
            $options.AllowTrailingCommas = $false
            $options.CommentHandling = [Text.Json.JsonCommentHandling]::Disallow
            $options.MaxDepth = 8
            $document = [Text.Json.JsonDocument]::Parse([ReadOnlyMemory[byte]]$bytes, $options)
            try {
                if ($document.RootElement.ValueKind -ne [Text.Json.JsonValueKind]::Object) { throw "JSON root is not an object: $fullPath" }
                $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
                foreach ($property in $document.RootElement.EnumerateObject()) {
                    if (-not $names.Add($property.Name)) { throw "JSON contains a duplicate key: $fullPath" }
                }
                if ($names.Count -ne $ExpectedKeys.Count) { throw "JSON contains an unexpected key count: $fullPath" }
                foreach ($name in $ExpectedKeys) {
                    if (-not $names.Contains($name)) { throw "JSON is missing required key '$name': $fullPath" }
                }
                return $document.RootElement.Clone()
            } finally {
                $document.Dispose()
            }
        } finally {
            if ($null -ne $bytes) { [Array]::Clear($bytes, 0, $bytes.Length) }
        }
    } finally {
        $stream.Dispose()
    }
}

function Get-RequiredString {
    param([Text.Json.JsonElement]$Root, [string]$Name)
    $value = $Root.GetProperty($Name)
    if ($value.ValueKind -ne [Text.Json.JsonValueKind]::String) { throw "$Name must be a JSON string." }
    $text = $value.GetString()
    if ([string]::IsNullOrWhiteSpace($text)) { throw "$Name must not be empty." }
    return $text
}

function Test-ExactNgrokCommandLine {
    param([string]$CommandLine, [string]$ExecutablePath, [string]$Domain)
    $exe = [regex]::Escape([IO.Path]::GetFullPath($ExecutablePath))
    $hostName = [regex]::Escape($Domain)
    return $CommandLine -match ('(?i)^\s*"?' + $exe + '"?\s+http\s+"?http://127\.0\.0\.1:8787"?\s+--domain=' + $hostName + '\s+--scheme=https\s+--inspect=false\s+--log=false\s+--web-addr=127\.0\.0\.1:4040\s*$')
}

function Invoke-WebhookProbe {
    param([string]$Url, [byte[]]$Body, [string]$Signature = '')
    $request = [Net.Http.HttpRequestMessage]::new([Net.Http.HttpMethod]::Post, $Url)
    try {
        $request.Content = [Net.Http.ByteArrayContent]::new($Body)
        $request.Content.Headers.ContentType = [Net.Http.Headers.MediaTypeHeaderValue]::Parse('application/json')
        if (-not [string]::IsNullOrEmpty($Signature)) { [void]$request.Headers.TryAddWithoutValidation('x-line-signature', $Signature) }
        $response = $http.Send($request)
        try { return [int]$response.StatusCode } finally { $response.Dispose() }
    } finally {
        $request.Dispose()
    }
}

try {
    $created = $false
    $transportMutex = [Threading.Mutex]::new($false, 'Global\AevryndLineGatewayPublicTransportControl', [ref]$created)
    try { $transportLockHeld = $transportMutex.WaitOne(30000) } catch [Threading.AbandonedMutexException] { $transportLockHeld = $true }
    if (-not $transportLockHeld) { throw 'Timed out waiting for the public transport control mutex.' }

    foreach ($path in @($configPath, $statePath, $identityPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required installed file is missing: $path" }
    }

    $config = Read-StrictJsonObject $configPath @('schemaVersion','domain') 2048
    $state = Read-StrictJsonObject $statePath @('schemaVersion','pid','startedUtc','domain','executablePath','executableSha256','origin','path') 8192
    $identity = Read-StrictJsonObject $identityPath @('schemaVersion','packageId','packageVersion','packageSource','packageFullName','packagePublisher','packagePublisherId','packageSignatureKind','installedExecutablePath','installedExecutableSha256') 8192
    try {
        $configRoot = $config
        $stateRoot = $state
        $identityRoot = $identity
        if ($configRoot.GetProperty('schemaVersion').ValueKind -ne [Text.Json.JsonValueKind]::Number -or $configRoot.GetProperty('schemaVersion').GetInt32() -ne 1) { throw 'Ngrok domain schema is invalid.' }
        if ($stateRoot.GetProperty('schemaVersion').ValueKind -ne [Text.Json.JsonValueKind]::Number -or $stateRoot.GetProperty('schemaVersion').GetInt32() -ne 1) { throw 'Ngrok state schema is invalid.' }
        if ($identityRoot.GetProperty('schemaVersion').ValueKind -ne [Text.Json.JsonValueKind]::Number -or $identityRoot.GetProperty('schemaVersion').GetInt32() -ne 1) { throw 'Ngrok package identity schema is invalid.' }
        $domain = Get-RequiredString $configRoot 'domain'
        if ($domain -cne $domain.ToLowerInvariant() -or $domain -notmatch '^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.ngrok-free\.dev$' -or $domain.Length -gt 253) { throw 'Installed ngrok domain is invalid.' }
        if ((Get-RequiredString $stateRoot 'domain') -cne $domain -or (Get-RequiredString $stateRoot 'origin') -cne 'http://127.0.0.1:8787' -or (Get-RequiredString $stateRoot 'path') -cne '/webhook') { throw 'Ngrok state transport tuple is invalid.' }
        $pidElement = $stateRoot.GetProperty('pid')
        $pid = 0
        if ($pidElement.ValueKind -ne [Text.Json.JsonValueKind]::Number -or -not $pidElement.TryGetInt32([ref]$pid) -or $pid -lt 1) { throw 'Ngrok state PID is invalid.' }
        $startedUtc = Get-RequiredString $stateRoot 'startedUtc'
        $started = [DateTimeOffset]::MinValue
        if (-not [DateTimeOffset]::TryParseExact($startedUtc, 'o', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind, [ref]$started)) { throw 'Ngrok state timestamp is invalid.' }
        $exe = [IO.Path]::GetFullPath((Get-RequiredString $stateRoot 'executablePath'))
        $expectedHash = Get-RequiredString $stateRoot 'executableSha256'
        if ($expectedHash -notmatch '^[0-9a-f]{64}$') { throw 'Ngrok state executable hash is invalid.' }
        if ((Get-RequiredString $identityRoot 'packageSource') -cne 'MicrosoftStore' -or (Get-RequiredString $identityRoot 'packageSignatureKind') -cne 'Store' -or [IO.Path]::GetFullPath((Get-RequiredString $identityRoot 'installedExecutablePath')) -cne $exe -or (Get-RequiredString $identityRoot 'installedExecutableSha256') -cne $expectedHash) { throw 'Ngrok state and package identity differ.' }
        $package = @(Get-AppxPackage -Name (Get-RequiredString $identityRoot 'packageId') -ErrorAction Stop)
        if ($package.Count -ne 1 -or $package[0].PackageFullName -cne (Get-RequiredString $identityRoot 'packageFullName') -or $package[0].Version.ToString() -cne (Get-RequiredString $identityRoot 'packageVersion') -or $package[0].Publisher -cne (Get-RequiredString $identityRoot 'packagePublisher') -or $package[0].PublisherId -cne (Get-RequiredString $identityRoot 'packagePublisherId') -or $package[0].SignatureKind.ToString() -cne 'Store' -or $package[0].Status.ToString() -cne 'Ok') { throw 'Installed ngrok Store package identity is invalid.' }

        $executableStream = [IO.FileStream]::new($exe, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        $actualHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -cne $expectedHash) { throw 'Live ngrok executable hash is invalid.' }
        $records = @(Get-CimInstance Win32_Process -Filter "Name='ngrok.exe'" -ErrorAction Stop)
        if ($records.Count -ne 1 -or [int]$records[0].ProcessId -ne $pid -or -not (Test-ExactNgrokCommandLine ([string]$records[0].CommandLine) $exe $domain)) { throw 'Exactly one managed ngrok process with the fixed command tuple is required.' }
        if ([string]$records[0].CommandLine -match '(?i)authtoken|token=') { throw 'Ngrok token material appears in the process command line.' }
        $process = Get-Process -Id $pid -ErrorAction Stop
        if ($process.Path -cne $exe -or $process.StartTime.ToUniversalTime().ToString('o') -cne $startedUtc) { throw 'Live ngrok process identity differs from durable state.' }

        if (-not (Test-NetConnection 127.0.0.1 -Port 8787 -InformationLevel Quiet -WarningAction SilentlyContinue)) { throw 'The gateway is not listening on 127.0.0.1:8787.' }
        $apiRaw = Invoke-WebRequest -Uri 'http://127.0.0.1:4040/api/tunnels' -MaximumRedirection 0 -TimeoutSec 3 -SkipHttpErrorCheck
        if ([int]$apiRaw.StatusCode -ne 200) { throw 'Ngrok local API did not return HTTP 200.' }
        $api = $apiRaw.Content | ConvertFrom-Json
        $tunnels = @($api.tunnels)
        if ($tunnels.Count -ne 1 -or $tunnels[0].public_url -cne "https://$domain" -or $tunnels[0].proto -cne 'https' -or $tunnels[0].config.addr -cne 'http://127.0.0.1:8787') { throw 'Ngrok local API does not match the exact durable tuple.' }

        $secret = Read-Host 'Current LINE Channel Secret (input is hidden)' -AsSecureString
        if ($secret.Length -lt 16 -or $secret.Length -gt 512) { throw 'LINE Channel Secret length is outside policy.' }
        $secretPointer = [Runtime.InteropServices.Marshal]::SecureStringToGlobalAllocUnicode($secret)
        $secretChars = [char[]]::new($secret.Length)
        [Runtime.InteropServices.Marshal]::Copy($secretPointer, $secretChars, 0, $secretChars.Length)
        $secretBytes = [Text.Encoding]::UTF8.GetBytes($secretChars)
        $bodyBytes = [Text.UTF8Encoding]::new($false).GetBytes('{"destination":"U00000000000000000000000000000000","events":[]}')
        $hmac = [Security.Cryptography.HMACSHA256]::new($secretBytes)
        try { $signature = [Convert]::ToBase64String($hmac.ComputeHash($bodyBytes)) } finally { $hmac.Dispose() }

        $handler = [Net.Http.HttpClientHandler]::new()
        $handler.AllowAutoRedirect = $false
        $http = [Net.Http.HttpClient]::new($handler, $true)
        $http.Timeout = [TimeSpan]::FromSeconds(10)
        $webhookUrl = "https://$domain/webhook"
        if ((Invoke-WebhookProbe $webhookUrl $bodyBytes $signature) -ne 200) { throw 'Signed empty-event public preflight did not return HTTP 200.' }
        $unsignedStatus = Invoke-WebhookProbe $webhookUrl $bodyBytes
        if ($unsignedStatus -ge 200 -and $unsignedStatus -lt 300) { throw 'Unsigned public webhook was accepted.' }
        $wrongPathStatus = Invoke-WebhookProbe "https://$domain/not-webhook" $bodyBytes $signature
        if ($wrongPathStatus -ge 200 -and $wrongPathStatus -lt 300) { throw 'Wrong public webhook path was accepted.' }
    } finally {}
    Write-Output 'PASS live LINE gateway acceptance: managed ngrok tuple, loopback origin, signed empty preflight, and rejection guards.'
} finally {
    if ($null -ne $http) { $http.Dispose() }
    if ($null -ne $executableStream) { $executableStream.Dispose() }
    if ($null -ne $bodyBytes) { [Array]::Clear($bodyBytes, 0, $bodyBytes.Length) }
    if ($null -ne $secretBytes) { [Array]::Clear($secretBytes, 0, $secretBytes.Length) }
    if ($null -ne $secretChars) { [Array]::Clear($secretChars, 0, $secretChars.Length) }
    if ($secretPointer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeGlobalAllocUnicode($secretPointer) }
    if ($null -ne $secret) { $secret.Dispose() }
    if ($transportLockHeld) { $transportMutex.ReleaseMutex() }
    if ($null -ne $transportMutex) { $transportMutex.Dispose() }
}
