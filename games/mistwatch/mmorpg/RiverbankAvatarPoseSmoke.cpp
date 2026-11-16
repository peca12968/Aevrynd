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

    if (motion.HasDownedClip()) {
        auto fallen = state;
        const auto head = std::ranges::find(asset.jointNames, "head");
        if (head == asset.jointNames.end()) return 15;
        const auto headIndex = static_cast<std::size_t>(head - asset.jointNames.begin());
        float gap = 0.f, signedWorstGap = 0.f;
        int worstFrame = 0;
        for (int frame = 0; frame <= 24; ++frame) {
            if (frame != 0) fallen.Advance({}, 1.f / 24.f, true);
            const auto placement = motion.Evaluate(asset.model.skeleton, fallen, feet, scratch, pose);
            if (!SkinModelInto(asset.model, pose, workspace, skin)) return 16;
            float floor = 100.f;
            for (const auto& v : skin.vertices) floor = std::min(floor, TransformPoint(placement, {v.x, v.y, v.z}).y);
            if (std::abs(floor - feet.y) > gap) {
                signedWorstGap = floor - feet.y;
                gap = std::abs(signedWorstGap);
                worstFrame = frame;
            }
            if (!std::isfinite(floor)) return 17;
        }
        fallen.Advance({}, .001f, true);
        const auto placement = motion.Evaluate(asset.model.skeleton, fallen, feet, scratch, pose);
        const Vec3 at = TransformPoint(placement * pose.globalMatrices[headIndex], {});
        const Vec3 hips = TransformPoint(placement * pose.globalMatrices[pelvis], {});
        const float headHeight = at.y - feet.y;
        const float horizontalReach = std::hypot(at.x - hips.x, at.z - hips.z);
        std::cout << "downed_ground_gap_m=" << gap << " signed_gap_m=" << signedWorstGap << " worst_frame=" << worstFrame << " downed_head_height_m=" << headHeight
                  << " downed_head_reach_m=" << horizontalReach << '\n';
        if (gap > .03f || headHeight < .05f || headHeight > .37f || horizontalReach < .5f) return 18;
        float slopeGap = 0.f;
        for (const auto normal : {Normalize(Vec3{.26f, 1.f, 0}), Normalize(Vec3{-.46f, 1.f, .35f}),
                                  Normalize(Vec3{0, 1.f, -.57f})}) {
            for (const float heading : {0.f, 1.57f, -2.36f}) {
                fallen.heading = heading;
                const auto onSlope = motion.Evaluate(asset.model.skeleton, fallen, feet, scratch, pose, normal);
                if (!SkinModelInto(asset.model, pose, workspace, skin)) return 20;
                float supportGap = 100.f;
                for (const auto& v : skin.vertices)
                    supportGap = std::min(supportGap, Dot(TransformPoint(onSlope, {v.x, v.y, v.z}) - feet, normal));
                if (!std::isfinite(supportGap)) return 21;
                slopeGap = std::max(slopeGap, std::abs(supportGap));
            }
        }
        std::cout << "downed_slope_cases=9 max_support_gap_m=" << slopeGap << '\n';
        if (slopeGap > .01f) return 22;
        for (int frame = 0; frame < 10; ++frame) fallen.Advance({}, .05f);
        if (fallen.downedSeconds != 0.f) return 19;
    }

    DrawList scene;
    const SkinnedMeshHandle3D mesh{1, asset.model.skeletonCompatibilityId};
    using Mistwatch::RiverbankClass;
    const auto base = Mistwatch::RiverbankAvatarMaterials(asset);
    const std::array roles{RiverbankClass::Guardian, RiverbankClass::Ranger, RiverbankClass::Priest};
    const std::array vests{Color{.30f, .073f, .044f, 1.f}, Color{.091f, .20f, .085f, 1.f}, Color{.56f, .46f, .29f, 1.f}};
    const auto vest = std::ranges::find(asset.materialSlotNames, "AX textile | Warm woven vest");
    if (vest == asset.materialSlotNames.end()) return 10;
    const auto vestIndex = static_cast<std::size_t>(vest - asset.materialSlotNames.begin());
    for (std::size_t actor = 0; actor < roles.size(); ++actor) {
        const auto playerClass = Mistwatch::ReadRiverbankClassFlags(Mistwatch::RiverbankClassFlags(roles[actor]) | 1u);
        const auto materials = Mistwatch::RiverbankAvatarMaterials(asset, playerClass);
        if (PackColorRgba8(materials[vestIndex].tint) != PackColorRgba8(vests[actor])) return 11;
        const Vec3 position = feet + Vec3{2.f * static_cast<float>(actor), 0, 0};
        const auto transform = motion.Evaluate(asset.model.skeleton, state, position, scratch, pose);
        const auto palette = AddSkinPaletteFromPose(scene, asset.model, pose, workspace);
        if (!SubmitSkinnedRenderable3D(scene, mesh, palette, transform,
            static_cast<std::uint32_t>(asset.model.bindMesh.indices.size()), asset.submeshes, materials)) return 8;
        bool submittedVest = false;
        std::size_t changedFabricParts = 0;
        for (std::size_t part = 0; part < asset.submeshes.size(); ++part) {
            const auto slot = asset.submeshes[part].materialSlot;
            const auto& drawPart = scene.skinnedMeshInstances.at(actor * asset.submeshes.size() + part);
            const auto before = PackColorRgba8(base[slot].tint);
            const auto after = PackColorRgba8(materials[slot].tint);
            if (drawPart.tint != after || drawPart.shadingModel != base[slot].shadingModel) return 12;
            if (slot == vestIndex) submittedVest = true;
            if (before != after) {
                if (base[slot].shadingModel != CharacterShadingModel3D::Cloth) return 13;
                ++changedFabricParts;
            }
        }
        if (!submittedVest || changedFabricParts != 3) return 14;
    }
    if (scene.skinnedMeshInstances.size() != (Mistwatch::HasRiverbankBoots(asset) ? 66u : 54u) || scene.skinPaletteJoints.size() != 240) return 9;
    std::cout << "samples=195 pelvis_planar_drift_m=" << largestDrift
              << " lowest_animated_floor_m=" << lowest << " highest_animated_floor_m=" << highest
              << " three_role_draw_commands=" << scene.skinnedMeshInstances.size() << '\n';
    if (largestDrift > .035f || lowest < -.004f || highest > .005f) return 7;
    return 0;
}
