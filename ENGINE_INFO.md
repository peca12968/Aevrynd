# Engine information

引擎能力、型別盤點與發布狀態保存在同一 workspace 的 `../Info`。發布前請執行：

```powershell
& .\tools\ci\check-engine-info.ps1 -ProjectRoot $PWD -InfoRoot (Join-Path (Split-Path $PWD -Parent) 'Info') -RequireGeneratedInventory
```

此指標不會被 engine runtime 讀取，也不含固定機器路徑。
