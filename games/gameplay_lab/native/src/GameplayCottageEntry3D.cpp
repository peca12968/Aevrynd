#include "GameplayCottageEntry3D.h"

#include <cmath>
#include <limits>

namespace GameplayLab {
namespace {

bool Finite(Aevrynd::Vec3 value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

bool Valid(const Aevrynd::EnvironmentColliderDescriptor3D &value) noexcept {
  return value.id.high != 0 && value.id.low != 0 && Finite(value.center) &&
         Finite(value.extents) && std::isfinite(value.yaw) &&
         value.extents.x > 0.f && value.extents.y > 0.f &&
         value.extents.z > 0.f;
}

Aevrynd::Vec3 RotateLocalXZ(float x, float z, float yaw) noexcept {
  const float cosine = std::cos(yaw), sine = std::sin(yaw);
  return {x * cosine + z * sine, 0.f, -x * sine + z * cosine};
}

} // namespace

std::size_t BuildGameplayCottageCollisionPieces3D(
    const Aevrynd::EnvironmentColliderDescriptor3D &source,
    std::string_view meshAsset, std::uint64_t firstSupplementalIdentityLow,
    std::array<Aevrynd::EnvironmentColliderDescriptor3D,
               GameplayCottageFrontCollisionPieceCount3D> &pieces) noexcept {
  pieces = {};
  if (!Valid(source))
    return 0;
  if (meshAsset == "procedural/cottage-foundation") {
    // The visible plinth is not a step-capable controller surface. Keep its
    // collision identity registered, but recess the proxy below the authored
    // terrain so its front edge cannot silently seal an otherwise open door.
    pieces[0] = source;
    const float bottom = source.center.y - source.extents.y;
    pieces[0].extents.y = .04f;
    pieces[0].center.y = bottom - pieces[0].extents.y - .01f;
    return 1;
  }
  if (meshAsset != "procedural/cottage-front") {
    pieces[0] = source;
    return 1;
  }
  if (source.semantic != Aevrynd::EnvironmentColliderSemantic3D::Building ||
      firstSupplementalIdentityLow == 0 ||
      firstSupplementalIdentityLow == (std::numeric_limits<std::uint64_t>::max)() ||
      firstSupplementalIdentityLow == source.id.low ||
      firstSupplementalIdentityLow + 1u == source.id.low ||
      source.extents.x * 2.f <= GameplayCottageDoorClearWidth3D ||
      source.extents.y * 2.f <= GameplayCottageDoorClearHeight3D)
    return 0;

  const float openingHalfWidth = GameplayCottageDoorClearWidth3D * .5f;
  const float sideHalfWidth = (source.extents.x - openingHalfWidth) * .5f;
  const float openingBottom = source.center.y - source.extents.y;
  const float openingTop = openingBottom + GameplayCottageDoorClearHeight3D;
  const float wallTop = source.center.y + source.extents.y;
  const float headerHalfHeight = (wallTop - openingTop) * .5f;
  if (!(sideHalfWidth > 0.f) || !(headerHalfHeight > 0.f) ||
      GameplayCottageDoorClearWidth3D < GameplayCottageControllerDiameter3D + .12f ||
      GameplayCottageDoorClearHeight3D < GameplayCottageControllerHeight3D + .10f)
    return 0;

  pieces[0] = source;
  pieces[0].center = source.center +
      RotateLocalXZ(-(openingHalfWidth + sideHalfWidth), 0.f, source.yaw);
  pieces[0].extents.x = sideHalfWidth;
  pieces[1] = source;
  pieces[1].id.low = firstSupplementalIdentityLow;
  pieces[1].userData = (source.userData & 0xffffffff00000000ull) |
                       (firstSupplementalIdentityLow & 0xffffffffull);
  pieces[1].center = source.center +
      RotateLocalXZ(openingHalfWidth + sideHalfWidth, 0.f, source.yaw);
  pieces[1].extents.x = sideHalfWidth;
  pieces[2] = source;
  pieces[2].id.low = firstSupplementalIdentityLow + 1u;
  pieces[2].userData = (source.userData & 0xffffffff00000000ull) |
                       ((firstSupplementalIdentityLow + 1u) & 0xffffffffull);
  pieces[2].center.y = openingTop + headerHalfHeight;
  pieces[2].extents = {openingHalfWidth, headerHalfHeight, source.extents.z};
  return GameplayCottageFrontCollisionPieceCount3D;
}

} // namespace GameplayLab
