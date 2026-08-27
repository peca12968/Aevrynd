#include <Aevrynd/Scene3D.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

static bool Near(float left,float right,float tolerance=.001f){return std::abs(left-right)<=tolerance;}

int main(){
    Scene3D scene;for(int z=0;z<100;++z)for(int x=-50;x<50;++x)scene.Box({float(x)*4.f,0,float(z)*4.f+5.f},{1,1,1},{1,1,1,1});
    SceneQueryStats3D stats{};const Ray3D ray{{0,0,-10},{0,0,1}};SceneHit3D hit=scene.Raycast(ray,1000,&stats);
    if(!hit||hit.objectType!=SceneObjectType3D::Box||!Near(hit.distance,14.5f)||stats.bvhBuilds!=1||stats.bvhNodesTested==0||stats.bvhObjectCandidates>=100||stats.boundsTested>=100){std::cerr<<"Large-scene BVH query failed candidates="<<stats.bvhObjectCandidates<<" bounds="<<stats.boundsTested<<" nodes="<<stats.bvhNodesTested<<'\n';return 1;}
    hit=scene.Raycast(ray,14,&stats);if(hit||stats.bvhBuilds!=0){std::cerr<<"Maximum ray distance failed\n";return 2;}
    scene.Box({0,0,2},{1,1,1},{1,1,1,1});hit=scene.Raycast(ray,1000,&stats);if(!hit||!Near(hit.distance,11.5f)||stats.bvhBuilds!=1){std::cerr<<"Dirty BVH rebuild failed\n";return 3;}

    const Ray3D forwardRay{{0,0,0},{0,0,1}};Scene3D triangleScene;triangleScene.Triangle({-1,-1,5},{0,1,5},{1,-1,5},{1,1,1,1});if(!triangleScene.Raycast(forwardRay,10,nullptr,true)){std::cerr<<"Front-face culling query failed\n";return 4;}triangleScene.Clear();triangleScene.Triangle({-1,-1,5},{1,-1,5},{0,1,5},{1,1,1,1});if(triangleScene.Raycast(forwardRay,10,nullptr,true)||!triangleScene.Raycast(forwardRay,10,nullptr,false)){std::cerr<<"Back-face query semantics failed\n";return 5;}

    auto nearCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox()),farCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitPyramid());const MeshResource3D nearMesh{{21},nearCpu->Bounds(),nearCpu->vertices.size(),nearCpu->indices.size(),nearCpu},farMesh{{22},farCpu->Bounds(),farCpu->vertices.size(),farCpu->indices.size(),farCpu};Scene3D lodScene;lodScene.MeshLod(nearMesh,farMesh,5,{{0,0,8},{},{2,2,2}});hit=lodScene.Raycast(forwardRay,100,&stats);if(!hit||hit.objectType!=SceneObjectType3D::Mesh||hit.objectIndex!=0||!Near(hit.distance,7.f)||stats.trianglesTested!=12){std::cerr<<"LOD collision mesh semantics failed\n";return 6;}

    const ReusableMemoryStats peak=scene.MemoryStats();scene.Clear();const ReusableMemoryStats reclaimable=scene.MemoryStats();const std::size_t released=scene.TrimUnusedMemory();const ReusableMemoryStats trimmed=scene.MemoryStats();if(!reclaimable.reclaimableCapacityBytes||!released||trimmed.retainedCapacityBytes>=peak.retainedCapacityBytes||trimmed.reclaimableCapacityBytes||trimmed.lastReleasedBytes!=released||trimmed.trimCount!=1){std::cerr<<"BVH reusable memory trim failed\n";return 7;}
    scene.Box({0,0,5},{1,1,1},{1,1,1,1});if(!scene.Raycast(ray,100,&stats)||stats.bvhBuilds!=1){std::cerr<<"Scene reuse after BVH trim failed\n";return 8;}
    std::cout<<"nodes="<<stats.bvhNodesTested<<" candidates="<<stats.bvhObjectCandidates<<" released="<<released<<'\n';return 0;
}
