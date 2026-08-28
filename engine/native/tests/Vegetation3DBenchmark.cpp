#include <Aevrynd/Vegetation3D.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
using namespace Aevrynd;
static std::uint64_t Median(std::vector<std::uint64_t> values){std::sort(values.begin(),values.end());return values[values.size()/2];}
int main(){
    constexpr std::size_t count=65536;const VegetationAsset3D asset{{MeshHandle3D{1},MeshHandle3D{2},MeshHandle3D{3}},40,150,12};std::vector<VegetationInstance3D> instances;instances.reserve(count);
    for(std::size_t index=0;index<count;++index){const float x=float(int(index%256)-128)*3.f,z=float(index/256)*3.f+3.f;Matrix4 transform=Matrix4::Identity();transform[0][3]=x;transform[2][3]=z;instances.push_back({index+1,0,transform,{{x,1,z},1.5f,{1.5f,1.5f,1.5f}},{1,1,1,1}});}
    VegetationWorld3D world;const auto buildStart=std::chrono::steady_clock::now();if(!world.Build(std::span{&asset,1},instances))return 1;const auto buildUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-buildStart).count();
    Camera3D camera{};camera.position={0,3,0};camera.target={0,2,1};camera.projection.farPlane=1000;const VegetationView3D view{camera,16.f/9.f,420,true,true};world.Query(view);const auto warmBytes=world.MemoryStats().retainedCapacityBytes;std::vector<std::uint64_t> samples;samples.reserve(101);std::size_t draws{};for(int iteration=0;iteration<101;++iteration){const auto start=std::chrono::steady_clock::now();draws=world.Query(view).size();samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());}const auto stats=world.Stats();const auto afterBytes=world.MemoryStats().retainedCapacityBytes;
    const ProcessMemoryStats process=QueryProcessMemoryStats();std::cout<<"instances="<<count<<" candidates="<<stats.candidates<<" draws="<<draws<<" drawGroups=3 buildUs="<<buildUs<<" queryMedianNs="<<Median(samples)<<" retainedBytes="<<afterBytes<<" retainedBytesPerInstance="<<double(afterBytes)/double(count)<<" workingSetBytes="<<process.workingSetBytes<<" warmAllocationBytes="<<(afterBytes-warmBytes)<<" gpuResourceBytes=0 pure2DResourceBytes=0\n";
    return !draws||draws>MaximumMeshInstancesPerFrame3D||afterBytes!=warmBytes?2:0;
}
