#pragma once

#include "RiverbankAvatarAsset.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <span>

namespace Mistwatch {
struct RiverbankAvatarMotion {
    // Shared by W3's Blender ground measurement and the imported geometry.
    static constexpr float GroundDatum = -1.562f;

    struct Player {
        float distance{}, heading{}, walking{}, downedSeconds{};

        void Advance(Aevrynd::Vec3 tickDisplacement, float elapsed, bool downed = false) {
            // 本機仍沿用權威快照中的每刻位移。
            elapsed = std::clamp(elapsed, 0.f, .1f);
            AdvanceByTravel(tickDisplacement * (elapsed * 20.f), elapsed, downed, .0001f * elapsed * 20.f);
        }

        void AdvanceByTravel(Aevrynd::Vec3 displacement, float elapsed, bool downed, float movementThreshold = .000001f) {
            elapsed = std::clamp(elapsed, 0.f, .1f);
            downedSeconds = std::clamp(downedSeconds + (downed ? elapsed : -elapsed * 2.5f), 0.f, 1.f);
            const float movement = std::hypot(displacement.x, displacement.z);
            const bool moving = !downed && movement > movementThreshold;
            if (moving) heading = std::atan2(displacement.x, displacement.z);
            walking = std::clamp(walking + (moving ? elapsed : -elapsed) * 7.f, 0.f, 1.f);
            if (!downed) distance += movement;
            if (walking == 0.f) distance = 0.f;
        }
    };

    explicit RiverbankAvatarMotion(const Aevrynd::SkinnedModelAssetData& asset) {
        const auto clip = std::ranges::find(asset.clips, "RiverbankExplorer_Walk", &Aevrynd::NamedAnimationClip3D::name);
        const auto pelvis = std::ranges::find(asset.jointNames, "pelvis");
        if (clip == asset.clips.end() || pelvis == asset.jointNames.end())
            throw std::runtime_error("Riverbank avatar pelvis or walk clip missing");
        if (HasRiverbankBoots(asset)) {
            const auto sole = std::ranges::find(asset.materialSlotNames, RiverbankBootSole);
            const auto slot = static_cast<std::size_t>(sole - asset.materialSlotNames.begin());
            groundDatum = std::numeric_limits<float>::infinity();
            for (const auto& part : asset.submeshes) {
                if (part.materialSlot != slot) continue;
                for (const auto vertex : std::span(asset.model.bindMesh.indices).subspan(part.firstIndex, part.indexCount))
                    groundDatum = std::min(groundDatum, asset.model.bindMesh.vertices[vertex].y);
            }
            if (!std::isfinite(groundDatum) || groundDatum > GroundDatum - .005f || groundDatum < GroundDatum - .022f)
                throw std::runtime_error("Riverbank boot sole has an invalid authored ground datum");
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
        if (!std::isfinite(stride) || std::abs(stride - 0.915f) > 0.002f)
            throw std::runtime_error("Riverbank avatar walk no longer matches the authored stride");
        assetYaw = std::atan2(travel.x, travel.z);
    }

    Aevrynd::Matrix4 Evaluate(const Aevrynd::Skeleton3D& skeleton, const Player& player,
        Aevrynd::Vec3 feet, Aevrynd::AnimationPose3D& sampled, Aevrynd::AnimationPose3D& pose, Aevrynd::Vec3 groundNormal = {0, 1, 0}) const {
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
        Aevrynd::SampleAnimationClipInto(skeleton, walk, phase * walk.duration, sampled);
        Aevrynd::BlendAnimationPosesInto(skeleton, idle, sampled, player.walking, pose);
        // Remove linear root travel and put the measured Blender floor on the replicated foot position.
        return world * Aevrynd::CreateTranslationMatrix(Aevrynd::Vec3{0, -groundDatum, 0} - travel * (phase * player.walking));
    }

    float Stride() const noexcept { return stride; }
    bool HasDownedClip() const noexcept { return !downed.tracks.empty(); }

private:
    Aevrynd::AnimationClip3D walk, downed;
    Aevrynd::AnimationPose3D idle;
    Aevrynd::Vec3 travel{};
    float stride{}, assetYaw{}, groundDatum{GroundDatum};
};
}
