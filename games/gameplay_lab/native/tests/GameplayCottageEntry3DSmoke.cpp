#include "GameplayCottageEntry3D.h"

#include <Aevrynd/CharacterController3D.h>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
BoxCollider3D AxisAligned(const EnvironmentColliderDescriptor3D &value) {
  const float sine = std::abs(std::sin(value.yaw));
  const float cosine = std::abs(std::cos(value.yaw));
  return {value.center,
          {cosine * value.extents.x + sine * value.extents.z,
           value.extents.y,
           sine * value.extents.x + cosine * value.extents.z}};
}
} // namespace

int main() {
  const EnvironmentColliderDescriptor3D front{
      {0xabcdu, 100u}, EnvironmentColliderSemantic3D::Building,
      {-12.f, 1.95f, 20.4f}, {4.2f, 1.65f, .14f}, 0.f,
      1u, CollisionLayerAll3D, 0x100u};
  std::array<EnvironmentColliderDescriptor3D,
             GameplayCottageFrontCollisionPieceCount3D> pieces{};
  if (BuildGameplayCottageCollisionPieces3D(
          front, "procedural/cottage-front", 200u, pieces) != 3u)
    return 1;
  if (GameplayCottageDoorClearWidth3D < .90f + .12f ||
      GameplayCottageDoorClearHeight3D < 1.80f + .10f ||
      pieces[0].id == pieces[1].id || pieces[1].id == pieces[2].id ||
      pieces[0].id == pieces[2].id)
    return 2;

  auto invalid = front;
  invalid.center.x = std::numeric_limits<float>::quiet_NaN();
  pieces.fill(front);
  if (BuildGameplayCottageCollisionPieces3D(
          invalid, "procedural/cottage-front", 200u, pieces) != 0u)
    return 3;
  for (const auto &piece : pieces)
    if (piece.id || piece.center.x != 0.f || piece.extents.x != .5f)
      return 4;

  std::array<BoxCollider3D, 4> entrance{};
  if (BuildGameplayCottageCollisionPieces3D(
          front, "procedural/cottage-front", 200u, pieces) != 3u)
    return 5;
  for (std::size_t index = 0; index < pieces.size(); ++index)
    entrance[index] = AxisAligned(pieces[index]);
  const EnvironmentColliderDescriptor3D foundation{
      {0xabcdu, 99u}, EnvironmentColliderSemantic3D::Building,
      {-12.f, .15f, 24.f}, {4.2f, .15f, 3.8f}, 0.f,
      1u, CollisionLayerAll3D, 0x99u};
  std::array<EnvironmentColliderDescriptor3D, 3> foundationPieces{};
  if (BuildGameplayCottageCollisionPieces3D(
          foundation, "procedural/cottage-foundation", 202u,
          foundationPieces) != 1u ||
      foundationPieces[0].center.y + foundationPieces[0].extents.y >= 0.f)
    return 6;
  entrance[3] = AxisAligned(foundationPieces[0]);

  CharacterController3D walker;
  walker.Reset({-12.f, 0.f, 18.6f});
  CharacterDirectDisplacementResult3D passage{};
  if (!walker.ApplyDirectDisplacementTick({1u, {0.f, 0.f, 3.2f}, 0.f},
                                           entrance, &passage) ||
      std::abs(passage.remainderWorld.x) > .00001f ||
      std::abs(passage.remainderWorld.y) > .00001f ||
      std::abs(passage.remainderWorld.z) > .00001f ||
      walker.Position().z < 21.79f) {
    std::cerr << "passage status=" << unsigned(passage.status)
              << " position=" << walker.Position().x << ','
              << walker.Position().y << ',' << walker.Position().z
              << " remainder=" << passage.remainderWorld.x << ','
              << passage.remainderWorld.y << ',' << passage.remainderWorld.z
              << '\n';
    return 7;
  }

  const EnvironmentColliderDescriptor3D side{
      {0xabcdu, 101u}, EnvironmentColliderSemantic3D::Building,
      {-15.95f, 1.95f, 24.f}, {3.6f, 1.65f, .14f}, 1.5707963f,
      1u, CollisionLayerAll3D, 0x101u};
  std::array<EnvironmentColliderDescriptor3D, 3> sidePieces{};
  if (BuildGameplayCottageCollisionPieces3D(
          side, "procedural/cottage-left", 202u, sidePieces) != 1u)
    return 8;
  const std::array<BoxCollider3D, 1> sideWall{AxisAligned(sidePieces[0])};
  CharacterController3D wallProbe;
  wallProbe.Reset({-14.f, 0.f, 24.f});
  CharacterDirectDisplacementResult3D blocked{};
  if (!wallProbe.ApplyDirectDisplacementTick({1u, {-3.f, 0.f, 0.f}, 0.f},
                                              sideWall, &blocked) ||
      blocked.status != CharacterDirectDisplacementStatus3D::Blocked ||
      wallProbe.Position().x < -15.38f)
    return 9;

  std::cout << "GAMEPLAY_COTTAGE_ENTRY=PASS width="
            << GameplayCottageDoorClearWidth3D << " height="
            << GameplayCottageDoorClearHeight3D << " insideZ="
            << walker.Position().z << '\n';
  return 0;
}
