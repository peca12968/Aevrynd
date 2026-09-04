# Codex Command Relay

這是 Windows PowerShell 5.1 的常駐 GitHub Issue 輕量輪詢器。Scheduled Task 只在使用者登入時啟動一次，使用 `-WindowStyle Hidden`，沒有每分鐘 repetition trigger，因此不會反覆跳出視窗。idle 只呼叫已登入的 `gh api`；沒有新合法命令時不啟動 Codex、不寫 Git、不發留言。API 失敗採 exponential backoff（上限 5 分鐘）與 jitter，成功後恢復 20 秒輪詢。

安裝會先把游標 baseline 到 Issue 目前最新留言，舊命令不會被執行。安裝器與 Scheduled Task 固定使用 `%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe`；`gh.exe` 與 `git.exe` 也只從 Program Files 的 canonical 安裝位置解析，驗絕對路徑、reparse、ACL 與簽章/Program Files trust，並把路徑封印進 task arguments 與 ACL-protected `install.json`，runtime 沒有 `PATH` fallback。Codex 仍只解析 Desktop app 內 OpenAI 簽章有效且已登入的 `codex.exe`，不需要 OpenAI API key，只有收到新合法命令才消耗 Codex 額度。

需求：GitHub CLI 已登入可讀寫 `peca12968/Aevrynd`，Codex 桌面版附帶且 OpenAI 簽章有效的 `codex.exe` 已登入，副機工作區位於 `C:\Aevrynd\Games`。正式命令必須由 `peca12968` 且 GitHub `author_association=OWNER` 發布，整份留言必須恰好由一組標記包住：

```text
[MAIN_COMMAND]
TARGET: TAICHUNG
COMMAND_ID: EXAMPLE-001
OWNED_FILES: engine/native/example.cpp, engine/native/example.h
TASK: 在指定檔案內完成明確工作並執行局部驗證。
[/MAIN_COMMAND]
```

`TARGET` 只能是該副機節點或 `ALL`；`COMMAND_ID` 是 publisher nonce，相同值一旦 durable claim 後由 `claimedIds` ledger 永久拒絕重放（容量 4096，滿載時 fail closed，不淘汰舊 claim）；GitHub immutable comment id 則負責游標順序。`OWNED_FILES` 是執行後邊界稽核允許變更的相對路徑/glob，不能包含 `.git` 或上層跳脫。每個 state root 永久綁定一個節點，因此兩個副機必須各自在自己的電腦安裝，不能在同一份 state 上改用另一個 `-Node`。

台中副機（以已登入 Codex 的互動式使用者，在系統管理員 PowerShell 執行）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Aevrynd\Games\tools\codex_command_relay\Install-CodexCommandRelay.ps1 -Node TAICHUNG
```

桃園副機：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Aevrynd\Games\tools\codex_command_relay\Install-CodexCommandRelay.ps1 -Node TAOYUAN
```

installer 是 transaction：先驗證 event relay 的 task + ACL-protected `install.json` ownership，停止並 disable event transport、保留 rollback XML/running state，確認完全停止後才啟動 GitHub daemon，因此兩種 transport 不會重疊執行。新 daemon 在 readiness request 尚存在時 execution gate 維持 closed，只發布 readiness、不讀取或 claim GitHub command；readiness 通過並 unregister 舊 event task 後，installer 刪除 request 原子開閘。故後段交易失敗恢復 event 時，daemon 也不可能已執行同一命令。不需要再次提供 endpoint/secret。readiness request 使用 CSPRNG nonce；daemon 在 ACL、single-instance lock、durable state 都通過後寫入 `C:\ProgramData\Aevrynd\Codex\daemon-readiness.json`：

```json
{"schemaVersion":1,"node":"TAICHUNG","installNonce":"43-char-base64url","processId":1234,"startedAtUtc":"UTC round-trip timestamp"}
```

installer 會 exact 驗 nonce/node/time/live PID，並要求 task 持續 Running 1.5 秒才切換。任一步失敗會復原既有 command/event task XML、running 狀態、installed payload、state 與 readiness 檔。成功後 request 會刪除，receipt 保留為安裝證據。

預覽安裝而不改系統：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\codex_command_relay\Install-CodexCommandRelay.ps1 -Node TAICHUNG -WhatIf
```

解除安裝只會處理 action、使用者、絕對 PowerShell 路徑、arguments 與 description 完整符合 ownership fingerprint 的 task。它先寫入 ACL-protected stop request，讓 daemon 在輪詢間乾淨釋放服務鎖；逾時才要求 Task Scheduler 停止，之後移除兩個已安裝腳本。狀態、receipt 與稽核紀錄保留：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Aevrynd\Games\tools\codex_command_relay\Uninstall-CodexCommandRelay.ps1 -Node TAICHUNG
```

執行中的網站 Active 狀態位於 `C:\ProgramData\Aevrynd\Codex\status.json`，結束後會移除；網站 History 最近最多 100 筆 completed/failed receipt 位於 `operations-history.json`。relay 私有 cursor、永久 `claimedIds`、ACK 與 terminal receipt 狀態存放在 `relay-state.json`，模型輸出與執行前後 Git 邊界稽核位於 `C:\AevryndAudit\codex-relay\<node>`。命令在模型啟動前即原子 claim；RUNNING ACK 失敗時 fail closed（exit 93）且不啟動 Codex，下輪只重試 ACK/FAILED receipt，不重 claim 或重跑。程序留下的 `running` claim 會在下次啟動原子封存為 failed/exit 92、清掉 Active status 並補 history，第二次啟動不重複補寫。終態 GitHub receipt 只含 completed/failed、command ID、exit code、boundary 結果與 output SHA-256，不含模型原始輸出。若 HEAD、Git metadata 或 `OWNED_FILES` 外內容改變，結果會標成 failed 並保留證據，不自動復原使用者資料。
