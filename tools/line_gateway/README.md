# Aevrynd LINE Gateway

此部署層在 Windows 上執行單一 LINE 工作程序，接收 LINE webhook、呼叫內網 Qwen，並透過 `%ProgramData%\Aevrynd\LineGateway\handoff` 與 Codex 交換工作。HTTP server 固定只監聽 `127.0.0.1:8787`，不得直接公開 TCP 8787。

## 安全邊界

- LINE Channel Secret、Channel Access Token 與 Qwen API key 只允許存入目前執行帳號的 Windows Credential Manager，或由目前程序環境暫時提供。
- 腳本、設定檔、程序參數、PID 狀態與 log 都不得包含密鑰。
- 使用者與群組 allowlist 缺任一項時，gateway 會拒絕啟動。
- 對外 webhook 必須使用 Cloudflare named tunnel 的 HTTPS hostname，ingress 只能轉送至 `http://127.0.0.1:8787`，最後一條規則固定回傳 `404`。
- Named tunnel 設定採封閉六行 schema：只允許一個 `hostname` 對應 `http://127.0.0.1:8787`，再接一個末尾 `http_status:404`。SSH、TCP、其他 hostname、其他 service 與額外 ingress 欄位都會被拒絕。
- 未安裝 `cloudflared`、named tunnel 設定不完整或驗證失敗時，不建立 tunnel 自啟工作；gateway 仍維持 loopback-only。
- 不使用 Quick Tunnel、路由器 port forwarding、UPnP 或直接開放 Windows Firewall 的 8787。
- Codex handoff 固定位於 `%ProgramData%\Aevrynd\LineGateway\handoff`；gateway 只寫入請求檔，不會自動執行 `codex.exe` 或任意 shell 指令。

## 前置需求

- Windows 10/11
- PowerShell 7 (`pwsh`)
- Python 3.11 以上
- 若要接收 LINE 公網 webhook：已由管理者建立的 Cloudflare named tunnel、專用 hostname 與本機 credentials JSON

## 安裝

使用系統管理員 PowerShell 7，在 repository 根目錄執行：

```powershell
pwsh -NoProfile -File .\tools\line_gateway\scripts\Install.ps1
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Set-Configuration.ps1 `
  -AllowedUserIds 'Uxxxxxxxx' `
  -AllowedGroupIds 'Cxxxxxxxx' `
  -QwenModel 'Qwen3.8-27B-UD-Q4_K_M.gguf'
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Set-Credentials.ps1
```

`Set-Credentials.ps1` 會以隱密輸入依序要求三個值，不會顯示或寫入檔案。請勿把已在聊天或文件中曝光的舊密鑰再次使用。

主要 Qwen 預設為 `http://192.168.50.98:8080/v1`。`http://100.89.42.36:8080/v1` 只先保留於非機密設定供後續健康派工器使用；目前 Python Qwen client 僅允許主要主機，部署層不會宣稱第二台已投入派工。

第二節點的 API key 閘門不得以未授權 `GET /v1/models` 必須回傳 401 作判定；llama.cpp 可將 models 保持公開並回傳 200。安全驗收應使用未授權 `POST /v1/chat/completions` 或 `GET /slots`，結果必須為 401 或 403；另外必須在遠端主機獨立確認 Windows Firewall 的 TCP 8080 inbound rule 只允許 Gateway Tailscale IPv4 `100.115.174.48`。API 授權與來源 IP 防火牆是兩個不同 gate，缺少任一項都不可啟用第二節點派工。

## 啟動與檢查

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Start-Gateway.ps1
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Test-Health.ps1
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Stop-Gateway.ps1
```

`Start-Gateway.ps1` 只啟動一個 worker；重複執行會拒絕建立第二個程序。log 會在啟動前輪替，每檔上限 20 MiB、最多保留 5 份。

安裝程式只接受固定的 12 個核心模組與部署腳本白名單，會建立包含 `install.json` 的封閉 SHA-256 manifest。啟動前會同時檢查內容雜湊與執行目錄完整檔案集合；額外加入的 `.py`、`.pyc` 或腳本都會阻止啟動。`app`、`scripts`、`manifests` 會使用受保護的唯讀 runtime ACL，更新時先在同磁碟 staging 驗證，再切換固定目錄。

## Cloudflare named tunnel

把 [cloudflared-config.yml.example](cloudflared-config.yml.example) 複製到安全位置後填入 named tunnel UUID、credentials-file 與 hostname。credentials JSON 不得放入 repository。先驗證：

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Test-TunnelConfiguration.ps1 `
  -ConfigPath C:\ProgramData\Aevrynd\LineGateway\cloudflared\config.yml
```

Tunnel 在建立程序前先原子寫入 `pending` 啟動意圖，程序建立後再切換為 `active`；兩階段都包含不可變的 `cloudflared.exe` 路徑、固定 config 路徑、config SHA-256 與隨機 launch nonce。`active` 另記錄 PID 及程序啟動時間，因此啟動腳本即使在 `Start-Process` 後崩潰也能安全收斂。安全停止命令：

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Stop-Tunnel.ps1
```

停止既有程序不要求 config 檔仍存在或內容雜湊仍相同；它使用 state 中的 PID、執行檔、固定 `--config <path> tunnel run` 完整命令與啟動時間防止 PID reuse。若 state 遺失、JSON 損毀或截斷，只枚舉唯一 `cloudflared.exe` 及固定 `%ProgramData%\Aevrynd\LineGateway\cloudflared\config.yml` 的完整命令；多個或模糊候選一律 fail closed，不會停止任何程序。

LINE Developers Console 的 webhook URL 使用 `https://<hostname>/webhook`。不要填 IP、HTTP 或 `:8787`。

## 登入後自動啟動

Credential Manager 是使用者範圍，因此排程使用相同帳號的 `AtLogOn` trigger，而不是 SYSTEM。這可避免把密鑰降級成明文檔案。

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Register-Startup.ps1 `
  -CloudflaredConfigPath C:\ProgramData\Aevrynd\LineGateway\cloudflared\config.yml
```

若未提供 `-CloudflaredConfigPath`，只註冊 loopback gateway；若提供但 cloudflared 或設定驗證失敗，腳本會在修改排程前停止。

## 資料與 Codex handoff

- Queue：`%ProgramData%\Aevrynd\LineGateway\data\queue.sqlite3`
- Codex requests：`%ProgramData%\Aevrynd\LineGateway\handoff\requests`
- Codex responses：`%ProgramData%\Aevrynd\LineGateway\handoff\responses`
- Logs：`%ProgramData%\Aevrynd\LineGateway\logs`

Codex 回覆程式必須沿用 request 的 `jobId` 與 `requestHash`，並以既有 schema 原子寫入 responses。不要直接修改 SQLite。

## 移除

預設移除程式、排程與 Credential Manager 項目，但保留 queue、handoff 與 logs：

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Uninstall.ps1
```

移除程序會先停用專用排程，再分別嘗試停止 gateway 與 tunnel；其中一項失敗仍會嘗試另一項。任何停止失敗都會以固定聚合錯誤中止，並保留排程、憑證及檔案供後續安全收斂。

只有明確要求且確認後才刪除全部資料：

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Uninstall.ps1 -RemoveData
```

移除流程先停用專用自啟排程，再安全停止 gateway 與 tunnel。即使安裝 manifest 已損壞，也不會阻止緊急停止；manifest gate 只控制後續檔案刪除。任何程序身分模糊都會在刪除檔案前停止流程。

## 部署測試

不修改系統設定的 parser、封閉 manifest 與 hostile tunnel fixture 測試：

```powershell
pwsh -NoProfile -File .\tools\line_gateway\scripts\tests\Deployment.Tests.ps1
```
