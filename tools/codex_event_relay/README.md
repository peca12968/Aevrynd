# Aevrynd Codex Event Relay

這是 Windows 副機上的事件式 client。它以 `.NET ClientWebSocket` 主動連到 AevryndWeb 的 `/api/codex-relay/connect`，idle 時只阻塞等待 WebSocket frame：不輪詢、不啟動 Codex/Qwen、不執行 Git，也不產生週期留言。斷線只做 1、2、4…最多 60 秒的 exponential backoff 重連。

不需要 OpenAI API key。relay 使用已登入 Codex Desktop 隨附、OpenAI 有效簽章的 `codex.exe`；只有合法 command 事件到達並成功回覆 `running` ACK 後，才會執行 Codex 並消耗額度：

```text
codex.exe exec --ephemeral -m gpt-5.6-luna -c model_reasoning_effort="medium" -s workspace-write --approve-for-me -C C:\Aevrynd\Games
```

## 握手契約

endpoint 必須是 `wss://<host>/api/codex-relay/connect`（測試只允許 loopback `ws`）。nodeId 只能是 lowercase `taichung` 或 `taoyuan`。upgrade request 帶：

- `X-Aevrynd-Node`
- `X-Aevrynd-Timestamp`：Unix seconds invariant
- `X-Aevrynd-Nonce`：32 bytes CSPRNG 的 base64url
- `X-Aevrynd-Signature`：`sha256=<64 lowercase hex>`

HMAC-SHA256 的 canonical UTF-8 bytes（最後一行也含 LF）：

```text
GET\n/api/codex-relay/connect\n{nodeId}\n{unixSeconds}\n{nonce}\n
```

封印測試向量：secret base64 `AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=`、node `taichung`、timestamp `1788537600`、nonce `AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA`，signature hex 必須是 `293899972d86bc7520b25138db524744bed63373e3bd3ab4b18ffc0fa0cc82ed`。

server command 與 client ACK 均為嚴格 version 1 JSON；多欄、少欄、錯誤 node、非 UTC timestamp、越界或 `.git` ownedFiles 一律在任何模型副作用前拒絕。claim 與 ACK outbox 會先原子寫入本機 state，因此 duplicate 不重跑，terminal ACK 在重連後補送；durable claim ledger 不淘汰舊 commandId，達安全上限時 fail closed 而不是忘記舊 claim。已 durable 標成 executionStarted 卻未留下結果的中斷工作會回報 failed/92，絕不猜測並重跑。Git HEAD、metadata 與 ownedFiles 外的變更沿用 `tools/codex_command_relay/CodexCommandRelay.psm1` 的同一份唯讀 boundary 實作。

## 安裝

先建立只允許目前使用者、SYSTEM、Administrators 寫入的 secret file；檔案內容必須是解碼後 32–64 bytes 的 base64。以實際網站 host 取代範例：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Aevrynd\Games\tools\codex_event_relay\Install-CodexEventRelay.ps1 -Node taichung -Endpoint wss://example.invalid/api/codex-relay/connect -SecretPath C:\ProgramData\Aevrynd\Codex\event-relay.secret
```

installer 解析並封印 Windows PowerShell、OpenAI-signed Desktop `codex.exe` 與共享 boundary module 的絕對路徑，建立目前使用者的 AtLogOn 常駐 Scheduled Task；不依賴 `PATH`。絕對路徑與完整 action arguments 會寫入 ACL 保護的 install manifest，Desktop 更新後仍可精確辨識原 task。移除時必須傳入完全相同的 node、endpoint、secret 與路徑參數，uninstaller 只會移除 action、principal、description、manifest 全部符合 ownership fingerprint 的 task 與三個 installed payload，不刪 state、audit、secret 或工作區資料。

安裝器會先完整比對同節點舊 `Aevrynd-CodexCommandRelay-*` 的 action、principal、description 與全部 arguments；只有完全吻合舊 relay 預設 ownership fingerprint 時，才先停止並 disable，待新的 hidden AtLogOn task 註冊成功後移除。若需要在安裝前立刻停止舊視窗，可先執行（依副機替換節點）：

```powershell
Stop-ScheduledTask -TaskPath \Aevrynd\ -TaskName Aevrynd-CodexCommandRelay-TAICHUNG
Disable-ScheduledTask -TaskPath \Aevrynd\ -TaskName Aevrynd-CodexCommandRelay-TAICHUNG
```

新 task 的 PowerShell action 固定帶 `-WindowStyle Hidden`，trigger 只有 `AtLogOn`，沒有 `Repetition` 或每分鐘 trigger。

安裝採 readiness transaction：installer 以 CSPRNG 建立一次性 install nonce，原子寫入固定的 `StateRoot\readiness-request.json`；nonce 不進入 task arguments，因此不會改變 ownership fingerprint。relay 只有在 module、ACL、single-instance lock、base64 secret 與 durable state 全數初始化成功後，才原子寫入固定的 `StateRoot\readiness.json`。installer 會嚴格比對 node、nonce、UTC 時間、live process id，並要求 Scheduled Task 持續 `Running` 1.5 秒，之後才移除舊 polling task並清除本次 request/receipt。驗證、清除或 legacy cutover 任一步失敗時，會復原既有 event/legacy task XML 與先前 running 狀態，以及 installed payload、manifest、request、receipt 的原始 bytes、存在性和 ACL。若安裝器異常終止而遺留過期 request，日後一般登入會把它視為「沒有 active install transaction」而忽略，不會阻止 relay 常駐。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Aevrynd\Games\tools\codex_event_relay\Uninstall-CodexEventRelay.ps1 -Node taichung -Endpoint wss://example.invalid/api/codex-relay/connect -SecretPath C:\ProgramData\Aevrynd\Codex\event-relay.secret
```

## Fixtures

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\codex_event_relay\tests\CodexEventRelay.Fixture.ps1
```
