#include "RiverbankAvatarMotion.h"
#include "RiverbankAvatarAsset.h"
#include <Aevrynd/SkinnedRenderable3D.h>
#include <iostream>

using namespace Aevrynd;

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const auto asset = Mistwatch::LoadRiverbankAvatarAsset(argv[1]);
    const Mistwatch::RiverbankAvatarMotion motion(asset);
    const auto joint = std::ranges::find(asset.jointNames, "pelvis");
    if (joint == asset.jointNames.end()) return 3;
    const auto pelvis = static_cast<std::size_t>(joint - asset.jointNames.begin());
    const Vec3 feet{6.f, 2.15f, -12.f};
    AnimationPose3D scratch, pose;
    SkinningWorkspace3D workspace;
    Mesh3D skin;
    float largestDrift = 0.f, lowest = 100.f, highest = -100.f;
    for (const float heading : {0.f, 1.5707963f, -2.3561945f}) {
        Mistwatch::RiverbankAvatarMotion::Player actor{0.f, heading, 1.f};
        auto placement = motion.Evaluate(asset.model.skeleton, actor, feet, scratch, pose);
        const Vec3 seam = TransformPoint(placement * pose.globalMatrices[pelvis], {});
        for (int sample = 0; sample <= 64; ++sample) {
            actor.distance = motion.Stride() * float(sample) / 64.f;
            placement = motion.Evaluate(asset.model.skeleton, actor, feet, scratch, pose);
            const Vec3 at = TransformPoint(placement * pose.globalMatrices[pelvis], {});
            largestDrift = std::max(largestDrift, std::hypot(at.x - seam.x, at.z - seam.z));
            if (!SkinModelInto(asset.model, pose, workspace, skin)) return 4;
            float floor = 100.f;
            for (const auto& v : skin.vertices) floor = std::min(floor, TransformPoint(placement, {v.x, v.y, v.z}).y);
            lowest = std::min(lowest, floor - feet.y);
            highest = std::max(highest, floor - feet.y);
        }
    }

    Mistwatch::RiverbankAvatarMotion::Player state;
    for (int i = 0; i < 5; ++i) state.Advance({.1f, 0, 0}, .05f);
    if (std::abs(state.distance - .5f) > 1e-5f || state.walking != 1.f || state.heading < 1.56f) return 5;
    for (int i = 0; i < 5; ++i) state.Advance({}, .05f);
    if (state.distance != 0.f || state.walking != 0.f || state.heading < 1.56f) return 6;

    DrawList scene;
    const SkinnedMeshHandle3D mesh{1, asset.model.skeletonCompatibilityId};
    const auto materials = Mistwatch::RiverbankAvatarMaterials(asset);
    for (const Vec3 position : {feet, feet + Vec3{2.f, 0, 0}}) {
        const auto transform = motion.Evaluate(asset.model.skeleton, state, position, scratch, pose);
        const auto palette = AddSkinPaletteFromPose(scene, asset.model, pose, workspace);
        if (!SubmitSkinnedRenderable3D(scene, mesh, palette, transform,
            static_cast<std::uint32_t>(asset.model.bindMesh.indices.size()), asset.submeshes, materials)) return 8;
    }
    if (scene.skinnedMeshInstances.size() != 36 || scene.skinPaletteJoints.size() != 160) return 9;
    std::cout << "samples=195 pelvis_planar_drift_m=" << largestDrift
              << " lowest_animated_floor_m=" << lowest << " highest_animated_floor_m=" << highest
              << " two_player_draw_commands=" << scene.skinnedMeshInstances.size() << '\n';
    if (largestDrift > .035f || lowest < -.004f || highest > .005f) return 7;
    return 0;
}
