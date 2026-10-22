#pragma once
#include "MistwatchEncounter.h"
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/HeightFieldLineOfSight3D.h>

namespace Mistwatch {
// 包住目前野豬非對稱吻部的根部半徑；不是人物控制器的預設尺寸。
inline constexpr float EncounterBodyRadius=1.30f, EncounterBodyBottom=-.30f, EncounterBodyHeight=1.60f;
enum class EncounterMotionStatus { Applied, Blocked, Invalid };
struct EncounterMotionResult {
    EncounterMotionStatus status{EncounterMotionStatus::Invalid};
    Aevrynd::Vec3 feet{};
    unsigned collisions{},damage{};
};
inline bool EncounterBodyOverlaps(Aevrynd::Vec3 feet,std::span<const Aevrynd::BoxCollider3D> solids) noexcept {
    const auto center=feet+Aevrynd::Vec3{0,EncounterBodyBottom+EncounterBodyHeight*.5f,0};
    const Aevrynd::Vec3 extents{EncounterBodyRadius,EncounterBodyHeight*.5f,EncounterBodyRadius};
    for(const auto& box:solids){
        const auto low=box.center-box.extents-extents,high=box.center+box.extents+extents;
        if(center.x>low.x&&center.x<high.x&&center.y>low.y&&center.y<high.y&&center.z>low.z&&center.z<high.z)return true;
    }
    return false;
}
inline bool ValidEncounterWorld(std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain) noexcept {
    const auto& grid=terrain.Settings();
    if(!Session::Finite(grid.origin)||!std::isfinite(grid.cellSize)||grid.cellSize<=0||grid.width<2||grid.depth<2||
        terrain.SampleCount()!=std::size_t(grid.width)*grid.depth)return false;
    for(const auto& box:solids)if(!Session::Finite(box.center)||!Session::Finite(box.extents)||
        box.extents.x<0||box.extents.y<0||box.extents.z<0||!Session::Finite(box.center-box.extents)||
        !Session::Finite(box.center+box.extents))return false;
    return true;
}
inline bool EncounterTerrainContains(Aevrynd::Vec3 feet,const Aevrynd::HeightField3D& terrain) noexcept {
    return Session::Finite(feet)&&terrain.Contains(feet.x-EncounterBodyRadius,feet.z-EncounterBodyRadius)&&
        terrain.Contains(feet.x+EncounterBodyRadius,feet.z+EncounterBodyRadius);
}
inline bool EncounterSupported(Aevrynd::Vec3 feet,const Aevrynd::HeightField3D& terrain) noexcept {
    if(!EncounterTerrainContains(feet,terrain))return false;
    const auto& grid=terrain.Settings();
    if(!Session::Finite(grid.origin)||!std::isfinite(grid.cellSize)||grid.cellSize<=0)return false;
    const unsigned minX=unsigned(std::floor((feet.x-EncounterBodyRadius-grid.origin.x)/grid.cellSize));
    const unsigned minZ=unsigned(std::floor((feet.z-EncounterBodyRadius-grid.origin.z)/grid.cellSize));
    const unsigned maxX=(std::min)(grid.width-1,unsigned(std::ceil((feet.x+EncounterBodyRadius-grid.origin.x)/grid.cellSize)));
    const unsigned maxZ=(std::min)(grid.depth-1,unsigned(std::ceil((feet.z+EncounterBodyRadius-grid.origin.z)/grid.cellSize)));
    for(unsigned z=minZ;z<=maxZ;++z)for(unsigned x=minX;x<=maxX;++x)if(!std::isfinite(terrain.Height(x,z)))return false;
    const auto ground=terrain.SampleHeight(feet.x,feet.z);
    return std::isfinite(ground)&&std::abs(feet.y-ground)<=.0001f;
}
inline EncounterMotionResult ResolveEncounterMove(Aevrynd::Vec3 feet,Aevrynd::Vec3 delta,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain) noexcept {
    using namespace Aevrynd;
    EncounterMotionResult result;result.feet=feet;
    if(!ValidEncounterWorld(solids,terrain)||!Session::Finite(delta)||delta.y!=0||
        std::hypot(delta.x,delta.z)>.201f||!EncounterSupported(feet,terrain)||EncounterBodyOverlaps(feet,solids))return result;
    CharacterControllerSettings3D settings;
    settings.radius=EncounterBodyRadius;settings.height=EncounterBodyHeight;
    settings.maximumDirectDisplacement=4.f;settings.groundSnapDistance=0;
    const Vec3 bodyOffset{0,EncounterBodyBottom,0};
    CharacterController3D controller(settings);controller.Reset(feet+bodyOffset);
    const auto count=(std::max)(1u,static_cast<unsigned>(std::ceil(std::hypot(delta.x,delta.z)/.05f)));
    const auto part=delta*(1.f/float(count));
    bool blocked{};std::uint64_t tick{};
    for(unsigned substep=0;substep<count;++substep){
        const auto prior=controller.Position(),target=prior+part-bodyOffset;
        // 場外、陡崖或被障礙阻擋保留最後合法落點；不捏造地形高度。
        if(!EncounterTerrainContains(target,terrain)){blocked=true;break;}
        const float height=terrain.SampleHeight(target.x,target.z)+bodyOffset.y;
        if(!std::isfinite(height))return result;
        if(std::abs(height-prior.y)>.25f){blocked=true;break;}
        CharacterDirectDisplacementResult3D moved;
        // 不用 terrain overload 的碰撞後直接抬升；Y 修正也必須真正通過 swept-AABB。
        if(!controller.ApplyDirectDisplacementTick({++tick,{part.x,height-prior.y,part.z},0},solids,&moved))return result;
        result.collisions+=moved.collisionCount;
        if(moved.status==CharacterDirectDisplacementStatus3D::StartedOverlapping)return result;
        auto candidate=controller.Position();
        if(!EncounterTerrainContains(candidate-bodyOffset,terrain)){blocked=true;controller.Reset(prior);break;}
        const float actual=terrain.SampleHeight(candidate.x,candidate.z)+bodyOffset.y;
        if(!std::isfinite(actual))return result;
        CharacterDirectDisplacementResult3D corrected;
        if(!controller.ApplyDirectDisplacementTick({++tick,{0,actual-candidate.y,0},0},solids,&corrected))return result;
        result.collisions+=corrected.collisionCount;candidate=controller.Position();
        if(!EncounterSupported(candidate-bodyOffset,terrain)||EncounterBodyOverlaps(candidate-bodyOffset,solids)){
            blocked=true;controller.Reset(prior);break;
        }
        // direct result 把浮點剩餘量也標成 Blocked；此遊戲狀態只把真正碰撞算作障礙停步。
        blocked|=(moved.collisionCount>0&&moved.status==CharacterDirectDisplacementStatus3D::Blocked)||
            (corrected.collisionCount>0&&corrected.status==CharacterDirectDisplacementStatus3D::Blocked);
    }
    result.feet=controller.Position()-bodyOffset;
    result.status=blocked?EncounterMotionStatus::Blocked:EncounterMotionStatus::Applied;
    return result;
}
inline EncounterMotionResult TickEncounterWorld(Encounter& enemy,Aevrynd::Vec3 player,float dt,bool attack,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain) noexcept {
    EncounterMotionResult invalid;invalid.feet=enemy.feet;
    if(!enemy.Valid()||!Session::Finite(player)||!std::isfinite(dt)||dt<=0||dt>.1f)return invalid;
    if(!ValidEncounterWorld(solids,terrain))return invalid;
    // .65m 位於目前站立／蹲姿與野豬軀幹內；腳底高度隨跳躍改變，不以地面固定射線穿牆。
    const auto clear=[&](Aevrynd::Vec3 target,bool& malformed){
        const auto start=enemy.feet+Aevrynd::Vec3{0,.65f,0},end=target+Aevrynd::Vec3{0,.65f,0};
        for(const auto& box:solids)if(Aevrynd::SegmentIntersectsAabb3D(start,end,box.center,box.extents))return false;
        const auto status=Aevrynd::QueryHeightFieldLine3D(start,end,terrain);
        malformed|=status==Aevrynd::HeightFieldLineStatus3D::InvalidInput;
        return status==Aevrynd::HeightFieldLineStatus3D::Clear;
    };
    bool malformed{},playerClear=true,strikeClear=true;
    if(enemy.phase==EnemyPhase::Windup||std::hypot(player.x-enemy.feet.x,player.z-enemy.feet.z)<=2.5f)playerClear=clear(player,malformed);
    if(enemy.phase==EnemyPhase::Windup)strikeClear=clear(enemy.strikeTarget,malformed);
    if(malformed)return invalid;
    auto next=enemy;const unsigned damage=next.Tick(player,dt,attack,playerClear,strikeClear);
    auto result=ResolveEncounterMove(enemy.feet,{next.feet.x-enemy.feet.x,0,next.feet.z-enemy.feet.z},solids,terrain);
    // 非法世界／狀態整步拒絕；安全碰撞停步仍推進既有蓄力、反擊及冷卻。
    if(result.status==EncounterMotionStatus::Invalid)return result;
    next.feet=result.feet;enemy=next;result.damage=damage;return result;
}
}

namespace Mistwatch {
// 舊公開入口逐字保留；新入口只在本作候選逐段加驗玩家與落點。
template<class SegmentCheck>
inline EncounterMotionResult ResolveEncounterMoveChecked(Aevrynd::Vec3 feet,Aevrynd::Vec3 delta,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain,SegmentCheck&& check) noexcept {
    using namespace Aevrynd;
    EncounterMotionResult result;result.feet=feet;
    if(!ValidEncounterWorld(solids,terrain)||!Session::Finite(delta)||delta.y!=0||
        std::hypot(delta.x,delta.z)>.201f||!EncounterSupported(feet,terrain)||EncounterBodyOverlaps(feet,solids))return result;
    CharacterControllerSettings3D settings;
    settings.radius=EncounterBodyRadius;settings.height=EncounterBodyHeight;
    settings.maximumDirectDisplacement=4.f;settings.groundSnapDistance=0;
    const Vec3 bodyOffset{0,EncounterBodyBottom,0};
    CharacterController3D controller(settings);controller.Reset(feet+bodyOffset);
    const auto count=(std::max)(1u,static_cast<unsigned>(std::ceil(std::hypot(delta.x,delta.z)/.05f)));
    const auto part=delta*(1.f/float(count));
    bool blocked{};std::uint64_t tick{};
    for(unsigned substep=0;substep<count;++substep){
        const auto beforeStep=controller;
        const auto prior=controller.Position(),target=prior+part-bodyOffset;
        // 場外、陡崖或被障礙阻擋保留最後合法落點；不捏造地形高度。
        if(!EncounterTerrainContains(target,terrain)){blocked=true;break;}
        const float height=terrain.SampleHeight(target.x,target.z)+bodyOffset.y;
        if(!std::isfinite(height))return result;
        if(std::abs(height-prior.y)>.25f){blocked=true;break;}
        CharacterDirectDisplacementResult3D moved;
        // 不用 terrain overload 的碰撞後直接抬升；Y 修正也必須真正通過 swept-AABB。
        if(!controller.ApplyDirectDisplacementTick({++tick,{part.x,height-prior.y,part.z},0},solids,&moved))return result;
        result.collisions+=moved.collisionCount;
        if(moved.status==CharacterDirectDisplacementStatus3D::StartedOverlapping)return result;
        auto candidate=controller.Position();
        if(!check(prior-bodyOffset,candidate-bodyOffset,0u)){blocked=true;controller=beforeStep;break;}
        if(!EncounterTerrainContains(candidate-bodyOffset,terrain)){blocked=true;controller.Reset(prior);break;}
        const float actual=terrain.SampleHeight(candidate.x,candidate.z)+bodyOffset.y;
        if(!std::isfinite(actual))return result;
        CharacterDirectDisplacementResult3D corrected;
        if(!controller.ApplyDirectDisplacementTick({++tick,{0,actual-candidate.y,0},0},solids,&corrected))return result;
        result.collisions+=corrected.collisionCount;const auto beforeCorrection=candidate;candidate=controller.Position();
        if(!check(beforeCorrection-bodyOffset,candidate-bodyOffset,1u)){blocked=true;controller=beforeStep;break;}
        if(!EncounterSupported(candidate-bodyOffset,terrain)||EncounterBodyOverlaps(candidate-bodyOffset,solids)){
            blocked=true;controller.Reset(prior);break;
        }
        if(!check(prior-bodyOffset,candidate-bodyOffset,2u))return result;
        // direct result 把浮點剩餘量也標成 Blocked；此遊戲狀態只把真正碰撞算作障礙停步。
        blocked|=(moved.collisionCount>0&&moved.status==CharacterDirectDisplacementStatus3D::Blocked)||
            (corrected.collisionCount>0&&corrected.status==CharacterDirectDisplacementStatus3D::Blocked);
    }
    result.feet=controller.Position()-bodyOffset;
    result.status=blocked?EncounterMotionStatus::Blocked:EncounterMotionStatus::Applied;
    return result;
}
}
