#pragma once
#include "MistwatchBridgeDeck.h"

#include "RiverbankAvatarAsset.h"
#include <Aevrynd/SkeletalIk3D.h>
#include <Aevrynd/HeightField3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <span>
#include <vector>

namespace Mistwatch {
struct RiverbankAvatarMotion {
    // Shared by W3's Blender ground measurement and the imported geometry.
    static constexpr float GroundDatum = -1.562f;
    static constexpr float WalkStride = .915f;
    static constexpr float RunStride = 3.2f * 68.f / 96.f;

    struct Player {
        float distance{}, heading{}, walking{}, downedSeconds{}, runWeight{};

        void ResetGait() { distance = walking = runWeight = 0.f; }

        void Advance(Aevrynd::Vec3 tickDisplacement, float elapsed, bool downed = false, bool runAvailable = false) {
            // 本機仍沿用權威快照中的每刻位移。
            elapsed = std::clamp(elapsed, 0.f, .1f);
            AdvanceByTravel(tickDisplacement * (elapsed * 20.f), elapsed, downed, runAvailable, .0001f * elapsed * 20.f);
        }

        void AdvanceByTravel(Aevrynd::Vec3 displacement, float elapsed, bool downed, bool runAvailable = false,
            float movementThreshold = .000001f) {
            elapsed = std::clamp(elapsed, 0.f, .1f);
            downedSeconds = std::clamp(downedSeconds + (downed ? elapsed : -elapsed * 2.5f), 0.f, 1.f);
            const float movement = std::hypot(displacement.x, displacement.z);
            const bool moving = !downed && movement > movementThreshold;
            if (moving) heading = std::atan2(displacement.x, displacement.z);
            walking = std::clamp(walking + (moving ? elapsed : -elapsed) * 7.f, 0.f, 1.f);
            const float speed = moving && elapsed > 0.f ? movement / elapsed : 0.f;
            const float target = runAvailable ? std::clamp((speed - 2.2f) / .7f, 0.f, 1.f) : 0.f;
            runWeight += std::clamp(target - runWeight, -elapsed * 7.f, elapsed * 7.f);
            // 使用同一個循環相位，速度平滑切換時仍維持左右腳交替。
            if (!downed) distance += movement * WalkStride / std::lerp(WalkStride, RunStride, runWeight);
            if (walking == 0.f) ResetGait();
        }
    };

    explicit RiverbankAvatarMotion(const Aevrynd::SkinnedModelAssetData& asset) {
        const auto clip = std::ranges::find(asset.clips, "RiverbankExplorer_Walk", &Aevrynd::NamedAnimationClip3D::name);
        const auto pelvis = std::ranges::find(asset.jointNames, "pelvis");
        if (clip == asset.clips.end() || pelvis == asset.jointNames.end())
            throw std::runtime_error("Riverbank avatar pelvis or walk clip missing");
        const auto running = std::ranges::find(asset.clips, "RiverbankExplorer_Run", &Aevrynd::NamedAnimationClip3D::name);
        if (HasRiverbankBoots(asset)) {
            const auto sole = std::ranges::find(asset.materialSlotNames, RiverbankBootSole);
            const auto slot = static_cast<std::size_t>(sole - asset.materialSlotNames.begin());
            groundDatum = std::numeric_limits<float>::infinity();
            std::vector<std::uint32_t> soleVertices;
            for (const auto& part : asset.submeshes) {
                if (part.materialSlot != slot) continue;
                for (const auto vertex : std::span(asset.model.bindMesh.indices).subspan(part.firstIndex, part.indexCount)) {
                    if (running != asset.clips.end()) soleVertices.push_back(vertex);
                    groundDatum = std::min(groundDatum, asset.model.bindMesh.vertices[vertex].y);
                }
            }
            if (!std::isfinite(groundDatum) || groundDatum > GroundDatum - .005f || groundDatum < GroundDatum - .022f)
                throw std::runtime_error("Riverbank boot sole has an invalid authored ground datum");
            std::ranges::sort(soleVertices);
            soleVertices.erase(std::unique(soleVertices.begin(), soleVertices.end()), soleVertices.end());
            for (const auto index : soleVertices) {
                const auto& vertex = asset.model.bindMesh.vertices[index];
                const auto& influence = asset.model.influences[index];
                auto& contact = runContacts.emplace_back();
                contact.influence = influence;
                contact.side = vertex.x > 0.f ? 1u : 0u;
                for (std::size_t slotIndex = 0; slotIndex < influence.joints.size(); ++slotIndex) {
                    if (influence.weights[slotIndex] == 0) continue;
                    contact.jointSpacePoints[slotIndex] = Aevrynd::TransformPoint(
                        asset.model.inverseBindMatrices[influence.joints[slotIndex]], {vertex.x, vertex.y, vertex.z});
                }
            }
        }
        walk = clip->clip;
        walk.looping = false;
        const auto fallen = std::ranges::find(asset.clips, "RiverbankExplorer_Downed", &Aevrynd::NamedAnimationClip3D::name);
        if (fallen != asset.clips.end()) {
            downed = fallen->clip;
            downed.looping = false;
            if (std::abs(downed.duration - 1.f) > .001f)
                throw std::runtime_error("Riverbank avatar downed motion must last one second");
        }
        idle = Aevrynd::SampleAnimationClip(asset.model.skeleton, walk, 0.0f);
        const auto end = Aevrynd::SampleAnimationClip(asset.model.skeleton, walk, walk.duration);
        const auto index = static_cast<std::size_t>(pelvis - asset.jointNames.begin());
        travel = Aevrynd::TransformPoint(end.globalMatrices[index], {}) - Aevrynd::TransformPoint(idle.globalMatrices[index], {});
        travel.y = 0.0f;
        stride = Aevrynd::Length(travel);
        if (!std::isfinite(stride) || std::abs(stride - WalkStride) > 0.002f)
            throw std::runtime_error("Riverbank avatar walk no longer matches the authored stride");
        assetYaw = std::atan2(travel.x, travel.z);

        if (running != asset.clips.end()) {
            if (runContacts.empty()) throw std::runtime_error("Riverbank run boot contacts are missing");
            runPelvis = index;
            const auto findJoint = [&](std::string_view name) {
                const auto joint = std::ranges::find(asset.jointNames, name);
                if (joint == asset.jointNames.end()) throw std::runtime_error("Riverbank slope limb joint missing");
                return static_cast<std::size_t>(joint - asset.jointNames.begin());
            };
            runLegs = {{
                {findJoint("thigh.L"), findJoint("shin.L"), findJoint("foot.L")},
                {findJoint("thigh.R"), findJoint("shin.R"), findJoint("foot.R")}
            }};
            for (const auto& leg : runLegs)
                if (asset.model.skeleton.joints[leg.end].parent != static_cast<int>(leg.middle) ||
                    asset.model.skeleton.joints[leg.middle].parent != static_cast<int>(leg.upper))
                    throw std::runtime_error("Riverbank slope limb chain does not match the authored rig");
            run = running->clip;
            run.looping = false;
            const auto runBegin = Aevrynd::SampleAnimationClip(asset.model.skeleton, run, 0.f);
            const auto runEnd = Aevrynd::SampleAnimationClip(asset.model.skeleton, run, run.duration);
            runTravel = Aevrynd::TransformPoint(runEnd.globalMatrices[index], {}) -
                Aevrynd::TransformPoint(runBegin.globalMatrices[index], {});
            runTravel.y = 0.f;
            if (std::abs(run.duration - 68.f / 96.f) > .001f ||
                Aevrynd::Length(runTravel - Aevrynd::Normalize(travel) * RunStride) > .002f)
                throw std::runtime_error("Riverbank avatar run does not match the authored stride");
        }
    }

    Aevrynd::Matrix4 Evaluate(const Aevrynd::Skeleton3D& skeleton, const Player& player,
        Aevrynd::Vec3 feet, Aevrynd::AnimationPose3D& sampled, Aevrynd::AnimationPose3D& pose, Aevrynd::Vec3 groundNormal = {0, 1, 0}, const RiverbankWalkSurface* surface = nullptr) const {
        const auto world = Aevrynd::CreateTransformMatrix(feet, {0, player.heading - assetYaw, 0}, {1, 1, 1});
        if (HasDownedClip() && player.downedSeconds > 0) {
            Aevrynd::SampleAnimationClipInto(skeleton, downed, player.downedSeconds, pose);
            // 角色側倒後以草坡法線為支撐面，權威座標仍是原本的接地錨點。
            constexpr Aevrynd::Vec3 up{0, 1, 0};
            const float t = std::clamp((player.downedSeconds - .15f) / .70f, 0.f, 1.f);
            const Aevrynd::Vec3 alignedUp = Aevrynd::Normalize(up + (groundNormal - up) * (t * t * (3.f - 2.f * t)));
            const auto axis = Aevrynd::Cross(up, alignedUp);
            Aevrynd::JointTransform3D support;
            support.usesQuaternion = true;
            support.quaternion = Aevrynd::NormalizeQuaternion3D({axis.x, axis.y, axis.z, 1.f + alignedUp.y});
            return Aevrynd::CreateTranslationMatrix(feet) * support.Matrix() *
                Aevrynd::CreateRotationMatrix({0, player.heading - assetYaw, 0}) *
                Aevrynd::CreateTranslationMatrix({0, -groundDatum, 0});
        }
        const float phase = std::fmod(player.distance, stride) / stride;
        const float amount = HasRunClip() ? player.runWeight : 0.f;
        if (player.walking == 0.f) {
            pose = idle;
        } else {
            if (amount < 1.f)
                Aevrynd::SampleAnimationClipInto(skeleton, walk, phase * walk.duration, sampled);
            if (amount > 0.f && amount < 1.f) {
                Aevrynd::SampleAnimationClipInto(skeleton, run, phase * run.duration, pose);
                // 混合逐骨骼取值後才寫入該骨骼，因此可重用步行暫存。
                Aevrynd::BlendAnimationPosesInto(skeleton, sampled, pose, amount, sampled);
            }
            if (amount == 1.f)
                Aevrynd::SampleAnimationClipInto(skeleton, run, phase * run.duration, sampled);
            Aevrynd::BlendAnimationPosesInto(skeleton, idle, sampled, player.walking, pose);
        }
        const auto rootTravel = travel * (1.f - amount) + runTravel * amount;
        // 根位移由伺服器決定，動畫僅播放原地姿勢。
        const float lift = amount > 0.f && (amount < 1.f || player.walking < 1.f) ? RunBlendFloorLift(pose) : 0.f;
        auto offset = Aevrynd::Vec3{0, lift - groundDatum, 0} - rootTravel * (phase * player.walking);
        if (HasRunClip() && groundNormal.y >= .8f && (surface || std::hypot(groundNormal.x, groundNormal.z) > .002f)) {
            const auto inverseYaw = Aevrynd::CreateRotationMatrix({0, assetYaw - player.heading, 0});
            const auto normal = Aevrynd::TransformNormal(inverseYaw, groundNormal);
            offset.y += ConformRunFeet(skeleton, pose, offset, normal, world, surface);
        }
        return world * Aevrynd::CreateTranslationMatrix(offset);
    }

    float Stride() const noexcept { return stride; }
    bool HasDownedClip() const noexcept { return !downed.tracks.empty(); }
    bool HasRunClip() const noexcept { return !run.tracks.empty(); }

private:
    struct RunContact {
        Aevrynd::SkinInfluences3D influence;
        std::array<Aevrynd::Vec3, 4> jointSpacePoints{};
        std::size_t side{};

        Aevrynd::Vec3 Sample(const Aevrynd::AnimationPose3D& pose) const {
            Aevrynd::Vec3 point{};
            for (std::size_t i = 0; i < influence.joints.size(); ++i) {
                const auto weight = influence.weights[i];
                if (weight != 0)
                    point = point + Aevrynd::TransformPoint(pose.globalMatrices[influence.joints[i]],
                        jointSpacePoints[i]) * (static_cast<float>(weight) / 65535.f);
            }
            return point;
        }
    };

    float RunBlendFloorLift(const Aevrynd::AnimationPose3D& pose) const {
        // 交叉混合會使靴底短暫穿入平地。只在混合期間取靴底樣本，沿垂直方向補償。
        float lowest = groundDatum;
        for (const auto& contact : runContacts)
            lowest = std::min(lowest, contact.Sample(pose).y);
        return groundDatum - lowest;
    }

    float ConformRunFeet(const Aevrynd::Skeleton3D& skeleton, Aevrynd::AnimationPose3D& pose,
        Aevrynd::Vec3 offset, Aevrynd::Vec3 normal, const Aevrynd::Matrix4& modelToWorld,
        const RiverbankWalkSurface* surface) const {
        using namespace Aevrynd;
        const auto clearance = [&](Vec3 local) {
            if (surface) {
                const auto world = TransformPoint(modelToWorld, local);
                if (surface->Contains(world.x,world.z))
                    return world.y - surface->SampleHeight(world.x,world.z);
            }
            return Dot(local, normal) / normal.y;
        };
        const float maximum = std::numeric_limits<float>::infinity();
        std::array<float, 2> flatGap{maximum, maximum}, surfaceGap{maximum, maximum};
        for (const auto& contact : runContacts) {
            const auto point = contact.Sample(pose) + offset;
            flatGap[contact.side] = std::min(flatGap[contact.side], point.y);
            surfaceGap[contact.side] = std::min(surfaceGap[contact.side], clearance(point));
        }
        // 每腳延續原步態的離地高度。先降低骨盆，讓斜坡低側不必把膝蓋拉直。
        std::array<Vec3, 2> targets{};
        float pelvisDrop = 0.f, largestAdjustment = 0.f;
        for (std::size_t side = 0; side < runLegs.size(); ++side) {
            const auto leg = runLegs[side];
            const auto upper = TransformPoint(pose.globalMatrices[leg.upper], {});
            const auto knee = TransformPoint(pose.globalMatrices[leg.middle], {});
            const auto ankle = TransformPoint(pose.globalMatrices[leg.end], {});
            targets[side] = ankle;
            const auto adjust = std::clamp(std::max(flatGap[side], 0.f) - surfaceGap[side], -.20f, .20f);
            largestAdjustment = std::max(largestAdjustment,std::abs(adjust));
            targets[side].y += adjust;
            const auto target = targets[side] - upper;
            const float reach = Length(knee - upper) + Length(ankle - knee) - .002f;
            const float verticalReach = std::sqrt(std::max(0.f, reach * reach - target.x * target.x - target.z * target.z));
            pelvisDrop = std::min(pelvisDrop, target.y + verticalReach);
        }
        if (largestAdjustment < .0002f) return 0.f;
        pelvisDrop = std::max(pelvisDrop, -.15f);
        if (pelvisDrop < 0.f) {
            auto translation = Vec3{0, pelvisDrop, 0};
            const auto parent = skeleton.joints[runPelvis].parent;
            if (parent >= 0) {
                const auto inverseTranspose = CreateNormalMatrix(pose.globalMatrices[static_cast<std::size_t>(parent)]);
                translation = {pelvisDrop * inverseTranspose[1][0], pelvisDrop * inverseTranspose[1][1],
                    pelvisDrop * inverseTranspose[1][2]};
            }
            pose.localTransforms[runPelvis].translation = pose.localTransforms[runPelvis].translation + translation;
            BuildAnimationPoseGlobalMatrices3D(skeleton, pose);
        }
        const auto forward = Normalize(travel);
        for (std::size_t side = 0; side < runLegs.size(); ++side) {
            const auto leg = runLegs[side];
            const auto hip = TransformPoint(pose.globalMatrices[leg.upper], {});
            (void)SolveTwoBoneIkKeepEndOrientation3D(skeleton, leg, {targets[side], hip + forward}, pose);
        }
        // 靴底有少量跨關節權重，最後用真實靴底位置補償殘差。
        float minimum = 0.f;
        for (const auto& contact : runContacts)
            minimum = std::min(minimum, clearance(contact.Sample(pose) + offset));
        return -minimum;
    }

    std::array<Aevrynd::TwoBoneIkChain3D, 2> runLegs{};
    std::size_t runPelvis{};
    std::vector<RunContact> runContacts;
    Aevrynd::AnimationClip3D walk, downed, run;
    Aevrynd::AnimationPose3D idle;
    Aevrynd::Vec3 travel{}, runTravel{};
    float stride{}, assetYaw{}, groundDatum{GroundDatum};
};
}
