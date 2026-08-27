# Versioned demos

每次重大展示更新都會保留獨立版本的執行檔，不會覆蓋舊版。請保留 `dist` 內的 DLL 與 `assets` 資料夾，材質包會由執行檔旁的資源路徑載入。

所有歷代測試程式也會集中發布到 `D:\Download\Aevrynd\Games\Demo`，可直接從該資料夾選擇版本執行。每次建置腳本完成後會自動重新同步執行檔、DLL 與共用資產。

## Riftwarden 2.5D demo

```text
games/riftwarden/dist/Riftwarden-Demo-v2.18.0-Engine-v2.46.0.exe
```

此版本使用 Engine 2.46.0 的級聯感知陰影批次、低記憶體 GPU Mesh LOD 資源鏈、有界 shadow-only caster、相鄰層淡化、解析天空環境光與物理水面 Fresnel。跨級聯物件只上傳一次實例資料，啟動時不需要 Qwen，按 `F3` 可查看引擎、場景剔除與記憶體遙測。

```powershell
powershell -ExecutionPolicy Bypass -File .\build-native.ps1
```

## Engine 3D feature demo

```text
games/engine_lab/dist/Aevrynd-Engine-Demo-v2.46.0-Lab-v2.6.0.exe
```

新版 EXE 已內嵌 Ground、Wood 與 Brick 三組展示用 CC0 PBR 材質，並套用級聯感知陰影批次。近、遠物件只送入需要的 shadow cascade，跨區物件仍共用單一 GPU 實例上傳；近級陰影範圍也已覆蓋完整混合帶與 PCF 邊界。

```powershell
powershell -ExecutionPolicy Bypass -File .\build-engine-lab.ps1
```

## Frontier open-world demo

```text
games/frontier_world/dist/Aevrynd-Frontier-World-Demo-v0.91.0.exe
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
