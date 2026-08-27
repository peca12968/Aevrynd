#include <Aevrynd/ProceduralPlacement3D.h>
#include <chrono>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

namespace {
bool Height(float x,float z,float& y,void*){y=2.f+.2f*std::sin(x*.1f)+.15f*std::cos(z*.08f);return x<35.f;}
float ForestDensity(float x,float z,void*){const float clearing=x*x+z*z<100.f?0.f:1.f;const float broad=.5f+.5f*std::sin(x*.055f+.7f*std::cos(z*.037f));const float detail=.5f+.5f*std::sin(x*.19f-z*.13f);return clearing*(.14f+.72f*broad)*(.72f+.28f*detail);}
float InvalidDensity(float,float,void*){return 2.f;}
bool Equal(const ProceduralPlacementResult3D& a,const ProceduralPlacementResult3D& b){if(a.attempts!=b.attempts||a.exhausted!=b.exhausted||a.instances.size()!=b.instances.size())return false;for(size_t index=0;index<a.instances.size();++index){const auto& x=a.instances[index];const auto& y=b.instances[index];if(x.position.x!=y.position.x||x.position.y!=y.position.y||x.position.z!=y.position.z||x.scale!=y.scale||x.yaw!=y.yaw)return false;}return true;}
}

int main(){
    const PlacementExclusion3D exclusions[]={{{18,0,-12},4.f}};ProceduralPlacementSettings3D settings;settings.bounds={{-50,0,-50},{50,5,50}};settings.targetCount=1200;settings.minimumSpacing=1.45f;settings.seed=73;settings.exclusions=exclusions;settings.heightCallback=Height;settings.acceptanceCallback=ForestDensity;
    const auto start=std::chrono::steady_clock::now();const ProceduralPlacementResult3D first=GenerateProceduralPlacement3D(settings);const auto elapsed=std::chrono::steady_clock::now()-start;const ProceduralPlacementResult3D repeat=GenerateProceduralPlacement3D(settings);
    if(first.instances.size()!=settings.targetCount||first.exhausted||first.attempts>settings.targetCount*settings.attemptsPerInstance){std::cerr<<"Placement count or attempt budget failed\n";return 1;}if(!Equal(first,repeat)){std::cerr<<"Placement is not deterministic\n";return 2;}
    for(size_t index=0;index<first.instances.size();++index){const auto& a=first.instances[index];if(a.position.x>=35.f||a.position.y<0||a.position.y>5||a.scale<settings.minimumScale||a.scale>settings.maximumScale||a.yaw<0||a.yaw>=6.283186f){std::cerr<<"Placement attributes failed\n";return 3;}if(a.position.x*a.position.x+a.position.z*a.position.z<100.f){std::cerr<<"Placement clearing failed\n";return 10;}for(const auto& exclusion:exclusions){const float dx=a.position.x-exclusion.center.x,dz=a.position.z-exclusion.center.z;if(dx*dx+dz*dz<exclusion.radius*exclusion.radius){std::cerr<<"Placement exclusion failed\n";return 4;}}for(size_t other=0;other<index;++other){const auto& b=first.instances[other];const float dx=a.position.x-b.position.x,dz=a.position.z-b.position.z;if(dx*dx+dz*dz<settings.minimumSpacing*settings.minimumSpacing-.0001f){std::cerr<<"Placement spacing failed\n";return 5;}}}
    ProceduralPlacementSettings3D dense=settings;dense.targetCount=10000;dense.bounds={{0,0,0},{2,1,2}};dense.attemptsPerInstance=3;dense.exclusions={};dense.heightCallback=nullptr;const auto saturated=GenerateProceduralPlacement3D(dense);if(!saturated.exhausted||saturated.attempts>dense.targetCount*dense.attemptsPerInstance){std::cerr<<"Placement exhaustion bound failed\n";return 6;}
    ProceduralPlacementSettings3D density=settings;density.targetCount=0;density.densityPerSquareUnit=.04f;density.heightCallback=nullptr;density.acceptanceCallback=nullptr;density.exclusions={};if(GenerateProceduralPlacement3D(density).instances.size()!=400){std::cerr<<"Density placement failed\n";return 7;}
    ProceduralPlacementSettings3D invalid=settings;invalid.maximumGridCells=4;if(!GenerateProceduralPlacement3D(invalid).instances.empty()){std::cerr<<"Grid memory bound failed\n";return 8;}
    invalid=settings;invalid.acceptanceCallback=InvalidDensity;if(!GenerateProceduralPlacement3D(invalid).instances.empty()){std::cerr<<"Invalid acceptance weight failed\n";return 11;}
    if(elapsed>std::chrono::seconds(2)){std::cerr<<"Placement performance budget failed\n";return 9;}
    std::cout<<"instances="<<first.instances.size()<<" attempts="<<first.attempts<<" elapsedMicroseconds="<<std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()<<'\n';
}
