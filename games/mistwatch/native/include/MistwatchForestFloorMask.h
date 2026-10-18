#pragma once
#include <Aevrynd/Math.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace Mistwatch {

enum class ForestFloorMaskSelector : std::uint8_t { Legacy, ForestWood };

struct ForestFloorCoverage {
    float soil{};
    float rock{};
};

inline bool ValidForestFloorMaskSelector(ForestFloorMaskSelector selector) noexcept {
    return selector==ForestFloorMaskSelector::Legacy||selector==ForestFloorMaskSelector::ForestWood;
}

inline float ForestFloorSmooth(float value) noexcept {
    const float t=std::clamp(value,0.f,1.f);
    return t*t*(3.f-2.f*t);
}

inline float CentralTrailVisualWeight(float x,float z,ForestFloorMaskSelector selector) {
    if(!ValidForestFloorMaskSelector(selector)||!std::isfinite(x)||!std::isfinite(z))
        throw std::invalid_argument("Invalid forest-floor selector or position");
    if(z< -10.f||z>96.f||(z>32.f&&z<40.f))return 0.f;
    const float centre=.35f*std::sin(z*.19f);
    if(selector==ForestFloorMaskSelector::Legacy) {
        const float width=2.8f+.3f*std::sin(z*.37f);
        return ForestFloorSmooth(1.f-std::abs(x-centre)/width);
    }
    // 只縮窄畫出的裸土核心；樹幹及任務通路安全淨空仍由既有 world gate 負責。
    const float shoulder=2.10f+.15f*std::sin(z*.31f);
    const float distance=std::abs(x-centre);
    if(distance>=shoulder)return 0.f;
    const float profile=ForestFloorSmooth(1.f-distance/shoulder);
    const float breakup=.82f+.18f*ForestFloorSmooth(.5f+.5f*std::sin(x*1.73f+z*.61f)*std::cos(x*.47f-z*1.11f));
    const float shoulderMix=ForestFloorSmooth((distance-.70f)/(shoulder-.70f));
    return profile*profile*(1.f-shoulderMix+shoulderMix*breakup);
}

inline ForestFloorCoverage ForestFloorAuthorCoverage(float x,float z,Aevrynd::Vec3 normal,
    ForestFloorMaskSelector selector) {
    if(!ValidForestFloorMaskSelector(selector)||!std::isfinite(x)||!std::isfinite(z)||
        !std::isfinite(normal.x)||!std::isfinite(normal.y)||!std::isfinite(normal.z))
        throw std::invalid_argument("Invalid forest-floor author input");
    if(selector==ForestFloorMaskSelector::Legacy||(z>32.f&&z<40.f))return {};
    const float patch=.5f+.5f*std::sin(x*.173f+z*.119f)*std::cos(x*.071f-z*.157f);
    const float soilPatch=.28f*ForestFloorSmooth((patch-.30f)/.55f);
    const float trailSoil=.78f*CentralTrailVisualWeight(x,z,selector);
    const float slope=ForestFloorSmooth((.88f-normal.y)/.22f);
    const float rock=.72f*slope*(.78f+.22f*ForestFloorSmooth(patch));
    const float soil=(std::max)(soilPatch,trailSoil)*(1.f-rock);
    return {soil,rock};
}

inline ForestFloorCoverage ComposeForestFloorCoverage(float x,float z,Aevrynd::Vec3 normal,float riverWeight,
    ForestFloorMaskSelector selector) {
    if(!std::isfinite(riverWeight)||riverWeight<0.f||riverWeight>1.f)
        throw std::invalid_argument("Invalid river coverage");
    if(!ValidForestFloorMaskSelector(selector))throw std::invalid_argument("Invalid forest-floor selector");
    if(selector==ForestFloorMaskSelector::Legacy)return {.15f*riverWeight,.85f*riverWeight};
    const auto authored=ForestFloorAuthorCoverage(x,z,normal,selector);
    const float remaining=1.f-riverWeight;
    const ForestFloorCoverage result{.15f*riverWeight+remaining*authored.soil,
        .85f*riverWeight+remaining*authored.rock};
    if(!std::isfinite(result.soil)||!std::isfinite(result.rock)||result.soil<0.f||result.rock<0.f||
        result.soil>1.f||result.rock>1.f||result.soil+result.rock>1.000001f)
        throw std::runtime_error("Forest-floor coverage escaped normalized bounds");
    return result;
}

}
