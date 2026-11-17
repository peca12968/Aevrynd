# 河岸多人角色資產

RiverbankExplorer.aeskin 是 Aevrynd 角色歷史素材的遊戲用版本。G3 提供骨架與分層服裝，W3 提供步行，P3 提供長褲。靴子使用 F3 足型製作，擬合版本為 B3。側倒片段沿用同一骨架，在題材角色的獨立 Blender 副本中製作。

| 資產 | SHA-256 |
| --- | --- |
| G3 服裝來源 | 70eb81cba943c652a4f9daa45fe777aa4496653d6a162df5706225b7066353c2 |
| W3 步行來源 | 1da36e66362fd8f822f9360678e3149d989fe252d517b0e81356b9b38ba49031 |
| F3 足型來源 | 19e776498a3ba229bd0268451b9c394362511764fb68ccda78671fa742b9dbfe |
| B3 人物來源 | f036e8e4af19388fb3889fc0e380c9bc5b860f289da7abc5ea8a69c47860a6f6 |
| 多人版 AESKIN | a1a1d2bd0290ba2ec01527bb6d7af10fe214514ca34a6230fab3cf874cfef42a |

遊戲資產包含 W3 步行與一秒的側倒動作。側倒透過具名骨架片段匯入，轉換時保留與綁定姿勢不同的固定關節造型。幾何、材質、骨架與原步行片段沿用 B3 遊戲用資料。

啟用 CMake 的 AEVRYND_BUILD_MISTWATCH_MULTIPLAYER 即可使用這份資產。建置會以 SHA-256 核對資產，再複製到多人客戶端相鄰的 character 目錄。MISTWATCH_LOCAL_WORLD_AVATAR 可指定雜湊清單裡的其他版本。

## 職業服裝輪廓

三種服裝沿用共同的 80 骨骼、步行與側倒動畫。從歷史披肩研究的 A/B 打版量出肩寬與後襬差異，並以既有背心網格重打版。守衛的背心上肩加寬，遊俠的後襬拉長，祭司的前襟加長。逐職業對照 58 個步行與側倒影格，檢查身體、內衣與背心交疊。底色與眉部參考沿用同一歷史角色，眉部只變更皮膚頂點的色彩。

| 資產 | SHA-256 |
| --- | --- |
| 共用基準（眉部修整） | 013a9f3a37f0942652ebd79932cb33955987515ffd829f4a981add4aa7f40d2e |
| Guardian | d9e3b391ee44e852aa85cad40cb1f207acf1ef6e5d7d082d2b3f24a10c12d841 |
| Ranger | 182f1baacdd1b7431aca9f49e579df150c392bdbffbb364cddc1ee77eeff5c6f |
| Priest | 0c215fa6a19b37ef02df4486262485ebc4f9df4262184d0e3da743582cb393de |

## 多人跑步

一輪專用跑姿由相同的 80 骨骼製作，0.708333 秒前進 2.266667 公尺。在共用版本增加跑步片段，保留原本的網格、材質、綁定姿勢、步行及側倒資料。斜坡依左右靴底分開調整腿部，維持原跑姿的騰空高度及靴子朝向。坡面支撐取自各靴底下方的地形高度。測試包含合成坡面與河岸東坡 972 個跑姿樣本。

含跑步動作的共用 AESKIN SHA-256 為 `03d30e0c5c050394391e9b18693b33616b67364ed87920eb92b8ea2a22ec5587`。

## r0012 職業髮型

守衛沿用 T2。遊俠使用 T1，祭司使用較短的 S3。兩款由歷史後頸髮型曲線匯出，與共同角色共用頭骨、材質與姿勢。使用含跑步片段的基準時，建置會核對兩款檔案，再一併放進多人試玩套件。

r0012 Blender 來源 SHA-256 為 7918cb2a631938402fd55d7e8c3dc30b45e48dd1d047b3b1748ab742324cf4b2。
- Ranger SHA-256 為 fe27db7b117514aa5ce9fa26fa64a41b3cd3b7162d5153f56f076af14eafe6cd。
- Priest SHA-256 為 09eab6f46a2058c69a72b3090429a93b556d8efd1bacf921bf7ad161a901b3ec。

## 開發歷史庫

以下是相對於 AevryndEvidence 歷史庫根目錄的路徑。角色製作用 Blender 副本及相容部件，原始檔保留作為來源。遊戲套件使用已匯出並核對雜湊的 AESKIN。

| 用途 | 歷史庫相對路徑 |
| --- | --- |
| G3 骨架及分層衣物 | `female-next-owned-garments/female-next-r0022-G3-layers-RESEARCH.blend` |
| W3 步行 | `female-next-owned-walk/female-next-r0021-W3-walk-RESEARCH.blend` |
| P3 長褲驗證紀錄 | `female-next-owned-trousers/trousers-draft-preflight.json` |
| F3 足型 | `female-next-owned-feet/female-next-r0020-F3-feet-RESEARCH.blend` |
| r0012 後頸髮型 | `female-next-owned-nape-groom/female-next-r0012-owned-nape-groom-FOUNDATION.blend` |

後期 r0426 眼部及 r0594 唇形研究的面索引與遊戲基準不同。可以參考輪廓及材質，在現用骨架上重作細節。需要整體替換時，先重驗骨架、表情、接地、服裝和動作。

## 多人跳躍試玩

角色沿用 G3 骨架、W3 步態、P3 長褲、F3 足型與 r0012 職業髮型。
以守衛 Blender 副本製作共用原地跳躍片段。網格、材質、骨架和原有動畫保持原檔資料。
伺服器決定跳躍軌跡，客戶端用權威高度和垂直運動方向播放角色姿勢。
現有專用伺服器、客戶端與角色資產須一併更新。

## 河岸多人野豬

多人版使用與單人版相容的十六關節骨架，沿體軸生成連續斷面，重做鬃毛、耳朵和口鼻。
多人客戶端讀取 RiverbankBoar.aeskin，SHA-256 為 1693b2a2db62a22ee379b799fe7b0b28a0bdb9ce18032cc51b148c9f27c15426。
單人角色與單人野豬維持既有資產部署。

## 歷史庫 r0594 下唇與下巴

共用角色沿用 r0594-lip-chin-profile/fit-receipt.json 的 B 方案，在河岸模型的既有網格上重作下唇至下巴輪廓。
調整 313 個匯出頂點，最大位移 1.0833 毫米。只修改局部頂點與法線，骨架、權重、材質、髮型、服裝與四段動畫資料維持一致。
歷史試作的完整模型仍是研究檔。遊戲使用河岸相容網格，並另存守衛、遊俠、祭司的可編輯 Blender 副本。

- r0594 參考紀錄 SHA-256：ec4d36fa8c2953e024064fd9481425485abfbe7d26d86b7e164f54e663f28acb
- 含跑步及跳躍的河岸角色 SHA-256：d5e382b063a007e98b772df846322de6b7014ac6ee9d7e9497a1aa008c16e776
- 含 r0594 局部修整的河岸角色 SHA-256：549ccccab3d8de8592a251b41b0fc7c7b583a4d7eff83218018a4e65a28fa031

## 歷史庫選材與匯入

人物製作從 `F:\AevryndEvidence` 查找骨架、面部、服裝、髮型及動畫等各部件的歷史版本。記錄來源相對路徑、SHA-256 和驗證結果，在獨立的遊戲用 Blender 副本上工作。保留目前的共用骨架和各職業可編輯副本。

| 歷史候選 | 用途與相容性 |
| --- | --- |
| `r0606-malar-form/female-next-r0606c-malar-form-UNACCEPTED.blend` | 用於參考顴骨和下眼瞼至臉頰間的曲面。主網格有 170,091 個頂點，以及 53 骨身體骨架和 8 骨眼球控制。 |
| `r0627-fringe-launch-bridge/C1/female-next-r0627c1-fringe-launch-bridge-UNACCEPTED.blend` | 用於參考瀏海從頭皮伸出的方向和側面髮束。主網格有 170,097 個頂點，沿用 53 骨身體骨架和 8 骨眼球控制，歷史實驗改善部分髮束交疊，側面整體輪廓仍未通過原有審查。 |

河岸可編輯守衛基準的主網格有 32,082 個頂點，使用 80 骨共用骨架。上述研究檔在檢查範圍內沒有同時符合面索引、綁定姿勢、基礎座標和物件變換的網格。將要採用的局部輪廓或材質在河岸基底重作，再檢查前、側、背面與動態穿模。髮型使用遊戲用網格，測完步行、跑步和跳躍的接合，再比較實機影格時間。
