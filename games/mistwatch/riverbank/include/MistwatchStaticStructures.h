#pragma once

#include "MistwatchSession.h"
#include <Aevrynd/CollisionWorld3D.h>
#include <vector>

namespace Mistwatch {
inline std::vector<Aevrynd::BoxCollider3D> WorkshopSolids() {
    const auto p=Session::fuse;
    return {{p+Aevrynd::Vec3{0,2.7f,0},{2.5f,.15f,2.f}},
            {p+Aevrynd::Vec3{-2.4f,1.3f,0},{.1f,1.3f,2.f}},
            {p+Aevrynd::Vec3{0,1.3f,-1.9f},{2.5f,1.3f,.1f}},
            {p+Aevrynd::Vec3{0,1.3f,1.9f},{2.5f,1.3f,.1f}}};
}
inline Aevrynd::BoxCollider3D GeneratorSolid() {
    return {Session::generator+Aevrynd::Vec3{0,.6f,0},{.85f,.6f,.6f}};
}
inline Aevrynd::BoxCollider3D FuseCaseSolid() {
    return {Session::fuse+Aevrynd::Vec3{-.9f,.45f,0},{.35f,.45f,.4f}};
}
inline std::vector<Aevrynd::BoxCollider3D> RangerHutSolids() {
    const auto p=Session::rangerLog;
    // 西側朝向主路，東側為實牆，南北窗戶由同一組板材／立柱碰撞體構成。
    std::vector<Aevrynd::BoxCollider3D> result{
        {p+Aevrynd::Vec3{0,2.7f,0},{2.5f,.15f,3.f}},
        {p+Aevrynd::Vec3{2.4f,1.3f,0},{.1f,1.3f,3.f}},
        {p+Aevrynd::Vec3{.9f,.5f,0},{.35f,.5f,.8f}}};
    for(float z:{-2.9f,2.9f}) {
        result.push_back({p+Aevrynd::Vec3{0,.45f,z},{2.5f,.45f,.1f}});
        result.push_back({p+Aevrynd::Vec3{0,2.4f,z},{2.5f,.2f,.1f}});
        for(float x:{-2.3f,0.f,2.3f})result.push_back({p+Aevrynd::Vec3{x,1.55f,z},{.15f,.65f,.1f}});
    }
    return result;
}

inline std::vector<Aevrynd::BoxCollider3D> RepairShelterSolids() {
    const auto p=Session::repairSites[0];
    // 東側入口開放；屋頂下緣 1.45 公尺，蹲姿 1.4 可進，站姿 1.8 被阻擋。
    return {{p+Aevrynd::Vec3{0,1.55f,0},{2.5f,.1f,2.f}},
        {p+Aevrynd::Vec3{-2.4f,.7f,0},{.1f,.7f,2.f}},
        {p+Aevrynd::Vec3{0,.7f,-1.9f},{2.5f,.7f,.1f}},
        {p+Aevrynd::Vec3{0,.7f,1.9f},{2.5f,.7f,.1f}},
        {p+Aevrynd::Vec3{0,.23f,0},{.3f,.23f,.2f}}};
}
}
