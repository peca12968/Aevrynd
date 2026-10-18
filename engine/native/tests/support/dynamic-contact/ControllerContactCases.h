#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace Aevrynd;
void Need(bool v,const char* reason){if(!v)throw std::runtime_error(reason);}
std::array<std::uint64_t,18> Snapshot(const CharacterController3D& c){
    const auto m=c.CaptureMotionState();const auto s=c.Stats();
    const auto f=[](float v){return std::uint64_t(std::bit_cast<std::uint32_t>(v));};
    return {f(m.feet.x),f(m.feet.y),f(m.feet.z),f(m.velocity.x),f(m.velocity.y),f(m.velocity.z),
        f(m.height),f(m.yaw),m.grounded,s.steps,s.collisions,s.jumps,s.landings,s.terrainContacts,
        s.directTicks,s.directBlocked,s.directOverlaps,c.StateHash()};
}
HeightField3D Terrain(float grade=0){
    HeightField3D terrain({{-16,0,-16},65,65,.5f});
    for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x)terrain.SetHeight(x,z,terrain.SamplePosition(x,z).x*grade);
    terrain.RecalculateRange();return terrain;
}
unsigned Parity(){
    std::ofstream out("parity.tsv",std::ios::binary);Need(bool(out),"PARITY_OUTPUT_FAILED");unsigned count{};
    for(unsigned recipe=0;recipe<24;++recipe){
        auto terrain=Terrain(recipe%3==2?.15f:0.f);CharacterControllerSettings3D settings;
        settings.radius=recipe%2?.45f:.5f;settings.height=recipe%4?1.8f:1.f;
        settings.moveSpeed=recipe%5?6.f:3.f;
        if(recipe%3==2){settings.maximumTerrainUphillGrade=.4f;settings.maximumTerrainStandingGrade=.5f;}
        CharacterController3D c(settings);c.Reset({-3,terrain.SampleHeight(-3,-2),-2});
        const std::array<BoxCollider3D,3> boxes{{{{0,1,0},{.1f,1,3}},{{-2,-.5f,1},{2,.5f,1}},{{-1,.05f,-1},{.4f,.05f,.4f}}}};
        for(unsigned tick=0;tick<64;++tick){
            CharacterControl3D input;input.movement={tick<32?1.f:-.6f,0,tick%9<4?.6f:-.3f};
            input.jump=tick==16||tick==48;input.speedMultiplier=tick%13==0?2.f:1.f;
            const float dt=tick%19==0?0.f:1.f/60.f;bool ok{};
#if CANDIDATE
            const auto status=recipe%2?c.Step(input,boxes,{},terrain,std::span<const DynamicCylinder3D>{},dt):
                c.Step(input,boxes,{},std::span<const DynamicCylinder3D>{},dt);
            ok=status==CharacterDynamicStepStatus3D::Applied;
#else
            ok=recipe%2?c.Step(input,boxes,{},terrain,dt):c.Step(input,boxes,{},dt);
#endif
            out<<recipe<<'\t'<<tick<<'\t'<<ok;
            for(auto v:Snapshot(c))out<<'\t'<<v;
            const auto memory=c.MemoryStats();out<<'\t'<<c.DirectLastTick()<<'\t'<<memory.retainedCapacityBytes<<'\n';++count;
        }
    }
    Need(bool(out),"PARITY_WRITE_FAILED");return count;
}
#if CANDIDATE
using Status=CharacterDynamicStepStatus3D;
unsigned positives{},negatives{};
const std::span<const BoxCollider3D> NoBoxes{};
const std::span<const ConvexAabbSweep3D* const> NoHulls{};
void Ready(CharacterController3D& c,Vec3 feet,const HeightField3D& terrain){c.Reset(feet);c.Step({},NoBoxes,terrain,0);}
void Separated(const CharacterController3D& c,std::span<const DynamicCylinder3D> bodies){
    for(const auto& body:bodies){BoxCylinderContact3D q;Need(QueryBoxCylinderContact3D(c.Center(),c.Extents(),{},body,q)&&
        q.kind==BoxCylinderContactKind3D::Clear,"FINAL_DYNAMIC_OVERLAP_OR_UNRESOLVED");}
}
void ExpectFailure(CharacterController3D& c,CharacterControl3D control,const HeightField3D& terrain,
    std::span<const DynamicCylinder3D> bodies,Status wanted,const char* reason){
    CharacterDirectDisplacementPreviewToken3D token;
    Need(c.PreviewDirectDisplacementTick({c.DirectLastTick()+1,{.001f,0,0},0},NoBoxes,token),"PREVIEW_FIXTURE_FAILED");
    const auto before=Snapshot(c);const auto memory=c.MemoryStats();
    const auto actual=c.Step(control,NoBoxes,NoHulls,terrain,bodies,.1f);
    if(actual!=wanted)std::cerr<<"wrong status actual="<<unsigned(actual)<<" wanted="<<unsigned(wanted)<<" case="<<reason<<'\n';
    Need(actual==wanted,reason);Need(Snapshot(c)==before&&c.CanCommitDirectDisplacementPreview(token)&&
        c.MemoryStats().retainedCapacityBytes==memory.retainedCapacityBytes,"FAILURE_NOT_WHOLE_STATE_ATOMIC");++negatives;
}
void DynamicCases(){
    const auto terrain=Terrain();const std::array<DynamicCylinder3D,1> bodies{{{{0,1,0},1,1}}};
    {
        CharacterController3D c;Ready(c,{-1.65f,0,-.4f},terrain);
        Need(c.Step({{1,0,.3f}},NoBoxes,NoHulls,terrain,bodies,.1f)==Status::Applied,"SIDE_SLIDE_FAILED");
        Need(c.Position().x<=-1.45f&&c.Position().x>-1.46f&&c.Position().z>-.26f&&c.Grounded(),"SIDE_SLIDE_DID_NOT_PRESERVE_TANGENT");
        Separated(c,bodies);++positives;
    }
    {
        CharacterController3D c;Ready(c,{-3,0,0},terrain);
        Need(c.Step({{1,0,0},false,4},NoBoxes,NoHulls,terrain,bodies,.1f)==Status::Applied,"FAST_SIDE_BLOCK_FAILED");
        Need(c.Position().x<=-1.45f&&c.Position().x>-1.46f&&c.Velocity().x==0,"FAST_SIDE_TUNNELLED");
        Separated(c,bodies);++positives;
    }
    {
        CharacterController3D c;Ready(c,{-1.65f,0,-.4f},terrain);
        Need(c.Step({{-1,0,.2f}},NoBoxes,NoHulls,terrain,bodies,.1f)==Status::Applied&&c.Position().x<-2.2f,"MOVING_AWAY_BLOCKED");
        Separated(c,bodies);++positives;
    }
    {
        CharacterControllerSettings3D s;s.radius=.5f;s.height=2;s.moveSpeed=10;
        CharacterController3D c(s);Ready(c,{-1.5f,0,-1},terrain);
        const std::array<DynamicCylinder3D,1> small{{{{0,1,0},.5f,1}}};
        const std::array<BoxCollider3D,1> wall{{{{0,1,.5f},{10,1,.5f}}}};
        Need(c.Step({{1,0,1}},wall,NoHulls,terrain,small,.1f)==Status::Applied,"STATIC_DYNAMIC_CORNER_FAILED");
        Need(c.Position().x<=-1&&c.Position().z<=-.5f&&c.Position().x>-1.02f&&c.Position().z>-.52f,"STATIC_DYNAMIC_CORNER_ESCAPED");
        Separated(c,small);++positives;
    }
    {
        CharacterControllerSettings3D s;s.gravity=0;
        CharacterController3D c(s);c.Reset({-1.65f,.2f,-.4f});
        Need(c.Step({{1,0,.3f}},NoBoxes,NoHulls,bodies,.1f)==Status::Applied,"AIR_SIDE_SLIDE_FAILED");
        Need(!c.Grounded()&&c.Position().y==.2f&&c.Velocity().y==0&&c.Stats().landings==0,"DYNAMIC_BODY_GRANTED_SUPPORT");
        Separated(c,bodies);++positives;
    }
    {
        CharacterController3D c;c.Reset({0,2.1f,0});
        ExpectFailure(c,{},terrain,bodies,Status::UnresolvedVerticalEntry,"VERTICAL_ENTRY_NOT_EXPLICIT");
    }
    {
        CharacterController3D c;c.Reset({0,0,0});
        ExpectFailure(c,{},terrain,bodies,Status::UnresolvedStartedOverlap,"STARTED_OVERLAP_NOT_EXPLICIT");
    }
    {
        CharacterControllerSettings3D s;s.radius=.5f;s.height=1;s.moveSpeed=10;s.gravity=0;
        CharacterController3D c(s);c.Reset({-1.5f,1.5f,0});
        Need(c.TryRestoreMotionState({{-1.5f,1.5f,0},{0,-10,0},1,0,false},NoBoxes,NoHulls,terrain),"SIMULTANEOUS_RESTORE_FAILED");
        const std::array<DynamicCylinder3D,1> small{{{{0,.5f,0},.5f,.5f}}};
        ExpectFailure(c,{{1,0,0}},terrain,small,Status::UnresolvedSimultaneousEntry,"SIMULTANEOUS_ENTRY_NOT_EXPLICIT");
    }
    {
        const float nan=std::numeric_limits<float>::quiet_NaN();
        for(unsigned field=0;field<4;++field){
            auto invalid=bodies;if(field==0)invalid[0].center.x=nan;if(field==1)invalid[0].radius=0;
            if(field==2)invalid[0].halfHeight=-1;if(field==3)invalid[0].radius=nan;
            CharacterController3D c;c.Reset({-3,0,0});
            ExpectFailure(c,{{1,0,0}},terrain,invalid,Status::InvalidInput,"INVALID_DYNAMIC_BODY_ACCEPTED");
        }
    }
    {
        // 控制器尚未接受 bodyDelta；相對運動查詢與 static 快照能力分開標記。
        BoxCylinderContact3D q;const DynamicCylinder3D moving{{0,1,0},1,1};
        Need(QueryBoxCylinderContact3D({-3,.9f,0},{.45f,.9f,.45f},Vec3{}-Vec3{-2,0,0},moving,q)&&
            q.kind==BoxCylinderContactKind3D::SideEntry,"RELATIVE_MOVING_BODY_QUERY_NOT_BLOCKED");++positives;
        Need(QueryBoxCylinderContact3D({-3,.9f,0},{.45f,.9f,.45f},Vec3{}-Vec3{2,0,0},moving,q)&&
            q.kind==BoxCylinderContactKind3D::Clear,"RELATIVE_MOVING_BODY_AWAY_NOT_CLEAR");++positives;
    }
}
#endif
}
int main(){
    try{
        const auto parity=Parity();
#if CANDIDATE
        DynamicCases();
        std::cout<<"DYNAMIC_SIDE_CONTROLLER_CANDIDATE_CPU_PASS empty_span_snapshots="<<parity<<" side_query_positives="<<positives
            <<" atomic_unresolved_negatives="<<negatives<<" moving_body_controller=0 pair_coordinator=0 gameplay=0 GPU=0\n";
#else
        std::cout<<"ORIGINAL_F_CONTROLLER_PARITY_SNAPSHOTS snapshots="<<parity<<'\n';
#endif
        return 0;
    }catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
}
