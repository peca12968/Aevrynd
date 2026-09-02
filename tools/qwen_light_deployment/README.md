# Aevrynd Qwen Light Portable

用途：在主電腦或兩台遠端 Qwen 主機安裝獨立的 Qwen3-4B light service，不變更既有 27B、8080 或 LINE Gateway 8787。

- 模型：`Qwen/Qwen3-4B-GGUF` 的 `Qwen3-4B-Q4_K_M.gguf`
- 固定來源 commit：`bc640142c66e1fdd12af0bd68f40445458f3869b`
- SHA-256：`7485fe6f11af29433bc51cab58009521f205840f5b4ae3a32fa7f92e8534fdf5`
- 大小：2,497,280,256 bytes
- 授權：Apache-2.0
- 主電腦設定：loopback `127.0.0.1:18080`、context 2048、parallel 1、threads 4、GPU layers 0。

依序執行：

```powershell
pwsh -File .\scripts\Install-Light.ps1
pwsh -File .\scripts\Set-LightCredential.ps1
pwsh -File .\scripts\Download-LightModel.ps1
pwsh -File .\scripts\Start-Light.ps1
pwsh -File .\scripts\Test-Light.ps1
```

遠端主機先解壓縮並執行相同步驟，但在完成獨立防火牆與 Tailscale allowlist 前仍只綁 loopback。此包不停止、切換或覆寫任何 27B 程序。

`Switch-LightMode.ps1 -Mode Full` 在 Main 角色固定拒絕；遠端 Full 切換目前只回報尚未配置，不會猜測或停止既有 27B。正式序列化切換控制器留待下一個整合工作包。

`control-policy.json` 與 `Resolve-ControlRequest.ps1` 是尚未接入 LINE 的安全控制契約：只接受固定 node/action/mode，主機 full 永久拒絕，switch 僅輸出序列化轉換計畫且不執行命令。上游仍必須驗證 LINE 使用者 allowlist；任何自然語言或 Qwen 輸出均不得直接傳入系統命令。

## 可重現封裝

Repo 只保存 manifest、腳本與測試，不保存 runtime、模型、憑證、PID/state 或 ZIP。使用已驗證 runtime source 建立 deterministic ZIP：

```powershell
pwsh -File .\Build-PortablePackage.ps1
pwsh -File .\tests\Test-Deployment.ps1
```

Builder 先依 `runtime-manifest.sha256` 驗證封閉檔案集合，再使用固定項目順序與時間戳建立 ZIP；下載腳本仍會對官方模型執行 byte count 與 SHA-256 雙重驗證。
