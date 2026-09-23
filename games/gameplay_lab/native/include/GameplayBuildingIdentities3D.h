#pragma once
#include "GameplayShowcaseEnvironment3D.h"
#include <span>
#include <stdexcept>

namespace GameplayLab {
using GameplayBuildingIdentities3D = std::array<Aevrynd::RenderObjectIdentity3D, 6>;
inline GameplayBuildingIdentities3D BuildGameplayBuildingIdentities3D(
    std::span<const ShowcaseSceneDescriptor3D> descriptors, std::uint64_t domain) {
  GameplayBuildingIdentities3D result{};
  // 與正式 descriptor 提交共用索引語意；地被數量或順序不得冒充建築身分。
  std::size_t part{};
  for(std::size_t index=0;index<descriptors.size();++index){
    if(descriptors[index].kind!=ShowcaseSceneKind3D::Building)continue;
    // Reference-opacity evidence intentionally measures the six-piece cottage.
    // World landmarks such as the river bridge remain normal render identities,
    // but must not widen that established cottage-only evidence contract.
    if(descriptors[index].meshAsset=="procedural/forest-river-bridge")continue;
    if(part==result.size())throw std::runtime_error("Gameplay building identity capacity exceeded");
    result[part++]={{domain,index+1},1};
  }
  if(part!=result.size())throw std::runtime_error("Gameplay building identity set incomplete");
  return result;
}
inline std::size_t GameplayBuildingPart3D(const GameplayBuildingIdentities3D& identities,
                                         const Aevrynd::RenderObjectIdentity3D& identity) {
  for(std::size_t part=0;part<identities.size();++part)
    if(identities[part]==identity)return part;
  return identities.size();
}
}
