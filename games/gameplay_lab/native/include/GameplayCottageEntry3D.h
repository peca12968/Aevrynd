#pragma once

#include <Aevrynd/EnvironmentCollisionStreaming3D.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace GameplayLab {

inline constexpr float GameplayCottageDoorClearWidth3D = 1.50f;
inline constexpr float GameplayCottageDoorClearHeight3D = 2.20f;
inline constexpr float GameplayCottageControllerDiameter3D = .90f;
inline constexpr float GameplayCottageControllerHeight3D = 1.80f;
inline constexpr std::size_t GameplayCottageFrontCollisionPieceCount3D = 3u;
inline constexpr std::size_t GameplayCottageAdditionalCollisionDescriptors3D =
    GameplayCottageFrontCollisionPieceCount3D - 1u;

// Replaces the cottage-front solid proxy with left jamb, right jamb and header.
// Other descriptors pass through unchanged. The two supplemental stable IDs must
// be reserved by the caller and remain in the environment identity domain.
[[nodiscard]] std::size_t BuildGameplayCottageCollisionPieces3D(
    const Aevrynd::EnvironmentColliderDescriptor3D &source,
    std::string_view meshAsset, std::uint64_t firstSupplementalIdentityLow,
    std::array<Aevrynd::EnvironmentColliderDescriptor3D,
               GameplayCottageFrontCollisionPieceCount3D> &pieces) noexcept;

} // namespace GameplayLab
