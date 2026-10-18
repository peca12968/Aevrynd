# dead_tree_trunk — 資產來源與授權

本包的倒木原始幾何與貼圖來自 Poly Haven 的 `dead_tree_trunk` 資產；既有官方下載來源收據記錄其授權為 **CC0-1.0**。本通知只涵蓋此資產，不以舊 firstbatch12 或 dead_tree_trunk_02 的通知替代。

- 官方資產頁：https://polyhaven.com/a/dead_tree_trunk
- 官方檔案 API：https://api.polyhaven.com/files/dead_tree_trunk
- Poly Haven 授權說明：https://polyhaven.com/license
- CC0 1.0 參考：https://creativecommons.org/publicdomain/zero/1.0/

五個官方檔案（glTF、BIN、diff／ARM／nor_gl 三張 JPG）的下載 URL、官方 MD5、實際 SHA-256 與大小，均列於 `fallen-log-package-manifest.json`；原始 bytes 保留在 `source/dead-tree-trunk/`。

## 加工與綁定

Aevrynd 使用原始來源，在 Blender 5.2.1 LTS 以 ratio 0.15 本地生成 LOD2，再轉成 AEMESH；不是官方提供的 LOD2 下載。轉換保留 UV／法線並依引擎座標約定反射 Z 與調整 winding。材質使用 2048²、完整 12 mip 的 DDS。

LOD2 的唯一 materialnone／-1 primitive，由 Aevrynd 明示外部綁定到原 glTF material0／cooked slot-0。原始 base 與 roughness factor 為 glTF 預設 1、metallic 為 0、doubleSided 為 true。ARM 的 R／G／B 分別用於 AO／roughness／metallic；原 glTF 沒有 occlusionTexture，因此 AO 是明示手動延伸，不宣稱原 glTF 已接入 AO。nor_gl 的綠色通道轉換為 DirectX 約定。

`provenance/` 保存原始來源／加工收據的逐 byte 副本；其中舊絕對執行路徑僅為歷史觀察，不是執行時依賴。`recipes/` 與 `source/` 只用於可攜再加工來源，不應隨正式遊戲 runtime 全量複製。

本資產通知不將 Aevrynd 引擎程式或附帶的專案轉換腳本重新授權為 CC0。脚本是此專案既有來源的原文副本。

此包未執行新的建置、GPU、美術驗收或正式產品採用；授權／來源紀錄不代表這些品質條件已通過。
