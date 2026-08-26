#include <Aevrynd/WorldStreamer3D.h>
#include <algorithm>
#include <cmath>

namespace Aevrynd {
namespace {
unsigned ChunkDistance(WorldChunkCoordinate3D left,WorldChunkCoordinate3D right){return unsigned((std::max)(std::abs(left.x-right.x),std::abs(left.z-right.z)));}
float DistanceSquared(Vec3 left,Vec3 right){const Vec3 delta=left-right;return Dot(delta,delta);}
}

std::size_t WorldStreamer3D::CoordinateHash::operator()(WorldChunkCoordinate3D coordinate)const noexcept{return(std::size_t(uint32_t(coordinate.x))*0x9e3779b1u)^std::size_t(uint32_t(coordinate.z)*0x85ebca77u);}
void WorldStreamer3D::Configure(WorldStreamingSettings3D settings){settings.chunkSize=(std::max)(1.f,settings.chunkSize);settings.retainRadius=(std::max)(settings.loadRadius,settings.retainRadius);settings.maximumLoadsPerUpdate=(std::max)(std::size_t(1),settings.maximumLoadsPerUpdate);settings.maximumUnloadsPerUpdate=(std::max)(std::size_t(1),settings.maximumUnloadsPerUpdate);settings_=settings;Reset();}
void WorldStreamer3D::Reset(){resident_.clear();residentView_.clear();loadedView_.clear();unloadedView_.clear();stats_={};}
WorldChunkCoordinate3D WorldStreamer3D::CoordinateAt(Vec3 position,float chunkSize){chunkSize=(std::max)(1.f,chunkSize);return{int(std::floor(position.x/chunkSize)),int(std::floor(position.z/chunkSize))};}
Vec3 WorldStreamer3D::ChunkCenter(WorldChunkCoordinate3D coordinate,float chunkSize){chunkSize=(std::max)(1.f,chunkSize);return{(float(coordinate.x)+.5f)*chunkSize,0,(float(coordinate.z)+.5f)*chunkSize};}
uint32_t WorldStreamer3D::ChunkSeed(WorldChunkCoordinate3D coordinate,uint32_t worldSeed){uint32_t value=worldSeed^uint32_t(coordinate.x)*0x9e3779b9u^uint32_t(coordinate.z)*0x85ebca6bu;value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;return value;}
bool WorldStreamer3D::Contains(WorldChunkCoordinate3D coordinate)const{return resident_.contains(coordinate);}

WorldStreamingStats3D WorldStreamer3D::Update(Vec3 observerPosition){
    const WorldChunkCoordinate3D observer=CoordinateAt(observerPosition,settings_.chunkSize);stats_.observerChunk=observer;stats_.lastLoaded=stats_.lastUnloaded=0;loadedView_.clear();unloadedView_.clear();++stats_.updates;
    std::vector<WorldChunkCoordinate3D> unloads;unloads.reserve(resident_.size());for(const auto& [coordinate,chunk]:resident_){(void)chunk;if(ChunkDistance(coordinate,observer)>settings_.retainRadius)unloads.push_back(coordinate);}std::sort(unloads.begin(),unloads.end(),[&](auto left,auto right){return ChunkDistance(left,observer)>ChunkDistance(right,observer);});if(unloads.size()>settings_.maximumUnloadsPerUpdate)unloads.resize(settings_.maximumUnloadsPerUpdate);for(WorldChunkCoordinate3D coordinate:unloads){const auto found=resident_.find(coordinate);if(found!=resident_.end()){unloadedView_.push_back(found->second);resident_.erase(found);}++stats_.lastUnloaded;++stats_.totalUnloaded;}
    std::vector<WorldChunk3D> loads;const int radius=int(settings_.loadRadius);loads.reserve(std::size_t(radius*2+1)*std::size_t(radius*2+1));for(int z=-radius;z<=radius;++z)for(int x=-radius;x<=radius;++x){const WorldChunkCoordinate3D coordinate{observer.x+x,observer.z+z};if(resident_.contains(coordinate))continue;const Vec3 center=ChunkCenter(coordinate,settings_.chunkSize);loads.push_back({coordinate,center,ChunkSeed(coordinate,settings_.worldSeed),DistanceSquared(center,observerPosition)});}std::sort(loads.begin(),loads.end(),[](const WorldChunk3D& left,const WorldChunk3D& right){if(left.observerDistanceSquared!=right.observerDistanceSquared)return left.observerDistanceSquared<right.observerDistanceSquared;if(left.coordinate.z!=right.coordinate.z)return left.coordinate.z<right.coordinate.z;return left.coordinate.x<right.coordinate.x;});const std::size_t loadCount=(std::min)(loads.size(),settings_.maximumLoadsPerUpdate);for(std::size_t index=0;index<loadCount;++index){resident_.emplace(loads[index].coordinate,loads[index]);loadedView_.push_back(loads[index]);++stats_.lastLoaded;++stats_.totalLoaded;}
    residentView_.clear();residentView_.reserve(resident_.size());for(auto& [coordinate,chunk]:resident_){(void)coordinate;chunk.observerDistanceSquared=DistanceSquared(chunk.center,observerPosition);residentView_.push_back(chunk);}std::sort(residentView_.begin(),residentView_.end(),[](const WorldChunk3D& left,const WorldChunk3D& right){if(left.observerDistanceSquared!=right.observerDistanceSquared)return left.observerDistanceSquared<right.observerDistanceSquared;if(left.coordinate.z!=right.coordinate.z)return left.coordinate.z<right.coordinate.z;return left.coordinate.x<right.coordinate.x;});
    stats_.desiredChunks=std::size_t(radius*2+1)*std::size_t(radius*2+1);stats_.residentChunks=resident_.size();stats_.queuedChunks=loads.size()-loadCount;stats_.peakResidentChunks=(std::max)(stats_.peakResidentChunks,stats_.residentChunks);return stats_;
}
}
