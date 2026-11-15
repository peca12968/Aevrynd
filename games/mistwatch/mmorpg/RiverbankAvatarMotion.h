#pragma once

#include <Aevrynd/SkinnedModelAsset.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Mistwatch {
struct RiverbankAvatarMotion {
    // 與 W3 的 Blender 地面量測及匯入幾何共用。
    static constexpr float GroundDatum = -1.562f;

    struct Player {
        float distance{}, heading{}, walking{};

        void Advance(Aevrynd::Vec3 tickDisplacement, float elapsed) {
            elapsed = std::clamp(elapsed, 0.0f, 0.1f);
            const float movement = std::hypot(tickDisplacement.x, tickDisplacement.z);
            const bool moving = movement > 0.0001f;
            if (moving) heading = std::atan2(tickDisplacement.x, tickDisplacement.z);
            walking = std::clamp(walking + (moving ? elapsed : -elapsed) * 7.0f, 0.0f, 1.0f);
            // 本機世界以每秒 20 次更新傳送每次位移公尺數。
            distance += movement * elapsed * 20.f;
            if (walking == 0.0f) distance = 0.0f;
        }
    };

    explicit RiverbankAvatarMotion(const Aevrynd::SkinnedModelAssetData& asset) {
        const auto clip = std::ranges::find(asset.clips, "RiverbankExplorer_Walk", &Aevrynd::NamedAnimationClip3D::name);
        const auto pelvis = std::ranges::find(asset.jointNames, "pelvis");
        if (clip == asset.clips.end() || pelvis == asset.jointNames.end())
            throw std::runtime_error("Riverbank avatar pelvis or walk clip missing");
        walk = clip->clip;
        walk.looping = false;
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
        Aevrynd::Vec3 feet, Aevrynd::AnimationPose3D& sampled, Aevrynd::AnimationPose3D& pose) const {
        const float phase = std::fmod(player.distance, stride) / stride;
        Aevrynd::SampleAnimationClipInto(skeleton, walk, phase * walk.duration, sampled);
        Aevrynd::BlendAnimationPosesInto(skeleton, idle, sampled, player.walking, pose);
        const auto world = Aevrynd::CreateTransformMatrix(feet, {0, player.heading - assetYaw, 0}, {1, 1, 1});
        // 扣除骨架根部的線性位移，將 Blender 量測的腳底高度對齊伺服器位置。
        return world * Aevrynd::CreateTranslationMatrix(Aevrynd::Vec3{0, -GroundDatum, 0} - travel * (phase * player.walking));
    }

    float Stride() const noexcept { return stride; }

private:
    Aevrynd::AnimationClip3D walk;
    Aevrynd::AnimationPose3D idle;
    Aevrynd::Vec3 travel{};
    float stride{}, assetYaw{};
};
}
