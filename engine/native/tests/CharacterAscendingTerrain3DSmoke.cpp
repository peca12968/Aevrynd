#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace Aevrynd;
unsigned checks{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(CharacterMotionState3D a,CharacterMotionState3D b){
    return Same(a.feet,b.feet)&&Same(a.velocity,b.velocity)&&a.height==b.height&&a.yaw==b.yaw&&a.grounded==b.grounded;
}
bool Same(CharacterControllerStats3D a,CharacterControllerStats3D b){
    return a.steps==b.steps&&a.collisions==b.collisions&&a.jumps==b.jumps&&a.landings==b.landings&&
        a.terrainContacts==b.terrainContacts&&a.directTicks==b.directTicks&&a.directBlocked==b.directBlocked&&a.directOverlaps==b.directOverlaps;
}
HeightField3D Field(float grade){
    HeightField3D result;result.Configure({{-5,0,-5},41,81,.25f});
    for(unsigned z=0;z<81;++z)for(unsigned x=0;x<41;++x){const auto p=result.SamplePosition(x,z);result.SetHeight(x,z,grade*p.z);}
    result.RecalculateRange();return result;
}
CharacterControllerSettings3D Settings(){
    CharacterControllerSettings3D settings;settings.moveSpeed=7.4f;
    settings.maximumTerrainUphillGrade=settings.maximumTerrainStandingGrade=1;return settings;
}
CharacterController3D Launched(const HeightField3D& terrain){
    CharacterController3D player(Settings());player.Reset({0,0,0});
    Need(player.Step({}, {}, {},terrain,1.f/120.f)&&player.Grounded(),"initial actual terrain support");
    Need(player.TryLaunchFromGround(1.2f,{}, {},terrain),"finite supported run-flight launch");return player;
}
void AscendingUphill(bool dynamic){
    const auto terrain=Field(.2f);auto player=Launched(terrain);
    const std::array<DynamicCylinder3D,1> remote{{{{0,.5f,12},1.3f,.8f}}};
    bool landed{};
    for(unsigned tick=0;tick<24;++tick){
        if(dynamic){CharacterDynamicContactState3D contact;
            Need(player.Step({{0,0,1},false,1},{},{},terrain,remote,1.f/120.f,CharacterDynamicVerticalResponse3D::BlockOnly,&contact)==CharacterDynamicStepStatus3D::Applied,"dynamic uphill step applied");
        }else Need(player.Step({{0,0,1},false,1},{},{},terrain,1.f/120.f),"static uphill step applied");
        const auto feet=player.Position();const auto ground=terrain.SampleHeight(feet.x,feet.z);
        Need(feet.y>=ground,"ascending motion must not commit feet below actual terrain");
        if(player.Grounded()){landed=true;Need(feet.y==ground&&player.Velocity().y==0,"grounded terrain contact exact");}
    }
    Need(landed&&player.Position().z>1.f,"uphill run continues after true contact");
}
void FreeAscending(float grade){
    const auto terrain=Field(grade);auto player=Launched(terrain);
    Need(player.Step({{0,0,1},false,1},{},{},terrain,1.f/120.f),"free ascending step applied");
    const auto feet=player.Position();
    Need(feet.y>terrain.SampleHeight(feet.x,feet.z)&&player.Velocity().y>0&&!player.Grounded(),"above-ground ascent is not optional terrain snap");
}
void BlockedCorrection(unsigned kind){
    const auto terrain=Field(.2f);auto player=Launched(terrain);
    const auto before=player.CaptureMotionState();const auto stats=player.Stats();
    const std::array<BoxCollider3D,1> roof{{{{0,1.90525f,0},{1,.09475f,1}}}};
    std::array<Vec3,8> points{};unsigned index{};
    for(float x:{-1.f,1.f})for(float y:{1.8105f,2.f})for(float z:{-1.f,1.f})points[index++]={x,y,z};
    ConvexAabbSweep3D hull;Need(hull.Prepare(points),"actual convex roof preparation");
    const std::array<const ConvexAabbSweep3D*,1> hulls{{&hull}};
    const std::array<DynamicCylinder3D,1> above{{{{0,2.1105f,0},1.3f,.3f}}};
    bool accepted{};
    if(kind==0)accepted=player.Step({{0,0,1},false,1},roof,{},terrain,1.f/120.f);
    else if(kind==1)accepted=player.Step({{0,0,1},false,1},{},hulls,terrain,1.f/120.f);
    else{CharacterDynamicContactState3D contact;
        accepted=player.Step({{0,0,1},false,1},{},{},terrain,above,1.f/120.f,CharacterDynamicVerticalResponse3D::BlockOnly,&contact)==CharacterDynamicStepStatus3D::Applied;
    }
    Need(!accepted,"necessary terrain correction cannot pass through roof or dynamic body");
    Need(Same(before,player.CaptureMotionState())&&Same(stats,player.Stats()),"blocked correction is fully transactional");
}
}
int main()try{
    AscendingUphill(false);AscendingUphill(true);FreeAscending(0.f);FreeAscending(-.2f);
    for(unsigned kind=0;kind<3;++kind)BlockedCorrection(kind);
    std::cout<<"CHARACTER_ASCENDING_TERRAIN_CPU_PASS scenarios=7 checks="<<checks<<" GPU=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"CHARACTER_ASCENDING_TERRAIN_FIRST_DIVERGENCE="<<error.what()<<'\n';return 1;}
