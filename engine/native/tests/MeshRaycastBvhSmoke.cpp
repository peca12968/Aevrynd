#include "Aevrynd/Scene3D.h"
#include <cmath>
#include <iostream>
#include <memory>

using namespace Aevrynd;

namespace {
std::shared_ptr<Mesh3D> CreateTerrain(std::uint32_t cells){
    auto mesh=std::make_shared<Mesh3D>();const std::uint32_t stride=cells+1;mesh->vertices.reserve(std::size_t{stride}*stride);mesh->indices.reserve(std::size_t{cells}*cells*6);
    for(std::uint32_t z=0;z<=cells;++z)for(std::uint32_t x=0;x<=cells;++x){const float y=.15f*std::sin(float(x)*.2f)*std::cos(float(z)*.17f);mesh->vertices.push_back({float(x),y,float(z),PackNormalSnorm8({0,1,0}),0xffffffffu});}
    for(std::uint32_t z=0;z<cells;++z)for(std::uint32_t x=0;x<cells;++x){const std::uint32_t a=z*stride+x,b=a+1,c=a+stride,d=c+1;mesh->indices.insert(mesh->indices.end(),{a,c,b,b,c,d});}return mesh;
}
MeshResource3D Resource(const std::shared_ptr<Mesh3D>& mesh){return{MeshHandle3D{1},mesh->Bounds(),mesh->vertices.size(),mesh->indices.size(),mesh};}
}

int main(){
    const auto require=[](bool condition,const char* message){if(condition)return true;std::cerr<<message<<'\n';return false;};
    auto terrain=CreateTerrain(64);if(!require(terrain->indices.size()/3==8192,"Terrain triangle count failed"))return 1;const MeshResource3D resource=Resource(terrain);Scene3D scene;scene.Mesh(resource,{{}, {},{1,1,1}});scene.Mesh(resource,{{100,0,0},{0,.18f,0},{1.5f,2.f,.75f}});
    SceneQueryStats3D firstStats{};const SceneHit3D first=scene.Raycast({{20,10,20},{0,-1,0}},30,&firstStats);if(!require(first&&first.objectIndex==0,"First terrain hit failed")||!require(firstStats.meshBvhBuilds==1,"Initial mesh BVH build count failed")||!require(firstStats.meshTriangleCandidates<128,"Initial candidate bound failed")||!require(firstStats.trianglesTested==firstStats.meshTriangleCandidates,"Triangle telemetry failed")||!require(std::isfinite(first.normal.x)&&std::isfinite(first.normal.y)&&std::isfinite(first.normal.z),"Hit normal failed"))return 2;
    SceneQueryStats3D reusedStats{};const SceneHit3D reused=scene.Raycast({{120,20,10},{0,-1,0}},50,&reusedStats);if(!require(reused&&reused.objectIndex==1,"Transformed terrain hit failed")||!require(reusedStats.meshBvhBuilds==0,"Mesh BVH reuse failed")||!require(reusedStats.meshTriangleCandidates<128,"Reused candidate bound failed")||!require(!scene.Raycast({{20,10,20},{0,-1,0}},1),"Maximum distance failed"))return 3;
    const auto beforeTrim=scene.MemoryStats();if(!require(beforeTrim.retainedCapacityBytes>0,"Retained memory telemetry failed")||!require(terrain->RaycastIndexBytes()>0,"Mesh BVH memory telemetry failed"))return 4;scene.TrimUnusedMemory();if(!require(terrain->RaycastIndexBytes()==0,"Mesh BVH trim failed"))return 5;SceneQueryStats3D rebuiltStats{};if(!require(scene.Raycast({{20,10,20},{0,-1,0}},30,&rebuiltStats)&&rebuiltStats.meshBvhBuilds==1,"Mesh BVH rebuild failed"))return 6;
    Mesh3D copied=*terrain;copied.vertices.front().y+=100.f;if(!require(copied.RaycastIndexBytes()==0,"Copied mesh must not share a stale raycast index"))return 7;
    auto triangle=std::make_shared<Mesh3D>();triangle->vertices={Vertex3D{-1,0,-1},Vertex3D{1,0,-1},Vertex3D{0,0,1}};triangle->indices={0,1,2};Scene3D backfaceScene;backfaceScene.Mesh(Resource(triangle),{});if(!require(bool(backfaceScene.Raycast({{0,1,0},{0,-1,0}},10,nullptr,false)),"Two-sided raycast failed")||!require(!backfaceScene.Raycast({{0,1,0},{0,-1,0}},10,nullptr,true),"Backface culling failed")||!require(bool(backfaceScene.Raycast({{0,-1,0},{0,1,0}},10,nullptr,true)),"Front-face raycast failed"))return 8;
    auto degenerate=std::make_shared<Mesh3D>();degenerate->vertices={Vertex3D{0,0,0},Vertex3D{1,0,0},Vertex3D{2,0,0}};degenerate->indices={0,1,2};Scene3D degenerateScene;degenerateScene.Mesh(Resource(degenerate),{});if(!require(!degenerateScene.Raycast({{1,1,0},{0,-1,0}},10),"Degenerate triangle rejection failed"))return 9;
    std::cout<<"triangles="<<terrain->indices.size()/3<<" candidates="<<firstStats.meshTriangleCandidates<<" nodes="<<firstStats.meshBvhNodesTested<<"\n";
}
