# Versioned demos

每次重大展示更新都會保留獨立版本的執行檔，不會覆蓋舊版。請保留 `dist` 內的 DLL 與 `assets` 資料夾，材質包會由執行檔旁的資源路徑載入。

## Riftwarden 2.5D demo

```text
games/riftwarden/dist/Riftwarden-Demo-v1.98.0-Engine-v2.8.0.exe
```

此版本使用 Engine 2.8 的靜態 2D 幾何快取與早期 2.5D 視野剔除。啟動時不需要 Qwen；按 `F3` 可查看引擎、場景剔除與記憶體遙測。

```powershell
powershell -ExecutionPolicy Bypass -File .\build-native.ps1
```

## Engine 3D feature demo

```text
games/engine_lab/dist/Aevrynd-Engine-Demo-v2.8.0-Lab-v1.77.0.exe
```

啟動後第一個自動鏡頭會近距離展示 Ground、Wood 與 Brick 三組 CC0 PBR 材質，畫面上方的 `CC0 PBR 3/3` 表示三組材質已成功載入。

```powershell
powershell -ExecutionPolicy Bypass -File .\build-engine-lab.ps1
```

## Frontier open-world demo

```text
games/frontier_world/dist/Aevrynd-Frontier-World-Demo-v0.69.0.exe
```

地表、房屋木板、樹皮與岩石會使用四組 CC0 PBR 材質，畫面上方的 `CC0 PBR 4/4` 表示四組材質已成功載入。

```powershell
powershell -ExecutionPolicy Bypass -File .\build-frontier-world.ps1
```

Frontier World controls:

- `F1`: First-person view
- `F2`: Third-person view
- `F3`: God view
- `F4`: Toggle automatic demonstration
