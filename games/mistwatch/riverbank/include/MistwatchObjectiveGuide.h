#pragma once
#include "MistwatchSession.h"
#include "MistwatchEncounterMotion.h"
#include "MistwatchEvasion.h"
#include "MistwatchCamera.h"
#include "MistwatchBridgeRepairPrompt.h"
#include <limits>
#include <optional>
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
enum class CounterOpportunity { Closed, Used, Evading, Cooldown, TooFar, HeightDifference, Obstructed, Ready };
struct ObjectiveGuide {
    bool valid{}, hasTarget{}, useReady{}, counterReady{}, evadeReady{}, tuneReady{};
    bool shelterApproach{}, settlementApproach{}, rangerDeskApproach{};
    std::optional<unsigned> earlyRepairIndex;
    BridgeRepairIntent bridgeIntent{BridgeRepairIntent::None};
    CounterOpportunity counterOpportunity{CounterOpportunity::Closed};
    EvasionAvailability evadeAvailability{EvasionAvailability::Unavailable};
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
inline bool ShelterWallCrossed(Aevrynd::Vec3 from,Aevrynd::Vec3 to,
    const Aevrynd::BoxCollider3D& wall,float radius) {
    const auto extents=wall.extents+Aevrynd::Vec3{radius,0,radius};
    const auto sameOutsideFace=[](float start,float end,float center,float extent){
        const double lower=double(center)-extent,upper=double(center)+extent;
        return (start<=lower&&end<=lower)||(start>=upper&&end>=upper);
    };
    // 貼著外側邊界平移或離開不算穿牆。跨向內側仍使用完整線段碰撞。
    if(sameOutsideFace(from.x,to.x,wall.center.x,extents.x)||
       sameOutsideFace(from.z,to.z,wall.center.z,extents.z))return false;
    return Aevrynd::SegmentIntersectsAabb3D(from,to,wall.center,extents);
}
inline bool BuildingWallClear(Aevrynd::Vec3 from,Aevrynd::Vec3 to,float footHeight,
    std::span<const Aevrynd::BoxCollider3D> walls,float radius) {
    // 以建築基準高度檢查牆面。其他地物仍由實際碰撞處理。
    from.y=to.y=footHeight+.65f;
    for(const auto& wall:walls)
        if(ShelterWallCrossed(from,to,wall,radius))return false;
    return true;
}
// 使用結構原有的牆與角色半徑，找通往開口一側且不穿牆的下一個轉角。
inline std::optional<Aevrynd::Vec3> BuildingApproach(Aevrynd::Vec3 feet,float radius,Aevrynd::Vec3 target,
    const Aevrynd::BoxCollider3D& roof,std::span<const Aevrynd::BoxCollider3D> walls,float entrySide) {
    if(!std::isfinite(radius)||radius<=0)return std::nullopt;
    constexpr float cornerMargin=.35f;
    const float xClearance=roof.extents.x+radius+cornerMargin;
    const float zClearance=roof.extents.z+radius+cornerMargin;
    const auto wallClear=[&](Aevrynd::Vec3 from,Aevrynd::Vec3 to) {
        return BuildingWallClear(from,to,target.y,walls,radius);
    };
    if(wallClear(feet,target))return std::nullopt;
    const Aevrynd::Vec3 entrance{roof.center.x+entrySide*xClearance,target.y,roof.center.z};
    std::optional<Aevrynd::Vec3> next;
    double shortest=(std::numeric_limits<double>::infinity)();
    const auto distance=[](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return std::hypot(double(a.x)-b.x,double(a.z)-b.z);};
    for(float side:{-1.f,1.f}){
        const float z=roof.center.z+side*zClearance;
        const std::array route{entrance,Aevrynd::Vec3{entrance.x,target.y,z},
            Aevrynd::Vec3{roof.center.x-entrySide*xClearance,target.y,z}};
        double remaining=0;
        for(unsigned i=0;i<route.size();++i){
            if(i)remaining+=distance(route[i],route[i-1]);
            const double leg=distance(feet,route[i]);
            const double length=leg+remaining;
            if(leg>.01&&length<shortest&&wallClear(feet,route[i])){
                shortest=length;next=route[i];
            }
        }
    }
    return next;
}
// 入口導引無法繞過室內桌子時，逐段驗證桌角至日誌的通路。
inline std::optional<Aevrynd::Vec3> RangerDeskApproach(Aevrynd::Vec3 feet,float radius,
    Aevrynd::Vec3 target,const Aevrynd::BoxCollider3D& roof,const Aevrynd::BoxCollider3D& desk,
    std::span<const Aevrynd::BoxCollider3D> walls) {
    if(!std::isfinite(radius)||radius<=0||std::abs(feet.x-roof.center.x)>roof.extents.x||
        std::abs(feet.z-roof.center.z)>roof.extents.z||
        BuildingWallClear(feet,target,target.y,std::span(&desk,1),radius))return std::nullopt;
    const auto distance=[](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return std::hypot(double(a.x)-b.x,double(a.z)-b.z);};
    const auto clear=[&](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return BuildingWallClear(a,b,target.y,walls,radius);};
    const float depth=desk.extents.z+radius+.08f;
    const float west=desk.center.x-(desk.extents.x+radius+.04f);
    const float east=desk.center.x+(desk.extents.x+radius+.02f);
    std::optional<Aevrynd::Vec3> next;
    double shortest=(std::numeric_limits<double>::infinity)();
    const auto consider=[&](Aevrynd::Vec3 point,double remaining){
        const double leg=distance(feet,point);
        if(leg>.01&&leg+remaining<shortest&&clear(feet,point)){
            shortest=leg+remaining;
            next=point;
        }
    };
    for(float side:{-1.f,1.f}){
        const float z=desk.center.z+side*depth;
        const Aevrynd::Vec3 logSide{west,target.y,z},rear{east,target.y,z};
        if(!clear(logSide,target))continue;
        const double lastLeg=distance(logSide,target);
        consider(logSide,lastLeg);
        if(clear(rear,logSide))consider(rear,distance(rear,logSide)+lastLeg);
    }
    return next;
}
inline std::optional<Aevrynd::Vec3> ShelterApproach(Aevrynd::Vec3 feet,float radius) {
    static const auto structure=RepairShelterSolids();
    return BuildingApproach(feet,radius,Session::repairSites[0],structure.front(),std::span(structure).subspan(1,3),1.f);
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
    unsigned preferredRelay=NoTrackedRelay,std::optional<float> presentationRadius=std::nullopt) {
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
        // 提前取材沿用正式互動判定；同時保留補給箱的長程目標。
        auto repairProbe=session;
        for(unsigned site=0;site<Session::repairSites.size();++site){
            if(repairProbe.CollectRepairMaterial(site,crouched)){
                guide.earlyRepairIndex=site;break;
            }
        }
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
        // 只有畫面導航使用中繼點。索引與操作門檻仍取自實際工具。
        if(presentationRadius&&guide.index==0&&!guide.useReady){
            if(const auto step=ShelterApproach(session.feet,*presentationRadius)){
                guide.target=*step;
                guide.shelterApproach=true;
                name="East shelter approach";
            }
        }
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
        if(enemy.phase==EnemyPhase::Recovery){
            if(enemy.countered)guide.counterOpportunity=CounterOpportunity::Used;
            else if(evasion.remaining>0)guide.counterOpportunity=CounterOpportunity::Evading;
            else if(enemy.playerCooldown>0)guide.counterOpportunity=CounterOpportunity::Cooldown;
            else if(range>2.5)guide.counterOpportunity=CounterOpportunity::TooFar;
            else if(std::abs(session.feet.y-enemy.feet.y)>=2.f)guide.counterOpportunity=CounterOpportunity::HeightDifference;
            else if(!CounterLineClear(session,enemy,solids,terrain))guide.counterOpportunity=CounterOpportunity::Obstructed;
            else guide.counterOpportunity=CounterOpportunity::Ready;
        }
        guide.counterReady=guide.counterOpportunity==CounterOpportunity::Ready;
        guide.evadeAvailability=evasion.Availability(grounded,crouched);
        guide.evadeReady=guide.evadeAvailability==EvasionAvailability::Ready;
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
        if(presentationRadius&&!guide.useReady&&!guide.tuneReady){
            static const auto workshop=WorkshopSolids();
            static const auto hut=RangerHutSolids();
            const auto& building=session.fuseClueRead?workshop:hut;
            const float side=session.fuseClueRead?1.f:-1.f;
            if(const auto step=BuildingApproach(session.feet,*presentationRadius,guide.target,building.front(),
                std::span(building).subspan(1),side)){
                guide.target=*step;
                guide.settlementApproach=true;
                name=session.fuseClueRead?"East workshop entrance":"West ranger hut entrance";
            }else if(!session.fuseClueRead){
                if(const auto around=RangerDeskApproach(session.feet,*presentationRadius,guide.target,
                    hut.front(),hut[2],std::span(hut).subspan(1))){
                    guide.target=*around;
                    guide.rangerDeskApproach=true;
                    name="Around ranger desk";
                }
            }
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
