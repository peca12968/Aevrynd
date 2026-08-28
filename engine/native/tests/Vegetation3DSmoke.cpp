#include <Aevrynd/Vegetation3D.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
using namespace Aevrynd;

static VegetationInstance3D Instance(std::uint64_t key,float z){Matrix4 transform=Matrix4::Identity();transform[2][3]=z;return{key,0,transform,{{0,0,z},1,{1,1,1}},{.72f,.88f,.64f,1}};}
int main(){
    const VegetationAsset3D asset{{MeshHandle3D{1},MeshHandle3D{2},MeshHandle3D{3}},20,60,8};
    std::vector<VegetationInstance3D> instances={Instance(10,8),Instance(20,20),Instance(21,22),Instance(30,36),Instance(40,60),Instance(50,90)};
    VegetationWorld3D world;if(!world.Build(std::span{&asset,1},instances)){std::cerr<<"build failed\n";return 1;}
    Camera3D camera{};camera.position={0,0,0};camera.target={0,0,1};camera.projection.farPlane=500;
    const auto selected=world.Query({camera,16.f/9.f,200,true,false});
    const auto stats=world.Stats();if(selected.size()!=9||stats.nearInstances!=3||stats.farInstances!=4||stats.impostorInstances!=2||stats.transitionInstances!=3||stats.shadowCasters!=6){std::cerr<<"LOD selection or shadow ownership failed\n";return 2;}
    for(const std::uint32_t seed:{20u,21u}){std::size_t shadowOwners=0;bool invertedOwner=false;for(const auto& draw:selected)if(draw.lodSeed==seed&&draw.lodCoverage<1.f){shadowOwners+=draw.castsShadow;invertedOwner|=draw.castsShadow&&draw.lodInvert;}if(shadowOwners!=1||(seed==20&&invertedOwner)||(seed==21&&!invertedOwner)){std::cerr<<"dominant transition shadow owner failed\n";return 17;}}
    const auto transition=std::find_if(selected.begin(),selected.end(),[](const auto& draw){return draw.lodCoverage<1;});if(transition==selected.end()||transition->lodCoverage<=0||transition->lodCoverage>=1){std::cerr<<"transition coverage failed\n";return 3;}
    const auto capacityBefore=world.MemoryStats().retainedCapacityBytes;for(int index=0;index<64;++index){const auto warm=world.Query({camera,16.f/9.f,200,true,false});if(warm.size()!=selected.size())return 4;}if(world.MemoryStats().retainedCapacityBytes!=capacityBefore){std::cerr<<"warm query allocated\n";return 5;}
    if(!world.Query({camera,1,200,false,true}).empty()||world.MemoryStats().retainedCapacityBytes!=capacityBefore){std::cerr<<"disabled path was not zero-work\n";return 6;}
    auto moved=instances[0];moved.bounds.center.z=150;moved.transform[2][3]=150;if(!world.Update(moved.key,moved.transform,moved.bounds)||world.Query({camera,1,100,true,false}).size()>=selected.size()){std::cerr<<"refit failed\n";return 7;}
    if(!world.Remove(30)||world.Remove(30)||world.Stats().instances!=5){std::cerr<<"remove failed\n";return 8;}
    std::reverse(instances.begin(),instances.end());if(!world.Build(std::span{&asset,1},instances)||world.Query({camera,1,200,true,false}).size()!=selected.size()){std::cerr<<"reorder rebuild failed\n";return 9;}
    std::vector<VegetationInstance3D> duplicate={Instance(1,5),Instance(1,10)};const auto stableCount=world.Stats().instances;if(world.Build(std::span{&asset,1},duplicate)||world.Stats().instances!=stableCount){std::cerr<<"duplicate failure was not atomic\n";return 10;}
    VegetationAsset3D missing=asset;missing.meshes[2]={};const VegetationInstance3D distant=Instance(70,90);if(!world.Build(std::span{&missing,1},std::span{&distant,1})||!world.Query({camera,1,200,true,false}).empty()||world.Stats().missingAssetRejected!=1){std::cerr<<"missing impostor was accepted\n";return 11;}
    missing=asset;missing.meshes[0]={};const VegetationInstance3D missingPrimary=Instance(71,20);if(!world.Build(std::span{&missing,1},std::span{&missingPrimary,1})||!world.Query({camera,1,200,true,false}).empty()||world.Stats().missingAssetRejected!=1){std::cerr<<"missing transition primary staged an orphan secondary\n";return 18;}
    missing=asset;missing.meshes[1]={};const VegetationInstance3D missingSecondary=Instance(72,20);if(!world.Build(std::span{&missing,1},std::span{&missingSecondary,1})){std::cerr<<"missing transition secondary build failed\n";return 19;}const auto fallback=world.Query({camera,1,200,true,false});if(fallback.size()!=1||fallback[0].lodCoverage!=1.f||fallback[0].lodInvert||!fallback[0].castsShadow||world.Stats().missingAssetRejected!=1){std::cerr<<"missing transition secondary did not restore full primary\n";return 20;}
    if(world.Reserve(MaximumMeshInstancesPerFrame3D+1)){std::cerr<<"overflow reserve accepted\n";return 12;}
    std::vector<VegetationInstance3D> maximumTransition;maximumTransition.reserve(MaximumMeshInstancesPerFrame3D);for(std::size_t index=0;index<MaximumMeshInstancesPerFrame3D;++index){auto value=Instance(index+1,20);value.bounds.center.x=float(index%256)*.01f;maximumTransition.push_back(value);}if(!world.Build(std::span{&asset,1},maximumTransition)){std::cerr<<"maximum transition build failed\n";return 14;}const auto maximumDraws=world.Query({camera,1,100,true,false});if(maximumDraws.size()!=MaximumMeshInstancesPerFrame3D||world.Stats().transitionDrawsDropped!=MaximumMeshInstancesPerFrame3D||world.Stats().shadowCasters!=MaximumMeshInstancesPerFrame3D){std::cerr<<"maximum transition fairness failed\n";return 15;}std::vector<bool> seen(MaximumMeshInstancesPerFrame3D+1);for(const auto& draw:maximumDraws){if(!draw.castsShadow||draw.lodCoverage!=1.f||draw.lodInvert||!draw.lodSeed||draw.lodSeed>=seen.size()||seen[draw.lodSeed]){std::cerr<<"maximum transition lost or duplicated primary\n";return 16;}seen[draw.lodSeed]=true;}
    world.Clear();const auto retained=world.MemoryStats().retainedCapacityBytes,reclaimable=world.MemoryStats().reclaimableCapacityBytes;if(!retained||!reclaimable||!world.TrimUnusedMemory()||world.MemoryStats().retainedCapacityBytes>=retained){std::cerr<<"lifecycle trim failed\n";return 13;}
    std::cout<<"draws="<<selected.size()<<" transitions="<<stats.transitionInstances<<" warmAllocationBytes=0 retainedBeforeTrim="<<retained<<" releasedBytes="<<world.MemoryStats().lastReleasedBytes<<'\n';
}
