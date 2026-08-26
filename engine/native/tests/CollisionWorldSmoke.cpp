#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/CharacterController3D.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;
bool Near(float left,float right,float tolerance=.01f){return std::abs(left-right)<=tolerance;}
int main(){
    CollisionWorld3D world;const ColliderHandle3D wall=world.CreateBox({{{0,1,5},{2,1,.5f}},1,1,42,false}),enemy=world.CreateBox({{{0,1,9},{1,1,1}},2,1,84,false}),trigger=world.CreateBox({{{0,1,2},{1,1,1}},4,1,126,true});if(!wall||!enemy||!trigger||world.Stats().colliders!=3||world.Stats().triggers!=1){std::cerr<<"Collider creation failed\n";return 1;}
    const Ray3D ray=MakeRay3D({0,1,0},{0,0,1});CollisionRayHit3D hit=world.Raycast(ray,20);if(!hit||hit.collider!=wall||!Near(hit.distance,4.5f)||hit.userData!=42||hit.normal.z!=-1){std::cerr<<"Nearest filtered raycast failed\n";return 2;}hit=world.Raycast(ray,20,{4,1,true});if(!hit||hit.collider!=trigger||!Near(hit.distance,1)){std::cerr<<"Trigger raycast failed\n";return 3;}
    const auto overlaps=world.QueryBox({0,1,5},{3,2,1});if(overlaps.size()!=1||overlaps.front()!=wall||!world.OverlapsBox({0,1,9},{.5f,.5f,.5f},{2,1,false})){std::cerr<<"Layered overlap query failed\n";return 4;}
    StaticBoxCollider3D moved={{{5,1,5},{1,1,1}},1,1,7,false};if(!world.UpdateBox(wall,moved)||world.Raycast(ray,6)||!world.Destroy(enemy)||world.Get(enemy)||world.Stats().colliders!=2){std::cerr<<"Collider update or destruction failed\n";return 5;}
    CharacterController3D controller;world.CreateBox({{{0,-.5f,0},{10,.5f,10}},1,1,0,false});controller.Reset({0,0,0});for(int frame=0;frame<120;++frame)controller.Step({},world,1.f/60.f);if(!controller.Grounded()){std::cerr<<"Character collision-world integration failed\n";return 6;}
    for(int index=0;index<4096;++index)world.CreateBox({{{float(index),0,0},{.5f,.5f,.5f}},1,1,uint64_t(index),false});const ReusableMemoryStats expandedMemory=world.MemoryStats();world.Clear();const std::size_t releasedBytes=world.TrimUnusedMemory();const ReusableMemoryStats trimmedMemory=world.MemoryStats();world.CreateBox({{{0,1,3},{1,1,.5f}},1,1,777,false});if(!releasedBytes||trimmedMemory.retainedCapacityBytes>=expandedMemory.retainedCapacityBytes||trimmedMemory.lastReleasedBytes!=releasedBytes||trimmedMemory.trimCount!=1||world.Raycast(ray,10).userData!=777){std::cerr<<"Collision workspace memory recovery failed\n";return 7;}
    const auto stats=world.Stats();std::cout<<"colliders="<<stats.colliders<<" queries="<<stats.queries<<" tested="<<stats.boundsTested<<" hits="<<stats.hits<<" released="<<releasedBytes<<'\n';return 0;
}
