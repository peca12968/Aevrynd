#pragma once

#include "RiverbankTerritory.h"
#include <Aevrynd/Mesh3D.h>

#include <algorithm>
#include <cmath>

namespace Mistwatch::TerritoryClearing {

inline constexpr float LowUnderstoryHeight = .46f;
inline constexpr float MaintainedRadius = RiverbankTerritory::Radius + 4.25f;
inline constexpr float WoodlandRadius = MaintainedRadius + 2.5f;

inline Aevrynd::Transform3D UnderstoryPose(Aevrynd::Transform3D pose, float localTop) {
    const float dx = pose.position.x - RiverbankTerritory::CenterX;
    const float dz = pose.position.z - RiverbankTerritory::CenterZ;
    const float distanceSquared = dx * dx + dz * dz;
    const float height = localTop * pose.scale.y;
    if (distanceSquared >= WoodlandRadius * WoodlandRadius || height <= LowUnderstoryHeight) return pose;

    // 保留枝葉覆蓋與地表根點，讓圈邊的修剪高度逐漸接回林地。
    const float edge = std::clamp((std::sqrt(distanceSquared) - MaintainedRadius) /
                                  (WoodlandRadius - MaintainedRadius), 0.f, 1.f);
    const float restoration = edge * edge * (3.f - 2.f * edge);
    pose.scale.y *= std::lerp(LowUnderstoryHeight / height, 1.f, restoration);
    return pose;
}

}
