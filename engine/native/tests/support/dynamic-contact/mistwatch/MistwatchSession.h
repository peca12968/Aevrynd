#pragma once
#include "MistwatchTerrainProfile.h"
#include "MistwatchFieldRelayTask.h"
#include <Aevrynd/Math.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <locale>

namespace Mistwatch {
enum class Stage : unsigned { Supplies, RepairMaterials, Bridge, Encounter, Fuse, Generator, Relays, Beacon, Complete };
struct Session {
    unsigned collected{}, health{100};
    bool bridgeRepaired{}, guardianDefeated{}, beaconLit{};
    bool fuseCollected{}, powerRestored{};
    unsigned circuit{};
    unsigned repairMaterials{};
    bool fuseClueRead{};
    unsigned fuseDials{};
    unsigned relayLinked{},relaySectors{};
    unsigned relayPatchA{},relayPatchB{},relayFieldTasks{},extractionRoute{};
    Aevrynd::Vec3 feet{};
    static constexpr std::array<Aevrynd::Vec3, 3> supplies{{{4,0,6},{-5,0,14},{8,0,22}}};
    static constexpr Aevrynd::Vec3 bridge{0,0,30}, arena{0,0,48}, beacon{0,0,70};
    static constexpr Aevrynd::Vec3 fuse{-7,0,59}, generator{7,0,65};
    static constexpr Aevrynd::Vec3 rangerLog{8,0,79};
    inline static const std::array<Aevrynd::Vec3,3> relaySites{{
        {-22,GroundElevation(-22,62),62},{22,GroundElevation(22,86),86},{-20,GroundElevation(-20,94),94}}};
    static constexpr std::array<unsigned,3> fuseCode{2,0,3};
    static constexpr unsigned solvedFuseDials=fuseCode[0]|(fuseCode[1]<<2)|(fuseCode[2]<<4);
    // 工具位於低矮避雨棚內，兩處木料沿南岸支線分布。
    static constexpr std::array<Aevrynd::Vec3,3> repairSites{{{-8,0,18},{8,0,-8},{-8,0,26}}};
    bool Alive() const { return health != 0; }
    Stage CurrentStage() const {
        if (beaconLit) return Stage::Complete;
        if (powerRestored) return relayLinked==7&&relayFieldTasks==FieldRelayTask::AllTasks?Stage::Beacon:Stage::Relays;
        if (fuseCollected) return Stage::Generator;
        if (guardianDefeated) return Stage::Fuse;
        if (bridgeRepaired) return Stage::Encounter;
        if(collected!=7)return Stage::Supplies;
        return repairMaterials==7?Stage::Bridge:Stage::RepairMaterials;
    }
    static bool Finite(Aevrynd::Vec3 p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }
    bool Near(Aevrynd::Vec3 p) const {
        return Finite(feet) && std::abs(feet.y-p.y)<=2.f &&
            std::hypot(feet.x-p.x,feet.z-p.z)<=2.5f;
    }
    bool Collect(unsigned index) {
        if (!Alive() || index>=supplies.size() || (collected&(1u<<index)) || !Near(supplies[index])) return false;
        collected |= 1u<<index;
        return true;
    }
    bool RepairBridge() {
        if (!Alive() || CurrentStage()!=Stage::Bridge || !Near(bridge)) return false;
        bridgeRepaired=true;
        return true;
    }
    bool CollectRepairMaterial(unsigned index,bool crouching) {
        if(!Alive() || bridgeRepaired || index>=repairSites.size() ||
           (repairMaterials&(1u<<index)) || !Near(repairSites[index]))return false;
        // 工具不可隔著牆或從棚外遠距取走；必須蹲行至棚內的近距離取物點。
        if(index==0 && (!crouching || std::hypot(feet.x-repairSites[0].x,feet.z-repairSites[0].z)>.8f ||
            std::abs(feet.y-repairSites[0].y)>.3f))return false;
        repairMaterials|=1u<<index;return true;
    }
    bool FinishEncounter(Aevrynd::Vec3 enemyFeet=arena) {
        if (!Alive() || CurrentStage()!=Stage::Encounter || !Finite(enemyFeet) || !Near(enemyFeet)) return false;
        guardianDefeated=true;
        return true;
    }
    bool LightBeacon() {
        if (!Alive() || CurrentStage()!=Stage::Beacon || !Valid() || relayFieldTasks!=FieldRelayTask::AllTasks || !Near(beacon)) return false;
        beaconLit=true;
        return true;
    }
    unsigned RelaySector(unsigned index) const {return index<3?(relaySectors>>(index*4))&15u:0;}
    static unsigned RelayTargetSector(unsigned index) {
        if(index>=relaySites.size())return 0;
        const auto delta=beacon-relaySites[index];
        float angle=std::atan2(delta.x,delta.z);
        if(angle<0)angle+=6.2831853f;
        return unsigned(std::floor(angle*16.f/6.2831853f+.5f))&15u;
    }
    unsigned RelaySignal(unsigned index) const {
        if(index>=3||!powerRestored)return 0;
        const unsigned sector=RelaySector(index),target=RelayTargetSector(index);
        const unsigned distance=(std::min)((sector+16-target)&15u,(target+16-sector)&15u);
        return 100-distance*100/8;
    }
    bool NearRelay(unsigned index) const {
        return index<relaySites.size()&&Finite(feet)&&std::abs(feet.y-GroundElevation(feet.x,feet.z))<=.12f&&
            std::hypot(feet.x-relaySites[index].x,feet.z-relaySites[index].z)<=1.65f;
    }
    bool TuneRelay(unsigned index,int direction) {
        if(!Alive()||CurrentStage()!=Stage::Relays||index>=3||(direction!=1&&direction!=-1)||
            (relayLinked&(1u<<index))||!NearRelay(index))return false;
        const unsigned shift=index*4;
        const unsigned sector=unsigned(int(RelaySector(index))+direction+16)&15u;
        relaySectors=(relaySectors&~(15u<<shift))|(sector<<shift);return true;
    }
    // 新操作只在原站點地面近場作用；一次呼叫只提交一項確認，不順便連站。
    bool CycleRelayPatch(unsigned connector) {
        if(!Alive()||CurrentStage()!=Stage::Relays||connector>=2||!Valid()||
           (relayFieldTasks&FieldRelayTask::WestCircuitBit)||(relayLinked&1u)||
           !NearRelay(FieldRelayTask::WestRelay))return false;
        auto& choice=connector==0?relayPatchA:relayPatchB;
        choice=(choice+1u)%FieldRelayTask::PatchOptions;return true;
    }
    bool ConfirmRelayCircuit() {
        if(!Alive()||CurrentStage()!=Stage::Relays||!Valid()||
           (relayFieldTasks&FieldRelayTask::WestCircuitBit)||(relayLinked&1u)||
           !NearRelay(FieldRelayTask::WestRelay)||
           !FieldRelayTask::CircuitConnected(relayPatchA,relayPatchB))return false;
        relayFieldTasks|=FieldRelayTask::WestCircuitBit;return true;
    }
    bool CycleExtractionRoute() {
        if(!Alive()||CurrentStage()!=Stage::Relays||!Valid()||
           (relayFieldTasks&FieldRelayTask::NorthWestRouteBit)||(relayLinked&4u)||
           !NearRelay(FieldRelayTask::NorthWestRelay))return false;
        extractionRoute=(extractionRoute+1u)%FieldRelayTask::RouteOptions;return true;
    }
    bool ConfirmExtractionRoute() {
        if(!Alive()||CurrentStage()!=Stage::Relays||!Valid()||
           (relayFieldTasks&FieldRelayTask::NorthWestRouteBit)||(relayLinked&4u)||
           !NearRelay(FieldRelayTask::NorthWestRelay)||
           !FieldRelayTask::SafeExtractionRoute(extractionRoute,bridgeRepaired))return false;
        relayFieldTasks|=FieldRelayTask::NorthWestRouteBit;return true;
    }
    bool RelayFieldTaskReady(unsigned index) const {
        if(!Valid())return false;
        if(index==FieldRelayTask::WestRelay)return (relayFieldTasks&FieldRelayTask::WestCircuitBit)&&
            FieldRelayTask::CircuitConnected(relayPatchA,relayPatchB);
        if(index==FieldRelayTask::NorthWestRelay)return (relayFieldTasks&FieldRelayTask::NorthWestRouteBit)&&
            FieldRelayTask::SafeExtractionRoute(extractionRoute,bridgeRepaired);
        return index==1;
    }
    bool ConnectRelay(unsigned index) {
        if(!Alive()||CurrentStage()!=Stage::Relays||index>=3||(relayLinked&(1u<<index))||
            !NearRelay(index)||RelaySector(index)!=RelayTargetSector(index)||!RelayFieldTaskReady(index))return false;
        relayLinked|=1u<<index;return true;
    }
    bool NearInterior(Aevrynd::Vec3 point) const {
        // 線索與鎖櫃要求角色進入室內，不能隔牆或站在屋頂互動。
        return Finite(feet)&&std::abs(feet.y-point.y)<=.35f&&std::hypot(feet.x-point.x,feet.z-point.z)<=.95f;
    }
    unsigned FuseDial(unsigned wheel) const {return wheel<3?(fuseDials>>(wheel*2))&3u:0;}
    bool ReadFuseClue() {
        if(!Alive()||CurrentStage()!=Stage::Fuse||fuseClueRead||!NearInterior(rangerLog))return false;
        fuseClueRead=true;return true;
    }
    bool CycleFuseDial(unsigned wheel) {
        if(!Alive()||CurrentStage()!=Stage::Fuse||wheel>=3||!NearInterior(fuse))return false;
        const unsigned shift=wheel*2;
        fuseDials=(fuseDials&~(3u<<shift))|(((FuseDial(wheel)+1u)&3u)<<shift);return true;
    }
    bool CollectFuse() {
        if(!Alive() || CurrentStage()!=Stage::Fuse || !NearInterior(fuse) || !fuseClueRead || fuseDials!=solvedFuseDials) return false;
        fuseCollected=true;return true;
    }
    bool RepairGenerator() {
        if(!Alive() || CurrentStage()!=Stage::Generator || !Near(generator) || circuit!=7) return false;
        powerRestored=true;return true;
    }
    bool ToggleCircuit(unsigned lever) {
        if(!Alive() || CurrentStage()!=Stage::Generator || !Near(generator) || lever>=3) return false;
        constexpr unsigned masks[]{3,6,4};
        circuit^=masks[lever];return true;
    }
    void Damage(unsigned amount) { health=amount>=health?0:health-amount; }
    void Retry() {
        health=100;
        feet=bridgeRepaired?bridge:Aevrynd::Vec3{};
    }
    bool Valid() const {
        if(relayPatchA>=FieldRelayTask::PatchOptions||relayPatchB>=FieldRelayTask::PatchOptions||
           relayFieldTasks>FieldRelayTask::AllTasks||extractionRoute>=FieldRelayTask::RouteOptions)return false;
        if((relayPatchA||relayPatchB||relayFieldTasks||extractionRoute)&&!powerRestored)return false;
        if((relayFieldTasks&FieldRelayTask::WestCircuitBit)&&
           !FieldRelayTask::CircuitConnected(relayPatchA,relayPatchB))return false;
        if((relayFieldTasks&FieldRelayTask::NorthWestRouteBit)&&
           !FieldRelayTask::SafeExtractionRoute(extractionRoute,bridgeRepaired))return false;
        if(((relayLinked&1u)&&!(relayFieldTasks&FieldRelayTask::WestCircuitBit))||
           ((relayLinked&4u)&&!(relayFieldTasks&FieldRelayTask::NorthWestRouteBit)))return false;
        if(relayLinked>7||relaySectors>4095||((relayLinked||relaySectors)&&!powerRestored))return false;
        for(unsigned index=0;index<3;++index)if((relayLinked&(1u<<index))&&RelaySector(index)!=RelayTargetSector(index))return false;
        return health<=100 && collected<=7 && repairMaterials<=7 && circuit<=7 && (!circuit || fuseCollected) && (!powerRestored || circuit==7) && Finite(feet) &&
            fuseDials<=63 && (!fuseClueRead || guardianDefeated) && (!fuseDials || guardianDefeated) &&
            (!fuseCollected || (fuseClueRead && fuseDials==solvedFuseDials)) &&
            std::abs(feet.x)<=10000 && std::abs(feet.y)<=10000 && std::abs(feet.z)<=10000 &&
            (!bridgeRepaired || (collected==7 && repairMaterials==7)) && (!guardianDefeated || bridgeRepaired) &&
            (!fuseCollected || guardianDefeated) && (!powerRestored || fuseCollected) &&
            (!beaconLit || (powerRestored&&relayLinked==7));
    }
    std::string Serialize() const {
        if (!Valid()) return {};
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out.precision(9);
        out << "MISTWATCH 7 " << collected << ' ' << health << ' ' << bridgeRepaired << ' '
            << guardianDefeated << ' ' << beaconLit << ' ' << feet.x << ' ' << feet.y << ' ' << feet.z
            << ' ' << fuseCollected << ' ' << powerRestored << ' ' << circuit << ' ' << repairMaterials
            << ' ' << fuseClueRead << ' ' << fuseDials << ' ' << relayLinked << ' ' << relaySectors
            << ' ' << relayPatchA << ' ' << relayPatchB << ' ' << relayFieldTasks << ' ' << extractionRoute;
        return out.str();
    }
    bool Restore(std::string_view text) {
        if (text.size()>512) return false;
        std::istringstream in{std::string(text)};
        in.imbue(std::locale::classic());
        std::string magic, extra;
        unsigned version{}, repaired{}, defeated{}, lit{};
        Session candidate;
        if (!(in>>magic>>version>>candidate.collected>>candidate.health>>repaired>>defeated>>lit
              >>candidate.feet.x>>candidate.feet.y>>candidate.feet.z) || magic!="MISTWATCH" || (version<1 || version>7) ||
              repaired>1 || defeated>1 || lit>1) return false;
        unsigned fuseValue{},powerValue{};
        if(version>=2) {
            if(!(in>>fuseValue>>powerValue)||fuseValue>1||powerValue>1) return false;
        } else if(lit) fuseValue=powerValue=1; // 保留舊版已完成的進度。
        if(version>=3) {if(!(in>>candidate.circuit)) return false;}
        else candidate.circuit=powerValue?7:0;
        if(version>=4) {if(!(in>>candidate.repairMaterials)) return false;}
        else candidate.repairMaterials=repaired?7:0; // 已修復舊檔保留通路，不重置既有完成進度。
        if(version>=5) {
            unsigned clue{};
            if(!(in>>clue>>candidate.fuseDials)||clue>1)return false;
            candidate.fuseClueRead=clue!=0;
        } else {
            // 舊檔已取出的保險絲與後續供電狀態不退回；未取者進入聚落探索。
            candidate.fuseClueRead=fuseValue!=0;
            candidate.fuseDials=fuseValue?solvedFuseDials:0;
        }
        if(version>=6) {
            if(!(in>>candidate.relayLinked>>candidate.relaySectors))return false;
        } else if(lit) {
            // 舊版已完成的救援不倒退；未完成者保留供電進度並進入新中繼任務。
            candidate.relayLinked=7;
            for(unsigned index=0;index<3;++index)candidate.relaySectors|=RelayTargetSector(index)<<(index*4);
        }
        if(version>=7) {
            if(!(in>>candidate.relayPatchA>>candidate.relayPatchB>>candidate.relayFieldTasks>>candidate.extractionRoute))return false;
        } else {
            // 舊版任意部分連線都保留；補值只表示相容，不偽稱舊玩家已做過新判斷。
            if(candidate.relayLinked&1u) {
                candidate.relayPatchA=1;candidate.relayPatchB=0;
                candidate.relayFieldTasks|=FieldRelayTask::WestCircuitBit;
            }
            if(candidate.relayLinked&4u) {
                candidate.extractionRoute=static_cast<unsigned>(FieldRelayTask::ExtractionRoute::RepairedBridge);
                candidate.relayFieldTasks|=FieldRelayTask::NorthWestRouteBit;
            }
        }
        if(in>>extra) return false;
        candidate.fuseCollected=fuseValue!=0;candidate.powerRestored=powerValue!=0;
        candidate.bridgeRepaired=repaired!=0;
        candidate.guardianDefeated=defeated!=0;
        candidate.beaconLit=lit!=0;
        if (!candidate.Valid()) return false;
        *this=candidate;
        return true;
    }
    bool RestoreStream(std::istream& in) {
        // 多讀一個位元組即可判斷超限，不把任意大小的檔案載入記憶體。
        std::array<char,513> bytes{};
        in.read(bytes.data(),bytes.size());
        const auto count=in.gcount();
        if(in.bad() || count>512) return false;
        return Restore(std::string_view(bytes.data(),static_cast<std::size_t>(count)));
    }
};
}
