# DDS 材質與正式磁碟區掛載相容性

2026-10-28：修正 `NativeMaterialDds3D.cpp` 將 Windows 正式掛載的磁碟區資料夾一律當成不安全連結的問題。這會讓已搬到 NTFS 虛擬磁碟的遊戲停在 `Fir material unavailable`，即使 DDS 大小與 SHA-256 正確。

## 接納範圍

一般檔案與一般目錄的規則不變。遇到 reparse 目錄時，必須同時滿足：

- 開啟不跟隨連結的目錄 handle，實際 tag 為 `IO_REPARSE_TAG_MOUNT_POINT`。
- 有界解析 reparse 資料，替代目標恰為 Windows 回報的磁碟區 GUID 根目錄；不接納指向任意子目錄的 junction。
- 該資料夾完整路徑確實列於 Windows 登記的磁碟區掛載清單。

仍保留檔案 handle 的讀取鎖、大小與 SHA-256 比對、DDS 格式／角色／mip／payload 上限檢查。這不是取消 reparse 驗證，也不是對某個使用者路徑、硬碟代號或 GUID 寫死特例。

使用 Windows 公開介面，參考 [取得掛載點的磁碟區名稱](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumenameforvolumemountpointw)、[取得磁碟區的正式掛載路徑清單](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumepathnamesforvolumenamew) 與 [reparse 資料格式](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_reparse_data_buffer)。

## 本輪實際驗證

證據位置：`C:/AevryndVolume/Aevrynd/Development/Mistwatch/m4181-dds-volume-root`。此路徑是 `F:/AevryndStorage/Aevrynd-NTFS.vhdx` 的資料夾掛載；新輸出未透過舊 E 槽別名寫入。

1. 原封閉引擎的 C++ DDS 測試在真實掛載路徑回報 `InvalidPath`，完整保留原失敗。
2. 重新編譯目前 `NativeMaterialDds3D.cpp`，連接既有 `NativeMaterialDds3DSmoke.cpp`，保留 `/W4 /WX /MP1`。既有材質、地表、錯誤雜湊／尺寸／格式、內容變更與目錄 junction 拒絕檢查通過；輸出含 `hostile=15 strictIntegrity=passed reparse=passed`。
3. 遊戲以目前 DDS 物件與其餘已封閉引擎 archive 作可追溯的增量連結；實際 compiler/link tlog 與 map 確認公開 DDS 函式來自新物件。不是全新 `ALL_BUILD`。
4. 普通遊戲入口實際執行 582 ticks，經 `Engine.GetInput` 的合成鍵值完成蹲站／移動、F5 存檔、F9 精確還原，正常退出且 owned Job 清空。兩張完整 PNG 已重新解碼，鏡頭與實際上傳矩陣相符。這不是人工鍵盤試玩、完整任務通關或 GPU validation layer 驗收。

可在自己的正式掛載來源路徑建置既有 `AevryndNativeMaterialDds3DTest`，執行同名 CTest；不需要新增套件或改動磁碟代號。上述局部證據不宣稱引擎整體完成或發布版本通過。

## 與美術候選分開

本輪同時取得原先未驗收落葉候選的完整畫面，但整體外觀改善不足，因此沒有採用該美術構造、提高分數或把候選發布成穩定版本。本提交只保存 DDS 掛載相容性修正；人物工作、網站及既有推薦版本保持原狀。
