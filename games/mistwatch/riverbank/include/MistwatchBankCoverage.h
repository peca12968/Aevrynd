#pragma once
#include "MistwatchForestFloorMask.h"
#include "MistwatchRiverProfile.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace Mistwatch {
enum class BankCoverageSelector : std::uint8_t { Legacy, Patchy };

inline bool ValidBankCoverageSelector(BankCoverageSelector selector) noexcept {
    return selector==BankCoverageSelector::Legacy||selector==BankCoverageSelector::Patchy;
}

inline void ValidateBankCoverageRoute(ForestFloorMaskSelector floor,BankCoverageSelector bank) {
    if(!ValidForestFloorMaskSelector(floor)||!ValidBankCoverageSelector(bank)||
        (bank==BankCoverageSelector::Patchy&&floor!=ForestFloorMaskSelector::ForestWood))
        throw std::invalid_argument("Invalid forest-wood bank route");
}

inline float ForestWoodBankSedimentWeight(float x,float z,Aevrynd::Vec3 normal,float oldWeight) {
    if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(normal.x)||!std::isfinite(normal.y)||
        !std::isfinite(normal.z)||normal.y< -1.f||normal.y>1.f||!std::isfinite(oldWeight)||oldWeight<0.f||oldWeight>1.f)
        throw std::invalid_argument("Invalid forest-wood bank input");
    // 沿用舊作者的橋域及水下界線；不以新斑塊延伸沉積範圍。
    const float distance=RiverDistance(x,z),width=RiverOuterWidth(x);
    if(oldWeight==0.f||std::abs(x)<=16.f||distance<=width*.5f)return oldWeight;
    const float outer=RiverSmooth((std::abs(x)-12.f)/12.f);
    const float bank=z<RiverCenterZ(x)?13.f+2.f*std::sin(x*.07f):13.f+2.f*std::cos(x*.08f);
    const float transition=(4.f+(bank-4.f)*outer)*(1.f+.5f*RiverErosionMask(x,z));
    const float t=(distance-width)/transition;
    // 下岸保留原沉積；上岸逐漸改成固定世界座標、非格子排列的連續斑塊。
    // 平滑函式具有真正零支撐區，坡肩可退回森林基底，不再保留 65% 下限。
    const float shoulder=RiverSmooth((t+.10f)/.35f);
    const float signal=.5f+.26f*std::sin(x*.17f+z*.11f)+
        .16f*std::cos(x*.09f-z*.19f)+.08f*std::sin(x*.037f+z*.071f);
    const float deposit=RiverSmooth((signal-.43f)/.25f);
    const float weight=oldWeight*(1.f-shoulder*(1.f-deposit));
    if(!std::isfinite(weight)||weight<0.f||weight>oldWeight)
        throw std::runtime_error("Bank sediment escaped bounded support");
    return weight;
}

inline ForestFloorCoverage ComposeBankFloorCoverage(float x,float z,Aevrynd::Vec3 normal,float oldWeight,
    ForestFloorMaskSelector floor,BankCoverageSelector bank) {
    ValidateBankCoverageRoute(floor,bank);
    const float weight=bank==BankCoverageSelector::Patchy?
        ForestWoodBankSedimentWeight(x,z,normal,oldWeight):oldWeight;
    return ComposeForestFloorCoverage(x,z,normal,weight,floor);
}
} // Mistwatch 命名空間
