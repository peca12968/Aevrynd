#pragma once
#include "MistwatchAuthoredLandscapePlanting.h"
#include "MistwatchLandscapeStandFill.h"
#include <string_view>

namespace Mistwatch::MixedLandscapeCanopy {
inline constexpr std::string_view Profile="mixed-edge-canopy-v1";
namespace Detail {
inline void Need(bool yes,const char* message){if(!yes)throw std::invalid_argument(message);}
inline bool SameCounts(const LandscapeStandFill::Counts& a,const LandscapeStandFill::Counts& b){
    return a.proposals==b.proposals&&a.accepted==b.accepted&&a.slope==b.slope&&
        a.core==b.core&&a.edge==b.edge&&a.spacing==b.spacing;
}
}
// 新組合只接受明確的 capture 或 replay 候選；其他場景互斥仍由 Main 驗證。
// 不改寫舊 standalone 開關，也不自動採用候選作為一般遊戲預設。
inline bool Resolve(bool capture,bool replay,std::wstring_view requested){
    if(requested.empty()||requested==L"original")return false;
    Detail::Need(requested==L"mixed-edge-canopy-v1","MIXED_CANOPY_PROFILE");
    Detail::Need(capture!=replay,"MIXED_CANOPY_REQUIRED_CONTEXT");
    return true;
}
// 呼叫者須傳入實際四個 LOD2 與 near-twig 網格的聯集 bounds，不可只傳樹幹。
// 不相信 complete 或封印字串：以當前地形與來源重新生成，逐項核對 616 株原始配置。
inline void ValidateStand(const Aevrynd::HeightField3D& terrain,
    const std::vector<Aevrynd::Transform3D>& original520,const Aevrynd::MeshBounds3D& fullTreeUnionBounds,
    const LandscapeStandFill::Plan& stand){
    namespace S=LandscapeStandFill;
    const auto canonical=S::Create(terrain,original520,fullTreeUnionBounds);
    Detail::Need(canonical.complete&&canonical.combined.size()==S::BaseCount+S::Target&&
        canonical.addedBands.size()==S::Target,"MIXED_STAND_CANONICAL_INCOMPLETE");
    Detail::Need(stand.complete,"MIXED_STAND_INCOMPLETE");
    Detail::Need(stand.combined.size()==canonical.combined.size()&&stand.addedBands.size()==canonical.addedBands.size(),
        "MIXED_STAND_COUNT");
    Detail::Need(stand.sourceFingerprint==canonical.sourceFingerprint&&stand.fingerprint==canonical.fingerprint,
        "MIXED_STAND_FINGERPRINT");
    Detail::Need(stand.generated==canonical.generated,"MIXED_STAND_GENERATED");
    Detail::Need(stand.addedBands==canonical.addedBands,"MIXED_STAND_BANDS");
    for(unsigned band=0;band<S::BandCount;++band)
        Detail::Need(Detail::SameCounts(stand.bands[band],canonical.bands[band]),"MIXED_STAND_COUNTERS");
    for(std::size_t i=0;i<canonical.combined.size();++i)
        Detail::Need(S::Detail::Same(stand.combined[i],canonical.combined[i]),"MIXED_STAND_POSE");
}
// 三個來源需由既有 SHA 載入器驗證並且只 RootBroadleafMesh 一次；既有作者函式再驗完整解碼封印。
// actualFinalLandscape 必須已套用所有材質遮罩。回傳獨立替換映射，不改動原 616 株 Stand 封印。
inline AuthoredLandscapePlantingPlan Create(const Aevrynd::HeightField3D& terrain,
    const std::vector<Aevrynd::Transform3D>& original520,const Aevrynd::MeshBounds3D& fullTreeUnionBounds,
    const LandscapeStandFill::Plan& stand,const Aevrynd::Mesh3D& actualFinalLandscape,
    const std::array<Aevrynd::Mesh3D,3>& rootedSources){
    ValidateStand(terrain,original520,fullTreeUnionBounds,stand);
    return CreateAuthoredLandscapePlantingPlan(original520,actualFinalLandscape,rootedSources);
}
}
