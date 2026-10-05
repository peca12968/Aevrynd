#include "MistwatchWorld.h"
#include "MistwatchEncounter.h"
#include "MistwatchMiddleLayer.h"
#include "MistwatchFernColonies.h"
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <string_view>
#include <iostream>

int main(int argc,char** argv) {
    using namespace Mistwatch;
    Session game;
    Encounter enemy;
    Aevrynd::CharacterController3D actor;
    const auto terrain=CreateTerrain();
    const bool middle=argc==2&&std::string_view(argv[1])=="--middle-layer";
    if(argc!=1&&!middle)return 24;
    std::vector<Aevrynd::BoxCollider3D> middleSolids;
    if(middle) {
        const auto conifers=TreePositions();const auto props=ForestProps(terrain,conifers);
        const auto understory=CreateUnderstory(terrain,conifers,props);
        const auto original=CreateBroadleafTrees(terrain,conifers,props,understory);
        std::array<float,4> radii{};
        for(unsigned i=0;i<radii.size();++i) {
            auto mesh=Aevrynd::LoadAevryndMeshFile(std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/mistwatch-fern-02/meshes"/FernMeshName(i));
            if(!mesh)return 25;
            NormalizeFernAsset(mesh.mesh);radii[i]=FernColonyAssetFromMesh(mesh.mesh).radius;
        }
        const auto generated=CreateMiddleLayer(terrain,conifers,props,understory,original,WorldSolids(false),24,radii);
        if(generated.trees.size()!=24||!generated.reachedBudget||MiddleLayerTransformFingerprint(generated.trees)!=9091870748330489572ull)return 26;
        for(const auto& tree:generated.trees)middleSolids.push_back(tree.trunk);
    }
    auto solids=WorldSolids(false);
    solids.insert(solids.end(),middleSolids.begin(),middleSolids.end());
    if(solids.size()!=(middle?245u:221u))return 27;
    unsigned ticks{};
    bool crouching{};
    actor.Reset({});
    const auto tick=[&](Aevrynd::Vec3 move,bool attack=false) {
        actor.Step({move,false,crouching?.45f:1.f},solids,terrain,1.f/60.f);
        game.feet=actor.Position();
        ++ticks;
        if(game.feet.y< -5) game.Damage(100);
        if(game.CurrentStage()==Stage::Encounter) {
            game.Damage(enemy.Tick(game.feet,1.f/60.f,attack));
            if(!enemy.health) game.FinishEncounter(enemy.feet);
        }
    };
    const auto walk=[&](Aevrynd::Vec3 destination) {
        for(unsigned step=0;step<1800;++step) {
            auto delta=destination-game.feet;delta.y=0;
            if(Aevrynd::Length(delta)<.15f) return game.Alive();
            tick(Aevrynd::Normalize(delta));
            if(!game.Alive()) return false;
        }
        std::cerr<<"Route stalled at "<<game.feet.x<<","<<game.feet.y<<","<<game.feet.z<<'\n';
        return false;
    };
    for(unsigned index=0;index<Session::supplies.size();++index) {
        const auto supply=Session::supplies[index];
        // 沿主路前進，再從橫向支線接近補給，全部經過真實碰撞。
        if(!walk({0,0,game.feet.z})||!walk({0,0,supply.z})||!walk(supply)||!game.Collect(index)) return 1;
    }
    for(unsigned index=0;index<Session::repairSites.size();++index) {
        const auto site=Session::repairSites[index];
        if(!walk({0,0,game.feet.z})||!walk({0,0,site.z}))return 11;
        crouching=index==0;
        const auto approach=site+(crouching?Aevrynd::Vec3{.65f,0,0}:Aevrynd::Vec3{});
        if(!actor.TrySetHeight(crouching?1.4f:1.8f,solids)||!walk(approach)||!game.CollectRepairMaterial(index,crouching))return 12;
        if(!walk({0,0,site.z}))return 13;
        crouching=false;
        if(!actor.TrySetHeight(1.8f,solids))return 14;
    }
    if(!walk({0,0,game.feet.z})||!walk(Session::bridge)||!game.RepairBridge()) return 2;
    solids=WorldSolids(true);
    solids.insert(solids.end(),middleSolids.begin(),middleSolids.end());
    if(solids.size()!=(middle?246u:222u))return 28;
    const auto checkpoint=game.Serialize();
    Session restored;
    if(!restored.Restore(checkpoint)||restored.CurrentStage()!=Stage::Encounter) return 3;
    if(!walk({0,0,45.5f})) return 4;
    for(unsigned step=0;step<600 && game.CurrentStage()==Stage::Encounter;++step) tick({},true);
    if(!game.Alive()||!game.guardianDefeated) return 5;
    if(!walk({0,0,Session::rangerLog.z})||!walk(Session::rangerLog)||!game.ReadFuseClue())return 15;
    const auto clueSave=game.Serialize();
    Session clueRestored;
    if(!clueRestored.Restore(clueSave)||!clueRestored.fuseClueRead||clueRestored.fuseDials)return 16;
    if(!walk({0,0,Session::rangerLog.z})||!walk({0,0,Session::fuse.z})||!walk(Session::fuse)) return 7;
    if(game.CollectFuse())return 17;
    for(unsigned wheel=0;wheel<3;++wheel)for(unsigned turn=0;turn<Session::fuseCode[wheel];++turn)
        if(!game.CycleFuseDial(wheel))return 18;
    if(!game.CollectFuse())return 19;
    if(!walk({0,0,Session::fuse.z})||!walk({0,0,Session::generator.z})||!walk(Session::generator+Aevrynd::Vec3{-1.6f,0,0})) return 8;
    if(game.RepairGenerator()||!game.ToggleCircuit(0)||!game.ToggleCircuit(2)||!game.RepairGenerator()) return 10;
    if(!walk({0,0,Session::generator.z})) return 9;
    if(game.CurrentStage()!=Stage::Relays||game.LightBeacon())return 20;
    for(unsigned index=0;index<3;++index) {
        const auto p=Session::relaySites[index];
        if(!walk({0,0,p.z})||!walk(p+Aevrynd::Vec3{p.x<0?1.15f:-1.15f,0,0}))return 21;
        for(unsigned turn=0;turn<Session::RelayTargetSector(index);++turn)if(!game.TuneRelay(index,1))return 22;
        if(!game.ConnectRelay(index)||!walk({0,0,p.z}))return 23;
    }
    if(!walk(Session::beacon)||!game.LightBeacon()||game.CurrentStage()!=Stage::Complete) return 6;
    std::cout<<"Complete collision-backed route PASS; middle_layer="<<middle<<" colliders="<<solids.size()<<" seconds="<<float(ticks)/60.f<<" health="<<game.health<<'\n';
}
