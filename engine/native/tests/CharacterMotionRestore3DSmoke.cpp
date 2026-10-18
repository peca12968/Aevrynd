#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
using namespace Aevrynd;
namespace {
unsigned checks{};
void Need(bool p,const char* why){++checks;if(!p)throw std::runtime_error(why);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(CharacterMotionState3D a,CharacterMotionState3D b){return Same(a.feet,b.feet)&&Same(a.velocity,b.velocity)&&a.height==b.height&&a.yaw==b.yaw&&a.grounded==b.grounded;}
bool Same(CharacterControllerStats3D a,CharacterControllerStats3D b){return a.steps==b.steps&&a.collisions==b.collisions&&a.jumps==b.jumps&&a.landings==b.landings&&a.terrainContacts==b.terrainContacts&&a.directTicks==b.directTicks&&a.directBlocked==b.directBlocked&&a.directOverlaps==b.directOverlaps;}
std::unique_ptr<ConvexAabbSweep3D> Floor(float slope){
    auto hull=std::make_unique<ConvexAabbSweep3D>();std::array<Vec3,8> points{};unsigned i=0;
    for(float x:{-5.f,5.f})for(float z:{-5.f,5.f})for(float thickness:{-.5f,0.f})points[i++]={x,1+slope*x+thickness,z};
    Need(hull->Prepare(points),"convex fixture prepare");return hull;
}
void ExactAndFuture(){
    HeightField3D terrain({{-20,0,-20},41,41,1});terrain.Fill(0);
    for(float slope:{0.f,.2f}){
        auto floor=Floor(slope);std::array<const ConvexAabbSweep3D*,1> hulls{floor.get()};
        for(bool convex:{false,true}){
            const auto shapes=convex?std::span<const ConvexAabbSweep3D* const>(hulls):std::span<const ConvexAabbSweep3D* const>{};
            CharacterController3D source;source.Reset({0,convex?2.f:.5f,0});
            for(unsigned n=0;n<120&&!source.Grounded();++n)Need(source.Step({}, {},shapes,terrain,1.f/120),"settle real source");
            Need(source.Grounded(),"source has actual support");
            for(unsigned sample=0;sample<4;++sample){
                if(sample==1)Need(source.TryLaunchFromGround(2.96f),"actual launch boundary");
                if(sample==2||sample==3)for(unsigned n=0;n<(sample==2?5u:17u);++n)
                    Need(source.Step({{.2f,0,0},false}, {},shapes,terrain,1.f/120),"advance real flight");
                const auto snapshot=source.CaptureMotionState();
                CharacterController3D restored;restored.Reset({8,3,8});
                const auto stats=restored.Stats();const auto tick=restored.DirectLastTick();
                Need(restored.TryRestoreMotionState(snapshot,{},shapes,terrain),"restore actual ground ascending descending state");
                Need(Same(restored.CaptureMotionState(),snapshot),"restore exact fields no snap");
                Need(Same(stats,restored.Stats())&&restored.DirectLastTick()==tick,"restore no statistics or timeline events");
                auto uninterrupted=source;
                for(unsigned n=0;n<100;++n){
                    const CharacterControl3D input{{.1f,0,.15f},false};
                    Need(uninterrupted.Step(input,{},shapes,terrain,1.f/120)&&restored.Step(input,{},shapes,terrain,1.f/120),"future actual collision steps");
                    Need(Same(uninterrupted.CaptureMotionState(),restored.CaptureMotionState()),"restored trajectory matches uninterrupted simulation");
                }
            }
        }
    }
}
void ValidationAndTokens(){
    HeightField3D terrain({{-10,0,-10},21,21,1});terrain.Fill(0);
    CharacterController3D actor;actor.Reset({0,0,0});
    CharacterMotionState3D baseline{{0,0,0},{},1.8f,.5f,true};
    Need(actor.TryRestoreMotionState(baseline,{}, {},terrain),"ground restore");
    CharacterDirectDisplacementPreviewToken3D token;
    Need(actor.PreviewDirectDisplacementTick({1,{.1f,0,0},0},{},terrain,token),"create valid pending preview");
    const auto hash=actor.StateHash();const auto stats=actor.Stats();
    const auto reject=[&](CharacterMotionState3D bad,std::span<const BoxCollider3D> boxes={},std::span<const ConvexAabbSweep3D* const> hulls={}){
        Need(!actor.TryRestoreMotionState(bad,boxes,hulls,terrain),"reject unsafe snapshot");
        Need(actor.StateHash()==hash&&Same(actor.CaptureMotionState(),baseline)&&Same(actor.Stats(),stats)&&actor.CanCommitDirectDisplacementPreview(token),"failure preserves fields stats and pending token");
    };
    auto bad=baseline;bad.feet.y=-.001f;reject(bad);
    bad=baseline;bad.feet.x=20;reject(bad);
    bad=baseline;bad.feet.y=.01f;reject(bad);
    bad=baseline;bad.velocity.y=1;reject(bad);
    bad=baseline;bad.velocity.x=24.01f;reject(bad);
    bad=baseline;bad.grounded=false;bad.velocity.y=7.01f;reject(bad);
    bad=baseline;bad.grounded=false;bad.velocity.y=-35.01f;reject(bad);
    for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        bad=baseline;bad.feet.x=value;reject(bad);bad=baseline;bad.velocity.z=value;reject(bad);
        bad=baseline;bad.height=value;reject(bad);bad=baseline;bad.yaw=value;reject(bad);
    }
    for(float value:{.59f,3.01f}){bad=baseline;bad.height=value;reject(bad);}
    bad=baseline;bad.yaw=4;reject(bad);
    std::array<BoxCollider3D,1> wall{{{{0,1,0},{.2f,1,.2f}}}};reject(baseline,wall);
    wall[0].extents.x=-1;reject(baseline,wall);
    std::array<const ConvexAabbSweep3D*,1> nullHull{nullptr};reject(baseline,{},nullHull);
    auto emptyHull=std::make_unique<ConvexAabbSweep3D>();nullHull[0]=emptyHull.get();reject(baseline,{},nullHull);
    // 非接地狀態即使距地面只有 0.1mm，也不能被恢復程序吸附。
    auto airborne=baseline;airborne.feet.y=.0001f;airborne.velocity={2,-1,0};airborne.grounded=false;airborne.height=1.4f;
    Need(actor.TryRestoreMotionState(airborne,{}, {},terrain)&&Same(actor.CaptureMotionState(),airborne),"near-ground descending restore without snap");
    Need(!actor.CanCommitDirectDisplacementPreview(token)&&Same(actor.Stats(),stats),"successful restore invalidates token without events");
    auto apex=airborne;apex.feet.y=2;apex.velocity.y=0;
    Need(actor.TryRestoreMotionState(apex,{}, {},terrain)&&Same(actor.CaptureMotionState(),apex),"zero vertical apex remains airborne");
    std::array<BoxCollider3D,1> platform{{{{0,.5f,0},{2,.5f,2}}}};
    auto standing=baseline;standing.feet.y=1;
    Need(actor.TryRestoreMotionState(standing,platform,{},terrain)&&Same(actor.CaptureMotionState(),standing),"exact box support");
    standing.feet.x=2.45f+.01f;
    Need(!actor.TryRestoreMotionState(standing,platform,{},terrain),"no support outside platform footprint");
    std::array<BoxCollider3D,1> ceiling{{{{0,1.7f,0},{2,.1f,2}}}};
    auto crouch=baseline;crouch.height=1.4f;
    Need(actor.TryRestoreMotionState(crouch,ceiling,{},terrain),"restore saved crouch under ceiling");
    Need(!actor.TryRestoreMotionState(baseline,ceiling,{},terrain)&&Same(actor.CaptureMotionState(),crouch),"standing volume rejected without changing crouch");
    auto settings=actor.Settings();settings.gravity=std::numeric_limits<float>::quiet_NaN();CharacterController3D invalid(settings);
    const auto before=invalid.StateHash();Need(!invalid.TryRestoreMotionState(baseline,{}, {},terrain)&&invalid.StateHash()==before,"invalid settings atomic reject");
    // 窄峰位於腳中心外，但穿過身體頂部，不能靠中心取樣漏掉。
    HeightField3D spike({{-1,0,-1},21,21,.1f});spike.Fill(0);Need(spike.SetHeight(13,10,3),"spike fixture");
    Need(!actor.TryRestoreMotionState(baseline,{}, {},spike),"terrain footprint spike rejected");
}
}
int main()try{ExactAndFuture();ValidationAndTokens();std::cout<<"CHARACTER_MOTION_RESTORE_PASS checks="<<checks<<'\n';return 0;}
catch(const std::exception& e){std::cerr<<"CHARACTER_MOTION_RESTORE_FAIL "<<e.what()<<'\n';return 1;}
