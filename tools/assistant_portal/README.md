# Aevrynd Assistant Portal

第一版三機 AI 助理網站介面原型。介面採繁體中文，所有資料皆為明確標示的 `Preview` 模擬資料，不會改變主電腦、台中或桃園節點的狀態。

## 本機預覽

需要 Python 3.11 或更新版本：

```powershell
python D:\Download\Aevrynd\Games\tools\assistant_portal\server.py --dev-show-otp
```

開啟 `http://127.0.0.1:8787`。若連接埠已被其他服務使用，可改用：

```powershell
python D:\Download\Aevrynd\Games\tools\assistant_portal\server.py --port 8790 --dev-show-otp
```

伺服器只允許回環位址，不會自行公開到區域網路或網際網路。按 `Ctrl+C` 可停止。

## 驗證

```powershell
python -m unittest discover -s D:\Download\Aevrynd\Games\tools\assistant_portal\tests -v
```

`tests/browser_check.js` 另以 Playwright 驗證桌面／手機水平溢位，以及錯誤 OTP 拒絕與正確 OTP 登入流程。

## 已展示功能

- 主電腦、台中與桃園三個節點狀態卡
- Email OTP 登入主流程（唯一初始 allowlist 為 `slime12968@gmail.com`）
- 自動分流或指定助理的對話介面
- 任務中心與檔案工作區入口
- 遊戲模式控制面板入口
- 工作進度摘要
- 桌面、平板與手機響應式版面
- 鍵盤導覽、清楚焦點與 reduced-motion 支援

## 限制

目前只有本機預覽，尚未串接 Qwen、Codex、即時節點狀態、工作佇列或檔案傳輸。Email OTP 由本機伺服器驗證：8 位英數 CSPRNG、10 分鐘有效、只能使用一次、5 次錯誤後作廢、60 秒重寄冷卻，且只保存 salted hash。Preview adapter 不會寄信；只有明確使用 `--dev-show-otp` 時，才會對唯一核准帳號在本機頁面顯示預覽碼。

驗證層已用 adapter 契約隔離，正式版本可換成 Firebase Auth、自有 SMTP 或 Resend；電話 OTP 與 Email Magic Link 只保留為未啟用的備選。SMTP／Resend 密鑰必須透過伺服器環境變數載入，部署工具也可由 Windows Credential Manager 注入環境，禁止提交到 Git。`--production` 禁止 Preview adapter 與 OTP 顯示，缺少真實寄信 adapter、精確 `--public-host` 或至少一個 `--trusted-proxy-source` 都會拒絕啟動。

伺服器端會隔離登入頁與控制中心；未驗證 session 無法取得 `/portal`、`app.js` 或私有 API。靜態檔案採固定白名單，拒絕原始碼、測試、README、dotfile 與 traversal。Production session cookie 使用 `Secure`、`HttpOnly`、`SameSite=Strict` 與 8 小時 `Max-Age`，並提供 Host／Origin／CSRF 三層檢查及登出失效。production 寄信採 bounded 非同步佇列與 reservation/commit，不讓 SMTP 延遲影響通用 HTTP 回應時間。只有精確設定的 proxy source 能提供單一、嚴格解析的 `Forwarded` 或 `X-Forwarded-For`；其他來源的轉送標頭不受信任。account 與 IP 各自限流，所有 OTP、reservation、rate、CSRF 與 session store 皆具 TTL 與容量上限。

## 固定 ngrok 網域正式部署

正式部署固定使用 `https://sample-cupid-overdrawn.ngrok-free.dev/`，將 ngrok 僅轉送到 `127.0.0.1:8792`。production 強制真實 Gmail SMTP adapter、精確 Host／Origin、Secure Cookie，且完全禁止 `--dev-show-otp`。唯一帳號與寄件帳號都是 `slime12968@gmail.com`。

先以系統管理員 PowerShell 7 安裝封閉且逐檔雜湊的部署檔案；這一步不會停止或切換目前的 LINE 服務：

```powershell
pwsh -NoProfile -File "D:\Download\Aevrynd\Games\tools\assistant_portal\scripts\Install.ps1"
```

安裝後執行啟用器；只有尚未存在時，才會分別以隱藏提示要求 Gmail App Password 與 ngrok API key。既存的 Gmail credential 會安全沿用，不會要求重輸。兩者都只存入目前 Windows 使用者的 Credential Manager，秘密不進入命令列、檔案、manifest 或 log。ngrok API key 只用管理 API 唯讀確認固定網域授權，預檢不建立 tunnel、也不切換公開網址。

```powershell
pwsh -NoProfile -File "C:\ProgramData\Aevrynd\AssistantPortal\scripts\Activate.ps1"
```

切換任一步驟失敗會依 durable baseline 自動回復原 LINE 與本機 Preview 狀態；程序崩潰後重跑也會先完成舊交易 rollback，不會以 state file 是否存在代替程序、listener 與 local API truth。日後需要明確放棄 Portal 並回復舊 LINE gateway 時執行：

```powershell
pwsh -NoProfile -File "C:\ProgramData\Aevrynd\AssistantPortal\scripts\Restore-LineGateway.ps1"
```

Credential Manager 屬於使用者範圍，因此 SMTP 預檢及 production Python/ngrok 固定由登入使用者的 `Limited` 工作執行。獨立的 `Highest` broker 只讀 durable journal 並停止或恢復原本可能已提升權限的 LINE 服務；它不執行 AppData Python 或載入其模組。登入後若發現未完成交易，broker 會先復原 LINE，再讓延遲啟動的 Portal 工作決定是否執行。明確 Restore 成功後會停用 Portal 自動啟動。部署腳本不提供公開 Preview OTP 或假登入模式。

未登入可讀取 `/api/health/email`，但只會看到 `ready/status`；完整安全代碼與 bounded 計數只供登入後的私有狀態 API。診斷狀態可跨程序重啟保存，且永不包含信箱、OTP、密碼或例外文字。
