# Aevrynd Engine — C++ / DirectX 12 Local AI Game Engine

Aevrynd Engine（艾弗林德引擎）是 Aevrynd Studios 自研、可擴充的 Windows 原生遊戲引擎與遊戲集合。核心、遊戲邏輯、輸入與畫面均由 C++20 實作，使用 Win32 視窗與 DirectX 12 呈現，並能透過本機 Qwen 模型生成遊戲事件，不需要瀏覽器或 Node.js。

第一款遊戲是 2.5D 塔防《裂隙守望者》（Riftwarden）。每款遊戲各自封裝於 `遊戲/<遊戲名稱>/`，並共用 `引擎/native/` 的 Aevrynd Engine 能力。

## 啟動遊戲

雙擊 `啟動遊戲.bat`。它會在背景啟動本地 Qwen3.8-27B，接著執行：

```text
遊戲/裂隙守望者/dist/Riftwarden-DX12.exe
```

結束時可關閉遊戲視窗；若也要釋放本地模型，雙擊 `停止遊戲.bat`。

## 操作

- 啟動時預設選中 English；用 `A/D`、左右方向鍵或滑鼠切換語言，再按 Enter／點擊 Confirm
- `L`：遊戲中重新開啟語言選單
- `1`：弧光塔（80）
- `2`：霜脈塔（120）
- `3`：星爆塔（170）
- 滑鼠左鍵：可完成整套教學，並可選塔、在建造台建塔、點現有塔升級
- 畫面按鈕：開始下一波、核心脈衝、時間鎖定、說明、暫停／繼續、切換語言與重新開始
- `N`：準備完成後開始下一波（波次間也可提早出兵）
- `Q`：核心脈衝（80 金幣，全場傷害，18 秒冷卻）
- `W`：時間鎖定（60 金幣，全場緩速，22 秒冷卻）
- `H`：顯示／隱藏簡易教學與操作說明
- `Space`：暫停
- `F2`：切換 DLSS Super Resolution／原生解析度
- `R`：勝敗畫面重新開始
- `Esc`：離開遊戲

### 手把操作（XInput／Xbox 配置）

- 十字鍵左右：切換防禦塔種類
- 十字鍵上下：移動建造台游標
- `A`：確認、建塔或升級
- `X`：開始下一波
- `LB`／`RB`：核心脈衝／時間鎖定
- `Y`：顯示或隱藏說明
- `Start`：暫停
- `Back`／`View`：返回語言選單
- 按下右搖桿：切換 DLSS

連接手把後，HUD 會自動切換成手把提示，目前選中的建造台會以金色顯示。鍵盤、滑鼠與手把可隨時交替使用；遠端開發時即使沒有鍵盤，也能只靠滑鼠完成教學與所有核心遊戲操作。

選擇語言後會依序引導選塔、建塔與開始第一波。繁體中文與 English 會套用到 HUD、技能、波次訊息及教學。敵軍包含一般、迅捷、重甲及每四波登場的首領；將滑鼠移到已建造的塔上可查看射程。

## 存檔

遊戲會自動保存語言設定與進度，包括波次、核心生命、金幣、擊殺數，以及每座防禦塔的位置、種類和等級。存檔位於：

```text
遊戲/裂隙守望者/saves/riftwarden-save.json
```

若在波次進行中離開，下次會保留資源與防禦塔，並從該波開頭重新挑戰，避免載入不完整的戰鬥狀態。

## 原生架構

```text
Games/
├─ CMakeLists.txt
├─ 引擎/
│  ├─ native/                        # 共用 C++ 引擎靜態函式庫
│  │  ├─ include/Aevrynd/
│  │  └─ src/
│  ├─ runtime/                       # llama.cpp 與本地模型
│  ├─ third_party/                   # 第三方 SDK
│  └─ build/                         # 原生建置輸出
└─ 遊戲/
   └─ 裂隙守望者/
      ├─ game.json
      ├─ native/                     # 此遊戲的 C++ 邏輯
      ├─ dist/Riftwarden-DX12.exe    # 可執行遊戲
      └─ legacy-web/                 # 舊版封存，不會被啟動
```

## 引擎能力

- Win32 原生視窗與輸入
- DirectX 12 device、swap chain 與 command queue
- HLSL shader pipeline 與 root signature
- NVIDIA Streamline 2.12 與 DLSS Super Resolution
- DLSS color、depth、motion-vector 與 output resources
- GPU upload vertex buffer 與 fence synchronization
- 60Hz fixed timestep 遊戲迴圈
- CPU draw-list：三角形、矩形、菱形、圓形、線段、像素字型
- 可重用的等角立體柱與橢圓投影原語，用於 2.5D 地形、單位、陰影與射程
- 依桌面工作區等比例縮放視窗，保留 1280×720 邏輯座標與輸入映射
- 2.5D 路徑、深度感地形、塔、敵人、投射物與粒子
- C++ WinHTTP 非同步呼叫本地 llama.cpp

## 編譯

已安裝 Visual Studio 2022 C++ Build Tools、Windows SDK 10.0.26100 與 CMake。

雙擊 `編譯原生遊戲.bat`，或執行：

```powershell
powershell -ExecutionPolicy Bypass -File .\build-native.ps1
```

新增遊戲時，在 `遊戲/<遊戲名稱>/native/` 建立自己的 CMake target，連結共用的 `AevryndEngine`：

```cmake
target_link_libraries(MyGame PRIVATE AevryndEngine)
```

## 本地 AI

遊戲直接以 C++ WinHTTP 呼叫 `127.0.0.1:8081`，不經過 Node 或雲端。Qwen 每三波根據生命、金幣、塔數與擊殺數產生事件；失敗時使用 C++ 內建備援事件。

## DLSS 測試

遊戲啟動時會在支援的 NVIDIA GPU 上嘗試 DLSS Quality。1280×720 輸出時，RTX 5080 上的內部渲染解析度為 853×480；遊戲標題列與右上角 HUD 會顯示目前模式。引擎會驗證實際 GPU 輸出，若 DLSS plugin 連續輸出黑幀便自動切回原生 DirectX 12，避免停留在黑畫面。

- `DLSS QUALITY 853x480`：DLSS 正在執行升頻
- `DLSS OFF 1280x720`：原生解析度 fallback
- `DLSS N/A`：GPU、驅動或 Streamline plugin 不支援

必要的 NVIDIA runtime 已隨遊戲放在 `遊戲/裂隙守望者/dist/`：

```text
sl.interposer.dll
sl.common.dll
sl.dlss.dll
nvngx_dlss.dll
```

目前實作的是 DLSS Super Resolution。DLSS Frame Generation／Multi Frame Generation 仍需加入 Streamline Reflex、HUD-less color 與 presentation hooks，尚未宣稱啟用。

可執行測試：

```text
引擎/build/bin/Release/RiftwardenQwenTest.exe
```
