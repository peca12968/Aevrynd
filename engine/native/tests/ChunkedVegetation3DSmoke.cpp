#include <Aevrynd/Vegetation3D.h>
#include <iostream>
#include <vector>
using namespace Aevrynd;

static VegetationInstance3D Instance(std::uint64_t key,float x,float z){Matrix4 transform=Matrix4::Identity();transform[0][3]=x;transform[2][3]=z;return{key,0,transform,{{x,0,z},1,{1,1,1}},{1,1,1,1}};}
int main(){
    const VegetationAsset3D asset{{MeshHandle3D{1},MeshHandle3D{2},MeshHandle3D{3}},20,60,8};
    const std::vector<VegetationInstance3D> left={Instance(10,-10,8),Instance(20,-10,40)},right={Instance(30,10,20),Instance(40,10,90)};
    ChunkedVegetationWorld3D world;VegetationChunkHandle3D leftHandle{},rightHandle{};
    if(!world.Reserve(2,4)||!world.AttachChunk(1,std::span{&asset,1},left,&leftHandle)||!world.AttachChunk(2,std::span{&asset,1},right,&rightHandle)){std::cerr<<"attach failed\n";return 1;}
    if(!leftHandle||leftHandle==rightHandle||world.Stats().chunks!=2||world.Stats().instances!=4){std::cerr<<"owner metadata failed\n";return 2;}
    const auto duplicate=Instance(20,20,10);if(world.AttachChunk(3,std::span{&asset,1},std::span{&duplicate,1})||world.Stats().chunks!=2){std::cerr<<"duplicate rollback failed\n";return 3;}
    std::vector<VegetationInstance3D> overflow(MaximumMeshInstancesPerFrame3D+1,duplicate);if(world.AttachChunkAtomic(3,std::span{&asset,1},overflow)||world.Stats().chunks!=2||world.Stats().instances!=4){std::cerr<<"overflow rollback failed\n";return 9;}
    Camera3D camera{};camera.position={0,0,0};camera.target={0,0,1};camera.projection.farPlane=500;const auto draws=world.Query({camera,1,200,true,false});
    if(draws.size()!=5||world.Stats().submitted!=5||world.Stats().nearInstances!=2||world.Stats().farInstances!=2||world.Stats().impostorInstances!=1){std::cerr<<"cross chunk LOD failed\n";return 4;}
    ChunkedVegetationWorld3D reordered;if(!reordered.AttachChunkAtomic(2,std::span{&asset,1},right)||!reordered.AttachChunkAtomic(1,std::span{&asset,1},left)){std::cerr<<"reordered attach failed\n";return 12;}const auto reorderedDraws=reordered.Query({camera,1,200,true,false});if(reorderedDraws.size()!=draws.size()){std::cerr<<"deterministic query size failed\n";return 13;}for(std::size_t index=0;index<draws.size();++index)if(draws[index].mesh!=reorderedDraws[index].mesh||draws[index].transform[0][3]!=reorderedDraws[index].transform[0][3]||draws[index].transform[2][3]!=reorderedDraws[index].transform[2][3]||draws[index].lod!=reorderedDraws[index].lod){std::cerr<<"deterministic owner order failed\n";return 14;}
    if(!world.DetachChunk(leftHandle)||world.DetachChunk(leftHandle)||world.Stats().chunks!=1||world.Stats().instances!=2){std::cerr<<"generation detach failed\n";return 5;}
    VegetationChunkHandle3D replacement{};if(!world.AttachChunk(1,std::span{&asset,1},left,&replacement)||replacement.generation==leftHandle.generation||world.DetachChunk(leftHandle)){std::cerr<<"stale handle accepted\n";return 6;}
    if(!world.DetachChunk(replacement)){std::cerr<<"replacement detach failed\n";return 10;}for(std::uint64_t cycle=0;cycle<128;++cycle){VegetationChunkHandle3D handle{};if(!world.AttachChunkAtomic(100+cycle,std::span{&asset,1},left,&handle)||!world.BeginDetach(handle)){std::cerr<<"attach detach cycle failed\n";return 11;}}
    world.Clear();if(world.Stats().chunks||world.Contains(1)){std::cerr<<"clear failed\n";return 7;}const auto before=world.MemoryStats().retainedCapacityBytes;world.TrimUnusedMemory();if(world.MemoryStats().retainedCapacityBytes>before){std::cerr<<"trim failed\n";return 8;}
    std::cout<<"chunked vegetation smoke passed\n";
}
