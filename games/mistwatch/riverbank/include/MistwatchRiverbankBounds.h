#pragma once

namespace Mistwatch::RiverbankBounds {
inline constexpr float MinimumX = -50.f, MinimumZ = -20.f, CellSize = .5f;
inline constexpr unsigned SamplesX = 201, SamplesZ = 241;
inline constexpr float MaximumX = MinimumX + float(SamplesX - 1) * CellSize;
inline constexpr float MaximumZ = MinimumZ + float(SamplesZ - 1) * CellSize;
inline constexpr float BodyClearance = .45f;
inline constexpr float WalkMinX = MinimumX + BodyClearance, WalkMaxX = MaximumX - BodyClearance;
inline constexpr float WalkMinZ = MinimumZ + BodyClearance, WalkMaxZ = MaximumZ - BodyClearance;
}
