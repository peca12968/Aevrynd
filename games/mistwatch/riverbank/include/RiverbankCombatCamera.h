#pragma once

#include <Aevrynd/Camera.h>
#include <Aevrynd/Geometry3D.h>
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace Mistwatch {
class RiverbankCombatCamera {
public:
    Aevrynd::Camera3D Update(const Aevrynd::Camera3D& base, Aevrynd::Vec3 feet,
        std::optional<Aevrynd::Vec3> threat, float dt, std::span<const Aevrynd::BoxCollider3D> solids,
        const Aevrynd::HeightField3D& terrain, std::span<const Aevrynd::BoxCollider3D> cameraOnly = {},
        std::span<const Aevrynd::BoxCollider3D> foliage = {}) {
        using namespace Aevrynd;
        const Vec3 forward = base.target - base.position;
        const float range = std::hypot(forward.x, forward.z);
        if (!(range > .2f) || !std::isfinite(dt) || dt <= 0.f) return base;
        const Vec3 at{forward.x / range, 0, forward.z / range};
        const Vec3 right{at.z, 0, -at.x};
        float desired = 0.f;
        if (threat) {
            const Vec3 delta = *threat - feet;
            if (Dot(delta, at) > .12f && std::abs(delta.y) < 1.8f) {
                const float close = std::clamp((4.3f - std::hypot(delta.x, delta.z)) / 1.7f, 0.f, 1.f);
                const float overlap = std::clamp((1.65f - std::abs(Dot(delta, right))) / .95f, 0.f, 1.f);
                desired = close * overlap;
                if (desired > 0.f && !engaged_) {
                    side_ = Dot(delta, right) < -.08f ? -1.f : 1.f;
                    engaged_ = true;
                }
            }
        }
        if (desired == 0.f && weight_ < .001f) {
            engaged_ = false;
            return base;
        }
        const auto displaced = [&](float side, float weight) {
            const Vec3 offset = (right * (side * std::min(2.87f, range * .51f)) +
                                Vec3{0, std::min(1.22f, range * .2f), 0}) * weight;
            auto view = base;
            view.position = view.position + offset;
            // 水平平移保留 WASD 方向。略向下取景保留腳下的攻擊預警。
            view.target = view.target + Vec3{offset.x, offset.y * .4f, offset.z};
            return view;
        };
        const auto blocked = [&](const Camera3D& view) {
            if (foliage.empty()) return 0u;
            const auto obscured = [&](Vec3 subject) {
                const Vec3 delta = subject - view.position;
                const float limit = Length(delta);
                if (limit < .55f) return false;
                const Ray3D ray{view.position, delta * (1.f / limit)};
                return std::ranges::any_of(foliage, [&](const auto& branch) {
                    return RayIntersectsAabb(ray, branch.center, branch.extents, limit - .5f);
                });
            };
            return unsigned(obscured(feet + Vec3{0, 1.35f, 0})) +
                   unsigned(obscured(feet + Vec3{0, .8f, 0})) +
                   (threat ? 2u * unsigned(obscured(*threat + Vec3{0, .65f, 0})) : 0u);
        };
        const unsigned baseBlocked = blocked(base);
        float requestedSide = side_;
        if (desired > 0.f && !foliage.empty() &&
            blocked(displaced(-side_, desired)) < blocked(displaced(side_, desired))) requestedSide = -side_;

        // 換肩時先收回位移，再往較清楚的一側移動，避免直接跳過玩家。
        const float targetWeight = requestedSide == side_ ? desired : 0.f;
        const float step = std::min(dt, .1f) * (targetWeight > weight_ ? 3.45f : 2.5f);
        weight_ += std::clamp(targetWeight - weight_, -step, step);
        if (weight_ < .02f && requestedSide != side_) {
            side_ = requestedSide;
            weight_ = 0.f;
        }
        if (weight_ < .001f) return base;

        for (const float scale : std::array{1.f, .5f, .25f}) {
            const auto view = displaced(side_, weight_ * scale);
            if (blocked(view) > baseBlocked ||
                !ProjectToViewport(feet + Vec3{0, .2f, 0}, view, {1280, 720}).visible ||
                !ProjectToViewport(feet + Vec3{0, 1.75f, 0}, view, {1280, 720}).visible) continue;
            if (ThirdPersonCameraPathClear3D(base.position, view.position, solids, terrain, cameraOnly, .155f))
                return view;
        }
        return base;
    }

private:
    bool engaged_{};
    float weight_{};
    float side_{1.f};
};
}
