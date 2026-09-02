# Aevrynd LINE Gateway

此部署層在 Windows 上執行單一 LINE 工作程序，接收 LINE webhook、呼叫內網 Qwen，並透過 `%ProgramData%\Aevrynd\LineGateway\handoff` 與 Codex 交換工作。HTTP server 固定只監聽 `127.0.0.1:8787`，不得直接公開 TCP 8787。公開 transport 可選免費 Tailscale Funnel 或 Cloudflare named tunnel，但只能啟用其中一種。

## 安全邊界

- LINE Channel Secret、Channel Access Token 與 Qwen API key 只允許存入目前執行帳號的 Windows Credential Manager，或由目前程序環境暫時提供。
- 腳本、設定檔、程序參數、PID 狀態與 log 都不得包含密鑰。
- 使用者與群組 allowlist 缺任一項時，gateway 會拒絕啟動。
- 對外 webhook 必須使用已驗證的 Tailscale Funnel 或 Cloudflare named tunnel；兩者互斥，且都只能轉送至 `http://127.0.0.1:8787`。
- Cloudflare named tunnel 設定採封閉六行 schema：只允許一個 `hostname` 對應 loopback backend，再接一個末尾 `http_status:404`。SSH、TCP、其他 hostname、其他 service 與額外 ingress 欄位都會被拒絕。
- 所選 transport 未安裝、設定不完整或驗證失敗時，不建立公開入口；gateway 仍維持 loopback-only。
- 不使用 Quick Tunnel、路由器 port forwarding、UPnP 或直接開放 Windows Firewall 的 8787。
- Tailscale Funnel 只允許由已驗證本機 status 取得的 `desktop-2cp53ov-1.tail972fc3.ts.net`，唯一公開入口是 HTTPS 443 的 `/webhook`，唯一 backend 是 `http://127.0.0.1:8787`。任何額外 host、path、TCP、port、backend 或欄位都會 fail closed。
- `tailscale.exe` 只接受 Windows Program Files 正規路徑、有效 Tailscale Inc. 簽章；SHA-256、版本、node ID、public key、DNS 與 Tailscale IPv4 會綁定於原子 state。啟停結果只相信 `tailscale funnel status --json` 的封閉 schema，不相信 CLI 顯示的網址或 redirect。
- Codex handoff 固定位於 `%ProgramData%\Aevrynd\LineGateway\handoff`；gateway 只寫入請求檔，不會自動執行 `codex.exe` 或任意 shell 指令。

## 前置需求

- Windows 10/11
- PowerShell 7 (`pwsh`)
- Python 3.11 以上
- 若選 Tailscale Funnel：Tailscale 已登入、MagicDNS/Funnel 已在 tailnet 核准，且本機 DNS 必須為 `desktop-2cp53ov-1.tail972fc3.ts.net`
- 若選 Cloudflare：已由管理者建立 named tunnel、專用 hostname 與本機 credentials JSON

## 安裝

首次安裝若尚不知道群組 `groupId`，先執行 `Install.ps1`，選擇並啟動 Tailscale Funnel 或 Cloudflare named tunnel其中一種；此時不要啟動 Gateway。再從 repo 執行一次性 discovery helper。它只在所選 transport 的身分與 authoritative state 驗證通過，且另一種 transport 已證明停用、Gateway 尚未佔用 `127.0.0.1:8787` 時啟動 30–300 秒。每個 request 的 body 最多讀取 3 秒，並且不能超過整個登錄期限；慢速 body 回覆 408 後繼續，拒絕請求累積到 32 筆時會安全終止本次登錄，必須以新 nonce 重開。有效事件必須同時通過原始 body HMAC、預先輸入的 developer user ID、LINE 官方 self mention 與本機隨機 nonce。成功取得一次 userId/groupId 後立即停止，只寫入目前 Windows 使用者 ACL 保護的暫存 JSON，不保存 raw body、reply token、channel secret 或訊息內容。

```powershell
pwsh -NoProfile -File .\tools\line_gateway\scripts\Discover-Allowlist.ps1 `
  -DeveloperUserId 'U<32_HEX_CHARACTERS>' `
  -DurationSeconds 120
```

這不是任意群組自助註冊功能；developer user ID 必須先從 LINE Developers Console 安全取得並精確輸入。把輸出的兩個 ID 人工審查後傳給 `Set-Configuration.ps1`，接著執行 `Set-Credentials.ps1`，最後才啟動 Gateway，再安全刪除暫存。完整人工流程見 `D:\Download\Aevrynd\Info\LineGatewayManualSetup.md`。

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

主要 Qwen 預設為 `http://192.168.50.98:8080/v1`。`http://100.89.42.36:8080/v1` 已加入固定 allowlist 作為第二節點；主要節點只有發生可重試錯誤時，Gateway 才會先以各自憑證檢查 secondary `/slots`，通過後轉移單一請求。單一 worker 不會平行送出多個 secondary 請求，永久授權／輸入錯誤也不會觸發故障轉移。

單節點安裝執行 `Set-Credentials.ps1` 只寫入三個必要憑證；要啟用第二節點時才加上 `-EnableSecondaryQwen`。未加此 switch 不會讀取空白輸入，也不會覆寫既有 secondary credential。primary 與 secondary 角色固定，不能只交換 URL 來改變助理位置映射。

第二節點的 API key 閘門不得以未授權 `GET /v1/models` 必須回傳 401 作判定；llama.cpp 可將 models 保持公開並回傳 200。安全驗收應使用未授權 `POST /v1/chat/completions` 或 `GET /slots`，結果必須為 401 或 403；另外必須在遠端主機獨立確認 Windows Firewall 的 TCP 8080 inbound rule 只允許 Gateway Tailscale IPv4 `100.115.174.48`。API 授權與來源 IP 防火牆是兩個不同 gate，缺少任一項都不可啟用第二節點派工。

## LINE 內的台中與桃園助理

- 群組：使用 LINE 的真正 mention 功能提及官方帳號後直接輸入問題。Gateway 只接受 webhook `mention.mentionees[]` 中 `type=user` 且 `isSelf=true` 的項目，並依官方 UTF-16 `index`／`length` 移除顯示名稱；單靠文字偽造 `@官方帳號` 不會通過。
- 指定助理：mention 後的內容以 `台中 <問題>`、`台中：<問題>`、`桃園 <問題>` 或 `桃園：<問題>` 開頭。只有此開頭 selector 會 pin；例如「比較台中和桃園」仍採自動分配。
- 一對一聊天：直接輸入問題即可自動分配；也可沿用 `@台中 <問題>`、`@桃園 <問題>` 相容前綴。
- 自動分配：以 job ID 的穩定 SHA-256 決定首選，讓同一 job 重試與重啟後不漂移，且大量工作近似平均分配；首選必須通過 authenticated single-slot gate，否則才選另一台。
- `@台中` 預設固定 `.98`，`@桃園` 預設固定 `.36`。非機密設定可交換兩個助理的 primary／secondary role，但兩個實際 endpoint 的 primary／secondary 角色及 allowlist 不可交換。
- `@主機` 保留只讀主機操作；`@Codex` 直接建立耐久 Codex handoff；`@全部` 固定收集主機、primary-first Qwen 與 Codex 三段結果，不等同對兩個城市助理各問一次。

一般問題由指定 Qwen 直接回答。Qwen 若判定為複雜工作、圖片或檔案需求，只能輸出 exact schema 的 JSON 決策；Gateway 不採用模型提供的命令、路徑或任意 handoff request，而是以原始使用者問題、固定 assistant identity 及受限 attachment manifest 建立耐久 Codex handoff，因此 webhook 不會同步等待 Codex。

目前尚未部署安全 artifact server。attachment manifest 只會記錄 `image`／`file` 的 requested 狀態，回覆會明確顯示尚未提供下載，不暴露本機路徑，也不宣稱 LINE 已收到附件。後續圖片須使用公開 HTTPS（TLS 1.2+）的 original／preview URL，原圖限 JPEG／PNG 10 MB、preview 1 MB；一般檔案不假設可直接用 LINE message 傳送，應以安全限時下載連結的文字或 Flex message 回傳。

範例：

```text
[群組 mention 官方帳號] 請整理今天的待辦
[群組 mention 官方帳號] 台中：請分析這段錯誤
[群組 mention 官方帳號] 桃園 請產生場景概念圖需求
@Codex 請進行完整程式碼審查
@全部 回報服務與專案狀態
```

## 啟動與檢查

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Start-Gateway.ps1
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Test-Health.ps1
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Stop-Gateway.ps1
```

`Start-Gateway.ps1` 只啟動一個 worker；重複執行會拒絕建立第二個程序。log 會在啟動前輪替，每檔上限 20 MiB、最多保留 5 份。

安裝程式只接受固定的核心模組與部署腳本白名單，包含 Tailscale Funnel 與 Cloudflare provider；它會建立包含 `install.json` 的封閉 SHA-256 manifest。啟動前會同時檢查內容雜湊與執行目錄完整檔案集合；額外加入的 `.py`、`.pyc` 或腳本都會阻止啟動。`app`、`scripts`、`manifests` 會使用受保護的唯讀 runtime ACL，更新時先在同磁碟 staging 驗證，再切換固定目錄。

## Tailscale Funnel（免費預設）

指令語意以 [Tailscale Funnel 官方 CLI 文件](https://tailscale.com/docs/reference/tailscale-cli/funnel) 為準；本專案再加上更嚴格的固定 host/path/backend 與 state gate。

先只讀驗證本機執行檔、節點身分與目前 Funnel JSON；未啟用時 `Mode` 必須是 `disabled`：

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Test-FunnelConfiguration.ps1
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Start-Funnel.ps1
```

啟用命令只使用固定參數 `funnel --bg --yes --https=443 --set-path=/webhook http://127.0.0.1:8787`。成功必須再由 `funnel status --json` 精確證明唯一 HTTPS 443、唯一 `/webhook` 與唯一 loopback backend；CLI exit code或顯示網址本身不算成功。停止同樣先驗證 state、執行檔與 node 身分，只移除該精確 tuple，禁止使用會清除其他服務的 `funnel reset`：

首次使用若 Tailscale 要求管理員核准 Funnel，啟動腳本只會即時顯示符合 `https://login.tailscale.com/a/...` 固定官方網域格式的授權網址，其他 CLI 輸出不回顯。請開啟該官方網址，並在 Tailscale Access Controls 核准此節點使用 Funnel。授權等待上限為 120 秒；逾時會終止 CLI、重新讀取 authoritative status，並只在狀態仍為 disabled 且 launch nonce 完全相符時清除 pending state。完成網頁核准後可重新執行相同啟動命令。

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Stop-Funnel.ps1
```

LINE webhook URL 固定為 `https://desktop-2cp53ov-1.tail972fc3.ts.net/webhook`。Funnel 是公開網際網路入口，不代表只有 tailnet 裝置能連入；LINE HMAC 與 allowlist 仍不可省略。

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
  -PublicTransport Funnel

# Cloudflare 相容模式
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Register-Startup.ps1 `
  -CloudflaredConfigPath C:\ProgramData\Aevrynd\LineGateway\cloudflared\config.yml
```

Funnel 的 background 設定由 Tailscale service 保存；`-PublicTransport Funnel` 只在 authoritative active status 通過後註冊 Gateway，不建立第二個 mutation task。若兩種 transport 都未指定，只註冊 loopback Gateway；任何 transport 驗證失敗都會在修改公開入口排程前停止。

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

移除程序會先停用專用排程，再分別嘗試停止 gateway、Cloudflare tunnel 與 Tailscale Funnel；其中一項失敗仍會嘗試其他項目。任何停止失敗都會以固定聚合錯誤中止，並保留排程、憑證及檔案供後續安全收斂。

只有明確要求且確認後才刪除全部資料：

```powershell
pwsh -NoProfile -File C:\ProgramData\Aevrynd\LineGateway\scripts\Uninstall.ps1 -RemoveData
```

移除流程先停用專用自啟排程，再安全停止 gateway 與唯一公開 transport。即使安裝 manifest 已損壞，也不會阻止緊急停止；manifest gate 只控制後續檔案刪除。任何程序或 Funnel state 身分模糊都會在刪除檔案前停止流程。

## 部署測試

不修改系統設定的 parser、封閉 manifest 與 hostile tunnel fixture 測試：

```powershell
pwsh -NoProfile -File .\tools\line_gateway\scripts\tests\Deployment.Tests.ps1
pwsh -NoProfile -File .\tools\line_gateway\scripts\tests\Enrollment.Tests.ps1
pwsh -NoProfile -File .\tools\line_gateway\scripts\tests\Funnel.Tests.ps1
```
