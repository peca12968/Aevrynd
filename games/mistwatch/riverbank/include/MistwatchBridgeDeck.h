#pragma once

#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/HeightField3D.h>
#include <cmath>

namespace Mistwatch {
inline constexpr Aevrynd::BoxCollider3D RiverbankBridgeDeck{{0,-.2f,36}, {2,.2f,4}};
inline constexpr float RiverbankBridgeDeckTop = RiverbankBridgeDeck.center.y + RiverbankBridgeDeck.extents.y;

struct RiverbankWalkSurface {
    const Aevrynd::HeightField3D& terrain;
    bool bridgeOpen{};

    bool Contains(float x, float z) const { return terrain.Contains(x, z); }

    bool IsDeck(float x, float z, float terrainHeight) const {
        return bridgeOpen && terrainHeight <= RiverbankBridgeDeckTop &&
            std::abs(x - RiverbankBridgeDeck.center.x) <= RiverbankBridgeDeck.extents.x &&
            std::abs(z - RiverbankBridgeDeck.center.z) <= RiverbankBridgeDeck.extents.z;
    }

    float SampleHeight(float x, float z) const {
        const float ground = terrain.SampleHeight(x, z);
        return IsDeck(x, z, ground) ? RiverbankBridgeDeckTop : ground;
    }

    Aevrynd::Vec3 SampleNormal(float x, float z) const {
        if (IsDeck(x, z, terrain.SampleHeight(x, z))) return {0, 1, 0};
        return terrain.SampleNormal(x, z);
    }
};
}
