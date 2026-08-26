#include <Aevrynd/CharacterController3D.h>
#include <cmath>
#include <iostream>
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
    std::cout<<"steps="<<controller.Stats().steps<<" collisions="<<controller.Stats().collisions<<" landings="<<controller.Stats().landings<<" cameraDistance="<<Length(camera.position-camera.target)<<'\n';return 0;
}
