# 封閉的歷史測試參照

本目錄 `mistwatch/` 六個標頭是 query run4 與 vertical run7 真正引用、且兩份收據 raw pins 相同的完整相對 include 閉包，逐位元組原樣保存。它們只服務既有 Encounter 八方向及 NPC 靜態／地形 resolver 控制案例；沒有假造 callback、刪除 gameplay 斷言或依賴 Evidence 絕對路徑。

這不是正式新版 EncounterMotion（950…）的資格證明；新版 pair／main 接線仍須由其專用測試與整合驗收覆蓋。這些參照也不是新產品的第二個實作入口。

`original-vertical1536.golden` 另置於 tests/fixtures/dynamic-contact，是原控制器 24 配方 × 64 tick × 23 欄的 ASCII／LF 全 byte 預期資料（195129 byte，SHA-256 41AE383AEAE1E85F08BDFC91F58A65B406B078E0847C8768732DFF89F517E359）。正式 vertical 測試須完整重產並逐 byte 相等；檔案不含 process、stdout、status 或 timestamp 執行紀錄。
