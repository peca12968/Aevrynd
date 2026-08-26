# Aevrynd Engine — C++ / DirectX 12 Local AI Game Engine

Aevrynd Engine（艾弗林德引擎）是 Aevrynd Studios 自研、可擴充的 Windows 原生遊戲引擎與遊戲集合。核心、遊戲邏輯、輸入與畫面均由 C++20 實作，使用 Win32 視窗與 DirectX 12 呈現，並能透過本機 Qwen 模型生成遊戲事件，不需要瀏覽器或 Node.js。

第一款遊戲是 2.5D 塔防《裂隙守望者》（Riftwarden）；`games/engine_lab/` 則是自動輪播不同相機並輸出場景統計的 3D 引擎驗證遊戲。每款遊戲各自封裝於 `games/<game-name>/`，並共用 `engine/native/` 的 Aevrynd Engine 能力。

## 啟動遊戲

雙擊 `start-game.bat`。它會在背景啟動本地 Qwen3.8-27B，接著執行：

```text
games/riftwarden/dist/Riftwarden-DX12.exe
```

結束時可關閉遊戲視窗；若也要釋放本地模型，雙擊 `stop-game.bat`。

## 操作

- 啟動時預設選中 English；用 `A/D`、左右方向鍵或滑鼠切換語言，再按 Enter／點擊 Confirm
- `L`：遊戲中重新開啟語言選單
- `1`：弧光塔（80）
- `2`：霜脈塔（120）
- `3`：星爆塔（170）
- 霜脈克制迅捷並施加冰凍；弧光克制首領，星爆克制重甲；非霜脈攻擊可擊碎冰凍並造成額外傷害
- 滑鼠左鍵：可完成整套教學，並可選塔、在建造台建塔、點現有塔升級
- 畫面按鈕：開始下一波、核心脈衝、時間鎖定、說明、暫停／繼續、切換語言與重新開始
- `N`：準備完成後開始下一波（波次間也可提早出兵）
- 波次間會預報下一波的普通、迅捷、重甲與首領數量，方便先調整塔種與升級
- `Q`：核心脈衝（80 金幣，全場傷害，18 秒冷卻）
- `W`：時間鎖定（60 金幣，全場緩速，22 秒冷卻）
- 3.25 秒內連續擊殺可累積連擊，逐步提高擊殺金幣；敵人撞擊核心會中斷連擊
- 第 4、8 波結束後必須選擇火力、賞金或射程永久強化；可用滑鼠、數字鍵、方向鍵或手把操作
- `H`：顯示／隱藏簡易教學與操作說明
- `Space`：暫停
- `F2`：切換 DLSS Super Resolution／原生解析度
- `F3`：顯示／隱藏引擎渲染診斷，包括頂點、上傳量、峰值、等待與裁切統計
- 自動化環境可在啟動前設定 `AEVRYND_RENDERER_STATS=1`，直接開啟相同診斷面板
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

遊戲會自動保存語言設定與進度，包括波次、核心生命、金幣、擊殺數、永久強化選擇，以及每座防禦塔的位置、種類和等級。存檔位於：

```text
games/riftwarden/saves/riftwarden-save.json
```

若在波次進行中離開，下次會保留資源與防禦塔，並從該波開頭重新挑戰，避免載入不完整的戰鬥狀態。

## 原生架構

```text
Games/
├─ CMakeLists.txt
├─ engine/
│  ├─ native/                        # 共用 C++ 引擎靜態函式庫
│  │  ├─ include/Aevrynd/
│  │  └─ src/
│  ├─ runtime/                       # llama.cpp 與本地模型
│  ├─ third_party/                   # 第三方 SDK
│  └─ build/                         # 原生建置輸出
└─ games/
   ├─ engine_lab/                   # 3D 相機與場景效能驗證遊戲
   └─ riftwarden/
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
- 雙緩衝持久映射頂點上傳與逐幀 fence，CPU 只在重用忙碌資源或驗證 DLSS 輸出時等待
- DrawList 預留 32,768 頂點並跨幀重用容量，240,000 頂點上限會裁切至完整三角形
- 頂點色彩採 GPU 原生 RGBA8 正規化格式，每頂點由 24 bytes 降為 12 bytes；雙緩衝最大上傳配置由 11,520,000 bytes 降為 5,760,000 bytes
- Renderer telemetry 可分別追蹤提交幀、頂點／bytes、峰值、裁切、資源等待與 DLSS 驗證等待
- 通用左手座標相機支援透視與正交投影，並提供第一人稱、第三人稱、上帝視角與等角 2.5D 預設
- Scene3D 可將方塊、三角形與世界空間線段直接提交至硬體 3D 管線，使用 D32 深度緩衝處理遮擋，並保留 CPU 投影／排序路徑供比較驗證
- 2D UI 與 3D 世界使用獨立頂點批次與 pipeline state，同一幀先深度繪製世界、再疊加 HUD
- Mesh3D 提供可重用索引拓撲、位置／旋轉／縮放變換與材質 tint；Scene3D 將多個 mesh 合併為單一 indexed draw
- 60Hz fixed timestep 遊戲迴圈
- CPU draw-list：三角形、矩形、菱形、圓形、線段、像素字型
- 可重用的等角立體柱與橢圓投影原語，用於 2.5D 地形、單位、陰影與射程
- 可重用的穩定深度排序，讓不同類型的 2.5D 場景物件依地面高度正確遮擋
- 可重用的畫布平移，讓世界層能套用鏡頭震動而 HUD 與選單維持穩定
- 依桌面工作區等比例縮放視窗，保留 1280×720 邏輯座標與輸入映射
- 2.5D 路徑、深度感地形、塔、敵人、投射物與粒子
- C++ WinHTTP 非同步呼叫本地 llama.cpp

## 玩法規則核心

- 局內永久強化節點定義於第 4、8 波結束後
- 火力每級增加 15%、賞金每級增加 20%、射程每級增加 12%，單項最多兩級
- 規則、存檔與 2.5D 選擇介面已整合，並以 C++20 編譯期斷言驗證倍率疊加及波次阻擋；新執行檔仍需通過系統簽章政策才能完成實機驗證

## 編譯

已安裝 Visual Studio 2022 C++ Build Tools、Windows SDK 10.0.26100 與 CMake。

雙擊 `build-native-game.bat`，或執行：

```powershell
powershell -ExecutionPolicy Bypass -File .\build-native.ps1
```

建置獨立 3D 驗證遊戲：

```powershell
powershell -ExecutionPolicy Bypass -File .\build-engine-lab.ps1
```

新增遊戲時，在 `games/<game-name>/native/` 建立自己的 CMake target，連結共用的 `AevryndEngine`：

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

必要的 NVIDIA runtime 已隨遊戲放在 `games/riftwarden/dist/`：

```text
sl.interposer.dll
sl.common.dll
sl.dlss.dll
nvngx_dlss.dll
```

目前實作的是 DLSS Super Resolution。DLSS Frame Generation／Multi Frame Generation 仍需加入 Streamline Reflex、HUD-less color 與 presentation hooks，尚未宣稱啟用。

可執行測試：

```text
engine/build/bin/Release/RiftwardenQwenTest.exe
```
