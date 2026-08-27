#include "Aevrynd/TerrainClipmap3D.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace Aevrynd {

bool TerrainClipmap3D::Configure(const TerrainClipmapSettings3D& settings){
    if(settings.chunksX==0||settings.chunksZ==0||settings.maximumChunks==0||settings.maximumUpdatesPerFrame==0||settings.maximumLodLevels==0||settings.distanceBands.size()>=settings.maximumLodLevels||settings.distanceBands.size()>(std::numeric_limits<std::uint32_t>::max)()||!std::isfinite(settings.origin.x)||!std::isfinite(settings.origin.z)||!std::isfinite(settings.chunkWorldSize)||settings.chunkWorldSize<=0||!std::isfinite(settings.hysteresis)||settings.hysteresis<0)return false;
    const std::size_t chunksX=settings.chunksX,chunksZ=settings.chunksZ;if(chunksX>(std::numeric_limits<std::size_t>::max)()/chunksZ)return false;const std::size_t count=chunksX*chunksZ;if(count>settings.maximumChunks)return false;
    float previous=-1.f;for(float band:settings.distanceBands){if(!std::isfinite(band)||band<=previous||band<settings.hysteresis)return false;previous=band;}
    const double maximumX=double(settings.origin.x)+double(settings.chunksX)*settings.chunkWorldSize,maximumZ=double(settings.origin.z)+double(settings.chunksZ)*settings.chunkWorldSize;if(!std::isfinite(maximumX)||!std::isfinite(maximumZ)||std::abs(maximumX)>double((std::numeric_limits<float>::max)())*.5||std::abs(maximumZ)>double((std::numeric_limits<float>::max)())*.5)return false;
    try{std::vector<float> newBands(settings.distanceBands.begin(),settings.distanceBands.end());std::vector<TerrainChunkLod3D> newChunks(count);std::vector<std::uint32_t> newDesired(count);for(std::size_t z=0;z<chunksZ;++z)for(std::size_t x=0;x<chunksX;++x)newChunks[z*chunksX+x]={std::uint32_t(x),std::uint32_t(z),0};settings_=settings;bands_.swap(newBands);chunks_.swap(newChunks);desiredLods_.swap(newDesired);settings_.distanceBands=bands_;stats_={count,0,0,std::uint32_t(bands_.size()),0};return true;}catch(const std::bad_alloc&){return false;}
}

bool TerrainClipmap3D::Update(Vec3 observer){
    if(chunks_.empty()||!std::isfinite(observer.x)||!std::isfinite(observer.z))return false;
    for(std::size_t index=0;index<chunks_.size();++index){const TerrainChunkLod3D& chunk=chunks_[index];const double centerX=double(settings_.origin.x)+(double(chunk.x)+.5)*settings_.chunkWorldSize,centerZ=double(settings_.origin.z)+(double(chunk.z)+.5)*settings_.chunkWorldSize,dx=centerX-observer.x,dz=centerZ-observer.z,distance=std::sqrt(dx*dx+dz*dz);if(!std::isfinite(distance))return false;std::uint32_t desired=chunk.lod;while(desired<bands_.size()&&distance>double(bands_[desired])+settings_.hysteresis)++desired;while(desired>0&&distance<double(bands_[desired-1])-settings_.hysteresis)--desired;desiredLods_[index]=desired;}
    // 先將目標場限制成相鄰最多一級，避免最終狀態產生 T-junction 級差。
    bool adjusted=true;while(adjusted){adjusted=false;for(std::size_t z=0;z<settings_.chunksZ;++z)for(std::size_t x=0;x<settings_.chunksX;++x){const std::size_t index=Index(x,z);auto constrain=[&](std::size_t neighbor){if(desiredLods_[index]>desiredLods_[neighbor]+1){desiredLods_[index]=desiredLods_[neighbor]+1;adjusted=true;}};if(x)constrain(Index(x-1,z));if(x+1<settings_.chunksX)constrain(Index(x+1,z));if(z)constrain(Index(x,z-1));if(z+1<settings_.chunksZ)constrain(Index(x,z+1));}}
    std::size_t changed{};bool progress=true;while(changed<settings_.maximumUpdatesPerFrame&&progress){progress=false;for(std::size_t index=0;index<chunks_.size()&&changed<settings_.maximumUpdatesPerFrame;++index){TerrainChunkLod3D& chunk=chunks_[index];if(chunk.lod==desiredLods_[index])continue;const std::uint32_t candidate=chunk.lod<desiredLods_[index]?chunk.lod+1:chunk.lod-1;const std::size_t x=chunk.x,z=chunk.z;bool valid=true;auto check=[&](std::size_t neighbor){const std::uint32_t other=chunks_[neighbor].lod;valid&=candidate>other?candidate-other<=1:other-candidate<=1;};if(x)check(Index(x-1,z));if(x+1<settings_.chunksX)check(Index(x+1,z));if(z)check(Index(x,z-1));if(z+1<settings_.chunksZ)check(Index(x,z+1));if(!valid)continue;chunk.lod=candidate;++changed;progress=true;}}
    std::size_t deferred{};for(std::size_t index=0;index<chunks_.size();++index)deferred+=chunks_[index].lod!=desiredLods_[index];stats_.changedChunks=changed;stats_.deferredChunks=deferred;++stats_.updateSerial;return true;
}

std::size_t TerrainClipmap3D::CapacityBytes()const{return bands_.capacity()*sizeof(float)+chunks_.capacity()*sizeof(TerrainChunkLod3D)+desiredLods_.capacity()*sizeof(std::uint32_t);}

}
