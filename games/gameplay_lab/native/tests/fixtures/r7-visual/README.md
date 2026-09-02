# R7 Visual Gate Fixture Contract

此目錄只描述獨立視覺 gate 的輸入契約，不包含 production 影像，也不得用合成 fixture 冒充正式擷取。

每輪必須提供 11 組 `1280x720` beauty、Object ID 與 depth PNG，名稱為 `first|third|god` × `day|dusk|night`，另加 `palette-building` 與 `melee-combat`。每組檔名分別為 `<name>.png`、`<name>-object-id.png`、`<name>-depth.png`。

同一擷取目錄必須提供 `r7-visual-sidecar.json`。Schema version 為 1，`shots` 必須是上述 11 個名稱的 exact-set；每列必須由正式 HDR/OID/材質分析產生：

- `hdr.meanLuma`、`hdr.p10Luma`
- `water.pixelCoverage`、`entropy`、`normalVariance`、`reflectionVariance`
- `terrain.autocorrelation32To256`
- `vegetation.coverage`、`spatialEntropy`
- `actor.coverage`
- `ui.coverage`、`heroOverlap`

缺少影像、sidecar、shot、欄位或 semantic readback 時一律 NO-GO。Release 必須有效分數至少 70，且沒有任何 hard issue。負向 fixture 應覆蓋黑夜、平水面、高重複地形、低密度植被、角色過小／過大與 UI 遮擋；不可藉由刪除 shot 或降低門檻通過。

`BuildR7VisualSyntheticFixture.ps1` 只產生測試用的完整可控資料，不得用於正式視覺評分。`GameplayR7VisualGateSyntheticTest.ps1` 會以固定 seed 在兩個獨立目錄生成 fixture、逐檔驗證 byte identity、重複驗證 PASS receipt，再執行七種負向 mutation。測試要求 Windows PowerShell 5.1；任何路徑、schema 或資料缺失都必須失敗。
