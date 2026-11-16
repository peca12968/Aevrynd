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
