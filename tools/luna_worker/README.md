# Aevrynd GPT-5.6 Luna Worker

此工具可在台中與桃園副機各自執行一個 GPT-5.6 Luna 工作節點。它只接受文字工作，不執行模型輸出的命令，也不會把 API 金鑰寫入檔案。兩台副機應使用各自的 OpenAI API 金鑰與相同的模型名稱。

## 副機建置

在每台副機以 PowerShell 7 執行（把此資料夾複製到副機後操作）：

```powershell
pwsh -NoProfile -File .\scripts\Install-LunaWorker.ps1 -NodeId taichung
$env:AEVRYND_OPENAI_API_KEY = '在本次程序環境暫時設定的金鑰'
pwsh -NoProfile -File .\scripts\Start-LunaWorker.ps1 -NodeId taichung
```

桃園副機把 `taichung` 改成 `taoyuan`。不要把金鑰寫入 `appsettings`、腳本、Git 或工作記錄；建議由副機的 Credential Manager／服務管理器在啟動時注入環境變數。

另開 PowerShell 驗證：

```powershell
pwsh -NoProfile -File .\scripts\Test-LunaWorker.ps1
```

健康端點只綁定 `127.0.0.1:8792`。提交文字工作：

```powershell
Invoke-RestMethod http://127.0.0.1:8792/work -Method Post -ContentType 'application/json' -Body (@{ prompt = '請只分析目前工作，不要執行命令' } | ConvertTo-Json)
```

工作記錄位於 `C:\ProgramData\Aevrynd\LunaWorker\records`，輸入佇列位於 `...\inbox`。此版本尚未把工作內容或金鑰送往 Aevrynd 網站，也不會自動安裝 Windows 服務或開放防火牆；需要固定常駐時，請由管理員明確建立受控排程。

