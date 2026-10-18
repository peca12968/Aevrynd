#include "MistwatchEncounterFeedback.h"
#include "MistwatchEncounterMotion.h"
#include "MistwatchActionFeedback.h"
#include "MistwatchFieldJournal.h"
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace Mistwatch;
unsigned checks{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
void Original(Session& s,const Encounter& e,std::string& m){
 if(!e.health)s.FinishEncounter(e.feet);
 if(e.phase==EnemyPhase::Windup)m="Leave the marked circle - Q: evade";
 else if(e.phase==EnemyPhase::Recovery)m="Boar recovering - counter with F";
}
int main(){try{
 Session s;s.collected=7;s.repairMaterials=7;s.bridgeRepaired=true;s.feet=Session::arena;
 Session old=s;Encounter e;std::string message="Objective updated",oldMessage=message;
 Aevrynd::HeightField3D terrain({{-10,0,35},41,61,.5f});terrain.Fill(0);
 const auto tick=[&](Aevrynd::Vec3 player,bool attack){
  s.feet=player;old.feet=player;
  const auto r=TickEncounterWorld(e,player,.05f,attack,{},terrain);
  Require(r.status==EncounterMotionStatus::Applied,"world tick rejected");
  s.Damage(r.damage);old.Damage(r.damage);
  Original(old,e,oldMessage);UpdateEncounterFeedback(s,e,message);
  Require(s.Serialize()==old.Serialize(),"feedback changed session");
  Require(e.Valid(),"enemy invalid");
 };
 for(unsigned round=0;round<3;++round){
  const auto strike=e.feet+Aevrynd::Vec3{1,0,0};tick(strike,false);
  Require(e.phase==EnemyPhase::Windup,"windup required");
  Require(message==oldMessage,"windup changed");
  const auto evade=e.feet+Aevrynd::Vec3{-1,0,0};
  for(unsigned i=0;i<30&&e.phase==EnemyPhase::Windup;++i)tick(evade,false);
  Require(e.phase==EnemyPhase::Recovery&&s.health==100,"evade failed");
  Require(message=="Boar recovering - counter with F","active recovery missing");
  tick(evade,true);Require(e.health==2-round,"counter not applied");
  if(round<2){
   Require(message=="Counter landed - prepare for the next attack"&&!s.guardianDefeated,
       "Counter already landed but stale F prompt remained");
   for(unsigned i=0;i<30&&e.phase==EnemyPhase::Recovery;++i)tick(evade,false);
   Require(e.phase==EnemyPhase::Pursuit,"recovery did not finish");
   Require(message=="Boar approaching - watch for the marked circle","Pursuit kept a stale recovery prompt");
  }
 }
 Require(s.guardianDefeated&&s.CurrentStage()==Stage::Fuse&&e.phase==EnemyPhase::Defeated,"terminal not reached");
 Require(oldMessage=="Boar recovering - counter with F","original stale message not reproduced");
 Require(message=="Boar defeated - find the ranger log","completion message missing");
 std::cout<<"ORIGINAL_STALE="<<oldMessage<<" FIXED="<<message<<" hp="<<s.health<<'\n';
 const auto saved=s.Serialize();Session loaded;Require(loaded.Restore(saved)&&loaded.Serialize()==saved,"snapshot changed");
 message="Loaded";UpdateEncounterFeedback(loaded,e,message);Require(message=="Loaded","load message overwritten");
 loaded.Retry();Require(loaded.guardianDefeated&&loaded.health==100,"retry progress changed");
 loaded={};message="New expedition";Encounter fresh;UpdateEncounterFeedback(loaded,fresh,message);Require(message=="New expedition","restart message overwritten");
 Session denied=s;denied.guardianDefeated=false;denied.health=0;message="Other action";UpdateEncounterFeedback(denied,e,message);Require(!denied.guardianDefeated&&message=="Other action","dead completion accepted");
 denied.health=100;denied.feet={90,0,90};UpdateEncounterFeedback(denied,e,message);Require(!denied.guardianDefeated&&message=="Other action","far completion accepted");
 ActionFeedbackToast toast;Require(toast.Publish(ActionFeedbackEvent::Saved),"toast publish");UpdateEncounterFeedback(denied,e,message);Require(toast.Text()=="Last action: Saved","toast changed");
 Session active=s;active.guardianDefeated=false;Encounter recovering;
 recovering.phase=EnemyPhase::Recovery;recovering.phaseTime=.5f;recovering.playerCooldown=.2f;
 UpdateEncounterFeedback(active,recovering,message);
 Require(message=="Counter cooling down - keep clear","cooldown gave invalid F instruction");
 recovering.playerCooldown=0;UpdateEncounterFeedback(active,recovering,message);
 Require(message=="Boar recovering - counter with F","ready counter prompt missing");
 recovering.health=2;recovering.countered=true;UpdateEncounterFeedback(active,recovering,message);
 Require(message=="Counter landed - prepare for the next attack","landed prompt missing");
 active.health=0;message="Other action";UpdateEncounterFeedback(active,recovering,message);
 Require(message=="Other action","dead player received combat prompt");
 Require(FieldPresentation::LastAction("Counter landed - prepare for the next attack")=="反擊命中；準備閃避下一次攻擊","counter translation");
 Require(FieldPresentation::LastAction("Counter cooling down - keep clear")=="反擊尚未恢復；保持安全距離","cooldown translation");
 Require(FieldPresentation::LastAction("Boar approaching - watch for the marked circle")=="野豬正在接近；留意地面預警圈","pursuit translation");
 // 新增獨立計數的同幀提示政策；不改原 Require、checks 或既有通過標記。
 {
  const unsigned originalChecks=checks;unsigned policyChecks{},policyCases{};
  const auto policy=[&](bool ok,const std::string& why){++policyChecks;if(!ok)throw std::runtime_error(why);};
  const auto sameFloat=[](float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);};
  const auto sameVec=[&](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return sameFloat(a.x,b.x)&&sameFloat(a.y,b.y)&&sameFloat(a.z,b.z);};
  const auto sameSession=[&](const Session& a,const Session& b){return
   a.collected==b.collected&&a.health==b.health&&a.bridgeRepaired==b.bridgeRepaired&&a.guardianDefeated==b.guardianDefeated&&
   a.beaconLit==b.beaconLit&&a.fuseCollected==b.fuseCollected&&a.powerRestored==b.powerRestored&&a.circuit==b.circuit&&
   a.repairMaterials==b.repairMaterials&&a.fuseClueRead==b.fuseClueRead&&a.fuseDials==b.fuseDials&&
   a.relayLinked==b.relayLinked&&a.relaySectors==b.relaySectors&&a.relayPatchA==b.relayPatchA&&a.relayPatchB==b.relayPatchB&&
   a.relayFieldTasks==b.relayFieldTasks&&a.extractionRoute==b.extractionRoute&&sameVec(a.feet,b.feet);};
  const auto sameEncounter=[&](const Encounter& a,const Encounter& b){return sameVec(a.feet,b.feet)&&sameVec(a.strikeTarget,b.strikeTarget)&&
   a.health==b.health&&a.phase==b.phase&&sameFloat(a.phaseTime,b.phaseTime)&&sameFloat(a.playerCooldown,b.playerCooldown)&&a.countered==b.countered;};
  const auto run=[&](const char* id,Session state,Encounter enemy,bool omitted,bool standingDenied,
                     const char* incoming,const char* expected,const Session& expectedState){
   ++policyCases;const Encounter before=enemy;std::string actual=incoming;
   if(omitted)UpdateEncounterFeedback(state,enemy,actual);
   else UpdateEncounterFeedback(state,enemy,actual,standingDenied);
   policy(actual==expected,std::string(id)+": message");
   policy(state.Serialize()==expectedState.Serialize()&&sameSession(state,expectedState),std::string(id)+": session fields/serialization");
   policy(sameEncounter(enemy,before),std::string(id)+": enemy fields");
  };
  Session base;base.collected=7;base.repairMaterials=7;base.bridgeRepaired=true;base.feet=Session::arena;
  Encounter pursuit;constexpr const char* low="Low ceiling - move clear before standing";
  constexpr const char* approaching="Boar approaching - watch for the marked circle";
  policy(base.Valid()&&base.Alive()&&base.CurrentStage()==Stage::Encounter&&pursuit.Valid(),"policy active Pursuit fixture");
  const char* preserved[]{low,"Objective updated","Move character clear before bridge repair","Move camera clear before bridge repair"};
  for(const auto* text:preserved)run("Pursuit standingDenied preserves prior frame choice",base,pursuit,false,true,text,text,base);
  const char* stale[]{low,"Objective updated","Move character clear before bridge repair","Move camera clear before bridge repair",
                     "Counter cooling down - keep clear"};
  for(const auto* text:stale){
   run("Pursuit explicit false keeps original approaching policy",base,pursuit,false,false,text,approaching,base);
   run("Pursuit omitted argument equals false",base,pursuit,true,false,text,approaching,base);
  }
  const auto phasePair=[&](const char* id,Encounter enemy,const char* expected,const Session& expectedState){
   policy(enemy.Valid()&&base.Valid()&&expectedState.Valid(),std::string(id)+": valid phase fixture");
   run(id,base,enemy,true,false,low,expected,expectedState);
   run(id,base,enemy,false,true,low,expected,expectedState);
  };
  Encounter windup;windup.phase=EnemyPhase::Windup;windup.phaseTime=.5f;
  phasePair("Windup stays urgent",windup,"Leave the marked circle - Q: evade",base);
  Encounter ready;ready.phase=EnemyPhase::Recovery;ready.phaseTime=.5f;
  phasePair("Recovery ready",ready,"Boar recovering - counter with F",base);
  Encounter cooling=ready;cooling.playerCooldown=.2f;
  phasePair("Recovery cooldown",cooling,"Counter cooling down - keep clear",base);
  Encounter landed=cooling;landed.health=2;landed.countered=true;
  phasePair("Recovery landed wins over cooldown",landed,"Counter landed - prepare for the next attack",base);
  Encounter defeated;defeated.health=0;defeated.countered=true;defeated.phase=EnemyPhase::Defeated;defeated.phaseTime=.5f;
  Session completed=base;completed.guardianDefeated=true;
  phasePair("Defeated still finishes Session",defeated,"Boar defeated - find the ranger log",completed);
  const auto guardedPair=[&](const char* id,Session state,Encounter enemy,const char* incoming){
   run(id,state,enemy,true,false,incoming,incoming,state);
   run(id,state,enemy,false,true,incoming,incoming,state);
  };
  Session inactive;policy(inactive.Valid()&&inactive.CurrentStage()!=Stage::Encounter,"inactive stage fixture");
  guardedPair("inactive stage",inactive,pursuit,"New expedition");
  Session dead=base;dead.health=0;policy(dead.Valid()&&!dead.Alive(),"dead active fixture");
  guardedPair("dead active player",dead,pursuit,"Other action");
  Encounter invalid=pursuit;invalid.health=4;policy(!invalid.Valid()&&invalid.health>0,"invalid LIVE encounter fixture");
  guardedPair("invalid live encounter",base,invalid,low);
  Session invalidSession=base;invalidSession.collected=8;policy(!invalidSession.Valid(),"invalid Session fixture");
  guardedPair("invalid Session no completion branch",invalidSession,pursuit,"Other action");
  Session loadedCompleted;policy(loadedCompleted.Restore(completed.Serialize())&&loadedCompleted.Serialize()==completed.Serialize()&&
   loadedCompleted.CurrentStage()==Stage::Fuse,"actual loaded completed Session fixture");
  guardedPair("loaded completed Session",loadedCompleted,defeated,"Loaded");
  Session farState=base;farState.feet={90,0,90};policy(farState.Valid()&&!farState.Near(defeated.feet),"far completion fixture");
  guardedPair("far completion rejected",farState,defeated,"Other action");
  policy(dead.Valid()&&!dead.Alive()&&defeated.Valid(),"dead completion fixture");
  guardedPair("dead completion rejected",dead,defeated,"Other action");
  policy(checks==originalChecks&&policyCases==38&&policyChecks==127,"policy accounting or original checks changed");
  std::cout<<"STANDING_DENIED_POLICY_PASS checks="<<policyChecks<<" cases="<<policyCases<<" original_counter_unchanged=1\n";
 }
 std::cout<<"FEEDBACK_PASS checks="<<checks<<" three_counters=3\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
