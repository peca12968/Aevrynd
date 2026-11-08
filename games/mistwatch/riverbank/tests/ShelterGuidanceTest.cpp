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
        std::cout << "SHELTER_APPROACH_GUIDE_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
