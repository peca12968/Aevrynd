#include "RiverbankAvatarMotion.h"
#include "RiverbankAvatarAsset.h"
#include "MistwatchWorld.h"
#include "RiverbankPeerPresentation.h"
#include <Aevrynd/SkinnedRenderable3D.h>
#include <iostream>

using namespace Aevrynd;

namespace {
bool VerifyRiverbankTerrainContact(const Mistwatch::RiverbankAvatarMotion& motion,
    const SkinnedModelAssetData& asset, const std::array<std::vector<std::uint32_t>, 2>& boots) {
    const auto terrain = Mistwatch::CreateTerrain();
    const Mistwatch::RiverbankWalkSurface surface{terrain};
    AnimationPose3D scratch, pose;
    SkinningWorkspace3D skinning;
    Mesh3D skinned;
    constexpr float frame = 1.f / 60.f, heading = 1.04719755f;
    const Vec3 direction{std::sin(heading), 0, std::cos(heading)};
    unsigned samples = 0, flightSamples = 0;
    float deepest = 0.f, steepest = 0.f;
    for (const float phase : {0.f, .25f, .5f, .75f}) {
        Mistwatch::RiverbankAvatarMotion::Player actor{motion.Stride() * phase, heading, 1.f, 0.f, 1.f};
        for (unsigned tick = 0; tick <= 242; ++tick) {
            auto feet = direction * (3.2f * frame * tick);
            feet.y = terrain.SampleHeight(feet.x, feet.z);
            const auto normal = terrain.SampleNormal(feet.x, feet.z);
            steepest = (std::max)(steepest, std::hypot(normal.x, normal.z) / normal.y);
            const auto placement = motion.Evaluate(asset.model.skeleton, actor, feet, scratch, pose, normal, &surface);
            if (!SkinModelInto(asset.model, pose, skinning, skinned)) return false;
            float minimum = std::numeric_limits<float>::infinity();
            for (const auto& boot : boots) {
                for (const auto index : boot) {
                    const auto& vertex = skinned.vertices[index];
                    const auto point = TransformPoint(placement, {vertex.x, vertex.y, vertex.z});
                    if (!terrain.Contains(point.x, point.z)) return false;
                    minimum = (std::min)(minimum, point.y - terrain.SampleHeight(point.x, point.z));
                }
            }
            if (!std::isfinite(minimum)) return false;
            deepest = (std::min)(deepest, minimum);
            flightSamples += minimum > .012f;
            ++samples;
            actor.AdvanceByTravel(direction * (3.2f * frame), frame, false, true);
        }
    }
    std::cout << "riverbank_route_samples=" << samples << " airborne_samples=" << flightSamples
              << " deepest_m=" << deepest << " maximum_grade=" << steepest << '\n';
    return samples == 972 && flightSamples > 0 && deepest >= -.006f && steepest >= .15f;
}

bool VerifyBridgeDeckContact(const Mistwatch::RiverbankAvatarMotion& motion,
    const SkinnedModelAssetData& asset, const std::array<std::vector<std::uint32_t>, 2>& boots) {
    const auto terrain = Mistwatch::CreateTerrain();
    const Mistwatch::RiverbankWalkSurface open{terrain, true};
    const Mistwatch::RiverbankWalkSurface closed{terrain, false};
    const auto& deck = Mistwatch::RiverbankBridgeDeck;
    const auto south = deck.center.z - deck.extents.z;
    const auto north = deck.center.z + deck.extents.z;
    const auto top = deck.center.y + deck.extents.y;
    if (closed.SampleHeight(0, deck.center.z) > top - 7.f ||
        open.SampleHeight(deck.extents.x + .01f, deck.center.z) != closed.SampleHeight(deck.extents.x + .01f, deck.center.z) ||
        open.SampleHeight(0, south - .01f) != closed.SampleHeight(0, south - .01f) ||
        open.SampleHeight(0, north + .01f) != closed.SampleHeight(0, north + .01f)) return false;

    AnimationPose3D scratch, pose;
    SkinningWorkspace3D skinning;
    Mesh3D mesh;
    float deepest = 0.f;
    unsigned samples = 0;
    for (float heading : {0.f, 3.14159265f}) {
        for (float x : {-.75f, 0.f, .75f}) {
            for (unsigned step = 0; step <= 32; ++step) {
                const auto z = south + (north - south) * float(step) / 32.f;
                const auto height = open.SampleHeight(x, z);
                const auto normal = open.SampleNormal(x, z);
                if (height != top || normal.x != 0.f || normal.y != 1.f || normal.z != 0.f) return false;
                if (step == 0 || step == 32) continue;
                const Mistwatch::RiverbankAvatarMotion::Player actor{float(step) * .25f, heading, 1.f, 0.f, 1.f};
                const auto transform = motion.Evaluate(asset.model.skeleton, actor, {x, height, z},
                    scratch, pose, normal, &open);
                if (!SkinModelInto(asset.model, pose, skinning, mesh)) return false;
                for (const auto& boot : boots) for (const auto index : boot) {
                    const auto& vertex = mesh.vertices[index];
                    const auto point = TransformPoint(transform, {vertex.x, vertex.y, vertex.z});
                    deepest = (std::min)(deepest, point.y - open.SampleHeight(point.x, point.z));
                }
                ++samples;
            }
        }
    }
    std::cout << "bridge_deck_run_samples=" << samples << " deepest_m=" << deepest << '\n';
    return samples == 186 && deepest >= -.006f;
}

bool VerifyCrouchHeadroom(const Mistwatch::RiverbankAvatarMotion& motion,
    const SkinnedModelAssetData& asset, const std::array<std::vector<std::uint32_t>, 2>& boots) {
    const auto terrain = Mistwatch::CreateTerrain();
    const Mistwatch::RiverbankWalkSurface surface{terrain};
    const auto roof = Mistwatch::RepairShelterSolids().front();
    const auto roofY = roof.center.y - roof.extents.y;
    const Vec3 feet{-6.8f, terrain.SampleHeight(-6.8f, 17.18f), 17.18f};
    const auto normal = surface.SampleNormal(feet.x, feet.z);
    AnimationPose3D scratch, pose;
    SkinningWorkspace3D skinning;
    Mesh3D skinned;
    float maximum = -std::numeric_limits<float>::infinity();
    float sole = std::numeric_limits<float>::infinity();
    const auto measure = [&](const Mistwatch::RiverbankAvatarMotion::Player& actor) {
        const auto placement = motion.Evaluate(asset.model.skeleton, actor, feet, scratch, pose, normal, &surface);
        if (!SkinModelInto(asset.model, pose, skinning, skinned)) return false;
        maximum = -std::numeric_limits<float>::infinity();
        sole = std::numeric_limits<float>::infinity();
        for (const auto& vertex : skinned.vertices) {
            const auto point = TransformPoint(placement, {vertex.x, vertex.y, vertex.z});
            if (!std::isfinite(point.y)) return false;
            maximum = std::max(maximum, point.y);
        }
        for (const auto& boot : boots) {
            for (const auto index : boot) {
                const auto& vertex = skinned.vertices[index];
                const auto point = TransformPoint(placement, {vertex.x, vertex.y, vertex.z});
                if (!terrain.Contains(point.x, point.z)) return false;
                sole = std::min(sole, point.y - terrain.SampleHeight(point.x, point.z));
            }
        }
        return std::isfinite(sole) && std::isfinite(maximum);
    };

    Mistwatch::RiverbankAvatarMotion::Player actor;
    if (!measure(actor) || maximum <= roofY) return false;
    // 第一個蹲姿快照即使還沒有累計時間，也能直接對齊權威姿勢。
    actor.ObserveStance(true, false, 0.f);
    if (!measure(actor) || maximum > roofY - .005f || std::abs(sole) > .008f) return false;
    const float firstTop = maximum;
    constexpr float dt = 1.f / 60.f;
    float walkingTop = maximum, walkingSole = sole;
    for (unsigned frame = 0; frame < 120; ++frame) {
        actor.AdvanceByTravel({-.9f * dt, 0, 0}, dt, false, true);
        if (!measure(actor)) return false;
        walkingTop = std::max(walkingTop, maximum);
        walkingSole = std::min(walkingSole, sole);
    }
    std::cout << "crouch_first_headroom_m=" << roofY - firstTop
              << " crouch_walk_headroom_m=" << roofY - walkingTop
              << " crouch_lowest_sole_m=" << walkingSole << '\n';
    return walkingTop <= roofY - .005f && walkingSole >= -.008f;
}

bool VerifyRunSlopeContact(const Mistwatch::RiverbankAvatarMotion& motion,
    const SkinnedModelAssetData& asset) {
    const auto findJoint = [&](std::string_view name) {
        return static_cast<std::size_t>(std::ranges::find(asset.jointNames, name) - asset.jointNames.begin());
    };
    const std::array legs{
        TwoBoneIkChain3D{findJoint("thigh.L"), findJoint("shin.L"), findJoint("foot.L")},
        TwoBoneIkChain3D{findJoint("thigh.R"), findJoint("shin.R"), findJoint("foot.R")},
    };
    for (const auto& leg : legs) if (leg.end >= asset.jointNames.size()) return false;
    const auto sole = std::ranges::find(asset.materialSlotNames, Mistwatch::RiverbankBootSole);
    const auto slot = static_cast<std::size_t>(sole - asset.materialSlotNames.begin());
    std::array<std::vector<std::uint32_t>, 2> boots;
    for (const auto& surface : asset.submeshes) {
        if (surface.materialSlot != slot) continue;
        for (const auto v : std::span(asset.model.bindMesh.indices).subspan(surface.firstIndex, surface.indexCount))
            boots[asset.model.bindMesh.vertices[v].x > 0.f].push_back(v);
    }
    for (auto& vertices : boots) {
        std::ranges::sort(vertices);
        vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
        if (vertices.empty()) return false;
    }

    AnimationPose3D work, flatPose, slopePose;
    SkinningWorkspace3D skinning;
    Mesh3D skinned;
    const auto& skeleton = asset.model.skeleton;
    const auto feet = Vec3{1.8f, 2.2f, -4.5f};
    const auto gaps = [&](const AnimationPose3D& pose, Matrix4 placement, Vec3 normal) {
        std::array<float, 2> result;
        result.fill(std::numeric_limits<float>::infinity());
        if (!SkinModelInto(asset.model, pose, skinning, skinned)) return result;
        for (std::size_t side = 0; side < boots.size(); ++side)
            for (const auto vertex : boots[side]) {
                const auto& p = skinned.vertices[vertex];
                const auto delta = TransformPoint(placement, {p.x,p.y,p.z}) - feet;
                result[side] = (std::min)(result[side], Dot(delta, normal) / normal.y);
            }
        return result;
    };
    int samples = 0, airborneSamples = 0;
    float deepest = 0.f, clearanceError = 0.f, orientationError = 0.f, legError = 0.f, maxAnkleChange = 0.f;
    const std::array normals{Normalize(Vec3{0, 1, -.17f}), Normalize(Vec3{0, 1, .36f}), Normalize(Vec3{.36f, 1, 0})};
    const auto modelPosition = [](const AnimationPose3D& pose, std::size_t joint) {
        return TransformPoint(pose.globalMatrices[joint], {});
    };
    for (const float heading : {0.f, 1.570796327f, -2.35619449f})
        for (const float amount : {.5f, 1.f})
            for (const int phase : {0, 7, 15, 23, 29, 42, 53, 61, 68}) {
                const Mistwatch::RiverbankAvatarMotion::Player player{motion.Stride()*phase/68.f, heading, 1.f, 0.f, amount};
                const auto flat = motion.Evaluate(skeleton, player, feet, work, flatPose);
                const auto expected = gaps(flatPose, flat, {0, 1, 0});
                for (const auto normal : normals) {
                    const auto slope = motion.Evaluate(skeleton, player, feet, work, slopePose, normal);
                    const auto actual = gaps(slopePose, slope, normal);
                    ++samples;
                    if (amount == 1.f && (std::min)(expected[0],expected[1]) > .02f) {
                        if ((std::min)(actual[0],actual[1]) <= .012f) return false;
                        ++airborneSamples;
                    }
                    for (std::size_t side = 0; side < boots.size(); ++side) {
                        if (!std::isfinite(actual[side]) || !std::isfinite(expected[side])) return false;
                        deepest = (std::min)(deepest, actual[side]);
                        clearanceError = (std::max)(clearanceError, std::abs(expected[side] - actual[side]));
                        const auto leg = legs[side];
                        const auto upper = modelPosition(slopePose, leg.upper);
                        const auto middle = modelPosition(slopePose, leg.middle);
                        const auto foot = modelPosition(slopePose, leg.end);
                        const auto flatUpper = modelPosition(flatPose, leg.upper);
                        const auto flatMiddle = modelPosition(flatPose, leg.middle);
                        const auto flatFoot = modelPosition(flatPose, leg.end);
                        maxAnkleChange = (std::max)(maxAnkleChange, std::abs(foot.y - flatFoot.y));
                        legError = (std::max)({legError, std::abs(Length(upper-middle)-Length(flatUpper-flatMiddle)),
                                              std::abs(Length(middle-foot)-Length(flatMiddle-flatFoot))});
                        for (int row = 0; row < 3; ++row)
                            for (int column = 0; column < 3; ++column)
                                orientationError = (std::max)(orientationError, std::abs(slopePose.globalMatrices[leg.end][row][column]
                                    - flatPose.globalMatrices[leg.end][row][column]));
                    }
                }
            }
    std::cout << "slope_cases=" << samples << " airborne_preserved=" << airborneSamples
              << " deepest_m=" << deepest << " max_clearance_change_m=" << clearanceError
              << " max_ankle_adjustment_m=" << maxAnkleChange << " segment_length_error_m=" << legError
              << " ankle_orientation_error=" << orientationError << '\n';
    return VerifyCrouchHeadroom(motion, asset, boots) &&
        VerifyRiverbankTerrainContact(motion, asset, boots) && VerifyBridgeDeckContact(motion, asset, boots) &&
        samples == 162 && airborneSamples > 0 && deepest >= -.006f && clearanceError < .015f &&
        maxAnkleChange > .04f && legError < .0002f && orientationError < .0002f;
}

bool VerifyRunPlayback(const Mistwatch::RiverbankAvatarMotion& motion,
    const SkinnedModelAssetData& asset) {
    constexpr float frame = 1.f / 60.f;
    Mistwatch::RiverbankAvatarMotion::Player actor;
    AnimationPose3D work, pose;
    SkinningWorkspace3D skinning;
    Mesh3D skinned;
    float lowest = 100.f;
    int transitions = 0;
    for (int sample = 0; sample < 8; ++sample) {
        actor.ResetGait();
        actor.walking = 1.f;
        actor.distance = static_cast<float>(sample) * motion.Stride() / 8.f;
        for (int direction : {1, -1}) {
            const float speed = direction > 0 ? .16f : .1f;
            float last = actor.runWeight;
            for (int index = 0; index < 12; ++index) {
                const float before = actor.distance;
                actor.Advance({0, 0, speed}, frame, false, true);
                if ((actor.runWeight - last) * static_cast<float>(direction) < -1.e-5f ||
                    std::abs(actor.runWeight - last) > frame * 7.f + 1.e-5f || actor.distance <= before)
                    return false;
                if (actor.runWeight > 0.f && actor.runWeight < 1.f) {
                    ++transitions;
                    const auto world = motion.Evaluate(asset.model.skeleton, actor, {}, work, pose);
                    if (!SkinModelInto(asset.model, pose, skinning, skinned)) return false;
                    for (const auto& vertex : skinned.vertices)
                        lowest = (std::min)(lowest, TransformPoint(world, {vertex.x, vertex.y, vertex.z}).y);
                }
                last = actor.runWeight;
            }
            if (std::abs(actor.runWeight - (direction > 0 ? 1.f : 0.f)) > 1.e-5f) return false;
        }
    }

    Mistwatch::RiverbankPeerPresentation peer;
    if (!peer.Observe(10, {}, false, true) || peer.Observe(13, {0, 0, .48f}, false, true)) return false;
    for (int i = 0; i < 9; ++i) {
        const auto step = peer.Advance(frame);
        if (std::abs(step.z - .48f / 9.f) > 1.e-5f) return false;
    }
    if (std::abs(peer.Position()->z - .48f) > 1.e-5f || !peer.Observe(14, {0, 0, 2.f}, false, true)) return false;

    std::cout << "transition_samples=" << transitions << " min_blended_floor_m=" << lowest
              << " missed_sprint_snapshots=2 recovered_peer_ticks=3\n";
    return transitions == 128 && std::isfinite(lowest) && lowest >= -.0015f;
}

bool VerifyPeerDodgePresentation() {
    Mistwatch::RiverbankPeerPresentation peer;
    if (!peer.Observe(7,{},false,true) ||
        peer.Observe(10,{.530f,0,.530f},false,true,true)) return false;
    if (std::abs(peer.Advance(.05f).x-.530f/3.f)>1.e-5f ||
        std::abs(peer.Advance(.1f).x-.530f*2.f/3.f)>1.e-5f) return false;
    if (peer.Observe(12,{.82f,0,.82f},false,true) ||
        std::abs(peer.Advance(.1f).x-.29f)>1.e-5f) return false;
    if (!peer.Observe(14,{1.21f,0,1.21f},false,true) ||
        !peer.Observe(18,{1.51f,0,1.51f},false,true,true) ||
        !peer.Observe(19,{41.f,0,41.f},false,true,true)) return false;
    std::cout << "peer_dodge_interpolation=1 exit_interpolation=1 teleport_snap=1\n";
    return true;
}

bool VerifyPeerPresentation() {
    using Mistwatch::RiverbankPeerPresentation;
    RiverbankPeerPresentation peer;
    constexpr float frame = 1.f / 60.f;
    if (peer.Position() || !peer.Observe(100, {}, false)) return false;
    Mistwatch::RiverbankAvatarMotion::Player walk;
    int frozenFrames = 0, originalFrozenFrames = 0;
    float largestStep = 0.f, originalLargestStep = 0.f;
    float previousRaw = 0.f;
    for (std::uint64_t tick = 101; tick <= 110; ++tick) {
        const float canonicalX = static_cast<float>(tick - 100) * .1f;
        if (peer.Observe(tick, {canonicalX, 0, 0}, false)) return false;
        for (int renderFrame = 0; renderFrame < 3; ++renderFrame) {
            const auto step = peer.Advance(frame);
            const float rawStep = canonicalX - previousRaw;
            previousRaw = canonicalX;
            frozenFrames += step.x < .0001f;
            originalFrozenFrames += rawStep < .0001f;
            largestStep = std::max(largestStep, step.x);
            originalLargestStep = std::max(originalLargestStep, rawStep);
            if (step.x < .032f || step.x > .035f || peer.Position()->x > canonicalX + .00001f) return false;
            walk.AdvanceByTravel(step, frame, false);
        }
        if (std::abs(peer.Position()->x - canonicalX) > .00001f) return false;
    }
    if (std::abs(walk.distance - 1.f) > .00001f || walk.heading < 1.56f || walk.walking != 1.f) return false;
    std::cout << "peer_render_frames=30 raw_frozen_frames=" << originalFrozenFrames << " frozen_frames=" << frozenFrames
              << " raw_max_step_m=" << originalLargestStep << " max_step_m=" << largestStep << '\n';

    // 遺失兩刻時以三刻的時間到達已知位置。舊資料及重複資料不可覆蓋。
    if (peer.Observe(113, {1.3f, 0, 0}, false) || peer.Observe(112, {50.f, 0, 0}, false) ||
        peer.Observe(113, {50.f, 0, 0}, false)) return false;
    for (int f = 0; f < 9; ++f) {
        if (std::abs(peer.Advance(frame).x - .3f / 9.f) > .00001f) return false;
    }
    if (std::abs(peer.Advance(.5f).x) > .00001f || peer.Position()->x != 1.3f || peer.Advance(.1f).x != 0.f) return false;

    // 倒地、復起、傳送及長時間中斷各自驗證立即對齊。
    if (!peer.Observe(114, {1.4f, 0, 0}, true) || peer.Position()->x != 1.4f ||
        !peer.Observe(115, {1.45f, 0, 0}, false) || peer.Position()->x != 1.45f ||
        !peer.Observe(116, {60.f, 0, 0}, false) || peer.Position()->x != 60.f ||
        !peer.Observe(124, {60.2f, 0, 0}, false) || peer.Position()->x != 60.2f) return false;
    return peer.Advance(frame).x == 0.f && peer.Position()->x == 60.2f;
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    if (!VerifyPeerPresentation() || !VerifyPeerDodgePresentation()) return 23;
    const auto asset = Mistwatch::LoadRiverbankAvatarAsset(argv[1]);
    const Mistwatch::RiverbankAvatarMotion motion(asset);
    if (motion.HasRunClip() && !VerifyRunPlayback(motion, asset)) return 24;
    if (motion.HasRunClip() && !VerifyRunSlopeContact(motion, asset)) return 25;
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
        Mistwatch::RiverbankAvatarMotion::Player resumed;
        resumed.ObserveStance(false, true, 0.f);
        const auto firstPose = motion.Evaluate(asset.model.skeleton, resumed, feet, scratch, pose);
        const float resumedHead = TransformPoint(firstPose * pose.globalMatrices[headIndex], {}).y - feet.y;
        if (resumed.downedSeconds != 1.f || !std::isfinite(resumedHead) || resumedHead > .45f) return 30;
        Mistwatch::RiverbankAvatarMotion::Player witnessed;
        witnessed.ObserveStance(false, false, 0.f);
        witnessed.ObserveStance(false, true, .05f);
        witnessed.Advance({}, .05f, true);
        if (witnessed.downedSeconds != .05f) return 31;
        // 大幅度權威位置重設時也必須立即呈現已倒地的快照。
        auto warped = resumed;
        warped.distance = .9f;
        warped.walking = .6f;
        warped.runWeight = .3f;
        constexpr Vec3 relocated{2.3f, 1.1f, 5.2f};
        warped.ResetAfterWarp(true);
        const auto warpPose = motion.Evaluate(asset.model.skeleton, warped, relocated, scratch, pose);
        const float warpedHead = TransformPoint(warpPose * pose.globalMatrices[headIndex], {}).y - relocated.y;
        std::cout << "authority_warp_downed_seconds=" << warped.downedSeconds
                  << " warped_head_height_m=" << warpedHead << '\n';
        if (warped.downedSeconds != 1.f || !std::isfinite(warpedHead) ||
            std::abs(warpedHead - resumedHead) > .001f) return 32;
        if (warped.distance != 0.f || warped.walking != 0.f || warped.runWeight != 0.f) return 33;
        warped.ResetAfterWarp(false);
        const auto campPose = motion.Evaluate(asset.model.skeleton, warped, relocated, scratch, pose);
        const auto campHead = TransformPoint(campPose * pose.globalMatrices[headIndex], {}).y - relocated.y;
        std::cout << "authority_warp_recovered_seconds=" << warped.downedSeconds
                  << " recovered_head_height_m=" << campHead << '\n';
        if (warped.downedSeconds != 0.f || !std::isfinite(campHead) || campHead < 1.2f) return 34;
        std::cout << "rejoined_downed_head_height_m=" << resumedHead << " witnessed_fall_seconds="
                  << witnessed.downedSeconds << '\n';
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
    using Mistwatch::RiverbankClass;
    const auto base = Mistwatch::RiverbankAvatarMaterials(asset);
    const std::array roles{RiverbankClass::Guardian, RiverbankClass::Ranger, RiverbankClass::Priest};
    const std::array vests{Color{.30f, .073f, .044f, 1.f}, Color{.091f, .20f, .085f, 1.f}, Color{.56f, .46f, .29f, 1.f}};
    const auto vest = std::ranges::find(asset.materialSlotNames, "AX textile | Warm woven vest");
    if (vest == asset.materialSlotNames.end()) return 10;
    const auto vestIndex = static_cast<std::size_t>(vest - asset.materialSlotNames.begin());
    constexpr std::size_t roomActors = 32;
    const auto unbatched = LoadSkinnedModelAssetFileDetailed(argv[1]);
    if (!unbatched || asset.model.bindMesh.vertices.size() != unbatched.asset.model.bindMesh.vertices.size()) return 23;
    std::size_t expectedFirstIndex = 0;
    for (const auto& group : asset.submeshes) {
        if (group.firstIndex != expectedFirstIndex) return 23;
        const auto& baseline = unbatched.asset.model.bindMesh.indices;
        for (const auto& part : unbatched.asset.submeshes) {
            if (part.materialSlot != group.materialSlot) continue;
            const auto first = baseline.begin() + part.firstIndex;
            if (expectedFirstIndex + part.indexCount > asset.model.bindMesh.indices.size() ||
                !std::equal(first, first + part.indexCount, asset.model.bindMesh.indices.begin() + expectedFirstIndex)) return 23;
            expectedFirstIndex += part.indexCount;
        }
        if (expectedFirstIndex != group.firstIndex + group.indexCount) return 23;
    }
    if (expectedFirstIndex != asset.model.bindMesh.indices.size()) return 23;
    const auto body = Mistwatch::CompactRiverbankAvatarPart(asset, asset.model, false);
    const auto vestPart = Mistwatch::CompactRiverbankAvatarPart(asset, asset.model, true);
    if (body.submeshes.size() + vestPart.submeshes.size() != asset.submeshes.size() ||
        vestPart.submeshes.front().materialSlot != vestIndex ||
        body.model.bindMesh.indices.size() + vestPart.model.bindMesh.indices.size() != asset.model.bindMesh.indices.size()) return 25;
    const SkinnedMeshHandle3D bodyHandle{3, asset.model.skeletonCompatibilityId, 80};
    const SkinnedMeshHandle3D vestHandle{4, asset.model.skeletonCompatibilityId, 80};
    std::vector<SkinnedVertex3D> gpuVertices;
    if (!BuildGpuSkinnedVertices(body.model, gpuVertices) || !BuildGpuSkinnedVertices(vestPart.model, gpuVertices)) return 26;
    const auto compactVertices = body.model.bindMesh.vertices.size() + 4 * vestPart.model.bindMesh.vertices.size();
    if (compactVertices >= 2 * asset.model.bindMesh.vertices.size()) return 27;
    std::vector<SkinnedSubmesh3D> renderedSurfaces = body.submeshes;
    renderedSurfaces.push_back(vestPart.submeshes.front());
    for (std::size_t actor = 0; actor < roomActors; ++actor) {
        const auto role = actor % roles.size();
        const auto playerClass = Mistwatch::ReadRiverbankClassFlags(Mistwatch::RiverbankClassFlags(roles[role]) | 1u);
        const auto materials = Mistwatch::RiverbankAvatarMaterials(asset, playerClass);
        if (PackColorRgba8(materials[vestIndex].tint) != PackColorRgba8(vests[role])) return 11;
        const Vec3 position = feet + Vec3{2.f * static_cast<float>(actor), 0, 0};
        const auto transform = motion.Evaluate(asset.model.skeleton, state, position, scratch, pose);
        const auto palette = AddSkinPaletteFromPose(scene, asset.model, pose, workspace);
        const std::array parts{
            SkinnedRenderablePart3D{.mesh=bodyHandle, .transform=transform,
                .totalIndexCount=static_cast<std::uint32_t>(body.model.bindMesh.indices.size()), .submeshes=body.submeshes,
                .materialBindings=materials, .key={1}},
            SkinnedRenderablePart3D{.mesh=vestHandle, .transform=transform,
                .totalIndexCount=static_cast<std::uint32_t>(vestPart.model.bindMesh.indices.size()), .submeshes=vestPart.submeshes,
                .materialBindings=materials, .key={2}},
        };
        if (!SubmitSkinnedRenderableGroup3D(scene, palette, parts)) return 8;
        bool submittedVest = false;
        std::size_t changedFabricParts = 0;
        for (std::size_t part = 0; part < asset.submeshes.size(); ++part) {
            const auto slot = renderedSurfaces[part].materialSlot;
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
    const auto commandsPerActor = Mistwatch::HasRiverbankBoots(asset) ? 15u : 13u;
    if (scene.skinnedMeshInstances.size() != roomActors * commandsPerActor || scene.skinPaletteJoints.size() != roomActors * 80) return 9;
    // 兩隻野豬各需要一個繪製項目及 16 根骨骼。
    const SkinnedMeshHandle3D wildlife{2, {}, 16};
    const std::array<SkinPaletteJoint3D, 16> wildlifeJoints{};
    for (int boar = 0; boar < 2; ++boar) {
        const auto palette = scene.AddSkinPalette(wildlifeJoints);
        if (!scene.SkinnedMeshInstance(wildlife, palette, Matrix4::Identity(), {}, {}, {}, .25f, false,
                                       MaterialClass3D::Surface, true, DirectionalShadowAllCascades3D, 0, 3)) return 24;
    }
    std::cout << "shared_body_vertices=" << body.model.bindMesh.vertices.size()
              << " one_vest_vertices=" << vestPart.model.bindMesh.vertices.size()
              << " five_resource_vertices=" << compactVertices
              << " original_four_mesh_vertices=" << 4 * asset.model.bindMesh.vertices.size() << '\n';
    std::cout << "room_actors=" << roomActors << " boars=2 full_room_draw_commands=" << scene.skinnedMeshInstances.size()
              << " full_room_skin_joints=" << scene.skinPaletteJoints.size() << '\n';
    std::cout << "samples=195 pelvis_planar_drift_m=" << largestDrift
              << " lowest_animated_floor_m=" << lowest << " highest_animated_floor_m=" << highest
              << " three_role_draw_commands=" << 3 * commandsPerActor << '\n';
    if (largestDrift > .035f || lowest < -.004f || highest > .005f) return 7;
    return 0;
}
