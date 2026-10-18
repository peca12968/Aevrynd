#include "MistwatchSaveFile.h"
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace Mistwatch;
unsigned checks{},roundtrips{},diskRoundtrips{},choiceCases{},subsetCases{},legacyVersions{},legacy6Cases{},legacyWrappers{},rejections{};
std::string phase="output-ownership",lastInput,lastSnapshot;
std::filesystem::path checkpoint;
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
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

bool SameVector(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return SameFloat(a.x,b.x)&&SameFloat(a.y,b.y)&&SameFloat(a.z,b.z);}
bool SameEnemy(const Encounter& a,const Encounter& b) {
    return a.health==b.health&&a.phase==b.phase&&a.countered==b.countered&&SameVector(a.feet,b.feet)&&
        SameVector(a.strikeTarget,b.strikeTarget)&&SameFloat(a.phaseTime,b.phaseTime)&&SameFloat(a.playerCooldown,b.playerCooldown);
}
bool SameEvasion(const Evasion& a,const Evasion& b) {
    return SameVector(a.direction,b.direction)&&SameFloat(a.remaining,b.remaining)&&SameFloat(a.cooldown,b.cooldown);
}
bool SameGame(const SaveGame& a,const SaveGame& b){return Same(a.session,b.session)&&SameEnemy(a.encounter,b.encounter)&&SameEvasion(a.evasion,b.evasion);}
std::string Bytes(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);Require(bool(in),"saved file unavailable");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
void RoundTrip(const SaveGame& original,bool disk=false) {
    Require(original.Valid(),"roundtrip source invalid");
    const auto text=original.Serialize();lastSnapshot=text;
    Require(!text.empty()&&text.size()<=SaveGame::MaximumBytes&&text.starts_with("MISTWATCH_SAVE 2\nMISTWATCH 7 "),
        "complete snapshot wrapper/session format changed");
    SaveGame loaded;lastInput=text;
    Require(loaded.Restore(text)&&SameGame(loaded,original)&&loaded.Serialize()==text,"SaveGame new/old fields did not roundtrip");
    std::istringstream stream(text);SaveGame fromStream;
    Require(fromStream.RestoreStream(stream)&&SameGame(fromStream,original),"bounded complete snapshot restore differed");
    ++roundtrips;
    if(disk) {
        Require(WriteSaveFile(checkpoint,original)==SaveFileResult::Saved,"actual WriteSaveFile failed");
        Require(Bytes(checkpoint)==text,"actual saved bytes differ from snapshot");
        SaveGame read;Require(ReadSaveFile(checkpoint,read)&&SameGame(read,original)&&read.Serialize()==text,
            "actual ReadSaveFile lost complete new/old fields");++diskRoundtrips;
    }
}
// 此處只合成規則位置；戰鬥仍用真 Encounter.Tick，沒有 native 輸入或世界碰撞宣稱。
SaveGame BridgeGame() {
    SaveGame g;
    for(unsigned i=0;i<3;++i){g.session.feet=Session::supplies[i];Require(g.session.Collect(i),"supply API failed");}
    for(unsigned i=0;i<3;++i){g.session.feet=Session::repairSites[i];Require(g.session.CollectRepairMaterial(i,i==0),"material API failed");}
    g.session.feet=Session::bridge;Require(g.session.RepairBridge(),"bridge API failed");g.session.feet=Session::arena;
    Require(g.Valid(),"bridge snapshot invalid");return g;
}
void BattleTick(SaveGame& g,bool attack) {
    g.session.Damage(g.encounter.Tick(g.session.feet,.02f,attack));
    if(!g.encounter.health)Require(g.session.FinishEncounter(g.encounter.feet),"defeat API did not advance Session");
}
SaveGame Powered() {
    auto g=BridgeGame();
    for(unsigned tick=0;tick<1200&&g.encounter.health;++tick)BattleTick(g,true);
    Require(g.session.Alive()&&g.session.guardianDefeated&&g.encounter.phase==EnemyPhase::Defeated,"finite battle fixture failed");
    g.session.feet=Session::rangerLog;Require(g.session.ReadFuseClue(),"clue API failed");
    g.session.feet=Session::fuse;
    for(unsigned wheel=0;wheel<3;++wheel)for(unsigned turn=0;turn<Session::fuseCode[wheel];++turn)
        Require(g.session.CycleFuseDial(wheel),"dial API failed");
    Require(g.session.CollectFuse(),"fuse API failed");g.session.feet=Session::generator;
    Require(g.session.ToggleCircuit(0)&&g.session.ToggleCircuit(2)&&g.session.RepairGenerator(),"power APIs failed");
    Require(g.Valid(),"powered snapshot invalid");return g;
}
constexpr std::array<unsigned,3> Targets{3,10,6};
void LocalTask(SaveGame& g,unsigned index) {
    g.session.feet=Session::relaySites[index];
    if(index==0)Require(g.session.CycleRelayPatch(0)&&g.session.ConfirmRelayCircuit(),"west local APIs failed");
    if(index==2)Require(g.session.CycleExtractionRoute()&&g.session.ConfirmExtractionRoute(),"route local APIs failed");
}
void Link(SaveGame& g,unsigned index) {
    LocalTask(g,index);
    for(unsigned n=0;n<Targets[index];++n)Require(g.session.TuneRelay(index,1),"original tuning failed");
    Require(g.session.ConnectRelay(index),"new link gate failed");
}
void ChoiceSnapshots() {
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b)for(unsigned route=0;route<3;++route) {
        phase="choice-snapshot-"+std::to_string(a)+"-"+std::to_string(b)+"-"+std::to_string(route);
        auto g=Powered();g.session.feet=Session::relaySites[0];
        for(unsigned n=0;n<a;++n)Require(g.session.CycleRelayPatch(0),"snapshot A choice failed");
        for(unsigned n=0;n<b;++n)Require(g.session.CycleRelayPatch(1),"snapshot B choice failed");
        g.session.feet=Session::relaySites[2];
        for(unsigned n=0;n<route;++n)Require(g.session.CycleExtractionRoute(),"snapshot route choice failed");
        Require(g.session.relayPatchA==a&&g.session.relayPatchB==b&&g.session.extractionRoute==route,
            "actual APIs did not produce requested choices");RoundTrip(g,true);
        if(route==1){Require(g.session.ConfirmExtractionRoute(),"independent north confirmation failed");RoundTrip(g,true);}
        if(a==1&&b==0){g.session.feet=Session::relaySites[0];Require(g.session.ConfirmRelayCircuit(),"west confirmation failed");RoundTrip(g,true);}
        ++choiceCases;
    }
}
void LinkedSubsetsAndTerminal() {
    for(unsigned mask=0;mask<8;++mask) {
        phase="linked-snapshot-"+std::to_string(mask);auto g=Powered();
        for(unsigned index=0;index<3;++index)if(mask&(1u<<index))Link(g,index);
        Require(g.session.relayLinked==mask,"snapshot subset wrong");RoundTrip(g,true);
        g.session.Damage(100);Require(!g.session.Alive(),"death fixture failed");RoundTrip(g,true);
        const auto prior=g.session;g.session.Retry();auto expected=prior;expected.health=100;expected.feet=Session::bridge;
        Require(Same(g.session,expected)&&g.Valid(),"Session Retry dropped complete snapshot fields");RoundTrip(g,true);
        if(mask==7) {
            g.session.feet=Session::beacon;Require(g.session.LightBeacon(),"complete API failed");RoundTrip(g,true);
            g.session.Damage(100);RoundTrip(g,true);g.session.Retry();Require(g.session.beaconLit&&g.Valid(),"Complete Retry lost progress");RoundTrip(g,true);
        }
        ++subsetCases;
    }
    phase="new-session-snapshot";RoundTrip(SaveGame{},true);
}
void CombatEvasionSnapshots() {
    auto live=BridgeGame();
    for(unsigned tick=0;tick<1200&&live.encounter.health>1;++tick)BattleTick(live,true);
    Require(live.session.Alive()&&live.encounter.health==1&&live.Valid(),"HP1 fixture failed");
    for(unsigned mode=0;mode<3;++mode) {
        phase="combat-evasion-"+std::to_string(mode);auto g=live;
        const auto initial=g.evasion.Tick(.02f,true,true,false,{1,0,.5f});
        Require(initial.dashing,"true dash API did not start");
        if(mode>0)for(unsigned n=0;n<(mode==1?15u:100u);++n)g.evasion.Tick(.02f,false,true,false,{});
        Require(mode==0?g.evasion.remaining>0:g.evasion.remaining==0,"dash phase wrong");
        Require(mode<2?g.evasion.cooldown>0:g.evasion.cooldown==0,"dash cooldown phase wrong");RoundTrip(g,true);
        SaveGame restored;Require(restored.Restore(g.Serialize()),"combat restore failed");
        const auto a=g.evasion.Tick(.02f,true,true,false,{-1,0,0});
        const auto b=restored.evasion.Tick(.02f,true,true,false,{-1,0,0});
        Require(a.dashing==b.dashing&&SameVector(a.movement,b.movement)&&SameGame(g,restored),
            "restored complete fields changed next dash behavior");
    }
}
std::string Legacy6(unsigned mask,unsigned angles,bool complete=false) {
    return "MISTWATCH 6 7 100 1 1 "+std::string(complete?"1":"0")+" 0 0 70 1 1 7 7 1 50 "+
        std::to_string(mask)+" "+std::to_string(angles);
}
void CheckLegacyChoices(const Session& s,unsigned mask,unsigned angles) {
    Require(s.Valid()&&s.relayLinked==mask&&s.relaySectors==angles,"legacy link/angle preservation failed");
    Require(s.relayPatchA==((mask&1)?1u:0u)&&s.relayPatchB==0&&s.extractionRoute==((mask&4)?1u:0u)&&
        s.relayFieldTasks==(((mask&1)?1u:0u)|((mask&4)?2u:0u)),"explicit legacy canonical choices wrong");
}
void LegacyVersions() {
    constexpr std::array<const char*,6> bridge{{
        "MISTWATCH 1 7 100 1 0 0 0 0 30",
        "MISTWATCH 2 7 100 1 0 0 0 0 30 0 0",
        "MISTWATCH 3 7 100 1 0 0 0 0 30 0 0 0",
        "MISTWATCH 4 7 100 1 0 0 0 0 30 0 0 0 7",
        "MISTWATCH 5 7 100 1 0 0 0 0 30 0 0 0 7 0 0",
        "MISTWATCH 6 7 100 1 0 0 0 0 30 0 0 0 7 0 0 0 0"}};
    constexpr std::array<const char*,5> complete{{
        "MISTWATCH 1 7 100 1 1 1 0 0 70",
        "MISTWATCH 2 7 100 1 1 1 0 0 70 1 1",
        "MISTWATCH 3 7 100 1 1 1 0 0 70 1 1 7",
        "MISTWATCH 4 7 100 1 1 1 0 0 70 1 1 7 7",
        "MISTWATCH 5 7 100 1 1 1 0 0 70 1 1 7 7 1 50"}};
    for(unsigned version=1;version<=6;++version) {
        phase="legacy-version-"+std::to_string(version);SaveGame g;lastInput=bridge[version-1];
        Require(g.Restore(lastInput)&&g.Valid()&&g.session.CurrentStage()==Stage::Encounter&&g.encounter.health==3&&
            g.session.relayFieldTasks==0&&g.session.relayPatchA==0&&g.session.extractionRoute==0,
            "legacy incomplete session failed migration");RoundTrip(g,true);
        lastInput=version<6?std::string(complete[version-1]):Legacy6(7,1699,true);
        Require(g.Restore(lastInput)&&g.Valid()&&g.session.CurrentStage()==Stage::Complete&&
            g.encounter.phase==EnemyPhase::Defeated&&g.encounter.health==0,"legacy completion restarted battle");
        CheckLegacyChoices(g.session,7,1699);RoundTrip(g,true);++legacyVersions;
    }
}
// 8種 linked mask 與每個未連方向的完整笛卡兒積：17^3=4913合法舊狀態。
// 格式1..5沒有部分中繼欄位，不虛構不存在的欄位；它們的未完成/完成另逐版覆蓋。
void ExhaustiveLegacy6() {
    for(unsigned mask=0;mask<8;++mask)for(unsigned angles=0;angles<4096;++angles) {
        bool legal=true;for(unsigned index=0;index<3;++index)
            if((mask&(1u<<index))&&((angles>>(index*4))&15u)!=Targets[index])legal=false;
        if(!legal)continue;
        phase="legacy6-"+std::to_string(mask)+"-"+std::to_string(angles);lastInput=Legacy6(mask,angles);
        Session s;Require(s.Restore(lastInput),"legal old Session6 rejected");CheckLegacyChoices(s,mask,angles);
        SaveGame g;Require(g.Restore(lastInput),"complete SaveGame legacy routing rejected");
        CheckLegacyChoices(g.session,mask,angles);Require(Same(s,g.session),"Session/SaveGame migration differs");
        RoundTrip(g);
        for(unsigned wrapper=1;wrapper<=2;++wrapper) {
            const auto old="MISTWATCH_SAVE "+std::to_string(wrapper)+"\n"+Legacy6(mask,angles)+
                "\nENCOUNTER 0 3 0 0 48 0 0 48 0 0 1\nEVASION 0 0 0 0 0";
            lastInput=old;SaveGame fromWrapper;Require(fromWrapper.Restore(old)&&SameGame(fromWrapper,g),
                "old complete wrapper lost arbitrary partial relay state");++legacyWrappers;
        }
        const unsigned baseline=((mask&1)?3u:15u)|(((mask&2)?10u:7u)<<4)|(((mask&4)?6u:13u)<<8);
        if(angles==baseline)RoundTrip(g,true);++legacy6Cases;
    }
    Require(legacy6Cases==4913&&legacyWrappers==9826,"legacy finite product coverage incomplete");
}
std::vector<std::string> Tokens(const std::string& line) {
    std::istringstream in(line);std::vector<std::string> fields;std::string field;
    while(in>>field)fields.push_back(field);return fields;
}
std::string Join(const std::vector<std::string>& fields) {
    std::string result;for(const auto& field:fields){if(!result.empty())result+=' ';result+=field;}return result;
}
std::string ReplaceSession(const std::string& snapshot,const std::string& sessionLine) {
    const auto first=snapshot.find('\n'),end=snapshot.find("\nENCOUNTER ");
    Require(first!=std::string::npos&&end!=std::string::npos&&first<end,"snapshot records missing");
    return snapshot.substr(0,first+1)+sessionLine+snapshot.substr(end);
}
void RejectSnapshot(SaveGame& live,const std::string& bad,const char* reason) {
    const auto before=live;lastInput=bad;lastSnapshot=live.Serialize();
    Require(!live.Restore(bad),reason);Require(SameGame(live,before)&&live.Serialize()==lastSnapshot,"failed Restore changed full live snapshot");
    ++rejections;
}
void MalformedAndAtomicFile() {
    phase="malformed-new-fields";auto live=Powered();
    live.evasion.Tick(.02f,true,true,false,{1,0,.5f});Require(live.Valid(),"active live snapshot invalid");
    const auto good=live.Serialize();const auto first=good.find('\n'),end=good.find("\nENCOUNTER ");
    auto fields=Tokens(good.substr(first+1,end-first-1));Require(fields.size()==22,"Session7 field count differs");
    const auto mutation=[&](unsigned index,const char* value) {
        auto changed=fields;changed[index]=value;RejectSnapshot(live,ReplaceSession(good,Join(changed)),"malformed new Session7 accepted");
    };
    for(unsigned index:{18u,19u,21u})for(const char* value:{"3","-1","4294967295","nan"})mutation(index,value);
    for(const char* value:{"4","-1","4294967295","nan"})mutation(20,value);
    mutation(20,"1");mutation(20,"2");mutation(20,"3");
    for(unsigned mask:{1u,4u,7u}) {
        auto changed=fields;changed[16]=std::to_string(mask);changed[17]=std::to_string(mask==1?3u:mask==4?1536u:1699u);
        RejectSnapshot(live,ReplaceSession(good,Join(changed)),"new linked state bypassed confirmations");
    }
    auto changed=fields;changed[6]="1";changed[16]="7";changed[17]="1699";
    RejectSnapshot(live,ReplaceSession(good,Join(changed)),"new completion bypassed confirmations");
    changed=fields;changed[11]="0";changed[18]="1";
    RejectSnapshot(live,ReplaceSession(good,Join(changed)),"pre-power new choice accepted");
    for(unsigned count=0;count<fields.size();++count) {
        const std::vector<std::string> prefix(fields.begin(),fields.begin()+count);
        RejectSnapshot(live,ReplaceSession(good,Join(prefix)),"truncated Session7 record accepted");
    }
    changed=fields;changed[1]="8";RejectSnapshot(live,ReplaceSession(good,Join(changed)),"unknown Session8 accepted");
    RejectSnapshot(live,good+" extra","trailing complete snapshot tokens accepted");
    RejectSnapshot(live,ReplaceSession(good,Join(fields)+" extra"),"trailing Session tokens accepted");
    RejectSnapshot(live,ReplaceSession(good,std::string(513,'x')),"Session bounded length bypassed");
    RejectSnapshot(live,std::string(SaveGame::MaximumBytes+1,'x'),"complete snapshot bound bypassed");
    const auto evasion=good.find("\nEVASION ");Require(evasion!=std::string::npos,"mandatory evasion missing");
    RejectSnapshot(live,good.substr(0,evasion),"missing complete Evasion accepted");
    auto inactive=Powered();const auto whole=inactive.Serialize();
    for(std::size_t length=0;length<whole.size();++length)RejectSnapshot(live,whole.substr(0,length),"truncated complete record accepted");
    std::istringstream oversized(std::string(100000,'x'));const auto before=live;
    Require(!live.RestoreStream(oversized)&&SameGame(live,before)&&
        oversized.tellg()==static_cast<std::streamoff>(SaveGame::MaximumBytes+1),"bounded failed stream changed live/read limit");
    Session sessionBefore=live.session;std::istringstream hugeSession(std::string(100000,'x'));
    Require(!live.session.RestoreStream(hugeSession)&&Same(live.session,sessionBefore)&&hugeSession.tellg()==513,
        "Session bounded stream changed live/read limit");
    // 真 WriteSaveFile 的 InvalidSnapshot 不能取代同一個已保存的完整檢查點。
    RoundTrip(live,true);const auto diskBefore=Bytes(checkpoint);auto invalid=live;invalid.session.relayPatchA=3;
    Require(WriteSaveFile(checkpoint,invalid)==SaveFileResult::InvalidSnapshot&&Bytes(checkpoint)==diskBefore,
        "invalid new snapshot overwrote prior actual saved file");
    SaveGame read=live;const auto readBefore=read;
    Require(!ReadSaveFile(checkpoint.parent_path()/"missing-checkpoint.txt",read)&&SameGame(read,readBefore),
        "actual missing ReadSaveFile changed output live");
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("usage: SaveTest ABSOLUTE_FRESH_OUTPUT");
        const std::filesystem::path output(argv[1]);
        if(!output.is_absolute()||!output.has_filename()||output.filename()==L"."||output.filename()==L"..")
            throw std::runtime_error("output must be complete and absolute");
        std::error_code error;
        if(std::filesystem::exists(output,error)||error)throw std::runtime_error("output preexists or cannot be checked; no evidence writes");
        if(!std::filesystem::create_directory(output,error)||error)throw std::runtime_error("fresh output directory creation failed");
        checkpoint=output/"checkpoint.txt";
        ChoiceSnapshots();LinkedSubsetsAndTerminal();CombatEvasionSnapshots();LegacyVersions();ExhaustiveLegacy6();MalformedAndAtomicFile();
        Require(choiceCases==27&&subsetCases==8&&legacyVersions==6&&roundtrips==5004&&diskRoundtrips==91,
            "finite complete-save coverage incomplete");
        std::cout<<"SAVE1420 PASS checks="<<checks<<" choice_cases="<<choiceCases<<" linked_subsets="<<subsetCases
            <<" legacy_versions="<<legacyVersions<<" legacy6_legal_states="<<legacy6Cases<<" old_wrapper_states="<<legacyWrappers
            <<" complete_roundtrips="<<roundtrips<<" actual_disk_roundtrips="<<diskRoundtrips
            <<" rejected_snapshot_cases="<<rejections<<" native_F5_F9_executed=0 human_play_executed=0\n";return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FIRST_DIVERGENCE SAVE1420 phase="<<phase<<" checks="<<checks<<" reason="<<e.what()
            <<"\nlast_input="<<lastInput<<"\nlast_snapshot="<<lastSnapshot<<"\ncheckpoint="<<checkpoint.string()<<'\n';return 1;
    }
}
