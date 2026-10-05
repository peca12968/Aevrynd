#include "MistwatchSaveGame.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace Mistwatch;
unsigned checks{},futureTicks{},roundtrips{};
void Require(bool condition,const char* name){++checks;if(!condition)throw std::runtime_error(name);}
bool SameEnemy(const Encounter& a,const Encounter& b){
    return a.health==b.health&&a.phase==b.phase&&a.phaseTime==b.phaseTime&&a.playerCooldown==b.playerCooldown&&
        a.countered==b.countered&&a.feet.x==b.feet.x&&a.feet.y==b.feet.y&&a.feet.z==b.feet.z&&
        a.strikeTarget.x==b.strikeTarget.x&&a.strikeTarget.y==b.strikeTarget.y&&a.strikeTarget.z==b.strikeTarget.z;
}
bool SameVector(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool SameEvasion(const Evasion& a,const Evasion& b){
    return SameVector(a.direction,b.direction)&&a.remaining==b.remaining&&a.cooldown==b.cooldown;
}
Session BridgeSession(){
    Session session;
    // 此處是規則單元測試的合成位置，不是輸入錄影或碰撞通關證據。
    for(unsigned i=0;i<3;++i){session.feet=Session::supplies[i];Require(session.Collect(i),"fixture supply collection failed");}
    for(unsigned i=0;i<3;++i){session.feet=Session::repairSites[i];Require(session.CollectRepairMaterial(i,i==0),"fixture repair material collection failed");}
    session.feet=Session::bridge;Require(session.RepairBridge(),"fixture bridge repair failed");
    session.feet=Session::arena;return session;
}
void Tick(SaveGame& game,Aevrynd::Vec3 player,float dt,bool attack){
    game.session.feet=player;
    if(game.session.CurrentStage()==Stage::Encounter&&game.session.Alive()){
        game.session.Damage(game.encounter.Tick(player,dt,attack));
        if(!game.encounter.health)Require(game.session.FinishEncounter(game.encounter.feet),"defeat did not advance mission");
    }
}
SaveGame HpOne(){
    SaveGame game;game.session=BridgeSession();
    for(unsigned hit=0;hit<2;++hit){
        unsigned ticks{};
        while(game.encounter.phase!=EnemyPhase::Recovery&&ticks++<300)Tick(game,Session::arena,.02f,false);
        Require(game.encounter.phase==EnemyPhase::Recovery,"HP1 fixture never reached counter window");
        const auto prior=game.encounter.health;Tick(game,Session::arena,.02f,true);
        Require(game.encounter.health+1==prior,"HP1 fixture counter failed");
        if(hit==0){ticks=0;while(game.encounter.phase==EnemyPhase::Recovery&&ticks++<300)Tick(game,Session::arena,.02f,false);}
    }
    Require(game.encounter.health==1&&game.Valid(),"HP1 fixture is not a valid live snapshot");
    return game;
}
std::string Raw(const Session& session,const std::vector<std::string>& fields,
    const std::vector<std::string>& evasionFields={"0","0","0","0","0"},unsigned version=2){
    std::string text="MISTWATCH_SAVE "+std::to_string(version)+"\n"+session.Serialize()+"\nENCOUNTER";
    for(const auto& field:fields)text+=' '+field;
    text+="\nEVASION";
    for(const auto& field:evasionFields)text+=' '+field;
    return text;
}
std::vector<std::string> Fields(const SaveGame& game){
    const auto text=game.Serialize();const auto marker=text.find("\nENCOUNTER ");
    Require(marker!=std::string::npos,"fixture encounter fields missing");
    const auto end=text.find("\nEVASION ",marker);
    Require(end!=std::string::npos,"fixture mandatory evasion record missing");
    std::istringstream in{text.substr(marker+11,end-marker-11)};std::vector<std::string> fields;std::string field;
    while(in>>field)fields.push_back(field);
    Require(fields.size()==11,"fixture encounter field count changed");return fields;
}
void RejectUnchanged(SaveGame& game,const std::string& text,const char* name){
    const auto before=game.Serialize();const auto enemy=game.encounter;const auto action=game.evasion;
    Require(!game.Restore(text),name);
    Require(game.Serialize()==before&&SameEnemy(game.encounter,enemy)&&SameEvasion(game.evasion,action),"rejected save partially mutated snapshot");
}
void Baseline(){
    auto saved=HpOne();Session loaded;Encounter reset;
    saved.evasion.Tick(.02f,true,true,false,{1,0,0});
    // 精確模擬已重現的舊版缺口：F5=session.Serialize；F9=session.Restore 後 encounter={}。
    Require(loaded.Restore(saved.session.Serialize()),"baseline legacy Session routing failed");
    reset={};
    Require(saved.encounter.health==1&&reset.health==3&&loaded.Serialize()==saved.session.Serialize(),"baseline HP-reset deficiency not reproduced");
    std::cout<<"BASELINE_FIRST_DIVERGENCE: simulated_current_F5_F9 enemy_hp=1->3; native_input_executed=0\n";
    Evasion resetAction;
    Require(saved.evasion.remaining>0&&saved.evasion.cooldown>0&&resetAction.remaining==0&&resetAction.cooldown==0,"baseline active dash reset not reproduced");
    auto continued=saved.evasion;
    const auto originalMotion=continued.Tick(.02f,true,true,false,{-1,0,0});
    const auto resetMotion=resetAction.Tick(.02f,true,true,false,{-1,0,0});
    Require(originalMotion.dashing&&resetMotion.dashing&&originalMotion.movement.x>0&&resetMotion.movement.x<0,"baseline reload direction/cooldown bypass not reproduced");
    std::cout<<"BASELINE_FIRST_DIVERGENCE: simulated_current_F9 active_dash_and_cooldown_erased; original_direction=+X reload_direction=-X; native_input_executed=0\n";
}
void RoundTripAndFuture(){
    auto original=HpOne();SaveGame loaded;
    const auto text=original.Serialize();Require(!text.empty()&&loaded.Restore(text),"HP1 snapshot restore failed");
    Require(loaded.session.Serialize()==original.session.Serialize()&&SameEnemy(loaded.encounter,original.encounter),"HP1 snapshot field roundtrip differed");
    for(unsigned tick=0;tick<600;++tick){
        // 相同玩家位置與按鍵序列：窗口外不攻擊，窗口內只嘗試一次有效反擊。
        const bool attack=original.encounter.phase==EnemyPhase::Recovery&&!original.encounter.countered&&original.encounter.playerCooldown==0;
        Tick(original,Session::arena,.02f,attack);Tick(loaded,Session::arena,.02f,attack);
        Require(original.Valid()&&loaded.Valid(),"Tick generated snapshot rejected by invariants");
        Require(SameEnemy(original.encounter,loaded.encounter)&&original.session.Serialize()==loaded.session.Serialize(),"post-restore deterministic future diverged");
        SaveGame repeated;Require(repeated.Restore(original.Serialize())&&SameEnemy(repeated.encounter,original.encounter),"reachable phase roundtrip failed");
        ++roundtrips;++futureTicks;
    }
    Require(original.session.guardianDefeated&&original.encounter.health==0,"restored HP1 battle did not reach defeat");
    const auto worldRoundTrip=[&](){
        SaveGame copy;const auto textNow=original.Serialize();
        Require(!textNow.empty()&&copy.Restore(textNow)&&copy.session.Serialize()==original.session.Serialize()&&
            SameEnemy(copy.encounter,original.encounter),"mission progression fields lost in complete snapshot");
        ++roundtrips;
    };
    original.session.feet=Session::rangerLog;Require(original.session.ReadFuseClue(),"snapshot clue fixture failed");worldRoundTrip();
    original.session.feet=Session::fuse;
    for(unsigned wheel=0;wheel<3;++wheel)for(unsigned turn=0;turn<Session::fuseCode[wheel];++turn){
        Require(original.session.CycleFuseDial(wheel),"snapshot dial fixture failed");worldRoundTrip();
    }
    Require(original.session.CollectFuse(),"snapshot fuse fixture failed");worldRoundTrip();
    original.session.feet=Session::generator;
    Require(original.session.ToggleCircuit(0),"snapshot partial circuit fixture failed");worldRoundTrip();
    Require(original.session.ToggleCircuit(2)&&original.session.RepairGenerator(),"snapshot power fixture failed");worldRoundTrip();
    for(unsigned index=0;index<3;++index){
        original.session.feet=Session::relaySites[index];
        for(unsigned turn=0;turn<Session::RelayTargetSector(index);++turn)Require(original.session.TuneRelay(index,1),"snapshot relay fixture failed");
        Require(original.session.ConnectRelay(index),"snapshot relay link fixture failed");worldRoundTrip();
    }
    original.session.feet=Session::beacon;
    Require(original.session.LightBeacon(),"snapshot completion fixture failed");worldRoundTrip();
    original.session.Damage(100);worldRoundTrip();
    // 額外完整涵蓋 Pursuit、Windup、Recovery 與負的到期餘數，不只取 Defeated 尾端。
    SaveGame phases;phases.session=BridgeSession();unsigned seen{};bool negativeRemainder{};
    for(unsigned tick=0;tick<240;++tick){
        const auto player=phases.encounter.phase==EnemyPhase::Windup?Session::arena+Aevrynd::Vec3{4,0,0}:Session::arena;
        Tick(phases,player,.02f,false);
        Require(phases.Valid(),"reachable non-defeat phase failed snapshot validation");
        SaveGame copy;Require(copy.Restore(phases.Serialize())&&SameEnemy(copy.encounter,phases.encounter),"phase or timer did not roundtrip exactly");
        seen|=1u<<static_cast<unsigned>(phases.encounter.phase);negativeRemainder|=phases.encounter.phase==EnemyPhase::Pursuit&&phases.encounter.phaseTime<0;
        ++roundtrips;
    }
    Require((seen&7)==7&&negativeRemainder,"phase coverage missed pursuit/windup/recovery expiry");
    std::cout<<"ROUNDTRIP enemy_hp=1 deterministic_future_ticks="<<futureTicks<<" reachable_state_roundtrips="<<roundtrips<<'\n';
}
void Legacy(){
    const std::array<const char*,6> old{{
        "MISTWATCH 1 7 100 1 0 0 0 0 30",
        "MISTWATCH 2 7 100 1 0 0 0 0 30 0 0",
        "MISTWATCH 3 7 100 1 0 0 0 0 30 0 0 0",
        "MISTWATCH 4 7 100 1 0 0 0 0 30 0 0 0 7",
        "MISTWATCH 5 7 100 1 0 0 0 0 30 0 0 0 7 0 0",
        "MISTWATCH 6 7 100 1 0 0 0 0 30 0 0 0 7 0 0 0 0"}};
    for(const auto text:old){SaveGame game;Require(game.Restore(text)&&game.Valid()&&game.session.CurrentStage()==Stage::Encounter&&game.encounter.health==3&&
        SameEvasion(game.evasion,Evasion{}),"legacy session-only migration failed");}
    SaveGame completed;Require(completed.Restore("MISTWATCH 1 7 100 1 1 1 0 0 70")&&completed.session.CurrentStage()==Stage::Complete&&
        completed.encounter.phase==EnemyPhase::Defeated&&completed.encounter.health==0,"legacy completion restarted guardian");
    SaveGame initial;Require(initial.Restore(Session{}.Serialize())&&initial.Valid(),"legacy initial session failed");
    const auto battle=HpOne();SaveGame lostEnemy;
    Require(lostEnemy.Restore(battle.session.Serialize())&&lostEnemy.encounter.health==3,"legacy missing enemy state did not use documented default");
    std::cout<<"LEGACY versions=1..6; missing_enemy_state=default_pursuit; completed_enemy=defeated\n";
}
void EvasionRoundTrip(){
    for(unsigned mode=0;mode<3;++mode){
        auto original=HpOne();
        original.evasion.Tick(.02f,true,true,false,{1,0,.5f});
        if(mode>0)for(unsigned i=0;i<(mode==1?15u:100u);++i)original.evasion.Tick(.02f,false,true,false,{});
        Require(original.Valid(),"reachable evasion seed failed validation");
        Require(mode==0?original.evasion.remaining>0:original.evasion.remaining==0,"evasion seed phase incorrect");
        if(mode==1)Require(original.evasion.cooldown>0,"cooldown-only seed expired prematurely");
        if(mode==2)Require(original.evasion.cooldown==0&&std::hypot(original.evasion.direction.x,original.evasion.direction.z)>.99f,"inactive retained direction seed failed");
        SaveGame restored;
        Require(restored.Restore(original.Serialize())&&SameEvasion(original.evasion,restored.evasion),"active/cooldown evasion did not roundtrip exactly");
        unsigned dashTicks{};
        for(unsigned i=0;i<120;++i){
            const bool requested=i==0||i==1||i==50||i==75;
            const bool grounded=i%30<22,crouched=i%40>=35;
            const Aevrynd::Vec3 input{i%25<12?-1.f:1.f,0,0};
            const auto prior=original.evasion;
            const auto a=original.evasion.Tick(.02f,requested,grounded,crouched,input);
            const auto b=restored.evasion.Tick(.02f,requested,grounded,crouched,input);
            Require(a.dashing==b.dashing&&SameVector(a.movement,b.movement)&&SameEvasion(original.evasion,restored.evasion),"restored evasion future input behavior diverged");
            if(i==0&&mode==0)Require(a.dashing&&SameVector(original.evasion.direction,prior.direction)&&
                original.evasion.remaining==prior.remaining-.02f&&original.evasion.cooldown==prior.cooldown-.02f,"reload restarted dash or erased cooldown/locked direction");
            if(i==0&&mode==1)Require(!a.dashing&&original.evasion.cooldown==prior.cooldown-.02f,"reload bypassed cooldown-only state");
            dashTicks+=unsigned(a.dashing);
            // 僅模擬相同規則輸入，不以此宣稱角色碰撞或原生 F5/F9 已驗收。
            const float speed=a.dashing?12.f:6.f;
            const auto player=original.session.feet+a.movement*(speed*.02f);
            const bool attack=original.encounter.phase==EnemyPhase::Recovery&&!original.encounter.countered&&
                original.encounter.playerCooldown==0&&!a.dashing;
            Tick(original,player,.02f,attack);Tick(restored,player,.02f,attack);
            Require(original.Valid()&&restored.Valid()&&original.Serialize()==restored.Serialize(),"combined battle/evasion future snapshot diverged");
            SaveGame reloaded;Require(reloaded.Restore(original.Serialize())&&SameEvasion(reloaded.evasion,original.evasion),"reachable evasion intermediate failed roundtrip");
        }
        Require(dashTicks>0,"evasion future sequence did not exercise a dash");
        std::cout<<"EVASION_ROUNDTRIP mode="<<mode<<" deterministic_future_ticks=120 dash_ticks="<<dashTicks<<'\n';
    }
}
void MalformedEvasion(){
    auto game=HpOne();game.evasion.Tick(.02f,true,true,false,{1,0,.5f});
    const auto enemyFields=Fields(game);const auto good=game.Serialize();
    const auto marker=good.find("\nEVASION ");Require(marker!=std::string::npos,"evasion record missing");
    std::istringstream in(good.substr(marker+9));std::vector<std::string> actionFields;std::string token;
    while(in>>token)actionFields.push_back(token);
    Require(actionFields.size()==5&&Raw(game.session,enemyFields,actionFields)==good,"raw evasion mutation fixture is not canonical");
    const auto mutation=[&](unsigned index,const char* value){auto changed=actionFields;changed[index]=value;RejectUnchanged(game,Raw(game.session,enemyFields,changed),"invalid evasion field accepted");};
    mutation(0,"nan");mutation(1,"0.01");mutation(2,"inf");
    mutation(3,"nan");mutation(3,"-0.01");mutation(3,"0.29");
    mutation(4,"nan");mutation(4,"-0.01");mutation(4,"1.21");mutation(4,"0.3");
    auto changed=actionFields;changed[0]="0";changed[2]="0";
    RejectUnchanged(game,Raw(game.session,enemyFields,changed),"active dash with zero direction accepted");
    changed=actionFields;changed[0]="2";changed[2]="0";
    RejectUnchanged(game,Raw(game.session,enemyFields,changed),"non-unit dash direction accepted");
    changed={"0","0","0","0","0.4"};
    RejectUnchanged(game,Raw(game.session,enemyFields,changed),"cooldown with never-initialized zero direction accepted");
    RejectUnchanged(game,good.substr(0,marker),"new snapshot silently defaulted missing EVASION record");
    changed=actionFields;changed[3]="0";changed[4]="0.4";
    SaveGame inactive;Require(inactive.Restore(Raw(game.session,enemyFields,changed))&&inactive.evasion.remaining==0&&inactive.evasion.cooldown>0,"valid cooldown with retained direction rejected");
    changed[4]="0";Require(inactive.Restore(Raw(game.session,enemyFields,changed))&&inactive.Valid(),"valid inactive retained unit direction rejected");
    changed={"0","0","0","0","0"};Require(inactive.Restore(Raw(game.session,enemyFields,changed))&&inactive.Valid(),"valid default zero direction rejected");
    auto invalid=game;invalid.evasion.remaining=Evasion::Duration+.01f;Require(invalid.Serialize().empty(),"invalid evasion timer serialized");
    invalid=game;invalid.evasion.direction.y=.1f;Require(invalid.Serialize().empty(),"vertical evasion direction serialized");
    std::cout<<"EVASION_MALFORMED mandatory_record=1 timer_direction_rejection=1 unchanged_state=1\n";
}
void Malformed(){
    auto game=HpOne();const auto good=game.Serialize();const auto fields=Fields(game);
    for(std::size_t length=0;length<good.size();++length)RejectUnchanged(game,good.substr(0,length),"truncated save accepted");
    RejectUnchanged(game,std::string(SaveGame::MaximumBytes+1,'x'),"oversized save accepted");
    auto unknown=good;unknown[15]='3';RejectUnchanged(game,unknown,"unknown snapshot version accepted");
    RejectUnchanged(game,good+" trailing", "trailing tokens accepted");
    const auto mutation=[&](unsigned field,const char* value){auto changed=fields;changed[field]=value;RejectUnchanged(game,Raw(game.session,changed),"invalid encounter field accepted");};
    mutation(0,"4");mutation(0,"0");mutation(0,"-1");mutation(1,"99");mutation(1,"-1");
    mutation(2,"nan");mutation(3,"inf");mutation(4,"10001");mutation(5,"-inf");
    // 舊版非零 Y 壞檔仍拒絕；新版有限高度交由實際世界預檢，不能靠常數假裝接地。
    for(const char* height:{"-100","100","0.001","-0.001"}){
        auto changed=fields;changed[3]=height;
        RejectUnchanged(game,Raw(game.session,changed,{"0","0","0","0","0"},1),"legacy nonzero enemy height accepted");
        SaveGame structural;Require(structural.Restore(Raw(game.session,changed)),"version2 finite enemy height structurally rejected");
    }
    SaveGame legacy;Require(legacy.Restore(Raw(game.session,fields,{"0","0","0","0","0"},1))&&
        SameEnemy(legacy.encounter,game.encounter),"version1 centered checkpoint failed migration");
    mutation(8,"nan");mutation(8,"2");mutation(8,"-0.2");mutation(9,"nan");mutation(9,"0.66");mutation(9,"-0.01");mutation(10,"2");
    auto contradiction=fields;contradiction[1]="1";contradiction[8]="0.3";
    RejectUnchanged(game,Raw(game.session,contradiction),"windup with consumed counter accepted");
    contradiction=fields;contradiction[0]="3";
    RejectUnchanged(game,Raw(game.session,contradiction),"full-HP enemy with consumed counter accepted");
    Session initial;RejectUnchanged(game,Raw(initial,fields),"pre-bridge session with progressed battle accepted");
    auto invalid=game;invalid.session.guardianDefeated=true;Require(invalid.Serialize().empty(),"inconsistent mission/guardian serialized");
    invalid=game;invalid.encounter.health=4;Require(invalid.Serialize().empty(),"invalid HP serialized");
    invalid=game;invalid.encounter.phase=static_cast<EnemyPhase>(99);Require(invalid.Serialize().empty(),"invalid enum serialized");
    invalid=game;invalid.encounter.feet.x=std::numeric_limits<float>::quiet_NaN();Require(invalid.Serialize().empty(),"NaN serialized");
    auto badSession=game.session;badSession.health=101;
    Require(!SaveGame{badSession,game.encounter,game.evasion}.Valid(),"Session health limit bypassed");
    std::istringstream large(std::string(100000,'x'));const auto before=game.Serialize();
    Require(!game.RestoreStream(large)&&game.Serialize()==before&&large.tellg()==static_cast<std::streamoff>(SaveGame::MaximumBytes+1),"stream loader did not stop after bounded read");
    std::istringstream stream(good);SaveGame fromStream;
    Require(fromStream.RestoreStream(stream)&&fromStream.Serialize()==good,"valid bounded stream restore failed");
    std::cout<<"MALFORMED truncation_prefixes="<<good.size()<<" max_read_bytes="<<SaveGame::MaximumBytes+1<<" transactional_rejection=1\n";
}
}
int main(){
    try{Baseline();RoundTripAndFuture();Legacy();EvasionRoundTrip();MalformedEvasion();Malformed();std::cout<<"SaveGame CPU contract PASS; checks="<<checks<<" native_F5_F9_tested=0\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<'\n';return 1;}
}
