#include <Aevrynd/Navigation3D.h>
#include <chrono>
#include <iostream>

using namespace Aevrynd;
int main(){
    NavigationGrid3D grid({{-10,0,-10},20,20,1,400,true});for(int z=0;z<20;++z)if(z!=11)grid.SetBlocked({9,z});const NavigationPath3D path=grid.FindPath({-8,0,-8},{8,0,8});if(!path||path.stats.rawPoints<=path.stats.simplifiedPoints||path.stats.visitedNodes>400){std::cerr<<"A-star routing or simplification failed\n";return 1;}for(Vec3 point:path.points)if(!grid.IsWalkable(grid.CoordinateAt(point))){std::cerr<<"Path crossed a blocked cell\n";return 2;}
    NavigationGrid3D corners({{0,0,0},3,3,1,32,true});corners.SetBlocked({1,0});corners.SetBlocked({0,1});if(corners.FindPath({.5f,0,.5f},{2.5f,0,2.5f})){std::cerr<<"Diagonal corner cutting was accepted\n";return 3;}
    CollisionWorld3D world;world.CreateBox({{{0,1,0},{2,1,2}},4,1,0,false});NavigationGrid3D raster({{-8,0,-8},16,16,1,256,true});const std::size_t blocked=raster.Rasterize(world,{4,1,false},.5f);if(blocked<25||raster.IsWalkable(raster.CoordinateAt({0,0,0}))){std::cerr<<"Collision rasterization failed\n";return 4;}
    NavigationGrid3D weighted({{0,0,0},5,3,1,64,false});for(int x=1;x<4;++x)weighted.SetTraversalCost({x,1},10);const NavigationPath3D weightedPath=weighted.FindPath({.5f,0,1.5f},{4.5f,0,1.5f});if(!weightedPath||weightedPath.stats.totalCost>=300) {std::cerr<<"Weighted traversal failed\n";return 5;}for(Vec3 point:weightedPath.points)if(weighted.TraversalCost(weighted.CoordinateAt(point))>1){std::cerr<<"Weighted path simplification failed\n";return 6;}
    NavigationGrid3D limited({{-10,0,-10},20,20,1,2,false});const NavigationPath3D limitedPath=limited.FindPath({-9,0,-9},{9,0,9});if(limitedPath||!limitedPath.stats.searchLimitReached){std::cerr<<"Search budget failed\n";return 7;}
    constexpr int Searches=1000;const auto start=std::chrono::steady_clock::now();std::size_t reached{};for(int index=0;index<Searches;++index)reached+=bool(grid.FindPath({-8,0,float(index%16)-8},{8,0,float((index*7)%16)-8}));const uint64_t averageUs=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count())/Searches;if(reached!=Searches||averageUs>1000){std::cerr<<"Navigation throughput failed\n";return 8;}
    std::cout<<"visited="<<path.stats.visitedNodes<<" raw="<<path.stats.rawPoints<<" simplified="<<path.stats.simplifiedPoints<<" blocked="<<blocked<<" averageUs="<<averageUs<<'\n';return 0;
}
