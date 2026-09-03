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
