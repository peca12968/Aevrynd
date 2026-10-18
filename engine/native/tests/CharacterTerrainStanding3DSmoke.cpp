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
constexpr float Dt=1.f/120;
void Need(bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(CharacterMotionState3D a,CharacterMotionState3D b){return Same(a.feet,b.feet)&&Same(a.velocity,b.velocity)&&a.height==b.height&&a.yaw==b.yaw&&a.grounded==b.grounded;}
CharacterControllerSettings3D Policy(float grade=1.f){CharacterControllerSettings3D s;s.maximumTerrainStandingGrade=grade;return s;}
HeightField3D Plane(float gx,float gz=0,bool flatBelow=false){
    HeightField3D field({{-32,0,-32},65,65,1});
    for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x){
        const float height=gx*(float(x)-32)+gz*(float(z)-32);
        Need(field.SetHeight(x,z,flatBelow?(std::max)(height,-4.f):height),"fixture height");
    }
    field.RecalculateRange();return field;
}
std::unique_ptr<ConvexAabbSweep3D> Hull(float y){
    auto hull=std::make_unique<ConvexAabbSweep3D>();std::array<Vec3,8> points{};unsigned i=0;
    for(float x:{-3.f,3.f})for(float z:{-3.f,3.f})for(float d:{-.5f,0.f})points[i++]={x,y+d,z};
    Need(hull->Prepare(points),"prepare convex support");return hull;
}
void Slide(){
    for(float uphill:{0.f,1.f})for(const auto g:{Vec2{2,0},Vec2{0,-2},Vec2{1.25f,1.5f}}){
        const auto terrain=Plane(g.x,g.y);auto settings=Policy();settings.maximumTerrainUphillGrade=uphill;
        CharacterController3D actor(settings);actor.Reset({});
        float previousDown=0;const float grade=std::hypot(g.x,g.y);
        const Vec3 down{-g.x/(grade*std::sqrt(1+grade*grade)),-grade/std::sqrt(1+grade*grade),-g.y/(grade*std::sqrt(1+grade*grade))};
        for(unsigned tick=0;tick<120;++tick){
            Need(actor.Step({}, {},{},terrain,Dt),"steep no input tick accepted");
            const auto p=actor.Position(),v=actor.Velocity();
            Need(!actor.Grounded()&&p.x*g.x+p.z*g.y<0&&v.y<0,"steep contact slides downhill and is not grounded");
            Need(p.y>=terrain.SampleHeight(p.x,p.z)-.0001f,"slide never penetrates terrain");
            const float speed=Dot(v,down);
            Need(speed>=previousDown-.001f&&speed<=settings.maximumFallSpeed+.001f,"tangential gravity accelerates within fall speed limit");previousDown=speed;
            Need(actor.Stats().jumps==0&&actor.Stats().landings==0&&actor.Stats().steps==tick+1,"slide not fabricated jump landing or rollback");
            if(tick==0||tick==60){
                CharacterController3D restored(settings);const auto snap=actor.CaptureMotionState();
                Need(restored.TryRestoreMotionState(snap,{}, {},terrain)&&Same(restored.CaptureMotionState(),snap),"actual sliding state restores exactly");
                auto continuing=actor;
                for(unsigned n=0;n<20;++n){Need(restored.Step({}, {},{},terrain,Dt)&&continuing.Step({}, {},{},terrain,Dt),"restored continuation accepted");
                    Need(Same(restored.CaptureMotionState(),continuing.CaptureMotionState()),"sliding replay deterministic after restore");}
            }
        }
        std::cout<<"SLIDE gradient="<<g.x<<','<<g.y<<" uphill_limit="<<uphill<<" feet="<<actor.Position().x<<','<<actor.Position().y<<','<<actor.Position().z<<'\n';
    }
}
void Controls(){
    const auto steep=Plane(2);const auto gentle=Plane(.5f);
    CharacterController3D actor(Policy());actor.Reset({});
    for(unsigned n=0;n<120;++n)Need(actor.Step({}, {},{},gentle,Dt)&&actor.Grounded()&&Same(actor.Position(),{}),"gentle slope remains stationary supported");
    actor.Reset({});const auto hash=actor.StateHash();
    Need(actor.Step({{1,0,0},true}, {},{},steep,0)&&actor.StateHash()==hash,"zero dt adds no movement gravity jump or events");
    Need(actor.Step({{1,0,0},true}, {},{},steep,Dt)&&!actor.Grounded()&&actor.Velocity().y<0&&actor.Stats().jumps==0,"steep initial contact cannot jump uphill");
    actor.Reset({});
    for(unsigned n=0;n<60;++n)Need(actor.Step({{0,0,1},false},{},{},steep,Dt),"contour input tick");
    Need(actor.Position().x<-.1f&&actor.Position().z>2.f,"contour control does not cancel downhill gravity");
    const std::array<BoxCollider3D,1> bridge{{{{0,5,0},{3,.25f,3}}}};
    actor.Reset({0,5.25f,0});
    Need(actor.TryRestoreMotionState({{0,5.25f,0},{},1.8f,0,true},bridge,{},steep),"bridge restore is actual solid support");
    for(unsigned n=0;n<120;++n)Need(actor.Step({},bridge,{},steep,Dt)&&actor.Grounded()&&std::abs(actor.Position().y-5.25f)<.001f&&actor.Position().x==0,"box bridge is not dragged by steep terrain underneath");
    auto roof=Hull(6);const std::array<const ConvexAabbSweep3D*,1> roofs{roof.get()};actor.Reset({0,6,0});
    Need(actor.TryRestoreMotionState({{0,6,0},{},1.8f,0,true},{},roofs,steep),"convex support restore");
    for(unsigned n=0;n<120;++n)Need(actor.Step({}, {},roofs,steep,Dt)&&actor.Grounded()&&std::abs(actor.Position().y-6)<.001f&&actor.Position().x==0,"convex roof excludes underlying steep ground");
    actor.Reset({0,3,0});Need(actor.TryRestoreMotionState({{0,3,0},{0,1,0},1.8f,0,false},{},{},steep),"airborne fixture restore");
    Need(actor.Step({{0,0,1},false},{},{},steep,Dt)&&actor.Velocity().x==0&&actor.Position().z>0,"air above steep ground not forced sideways");
    CharacterController3D ordinary(Policy()),queried(Policy());ordinary.Reset({});queried.Reset({});CollisionWorld3D world;
    ordinary.Step({},std::span<const BoxCollider3D>{},steep,Dt);queried.Step({},world,steep,Dt);
    Need(ordinary.Position().x<0&&!ordinary.Grounded()&&Same(ordinary.CaptureMotionState(),queried.CaptureMotionState()),"ordinary and world overload activate standing policy without uphill policy");
}
void TransitionAndWall(){
    const auto terrain=Plane(2,0,true);CharacterController3D actor(Policy());actor.Reset({});bool slid=false,landed=false;
    for(unsigned n=0;n<360;++n){Need(actor.Step({}, {},{},terrain,Dt),"steep to flat accepted");
        slid=slid||(!actor.Grounded()&&actor.Position().x<0);
        if(slid&&actor.Grounded()){landed=true;break;}}
    Need(slid&&landed&&actor.Position().x<=-2&&std::abs(actor.Position().y+4)<.001f,"slide reaches actual flat support and lands");
    for(unsigned n=0;n<30;++n)Need(actor.Step({}, {},{},terrain,Dt)&&actor.Grounded(),"flat after slide remains supported");
    Need(Length(actor.Velocity())<.001f,"no continued unwanted slide on flat ground");
    const auto steep=Plane(2);const std::array<BoxCollider3D,1> wall{{{{-1.5f,-1,0},{.1f,20,4}}}};actor.Reset({});
    for(unsigned n=0;n<240;++n){Need(actor.Step({},wall,{},steep,Dt),"wall slide tick accepted");Need(actor.Position().x>=-1.5f+.1f+actor.Settings().radius-.001f,"downhill sweep cannot pass wall");}
    Need(actor.Stats().collisions>0,"wall actually contacted");
    Need(std::abs(actor.Velocity().x)<.001f&&std::abs(actor.Velocity().y)<.001f,"wall constraints survive terrain projection without phantom inward velocity");
    for(unsigned n=0;n<30;++n){const auto before=actor.Position();Need(actor.Step({},wall,{},steep,Dt),"continued wall hold accepted");
        Need(Length(actor.Position()-before)<.0001f&&std::abs(actor.Velocity().x)<.001f,"wall rest has neither creep nor inward reported velocity");}
    const float beforeZ=actor.Position().z;
    for(unsigned n=0;n<30;++n)Need(actor.Step({{0,0,1},false},wall,{},steep,Dt),"wall contour motion accepted");
    Need(actor.Position().z>beforeZ+1.f&&std::abs(actor.Velocity().x)<.001f,"wall policy preserves free contour motion instead of freezing all input");
}
void RemovedSupportAndZeroTime(){
    const auto terrain=Plane(2);const std::array<BoxCollider3D,1> bridge{{{{0,5,0},{3,.25f,3}}}};
    CharacterController3D actor(Policy());actor.Reset({0,5.25f,0});
    Need(actor.Step({},bridge,{},terrain,Dt)&&actor.Grounded(),"removed support initially stands on bridge");
    auto launched=actor;const auto h=launched.StateHash();
    Need(!launched.TryLaunchFromGround(1.2f,{},{},terrain)&&launched.StateHash()==h,"launch after actual support removed rejects atomically");
    const auto jumps=actor.Stats().jumps;
    Need(actor.Step({{},true},{},{},terrain,Dt)&&actor.Stats().jumps==jumps&&actor.Velocity().y<0,"ordinary jump also requires present support rather than old grounded");
    const auto flat=Plane(0);CollisionWorld3D world;
    Need(bool(world.CreateBox({{{0,-.5f,0},{3,.5f,3}}})),"zero time world collider fixture");
    CharacterController3D zero(Policy());zero.Reset({});const auto before=zero.StateHash();const auto memory=zero.MemoryStats();
    zero.Step({},world,flat,0);
    Need(zero.StateHash()==before&&zero.MemoryStats().retainedCapacityBytes==memory.retainedCapacityBytes,"world zero time preserves scratch and state hash as well as motion");
}
void BridgeTerrainContact(){
    // 橋面與地形幾乎同高：保留真實支撐，不把浮點殘差當成穿地。
    for(float ground:{-.00005f,0.f,.00005f})for(bool convex:{false,true})for(float residual:{0.f,1.19539e-6f,.00001f}){
        HeightField3D terrain({{-8,0,24},17,33,1});
        for(unsigned z=0;z<33;++z)for(unsigned x=0;x<17;++x)Need(terrain.SetHeight(x,z,ground),"bridge terrain fixture");
        terrain.RecalculateRange();
        const std::array<BoxCollider3D,1> bridge{{{{0,-.2f,36},{2,.2f,4}}}};
        ConvexAabbSweep3D hull;std::array<Vec3,8> points{};unsigned i=0;
        for(float x:{-2.f,2.f})for(float y:{-.4f,0.f})for(float z:{32.f,40.f})points[i++]={x,y,z};
        Need(hull.Prepare(points),"bridge convex fixture");
        const std::array<const ConvexAabbSweep3D*,1> hs{&hull};
        const auto boxes=convex?std::span<const BoxCollider3D>{}:std::span<const BoxCollider3D>{bridge};
        const auto hulls=convex?std::span<const ConvexAabbSweep3D* const>{hs}:std::span<const ConvexAabbSweep3D* const>{};
        auto settings=Policy();settings.maximumTerrainUphillGrade=1;
        CharacterController3D actor(settings);const float startY=(std::max)(ground,residual);
        Need(actor.TryRestoreMotionState({{-.148892f,startY,39.9752f},{0,0,6},1.8f,0,true},boxes,hulls,terrain),"bridge same-height restore");
        const auto zero=actor.StateHash();Need(actor.Step({},boxes,hulls,terrain,0)&&actor.StateHash()==zero,"bridge zero dt does not snap");
        auto restored=actor;
        for(unsigned tick=0;tick<60;++tick){
            Need(actor.Step({{0,0,1},false},boxes,hulls,terrain,Dt),"bridge exit finite step accepted");
            Need(actor.Position().y>=ground&&actor.Grounded(),"bridge exit never penetrates ground or loses valid support");
            Need(restored.Step({{0,0,1},false},boxes,hulls,terrain,Dt)&&Same(restored.CaptureMotionState(),actor.CaptureMotionState()),"bridge continuation deterministic");
            if(tick==0)Need(restored.TryRestoreMotionState(actor.CaptureMotionState(),boxes,hulls,terrain),"bridge real first step restores");
        }
        Need(actor.Position().z>41.f&&actor.Position().y==ground,"bridge actually exits to terrain");
        auto below=actor.CaptureMotionState();below.feet.y=ground-.001f;const auto saved=actor.StateHash();
        Need(!actor.TryRestoreMotionState(below,boxes,hulls,terrain)&&actor.StateHash()==saved,"bridge does not authorize underground restore");
    }
    const auto flat=Plane(0);CharacterController3D noBridge(Policy());noBridge.Reset({0,1.19539e-6f,0});
    Need(noBridge.Step({}, {},{},flat,Dt)&&noBridge.Grounded()&&noBridge.Position().y==0,"no solid still settles to actual terrain");
    const std::array<BoxCollider3D,1> support{{{{0,-.2f,0},{2,.2f,2}}}};
    CharacterController3D jumping(Policy());jumping.Reset({0,1.19539e-6f,0});
    Need(jumping.TryRestoreMotionState({{0,1.19539e-6f,0},{},1.8f,0,true},support,{},flat),"jump fixture has verified initial support");
    Need(jumping.Step({{},true},support,{},flat,Dt)&&!jumping.Grounded()&&jumping.Velocity().y>0&&jumping.Position().y>.01f,"solid jump not pulled down to terrain");
    auto higher=Plane(0);for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x)Need(higher.SetHeight(x,z,.00005f),"higher terrain fixture");higher.RecalculateRange();
    CharacterController3D buried(Policy());buried.Reset({});
    Need(buried.Step({},support,{},higher,Dt)&&buried.Position().y>=.00005f,"solid support cannot suppress actual terrain penetration correction");
    const auto hash=buried.StateHash();const std::array<const ConvexAabbSweep3D*,1> bad{nullptr};
    Need(!buried.Step({},support,bad,higher,Dt)&&buried.StateHash()==hash,"invalid hull rejects atomically even with support");
    auto lower=Plane(0);for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x)Need(lower.SetHeight(x,z,-2.f),"lower terrain fixture");lower.RecalculateRange();
    CharacterController3D removed(Policy());removed.Reset({0,1.19539e-6f,0});
    Need(removed.Step({},support,{},lower,Dt)&&removed.Grounded(),"low terrain still stands on solid");
    const auto jumps=removed.Stats().jumps;
    Need(removed.Step({{},true},{},{},lower,Dt)&&!removed.Grounded()&&removed.Velocity().y<0&&removed.Stats().jumps==jumps,"removed bridge falls without fake jump or distant snap");
}
void DirectAndInvalid(){
    const auto terrain=Plane(2);CharacterController3D actor(Policy());actor.Reset({});
    Need(actor.TryRestoreMotionState({{}, {},1.8f,0,false},{},{},terrain),"nongrounded contact accepted");const auto initial=actor.StateHash();
    Need(!actor.TryRestoreMotionState({{}, {},1.8f,0,true},{},{},terrain)&&actor.StateHash()==initial,"false steep grounded save rejected transactionally");
    CharacterDirectDisplacementResult3D result;CharacterDirectDisplacementPreviewToken3D token;
    Need(actor.PreviewDirectDisplacementTick({1,{0,0,.1f},0},{},terrain,token,&result),"direct contour preview");
    Need(!result.grounded&&actor.StateHash()==initial&&actor.CanCommitDirectDisplacementPreview(token),"direct steep preview remains nongrounded and uncommitted");
    Need(actor.CancelDirectDisplacementPreview(token),"cancel preview");
    Need(actor.ApplyDirectDisplacementTick({1,{0,0,.1f},0},{},terrain,&result),"direct contour accepted");
    Need(Same(result.requestedWorld,result.consumedWorld+result.remainderWorld)&&!actor.Grounded()&&Same(actor.Velocity(),{})&&actor.Position().x==0,"direct no dt adds neither gravity nor downhill translation");
    for(float bad:{-1.f,17.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        CharacterController3D invalid(Policy(bad));invalid.Reset({});const auto hash=invalid.StateHash();
        Need(!invalid.Step({}, {},{},terrain,Dt)&&invalid.StateHash()==hash,"invalid standing grade bool step atomic");
        invalid.Step({},std::span<const BoxCollider3D>{},terrain,Dt);Need(invalid.StateHash()==hash,"invalid standing grade void step atomic");
        result.tick=91;Need(!invalid.ApplyDirectDisplacementTick({1,{},0},{},terrain,&result)&&result.tick==91&&invalid.StateHash()==hash,"invalid direct leaves output and controller untouched");
        Need(!invalid.TryRestoreMotionState({{}, {},1.8f,0,false},{},{},terrain)&&invalid.StateHash()==hash,"invalid grade restore atomic");
    }
    CharacterController3D a,b(Policy(0));a.Reset({});b.Reset({});Need(a.StateHash()==b.StateHash(),"zero standing policy preserves initial identity");
    for(unsigned n=0;n<120;++n){a.Step({{.1f,0,.1f},false},std::span<const BoxCollider3D>{},terrain,Dt);b.Step({{.1f,0,.1f},false},std::span<const BoxCollider3D>{},terrain,Dt);Need(a.StateHash()==b.StateHash(),"disabled setting matches legacy each tick");}
}
}
int main(){try{Slide();Controls();TransitionAndWall();RemovedSupportAndZeroTime();BridgeTerrainContact();DirectAndInvalid();std::cout<<"CHARACTER_TERRAIN_STANDING_PASS checks="<<checks<<" native_gameplay=0\n";return 0;}
catch(const std::exception& e){std::cerr<<"CHARACTER_TERRAIN_STANDING_FAIL checks="<<checks<<" reason="<<e.what()<<'\n';return 1;}}
