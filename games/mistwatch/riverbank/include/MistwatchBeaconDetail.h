#pragma once
#include "MistwatchSession.h"
#include <Aevrynd/Scene3D.h>
#include <array>
#include <optional>

namespace Mistwatch {
struct BeaconPart {
    Aevrynd::Vec3 center{},size{};
    Aevrynd::Material3D material{};
};
struct BeaconDetail {
    std::array<BeaconPart,23> parts{};
    bool lit{};
};
inline constexpr std::size_t BeaconLensPart=13,BeaconStatusPart=10;

// 純渲染作者：不寫 Session、不新增碰撞或光源；所有部件留在舊 .7m 足跡內。
inline std::optional<BeaconDetail> CreateBeaconDetail(const Session& session,
    Aevrynd::Vec3 anchor=Session::beacon) {
    using namespace Aevrynd;
    if(!session.Valid()||!Session::Finite(anchor)||
        std::abs(anchor.x)>1000||std::abs(anchor.y)>1000||std::abs(anchor.z)>1000)return std::nullopt;
    BeaconDetail result;result.lit=session.beaconLit;
    std::size_t count{};
    const Material3D base{{.22f,.24f,.22f,1},{.87f,.05f,0,1}};
    const Material3D steel{{.31f,.34f,.35f,1},{.43f,.72f,0,1}};
    const Material3D darkSteel{{.11f,.14f,.15f,1},{.65f,.58f,0,1}};
    const Material3D bolts{{.45f,.47f,.46f,1},{.35f,.75f,0,1}};
    const Material3D panel{{.055f,.07f,.075f,1},{.78f,.12f,0,1}};
    const Material3D lens{session.beaconLit?Color{.98f,.48f,.12f,1}:Color{.12f,.075f,.04f,1},
        {.23f,0,session.beaconLit?.9f:0,1}};
    const Material3D status{session.beaconLit?Color{.25f,.7f,.32f,1}:Color{.07f,.035f,.025f,1},
        {.45f,0,session.beaconLit?.5f:0,1}};
    const auto add=[&](Vec3 center,Vec3 size,Material3D material){
        result.parts[count++]={anchor+center,size,material};
    };
    // 向東偏18cm的窄機柱保留原任務原點；不再用4m方柱遮住中央角色。
    add({.18f,.075f,.10f},{.30f,.15f,.30f},base);
    for(float x:{-.11f,.11f})for(float z:{-.11f,.11f})
        add({.18f+x,.165f,.10f+z},{.035f,.035f,.035f},bolts);
    add({.18f,.97f,.10f},{.085f,1.64f,.085f},steel);
    for(float y:{.19f,1.58f})add({.18f,y,.10f},{.14f,.065f,.14f},darkSteel);
    add({.18f,.72f,.04f},{.18f,.26f,.18f},darkSteel);
    add({.18f,.72f,-.055f},{.14f,.19f,.012f},panel);
    add({.18f,.76f,-.0645f},{.065f,.025f,.008f},status);
    add({.18f,1.785f,.10f},{.24f,.07f,.24f},darkSteel);
    add({.18f,2.065f,.10f},{.30f,.07f,.30f},darkSteel);
    add({.18f,1.925f,.10f},{.20f,.21f,.20f},lens);
    // 四根護柱及四條護帶有實際厚度與深度遮擋，不用透明假玻璃。
    for(float x:{-.13f,.13f})for(float z:{-.13f,.13f})
        add({.18f+x,1.925f,.10f+z},{.020f,.29f,.020f},steel);
    for(float x:{-.13f,.13f})add({.18f+x,1.925f,.10f},{.018f,.018f,.26f},steel);
    for(float z:{-.13f,.13f})add({.18f,1.925f,.10f+z},{.26f,.018f,.018f},steel);
    add({.18f,2.20f,.10f},{.025f,.20f,.025f},steel);
    return result;
}

inline bool SubmitBeaconDetail(Aevrynd::Scene3D& scene,const Session& session) {
    const auto detail=CreateBeaconDetail(session);
    if(!detail)return false;
    for(const auto& part:detail->parts)
        if(!scene.MaterialBox(part.center,part.size,part.material))return false;
    return true;
}
}
