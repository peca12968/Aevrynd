#include "MistwatchFieldJournal.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
}

int main() {
    using namespace Mistwatch;
    try {
        Session session;
        const auto terrain = CreateTerrain();
        const std::array<Aevrynd::BoxCollider3D, 0> solids{};
        Aevrynd::Camera3D camera;
        camera.position = {0, 3, -6};
        camera.target = {0, 1, 0};
        const Encounter enemy;
        const Evasion evasion;
        auto instruction = [&](bool crouched, bool paused = false) {
            const auto before = session.Serialize();
            const auto guide = BuildObjectiveGuide(session, enemy, evasion, camera, crouched, true, solids, terrain);
            const auto hud = FieldPresentation::Build(session, guide, enemy, true, paused, false);
            Require(hud.valid && guide.valid, "invalid gameplay guide");
            Require(session.Serialize() == before, "the hint modified gameplay state");
            return hud.instruction;
        };
        const auto& roof = CameraCeilingSolids().back();
        const auto tool = Session::repairSites[0];
        const float eastEdge = roof.center.x + roof.extents.x;
        session.feet = {eastEdge + .7f, tool.y, tool.z};
        Require(!session.Near(tool), "the apron must exercise the old missed approach");
        Require(instruction(false).starts_with("按住 Ctrl"), "standing at the east approach lacks the entry hint");
        Require(!instruction(false, true).starts_with("按住 Ctrl"), "paused screen advertises a live approach");
        session.feet.x = eastEdge + 2.f;
        Require(!instruction(false).starts_with("按住 Ctrl"), "east apron hint appears too far away");
        session.feet.x = roof.center.x - roof.extents.x - .7f;
        Require(!instruction(false).starts_with("按住 Ctrl"), "the back wall advertises an entrance");
        session.feet = {eastEdge - .25f, tool.y, tool.z};
        Require(instruction(true).starts_with("保持蹲姿"), "inside approach instruction is missing");
        session.feet = {tool.x + .76f, tool.y, tool.z};
        Require(instruction(true) == "E：拾取低棚工具。", "ready pickup must take priority");
        Require(session.CollectRepairMaterial(0, true), "original tool pickup was rejected");
        Require(!instruction(true).starts_with("保持蹲姿"), "tool hint remains after pickup");
        session.feet = Session::supplies[0];
        Require(instruction(false) == "E：拾取補給", "ready supplies lost their instruction");
        session.collected=7;
        session.repairMaterials=6;
        constexpr float radius=Aevrynd::CharacterControllerSettings3D{}.radius;
        const auto routeGuide=[&](bool crouched,bool display) {
            const auto before=session.Serialize();
            const auto guide=BuildObjectiveGuide(session,enemy,evasion,camera,crouched,true,solids,terrain,NoTrackedRelay,
                display?std::optional<float>(radius):std::nullopt);
            Require(session.Serialize()==before&&guide.valid&&guide.index==0,"shelter route changes interaction state");
            return guide;
        };
        const Aevrynd::BoxCollider3D testWall{{0,.65f,0},{.25f,.7f,2.f}};
        const Aevrynd::Vec3 touching{-.75f,.65f,0};
        constexpr float testRadius=.5f;
        using ObjectiveGuideDetail::ShelterWallCrossed;
        Require(!ShelterWallCrossed(touching,{-1.f,.65f,1.f},testWall,testRadius),"touching player must be able to move away");
        Require(!ShelterWallCrossed(touching,{-.75f,.65f,1.f},testWall,testRadius),"parallel motion along wall face must be available");
        Require(ShelterWallCrossed(touching,{-.5f,.65f,1.f},testWall,testRadius),"route must not cross inward from the wall face");
        Require(ShelterWallCrossed({-.74f,.65f,0},{-1.f,.65f,1.f},testWall,testRadius),"interior overlap must not be treated as contact");
        const auto structure=RepairShelterSolids();
        const auto crossesWall=[&](Aevrynd::Vec3 target) {
            auto from=session.feet;
            from.y=target.y=tool.y+.65f;
            for(const auto& wall:std::span(structure).subspan(1,3))
                if(Aevrynd::SegmentIntersectsAabb3D(from,target,wall.center,wall.extents+Aevrynd::Vec3{radius,0,radius}))
                    return true;
            return false;
        };
        for(float zOffset:{0.f,3.f,-3.f}) {
            session.feet={roof.center.x-roof.extents.x-.7f,tool.y,tool.z+zOffset};
            Require(crossesWall(routeGuide(false,false).target),"baseline no longer points through the shelter wall");
            unsigned hops=0;
            for(;hops<4;++hops){
                const auto guide=routeGuide(false,true);
                Require(!guide.useReady&&!crossesWall(guide.target),"shelter route points through a wall or grants use");
                if(!guide.shelterApproach)break;
                const auto hud=FieldPresentation::Build(session,guide,enemy,true,false,false);
                Require(hud.navigation.starts_with("低棚入口路線")&&hud.instruction.starts_with("沿指引"),"HUD drops route guidance");
                session.feet=guide.target;
            }
            Require(hops>0&&hops<4&&session.feet.x>eastEdge,"shelter route failed to reach the open east side");
        }
        session.feet={tool.x+.76f,tool.y,tool.z};
        const auto ready=routeGuide(true,true);
        Require(ready.useReady&&!ready.shelterApproach,"route masks the ready tool pickup");
        session.repairMaterials=7;
        Require(FieldPresentation::LastAction("Move character clear before bridge repair")==
            "目前站位無法修橋。移到橋頭旁，再按 E。","BRIDGE_PLAYER_REJECTION_FEEDBACK");
        Require(FieldPresentation::LastAction("Move camera clear before bridge repair")==
            "鏡頭空間不足。拖曳轉動鏡頭後，再按 E。","BRIDGE_CAMERA_REJECTION_FEEDBACK");
        ActionFeedbackToast repairToast;
        Require(repairToast.Publish(ActionFeedbackEvent::BridgeRepaired)&&repairToast.Visible()&&
            FieldPresentation::Toast(repairToast.Event())=="跨谷橋已修復。沿橋前往谷北。",
            "BRIDGE_COMPLETED_FEEDBACK");
        Require(repairToast.Advance(1.0,ActionFeedbackClock::Active)&&repairToast.Visible()&&
            repairToast.Advance(3.0,ActionFeedbackClock::Active)&&!repairToast.Visible(),
            "BRIDGE_COMPLETED_FEEDBACK_DURATION");
        session.feet=Session::bridge;
        Require(session.CurrentStage()==Stage::Bridge&&instruction(false)=="E：嘗試修橋。"&&!session.bridgeRepaired,
            "BRIDGE_NEAR_PROMPT");
        session.feet.z-=3.f;
        Require(instruction(false)=="沿主土路到橋頭。","BRIDGE_OUT_OF_REACH_PROMPT");
        session.bridgeRepaired=true;
        session.guardianDefeated=true;
        session.feet=Session::fuse;
        const auto beforeClue=session.Serialize();
        auto lockedCabinet=session;
        Require(lockedCabinet.CycleFuseDial(0)&&!lockedCabinet.CollectFuse(),
            "fixture must permit an early dial turn but reject opening");
        const auto lockedGuide=BuildObjectiveGuide(session,enemy,evasion,camera,false,true,solids,terrain,NoTrackedRelay,radius);
        Require(lockedGuide.valid&&lockedGuide.hasTarget&&!lockedGuide.tuneReady&&!lockedGuide.useReady&&
            !session.NearInterior(lockedGuide.target),"fixture must still navigate toward the ranger log");
        const auto lockedHud=FieldPresentation::Build(session,lockedGuide,enemy,true,false,false);
        Require(lockedHud.valid&&lockedHud.instruction=="保險絲櫃需要日誌線索；先到北側木屋讀完日誌，再回來。"&&
            lockedHud.context.find("2 - 0 - 3")==std::string::npos&&session.Serialize()==beforeClue,
            "pre-clue cabinet HUD mixes route and tuning instructions");
        std::cout << "CABINET_CLUE_GUIDANCE_PASS\n";
        const auto hut=RangerHutSolids();
        const auto hutWalls=std::span(hut).subspan(1);
        const auto log=Session::rangerLog;
        for(const auto offset:std::array{Aevrynd::Vec3{.8f,0,-1.4f},Aevrynd::Vec3{.8f,0,1.4f},Aevrynd::Vec3{1.8f,0,0}}){
            session.feet=log+offset;
            const auto clear=[&](Aevrynd::Vec3 from,Aevrynd::Vec3 to){
                from.y=to.y=log.y+.65f;
                for(const auto& wall:hutWalls)
                    if(ShelterWallCrossed(from,to,wall,radius))return false;
                return true;
            };
            Require(!clear(session.feet,log),"fixture must start with desk between player and log");
            unsigned hops=0;
            for(;hops<4&&!session.NearInterior(log);++hops){
                const auto before=session.Serialize();
                const auto guide=BuildObjectiveGuide(session,enemy,evasion,camera,false,true,solids,terrain,NoTrackedRelay,radius);
                Require(guide.valid&&!guide.useReady&&clear(session.feet,guide.target)&&session.Serialize()==before,
                    "ranger log guidance points through the desk");
                if(!guide.settlementApproach&&(guide.target.x!=log.x||guide.target.z!=log.z)){
                    const auto hud=FieldPresentation::Build(session,guide,enemy,true,false,false);
                    Require(hud.navigation.starts_with("木屋內繞行")&&hud.instruction.starts_with("沿箭頭繞過桌子"),
                        "desk detour HUD points to the wrong destination");
                }
                session.feet=guide.target;
            }
            Require(hops>=2&&hops<4&&session.NearInterior(log)&&!session.fuseClueRead,
                "desk detour did not reach the log without granting interaction");
        }
        std::cout << "RANGER_DESK_GUIDANCE_PASS starts=3\n";
        unsigned settlementStarts=0;
        for(bool cabinet:{false,true}) {
            session.fuseClueRead=cabinet;
            const auto target=cabinet?Session::fuse:Session::rangerLog;
            const auto building=cabinet?WorkshopSolids():RangerHutSolids();
            const auto& buildingRoof=building.front();
            const auto walls=std::span(building).subspan(1);
            const float entrySide=cabinet?1.f:-1.f;
            const auto crosses=[&](Aevrynd::Vec3 from,Aevrynd::Vec3 to) {
                from.y=to.y=target.y+.65f;
                for(const auto& wall:walls)
                    if(ShelterWallCrossed(from,to,wall,radius))return true;
                return false;
            };
            for(float offset:{0.f,-2.f,2.f}) {
                session.feet={target.x-entrySide*(buildingRoof.extents.x+1.f),target.y,target.z+offset};
                Require(crosses(session.feet,target),"settlement fixture does not start behind a wall");
                unsigned hops=0;
                for(;hops<4;++hops){
                    const auto before=session.Serialize();
                    const auto guide=BuildObjectiveGuide(session,enemy,evasion,camera,false,true,solids,terrain,NoTrackedRelay,radius);
                    Require(guide.valid&&guide.hasTarget&&!guide.useReady&&!guide.tuneReady&&session.Serialize()==before,
                        "settlement arrow changed interaction eligibility");
                    Require(!crosses(session.feet,guide.target),"settlement arrow points through a wall");
                    if(!crosses(session.feet,target))break;
                    const auto hud=FieldPresentation::Build(session,guide,enemy,true,false,false);
                    Require(hud.navigation.starts_with(cabinet?"工棚東側入口":"木屋西側入口"),
                        "settlement entrance label is missing");
                    session.feet=guide.target;
                }
                Require(hops>0&&hops<4&&(session.feet.x-target.x)*entrySide>buildingRoof.extents.x,
                    "settlement guidance failed to lead to the open side");
                ++settlementStarts;
            }
            session.feet=target;
            const auto interior=BuildObjectiveGuide(session,enemy,evasion,camera,false,true,solids,terrain,NoTrackedRelay,radius);
            Require(interior.valid&&interior.useReady!=cabinet&&interior.tuneReady==cabinet,"interior interaction changed at the target");
            if(cabinet) {
                const auto tuningHud=FieldPresentation::Build(session,interior,enemy,true,false,false);
                Require(tuningHud.instruction.find("長按 1、2、3")!=std::string::npos,"unsolved cabinet lost tuning help");
                session.fuseDials=Session::solvedFuseDials;
                const auto solved=BuildObjectiveGuide(session,enemy,evasion,camera,false,true,solids,terrain,NoTrackedRelay,radius);
                const auto hud=FieldPresentation::Build(session,solved,enemy,true,false,false);
                Require(solved.valid&&solved.useReady&&hud.instruction=="E：打開櫃子，取得保險絲。",
                    "solved cabinet must advertise the open action");
            }
        }
        Require(session.CollectFuse(), "normal cabinet action did not reach the generator stage");
        session.feet=Session::generator;
        const auto generatorHud=[&](bool paused=false) {
            const auto before=session.Serialize();
            const auto guide=BuildObjectiveGuide(session,enemy,evasion,camera,false,true,solids,terrain,NoTrackedRelay,radius);
            const auto hud=FieldPresentation::Build(session,guide,enemy,true,paused,false);
            Require(hud.valid&&guide.valid&&session.Serialize()==before,"generator HUD changed gameplay state");
            return hud;
        };
        constexpr std::array lampA{"A 熄","A 亮"},lampB{"B 熄","B 亮"},lampC{"C 熄","C 亮"};
        for(unsigned mask=0;mask<8;++mask){
            session.circuit=mask;
            const auto hud=generatorHud();
            Require(hud.context.find(lampA[mask&1u])!=std::string::npos&&
                hud.context.find(lampB[(mask>>1)&1u])!=std::string::npos&&
                hud.context.find(lampC[(mask>>2)&1u])!=std::string::npos,"generator HUD lost per-lamp states");
            Require(hud.instruction.starts_with("E：")==(mask==7),"generator E eligibility changed");
        }
        session.circuit=0;
        Require(session.ToggleCircuit(0)&&generatorHud().context.find("A 亮、B 亮、C 熄")!=std::string::npos,
            "lamp names did not follow the actual A+B action");
        Require(generatorHud(true).instruction=="角色與世界暫停更新。","paused generator advertises active controls");
        session.feet.x+=2.6f;
        Require(!session.Near(Session::generator)&&generatorHud().context=="已亮燈 2 / 3",
            "distant generator advertised nearby lamp states");
        session.feet=Session::generator;
        Require(session.ToggleCircuit(2)&&session.RepairGenerator()&&session.CurrentStage()==Stage::Relays&&
            !generatorHud().context.starts_with("已亮燈"),"generator guidance survived or changed the original repair action");
        std::cout << "GENERATOR_LAMP_GUIDANCE_PASS masks=8\n";
        std::cout << "BRIDGE_REPAIR_FEEDBACK_PASS cases=6\n";
        std::cout << "SETTLEMENT_ENTRANCE_GUIDE_PASS starts=" << settlementStarts << '\n';
        std::cout << "SHELTER_APPROACH_GUIDE_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
