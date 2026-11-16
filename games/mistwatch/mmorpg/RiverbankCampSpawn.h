#pragma once

#include <Aevrynd/Math.h>
#include <array>
#include <cmath>

namespace Mistwatch {
inline constexpr auto RiverbankCampSpawnSites = [] {
    constexpr float spacing = 1.2f;
    constexpr std::array columns{0, 1, -1, 2, -2, 3, -3};
    constexpr std::array rows{0, 1, -1, -2, -3, -4};
    std::array<Aevrynd::Vec3, columns.size() * rows.size() - 1> positions{};
    std::size_t index = 0;
    for (const int z : rows)
        for (const int x : columns) {
            // 西南側樹幹佔用這一格。其餘格點保留任務牌與道路的通行空間。
            if (x == -3 && z == -3) continue;
            positions[index++] = {x * spacing, 0.f, z * spacing};
        }
    return positions;
}();

inline bool IsRiverbankCampSpawn(Aevrynd::Vec3 feet) {
    for (const auto marker : RiverbankCampSpawnSites)
        if (std::abs(feet.x - marker.x) <= .002f && std::abs(feet.z - marker.z) <= .002f)
            return true;
    return false;
}
}
