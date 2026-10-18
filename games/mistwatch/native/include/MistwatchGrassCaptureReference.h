#pragma once
#include <Aevrynd/Math.h>
#include <bit>
#include <cstdint>

namespace Mistwatch::GrassCaptureReference {
// m2198 實測舊草人口的第 1 株，診斷取景不再依賴新世界的生成順序。
// 此常數只用於 grass-profile 截圖，不能用作遊戲角色或植物位置。
inline constexpr Aevrynd::Vec3 Root{
    std::bit_cast<float>(std::uint32_t{0x413584c9}),
    std::bit_cast<float>(std::uint32_t{0x3ec28f78}),
    std::bit_cast<float>(std::uint32_t{0x3fb1d9a6})};
inline constexpr Aevrynd::Vec3 Target(){return Root+Aevrynd::Vec3{0,.3f,0};}
inline constexpr Aevrynd::Vec3 Position(){return Target()+Aevrynd::Vec3{1.15f,.55f,-1.15f};}
}
