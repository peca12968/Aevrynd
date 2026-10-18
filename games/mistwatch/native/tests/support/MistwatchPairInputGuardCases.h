#pragma once
namespace GuardInput {
using namespace Aevrynd;using namespace Mistwatch;namespace P=PairContact;
unsigned checks{},negativeCases{},positiveCases{},profiles{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
Encounter InvalidEnemy(unsigned kind){
    Encounter enemy;enemy.feet={};
    switch(kind){
    case 0:enemy.health=0;break;
    case 1:enemy.health=4;break;
    case 2:enemy.phase=static_cast<EnemyPhase>(99);break;
    case 3:enemy.feet.x=std::bit_cast<float>(0x7fc01234u);break;
    case 4:enemy.strikeTarget.z=std::bit_cast<float>(0x7fc01234u);break;
    case 5:enemy.phaseTime=std::bit_cast<float>(0x7fc01234u);break;
    case 6:enemy.playerCooldown=std::bit_cast<float>(0x7fc01234u);break;
    case 7:enemy.playerCooldown=-.001f;break;
    case 8:enemy.playerCooldown=std::nextafter(.65f,INFINITY);break;
    case 9:enemy.countered=true;break;
    case 10:enemy.health=2;enemy.phase=EnemyPhase::Windup;enemy.phaseTime=.5f;enemy.countered=true;break;
    case 11:enemy.health=2;enemy.phase=EnemyPhase::Recovery;enemy.phaseTime=0;break;
    case 12:enemy.health=0;enemy.phase=EnemyPhase::Defeated;enemy.countered=false;break;
    case 13:enemy.health=0;enemy.phase=EnemyPhase::Defeated;enemy.countered=true;enemy.phaseTime=std::nextafter(Encounter::RecoveryDuration,INFINITY);break;
    }
    Require(!enemy.Valid(),"invalid-enemy negative precondition not genuinely invalid");return enemy;
}
void Negative(Fixture& player,const Session& session,Encounter enemy,float dt,bool enabled,const char* reason){
    CharacterDirectDisplacementPreviewToken3D token;
    Require(player.actor.PreviewDirectDisplacementTick({player.actor.DirectLastTick()+1,{},0},EmptyBoxes,token),"negative real pending preview setup");
    const auto beforePlayer=Whole(player.actor,player.owner,player.evasion,player.explorer);const auto beforeEnemy=enemy;
    const auto* vertices=player.explorer.model.bindMesh.vertices.data();const auto* skeleton=player.explorer.model.skeleton.joints.data();
    P::State state;unsigned callbacks{};
    const auto result=P::AdvanceNpc(state,session,enemy,player.actor,dt,true,1,{},player.terrain,
        [&](const Encounter&){++callbacks;return true;},enabled);
    Require(result.status==P::Status::InvalidInput&&!result.inputConsumed&&result.candidateTicks==0&&result.committedTicks==0&&
        result.motion.damage==0&&result.audit.count==0&&callbacks==0&&state.lastSerial==0&&P::Same(enemy,beforeEnemy),reason);
    Require(Whole(player.actor,player.owner,player.evasion,player.explorer)==beforePlayer&&
        player.actor.CanCommitDirectDisplacementPreview(token)&&player.explorer.model.bindMesh.vertices.data()==vertices&&
        player.explorer.model.skeleton.joints.data()==skeleton,"invalid input changed player/motor/pose/queue or true pending preview");
    Require(player.actor.CancelDirectDisplacementPreview(token),"negative preview token cleanup");++negativeCases;
}
void Positive(Fixture& player,const Session& session,Encounter enemy,float dt,bool enabled){
    P::State state;unsigned callbacks{};auto expected=enemy;
    const bool active=session.CurrentStage()==Stage::Encounter;
    const auto legacy=active?TickEncounterWorld(expected,session.feet,dt,false,{},player.terrain):ResolveEncounterMove(enemy.feet,{}, {},player.terrain);
    const auto beforePlayer=Whole(player.actor,player.owner,player.evasion,player.explorer);
    const auto result=P::AdvanceNpc(state,session,enemy,player.actor,dt,false,1,{},player.terrain,
        [&](const Encounter&){++callbacks;return true;},enabled);
    Require(result.status==P::Status::Applied&&result.inputConsumed&&state.lastSerial==1&&callbacks==1&&
        result.candidateTicks==(active?1u:0u)&&result.committedTicks==(active?1u:0u)&&
        result.motion.damage==legacy.damage&&P::Same(enemy,expected)&&Whole(player.actor,player.owner,player.evasion,player.explorer)==beforePlayer,
        "legal dt/defeated/pre-stage original parity changed");++positiveCases;
}
void Run(){
    for(unsigned profile=0;profile<5;++profile)for(bool enabled:{false,true}){
        Fixture player({3,0,0});Session session;session.feet=player.actor.Position();Encounter enemy;enemy.feet={};
        if(profile==1||profile==3||profile==4){session.collected=7;session.repairMaterials=7;session.bridgeRepaired=true;}
        if(profile==3)session.guardianDefeated=true;
        if(profile>=2){enemy.health=0;enemy.phase=EnemyPhase::Defeated;enemy.countered=true;}
        Require(session.Valid()&&enemy.Valid(),"guard baseline profile invalid");++profiles;
        for(unsigned kind=0;kind<14;++kind)Negative(player,session,InvalidEnemy(kind),1.f/60.f,enabled,"invalid enemy bypassed shared entry guard");
        for(float dt:{0.f,-0.f,-1.f/60.f,std::bit_cast<float>(0x7fc01234u),INFINITY,-INFINITY,std::nextafter(.1f,INFINITY)})
            Negative(player,session,enemy,dt,enabled,"illegal dt bypassed shared entry guard");
        for(unsigned kind=0;kind<5;++kind){auto bad=session;
            if(kind==0)bad.collected=8;
            if(kind==1)bad.repairMaterials=8;
            if(kind==2){bad.bridgeRepaired=false;bad.guardianDefeated=true;}
            if(kind==3)bad.health=101;
            if(kind==4)bad.feet.y=std::bit_cast<float>(0x7fc01234u);
            Require(!bad.Valid(),"invalid-stage/session precondition not invalid");
            Negative(player,bad,enemy,1.f/60.f,enabled,"invalid session/stage bypassed shared guard");
        }
        auto mismatch=session;mismatch.feet.x+=1;Require(mismatch.Valid(),"position mismatch control structurally invalid");
        Negative(player,mismatch,enemy,1.f/60.f,enabled,"session/player mismatch bypassed shared guard");
        for(float dt:{.0001f,1.f/60.f,.1f})Positive(player,session,enemy,dt,enabled);
    }
    std::cout<<"PAIR_SHARED_INPUT_GUARD_CPU_PASS profiles="<<profiles<<" negatives="<<negativeCases<<" positives="<<positiveCases
        <<" zero_candidate_ticks=1 zero_committed_ticks=1 zero_damage=1 zero_callbacks=1 atomic_player_enemy_serial_preview=1 checks="<<checks<<'\n';
}
}
