# Versioned demos

每次重大展示更新都會保留獨立版本的執行檔，不會覆蓋舊版。請保留 `dist` 內的 DLL 與 `assets` 資料夾，材質包會由執行檔旁的資源路徑載入。

所有歷代測試程式也會集中發布到 `D:\Download\Aevrynd\Games\Demo`，可直接從該資料夾選擇版本執行。每次建置腳本完成後會自動重新同步執行檔、DLL 與共用資產。

## Riftwarden 2.5D demo

```text
games/riftwarden/dist/Riftwarden-Demo-v2.19.0-Engine-v2.47.0.exe
```

此版本使用 Engine 2.47.0 的 Renderer 權威 light-space bounds 裁切、級聯感知陰影批次與低記憶體 GPU Mesh LOD。直接 DrawList 與 Scene 都會使用 resident mesh bounds，自動排除完全離開 shadow volume 的物件；跨級聯物件仍只上傳一次實例資料。

```powershell
powershell -ExecutionPolicy Bypass -File .\build-native.ps1
```

## Engine 3D feature demo

```text
games/engine_lab/dist/Aevrynd-Engine-Demo-v2.47.0-Lab-v2.7.0.exe
```

新版 EXE 已內嵌 Ground、Wood 與 Brick 三組展示用 CC0 PBR 材質，並以實際網格 AABB 在 light clip space 自動裁切陰影。六個 D3D12 clip plane 採含邊界測試並保留 PCF、texel snapping 安全距離；無效或不確定資料會保守回退兩級聯。

```powershell
powershell -ExecutionPolicy Bypass -File .\build-engine-lab.ps1
```

## Frontier open-world demo

```text
games/frontier_world/dist/Aevrynd-Frontier-World-Demo-v0.92.0.exe
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
