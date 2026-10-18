#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
using namespace Aevrynd;
namespace {
unsigned checks{};
void Need(bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Near(float a,float b,float tolerance=.0005f){return std::abs(a-b)<=tolerance;}
HeightField3D Plane(){
    HeightField3D field({{-8,0,-8},17,17,1});
    for(unsigned z=0;z<17;++z)for(unsigned x=0;x<17;++x)
        Need(field.SetHeight(x,z,2.f*(float(x)-8.f)),"plane fixture height");
    field.RecalculateRange();return field;
}
CharacterControllerSettings3D Policy(float grade=1.f){CharacterControllerSettings3D s;s.maximumTerrainUphillGrade=grade;return s;}
void Ground(CharacterController3D& actor,const HeightField3D& terrain,Vec3 feet={},
    std::span<const BoxCollider3D> boxes={},std::span<const ConvexAabbSweep3D* const> hulls={}){
    actor.Reset(feet);
    Need(actor.TryRestoreMotionState({feet,{},1.8f,0,true},boxes,hulls,terrain),"restore actual support fixture");
}
std::unique_ptr<ConvexAabbSweep3D> FarHull(){
    auto hull=std::make_unique<ConvexAabbSweep3D>();std::array<Vec3,8> points{};unsigned i=0;
    for(float x:{30.f,31.f})for(float y:{0.f,1.f})for(float z:{30.f,31.f})points[i++]={x,y,z};
    Need(hull->Prepare(points),"prepare nonintersecting convex fixture");return hull;
}
void TerrainTicks(const HeightField3D& terrain){
    auto hull=FarHull();const std::array<const ConvexAabbSweep3D*,1> owned{hull.get()};
    for(bool withHull:{false,true}){
        const auto hulls=withHull?std::span<const ConvexAabbSweep3D* const>(owned):std::span<const ConvexAabbSweep3D* const>{};
        CharacterController3D actor(Policy());Ground(actor,terrain,{}, {},hulls);
        const auto before=actor.Stats();const auto oldHash=actor.StateHash();
        for(unsigned i=0;i<120;++i){
            Need(actor.Step({{1,0,0},false},{},hulls,terrain,1.f/120),"uphill block is accepted simulation tick");
            Need(Same(actor.Position(),{})&&Same(actor.Velocity(),{})&&actor.Grounded(),"steep climb stopped without drift");
            Need(actor.Stats().steps==before.steps+i+1,"blocked step advances timeline rather than rollback");
        }
        Need(actor.StateHash()!=oldHash&&actor.Stats().collisions>before.collisions,"blocked ticks record real state and contact");
        for(unsigned i=0;i<10;++i)Need(actor.Step({{-1,0,0},false},{},hulls,terrain,1.f/120),"downhill is accepted");
        Need(actor.Position().x<-.4f&&Near(actor.Position().y,terrain.SampleHeight(actor.Position().x,actor.Position().z)),"downhill moves along active terrain");
        Ground(actor,terrain,{}, {},hulls);
        for(unsigned i=0;i<10;++i)Need(actor.Step({{0,0,1},false},{},hulls,terrain,1.f/120),"steep tangent accepted");
        Need(actor.Position().z>.4f&&actor.Position().x==0&&actor.Position().y==0,"tangent not mistaken for uphill");
        Ground(actor,terrain,{}, {},hulls);
        Need(actor.Step({{.1f,0,0},true},{},hulls,terrain,1.f/120),"jump tick accepted");
        Need(!actor.Grounded()&&actor.Position().x>0&&actor.Position().y>terrain.SampleHeight(actor.Position().x,actor.Position().z)&&actor.Stats().jumps==1,"real ground jump clears terrain and moves");
        actor.Reset({0,4,0});
        Need(actor.TryRestoreMotionState({{0,4,0},{0,1,0},1.8f,0,false},{},hulls,terrain),"restore valid airborne fixture");
        Need(actor.Step({{1,0,0},false},{},hulls,terrain,1.f/120)&&actor.Position().x>0&&!actor.Grounded(),"airborne above steep face not blocked");
    }
    CharacterController3D ordinary(Policy());Ground(ordinary,terrain);
    ordinary.Step({{1,0,0},false},std::span<const BoxCollider3D>{},terrain,1.f/60);
    Need(Same(ordinary.Position(),{})&&ordinary.Stats().steps==1,"ordinary terrain overload uses policy");
    CollisionWorld3D world;
    CharacterController3D queried(Policy());Ground(queried,terrain);
    queried.Step({{1,0,0},false},world,terrain,1.f/60);
    Need(Same(queried.Position(),{})&&queried.Stats().steps==1,"world terrain overload uses policy");
}
void Bridge(const HeightField3D& terrain){
    const std::array<BoxCollider3D,1> bridge{{{{0,5,0},{4,.25f,4}}}};
    CharacterController3D actor(Policy());Ground(actor,terrain,{0,5.25f,0},bridge);
    for(unsigned i=0;i<10;++i)Need(actor.Step({{1,0,0},false},bridge,{},terrain,1.f/120),"bridge tick accepted");
    Need(actor.Position().x>.4f&&actor.Position().y>=5.25f&&actor.Position().y<5.251f&&actor.Grounded(),"steep terrain below bridge does not stop or pull actor down");
    Ground(actor,terrain,{0,5.25f,0},bridge);CharacterDirectDisplacementResult3D result;
    Need(actor.ApplyDirectDisplacementTick({1,{.5f,0,0},0},bridge,terrain,&result),"bridge direct tick accepted");
    Need(result.status==CharacterDirectDisplacementStatus3D::Applied&&Near(actor.Position().x,.5f)&&actor.Position().y==5.25f,"direct bridge traversal ignores terrain uphill restriction");
}
void Direct(const HeightField3D& terrain){
    CharacterController3D actor(Policy());Ground(actor,terrain);
    CharacterDirectDisplacementPreviewToken3D token;CharacterDirectDisplacementResult3D result;
    const auto hash=actor.StateHash();
    Need(actor.PreviewDirectDisplacementTick({1,{.5f,0,0},.1f},{},terrain,token,&result),"blocked direct preview accepted");
    Need(result.status==CharacterDirectDisplacementStatus3D::Blocked&&Same(result.consumedWorld,{})&&Same(result.remainderWorld,{.5f,0,0}),"preview retains requested consumed remainder accounting");
    Need(actor.StateHash()==hash&&actor.DirectLastTick()==0&&actor.CanCommitDirectDisplacementPreview(token),"preview does not mutate committed state and is committable");
    Need(actor.CancelDirectDisplacementPreview(token)&&actor.StateHash()==hash&&!actor.CanCommitDirectDisplacementPreview(token),"cancel preserves committed state and consumes token");
    Need(actor.ApplyDirectDisplacementTick({1,{.5f,0,0},.1f},{},terrain,&result),"blocked direct apply commits");
    Need(actor.DirectLastTick()==1&&actor.Stats().directTicks==1&&actor.Stats().directBlocked==1&&Same(actor.Position(),{})&&Near(actor.Yaw(),.1f),"blocked direct consumes tick and yaw without climbing");
    Need(actor.ApplyDirectDisplacementTick({2,{0,0,.25f},0},{},terrain,&result)&&result.status==CharacterDirectDisplacementStatus3D::Applied&&Near(actor.Position().z,.25f),"direct tangent after block proceeds");
    Need(actor.ApplyDirectDisplacementTick({3,{-.125f,0,0},0},{},terrain,&result)&&Near(actor.Position().x,-.125f)&&Near(actor.Position().y,-.25f),"direct downhill within snap range after block proceeds");
    Need(Same(result.requestedWorld,result.consumedWorld+result.remainderWorld),"ground correction separate from requested accounting");
    CollisionWorld3D world;CharacterController3D queried(Policy());Need(queried.PrepareDirectDisplacement(1),"prepare direct world capacity");Ground(queried,terrain);
    Need(queried.ApplyDirectDisplacementTick({1,{.5f,0,0},0},world,terrain,{},&result)&&result.status==CharacterDirectDisplacementStatus3D::Blocked&&queried.DirectLastTick()==1,"world direct path obeys policy and commits");
}
void InvalidAndLegacy(const HeightField3D& terrain){
    for(float grade:{-1.f,17.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        CharacterController3D actor(Policy(grade));actor.Reset({});const auto hash=actor.StateHash();
        Need(!actor.Step({{1,0,0},false},{},{},terrain,1.f/60)&&actor.StateHash()==hash,"invalid policy rejects bool step transactionally");
        actor.Step({{1,0,0},false},std::span<const BoxCollider3D>{},terrain,1.f/60);
        Need(actor.StateHash()==hash,"invalid policy ordinary step unchanged");
        CollisionWorld3D world;actor.Step({{1,0,0},false},world,terrain,1.f/60);
        Need(actor.StateHash()==hash,"invalid policy world step unchanged");
        CharacterDirectDisplacementResult3D result;result.tick=77;result.deterministicHash=99;
        Need(!actor.ApplyDirectDisplacementTick({1,{.5f,0,0},0},{},terrain,&result)&&actor.StateHash()==hash&&result.tick==77&&result.deterministicHash==99,"invalid policy direct leaves state and output unchanged");
        Need(!actor.TryRestoreMotionState({{}, {},1.8f,0,true},{},{},terrain)&&actor.StateHash()==hash,"invalid policy restore unchanged");
    }
    CharacterController3D actor(Policy());Ground(actor,terrain);auto hash=actor.StateHash();
    Need(!actor.Step({{std::numeric_limits<float>::quiet_NaN(),0,0},false},{},{},terrain,1.f/60)&&actor.StateHash()==hash,"nonfinite movement fails atomically");
    CharacterDirectDisplacementResult3D result;result.tick=77;
    Need(actor.ApplyDirectDisplacementTick({1,{9,0,0},0},{},terrain,&result)&&Same(actor.Position(),{})&&result.status==CharacterDirectDisplacementStatus3D::Blocked,"steep crossing toward field exterior stops at real surface not domain failure");
    CharacterController3D defaultActor,disabled(Policy(0));Ground(defaultActor,terrain);Ground(disabled,terrain);
    Need(defaultActor.StateHash()==disabled.StateHash(),"disabled policy has legacy initial identity");
    for(unsigned i=0;i<20;++i){
        const CharacterControl3D input{{.2f,0,.1f},false};
        defaultActor.Step(input,std::span<const BoxCollider3D>{},terrain,1.f/120);
        disabled.Step(input,std::span<const BoxCollider3D>{},terrain,1.f/120);
        Need(defaultActor.StateHash()==disabled.StateHash()&&Same(defaultActor.Position(),disabled.Position()),"default and explicit disabled trajectories and hashes match");
    }
    Need(defaultActor.Position().x>0&&defaultActor.Position().y>0,"disabled policy preserves legacy steep ascent");
    CharacterController3D enabled(Policy()),zero(Policy(0));enabled.Reset({});zero.Reset({});
    Need(enabled.StateHash()!=zero.StateHash(),"enabled policy participates in state identity");
}
void Adversarial(const HeightField3D& terrain){
    CharacterController3D actor(Policy());Ground(actor,terrain);CharacterDirectDisplacementResult3D result;
    Need(actor.ApplyDirectDisplacementTick({1,{1,.001f,0},0},{},terrain,&result),"positive epsilon request is accepted blocked tick");
    Need(actor.Position().x==0&&Near(actor.Position().y,.001f,.000001f)&&result.groundCorrectionWorld.y==0&&result.remainderWorld.x==1,"positive epsilon cannot climb by two metre upward snap");
    Need(actor.ApplyDirectDisplacementTick({2,{1,0,0},0},{},terrain,&result),"next airborne epsilon tick accepted");
    Need(actor.Position().x<=.00051f&&actor.Position().y<.002f&&result.groundCorrectionWorld.y<.00001f,"epsilon lift then second tick cannot bypass terrain");
    Ground(actor,terrain);
    Need(actor.ApplyDirectDisplacementTick({1,{1,2,0},0},{},terrain,&result),"exact steep surface path commits restricted tick");
    Need(actor.Position().x==0&&result.remainderWorld.x==1&&result.groundCorrectionWorld.y==0,"exact positive rise along steep face is contact not genuine takeoff");
    Ground(actor,terrain);
    Need(actor.ApplyDirectDisplacementTick({1,{1,3,0},0},{},terrain,&result)&&Near(actor.Position().x,1)&&Near(actor.Position().y,3)&&result.groundCorrectionWorld.y==0,"genuine upward segment fully clears steep surface");
    actor.Reset({0,1,0});
    Need(actor.TryRestoreMotionState({{0,1,0},{},1.8f,0,false},{},{},terrain),"prepare airborne contact later in segment");
    Need(actor.ApplyDirectDisplacementTick({1,{1,0,0},0},{},terrain,&result),"airborne segment reaching steep face accepted");
    Need(actor.Position().x>.49f&&actor.Position().x<=.5f&&Near(actor.Position().y,1)&&std::abs(result.groundCorrectionWorld.y)<.00001f,"retain airborne prefix until actual contact rather than freeze at start");
    actor.Reset({0,2,0});
    Need(actor.TryRestoreMotionState({{0,2,0},{},1.8f,0,false},{},{},terrain),"prepare airborne exact endpoint contact");
    Need(actor.ApplyDirectDisplacementTick({1,{1,0,0},0},{},terrain,&result)&&actor.Position().x==1&&actor.Position().y==2&&result.groundCorrectionWorld.y==0,"single endpoint touch preserves entire clear airborne prefix");
    HeightField3D small({{-2,0,-2},5,5,1});small.Fill(0);
    const std::array<BoxCollider3D,1> bridge{{{{2,10,0},{4,.25f,4}}}};
    for(bool grounded:{false,true}){
        CharacterController3D crossing(Policy());crossing.Reset({1.99f,10.25f,0});
        Need(crossing.TryRestoreMotionState({{1.99f,10.25f,0},grounded?Vec3{}:Vec3{0,1,0},1.8f,0,grounded},grounded?std::span<const BoxCollider3D>(bridge):std::span<const BoxCollider3D>{},{},small),"prepare finite boundary fixture");
        const auto boxes=grounded?std::span<const BoxCollider3D>(bridge):std::span<const BoxCollider3D>{};
        Need(crossing.Step({{1,0,0},false},boxes,{},small,1.f/120)&&crossing.Position().x>2,"bridge or airborne can cross unrelated terrain boundary");
        Need(crossing.Step({},boxes,{},small,1.f/120),"stationary outside terrain remains valid");
        Need(crossing.ApplyDirectDisplacementTick({1,{.1f,0,0},0},boxes,small,&result)&&crossing.Position().x>2.1f,"direct finite outside terrain remains valid");
    }
    auto corrupt=terrain;Need(corrupt.SetHeight(8,8,std::numeric_limits<float>::quiet_NaN()),"inject hostile intersecting height");
    CharacterController3D reject(Policy());reject.Reset({0,4,0});const auto before=reject.StateHash();
    Need(!reject.Step({{1,0,0},false},{},{},corrupt,1.f/120)&&reject.StateHash()==before,"airborne does not bypass intersecting NaN terrain validation");
    result.tick=77;
    Need(!reject.ApplyDirectDisplacementTick({1,{.1f,0,0},0},{},corrupt,&result)&&reject.StateHash()==before&&result.tick==77,"direct intersecting NaN terrain failure is transactional");
}
}
int main(){try{
    const auto terrain=Plane();TerrainTicks(terrain);Bridge(terrain);Direct(terrain);InvalidAndLegacy(terrain);Adversarial(terrain);
    std::cout<<"CHARACTER_TERRAIN_UPHILL_PASS checks="<<checks<<" uphill_ticks=240 bridge=1 airborne=1 direct_preview_cancel_apply=1 invalid_policy=4 legacy_disabled=1 positive_y_bypass_rejected=1 finite_boundary=1 airborne_prefix=1 native_gameplay=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"CHARACTER_TERRAIN_UPHILL_FAIL checks="<<checks<<" reason="<<e.what()<<'\n';return 1;}}
