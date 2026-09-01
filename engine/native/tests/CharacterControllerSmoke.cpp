#include <Aevrynd/CharacterController3D.h>
#include <cmath>
#include <array>
#include <iostream>
#include <limits>
#include <vector>

using namespace Aevrynd;
bool Near(float left,float right,float tolerance=.02f){return std::abs(left-right)<=tolerance;}

int main(){
    const std::vector<BoxCollider3D> colliders={{{0,-.5f,0},{20,.5f,20}},{{0,1,5},{2,1,1}}};CharacterController3D controller;controller.Reset({0,0,0});
    for(int frame=0;frame<120;++frame)controller.Step({},colliders,1.f/60.f);if(!controller.Grounded()||!Near(controller.Position().y,0)){std::cerr<<"Ground settling failed\n";return 1;}
    for(int frame=0;frame<180;++frame)controller.Step({{0,0,1},false},colliders,1.f/60.f);if(controller.Position().z>3.56f||controller.Stats().collisions==0){std::cerr<<"Wall collision failed\n";return 2;}
    controller.Reset({-5,0,0});float peak{};for(int frame=0;frame<180;++frame){controller.Step({{},frame==2},colliders,1.f/60.f);peak=(std::max)(peak,controller.Position().y);}if(peak<1.f||!controller.Grounded()||controller.Stats().jumps!=1||controller.Stats().landings<2){std::cerr<<"Jump and landing failed\n";return 3;}
    controller.Reset({5,8,0});for(int frame=0;frame<240;++frame)controller.Step({},colliders,1.f/60.f);if(!controller.Grounded()||!Near(controller.Position().y,0)||controller.Velocity().y!=0){std::cerr<<"Gravity landing failed\n";return 4;}
    Camera3D camera=MakeThirdPersonCamera({0,1,0},0,0,8);const std::vector<BoxCollider3D> cameraWall={{{0,1,-4},{2,2,.5f}}};camera=ConstrainThirdPersonCamera(camera,{0,1,0},cameraWall,.2f);if(camera.position.z<-3.31f||camera.position.z>-3.1f){std::cerr<<"Third-person camera collision failed\n";return 5;}
    HeightField3D slope({{-8,0,-2},17,5,1});for(unsigned z=0;z<5;++z)for(unsigned x=0;x<17;++x)slope.SetHeight(x,z,float(x)*.1f);controller.Reset({-7.5f,.05f,0});const std::vector<BoxCollider3D> noColliders;for(int frame=0;frame<120;++frame)controller.Step({{1,0,0},false},noColliders,slope,1.f/60.f);if(!controller.Grounded()||!Near(controller.Position().y,slope.SampleHeight(controller.Position().x,controller.Position().z))||controller.Stats().terrainContacts<100){std::cerr<<"Height-field grounding failed\n";return 6;}
    float terrainPeak=controller.Position().y;for(int frame=0;frame<180;++frame){controller.Step({{},frame==1},noColliders,slope,1.f/60.f);terrainPeak=(std::max)(terrainPeak,controller.Position().y);}if(!controller.Grounded()||terrainPeak-slope.SampleHeight(controller.Position().x,controller.Position().z)<1.f){std::cerr<<"Height-field jumping failed\n";return 7;}
    HeightField3D cameraTerrain({{-8,0,-8},17,17,1});cameraTerrain.FlattenCircle({0,0,-4},2,5);Camera3D terrainCamera=MakeThirdPersonCamera({0,1,0},0,0,8);terrainCamera=ConstrainThirdPersonCamera(terrainCamera,{0,1,0},cameraTerrain,.2f,64);if(terrainCamera.position.z<-2.f||terrainCamera.position.y<cameraTerrain.SampleHeight(terrainCamera.position.x,terrainCamera.position.z)){std::cerr<<"Height-field camera collision failed\n";return 8;}
    const auto travel=[](unsigned rate,float multiplier){CharacterController3D value;value.Reset({0,0,0});const std::vector<BoxCollider3D> floor={{{0,-.5f,0},{100,.5f,100}}};for(unsigned tick=0;tick<rate;++tick)value.Step({{1,0,0},false,multiplier},floor,1.f/float(rate));return value.Position().x;};
    const std::array walk{travel(30,4.f/6.f),travel(60,4.f/6.f),travel(144,4.f/6.f)},run{travel(30,7.f/6.f),travel(60,7.f/6.f),travel(144,7.f/6.f)},crouch{travel(30,2.f/6.f),travel(60,2.f/6.f),travel(144,2.f/6.f)};
    for(std::size_t index=0;index<walk.size();++index)if(!Near(walk[index],4,.03f)||!Near(run[index],7,.03f)||!Near(crouch[index],2,.03f)){std::cerr<<"Walk/run/crouch speed policy failed\n";return 9;}
    if(!Near(walk[0],walk[1],.03f)||!Near(walk[1],walk[2],.03f)||!Near(run[0],run[1],.03f)||!Near(run[1],run[2],.03f)){std::cerr<<"30/60/144 controller displacement changed\n";return 10;}
    if(!Near(travel(60,0),6,.03f)||!Near(travel(60,-2),6,.03f)||!Near(travel(60,(std::numeric_limits<float>::quiet_NaN)()),6,.03f)||!Near(travel(60,(std::numeric_limits<float>::infinity)()),6,.03f)||!Near(travel(60,-(std::numeric_limits<float>::infinity)()),6,.03f)||!Near(travel(60,.01f),1.5f,.03f)||!Near(travel(60,99.f),24.f,.03f)){std::cerr<<"Invalid speed multiplier fallback/clamp changed\n";return 11;}
    CharacterController3D runner;runner.Reset({0,0,0});for(int frame=0;frame<120;++frame)runner.Step({{0,0,1},false,2},colliders,1.f/60.f);if(runner.Position().z>3.56f||runner.Stats().collisions==0){std::cerr<<"Run multiplier bypassed collision\n";return 12;}
    const std::vector<BoxCollider3D> thinWall={{{0,-.5f,0},{20,.5f,20}},{{0,1,1},{2,1,.05f}}};CharacterController3D maximumSpeed;maximumSpeed.Reset({0,0,0});maximumSpeed.Step({{0,0,1},false,4},thinWall,.1f);if(maximumSpeed.Position().z>.501f||maximumSpeed.Stats().collisions==0){std::cerr<<"4x dt-limit thin-wall tunneling\n";return 13;}
    std::cout<<"steps="<<controller.Stats().steps<<" collisions="<<controller.Stats().collisions<<" landings="<<controller.Stats().landings<<" terrain="<<controller.Stats().terrainContacts<<" cameraDistance="<<Length(camera.position-camera.target)<<'\n';return 0;
}
