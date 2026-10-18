#pragma once
#include "MistwatchSession.h"
#include "MistwatchEncounterMotion.h"
#include "MistwatchEvasion.h"
#include "MistwatchCamera.h"
#include "MistwatchBridgeRepairPrompt.h"
#include <limits>
#include <string>

namespace Mistwatch {
inline constexpr unsigned NoTrackedRelay=3;
inline bool CanTrackRelay(const Session& session,unsigned index) {
    return index<3&&session.Valid()&&session.Alive()&&session.CurrentStage()==Stage::Relays&&
        !(session.relayLinked&(1u<<index));
}
inline unsigned CycleTrackedRelay(const Session& session,unsigned from,int direction) {
    if(direction!=1&&direction!=-1)return NoTrackedRelay;
    const unsigned start=from<3?from:(direction==1?2u:0u);
    for(unsigned step=1;step<=3;++step){
        const unsigned next=unsigned((int(start)+direction*int(step)+6)%3);
        if(CanTrackRelay(session,next))return next;
    }
    return NoTrackedRelay;
}
enum class ObjectiveBearing { AtTarget, Ahead, Right, Behind, Left };
struct ObjectiveGuide {
    bool valid{}, hasTarget{}, useReady{}, counterReady{}, evadeReady{}, tuneReady{};
    bool earlyToolsReady{};
    BridgeRepairIntent bridgeIntent{BridgeRepairIntent::None};
    unsigned index{};
    // index/useReady/tuneReady 仍是近場操作；此欄只識別玩家確認的導航目的地。
    unsigned navigationRelayIndex{NoTrackedRelay};bool relayTrackingActive{};
    Aevrynd::Vec3 target{};
    double distance{}, relativeRadians{};
    ObjectiveBearing bearing{ObjectiveBearing::AtTarget};
    std::string objective{"Objective guide unavailable"}, navigation, instruction, context;
};
namespace ObjectiveGuideDetail {
inline unsigned Count(unsigned bits) { return unsigned(bool(bits&1))+unsigned(bool(bits&2))+unsigned(bool(bits&4)); }
inline const char* BearingName(ObjectiveBearing bearing) {
    switch(bearing) {
    case ObjectiveBearing::AtTarget:return "at target";
    case ObjectiveBearing::Ahead:return "ahead";
    case ObjectiveBearing::Right:return "right";
    case ObjectiveBearing::Behind:return "behind";
    case ObjectiveBearing::Left:return "left";
    }
    return "unknown";
}
inline bool CounterLineClear(const Session& session,const Encounter& enemy,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain) {
    if(!ValidEncounterWorld(solids,terrain))return false;
    const auto start=enemy.feet+Aevrynd::Vec3{0,.65f,0},end=session.feet+Aevrynd::Vec3{0,.65f,0};
    for(const auto& box:solids)if(Aevrynd::SegmentIntersectsAabb3D(start,end,box.center,box.extents))return false;
    return Aevrynd::QueryHeightFieldLine3D(start,end,terrain)==Aevrynd::HeightFieldLineStatus3D::Clear;
}
}

// 純展示資料：座標、已完成旗標及近場操作都讀正式狀態，不寫任務、時間或控制器。
inline ObjectiveGuide BuildObjectiveGuide(const Session& session,const Encounter& enemy,const Evasion& evasion,
    const Aevrynd::Camera3D& renderedCamera,bool crouched,bool grounded,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain,
    unsigned preferredRelay=NoTrackedRelay) {
    using namespace ObjectiveGuideDetail;
    ObjectiveGuide guide;
    if(!session.Valid())return guide;
    if(!session.Alive()) {
        guide.valid=true;guide.objective="Expedition failed";
        guide.instruction="R: retry with completed objectives kept";guide.context="HP 0";return guide;
    }
    if(session.CurrentStage()==Stage::Complete) {
        guide.valid=true;guide.objective="Expedition complete";
        guide.instruction="Rescue signal active | R: new expedition";guide.context="All objectives complete";return guide;
    }
    if(!Session::Finite(renderedCamera.position)||!Session::Finite(renderedCamera.target))return guide;
    const double cx=double(renderedCamera.position.x)-renderedCamera.target.x;
    const double cz=double(renderedCamera.position.z)-renderedCamera.target.z;
    if(!std::isfinite(cx)||!std::isfinite(cz)||std::hypot(cx,cz)<=.001)return guide;
    const double cameraYaw=std::atan2(cx,-cz);
    std::string name;
    const auto select=[&](Aevrynd::Vec3 target,const char* label,unsigned index=0) {
        guide.hasTarget=true;guide.target=target;guide.index=index;name=label;
    };
    const auto nearest=[&](const auto& points,unsigned completed,const auto& names) {
        double closest=(std::numeric_limits<double>::infinity)();
        for(unsigned index=0;index<points.size();++index)if(!(completed&(1u<<index))) {
            const double distance=std::hypot(double(points[index].x)-session.feet.x,double(points[index].z)-session.feet.z);
            // 等距保留較小作者索引，避免鏡頭或刷新造成目標跳動。
            if(distance<closest){closest=distance;select(points[index],names[index],index);}
        }
    };
    auto candidate=session;
    switch(session.CurrentStage()) {
    case Stage::Supplies: {
        constexpr std::array names{"Supply crate 1","Supply crate 2","Supply crate 3"};
        nearest(Session::supplies,session.collected,names);
        guide.objective="Find supplies for the bridge expedition";
        guide.useReady=candidate.Collect(guide.index);
        // 提前取工具只旁記正式門檻，不改補給箱的遠程目標或 useReady 語意。
        auto toolsProbe=session;
        guide.earlyToolsReady=toolsProbe.CollectRepairMaterial(0,crouched);
        guide.instruction=guide.useReady?"E: collect supplies":"Follow the main dirt trail; approach the crate";
        guide.context="Supplies "+std::to_string(Count(session.collected))+" / 3 | Repair items "+std::to_string(Count(session.repairMaterials))+" / 3";
        break;
    }
    case Stage::RepairMaterials: {
        constexpr std::array names{"West shelter tools","South-east timber","West-bank timber"};
        nearest(Session::repairSites,session.repairMaterials,names);
        guide.objective="Find tools and two timber caches";
        guide.useReady=candidate.CollectRepairMaterial(guide.index,crouched);
        guide.instruction=guide.useReady?"E: collect repair item":guide.index==0?
            "Follow main trail; east shelter opening, CTRL crouch inside":"Follow the south-bank trail to the timber cache";
        guide.context=std::string("Tools ")+((session.repairMaterials&1)?"OK":"missing")+" | Timber "+std::to_string(Count(session.repairMaterials&6))+" / 2";
        break;
    }
    case Stage::Bridge:
        select(Session::bridge,"Bridge crossing");guide.objective="Repair the bridge";
        guide.bridgeIntent=EvaluateBridgeRepairIntent(session);
        // Session 允許按 E 嘗試，不代表新世界／鏡頭碰撞已預檢；絕不宣告 useReady。
        guide.useReady=false;
        guide.instruction=guide.bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck?
            "E: attempt repair; camera clearance checked on use":"Follow the main dirt trail to the bridge";
        guide.context="Tools and timber ready | Repair not prevalidated";break;
    case Stage::Encounter: {
        if(!enemy.Valid()||!Session::Finite(evasion.direction)||!std::isfinite(evasion.cooldown)||!std::isfinite(evasion.remaining)||
            evasion.cooldown<0||evasion.cooldown>Evasion::Cooldown||evasion.remaining<0||evasion.remaining>Evasion::Duration)return ObjectiveGuide{};
        select(enemy.feet,"Wild boar");guide.objective="Evade boar strikes, then counter the opening";
        const double range=std::hypot(double(enemy.feet.x)-session.feet.x,double(enemy.feet.z)-session.feet.z);
        guide.counterReady=enemy.phase==EnemyPhase::Recovery&&!enemy.countered&&enemy.playerCooldown<=0&&
            evasion.remaining<=0&&range<=2.5&&std::abs(session.feet.y-enemy.feet.y)<2.f&&CounterLineClear(session,enemy,solids,terrain);
        guide.evadeReady=grounded&&!crouched&&evasion.cooldown<=0&&evasion.remaining<=0;
        if(enemy.phase==EnemyPhase::Windup)guide.instruction="Leave the marked strike circle";
        else if(enemy.phase==EnemyPhase::Recovery&&enemy.countered)guide.instruction="Counter used; wait for the next opening";
        else if(guide.counterReady)guide.instruction="F: counter once during this opening";
        else if(enemy.phase==EnemyPhase::Recovery)guide.instruction="Opening active; approach with a clear path";
        else guide.instruction="Follow the main trail; watch for the strike circle";
        if(guide.evadeReady)guide.instruction+=" | Q: evade";
        else guide.instruction+=" | Q unavailable: cooldown, crouch or airborne";
        guide.context="Boar HP "+std::to_string(enemy.health)+" / 3";break;
    }
    case Stage::Fuse:
        guide.objective="Read ranger log, unlock workshop cabinet";
        if(!session.fuseClueRead) {
            select(Session::rangerLog,"North ranger log");guide.useReady=candidate.ReadFuseClue();
            guide.instruction=guide.useReady?"E: examine ranger log":"Follow the settlement branch; enter the hut west doorway";
            guide.context="Cabinet instructions not read";
        } else {
            select(Session::fuse,"West workshop cabinet");guide.useReady=candidate.CollectFuse();
            guide.tuneReady=candidate.CycleFuseDial(0);
            guide.instruction=guide.useReady?"E: open cabinet":guide.tuneReady?
                "1 / 2 / 3: turn cabinet dials; match the ranger log":"Follow the settlement branch; enter the workshop east opening";
            // 只在玩家實際讀到日誌後展示現有線索，不預告答案。
            guide.context="Ranger log: 2 - 0 - 3";
            if(session.NearInterior(Session::fuse))guide.context+=" | Dials "+std::to_string(session.FuseDial(0))+" - "+
                std::to_string(session.FuseDial(1))+" - "+std::to_string(session.FuseDial(2));
        }
        break;
    case Stage::Generator:
        select(Session::generator,"East generator");guide.objective="Restore power at the east generator";
        guide.useReady=candidate.RepairGenerator();guide.tuneReady=candidate.ToggleCircuit(0);
        guide.instruction=guide.useReady?"E: repair generator":guide.tuneReady?
            "1: A+B | 2: B+C | 3: C | Light all lamps":"Follow the settlement dirt branch to the east generator";
        guide.context="Circuits "+std::to_string(Count(session.circuit))+" / 3 lit";break;
    case Stage::Relays: {
        constexpr std::array names{"West ridge relay","East lookout relay","North-west relay"};
        nearest(Session::relaySites,session.relayLinked,names);guide.objective="Restore three ridge radio relays";
        guide.useReady=candidate.ConnectRelay(guide.index);guide.tuneReady=candidate.TuneRelay(guide.index,1);
        guide.instruction=guide.useReady?"GREEN LOCK | E: link relay":guide.tuneReady?
            "1 left / 2 right | Find green signal lock":"Follow the main trail, then the uphill dirt branch";
        guide.context="Radio links "+std::to_string(Count(session.relayLinked))+" / 3";
        // SIGNAL 是正式玩家可見回饋，不輸出目標 sector 或旋轉次數。
        if(session.NearRelay(guide.index))guide.context+=" | SIGNAL "+std::to_string(session.RelaySignal(guide.index));
        break;
    }
    case Stage::Beacon:
        select(Session::beacon,"Rescue beacon");guide.objective="Light the rescue beacon";
        guide.useReady=candidate.LightBeacon();guide.instruction=guide.useReady?"E: activate rescue beacon":"Return to the main dirt trail and follow the beacon bearing";
        guide.context="Power restored | Radio links 3 / 3";break;
    case Stage::Complete:break;
    }
    if(CanTrackRelay(session,preferredRelay)) {
        constexpr std::array names{"West ridge relay","East lookout relay","North-west relay"};
        guide.target=Session::relaySites[preferredRelay];name=names[preferredRelay];
        guide.navigationRelayIndex=preferredRelay;guide.relayTrackingActive=true;
    }
    if(!guide.hasTarget||!Session::Finite(guide.target))return ObjectiveGuide{};
    const double dx=double(guide.target.x)-session.feet.x,dz=double(guide.target.z)-session.feet.z;
    guide.distance=std::hypot(dx,dz);
    if(!std::isfinite(guide.distance))return ObjectiveGuide{};
    if(guide.distance>.001) {
        // 取呼叫者的實際渲染鏡頭；正常模式同 WASD 鏡頭，診斷圖只代表畫面方位。
        const double right=dx*std::cos(cameraYaw)+dz*std::sin(cameraYaw);
        const double forward=-dx*std::sin(cameraYaw)+dz*std::cos(cameraYaw);
        guide.relativeRadians=std::atan2(right,forward);
        constexpr double quarter=.7853981633974483;
        guide.bearing=std::abs(guide.relativeRadians)<=quarter?ObjectiveBearing::Ahead:
            std::abs(guide.relativeRadians)>=3*quarter?ObjectiveBearing::Behind:
            guide.relativeRadians>0?ObjectiveBearing::Right:ObjectiveBearing::Left;
    }
    guide.navigation=name+" | "+BearingName(guide.bearing)+" | "+std::to_string(unsigned(std::ceil(guide.distance)))+" m";
    guide.valid=true;return guide;
}
}
