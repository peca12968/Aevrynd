#include "ShowcaseRenderAssetBindings3D.h"

#include <Aevrynd/Mesh3D.h>

#include <array>
#include <cmath>
#include <limits>

using namespace GameplayLab;

namespace {
bool Close(float left, float right) noexcept {
  return std::abs(left - right) <= 0.0001f;
}
} // namespace

int main() {
  const std::array<Aevrynd::MeshBounds3D, 3> validLods{{
      {{0.0f, 2.0f, 0.0f}, 3.0f, {2.0f, 3.0f, 2.0f}},
      {{0.02f, 2.01f, 0.0f}, 2.9f, {1.9f, 2.9f, 1.9f}},
      {{-0.01f, 1.99f, 0.01f}, 2.8f, {1.8f, 2.8f, 1.8f}}}};
  if (!ValidateShowcaseAssetLodBounds3D(validLods))
    return 8;
  auto invalidLods = validLods;
  invalidLods[2].extents.z = 0.0f;
  if (ValidateShowcaseAssetLodBounds3D(invalidLods))
    return 9;
  invalidLods = validLods;
  invalidLods[1].center.x = 4.0f;
  if (ValidateShowcaseAssetLodBounds3D(invalidLods))
    return 10;

  const ShowcaseAssetBounds3D unionBounds{{-1.0f, -2.0f, -3.0f},
                                           {3.0f, 2.0f, 3.0f}};
  Aevrynd::Transform3D target{};
  target.position = {10.0f, 2.0f, 20.0f};
  target.scale = {4.0f, 8.0f, 6.0f};
  Aevrynd::Transform3D first{}, second{};
  if (!NormalizeShowcaseAssetTransform3D(unionBounds, target, first) ||
      !NormalizeShowcaseAssetTransform3D(unionBounds, target, second))
    return 1;
  // 三軸必須使用同一尺度，且聯集底面必須準確落在目標地面。
  if (!Close(first.scale.x, 1.0f) || !Close(first.scale.y, first.scale.x) ||
      !Close(first.scale.z, first.scale.x) ||
      !Close(first.position.x, 9.0f) || !Close(first.position.y, 4.0f) ||
      !Close(first.position.z, 20.0f) ||
      !Close(first.position.y + unionBounds.minimum.y * first.scale.y,
             target.position.y))
    return 2;
  // 多個子網格重複解析同一聯集時，必須得到完全相同的世界轉換。
  if (!Close(first.position.x, second.position.x) ||
      !Close(first.position.y, second.position.y) ||
      !Close(first.position.z, second.position.z) ||
      !Close(first.scale.x, second.scale.x))
    return 3;

  auto invalid = unionBounds;
  invalid.maximum.x = invalid.minimum.x;
  if (NormalizeShowcaseAssetTransform3D(invalid, target, first))
    return 4;
  invalid = unionBounds;
  invalid.maximum.y = std::numeric_limits<float>::infinity();
  if (NormalizeShowcaseAssetTransform3D(invalid, target, first))
    return 5;
  auto invalidTarget = target;
  invalidTarget.scale.z = 0.0f;
  if (NormalizeShowcaseAssetTransform3D(unionBounds, invalidTarget, first))
    return 6;
  invalidTarget = target;
  invalidTarget.position.x = std::numeric_limits<float>::quiet_NaN();
  return NormalizeShowcaseAssetTransform3D(unionBounds, invalidTarget, first)
             ? 7
             : 0;
}
