#pragma once
#include "Mesh3D.h"
#include <cstdint>

namespace Aevrynd {

enum class ArchitectureMeshDetail3D : std::uint8_t {Near,Far};

struct CottageMeshSettings3D {
    float width{6.f};
    float depth{4.5f};
    float wallHeight{3.f};
    float roofHeight{1.6f};
    float wallThickness{.24f};
    float roofOverhang{.35f};
    std::uint32_t seed{1};
    ArchitectureMeshDetail3D detail{ArchitectureMeshDetail3D::Near};
};

// 建築網格以原點為地面中心，正面朝向負 Z。
Mesh3D CreateCottageMesh3D(const CottageMeshSettings3D& settings={});

}
