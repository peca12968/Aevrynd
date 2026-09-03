# Remote Game Mode enrollment

同一份 ZIP 可用於台中與桃園。ZIP 不含模型、runtime、PID 狀態或任何 credential。
只使用檔名含 `v6-reviewed` 的最終包，並先核對交付時提供的 ZIP SHA-256。包內
`package-manifest.sha256` 固定 package manifest，manifest 逐檔列出大小與 SHA-256；任何不符都不得執行。

在主電腦先為每個節點建立 shared controller credential（值需與對應遠端輸入相同）：

```powershell
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Set-NodeCredentials.ps1 -NodeId taichung -ControllerOnly
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Set-NodeCredentials.ps1 -NodeId taoyuan -ControllerOnly
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Register-HubFirewall.ps1 -WhatIf
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Register-HubFirewall.ps1
```

將 ZIP 複製到遠端、解壓縮，先以一般使用者執行唯讀採集。`ModelId` 必須是該機 `/v1/models` 回報的精確 ID：
既有 Qwen 必須綁定該節點的固定 IP（台中 `192.168.50.98:8080`、桃園 `100.89.42.36:8080`）；
`0.0.0.0`、loopback、hostname 或其他 port 都會 fail closed。

```powershell
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Collect-Readiness.ps1 -NodeId taichung -ModelId '<exact-model-id>'
```

核對 readiness 後，在遠端用相同登入帳號互動輸入 shared controller secret 與本機 Qwen API key，再以系統管理員執行安裝：

```powershell
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Set-NodeCredentials.ps1 -NodeId taichung
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Install-RemoteGameMode.ps1 -NodeId taichung -ReadinessDirectory .\Aevrynd-GameMode-Readiness -WhatIf
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Install-RemoteGameMode.ps1 -NodeId taichung -ReadinessDirectory .\Aevrynd-GameMode-Readiness
```

桃園只需把三處 `taichung` 改成 `taoyuan`。採集後若 PID、owner、exe/model hash 或完整 argv 有任何改變，安裝會 fail closed；重新執行 readiness 即可。Firewall 腳本只允許兩個固定遠端來源連到兩個固定主機介面的 TCP 8766；規則已存在但內容不同時會拒絕覆寫。
