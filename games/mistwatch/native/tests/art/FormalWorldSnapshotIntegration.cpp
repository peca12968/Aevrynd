// 直接取正式來源，避免候選 include 順序影響獨立基準。
#include "formal-before/games/mistwatch/native/include/MistwatchWorld.h"
std::vector<Aevrynd::BoxCollider3D> FormalPropWorldSnapshot(bool repaired,Mistwatch::FirLayoutSelector selector) {
    return Mistwatch::WorldSolids(repaired,selector);
}
