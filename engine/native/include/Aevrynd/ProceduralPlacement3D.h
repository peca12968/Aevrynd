#pragma once
#include "Math.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Aevrynd {

struct PlacementBounds3D {Vec3 minimum{},maximum{};};
struct PlacementExclusion3D {Vec3 center{};float radius{};};
struct PlacementInstance3D {Vec3 position{};float scale{1.f};float yaw{};};
using PlacementHeightCallback3D=bool(*)(float x,float z,float& height,void* context);

struct ProceduralPlacementSettings3D {
    PlacementBounds3D bounds{};
    std::size_t targetCount{};
    float densityPerSquareUnit{};
    float minimumSpacing{1.f};
    float minimumScale{.85f};
    float maximumScale{1.15f};
    std::uint32_t seed{1};
    std::uint32_t attemptsPerInstance{24};
    std::size_t maximumInstances{100000};
    std::size_t maximumGridCells{1000000};
    std::span<const PlacementExclusion3D> exclusions{};
    PlacementHeightCallback3D heightCallback{};
    void* heightContext{};
};

struct ProceduralPlacementResult3D {
    std::vector<PlacementInstance3D> instances;
    std::size_t attempts{};
    bool exhausted{};
};

// 無效或超出硬限制的設定會回傳空結果；配置不足則 exhausted 為 true。
ProceduralPlacementResult3D GenerateProceduralPlacement3D(const ProceduralPlacementSettings3D& settings);

}
