#include <Aevrynd/Scene3D.h>
#include <chrono>
#include <cstdint>
#include <iostream>

using namespace Aevrynd;

int main(){
    constexpr int Side=256;constexpr int Queries=512;Scene3D scene;scene.Reserve({0,0,Side*Side,0,0});for(int z=0;z<Side;++z)for(int x=0;x<Side;++x)scene.Box({float(x)*3.f,0,float(z)*3.f},{1,1,1},{1,1,1,1});
    std::uint64_t candidates{},nodes{},triangles{};const auto start=std::chrono::steady_clock::now();for(int query=0;query<Queries;++query){const int x=(query*73)%Side;SceneQueryStats3D stats{};const SceneHit3D hit=scene.Raycast({{float(x)*3.f,0,-5},{0,0,1}},1000,&stats);if(!hit)return 1;candidates+=stats.bvhObjectCandidates;nodes+=stats.bvhNodesTested;triangles+=stats.trianglesTested;}const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();const double averageCandidates=double(candidates)/Queries,averageNodes=double(nodes)/Queries;if(averageCandidates>=double(Side*Side)/20.0||averageNodes>=double(Side*Side)/10.0){std::cerr<<"BVH candidate reduction failed candidates="<<averageCandidates<<" nodes="<<averageNodes<<'\n';return 2;}std::cout<<"objects="<<Side*Side<<" queries="<<Queries<<" averageCandidates="<<averageCandidates<<" averageNodes="<<averageNodes<<" averageTriangles="<<double(triangles)/Queries<<" elapsedUs="<<elapsed<<'\n';return 0;
}
