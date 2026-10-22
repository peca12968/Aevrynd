# Original Character PBR Materials

執行期使用 27 張 PBR 貼圖，涵蓋 skin、lips、sclera、iris、cloth、leather、hair、eye 與 metal 九組材質；每組都有 base color、roughness 與 DirectX normal。所有貼圖皆由 Aevrynd 專案自行程序化產生或由同一批專案自製貼圖衍生，未混入外部圖片、模型或第三方材質，並以 CC0-1.0 釋出。`skin/source` 內的圖片是本專案生成並納入同一授權的視覺參考，不會在執行期載入。

- License: CC0-1.0，可商用、修改與再散布。
- Channels: base color、roughness、DirectX normal。
- Base color 維持約 0.82–1.0 的中性微細節，執行期再乘上 AESKIN 頂點色；因此男女角色可共用材質類型而不會雙重變暗或被固定色調覆蓋。
- Resolution: skin、lips、sclera 與 iris 為 2048×2048；cloth 與 hair 為 1024×1024；leather、eye 與 metal 為 512×512。
- Generator: `tools/generate-character-materials.py`
- Manifest: `manifest.json` 以 exact-set 記錄九組材質、27 張執行期貼圖、固定 seed、尺寸、檔案大小與 SHA-256。
- Sidecars: `female.materials.json` 與 `male.materials.json` 對每個實際 base color、roughness/ORM、normal 與額外 layer 引用都固定記錄 `path`、`expectedBytes`、`expectedSha256`。解析器不會在執行期補算或接受缺少的完整性資料；重複路徑必須宣告完全相同的大小與 SHA-256。

重新產生時，請使用專案鎖定的 Python 與 Pillow 環境執行 `tools/generate-character-materials.py`，並將此目錄作為唯一輸出位置；完成後必須核對 `manifest.json` 的 exact-set、大小與 SHA-256。
