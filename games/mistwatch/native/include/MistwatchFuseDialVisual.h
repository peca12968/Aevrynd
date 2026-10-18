#pragma once
#include "MistwatchSession.h"
#include <array>
#include <cstddef>
#include <optional>

namespace Mistwatch {
struct FuseDialTriangle { Aevrynd::Vec3 a{},b{},c{}; Aevrynd::Color color{}; };
struct FuseDialVisual {
    static constexpr std::size_t TriangleCount=47;
    Aevrynd::Vec3 centre{},panelSize{.03f,.21f,.21f},pointerTip{};
    Aevrynd::Color panelColor{.1f,.14f,.15f,1},indicatorColor{};
    std::array<Aevrynd::Vec3,4> tickCentres{};
    std::array<FuseDialTriangle,TriangleCount> triangles{};
    unsigned value{};
};
namespace FuseDialVisualDetail {
// 由櫃面 +X 看：u 向右（-Z），v 向上（+Y）。四值順時針，不參考解答。
inline constexpr std::array<Aevrynd::Vec2,4> Directions{{{0,1},{1,0},{0,-1},{-1,0}}};
inline Aevrynd::Vec3 Point(Aevrynd::Vec3 centre,float depth,Aevrynd::Vec2 point) {
    return {centre.x+depth,centre.y+point.y,centre.z-point.x};
}
inline void Rect(FuseDialVisual& visual,std::size_t& next,float depth,Aevrynd::Vec2 centre,
    Aevrynd::Vec2 halfSize,Aevrynd::Color color) {
    const auto a=Point(visual.centre,depth,centre+Aevrynd::Vec2{-halfSize.x,-halfSize.y});
    const auto b=Point(visual.centre,depth,centre+Aevrynd::Vec2{halfSize.x,-halfSize.y});
    const auto c=Point(visual.centre,depth,centre+halfSize);
    const auto d=Point(visual.centre,depth,centre+Aevrynd::Vec2{-halfSize.x,halfSize.y});
    // 所有前表面三角形均朝 +X，不用雙面材質遮掩繞序。
    visual.triangles[next++]={a,b,c,color};visual.triangles[next++]={a,c,d,color};
}
inline void Digit(FuseDialVisual& visual,std::size_t& next,unsigned digit,Aevrynd::Vec2 centre,
    Aevrynd::Color color) {
    // 七段數字 0..3 是輸入值刻度，並非正確答案或逐輪解鎖判定。
    constexpr std::array<unsigned,4> masks{0x3f,0x06,0x5b,0x4f};
    constexpr std::array<Aevrynd::Vec2,7> offsets{{{0,.012f},{.006f,.006f},{.006f,-.006f},
        {0,-.012f},{-.006f,-.006f},{-.006f,.006f},{0,0}}};
    for(unsigned segment=0;segment<7;++segment)if(masks[digit]&(1u<<segment)) {
        const bool horizontal=segment==0||segment==3||segment==6;
        Rect(visual,next,.020f,centre+offsets[segment],horizontal?
            Aevrynd::Vec2{.006f,.0012f}:Aevrynd::Vec2{.0012f,.0048f},color);
    }
}
}
inline std::optional<std::array<FuseDialVisual,3>> CreateFuseDialVisuals(
    const Session& session,Aevrynd::Vec3 cabinetCentre) {
    // 有界作者域；不把 NaN／極大世界座標送進圖形管線，不修改呼叫者。
    if(!session.Valid()||!Session::Finite(cabinetCentre)||std::abs(cabinetCentre.x)>1000.f||
        std::abs(cabinetCentre.y)>1000.f||std::abs(cabinetCentre.z)>1000.f)return std::nullopt;
    std::array<FuseDialVisual,3> result{};
    const Aevrynd::Color indicator=session.fuseCollected?Aevrynd::Color{.2f,1,.3f,1}:
        Aevrynd::Color{1,.65f,.12f,1};
    const Aevrynd::Color markings{.86f,.91f,.88f,1};
    for(unsigned wheel=0;wheel<3;++wheel) {
        auto& visual=result[wheel];
        visual.centre=cabinetCentre+Aevrynd::Vec3{.36f,.25f,-.25f+float(wheel)*.25f};
        visual.indicatorColor=indicator;visual.value=session.FuseDial(wheel);
        std::size_t next{};
        for(unsigned value=0;value<4;++value) {
            const auto direction=FuseDialVisualDetail::Directions[value];
            visual.tickCentres[value]=FuseDialVisualDetail::Point(visual.centre,.020f,direction*.062f);
            FuseDialVisualDetail::Rect(visual,next,.020f,direction*.062f,value%2?
                Aevrynd::Vec2{.007f,.002f}:Aevrynd::Vec2{.002f,.007f},markings);
            FuseDialVisualDetail::Digit(visual,next,value,direction*.083f,markings);
        }
        const auto direction=FuseDialVisualDetail::Directions[visual.value];
        const Aevrynd::Vec2 side{-direction.y,direction.x};
        visual.pointerTip=FuseDialVisualDetail::Point(visual.centre,.024f,direction*.053f);
        visual.triangles[next++]={visual.pointerTip,
            FuseDialVisualDetail::Point(visual.centre,.024f,side*.009f),
            FuseDialVisualDetail::Point(visual.centre,.024f,side*-.009f),indicator};
        FuseDialVisualDetail::Rect(visual,next,.025f,{0,0},{.004f,.004f},indicator);
        if(next!=FuseDialVisual::TriangleCount)return std::nullopt;
    }
    return result;
}
}
