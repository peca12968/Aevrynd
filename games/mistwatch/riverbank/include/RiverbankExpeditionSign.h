#pragma once

#include <Aevrynd/Scene3D.h>
#include <span>

namespace Mistwatch::RiverbankSigns {
struct Stroke {
    Aevrynd::Vec2 start, end;
};

// 手刻筆畫使用局部座標。每筆會放在牌面上，與木牌一起接受深度遮擋。
inline constexpr Stroke commission[]{
    {{.17f,.17f},{.80f,.10f}}, {{.16f,.31f},{.86f,.31f}}, {{.51f,.13f},{.51f,.53f}},
    {{.48f,.31f},{.16f,.51f}}, {{.55f,.32f},{.86f,.52f}},
    {{.49f,.53f},{.30f,.74f}}, {{.30f,.74f},{.77f,.91f}}, {{.14f,.66f},{.87f,.66f}},
    {{.70f,.67f},{.62f,.79f}}, {{.62f,.79f},{.42f,.88f}}, {{.42f,.88f},{.18f,.91f}},
};
inline constexpr Stroke request[]{
    {{.21f,.10f},{.27f,.19f}}, {{.05f,.26f},{.48f,.26f}}, {{.12f,.41f},{.42f,.41f}},
    {{.12f,.53f},{.42f,.53f}}, {{.14f,.66f},{.41f,.66f}}, {{.14f,.66f},{.14f,.88f}},
    {{.41f,.66f},{.41f,.88f}}, {{.14f,.88f},{.41f,.88f}}, {{.54f,.25f},{.89f,.13f}},
    {{.50f,.53f},{.92f,.47f}}, {{.69f,.23f},{.69f,.83f}}, {{.69f,.83f},{.89f,.88f}},
    {{.89f,.88f},{.95f,.76f}},
};
inline constexpr Stroke north[]{
    {{.38f,.14f},{.38f,.84f}}, {{.12f,.39f},{.38f,.39f}}, {{.09f,.76f},{.38f,.60f}},
    {{.59f,.13f},{.59f,.84f}}, {{.59f,.84f},{.86f,.88f}}, {{.86f,.88f},{.95f,.72f}},
    {{.59f,.46f},{.91f,.30f}},
};
inline constexpr Stroke shore[]{
    {{.24f,.12f},{.24f,.31f}}, {{.24f,.31f},{.79f,.31f}}, {{.79f,.31f},{.79f,.12f}},
    {{.51f,.07f},{.51f,.31f}}, {{.19f,.43f},{.89f,.43f}}, {{.19f,.43f},{.15f,.75f}},
    {{.15f,.75f},{.10f,.87f}}, {{.37f,.61f},{.83f,.61f}}, {{.31f,.76f},{.92f,.76f}},
    {{.61f,.60f},{.61f,.95f}},
};
inline constexpr Stroke east[]{
    {{.09f,.18f},{.91f,.18f}}, {{.25f,.34f},{.75f,.34f}}, {{.25f,.34f},{.25f,.63f}},
    {{.75f,.34f},{.75f,.63f}}, {{.25f,.49f},{.75f,.49f}}, {{.25f,.63f},{.75f,.63f}},
    {{.50f,.06f},{.50f,.95f}}, {{.49f,.65f},{.13f,.92f}}, {{.51f,.65f},{.88f,.92f}},
};
inline constexpr Stroke west[]{
    {{.07f,.13f},{.93f,.13f}}, {{.17f,.32f},{.83f,.32f}}, {{.17f,.32f},{.17f,.88f}},
    {{.83f,.32f},{.83f,.88f}}, {{.17f,.88f},{.83f,.88f}},
    {{.40f,.13f},{.38f,.51f}}, {{.38f,.51f},{.28f,.66f}},
    {{.61f,.13f},{.61f,.58f}}, {{.61f,.58f},{.73f,.66f}},
};
inline constexpr std::span<const Stroke> campLabel[]{commission,request};
inline constexpr std::span<const Stroke> bridgeLabel[]{north,shore};
inline constexpr std::span<const Stroke> eastLabel[]{east};
inline constexpr std::span<const Stroke> westLabel[]{west};

inline void Inscribe(Aevrynd::Scene3D& scene, Aevrynd::Vec3 anchor, Aevrynd::Vec2 size,
                     std::span<const std::span<const Stroke>> label, float side) {
    using namespace Aevrynd;
    constexpr Color paint{.94f,.83f,.61f,1.f};
    const float glyph = size.y * .78f;
    const float spacing = glyph * 1.17f;
    const float left = -(float(label.size()) - 1.f) * spacing * .5f;
    for (std::size_t i = 0; i < label.size(); ++i) {
        const auto point = [&](Vec2 uv) {
            return anchor + Vec3{-side * (left + float(i) * spacing + (uv.x-.5f)*glyph), (.5f-uv.y)*glyph, 0.f};
        };
        for (const auto& stroke : label[i]) {
            const auto a = point(stroke.start), b = point(stroke.end);
            const auto delta = b-a;
            const Vec3 thickness = Normalize(Cross(Vec3{0,0,side},delta)) * (glyph * .031f);
            scene.Triangle(a-thickness, b-thickness, b+thickness, paint);
            scene.Triangle(a-thickness, b+thickness, a+thickness, paint);
        }
    }
}

inline void Plate(Aevrynd::Scene3D& scene, Aevrynd::Vec3 center, Aevrynd::Vec2 size, float bodyDepth,
                  Aevrynd::Color backing, std::span<const std::span<const Stroke>> label) {
    using namespace Aevrynd;
    constexpr Color pin{.44f,.32f,.16f,1};
    for (const float side : {-1.f,1.f}) {
        const auto surface = center + Vec3{0,0,side*(bodyDepth*.5f+.006f)};
        scene.Box(surface,{size.x,size.y,.016f},backing);
        for (const float x : {-size.x*.44f,size.x*.44f})
            for (const float y : {-size.y*.36f,size.y*.36f})
                scene.Box(surface+Vec3{x,y,side*.012f},{.021f,.021f,.021f},pin);
        Inscribe(scene,surface+Vec3{0,0,side*.010f},size,label,side);
    }
}
}
