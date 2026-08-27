# Versioned demos

每次重大展示更新都會保留獨立版本的執行檔，不會覆蓋舊版。請保留 `dist` 內的 DLL 與 `assets` 資料夾，材質包會由執行檔旁的資源路徑載入。

## Riftwarden 2.5D demo

```text
games/riftwarden/dist/Riftwarden-Demo-v2.1.0-Engine-v2.11.0.exe
```

此版本使用 Engine 2.11 的橢圓步進 LRU 快取、自適應深度排序、早期 2.5D 實體與遮擋物剔除。每個版本化 Demo 使用獨立存檔；啟動時不需要 Qwen，按 `F3` 可查看引擎、圖元快取、場景剔除與記憶體遙測。

```powershell
powershell -ExecutionPolicy Bypass -File .\build-native.ps1
```

## Engine 3D feature demo

```text
games/engine_lab/dist/Aevrynd-Engine-Demo-v2.11.0-Lab-v1.79.0.exe
```

新版 EXE 已內嵌 Ground、Wood 與 Brick 三組展示用 CC0 PBR 材質，並直接套用到主要地形、岩體、建築與世界物件；即使單獨複製 EXE，仍可由內嵌資源建立快取並載入。畫面上方的 `CC0 PBR 3/3` 表示三組材質已成功載入，`LIVE 已貼圖實例/全部實例` 則顯示目前畫面真正使用貼圖的物件數。

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
