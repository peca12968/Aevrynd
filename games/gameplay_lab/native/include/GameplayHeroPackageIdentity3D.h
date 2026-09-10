#pragma once
#include "GameplayCandidate7Package3D.h"
#include <string_view>

namespace GameplayLab {
// 僅接受明確版本；不存在自動回退或根據檔名猜角色。
struct HeroPackageSelection3D {
    unsigned schemaVersion{1};
    std::string heroId;
    std::filesystem::path root;
};
struct SelectedHeroPackage3D {
    Candidate7Package3D package;
    std::string actualHeroId;
    std::string identityReceiptJson;
};
// 拒絕載入後混入不同骨架；也供獨立敵意測試直接驗證記憶體邊界。
void ValidateCandidate7ComponentIdentity3D(const Candidate7Package3D& package);
// 丟出例外即選擇失敗；不配置 GPU、不寫檔、不宣稱已 Draw 或視覺通過。
SelectedHeroPackage3D SelectHeroPackage3D(const HeroPackageSelection3D& selection);
}
