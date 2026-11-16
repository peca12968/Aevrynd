#pragma once

#include <Aevrynd/Camera.h>
#include <Aevrynd/Geometry3D.h>
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <algorithm>
#include <array>
#include <optional>
#include <span>

namespace Mistwatch {
class RiverbankDownedCamera {
public:
    bool Moving() const noexcept { return Aevrynd::Length(offset_) > .001f || Aevrynd::Length(aimOffset_) > .001f; }

    Aevrynd::Camera3D Update(const Aevrynd::Camera3D& base, const std::optional<std::array<Aevrynd::Vec3, 2>>& body,
        const std::optional<Aevrynd::BoxCollider3D>& occluder, float dt,
        std::span<const Aevrynd::BoxCollider3D> solids, const Aevrynd::HeightField3D& terrain,
        std::span<const Aevrynd::BoxCollider3D> cameraOnly, std::span<const Aevrynd::BoxCollider3D> foliage) {
        using namespace Aevrynd;
        const Vec3 forward = base.target - base.position;
        const Vec3 horizontal = Normalize(Vec3{forward.x, 0, forward.z});
        if (Length(horizontal) < .5f || !std::isfinite(dt) || dt <= 0) return base;
        const Vec3 right{horizontal.z, 0, -horizontal.x};
        const float closer = std::min(3.3f, .62f * std::hypot(forward.x, forward.z));
        const auto blocked = [&](Vec3 origin, Vec3 subject) {
            const Vec3 delta = subject - origin;
            const float distance = Length(delta);
            if (distance < .20f) return true;
            const Ray3D ray{origin, delta * (1.f / distance)};
            const auto hits = [&](const BoxCollider3D& box) {
                return RayIntersectsAabb(ray, box.center, box.extents, distance - .18f);
            };
            return (occluder && hits(*occluder)) || std::ranges::any_of(foliage, hits);
        };
        // 先找能看到頭肩與胸口的視角，分數相同時選較短的位移。
        Vec3 selected{};
        unsigned best = 3;
        if (body) for (const auto [side, lift, inset] : std::array<Vec3, 6>{{
            {0, 2.35f, .45f}, {-1.85f, 2.6f, .60f}, {1.85f, 2.6f, .60f},
            {0, 3.5f, closer}, {-2.2f, 3.5f, closer}, {2.2f, 3.5f, closer}}}) {
            const Vec3 offset = right * side + Vec3{0, lift, 0} + horizontal * inset;
            const Vec3 eye = base.position + offset;
            const unsigned count = unsigned(blocked(eye, (*body)[0])) + unsigned(blocked(eye, (*body)[1]));
            if (count >= best || !ThirdPersonCameraPathClear3D(base.position, eye, solids, terrain, cameraOnly, .155f))
                continue;
            selected = offset;
            best = count;
            if (count == 0) break;
        }
        const float elapsed = std::min(dt, .1f);
        const auto approach = [&](Vec3 current, Vec3 target, float metresPerSecond) {
            const Vec3 delta = target - current;
            const float distance = Length(delta);
            const float step = metresPerSecond * elapsed;
            return distance <= step ? target : current + delta * (step / distance);
        };
        offset_ = approach(offset_, selected, 12.f);
        const Vec3 aim = body ? ((*body)[0] + (*body)[1]) * .5f - base.target : Vec3{};
        aimOffset_ = approach(aimOffset_, aim, 5.f);
        auto view = base;
        const Vec3 eye = base.position + offset_;
        if (ThirdPersonCameraPathClear3D(base.position, eye, solids, terrain, cameraOnly, .155f))
            view.position = eye;
        view.target = base.target + aimOffset_;
        return view;
    }

private:
    Aevrynd::Vec3 offset_{}, aimOffset_{};
};
}
