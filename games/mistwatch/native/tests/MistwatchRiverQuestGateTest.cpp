#include "MistwatchMiddleLayer.h"
#include "MistwatchFernColonies.h"
#include "MistwatchEvasion.h"
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/MeshLoader.h>
#include <iostream>
#include <algorithm>
#include <stdexcept>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
void Require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
Session BeforeBridge(unsigned inventory) {
    Session session;
    if(inventory>=1)for(unsigned index=0;index<3;++index) {
        session.feet=Session::supplies[index];
        Require(session.Collect(index),"Supply fixture interaction failed");
    }
    if(inventory>=2)for(unsigned index=0;index<3;++index) {
        session.feet=Session::repairSites[index];
        Require(session.CollectRepairMaterial(index,index==0),"Repair fixture interaction failed");
    }
    Require(session.Valid()&&!session.bridgeRepaired,"Invalid unrepaired fixture");
    return session;
}
template<class Action>void RejectUnchanged(Session& session,Action action,unsigned& checks) {
    const auto before=session.Serialize();
    Require(!before.empty()&&!action()&&session.Serialize()==before,"Downstream action bypassed bridge or mutated state");
    ++checks;
}
// 這是任務語義驗證：後續互動點採近距 fixture，避免因距離不符而得到假陰性。
void CheckLaterActions(Session session,unsigned& checks) {
    const auto expected=session.CurrentStage();
    session.feet=Session::arena;
    RejectUnchanged(session,[&]{return session.FinishEncounter();},checks);
    session.feet=Session::rangerLog;
    RejectUnchanged(session,[&]{return session.ReadFuseClue();},checks);
    session.feet=Session::fuse;
    for(unsigned wheel=0;wheel<3;++wheel)
        RejectUnchanged(session,[&]{return session.CycleFuseDial(wheel);},checks);
    RejectUnchanged(session,[&]{return session.CollectFuse();},checks);
    session.feet=Session::generator;
    for(unsigned lever=0;lever<3;++lever)
        RejectUnchanged(session,[&]{return session.ToggleCircuit(lever);},checks);
    RejectUnchanged(session,[&]{return session.RepairGenerator();},checks);
    for(unsigned index=0;index<3;++index) {
        session.feet=Session::relaySites[index];
        for(int direction:{-1,1})RejectUnchanged(session,[&]{return session.TuneRelay(index,direction);},checks);
        RejectUnchanged(session,[&]{return session.ConnectRelay(index);},checks);
    }
    session.feet=Session::beacon;
    RejectUnchanged(session,[&]{return session.LightBeacon();},checks);
    Require(session.Valid()&&!session.bridgeRepaired&&!session.guardianDefeated&&
        !session.fuseCollected&&!session.powerRestored&&!session.beaconLit&&session.CurrentStage()==expected,
        "Unrepaired quest progression changed");
}
void CheckCorruptSaves(const Session& clean,unsigned& checks) {
    for(unsigned field=0;field<7;++field) {
        auto corrupt=clean;
        switch(field) {
            case 0:corrupt.guardianDefeated=true;break;
            case 1:corrupt.fuseClueRead=true;break;
            case 2:corrupt.fuseDials=Session::solvedFuseDials;break;
            case 3:corrupt.fuseCollected=true;break;
            case 4:corrupt.powerRestored=true;break;
            case 5:corrupt.relayLinked=7;break;
            case 6:corrupt.beaconLit=true;break;
        }
        Require(!corrupt.Valid()&&corrupt.Serialize().empty(),"Invalid downstream flag serialized");
        auto target=clean;
        const auto before=target.Serialize();
        std::ostringstream text;
        text.imbue(std::locale::classic());text.precision(9);
        text<<"MISTWATCH 6 "<<corrupt.collected<<' '<<corrupt.health<<' '<<corrupt.bridgeRepaired<<' '
            <<corrupt.guardianDefeated<<' '<<corrupt.beaconLit<<' '<<corrupt.feet.x<<' '
            <<corrupt.feet.y<<' '<<corrupt.feet.z<<' '<<corrupt.fuseCollected<<' '
            <<corrupt.powerRestored<<' '<<corrupt.circuit<<' '<<corrupt.repairMaterials<<' '
            <<corrupt.fuseClueRead<<' '<<corrupt.fuseDials<<' '<<corrupt.relayLinked<<' '<<corrupt.relaySectors;
        Require(!target.Restore(text.str())&&target.Serialize()==before,"Corrupt save bypassed mandatory quest");
        ++checks;
    }
}
}
int main() {
    try {
        const auto terrain=CreateTerrain();const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
        const auto original=CreateUnderstory(terrain,trees,props);const auto broadleaf=CreateBroadleafTrees(terrain,trees,props,original);
        auto solids=WorldSolids(false);std::array<float,4> radii{};
        for(unsigned variant=0;variant<4;++variant) {
            auto loaded=LoadAevryndMeshFile(std::filesystem::path(MISTWATCH_SOURCE_ROOT)/
                "engine/assets/mistwatch-fern-02/meshes"/FernMeshName(variant));
            Require(bool(loaded),"Real fern mesh absent");NormalizeFernAsset(loaded.mesh);
            radii[variant]=FernColonyAssetFromMesh(loaded.mesh).radius;
        }
        const auto middle=CreateMiddleLayer(terrain,trees,props,original,broadleaf,solids,24,radii);
        Require(middle.reachedBudget&&middle.trees.size()==24,"Real middle-layer incomplete");
        for(const auto& tree:middle.trees)solids.push_back(tree.trunk);
        Require(solids.size()==245,"Coherent world solids incomplete");
        // 固定輸入順序：13個x、雙方向、walk/run各一次、jump/dash各11個tick。
        // 此97案為17385141已發布真245固體的跨河基準，不取自本候選。
        // 原CSV SHA256：F5B570BCE031697826C8B827C3AF21F0B8C6585819373FAB951CB45DA1944BDC。
        constexpr std::array<unsigned,97> legacyCrossings{
            100,101,102,124,125,126,149,150,172,173,174,196,197,198,220,221,222,
            242,243,244,245,246,247,248,249,250,251,252,253,254,255,256,257,258,
            259,260,261,262,263,264,268,269,270,280,281,290,291,292,293,294,295,
            296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,
            314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,330,
            331,332,333,334,335,336,484,485,486,508,509,510};
        Require(std::is_sorted(legacyCrossings.begin(),legacyCrossings.end())&&
            std::adjacent_find(legacyCrossings.begin(),legacyCrossings.end())==legacyCrossings.end(),
            "Frozen legacy crossing set is not unique and sorted");
        unsigned cases{},crossings{},checks{},roundtrips{},retries{};
        for(float x:{-40.f,-32.f,-24.f,-16.f,-12.f,-6.f,0.f,6.f,12.f,16.f,24.f,32.f,40.f})
        for(int sign:{-1,1})for(unsigned mode=0;mode<4;++mode) {
            for(unsigned sample=0;sample<(mode<2?1u:11u);++sample) {
                ++cases;const unsigned trigger=mode<2?0u:16u+sample*16u;
                const float center=RiverCenterZ(x),start=center-float(sign)*13.f;
                CharacterController3D actor;actor.Reset({x,terrain.SampleHeight(x,start),start});Evasion evasion;
                bool crossed{};
                for(unsigned tick=0;tick<600;++tick) {
                    const auto position=actor.Position();
                    if(position.y<-5.f)break;
                    if(float(sign)*(position.z-center)>=13.f){crossed=true;break;}
                    const auto motion=evasion.Tick(1.f/60.f,mode==3&&tick==trigger,actor.Grounded(),false,{0,0,float(sign)});
                    actor.Step({motion.movement,mode==2&&tick==trigger,
                        motion.dashing?Evasion::SpeedMultiplier:mode==0?1.f:1.5f},solids,terrain,1.f/60.f);
                }
                if(!crossed)continue;
                Require(std::binary_search(legacyCrossings.begin(),legacyCrossings.end(),cases),
                    "New fixed-input fullworld unrepaired crossing introduced");
                ++crossings;
                for(unsigned inventory=0;inventory<3;++inventory) {
                    auto session=BeforeBridge(inventory);session.feet=actor.Position();
                    Require(session.Valid(),"Reachable crossing session invalid");
                    Session restored;
                    Require(restored.Restore(session.Serialize())&&restored.Serialize()==session.Serialize()&&
                        !restored.bridgeRepaired,"Reachable crossing save changed mandatory bridge");
                    ++roundtrips;
                    RejectUnchanged(restored,[&]{return restored.FinishEncounter(restored.feet);},checks);
                    CheckLaterActions(restored,checks);CheckCorruptSaves(restored,checks);
                    restored.Damage(100);restored.Retry();
                    Require(restored.health==100&&Length(restored.feet)==0&&!restored.bridgeRepaired&&
                        restored.CurrentStage()==session.CurrentStage(),"Unrepaired retry skipped bridge");
                    ++retries;
                }
            }
        }
        Require(cases==624&&crossings==92&&roundtrips==276&&retries==276,"Measured crossing coverage changed");
        auto positive=BeforeBridge(2);positive.feet=Session::arena;
        RejectUnchanged(positive,[&]{return positive.FinishEncounter();},checks);
        positive.feet=Session::bridge;
        Require(positive.RepairBridge()&&positive.CurrentStage()==Stage::Encounter,"Legitimate repair blocked");
        positive.feet=Session::arena;
        Require(positive.FinishEncounter()&&positive.CurrentStage()==Stage::Fuse,"Legitimate encounter blocked");
        std::cout<<"RiverQuestGate CPU PASS cases="<<cases<<" crossed="<<crossings<<" checks="<<checks
            <<" save_roundtrips="<<roundtrips<<" retries="<<retries
            <<"; near-interaction fixtures, not a human playthrough; fullworld no-new-crossing gate remains separate\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 1;}
}
