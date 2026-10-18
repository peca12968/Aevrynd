#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
int main()try{
    HeightField3D terrain({{-10,0,-10},64,64,1});terrain.Fill(0);
    std::array<Vec3,8> vertices{};size_t n=0;
    for(float x:{-10.f,40.f})for(float y:{-1.f,0.f})for(float z:{-10.f,40.f})vertices[n++]={x,y,z};
    ConvexAabbSweep3D floor;Need(floor.Prepare(vertices),"floor convex");
    std::array<const ConvexAabbSweep3D*,1> hull{&floor};
    for(bool convex:{false,true})for(unsigned hz:{60u,120u,240u}){
        const std::span<const ConvexAabbSweep3D* const> shapes=convex?std::span<const ConvexAabbSweep3D* const>(hull):std::span<const ConvexAabbSweep3D* const>{};
        CharacterController3D actor;actor.Reset({0,.01f,0});const float dt=1.f/float(hz);
        for(unsigned i=0;i<30;++i)Need(actor.Step({}, {},shapes,terrain,dt),"settle");
        Need(actor.Grounded(),"launch begins grounded");
        const auto before=actor.StateHash();
        for(float bad:{0.f,-1.f,8.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
            Need(!actor.TryLaunchFromGround(bad)&&actor.StateHash()==before,"invalid launch atomic");
        const auto origin=actor.Position();Need(actor.TryLaunchFromGround(1.2f),"running takeoff accepted");
        Need(actor.Position().x==origin.x&&actor.Position().y==origin.y&&actor.Position().z==origin.z&&!actor.Grounded(),"launch never teleports");
        const auto airborne=actor.StateHash();Need(!actor.TryLaunchFromGround(1.2f)&&actor.StateHash()==airborne,"no repeated airborne boost");
        float peak=0;unsigned airborneTicks=0;
        for(unsigned i=0;i<hz;++i){
            Need(actor.Step({{0,0,1},false}, {},shapes,terrain,dt),"flight collision step");
            peak=(std::max)(peak,actor.Position().y-origin.y);
            Need(actor.Position().y>=-.0001f,"no floor penetration");
            if(!actor.Grounded())++airborneTicks;else break;
        }
        Need(actor.Grounded()&&peak>.015f&&peak<.04f&&airborneTicks>2,"short physical flight and landing");
        Need(std::abs(actor.Position().y-origin.y)<.001f,"same support return");
        std::cout<<"GROUND_LAUNCH convex="<<convex<<" hz="<<hz<<" peak="<<peak<<" airborne_ticks="<<airborneTicks<<'\n';
        // 完整跳躍仍用原 jump 輸入，不能被短騰空改成小跳。
        Need(actor.Step({{},true}, {},shapes,terrain,dt),"normal jump");
        Need(actor.Velocity().y>6,"original jump speed retained");
    }
    // 低頂棚先阻擋向上運動，再自然回落；不要求觸頂當下就是接地。
    CharacterController3D capped;capped.Reset({0,0,0});
    Need(capped.Step({}, {},hull,terrain,1.f/120),"capped initial support");
    CharacterDirectDisplacementPreviewToken3D pending;
    Need(capped.PreviewDirectDisplacementTick({1,{},0},{},terrain,pending),"pending direct preview");
    Need(capped.CanCommitDirectDisplacementPreview(pending),"valid direct preview before launch");
    Need(!capped.TryLaunchFromGround(0)&&capped.CanCommitDirectDisplacementPreview(pending),"failed launch preserves preview generation");
    std::array<BoxCollider3D,1> ceiling{{{{0,1.91f,0},{2,.1f,2}}}};
    Need(capped.TryLaunchFromGround(1.2f),"ceiling takeoff");
    Need(!capped.CanCommitDirectDisplacementPreview(pending),"successful takeoff invalidates stale direct preview");
    for(unsigned i=0;i<40;++i){Need(capped.Step({},ceiling,hull,terrain,1.f/120),"ceiling step");Need(capped.Position().y+capped.Settings().height<=1.8102f,"ceiling cannot be crossed");}
    Need(capped.Grounded(),"ceiling return grounded");
    std::cout<<"GROUND_LAUNCH_PASS checks="<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"GROUND_LAUNCH_FAIL "<<e.what()<<'\n';return 1;}
