#include "RiverbankCombatCamera.h"
#include "RiverbankDownedCamera.h"
#include <iostream>

int main() {
    using namespace Aevrynd;
    HeightField3D terrain({{-20, 0, -20}, 41, 41, 1.f});
    terrain.Fill(0.f);
    const Vec3 feet{0, 0, 0};
    const auto base = MakeThirdPersonCamera({0, 1.2f, 0}, 0.f, .3f, 6.f);
    const Vec3 threat{0, 0, 1.5f};
    Mistwatch::RiverbankCombatCamera camera;
    auto view = camera.Update(base, feet, Vec3{0, 0, 8}, 1.f / 60.f, {}, terrain);
    if (Length(view.position - base.position) > .001f) return 1;

    float largestStep = 0.f;
    for (int tick = 0; tick < 24; ++tick) {
        const auto next = camera.Update(base, feet, threat, 1.f / 60.f, {}, terrain);
        largestStep = std::max(largestStep, Length(next.position - view.position));
        view = next;
    }
    const auto before = ProjectToViewport(threat + Vec3{0, .55f, 0}, base, {1280, 720});
    const auto after = ProjectToViewport(threat + Vec3{0, .55f, 0}, view, {1280, 720});
    const auto player = ProjectToViewport(feet + Vec3{0, .55f, 0}, view, {1280, 720});
    const Vec3 aimChange = (view.target - view.position) - (base.target - base.position);
    if (largestStep > .18f || view.position.x < 2.f || view.position.y < base.position.y + .9f ||
        std::hypot(aimChange.x, aimChange.z) > 1e-4f || aimChange.y >= -.5f ||
        !before.visible || !after.visible || !player.visible || std::abs(after.position.x - player.position.x) < 45.f) {
        std::cerr << "Camera motion step=" << largestStep << " horizontal separation="
                  << std::abs(after.position.x - player.position.x) << " pitch change=" << aimChange.y << '\n';
        return 2;
    }

    const auto stableSide = camera.Update(base, feet, Vec3{-.25f, 0, 1.5f}, 1.f / 60.f, {}, terrain);
    if (stableSide.position.x <= 0.f) return 3;
    for (int tick = 0; tick < 35; ++tick)
        view = camera.Update(base, feet, std::nullopt, 1.f / 60.f, {}, terrain);
    if (Length(view.position - base.position) > .001f) return 4;

    const std::array wall{BoxCollider3D{{.42f, base.position.y + .5f, base.position.z}, {.12f, 2.f, 1.f}}};
    for (int tick = 0; tick < 24; ++tick)
        view = camera.Update(base, feet, threat, 1.f / 60.f, {}, terrain, wall);
    if (Length(view.position - base.position) > .001f) return 5;
    const std::array vegetation{BoxCollider3D{{1.6f, base.position.y, base.position.z + 1.f}, {.65f, 2.f, .55f}}};
    Mistwatch::RiverbankCombatCamera foliageRoute;
    for (int tick = 0; tick < 32; ++tick)
        view = foliageRoute.Update(base, feet, threat, 1.f / 60.f, {}, terrain, {}, vegetation);
    if (view.position.x > -2.f ||
        !ThirdPersonCameraPathClear3D(base.position, view.position, {}, terrain, {}, .155f)) return 7;
    const auto behind = Mistwatch::RiverbankCombatCamera{}.Update(base, feet, Vec3{0, 0, -1.5f}, 1.f / 60.f, {}, terrain);
    if (Length(behind.position - base.position) > .001f) return 6;
    const std::array body{Vec3{.68f, .36f, .1f}, Vec3{.22f, .31f, .04f}};
    const BoxCollider3D animal{{.14f, .66f, -1.1f}, {.60f, .65f, .64f}};
    const auto concealed = [&](Vec3 eye, Vec3 point) {
        const Vec3 delta = point - eye;
        return RayIntersectsAabb({eye, Normalize(delta)}, animal.center, animal.extents, Length(delta) - .18f);
    };
    if (!concealed(base.position, body[0]) || !concealed(base.position, body[1])) return 8;
    Mistwatch::RiverbankDownedCamera rescue;
    float downedStep = 0.f;
    view = base;
    for (int tick = 0; tick < 42; ++tick) {
        const auto next = rescue.Update(base, body, animal, 1.f / 60.f, {}, terrain, {}, {});
        downedStep = std::max(downedStep, Length(next.position - view.position));
        view = next;
    }
    if (downedStep > .201f || concealed(view.position, body[0]) || concealed(view.position, body[1]) ||
        !ProjectToViewport(body[0], view, {1280, 720}).visible ||
        !ProjectToViewport(body[1], view, {1280, 720}).visible) {
        std::cerr << "Downed view still concealed by nearby animal; eye step " << downedStep << " position " << view.position.x << "," << view.position.y << "," << view.position.z << " blocked " << concealed(view.position, body[0]) << "," << concealed(view.position, body[1]) << " visible " << ProjectToViewport(body[0], view, {1280,720}).visible << "," << ProjectToViewport(body[1], view, {1280,720}).visible << '\n';
        return 9;
    }
    for (int tick = 0; tick < 42; ++tick) {
        const auto next = rescue.Update(base, {}, {}, 1.f / 60.f, {}, terrain, {}, {});
        if (Length(next.position - view.position) > .201f) return 10;
        view = next;
    }
    if (rescue.Moving() || Length(view.position - base.position) > .001f ||
        Length(view.target - base.target) > .001f) return 11;
    const std::array ceiling{BoxCollider3D{base.position + Vec3{0, .70f, 0}, {8.f, .10f, 8.f}}};
    Mistwatch::RiverbankDownedCamera underCover;
    for (int tick = 0; tick < 42; ++tick)
        view = underCover.Update(base, body, animal, 1.f / 60.f, {}, terrain, ceiling, {});
    if (Length(view.position - base.position) > .001f ||
        !ThirdPersonCameraPathClear3D(base.position, view.position, {}, terrain, ceiling, .155f)) return 12;
    std::cout << "Downed camera clears a near animal; maximum step " << downedStep << " m\n";
    std::cout << "Riverbank close combat camera verified; maximum step " << largestStep
              << " m, enemy horizontal separation " << std::abs(after.position.x - player.position.x) << " px\n";
}
