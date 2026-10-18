#pragma once
#include "MistwatchRepairShelterRoofUv.h"
#include <array>
#include <bit>
#include <cstdint>

namespace Mistwatch::ShelterWeatheredUv {
inline constexpr const char* Profile="weathered-bands-v1";
// 對應既有 Worn Planks 真圖的四塊灰褐木板，避開板縫與黃漆。
inline constexpr std::array<Aevrynd::Vec2,4> Bands{{
    {132.f/1024.f,216.f/1024.f},{360.f/1024.f,436.f/1024.f},
    {694.f/1024.f,758.f/1024.f},{932.f/1024.f,1006.f/1024.f}}};
inline constexpr float LongitudinalMeters=1.4f;

[[nodiscard]] inline Aevrynd::Mesh3D Create() {
    auto result=ShelterRoofUv::CreatePlankedRepairShelterRoofUv();
    std::size_t first{};
    for(unsigned part=0;part<ShelterBoardCounts.size();++part) {
        for(unsigned board=0;board<ShelterBoardCounts[part];++board) {
            if(part==4)continue; // 棚內工具台的全部頂點保持原樣。
            const auto band=Bands[(board+part*3u)%Bands.size()];
            const float oldPhase=.1f+.025f*float(part==0?board%5u:(board+part*3u)%5u);
            const float phase=.17f*float((board+part*2u)%5u);
            for(std::size_t i=first+std::size_t(board)*96;i<first+std::size_t(board+1)*96;++i) {
                auto& v=result.vertices[i];
                const float across=(v.u*ShelterRoofUv::StaticDecodedScale-ShelterRoofUv::SourceULow)/ShelterRoofUv::SourceUWidth;
                const float along=(v.v*ShelterRoofUv::StaticDecodedScale-oldPhase)/.5f;
                v.u=(band.x+across*(band.y-band.x))/ShelterRoofUv::StaticDecodedScale;
                v.v=(phase+along/LongitudinalMeters)/ShelterRoofUv::StaticDecodedScale;
                if(!std::isfinite(v.u)||!std::isfinite(v.v))throw std::runtime_error("Weathered shelter UV not finite");
            }
        }
        first+=std::size_t(ShelterBoardCounts[part])*96;
    }
    if(first!=result.vertices.size())throw std::runtime_error("Weathered shelter complete board membership");
    result.InvalidateRaycastIndex();return result;
}
inline std::uint64_t Fingerprint(const Aevrynd::Mesh3D& mesh) {
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& v:mesh.vertices)for(const auto value:{v.u,v.v}) {
        const auto bits=std::bit_cast<std::uint32_t>(value);
        for(unsigned byte=0;byte<4;++byte){hash^=(bits>>(byte*8))&255u;hash*=1099511628211ull;}
    }
    return hash;
}
}
