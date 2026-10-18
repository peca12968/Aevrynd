#include "MistwatchPairContact.h"
#include <MistwatchBoar.h>
#include <Aevrynd/SkinnedModelAsset.h>
#include <iomanip>
#define main PreservedGameplayMain
#include "support/MistwatchGameplayDynamicContactCases.inc"
#undef main

namespace PairTests {
using namespace Aevrynd;using namespace Mistwatch;namespace P=PairContact;
unsigned checks{},approaches{},earlyFalls{},yieldFrames{},combatTicks{},segments{},slopeCorrections{},legacyTicks{},envelopeChecks{};
SkinnedModel3D rawBoar;
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
Session Battle(Vec3 feet){Session s;s.collected=7;s.repairMaterials=7;s.bridgeRepaired=true;s.feet=feet;Require(s.Valid(),"battle fixture");return s;}
void ClearPair(const CharacterController3D& player,Vec3 enemy){BoxCylinderContact3D q;
    Require(QueryBoxCylinderContact3D(player.Center(),player.Extents(),{},P::BodyAt(enemy),q)&&q.kind==BoxCylinderContactKind3D::Clear,"actual pair overlaps");}
void AuditSegments(const P::Audit& audit,std::ofstream& log,unsigned frame){
    for(unsigned i=0;i<audit.count;++i){const auto& s=audit.segments[i];
        log<<frame<<'\t'<<i<<'\t'<<s.phase<<'\t'<<s.committed<<'\t'<<s.valid<<'\t'<<s.clear
            <<'\t'<<s.from.x<<'\t'<<s.from.y<<'\t'<<s.from.z<<'\t'<<s.to.x<<'\t'<<s.to.y<<'\t'<<s.to.z
            <<'\t'<<s.envelopeCenter.x<<'\t'<<s.envelopeCenter.y<<'\t'<<s.envelopeCenter.z
            <<'\t'<<s.envelopeHalf.x<<'\t'<<s.envelopeHalf.y<<'\t'<<s.envelopeHalf.z<<'\n';
        if(s.committed){Require(s.valid&&s.clear,"committed NPC subsegment not separated");++segments;
            if(s.phase==1&&s.from.y!=s.to.y)++slopeCorrections;}
    }
}
struct BoarWritable {
    AnimationPose3D pose;float yaw,cycle,displacement,brace;std::array<Vec3,4> targets;std::array<bool,4> planted;Vec3 last;bool initialized;
    explicit BoarWritable(const Boar& b):pose(b.pose),yaw(b.yaw),cycle(b.cycle),displacement(b.displacement),brace(b.brace),targets(b.footTargets),planted(b.planted),last(b.lastFeet),initialized(b.initialized){}
    void Restore(Boar& b){b.pose=std::move(pose);b.yaw=yaw;b.cycle=cycle;b.displacement=displacement;b.brace=brace;b.footTargets=targets;b.planted=planted;b.lastFeet=last;b.initialized=initialized;}
};
void InitBoar(Boar& boar,const Encounter& e,const HeightField3D& terrain){boar.model=rawBoar;
    Require(boar.Update({},0,e.phase,(std::max)(0.f,e.phaseTime),{1,0,0})&&boar.PlaceFeet(e.feet,terrain),"raw NPC initial actual pose");}
bool Pose(Boar& boar,const Encounter& old,const Encounter& next,Vec3 player,float dt,const HeightField3D& terrain){
    BoarWritable prior(boar);
    const bool ok=boar.Update(next.feet-old.feet,dt,next.phase,(std::max)(0.f,next.phaseTime),
        (next.phase==EnemyPhase::Windup?next.strikeTarget:player)-next.feet)&&boar.PlaceFeet(next.feet,terrain);
    if(!ok)prior.Restore(boar);return ok;
}
void LiveBodyAndEnvelope(){
    Session s;Encounter e;Require(P::LiveBody(s,e).has_value(),"visible pre-Encounter NPC missing body");
    e.health=0;e.phase=EnemyPhase::Defeated;e.countered=true;Require(!P::LiveBody(s,e),"defeated same-frame body retained");
    e={};s.guardianDefeated=true;Require(!P::LiveBody(s,e),"finished session body retained");
    for(float origin:{0.f,1.f,-1.f,100000.f,-100000.f,8388607.f})for(int side:{-1,1})for(unsigned i=0;i<32;++i){
        float destination=origin;for(unsigned j=0;j<=i;++j)destination=std::nextafter(destination,side>0?INFINITY:-INFINITY);
        const Vec3 a{origin,origin,-origin},b{destination,-destination,destination};Vec3 c,h;
        Require(P::Envelope(a,b,c,h),"finite outward envelope rejected");
        for(unsigned axis=0;axis<3;++axis){const float av=axis==0?a.x:axis==1?a.y:a.z,bv=axis==0?b.x:axis==1?b.y:b.z;
            const float cv=axis==0?c.x:axis==1?c.y:c.z,hv=axis==0?h.x:axis==1?h.y:h.z;
            Require(double(cv)-hv<=(std::min)(double(av),double(bv))&&double(cv)+hv>=(std::max)(double(av),double(bv)),"envelope rounded inward");++envelopeChecks;}
    }
    Vec3 c,h;Require(!P::Envelope({NAN,0,0},{},c,h),"NaN envelope accepted");
}
void LegacyParity(){
    Fixture f;P::State state;Encounter old,current;const auto terrain=f.terrain;
    for(unsigned tick=0;tick<600;++tick){const Vec3 player=tick<90?Vec3{0,0,43}:current.feet;
        f.actor.Reset(player);auto session=Battle(player);
        const bool attack=current.phase==EnemyPhase::Recovery&&!current.countered&&current.playerCooldown<=0;
        const auto expected=TickEncounterWorld(old,player,.02f,attack,{},terrain);
        const auto result=P::AdvanceNpc(state,session,current,f.actor,.02f,attack,tick+1,{},terrain,[](const Encounter&){return true;},false);
        Require(result.status==P::Status::Applied&&P::Same(old,current)&&expected.status==result.motion.status&&
            expected.collisions==result.motion.collisions&&expected.damage==result.motion.damage&&P::Same(expected.feet,result.motion.feet),"empty opt-out wrapper timing/bit parity");
        Require(result.committedTicks==1,"legacy Tick count");++legacyTicks;
    }
    Require(current.health==0,"original three-counter fixture missed defeat");
    for(unsigned tick=0;tick<60;++tick){auto session=Battle(current.feet);session.guardianDefeated=true;f.actor.Reset(session.feet);
        const auto before=current;const auto result=P::AdvanceNpc(state,session,current,f.actor,.02f,false,601+tick,{},terrain,[](const Encounter&){return true;});
        Require(result.status==P::Status::Applied&&result.committedTicks==0&&P::Same(before,current),"defeated original wrapper differs");++legacyTicks;
    }
}
void NpcApproaches(std::ofstream& log){
    Fixture f;const P::PlayerBox box{f.actor.Center(),f.actor.Extents()};P::LandingReservation reservation;
    for(unsigned direction=0;direction<8;++direction){const float angle=float(direction)*.7853981634f;
        const Vec3 unit{std::sin(angle),0,std::cos(angle)};Vec3 feet=unit*-3.5f;bool stopped{};
        for(unsigned tick=0;tick<30;++tick){P::Audit audit;bool numeric{};
            const auto result=P::CheckedMove(feet,unit*.2f,box,reservation,false,{},f.terrain,audit,numeric);
            Require(!numeric&&result.status!=EncounterMotionStatus::Invalid,"NPC approach rejected legal side geometry");
            stopped|=result.status==EncounterMotionStatus::Blocked;feet=result.feet;ClearPair(f.actor,feet);AuditSegments(audit,log,1000+direction*30+tick);
        }
        Require(stopped&&std::hypot(feet.x,feet.z)<2.2f&&std::hypot(feet.x,feet.z)>1.74f,"NPC cannot stop within unchanged charge range");++approaches;
    }
}
void SlideAndSlope(std::ofstream& log){
    Fixture f;P::LandingReservation reservation;
    const std::array<BoxCollider3D,1> wall{{{{-1.5f,1,-2},{.01f,4,10}}}};
    Vec3 feet{-2.85f,0,-2};const P::PlayerBox box{{4,.9f,4},{.45f,.9f,.45f}};
    P::Audit audit;bool numeric{};
    const auto oldBad=ResolveEncounterMove(feet,{.18f,0,.12f},wall,f.terrain);
    const auto newBad=P::CheckedMove(feet,{.18f,0,.12f},box,reservation,false,wall,f.terrain,audit,numeric);
    Require(oldBad.status==EncounterMotionStatus::Invalid&&newBad.status==EncounterMotionStatus::Invalid,"preserved invalid-length control not rejected");
    audit={};numeric=false;const auto move=P::CheckedMove(feet,{.12f,0,.12f},box,reservation,false,wall,f.terrain,audit,numeric);
    std::cout<<"SLIDE_LEGAL_READBACK old_bad_status="<<int(oldBad.status)<<" new_bad_status="<<int(newBad.status)
        <<" status="<<int(move.status)<<" collisions="<<move.collisions<<" x="<<move.feet.x<<" z="<<move.feet.z<<" numeric="<<numeric<<'\n';
    Require(!numeric&&move.status==EncounterMotionStatus::Blocked&&move.collisions>0&&move.feet.z>feet.z+.1f,"real NPC box slide lost free axis");
    AuditSegments(audit,log,2000);bool bent{};
    for(unsigned i=0;i<audit.count;++i)if(audit.segments[i].phase==0){const auto& s=audit.segments[i];bent|=s.envelopeHalf.x>0&&s.envelopeHalf.z>0;}
    Require(bent,"real static slide envelope not checked");
    auto slope=f.terrain;
    for(unsigned z=0;z<slope.Settings().depth;++z)for(unsigned x=0;x<slope.Settings().width;++x){const auto p=slope.SamplePosition(x,z);Require(slope.SetHeight(x,z,.1f*p.x+.1f*p.z),"slope setup");}
    feet.y=slope.SampleHeight(feet.x,feet.z);audit={};numeric=false;
    const auto corrected=P::CheckedMove(feet,{.12f,0,.12f},box,reservation,false,wall,slope,audit,numeric);
    Require(!numeric&&corrected.status!=EncounterMotionStatus::Invalid&&EncounterSupported(corrected.feet,slope),"slope NPC correction invalid");
    AuditSegments(audit,log,2001);Require(slopeCorrections>0,"actual nonzero terrain correction never checked");
}
void OnceOnly(){
    Fixture f({1.8f,0,0});auto session=Battle(f.actor.Position());P::State state;Encounter e;e.feet={};e.phase=EnemyPhase::Windup;e.phaseTime=.01f;e.strikeTarget=session.feet;
    Boar boar;InitBoar(boar,e,f.terrain);const auto prior=e;
    const auto result=P::AdvanceNpc(state,session,e,f.actor,1.f/60.f,false,1,{},f.terrain,[&](const Encounter& n){return Pose(boar,prior,n,session.feet,1.f/60.f,f.terrain);});
    Require(result.status==P::Status::Applied&&result.motion.damage==20&&result.candidateTicks==1&&result.committedTicks==1&&
        e.phase==EnemyPhase::Recovery&&e.phaseTime==Encounter::RecoveryDuration,"damage/phase tick not exactly once");
    session.Damage(result.motion.damage);const auto after=e;
    const auto replay=P::AdvanceNpc(state,session,e,f.actor,1.f/60.f,true,1,{},f.terrain,[](const Encounter&){return true;});
    Require(replay.status==P::Status::InvalidInput&&!replay.inputConsumed&&replay.candidateTicks==0&&session.health==80&&P::Same(after,e),"same serial replay advanced damage/time");
    e.health=1;e.phase=EnemyPhase::Recovery;e.phaseTime=.5f;e.countered=false;e.playerCooldown=0;
    const auto defeated=P::AdvanceNpc(state,session,e,f.actor,1.f/60.f,true,2,{},f.terrain,[](const Encounter&){return true;});
    Require(defeated.inputConsumed&&e.health==0&&!P::LiveBody(session,e)&&defeated.committedTicks==1,"counter defeat retained collision body");
}
void EarlyFall(bool active,std::ofstream& log){
    Fixture f;f.Air({0,12,0},{0,-1,0});Session session=active?Battle(f.actor.Position()):Session{};session.feet=f.actor.Position();
    Encounter e;e.feet={};e.phase=EnemyPhase::Recovery;e.phaseTime=1.2f;P::State state;Boar boar;InitBoar(boar,e,f.terrain);
    unsigned recovery{},landed{},yields{};const auto initialLandings=f.actor.Stats().landings;
    for(unsigned tick=0;tick<120;++tick){const auto body=P::LiveBody(session,e);std::array<DynamicCylinder3D,1> bodies{};if(body)bodies[0]=*body;
        const auto result=f.Step({},body?std::span<const DynamicCylinder3D>(bodies):std::span<const DynamicCylinder3D>{},2*LocomotionMotor::FixedStep);
        Require(result.motion.accepted,"early reserved falling real Player step rejected");session.feet=f.actor.Position();const auto before=e;
        auto expected=e;const auto expectedDamage=active?expected.Tick(session.feet,2*LocomotionMotor::FixedStep,false):0u;
        const auto update=P::AdvanceNpc(state,session,e,f.actor,2*LocomotionMotor::FixedStep,false,tick+1,{},f.terrain,
            [&](const Encounter& n){return Pose(boar,before,n,session.feet,2*LocomotionMotor::FixedStep,f.terrain);});
        recovery+=update.status==P::Status::RecoveryRequired;
        Require(update.inputConsumed&&(update.status==P::Status::Applied||update.status==P::Status::Yielded),"normal reachable early falling returned recovery/blocked");
        Require(update.committedTicks==(active?1u:0u)&&update.motion.damage==expectedDamage,"yield advanced/dropped encounter dt");
        expected.feet=e.feet;Require(P::Same(expected,e),"yield phase/cooldown differs from one real Tick");
        Require(Length(e.feet-before.feet)<=2.f*(2*LocomotionMotor::FixedStep)+.000001f,"yield exceeded original NPC speed");
        session.Damage(update.motion.damage);ClearPair(f.actor,e.feet);AuditSegments(update.audit,log,3000+unsigned(active)*120+tick);
        yields+=update.status==P::Status::Yielded;combatTicks+=update.committedTicks;
        if(f.actor.Grounded()){Require(f.actor.Position().y==0,"NPC granted false terrain support");++landed;}
    }
    Require(recovery==0&&yields>0&&landed>0&&f.actor.Grounded()&&f.actor.Stats().landings==initialLandings+1,"early fall did not auto-yield and land");
    Require(std::hypot(e.feet.x,e.feet.z)<2.2f,"yield changed accessible encounter range");yieldFrames+=yields;++earlyFalls;
    std::cout<<"EARLY_FALL_AUTOMATIC active="<<active<<" frames=120 yielded="<<yields<<" recovery="<<recovery<<" final_y="<<f.actor.Position().y
        <<" NPC_x="<<e.feet.x<<" NPC_z="<<e.feet.z<<" real_raw_boar_pose=1\n";
}
void EnclosedAndReject(){
    Fixture f;f.Air({0,12,0},{0,-1,0});auto session=Battle(f.actor.Position());Encounter e;e.feet={};e.phase=EnemyPhase::Recovery;e.phaseTime=1;
    const std::array<BoxCollider3D,4> walls{{{{1.3f,.5f,0},{0,5,5}},{{-1.3f,.5f,0},{0,5,5}},{{0,.5f,1.3f},{5,5,0}},{{0,.5f,-1.3f},{5,5,0}}}};
    Require(!EncounterBodyOverlaps(e.feet,walls)&&P::AxisEnclosed(e.feet,walls),"closed static constraint fixture");
    CharacterDirectDisplacementPreviewToken3D token;Require(f.actor.PreviewDirectDisplacementTick({1,{},0},EmptyBoxes,token),"hold real pending preview");
    const auto before=Whole(f.actor,f.owner,f.evasion,f.explorer);const auto prior=e;P::State state;
    const auto held=P::AdvanceNpc(state,session,e,f.actor,1.f/60.f,true,1,walls,f.terrain,[](const Encounter&){return true;});
    Require(held.status==P::Status::RecoveryRequired&&!held.inputConsumed&&held.committedTicks==0&&held.audit.axisEnclosed&&
        held.audit.candidates==8&&held.audit.staticBlocked==8&&state.lastSerial==0&&P::Same(e,prior)&&
        Whole(f.actor,f.owner,f.evasion,f.explorer)==before&&f.actor.CanCommitDirectDisplacementPreview(token),"fully enclosed recovery partially advanced actors/time/input");
    Fixture overlap;session=Battle(overlap.actor.Position());e=prior;P::State overlapState;
    const auto unresolved=P::AdvanceNpc(overlapState,session,e,overlap.actor,1.f/60.f,false,1,{},overlap.terrain,[](const Encounter&){return true;});
    Require(unresolved.status==P::Status::UnresolvedInitialOverlap&&!unresolved.inputConsumed&&P::Same(e,prior),"unproven overlap mislabeled as recovery or clear");
    Fixture safe({2,0,0});session=Battle(safe.actor.Position());e=prior;P::State rejected;const auto beforeE=e;
    const auto reject=P::AdvanceNpc(rejected,session,e,safe.actor,1.f/60.f,false,1,{},safe.terrain,[](const Encounter&){return false;});
    Require(reject.status==P::Status::PoseRejected&&!reject.inputConsumed&&rejected.lastSerial==0&&P::Same(beforeE,e),"NPC pose rejection partially committed");
}
}
#include "support/MistwatchPairInputGuardCases.h"
int main(int argc,char** argv){try{
    if(PreservedGameplayMain()!=0)return 1;
    using namespace PairTests;Require(argc==2,"raw asset argument required");const auto asset=Aevrynd::LoadSkinnedModelAssetFile(argv[1]);
    Require(bool(asset),"real raw asset failed to load");rawBoar=asset.model;
    std::ofstream log("pair-segments.tsv",std::ios::binary);Require(bool(log),"segment log open");log<<std::setprecision(9);
    LiveBodyAndEnvelope();LegacyParity();NpcApproaches(log);SlideAndSlope(log);OnceOnly();EarlyFall(false,log);EarlyFall(true,log);EnclosedAndReject();
    GuardInput::Run();
    Require(bool(log),"segment log write");
    std::cout<<"SEQUENTIAL_PAIR_MINIMAL_CPU_PASS legacy_ticks="<<PairTests::legacyTicks<<" npc_approaches="<<PairTests::approaches<<" early_falls="<<PairTests::earlyFalls
        <<" yield_frames="<<PairTests::yieldFrames<<" committed_combat_ticks="<<PairTests::combatTicks<<" committed_segments="<<PairTests::segments
        <<" actual_slope_corrections="<<PairTests::slopeCorrections<<" outward_bound_checks="<<PairTests::envelopeChecks<<" checks="<<PairTests::checks
        <<" main=0 camera_transaction=0 save_integration=0 simultaneous_CCD=0 GPU=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"PAIR_GREEN_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
