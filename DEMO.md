# Versioned demos

每次重大引擎或遊戲版本更新後，建置腳本都會保留一份帶版本號的可執行檔，不會覆蓋先前版本。

## Engine 3D feature demo

```text
games/engine_lab/dist/Aevrynd-Engine-Demo-v2.6.0.exe
```

重新產生最新版與版本化 Demo：

```powershell
powershell -ExecutionPolicy Bypass -File .\build-engine-lab.ps1
```

## Frontier open-world demo

```text
games/frontier_world/dist/Aevrynd-Frontier-World-Demo-v0.68.0.exe
```

重新產生最新版與版本化 Demo：

```powershell
powershell -ExecutionPolicy Bypass -File .\build-frontier-world.ps1
```

Frontier World 視角控制：

- `F1`：第一人稱
- `F2`：第三人稱
- `F3`：上帝視角
- `F4`：自動展示

版本化 `.exe` 會與同一個 `dist` 資料夾內的 DLL 和 `assets` 共用執行資源，因此不要單獨移走執行檔。
