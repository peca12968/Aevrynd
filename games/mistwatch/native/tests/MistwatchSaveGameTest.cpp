#include "MistwatchSaveGame.h"
#include <bit>
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
        // 新局部進度也逐步真完整存檔往返；原敵人／閃避／malformed回歸全部保留。
        if(index==0){
            Require(!original.session.ConnectRelay(index),"unconfirmed west circuit connected");
            Require(original.session.CycleRelayPatch(0),"snapshot west patch selection failed");worldRoundTrip();
            Require(original.session.ConfirmRelayCircuit(),"snapshot west patch confirmation failed");worldRoundTrip();
        }
        if(index==2){
            Require(!original.session.ConnectRelay(index),"unconfirmed northwest route connected");
            Require(original.session.CycleExtractionRoute(),"snapshot route selection failed");worldRoundTrip();
            Require(original.session.ConfirmExtractionRoute(),"snapshot route confirmation failed");worldRoundTrip();
        }
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
    auto unknown=good;unknown[15]='4';RejectUnchanged(game,unknown,"unknown snapshot version accepted");
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
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SameMotion(const SavedPlayerMotion& a,const SavedPlayerMotion& b){
    const auto vector=[](Aevrynd::Vec3 x,Aevrynd::Vec3 y){return SameFloat(x.x,y.x)&&SameFloat(x.y,y.y)&&SameFloat(x.z,y.z);};
    const auto& c=a.controller;const auto& d=b.controller;const auto& m=a.locomotion.motor;const auto& n=b.locomotion.motor;
    return vector(c.feet,d.feet)&&vector(c.velocity,d.velocity)&&SameFloat(c.height,d.height)&&SameFloat(c.yaw,d.yaw)&&c.grounded==d.grounded&&
        vector(m.velocity,n.velocity)&&SameFloat(m.heading,n.heading)&&m.headingReady==n.headingReady&&m.phase==n.phase&&
        m.supportTicks==n.supportTicks&&m.tick==n.tick&&m.runFlight==n.runFlight&&m.wasDashing==n.wasDashing&&
        std::bit_cast<std::uint64_t>(a.locomotion.remainder)==std::bit_cast<std::uint64_t>(b.locomotion.remainder)&&
        a.locomotion.dodgeFlight==b.locomotion.dodgeFlight&&SameFloat(a.explorerYaw,b.explorerYaw)&&
        SameFloat(a.explorerRequestedYaw,b.explorerRequestedYaw)&&SameFloat(a.explorerPhase,b.explorerPhase)&&
        SameFloat(a.explorerDistance,b.explorerDistance)&&SameFloat(a.explorerSeconds,b.explorerSeconds);
}
SaveGame MotionFixture(unsigned mode){
    auto saved=HpOne();SavedPlayerMotion motion;
    auto& c=motion.controller;auto& l=motion.locomotion;auto& m=l.motor;
    c.feet=saved.session.feet;c.height=mode==1?1.4f:1.8f;c.yaw=-.371234567f;c.grounded=mode<3;
    c.velocity={mode==0?0.f:mode==2?6.f:2.34567881f,mode<3?0.f:mode==3?3.12500024f:-2.98765421f,0};
    m.velocity={c.velocity.x,0,c.velocity.z};m.heading=mode==0?0.f:1.23456788f;m.headingReady=mode!=0;
    m.phase=mode==0?LocomotionPhase::Standing:mode==2?LocomotionPhase::RunSupport:
        mode<3?LocomotionPhase::GroundMotion:LocomotionPhase::JumpOrFall;
    m.supportTicks=mode==2?7:0;m.tick=9007199254740993ull;
    l.remainder=std::nextafter(double(LocomotionMotor::FixedStep),0.);
    motion.explorerYaw=-1.12345672f;motion.explorerRequestedYaw=2.98765421f;motion.explorerPhase=5.12345648f;
    motion.explorerDistance=mode==0?0.f:.0834567845f;motion.explorerSeconds=mode==0?0.f:LocomotionMotor::FixedStep;
    if(mode==5){saved.evasion.Tick(.02f,true,true,false,{1,0,0});l.dodgeFlight=true;m.wasDashing=true;}
    if(mode==6){m.phase=LocomotionPhase::RunFlight;m.runFlight=true;}
    saved.motion=motion;Require(saved.Valid(),"synthetic motion fixture invalid");return saved;
}
void MotionRoundTrip(){
    for(unsigned mode=0;mode<7;++mode){
        const auto original=MotionFixture(mode);const auto text=original.Serialize();SaveGame restored;
        Require(text.starts_with("MISTWATCH_SAVE 3\n")&&text.ends_with("MOTION_END"),"motion did not use complete v3 envelope");
        Require(restored.Restore(text)&&restored.motion.has_value()&&SameMotion(*original.motion,*restored.motion),"motion did not roundtrip bit exactly");
        Require(restored.Serialize()==text&&SameEnemy(restored.encounter,original.encounter)&&
            SameEvasion(restored.evasion,original.evasion)&&restored.session.Serialize()==original.session.Serialize(),"v3 changed surrounding game state");
        std::istringstream stream(text);SaveGame bounded;
        Require(bounded.RestoreStream(stream)&&bounded.motion&&SameMotion(*original.motion,*bounded.motion),"v3 bounded read failed");
        auto old=original;old.motion.reset();const auto legacy=old.Serialize();
        Require(legacy.starts_with("MISTWATCH_SAVE 2\n")&&restored.Restore(legacy)&&!restored.motion&&restored.Serialize()==legacy,
            "v2 migration invented or retained prior v3 motion");
        Require(restored.Restore(text)&&restored.Restore(old.session.Serialize())&&!restored.motion,"session-only restore retained prior v3 motion");
    }
    std::cout<<"MOTION_ROUNDTRIP versions=2,3 modes=7 float_double_bit_exact=1 large_integer_exact=1 world_and_native_validation=0\n";
}
void MalformedMotion(){
    auto saved=MotionFixture(5);const auto good=saved.Serialize();
    for(std::size_t size=0;size<good.size();++size)RejectUnchanged(saved,good.substr(0,size),"truncated v3 motion accepted");
    RejectUnchanged(saved,good+"\nMOTION_END","duplicate motion terminator accepted");
    RejectUnchanged(saved,good+" trailing","v3 trailing token accepted");
    const auto mutate=[&](const char* name,unsigned index,const char* value){
        const std::string marker=std::string("\n")+name+' ';const auto start=good.find(marker);
        Require(start!=std::string::npos,"motion mutation record absent");
        const auto end=good.find('\n',start+1);Require(end!=std::string::npos,"motion mutation end absent");
        std::istringstream in(good.substr(start+marker.size(),end-start-marker.size()));
        std::vector<std::string> fields;std::string token;while(in>>token)fields.push_back(token);
        Require(index<fields.size(),"motion mutation field absent");fields[index]=value;
        std::string record=std::string("\n")+name;for(const auto& field:fields)record+=' '+field;
        RejectUnchanged(saved,good.substr(0,start)+record+good.substr(end),"malformed motion field accepted");
    };
    mutate("MOTION_CONTROLLER",0,"10001");mutate("MOTION_CONTROLLER",0,"42");
    mutate("MOTION_CONTROLLER",3,"nan");mutate("MOTION_CONTROLLER",4,"inf");mutate("MOTION_CONTROLLER",5,"1e50");
    mutate("MOTION_CONTROLLER",6,"0.59");mutate("MOTION_CONTROLLER",6,"3.1");mutate("MOTION_CONTROLLER",7,"4");
    mutate("MOTION_CONTROLLER",8,"2");mutate("MOTION_CONTROLLER",8,"-1");mutate("MOTION_CONTROLLER",8,"1");
    mutate("MOTION_MOTOR",0,"0");mutate("MOTION_MOTOR",1,"1");mutate("MOTION_MOTOR",3,"inf");
    mutate("MOTION_MOTOR",4,"2");mutate("MOTION_MOTOR",4,"0");mutate("MOTION_MOTOR",5,"9");
    mutate("MOTION_MOTOR",5,"0");mutate("MOTION_MOTOR",5,"-1");mutate("MOTION_MOTOR",6,"11");
    mutate("MOTION_MOTOR",7,"-2");mutate("MOTION_MOTOR",7,"18446744073709551615");
    mutate("MOTION_MOTOR",7,"18446744073709551616");mutate("MOTION_MOTOR",7,"12x");mutate("MOTION_MOTOR",7,"1.0");
    mutate("MOTION_MOTOR",8,"2");mutate("MOTION_MOTOR",8,"1");mutate("MOTION_MOTOR",9,"2");
    mutate("MOTION_OWNER",0,"nan");mutate("MOTION_OWNER",0,"-0.1");mutate("MOTION_OWNER",0,"0.1");
    mutate("MOTION_OWNER",1,"0");mutate("MOTION_OWNER",1,"2");
    mutate("MOTION_EXPLORER",0,"nan");mutate("MOTION_EXPLORER",1,"4");
    mutate("MOTION_EXPLORER",2,"-0.1");mutate("MOTION_EXPLORER",2,"6.2831853");
    mutate("MOTION_EXPLORER",3,"nan");mutate("MOTION_EXPLORER",3,"-0.01");
    mutate("MOTION_EXPLORER",4,"inf");mutate("MOTION_EXPLORER",4,"-0.01");mutate("MOTION_EXPLORER",4,"0.101");
    {
        // 未發佈的三欄候選格式不可靜默遺失重建姿態所需的最後一步距離與時間。
        const auto start=good.find("\nMOTION_EXPLORER ");const auto end=good.find("\nMOTION_END",start);
        std::istringstream fields(good.substr(start,end-start));std::string name,yaw,requested,phase;
        Require(bool(fields>>name>>yaw>>requested>>phase),"motion pose record fixture missing");
        RejectUnchanged(saved,good.substr(0,start)+"\n"+name+" "+yaw+" "+requested+" "+phase+good.substr(end),
            "missing pose-driving distance and seconds silently defaulted");
    }
    for(const char* name:{"MOTION_CONTROLLER","MOTION_MOTOR","MOTION_OWNER","MOTION_EXPLORER"}){
        const auto start=good.find(std::string("\n")+name);const auto end=good.find('\n',start+1);
        RejectUnchanged(saved,good.substr(0,start)+good.substr(end),"missing v3 record accepted");
        RejectUnchanged(saved,good.substr(0,end)+good.substr(start,end-start)+good.substr(end),"duplicate v3 record accepted");
    }
    auto wrongVersion=good;wrongVersion[15]='2';RejectUnchanged(saved,wrongVersion,"motion smuggled into v2 accepted");
    auto invalid=saved;invalid.motion->locomotion.remainder=std::numeric_limits<double>::quiet_NaN();
    Require(invalid.Serialize().empty(),"non-finite motion serialized");
    invalid=saved;invalid.motion->controller.feet.x+=1;Require(invalid.Serialize().empty(),"session/controller mismatch serialized");
    std::cout<<"MOTION_MALFORMED missing_duplicate_trailing_nonfinite_integer_overflow_cross_state=1 transactional_rejection=1\n";
}
}
int main(){
    try{Baseline();RoundTripAndFuture();Legacy();EvasionRoundTrip();MalformedEvasion();Malformed();MotionRoundTrip();MalformedMotion();std::cout<<"SaveGame CPU contract PASS; checks="<<checks<<" native_F5_F9_tested=0\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<'\n';return 1;}
}
