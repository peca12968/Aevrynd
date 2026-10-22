#include "MistwatchInputReplay.h"
#include "MistwatchSurveyMap.h"
#include "MistwatchSupplyCrate.h"
#include "MistwatchWorld.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace Mistwatch;using namespace Aevrynd;
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
bool Same(Vec3 a,Vec3 b){return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
    std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&
    std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);}
Camera3D Camera(Vec3 feet){Camera3D c;c.target=feet+Vec3{0,1,0};c.position=c.target+Vec3{0,2,-6};return c;}
std::string Read(const std::filesystem::path& p,std::size_t bound){
    if(std::filesystem::file_size(p)>bound)throw std::runtime_error("CROSS_SAVE_SIZE");
    std::ifstream in(p,std::ios::binary);std::string s{std::istreambuf_iterator<char>(in),{}};
    if(!in||s.empty())throw std::runtime_error("CROSS_SAVE_READ");return s;
}
void Write(const std::filesystem::path& p,const std::string& s){
    std::ofstream out(p,std::ios::binary);out<<s;out.close();Need(bool(out),"EXPORT_WRITE");
}
void RejectOther(const std::filesystem::path& path){
    Session s;s.feet=Session::supplies[1];Need(s.Collect(1),"CROSS_FIXTURE");const auto before=s.Serialize();
    const auto other=Read(path/"session.txt",512);
    Need(!s.Restore(other)&&s.Serialize()==before,"CROSS_LAYOUT_SESSION_ACCEPTED_OR_MUTATED");
    SaveGame game;game.session=s;const auto saved=game.Serialize();
    Need(!game.Restore(other)&&game.Serialize()==saved,"CROSS_LEGACY_SESSION_SAVE_ENTRY_ACCEPTED");
    Need(!game.Restore(Read(path/"save.txt",SaveGame::MaximumBytes))&&game.Serialize()==saved,
        "CROSS_LAYOUT_OUTER_SAVE_ACCEPTED_OR_MUTATED");
}
void Basic(const std::filesystem::path& out){
    const auto& layout=ActiveEntryLayout;
    const auto supply=Session::supplies[0];
    Need(Same(supply,OriginalEntryLayoutActive?Vec3{4,0,6}:Vec3{-7,0,6}),"SELECTED_SUPPLY0");
    for(unsigned i=1;i<3;++i)Need(Same(Session::supplies[i],OriginalEntryLayout.supplies[i]),"OTHER_SUPPLY_MOVED");
    for(unsigned i=0;i<3;++i)Need(Same(Session::repairSites[i],OriginalEntryLayout.repairSites[i]),"REPAIR_SITE_MOVED");
    Need(EntryLayoutFingerprint(OriginalEntryLayout)!=EntryLayoutFingerprint(WestSupplyPilotLayout),"DISTINCT_LAYOUT_IDENTITY");
    const auto terrain=CreateTerrain();
    Need(terrain.SampleHeight(supply.x,supply.z)==supply.y,"SUPPLY_ROOT_NOT_REAL_TRIANGLE_HEIGHT");
    const auto crate=CreateSupplyCrate();
    const Transform3D transform{supply,{},{1,1,1}};
    const auto box=TransformBounds(crate.wood.Bounds(),transform);
    Need(std::abs(box.center.x-supply.x)<.00001f&&std::abs(box.center.z-supply.z)<.00001f,"CRATE_WORLD_POSE");
    for(float x:{-.4f,0.f,.4f})for(float z:{-.4f,0.f,.4f})
        Need(terrain.SampleHeight(supply.x+x,supply.z+z)==supply.y,"CRATE_COMPLETE_FOOTPRINT_GROUND");
    Need(!PropClearsObjectives({supply+Vec3{0,.2f,0},{.1f,.2f,.1f}}),"NEW_TARGET_NOT_PROTECTED");
    const bool rightBankFree=PropClearsObjectives({{7,.2f,6},{.1f,.2f,.1f}});
    Need(rightBankFree!=OriginalEntryLayoutActive,"RIGHT_BANK_PROTECTION_DID_NOT_FOLLOW_LAYOUT");
    const auto approach=InputReplay::BridgeSupplyApproach(0);
    Need(Same(approach,SupplyApproach(0))&&Same(InputReplay::BridgeSupplyReturn(0),{0,0,approach.z}),"ACTUAL_REPLAY_ROUTE_NOT_LAYOUT_AWARE");
    for(unsigned i=1;i<3;++i)Need(Same(InputReplay::BridgeSupplyApproach(i),SupplyApproach(i)),"OTHER_REPLAY_APPROACH_CHANGED");

    // 真控制器走原地形與同profile的legacy碰撞世界；不是目前完整8canopy/8617wood的證明。
    const auto world=WorldSolids(false);
    CharacterController3D actor;actor.Reset({0,0,0});unsigned ticks{};
    for(const auto target:{Vec3{0,0,approach.z},approach}){
        while(std::hypot(actor.Position().x-target.x,actor.Position().z-target.z)>.04f&&ticks<1200){
            const auto p=actor.Position();const auto direction=Normalize(Vec3{target.x-p.x,0,target.z-p.z});
            actor.Step({direction,false,.65f},world,terrain,1.f/60.f);++ticks;
        }
        Need(std::hypot(actor.Position().x-target.x,actor.Position().z-target.z)<=.04f,"REAL_CONTROLLER_ROUTE_STALLED");
    }
    Session s;s.feet=actor.Position();Encounter enemy;Evasion evade;
    const auto guide=BuildObjectiveGuide(s,enemy,evade,Camera(s.feet),false,actor.Grounded(),world,terrain);
    Need(guide.valid&&guide.hasTarget&&guide.index==0&&Same(guide.target,supply)&&guide.useReady,"GUIDE_TARGET_OR_USE_DRIFT");
    const auto map=SurveyMap::Build(s,guide,Camera(s.feet),terrain);Need(map.valid,"MAP_INVALID");
    bool marker{};for(unsigned i=0;i<map.markerCount;++i)
        if(map.markers[i].kind==SurveyMap::Kind::Supply&&map.markers[i].index==0)marker=Same(map.markers[i].world,supply);
    Need(marker,"MAP_TARGET_DRIFT");
    Need(s.Collect(0)&&!s.Collect(0),"ARRIVAL_PICKUP");
    Session oldSpot;oldSpot.feet=OriginalEntryLayout.supplies[0];
    Need(oldSpot.Collect(0)==OriginalEntryLayoutActive,"OLD_VISIBLE_POSITION_STILL_COLLECTS");
    const auto state=s.Serialize();Session loaded;Need(loaded.Restore(state)&&loaded.Serialize()==state,"LOCAL_SESSION_ROUNDTRIP");
    SaveGame saved;saved.session=s;const auto encoded=saved.Serialize();SaveGame restored;
    Need(!encoded.empty()&&restored.Restore(encoded)&&restored.Serialize()==encoded,"LOCAL_SAVE_ROUNDTRIP");
    const auto stable=loaded.Serialize();
    if constexpr(!OriginalEntryLayoutActive){
        for(const auto replacement:{"0","1","3","-2","+2","4294967298"}){
            auto corrupt=state;const auto prefix=std::string("MISTWATCH 8 ENTRY_LAYOUT ");
            const auto end=corrupt.find(' ',prefix.size());Need(end!=std::string::npos,"IDENTITY_FIXTURE");
            corrupt.replace(prefix.size(),end-prefix.size(),replacement);
            Need(!loaded.Restore(corrupt)&&loaded.Serialize()==stable,"BAD_LAYOUT_ID_ACCEPTED_OR_MUTATED");
        }
        for(const auto replacement:{"0","-1","18446744073709551616"}){
            auto corrupt=state;const auto begin=corrupt.find(' ',std::string("MISTWATCH 8 ENTRY_LAYOUT ").size())+1;
            const auto end=corrupt.find(' ',begin);Need(end!=std::string::npos,"HASH_FIXTURE");
            corrupt.replace(begin,end-begin,replacement);
            Need(!loaded.Restore(corrupt)&&loaded.Serialize()==stable,"BAD_LAYOUT_HASH_ACCEPTED_OR_MUTATED");
        }
        for(const auto replacement:{"0","1","-1","4294967296"}){
            auto corrupt=state;auto begin=std::string("MISTWATCH 8 ENTRY_LAYOUT ").size();
            for(unsigned token=0;token<2;++token)begin=corrupt.find(' ',begin)+1;
            const auto end=corrupt.find(' ',begin);Need(end!=std::string::npos,"EPOCH_FIXTURE");
            corrupt.replace(begin,end-begin,replacement);
            Need(!loaded.Restore(corrupt)&&loaded.Serialize()==stable,"BAD_WORLD_EPOCH_ACCEPTED_OR_MUTATED");
        }
        for(std::size_t size=0;size<state.size();++size)
            Need(!loaded.Restore(std::string_view(state).substr(0,size))&&loaded.Serialize()==stable,"TRUNCATED_PILOT_ACCEPTED");
        Need(!loaded.Restore(state+" extra")&&loaded.Serialize()==stable,"TRAILING_PILOT_ACCEPTED");
    }
    if(!out.empty()){
        Need(out.is_absolute()&&!std::filesystem::exists(out)&&std::filesystem::create_directory(out),"FRESH_EXPORT");
        Write(out/"session.txt",state);Write(out/"save.txt",encoded);
        Write(out/"summary.txt","layout_id="+std::to_string(static_cast<std::uint32_t>(layout.id))+
            "\nlayout_fingerprint="+std::to_string(EntryLayoutFingerprint(layout))+"\nworld_epoch="+std::to_string(ActiveEntryWorldEpoch)+"\ncontroller_ticks="+std::to_string(ticks)+
            "\nfull_current_world_qualified=0\nnative_game_executed=0\n");
    }
    std::cout<<"ENTRY_PILOT_CPU_PASS layout="<<static_cast<std::uint32_t>(layout.id)<<" checks="<<checks
        <<" real_controller_ticks="<<ticks<<" right_bank_clearance="<<rightBankFree
        <<" full_current_world=0 native_game=0\n";
}
}
int main(int argc,char** argv){try{
    if(argc==1){Basic({});return 0;}
    if(argc!=3)throw std::runtime_error("ARGS");
    if(std::string_view(argv[1])=="--export"){Basic(std::filesystem::path(argv[2]));return 0;}
    if(std::string_view(argv[1])=="--reject-other"){
        RejectOther(std::filesystem::path(argv[2]));std::cout<<"CROSS_LAYOUT_REJECT_PASS layout="<<static_cast<unsigned>(ActiveEntryLayout.id)<<" checks="<<checks<<'\n';return 0;
    }
    throw std::runtime_error("ARGS");
}catch(const std::exception& error){std::cerr<<"ENTRY_PILOT_FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
