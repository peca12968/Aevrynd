# Codex Command Relay

這是 Windows 每分鐘啟動一次的短命事件輪詢器。沒有新合法命令時不啟動 AI、不寫 Git、不發留言，且 stdout/stderr 保持 0 bytes。安裝會先把游標 baseline 到 Issue 目前最新留言，舊命令不會被執行。安裝器與 Scheduled Task 固定使用 `%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe`，不會由 `PATH` 尋找可執行檔。

需求：GitHub CLI 已登入可讀寫 `peca12968/Aevrynd`，Codex 桌面版附帶且 OpenAI 簽章有效的 `codex.exe` 已登入，副機工作區位於 `C:\Aevrynd\Games`。正式命令必須由 `peca12968` 且 GitHub `author_association=OWNER` 發布，整份留言必須恰好由一組標記包住：

```text
[MAIN_COMMAND]
TARGET: TAICHUNG
COMMAND_ID: EXAMPLE-001
OWNED_FILES: engine/native/example.cpp, engine/native/example.h
TASK: 在指定檔案內完成明確工作並執行局部驗證。
[/MAIN_COMMAND]
```

`TARGET` 只能是該副機節點或 `ALL`；`OWNED_FILES` 是執行後邊界稽核允許變更的相對路徑/glob，不能包含 `.git` 或上層跳脫。每個 state root 永久綁定一個節點，因此兩個副機必須各自在自己的電腦安裝，不能在同一份 state 上改用另一個 `-Node`。

台中副機（以已登入 Codex 的互動式使用者，在系統管理員 PowerShell 執行）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Aevrynd\Games\tools\codex_command_relay\Install-CodexCommandRelay.ps1 -Node TAICHUNG
```

桃園副機：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Aevrynd\Games\tools\codex_command_relay\Install-CodexCommandRelay.ps1 -Node TAOYUAN
```

預覽安裝而不改系統：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\codex_command_relay\Install-CodexCommandRelay.ps1 -Node TAICHUNG -WhatIf
```

解除安裝只會停止並刪除符合 ownership description 的精確 task，並移除兩個已安裝腳本；狀態與稽核紀錄保留：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Aevrynd\Games\tools\codex_command_relay\Uninstall-CodexCommandRelay.ps1 -Node TAICHUNG
```

執行中的網站 Active 狀態位於 `C:\ProgramData\Aevrynd\Codex\status.json`，結束後會移除；網站 History 最近最多 100 筆事件位於 `operations-history.json`。relay 私有 cursor 與 claim/ACK 狀態只存放在獨立的 `relay-state.json`，模型輸出與執行前後 Git 邊界稽核位於 `C:\AevryndAudit\codex-relay\<node>`。命令在模型啟動前即原子標成 `running` 並前移游標，因此 ACK 失敗或程序崩潰後都不會自動重跑同一命令；若 HEAD、Git metadata 或 `OWNED_FILES` 外的內容改變，結果會標成 failed 並保留證據，不會自動復原使用者資料。
