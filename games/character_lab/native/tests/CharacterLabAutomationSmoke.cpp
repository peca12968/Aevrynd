#include "CharacterLabRules.h"
#include <Aevrynd/CharacterController3D.h>
#include <cmath>
#include <iostream>
#include <vector>

using namespace Aevrynd;
int main(){
    const std::vector<BoxCollider3D> colliders={{{0,-.5f,0},{14,.5f,14}},{{-13.5f,2,0},{.5f,2,14}},{{13.5f,2,0},{.5f,2,14}},{{0,2,-13.5f},{14,2,.5f}},{{0,2,13.5f},{14,2,.5f}},{{0,1,2},{2,1,.5f}}};CharacterController3D controller;controller.Reset({0,0,-8});int previousCycle=-1,firstBelowFrame=-1;Vec3 firstBelowPosition{};unsigned firstPersonFrames=0,thirdPersonFrames=0;
    for(int frame=0;frame<720;++frame){const float time=float(frame)/60.f;const int phase=CharacterLabRules::AutomaticPhase(time);CharacterControl3D control{CharacterLabRules::Movement(phase),false};const int cycle=CharacterLabRules::JumpCycle(time);if(cycle!=previousCycle&&controller.Grounded()){control.jump=true;previousCycle=cycle;}controller.Step(control,colliders,1.f/60.f);if(firstBelowFrame<0&&controller.Position().y<-.01f){firstBelowFrame=frame;firstBelowPosition=controller.Position();}if(CharacterLabRules::FirstPerson(phase))++firstPersonFrames;else ++thirdPersonFrames;}
    const auto stats=controller.Stats();if(firstPersonFrames!=360||thirdPersonFrames!=360){std::cerr<<"Automatic camera coverage failed\n";return 1;}if(stats.jumps<3||stats.landings<3||stats.collisions==0){std::cerr<<"Automatic character traversal failed\n";return 2;}if(std::abs(controller.Position().x)>13.06f||std::abs(controller.Position().z)>13.06f||controller.Position().y<-.01f){std::cerr<<"Automatic character escaped collision bounds: "<<controller.Position().x<<' '<<controller.Position().y<<' '<<controller.Position().z<<" firstBelow="<<firstBelowFrame<<' '<<firstBelowPosition.x<<' '<<firstBelowPosition.y<<' '<<firstBelowPosition.z<<'\n';return 3;}std::cout<<"firstPersonFrames="<<firstPersonFrames<<" thirdPersonFrames="<<thirdPersonFrames<<" jumps="<<stats.jumps<<" landings="<<stats.landings<<" collisions="<<stats.collisions<<'\n';return 0;
}
