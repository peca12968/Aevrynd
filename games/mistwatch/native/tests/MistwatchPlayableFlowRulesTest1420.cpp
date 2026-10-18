#include "MistwatchSession.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace Mistwatch;
namespace Field=FieldRelayTask;
unsigned checks{},patchCases{},routeCases{},sectorCases{},subsetCases{},orderCases{},rejections{};
std::string phase="start";
void Require(bool value,const char* reason) {
    ++checks;if(!value)throw std::runtime_error(reason);
}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(const Session& a,const Session& b) {
    return a.collected==b.collected&&a.health==b.health&&a.bridgeRepaired==b.bridgeRepaired&&
        a.guardianDefeated==b.guardianDefeated&&a.beaconLit==b.beaconLit&&a.fuseCollected==b.fuseCollected&&
        a.powerRestored==b.powerRestored&&a.circuit==b.circuit&&a.repairMaterials==b.repairMaterials&&
        a.fuseClueRead==b.fuseClueRead&&a.fuseDials==b.fuseDials&&a.relayLinked==b.relayLinked&&
        a.relaySectors==b.relaySectors&&a.relayPatchA==b.relayPatchA&&a.relayPatchB==b.relayPatchB&&
        a.relayFieldTasks==b.relayFieldTasks&&a.extractionRoute==b.extractionRoute&&
        SameFloat(a.feet.x,b.feet.x)&&SameFloat(a.feet.y,b.feet.y)&&SameFloat(a.feet.z,b.feet.z);
}
template<class Fn> void Reject(Session& s,Fn fn,const char* reason) {
    const auto before=s;Require(!fn(),reason);Require(Same(s,before),"rejected API changed full Session");
    ++rejections;
}
// 合成規則位置只用來呼叫正式 API；不是碰撞、原生按鍵或真人路線證據。
Session Powered() {
    Session s;
    for(unsigned i=0;i<3;++i){s.feet=Session::supplies[i];Require(s.Collect(i),"supply API failed");}
    Require(s.CurrentStage()==Stage::RepairMaterials,"material stage lost");
    for(unsigned i=0;i<3;++i){s.feet=Session::repairSites[i];Require(s.CollectRepairMaterial(i,i==0),"material API failed");}
    s.feet=Session::bridge;Require(s.RepairBridge(),"bridge API failed");
    s.feet=Session::arena;Require(s.FinishEncounter(),"encounter completion API failed");
    s.feet=Session::rangerLog;Require(s.ReadFuseClue(),"clue API failed");
    s.feet=Session::fuse;
    for(unsigned wheel=0;wheel<3;++wheel)for(unsigned turn=0;turn<Session::fuseCode[wheel];++turn)
        Require(s.CycleFuseDial(wheel),"dial API failed");
    Require(s.CollectFuse(),"fuse API failed");
    s.feet=Session::generator;
    Require(s.ToggleCircuit(0)&&s.ToggleCircuit(2)&&s.RepairGenerator(),"generator APIs failed");
    Require(s.Valid()&&s.CurrentStage()==Stage::Relays,"powered state invalid");return s;
}
constexpr std::array<unsigned,3> SourceTargets{3,10,6};
void ConfirmFor(Session& s,unsigned index) {
    s.feet=Session::relaySites[index];const auto linked=s.relayLinked;
    if(index==0){Require(s.CycleRelayPatch(0)&&s.ConfirmRelayCircuit(),"west task APIs failed");}
    if(index==2){Require(s.CycleExtractionRoute()&&s.ConfirmExtractionRoute(),"north-west task APIs failed");}
    Require(s.relayLinked==linked,"task confirmation also linked antenna");
}
void Aim(Session& s,unsigned index,unsigned sector) {
    for(unsigned turn=0;turn<sector;++turn)Require(s.TuneRelay(index,1),"original tuning API failed");
}
void PatchMatrix() {
    constexpr std::array expected{
        Field::CircuitStatus::Grounded,Field::CircuitStatus::Grounded,Field::CircuitStatus::Grounded,
        Field::CircuitStatus::Connected,Field::CircuitStatus::Grounded,Field::CircuitStatus::Loop,
        Field::CircuitStatus::Loop,Field::CircuitStatus::Loop,Field::CircuitStatus::Loop};
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b) {
        phase="patch-"+std::to_string(a)+"-"+std::to_string(b);
        const auto trace=Field::TraceCircuit(a,b);
        Require(trace.status==expected[a*3+b],"fixed directed graph trace differed");
        Require(trace.pathLength>=3&&trace.pathLength<=4&&trace.path[0]==Field::CircuitNode::Source&&
            trace.path[1]==Field::CircuitNode::ConnectorA,"trace does not follow source-owned graph");
        auto s=Powered();s.feet=Session::relaySites[0];
        for(unsigned n=0;n<a;++n)Require(s.CycleRelayPatch(0),"west A cycle failed");
        for(unsigned n=0;n<b;++n)Require(s.CycleRelayPatch(1),"west B cycle failed");
        Require(s.relayPatchA==a&&s.relayPatchB==b,"cycles did not set requested choices");
        const auto before=s;
        const bool connected=a==1&&b==0;
        Require(s.ConfirmRelayCircuit()==connected,"true confirmation ignored circuit topology");
        if(connected) {
            Require(s.relayFieldTasks==1&&s.relayLinked==0,"confirmation changed unrelated task/link");
            Reject(s,[&]{return s.ConfirmRelayCircuit();},"duplicate west confirmation accepted");
            Reject(s,[&]{return s.CycleRelayPatch(0);},"confirmed west A unlocked");
            Reject(s,[&]{return s.CycleRelayPatch(1);},"confirmed west B unlocked");
        } else Require(Same(s,before),"failed circuit confirmation mutated state");
        Require(s.Valid(),"patch result invalid");++patchCases;
    }
    Require(Field::TraceCircuit(3,0).status==Field::CircuitStatus::Invalid&&
        Field::TraceCircuit(0,3).status==Field::CircuitStatus::Invalid&&
        !Field::CircuitConnected((std::numeric_limits<unsigned>::max)(),0),"out-of-range graph accepted");
}
void RouteMatrix() {
    for(unsigned route=0;route<3;++route) {
        phase="route-"+std::to_string(route);
        auto s=Powered();s.feet=Session::relaySites[2];
        for(unsigned n=0;n<route;++n)Require(s.CycleExtractionRoute(),"route cycle failed");
        const auto before=s;const bool safe=route==1;
        Require(Field::SafeExtractionRoute(route,true)==safe&&!Field::SafeExtractionRoute(route,false),
            "route did not require repaired bridge evidence");
        Require(s.ConfirmExtractionRoute()==safe,"true route confirmation accepted unsafe entrance");
        if(safe) {
            Require(s.relayFieldTasks==2&&s.relayLinked==0,"route confirmation also linked/confirmed west");
            Reject(s,[&]{return s.ConfirmExtractionRoute();},"duplicate route confirmation accepted");
            Reject(s,[&]{return s.CycleExtractionRoute();},"confirmed route unlocked");
        } else Require(Same(s,before),"failed route confirmation mutated state");
        Require(s.Valid(),"route result invalid");++routeCases;
    }
    auto west=Powered();west.feet=Session::relaySites[0];
    for(unsigned n=0;n<3;++n)Require(west.CycleRelayPatch(0)&&west.CycleRelayPatch(1),"patch wrap cycle rejected");
    Require(west.relayPatchA==0&&west.relayPatchB==0,"patch modulo-three wrap wrong");
    auto north=Powered();north.feet=Session::relaySites[2];
    for(unsigned n=0;n<3;++n)Require(north.CycleExtractionRoute(),"route wrap cycle rejected");
    Require(north.extractionRoute==0,"route modulo-three wrap wrong");
}
void OriginalAntennaGates() {
    for(unsigned index=0;index<3;++index)for(unsigned sector=0;sector<16;++sector) {
        phase="sector-"+std::to_string(index)+"-"+std::to_string(sector);
        auto s=Powered();s.feet=Session::relaySites[index];
        Require(Session::RelayTargetSector(index)==SourceTargets[index],"original source target changed");
        Aim(s,index,sector);
        Require(s.RelaySector(index)==sector,"original 16-sector tuning changed");
        const bool aimed=sector==SourceTargets[index];
        Require((s.RelaySignal(index)==100)==aimed,"original signal lock changed");
        if(index!=1)Reject(s,[&]{return s.ConnectRelay(index);},"antenna bypassed new local task");
        ConfirmFor(s,index);
        const auto before=s;
        Require(s.ConnectRelay(index)==aimed,"link gate ignored original sector");
        if(aimed) {
            Require(s.relayLinked==(1u<<index),"link changed another station");
            Reject(s,[&]{return s.ConnectRelay(index);},"duplicate link accepted");
            Reject(s,[&]{return s.TuneRelay(index,1);},"linked antenna rotated");
        } else Require(Same(s,before),"wrong-sector link mutated Session");
        Require(s.Valid(),"antenna state invalid");++sectorCases;
    }
    auto s=Powered();s.feet=Session::relaySites[0];
    Require(s.TuneRelay(0,-1)&&s.RelaySector(0)==15,"original left wrap changed");
    Reject(s,[&]{return s.TuneRelay(0,0);},"zero tuning direction accepted");
    Reject(s,[&]{return s.TuneRelay(0,2);},"invalid tuning direction accepted");
    Reject(s,[&]{return s.TuneRelay(3,1);},"invalid tuning index accepted");
    Reject(s,[&]{return s.ConnectRelay(3);},"invalid link index accepted");
}
bool Invoke(Session& s,unsigned operation) {
    switch(operation) {
    case 0:return s.CycleRelayPatch(0);
    case 1:return s.ConfirmRelayCircuit();
    case 2:return s.CycleExtractionRoute();
    case 3:return s.ConfirmExtractionRoute();
    default:return false;
    }
}
Session OperationSeed(unsigned operation) {
    auto s=Powered();s.feet=Session::relaySites[operation<2?0:2];
    if(operation==1)Require(s.CycleRelayPatch(0),"operation circuit seed failed");
    if(operation==3)Require(s.CycleExtractionRoute(),"operation route seed failed");
    return s;
}
void NewApiGuards() {
    for(unsigned operation=0;operation<4;++operation) {
        phase="guards-"+std::to_string(operation);
        Session early;early.feet=Session::relaySites[operation<2?0:2];
        Reject(early,[&]{return Invoke(early,operation);},"new operation before power accepted");
        for(unsigned reason=0;reason<5;++reason) {
            auto s=OperationSeed(operation);
            if(reason==0)s.Damage(100);
            if(reason==1)s.feet.x+=10;
            if(reason==2)s.feet.y+=.2f;
            if(reason==3)s.feet.x=(std::numeric_limits<float>::quiet_NaN)();
            if(reason==4)s.feet.z=(std::numeric_limits<float>::infinity)();
            Reject(s,[&]{return Invoke(s,operation);},"dead/far/airborne/nonfinite operation accepted");
        }
    }
    auto s=Powered();s.feet=Session::relaySites[0];
    Reject(s,[&]{return s.CycleRelayPatch(2);},"invalid connector accepted");
    Reject(s,[&]{return s.CycleRelayPatch((std::numeric_limits<unsigned>::max)());},"overflow connector accepted");
    Reject(s,[&]{return s.CycleExtractionRoute();},"route changed at west station");
    s.feet=Session::relaySites[2];
    Reject(s,[&]{return s.CycleRelayPatch(0);},"patch changed at north-west station");
    // 只取原門檻相鄰的可表示 float 內外點，不加 epsilon 或改寫 NearRelay。
    for(unsigned axis=0;axis<2;++axis)for(unsigned outside=0;outside<2;++outside) {
        phase="original-boundary-"+std::to_string(axis)+"-"+std::to_string(outside);
        auto b=Powered();const auto p=Session::relaySites[0];b.feet=p;
        if(axis==0) {
            b.feet.x=std::nextafter(p.x+1.65f,outside?(std::numeric_limits<float>::infinity)():p.x);
            b.feet.y=GroundElevation(b.feet.x,b.feet.z);
        } else {
            b.feet.y=std::nextafter(p.y+.12f,outside?(std::numeric_limits<float>::infinity)():p.y);
        }
        const auto before=b;
        Require(b.NearRelay(0)==!outside,"original representable near-boundary classification changed");
        Require(b.CycleRelayPatch(0)==!outside,"new API did not retain original near-boundary gate");
        if(outside)Require(Same(b,before),"outside-boundary API mutated state");
    }
}
void InvalidState() {
    auto beacon=Powered();beacon.feet=Session::beacon;
    beacon.relayLinked=7;beacon.relaySectors=1699;beacon.relayFieldTasks=3;
    beacon.relayPatchA=2;beacon.extractionRoute=1;
    Require(!beacon.Valid(),"forged confirmations treated as valid");
    Reject(beacon,[&]{return beacon.LightBeacon();},"beacon bypassed true circuit state");
    for(unsigned bad=0;bad<11;++bad) {
        phase="invalid-state-"+std::to_string(bad);auto s=Powered();s.feet=Session::relaySites[0];
        switch(bad) {
        case 0:s.relayPatchA=3;break;case 1:s.relayPatchB=3;break;
        case 2:s.relayFieldTasks=4;break;case 3:s.extractionRoute=3;break;
        case 4:s.relayFieldTasks=1;break;case 5:s.relayFieldTasks=2;break;
        case 6:s.relayLinked=1;s.relaySectors=3;break;
        case 7:s.relayLinked=4;s.relaySectors=6u<<8;break;
        case 8:s.powerRestored=false;s.relayPatchA=1;break;
        case 9:s.powerRestored=false;s.extractionRoute=1;break;
        case 10:s.beaconLit=true;break;
        }
        Require(!s.Valid()&&s.Serialize().empty(),"invalid new state serialized");
        for(unsigned op=0;op<4;++op)Reject(s,[&]{return Invoke(s,op);},"new API repaired invalid live state");
    }
}
void SubsetsOrdersAndRetry() {
    for(unsigned mask=0;mask<8;++mask) {
        phase="linked-subset-"+std::to_string(mask);auto s=Powered();
        for(unsigned index=0;index<3;++index)if(mask&(1u<<index)) {
            ConfirmFor(s,index);Aim(s,index,SourceTargets[index]);Require(s.ConnectRelay(index),"subset link failed");
        }
        Require(s.Valid()&&s.relayLinked==mask,"linked subset lost");
        Require(s.CurrentStage()==(mask==7?Stage::Beacon:Stage::Relays),"subset stage wrong");
        const auto progress=s;s.Damage(100);s.Retry();
        Require(s.health==100&&s.feet.x==Session::bridge.x&&s.feet.y==Session::bridge.y&&s.feet.z==Session::bridge.z,
            "Retry moved to wrong checkpoint");
        auto expected=progress;expected.health=100;expected.feet=Session::bridge;
        Require(Same(s,expected),"Retry dropped old/new fields");
        s.feet=Session::beacon;const auto before=s;
        Require(s.LightBeacon()==(mask==7),"beacon bypassed complete network");
        if(mask!=7)Require(Same(s,before),"failed beacon action changed state");
        else {
            Require(s.CurrentStage()==Stage::Complete&&s.relayFieldTasks==3,"complete state lost tasks");
            Reject(s,[&]{return s.LightBeacon();},"repeat completion accepted");
            s.Damage(100);s.Retry();Require(s.beaconLit&&s.Valid(),"complete Retry lost result");
        }
        ++subsetCases;
    }
    constexpr std::array<std::array<unsigned,3>,6> orders{{{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}}};
    for(unsigned n=0;n<orders.size();++n) {
        phase="order-"+std::to_string(n);auto s=Powered();
        for(const auto index:orders[n]){ConfirmFor(s,index);Aim(s,index,SourceTargets[index]);Require(s.ConnectRelay(index),"station order was locked");}
        s.feet=Session::beacon;Require(s.LightBeacon()&&s.Valid(),"order did not complete");++orderCases;
    }
    Session fresh;Require(fresh.relayPatchA==0&&fresh.relayPatchB==0&&fresh.relayFieldTasks==0&&fresh.extractionRoute==0,
        "new Session contains prior tasks");
}
void LegacySubsets() {
    for(unsigned mask=0;mask<8;++mask) {
        phase="legacy6-subset-"+std::to_string(mask);
        const unsigned angles=((mask&1)?3u:15u)|(((mask&2)?10u:7u)<<4)|(((mask&4)?6u:13u)<<8);
        const std::string text="MISTWATCH 6 7 100 1 1 0 0 0 70 1 1 7 7 1 50 "+std::to_string(mask)+" "+std::to_string(angles);
        Session s;Require(s.Restore(text)&&s.Valid(),"legacy partial subset rejected");
        Require(s.relayLinked==mask&&s.relaySectors==angles,"legacy linked/unlinked angles changed");
        Require(s.relayFieldTasks==(((mask&1)?1u:0u)|((mask&4)?2u:0u)),"legacy confirmation mask wrong");
        Require(s.relayPatchA==((mask&1)?1u:0u)&&s.relayPatchB==0&&s.extractionRoute==((mask&4)?1u:0u),
            "legacy canonical choices wrong");
        Require(s.Serialize().starts_with("MISTWATCH 7 "),"new serialization still version6");
        Session round;Require(round.Restore(s.Serialize())&&Same(round,s),"migrated subset did not roundtrip");
    }
}
}
int main() {
    try {
        PatchMatrix();RouteMatrix();OriginalAntennaGates();NewApiGuards();InvalidState();SubsetsOrdersAndRetry();LegacySubsets();
        Require(patchCases==9&&routeCases==3&&sectorCases==48&&subsetCases==8&&orderCases==6,"finite rules coverage incomplete");
        std::cout<<"RULES1420 PASS checks="<<checks<<" patch_cases="<<patchCases<<" route_cases="<<routeCases
            <<" sectors="<<sectorCases<<" linked_subsets="<<subsetCases<<" station_orders="<<orderCases
            <<" rejected_api_calls="<<rejections<<" native_input_executed=0 human_play_executed=0\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FIRST_DIVERGENCE RULES1420 phase="<<phase<<" checks="<<checks<<" reason="<<error.what()<<'\n';return 1;
    }
}
