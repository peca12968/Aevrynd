param(
    [string[]]$AllowedUserIds,
    [string[]]$AllowedGroupIds,
    [string]$QwenBaseUrl = 'http://192.168.50.98:8080/v1',
    [string]$QwenModel = 'Qwen3.8-27B-UD-Q4_K_M.gguf',
    [string]$SecondaryQwenBaseUrl = 'http://100.89.42.36:8080/v1',
    [string]$InstallRoot = "$env:ProgramData\Aevrynd\LineGateway"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Assert-Administrator
$paths = Get-GatewayPaths -InstallRoot $InstallRoot

function Normalize-IdentifierList {
    param([string[]]$Values, [string]$Label)
    $result = [Collections.Generic.List[string]]::new()
    foreach ($entry in @($Values)) {
        foreach ($value in $entry.Split(',')) {
            $trimmed = $value.Trim()
            if ($trimmed -notmatch '^[A-Za-z0-9_-]{2,128}$' -or $trimmed -match '(?i)replace|example') {
                throw "$Label contains an invalid identifier."
            }
            if (-not $result.Contains($trimmed)) { $result.Add($trimmed) }
        }
    }
    if ($result.Count -lt 1 -or $result.Count -gt 256) { throw "$Label must contain 1 to 256 identifiers." }
    return $result.ToArray()
}

$users = Normalize-IdentifierList -Values $AllowedUserIds -Label 'AllowedUserIds'
$groups = Normalize-IdentifierList -Values $AllowedGroupIds -Label 'AllowedGroupIds'
if ($QwenBaseUrl -ne 'http://192.168.50.98:8080/v1') {
    throw 'The current Qwen client only permits http://192.168.50.98:8080/v1.'
}
if ($SecondaryQwenBaseUrl -ne 'http://100.89.42.36:8080/v1') {
    throw 'The reserved secondary Qwen URL must remain http://100.89.42.36:8080/v1.'
}
if ($QwenModel -notmatch '^[A-Za-z0-9._-]{1,256}$') { throw 'QwenModel is invalid.' }

New-Item -ItemType Directory -Force -Path $paths.Config | Out-Null
$lines = @(
    '# 此檔只包含非機密設定；密鑰一律存入 Windows Credential Manager。',
    "AEVRYND_LINE_ALLOWED_USER_IDS=$($users -join ',')",
    "AEVRYND_LINE_ALLOWED_GROUP_IDS=$($groups -join ',')",
    'AEVRYND_LINE_BIND_HOST=127.0.0.1',
    'AEVRYND_LINE_PORT=8787',
    'AEVRYND_LINE_MAX_BODY_BYTES=262144',
    'AEVRYND_LINE_REQUEST_TIMEOUT_SECONDS=5',
    'AEVRYND_LINE_MAX_EVENTS_PER_WEBHOOK=100',
    "AEVRYND_QWEN_BASE_URL=$QwenBaseUrl",
    "AEVRYND_QWEN_MODEL=$QwenModel",
    "AEVRYND_QWEN_SECONDARY_BASE_URL=$SecondaryQwenBaseUrl"
)
$temporary = "$($paths.SettingsFile).$([Guid]::NewGuid().ToString('N')).tmp"
[IO.File]::WriteAllLines($temporary, $lines, [Text.UTF8Encoding]::new($false))
Move-Item -LiteralPath $temporary -Destination $paths.SettingsFile -Force
Write-Output "Non-secret configuration saved: $($paths.SettingsFile)"
