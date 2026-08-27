# Original Character PBR Materials

執行期使用的 18 張 PBR 貼圖由 Aevrynd 專案的 Blender Python 工具以固定 seed 原創程序化產生，未使用外部圖片、模型或第三方材質。`skin/source` 內的圖片是本專案生成並納入同一 CC0 授權的視覺參考，不會在執行期載入。

- License: CC0-1.0，可商用、修改與再散布。
- Channels: base color、roughness、DirectX normal。
- Base color 維持約 0.82–1.0 的中性微細節，執行期再乘上 AESKIN 頂點色；因此男女角色可共用材質類型而不會雙重變暗或被固定色調覆蓋。
- Resolution: skin 與 eye 為 1024×1024，其餘為 512×512。
- Generator: `tools/blender/create-character-pbr-materials.py`
- Manifest: `manifest.json` 記錄固定 seed、尺寸、檔案大小與 SHA-256。

重新產生時，請以 Blender 5.2 background 執行並將此目錄作為輸出位置。
