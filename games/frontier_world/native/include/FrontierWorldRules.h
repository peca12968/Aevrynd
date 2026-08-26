#pragma once
#include <Aevrynd/Math.h>
#include <cmath>
#include <cstdint>

namespace FrontierWorldRules {
enum class BuildingKind {Foundation,Wall,Roof};
struct ResourceCost {int wood{},stone{};};
struct ChunkCoordinate {int x{},z{};constexpr bool operator==(const ChunkCoordinate&)const=default;};
inline ResourceCost Cost(BuildingKind kind){switch(kind){case BuildingKind::Foundation:return{8,6};case BuildingKind::Wall:return{12,2};case BuildingKind::Roof:return{14,0};}return{};}
inline bool CanAfford(int wood,int stone,BuildingKind kind){const auto cost=Cost(kind);return wood>=cost.wood&&stone>=cost.stone;}
inline Aevrynd::Vec3 SnapBuildPosition(Aevrynd::Vec3 position,float grid=2.5f){grid=grid<.1f?.1f:grid;return{std::round(position.x/grid)*grid,std::round(position.y/grid)*grid,std::round(position.z/grid)*grid};}
inline ChunkCoordinate ChunkAt(Aevrynd::Vec3 position,float size=32.f){return{int(std::floor(position.x/size)),int(std::floor(position.z/size))};}
inline uint32_t ChunkSeed(ChunkCoordinate chunk,uint32_t worldSeed=0x4a17u){uint32_t value=worldSeed^uint32_t(chunk.x)*0x9e3779b9u^uint32_t(chunk.z)*0x85ebca6bu;value^=value>>16;value*=0x7feb352du;value^=value>>15;return value;}
inline bool CanMeleeHit(Aevrynd::Vec3 attacker,float yaw,Aevrynd::Vec3 target,float range=2.8f,float minimumDot=.25f){const Aevrynd::Vec3 delta=target-attacker;const float distance=Aevrynd::Length(delta);if(distance>.001f&&distance<=range){const Aevrynd::Vec3 forward{std::sin(yaw),0,std::cos(yaw)},direction=Aevrynd::Normalize(Aevrynd::Vec3{delta.x,0,delta.z});return Aevrynd::Dot(forward,direction)>=minimumDot;}return false;}
inline int ExperienceForLevel(int level){return 60+(level<1?1:level)*40;}
}
