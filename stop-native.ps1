Get-Process -Name 'Riftwarden-DX12' -ErrorAction SilentlyContinue | Stop-Process -Force
foreach ($port in @(4173,8081)) {
  Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty OwningProcess -Unique |
    ForEach-Object { Stop-Process -Id $_ -Force -ErrorAction SilentlyContinue }
}
Write-Host 'Riftwarden and local AI stopped.'
