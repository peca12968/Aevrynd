# Aevrynd Engine — C++ / DirectX 12 Local AI Game Engine

Aevrynd Engine（艾弗林德引擎）是 Aevrynd Studios 自研、可擴充的 Windows 原生遊戲引擎與遊戲集合。核心、遊戲邏輯、輸入與畫面均由 C++20 實作，使用 Win32 視窗與 DirectX 12 呈現，不需要瀏覽器或 Node.js。

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
- `F2`：在以 `--dlss` 啟動時切換 DLSS Super Resolution／原生解析度
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
- 按下右搖桿：在以 `--dlss` 啟動時切換 DLSS

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
- 建置期編譯並嵌入 bytecode 的 HLSL shader pipeline 與 root signature，正式執行檔不再依賴 `D3DCOMPILER_47.dll`
- 可在建立 D3D 裝置前按需驗證簽章並動態載入的 NVIDIA Streamline 2.12 與 DLSS Super Resolution 模組；未選用時不載入 `sl.interposer.dll`
- DLSS color、depth、motion-vector 與 output resources
- 可成長 GPU upload vertex／index／instance buffer 與 fence synchronization
- 三重緩衝持久映射上傳資源由 4,116,480 bytes 起始，依各幀需求倍增至既有硬上限；背景記憶體修剪會在 GPU 安全點釋放尖峰容量
- Mesh 與材質最後一個引用釋放時會立即退出常駐資源表，並透過 fence 延後回收仍可能被既有幀使用的 GPU 資源與材質描述符，避免世界串流卸載時同步等待整條 GPU queue
- 240 幀固定負載基準保持 0 次容量成長與 0 次資源等待，GPU 平均約 11 µs
- DrawList 預留 32,768 頂點並跨幀重用容量，240,000 頂點上限會裁切至完整三角形
- 頂點色彩採 GPU 原生 RGBA8 正規化格式，每個 2D 頂點由 24 bytes 降為 12 bytes；只有超過 32,768 個 2D 頂點的幀槽才會擴張上傳資源
- Renderer telemetry 可分別追蹤提交幀、頂點／bytes、峰值、裁切、資源等待、幀上傳常駐容量／成長／修剪與 DLSS 模組狀態
- Fence telemetry 量測資源重用與 DLSS 驗證等待的最近、累積、峰值奈秒及平均微秒，不再只記錄等待次數
- 通用左手座標相機支援透視與正交投影，並提供第一人稱、第三人稱、上帝視角與等角 2.5D 預設
- Scene3D 可將方塊、三角形與世界空間線段直接提交至硬體 3D 管線，使用 D32 深度緩衝處理遮擋，並保留 CPU 投影／排序路徑供比較驗證
- Scene3D 以保守球體同時支援透視與正交視錐裁切；6,561 mesh 壓力基準只提交 593 個可見物件，上傳量減少 90.9%，CPU 完整展開平均 3,076 µs，實例化提交平均 304 µs
- 2D UI 與 3D 世界使用獨立頂點批次與 pipeline state，同一幀先深度繪製世界、再疊加 HUD
- Mesh3D 提供可重用索引拓撲、位置／旋轉／縮放變換與材質 tint；Scene3D 將多個 mesh 合併為單一 indexed draw
- MeshHandle3D 將任意合法索引網格一次上傳至 GPU DEFAULT heap，並以三緩衝 matrix／tint instance stream 合併相同 mesh 的 `DrawIndexedInstanced`；583 個可見 mesh 的逐幀上傳由 307,824 bytes 降至 39,644 bytes，再減少 87.1%
- GPU 實例化同時保留 CPU 展開 fallback 與資源建立／銷毀統計；加入保守 transform bounds 後，同一壓力場景的 Scene3D 提交平均由 348 µs 降至約 240 µs
- MeshResource3D 封裝 handle、自動 AABB 球體 bounds、拓撲統計與共用 CPU fallback；Scene3D 可混合任意網格、transform 與 material，先逐物件裁切再依 mesh handle 分批實例化
- Bounds 轉換支援偏心網格、旋轉與非等比縮放；中心位於原點的常見資產走無矩陣快速路徑，6,561 物件基準維持約 240 µs GPU 實例提交
- Engine Lab 同幀混合常駐 box、pyramid 與外部 OBJ 三種網格；OBJ 載入器支援多邊形三角化、負索引、頂點色、平滑／平面法線生成與右手轉左手座標，GPU／CPU 場景驗證僅一個瞬時像素超過 1/255
- Scene3D 支援透視／正交畫面射線、世界 AABB 粗篩與最近三角形命中；6,561 物件、240 次查詢平均 77 微秒，99.9% 候選在精確測試前排除，並回傳位置、法線、重心座標與物件索引
- AudioEngine 使用 Windows XAudio2 提供 PCM WAV、程序音色、音量／音高／循環播放、64 voice 資源上限與完整遙測；Engine Lab 驗證 4 voice 同時播放後自動回收，Riftwarden 的出兵、建造、技能、受創與升級事件共用同一管線
- generation-safe ECS 提供 1,600 萬 entity 明確上限、可回收 index、永久退役的 generation overflow slot、bounded sparse component storage、typed view、延遲銷毀、殭屍 component 過濾與記憶體修剪
- 版本化序列化核心使用受限 UTF-8 JSON 封裝，精確保存 64-bit 整數，支援 Vec2、Vec3、Transform3D 與巢狀物件；未知欄位、預設值、原子失敗、文件／字串／欄位／深度上限均有自動測試
- PhysicsWorld3D 獨立回歸涵蓋 static／dynamic／kinematic、layer mask、sleep／wake、高速 CCD、stale handle 與 4,096 bodies 工作區回收
- 寫實 3D 資產清單強制 CC0-1.0 商用來源、SHA-256、完整 PBR 通道、Far／General／Near／Hero 貼圖層級、Mesh LOD、切線、碰撞、Atlas／Trim Sheet 與幾何最佳化資訊；失敗不會污染既有輸出
- Scene/ECS 3D 橋接可批次同步 Transform、雙材質 Mesh LOD 與局部 PointLight；1,000 entity 自動測試涵蓋 visibility layer、完整燈光 transform、stale handle、slot 退役與記憶體修剪
- 有界 CPU profiler 提供跨執行緒 scope、counter、gauge、固定 frame ring、JSON／CSV 匯出與記憶體修剪；非法數值、跨幀 scope 與容量溢位會被拒絕並留下 dropped telemetry
- World shader 採用 Cook-Torrance GGX、Smith 幾何遮蔽、Schlick Fresnel、能量守恆漫反射與 ACES tone mapping，並以精確 sRGB 輸出轉換及 CPU 黃金參考測試避免 gamma 錯誤
- MeshProcessing3D 提供退化面移除、完整頂點屬性去重、平滑／硬邊法線重建、保護 UV 與法線接縫的保守 LOD，以及具實測 cache miss 改善的索引重排
- Engine Lab 的靜態開放世界景觀包含地形、蜿蜒河流、森林、岩石、聚落、遠山與地標塔，分離靜態／動態 Scene3D 提交並保留跨場景完整統計
- 3D 材質使用明確的水體分類傳入 GPU，不再借用透明度猜測表面類型；連續河道網格可穩定套用水波法線與 Fresnel 反射
- 程序環境網格提供草叢、多葉簇樹冠、不規則山體與天空穹頂；山體近遠 LOD 取代規則金字塔剪影，並保留可大量實例化的頂點預算
- Scene3D 靜態射線查詢採用有界記憶體 BVH，維持最近命中與完整材質／LOD 語意，同時避免大型世界逐物件掃描
- Water 材質使用獨立透明 3D pass，在不寫入深度的情況下保留深度測試並依鏡頭距離由後向前繪製，避免不透明水面遮蔽岸邊與水下幾何
- 網格級三角形 BVH 可跨 MeshResource3D 實例共用，將大型地形命中由完整三角形掃描縮小為少量候選，同時保留 transform、backface 與最近命中語意
- ArchitectureMeshes3D 可依尺寸、seed 與 Near／Far 細節級別建立含屋簷、門窗、木樑及煙囪的程序建築；目前 cottage LOD 由 308 面降至 32 面
- FrameCapture3D 直接由 DX12 back buffer 執行有界 R8G8B8A8 readback 與原子 PNG 輸出；Engine Lab 的 `--capture-demo` 會自動輸出第一人稱、第三人稱、上帝視角及 2.5D 四種實際渲染畫面
- 方向光使用固定 2048² D32 shadow map、獨立 depth-only world／instanced pass、3×3 comparison PCF 與斜率 bias；材質可明確停用投影，避免天空穹頂、山體與地形污染陰影深度
- Shadow pass 採三重緩衝、每幀 256-byte 對齊常數緩衝，並回報固定 16 MiB、實際 caster 與 draw call；真陰影啟用時不再建立舊式平面假陰影
- FrameCapture3D 以最多 262,144 個像素樣本分析平均亮度、變異、全黑與全白比例；不合理曝光不會被計為有效畫面驗證
- Broadleaf 近景 LOD 使用可重現的分枝、細枝與雙面不規則葉片輪廓；程序 cottage 近景 LOD 增加突出石基、雙側木框窗與五列屋瓦層次
- 60Hz fixed timestep 遊戲迴圈
- CPU draw-list：三角形、矩形、菱形、圓形、線段、像素字型
- 可重用的等角立體柱與橢圓投影原語，用於 2.5D 地形、單位、陰影與射程
- 可重用的穩定深度排序，讓不同類型的 2.5D 場景物件依地面高度正確遮擋
- 可重用的畫布平移，讓世界層能套用鏡頭震動而 HUD 與選單維持穩定
- 依桌面工作區等比例縮放視窗，保留 1280×720 邏輯座標與輸入映射
- 2.5D 路徑、深度感地形、塔、敵人、投射物與粒子

## 玩法規則核心

- 局內永久強化節點定義於第 4、8 波結束後
- 火力每級增加 15%、賞金每級增加 20%、射程每級增加 12%，單項最多兩級
- 規則、存檔與 2.5D 選擇介面已整合，並以 C++20 編譯期斷言驗證倍率疊加及波次阻擋；Demo 2.7.6 由自動化流程執行啟動、渲染、記憶體與正常關閉驗證

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

Riftwarden 預設不載入 DLSS 模組，以降低 2D／2.5D 遊戲的常駐記憶體。需要 DLSS 時必須在建立 D3D 裝置前選用模組；引擎會先驗證 NVIDIA 雙重簽章，再動態載入 `sl.interposer.dll` 並透過代理 DXGI／D3D12 建立路徑完成整合。可使用下列任一方式：

```powershell
games/riftwarden/dist/Riftwarden-DX12.exe --dlss
$env:AEVRYND_ENABLE_DLSS='1'; games/riftwarden/dist/Riftwarden-DX12.exe
```

Engine Lab、Character Lab 與 Frontier World 會透過 `EngineConfig.enableDLSS` 啟用模組，持續驗證 3D 路徑。1280×720 輸出時，RTX 5080 上的 DLSS Quality 內部渲染解析度為 853×480；引擎會驗證實際 GPU 輸出，若 DLSS plugin 連續輸出黑幀便自動切回原生 DirectX 12。

- `DLSS QUALITY 853x480`：DLSS 正在執行升頻
- `DLSS OFF 1280x720`：原生解析度 fallback
- `DLSS N/A`：GPU、驅動或 Streamline plugin 不支援
- `DLSS MODULE OFF`：本次啟動未選用 DLSS，僅使用原生 DirectX 12

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
