#pragma once
#include <Aevrynd/Math.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <locale>

namespace Mistwatch {
enum class Stage : unsigned { Supplies, RepairMaterials, Bridge, Encounter, Fuse, Generator, Beacon, Complete };
struct Session {
    unsigned collected{}, health{100};
    bool bridgeRepaired{}, guardianDefeated{}, beaconLit{};
    bool fuseCollected{}, powerRestored{};
    unsigned circuit{};
    unsigned repairMaterials{};
    Aevrynd::Vec3 feet{};
    static constexpr std::array<Aevrynd::Vec3, 3> supplies{{{4,0,6},{-5,0,14},{8,0,22}}};
    static constexpr Aevrynd::Vec3 bridge{0,0,30}, arena{0,0,48}, beacon{0,0,70};
    static constexpr Aevrynd::Vec3 fuse{-7,0,59}, generator{7,0,65};
    // 工具位於低矮避雨棚內，兩處木料沿南岸支線分布。
    static constexpr std::array<Aevrynd::Vec3,3> repairSites{{{-8,0,18},{8,0,-8},{-8,0,26}}};
    bool Alive() const { return health != 0; }
    Stage CurrentStage() const {
        if (beaconLit) return Stage::Complete;
        if (powerRestored) return Stage::Beacon;
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
        if (!Alive() || CurrentStage()!=Stage::Beacon || !Near(beacon)) return false;
        beaconLit=true;
        return true;
    }
    bool CollectFuse() {
        if(!Alive() || CurrentStage()!=Stage::Fuse || !Near(fuse)) return false;
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
        return health<=100 && collected<=7 && repairMaterials<=7 && circuit<=7 && (!circuit || fuseCollected) && (!powerRestored || circuit==7) && Finite(feet) &&
            std::abs(feet.x)<=10000 && std::abs(feet.y)<=10000 && std::abs(feet.z)<=10000 &&
            (!bridgeRepaired || (collected==7 && repairMaterials==7)) && (!guardianDefeated || bridgeRepaired) &&
            (!fuseCollected || guardianDefeated) && (!powerRestored || fuseCollected) &&
            (!beaconLit || powerRestored);
    }
    std::string Serialize() const {
        if (!Valid()) return {};
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out.precision(9);
        out << "MISTWATCH 4 " << collected << ' ' << health << ' ' << bridgeRepaired << ' '
            << guardianDefeated << ' ' << beaconLit << ' ' << feet.x << ' ' << feet.y << ' ' << feet.z
            << ' ' << fuseCollected << ' ' << powerRestored << ' ' << circuit << ' ' << repairMaterials;
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
              >>candidate.feet.x>>candidate.feet.y>>candidate.feet.z) || magic!="MISTWATCH" || (version<1 || version>4) ||
              repaired>1 || defeated>1 || lit>1) return false;
        unsigned fuseValue{},powerValue{};
        if(version>=2) {
            if(!(in>>fuseValue>>powerValue)||fuseValue>1||powerValue>1) return false;
        } else if(lit) fuseValue=powerValue=1; // 保留舊版已完成的進度。
        if(version>=3) {if(!(in>>candidate.circuit)) return false;}
        else candidate.circuit=powerValue?7:0;
        if(version>=4) {if(!(in>>candidate.repairMaterials)) return false;}
        else candidate.repairMaterials=repaired?7:0; // 已修復舊檔保留通路，不重置既有完成進度。
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
