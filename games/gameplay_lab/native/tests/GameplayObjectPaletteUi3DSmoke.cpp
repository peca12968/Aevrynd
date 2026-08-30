#include "GameplayObjectPaletteUi3D.h"
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/PhysicsWorld3D.h>
#include <Aevrynd/SceneECS3D.h>
#include <array>
#include <iostream>

using namespace Aevrynd;
using namespace GameplayLab;

static std::uint64_t RunScenario() {
  Rules3D rules;
  CollisionWorld3D collision;
  PhysicsWorld3D physics;
  SceneECSRegistry3D scene;
  collision.CreateBox({{{0, -.5f, 0}, {20, .5f, 20}}});
  ObjectPaletteAdapter3D adapter(rules, collision, physics, scene);
  GameplayObjectPaletteUi3D ui(adapter);
  ui.SetLayout({10, 20, 180, 300}, 5);

  for (std::size_t index = 0; index < ui.Categories.size(); ++index) {
    const auto bounds = ui.ItemBounds(index);
    const auto hit = ui.HitTest(
        {bounds.x + bounds.width * .5f, bounds.y + bounds.height * .5f});
    if (!hit || *hit != ui.Categories[index])
      return 0;
  }
  if (ui.HitTest({0, 0}))
    return 0;

  const Ray3D first{{2, 10, 2}, {0, -1, 0}};
  const auto physicsRow = ui.ItemBounds(1);
  if (!ui.PointerDown({physicsRow.x + 2, physicsRow.y + 2}, first) ||
      !ui.PointerDrag(first, .25f) || !ui.PointerUp() || ui.Active() ||
      rules.Entities().size() != 1 || physics.Stats().dynamicBodies != 1)
    return 0;

  const auto stableEntities = rules.Entities().size();
  const auto stableBodies = physics.Stats().dynamicBodies;
  if (!ui.KeyboardBegin(PaletteCategory::Props,
                        {{2, 10, 2}, {0, -1, 0}}) ||
      ui.KeyboardCommit() || ui.Active() || adapter.Preview().active ||
      rules.Entities().size() != stableEntities ||
      physics.Stats().dynamicBodies != stableBodies)
    return 0;

  if (!ui.KeyboardBegin(PaletteCategory::Building,
                        {{5, 10, 5}, {0, -1, 0}}))
    return 0;
  ui.Cancel();
  if (ui.Active() || adapter.Preview().active ||
      rules.Entities().size() != stableEntities ||
      physics.Stats().dynamicBodies != stableBodies)
    return 0;

  const auto receipt = ui.Receipt();
  if (receipt.pointerDowns != 1 || receipt.drags != 1 ||
      receipt.previews != 1 || receipt.commits != 1 ||
      receipt.invalidPlacements != 1 || receipt.cancels != 1)
    return 0;
  adapter.Clear();
  return receipt.deterministicHash;
}

int main() {
  static_assert(GameplayObjectPaletteUi3D::Categories ==
                std::array{PaletteCategory::Primitives, PaletteCategory::Physics,
                           PaletteCategory::Props, PaletteCategory::Building,
                           PaletteCategory::Characters, PaletteCategory::Lights});
  static_assert(GameplayObjectPaletteUi3D::Labels ==
                std::array<std::string_view, 6>{
                    "Primitives", "Physics", "Props", "Building",
                    "Characters", "Lights"});
  const std::uint64_t first = RunScenario(), second = RunScenario();
  if (!first || first != second)
    return 1;
  std::cout << "categories=6 hitTest=1 commit=1 invalidAtomic=1 cancelAtomic=1 "
               "receiptHash="
            << first << '\n';
  return 0;
}
