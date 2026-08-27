#include <Aevrynd/TerrainClipmap3D.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace Aevrynd;

namespace {
bool NeighborSafe(const TerrainClipmap3D& clipmap,std::uint32_t width){const auto chunks=clipmap.Chunks();for(const auto& chunk:chunks){const size_t index=size_t(chunk.z)*width+chunk.x;if(chunk.x+1<width&&std::abs(int(chunk.lod)-int(chunks[index+1].lod))>1)return false;if(index+width<chunks.size()&&std::abs(int(chunk.lod)-int(chunks[index+width].lod))>1)return false;}return true;}
bool Equal(std::span<const TerrainChunkLod3D> a,std::span<const TerrainChunkLod3D> b){if(a.size()!=b.size())return false;for(size_t index=0;index<a.size();++index)if(a[index].x!=b[index].x||a[index].z!=b[index].z||a[index].lod!=b[index].lod)return false;return true;}
}

int main(){
    const float bands[]={96,192,384,768};TerrainClipmapSettings3D settings;settings.origin={-1024,0,-1024};settings.chunksX=32;settings.chunksZ=32;settings.chunkWorldSize=64;settings.distanceBands=bands;settings.hysteresis=12;settings.maximumChunks=2048;settings.maximumUpdatesPerFrame=96;TerrainClipmap3D first,second;if(!first.Configure(settings)||!second.Configure(settings)){std::cerr<<"Clipmap configuration failed\n";return 1;}const size_t capacity=first.CapacityBytes();for(unsigned frame=0;frame<64;++frame){if(!first.Update({15,0,-27})||!second.Update({15,0,-27})||!NeighborSafe(first,32)){std::cerr<<"Clipmap deterministic update failed\n";return 2;}if(first.Stats().changedChunks>settings.maximumUpdatesPerFrame||first.CapacityBytes()!=capacity){std::cerr<<"Clipmap update or allocation budget failed\n";return 3;}}if(first.Stats().deferredChunks!=0||!Equal(first.Chunks(),second.Chunks())){std::cerr<<"Clipmap convergence failed\n";return 4;}
    const auto stable=std::vector<TerrainChunkLod3D>(first.Chunks().begin(),first.Chunks().end());for(unsigned frame=0;frame<20;++frame)if(!first.Update({15.f+float(frame%2)*.1f,0,-27.f-float(frame%3)*.1f})||first.Stats().changedChunks!=0){std::cerr<<"Clipmap hysteresis failed\n";return 5;}if(!Equal(first.Chunks(),stable)){std::cerr<<"Clipmap small-motion stability failed\n";return 6;}
    const auto start=std::chrono::steady_clock::now();for(unsigned frame=0;frame<1000;++frame)if(!first.Update({float(frame%300)-150,0,float((frame*7)%300)-150})){std::cerr<<"Clipmap performance update failed\n";return 7;}const auto elapsed=std::chrono::steady_clock::now()-start;if(elapsed>std::chrono::milliseconds(250)||first.CapacityBytes()!=capacity||!NeighborSafe(first,32)){std::cerr<<"Clipmap performance budget failed\n";return 8;}
    const auto snapshot=std::vector<TerrainChunkLod3D>(first.Chunks().begin(),first.Chunks().end());if(first.Update({(std::numeric_limits<float>::quiet_NaN)(),0,0})||!Equal(first.Chunks(),snapshot)){std::cerr<<"Clipmap NaN atomicity failed\n";return 9;}TerrainClipmapSettings3D invalid=settings;invalid.chunksX=(std::numeric_limits<std::uint32_t>::max)();invalid.chunksZ=(std::numeric_limits<std::uint32_t>::max)();if(first.Configure(invalid)||!Equal(first.Chunks(),snapshot)){std::cerr<<"Clipmap overflow atomicity failed\n";return 10;}const float badBands[]={100,90};invalid=settings;invalid.distanceBands=badBands;if(first.Configure(invalid)||!Equal(first.Chunks(),snapshot)){std::cerr<<"Clipmap band atomicity failed\n";return 11;}invalid=settings;invalid.maximumLodLevels=4;if(first.Configure(invalid)||!Equal(first.Chunks(),snapshot)){std::cerr<<"Clipmap LOD capacity atomicity failed\n";return 12;}
    std::cout<<"chunks="<<first.Chunks().size()<<" capacityBytes="<<capacity<<" thousandUpdatesMicroseconds="<<std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()<<" changed="<<first.Stats().changedChunks<<" deferred="<<first.Stats().deferredChunks<<'\n';
}
