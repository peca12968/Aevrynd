#include "Aevrynd/CollisionWorld3D.h"
#include <algorithm>
#include <cmath>

namespace Aevrynd {
namespace {
Vec3 PositiveExtents(Vec3 value){return{std::abs(value.x),std::abs(value.y),std::abs(value.z)};}
bool Overlaps(Vec3 leftCenter,Vec3 leftExtents,Vec3 rightCenter,Vec3 rightExtents){const Vec3 delta=leftCenter-rightCenter;return std::abs(delta.x)<leftExtents.x+rightExtents.x&&std::abs(delta.y)<leftExtents.y+rightExtents.y&&std::abs(delta.z)<leftExtents.z+rightExtents.z;}
Vec3 HitNormal(const Ray3D& ray,const BoxCollider3D& box,float distance){const Vec3 point=ray.origin+ray.direction*distance,delta=point-box.center,extents=PositiveExtents(box.extents);const float x=extents.x>0?std::abs(std::abs(delta.x)-extents.x):0,y=extents.y>0?std::abs(std::abs(delta.y)-extents.y):0,z=extents.z>0?std::abs(std::abs(delta.z)-extents.z):0;if(x<=y&&x<=z)return{delta.x>=0?1.f:-1.f,0,0};if(y<=z)return{0,delta.y>=0?1.f:-1.f,0};return{0,0,delta.z>=0?1.f:-1.f};}
}

ColliderHandle3D CollisionWorld3D::CreateBox(StaticBoxCollider3D collider){collider.bounds.extents=PositiveExtents(collider.bounds.extents);const ColliderHandle3D handle{nextHandle_++};colliders_.emplace(handle.value,collider);return handle;}
bool CollisionWorld3D::UpdateBox(ColliderHandle3D handle,StaticBoxCollider3D collider){auto found=colliders_.find(handle.value);if(found==colliders_.end())return false;collider.bounds.extents=PositiveExtents(collider.bounds.extents);found->second=collider;return true;}
bool CollisionWorld3D::Destroy(ColliderHandle3D handle){return colliders_.erase(handle.value)>0;}
void CollisionWorld3D::Clear(){colliders_.clear();}
ReusableMemoryStats CollisionWorld3D::MemoryStats()const{return{colliders_.bucket_count()*sizeof(void*)+colliders_.size()*sizeof(std::pair<const uint32_t,StaticBoxCollider3D>),lastReleasedBytes_,trimCount_};}
std::size_t CollisionWorld3D::TrimUnusedMemory(){const std::size_t before=MemoryStats().retainedCapacityBytes;if(colliders_.empty())decltype(colliders_){}.swap(colliders_);else colliders_.rehash(colliders_.size());const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;}
const StaticBoxCollider3D* CollisionWorld3D::Get(ColliderHandle3D handle)const{const auto found=colliders_.find(handle.value);return found==colliders_.end()?nullptr:&found->second;}
bool CollisionWorld3D::Matches(const StaticBoxCollider3D& collider,CollisionQueryFilter3D filter)const{return(filter.includeTriggers||!collider.trigger)&&(collider.layer&filter.layerMask)&&(collider.collisionMask&filter.queryLayer);}

CollisionRayHit3D CollisionWorld3D::Raycast(Ray3D ray,float maximumDistance,CollisionQueryFilter3D filter)const{++stats_.queries;ray.direction=Normalize(ray.direction);if(Length(ray.direction)<.0001f)return{};CollisionRayHit3D closest{};const float limit=(std::max)(0.f,maximumDistance);for(const auto& [id,collider]:colliders_){if(!Matches(collider,filter))continue;++stats_.boundsTested;float distance{};if(!RayIntersectsAabb(ray,collider.bounds.center,collider.bounds.extents,(std::min)(limit,closest.distance),&distance))continue;closest={ColliderHandle3D{id},distance,ray.origin+ray.direction*distance,HitNormal(ray,collider.bounds,distance),collider.userData};}if(closest)++stats_.hits;return closest;}
std::vector<ColliderHandle3D> CollisionWorld3D::QueryBox(Vec3 center,Vec3 extents,CollisionQueryFilter3D filter)const{++stats_.queries;extents=PositiveExtents(extents);std::vector<ColliderHandle3D> results;for(const auto& [id,collider]:colliders_){if(!Matches(collider,filter))continue;++stats_.boundsTested;if(Overlaps(center,extents,collider.bounds.center,collider.bounds.extents))results.push_back({id});}stats_.hits+=results.size();return results;}
bool CollisionWorld3D::OverlapsBox(Vec3 center,Vec3 extents,CollisionQueryFilter3D filter)const{++stats_.queries;extents=PositiveExtents(extents);for(const auto& [id,collider]:colliders_){(void)id;if(!Matches(collider,filter))continue;++stats_.boundsTested;if(Overlaps(center,extents,collider.bounds.center,collider.bounds.extents)){++stats_.hits;return true;}}return false;}
void CollisionWorld3D::CollectBoxes(std::vector<BoxCollider3D>& output,CollisionQueryFilter3D filter)const{++stats_.queries;output.clear();output.reserve(colliders_.size());for(const auto& [id,collider]:colliders_){(void)id;if(!Matches(collider,filter))continue;++stats_.boundsTested;output.push_back(collider.bounds);}stats_.hits+=output.size();}
CollisionWorldStats3D CollisionWorld3D::Stats()const{CollisionWorldStats3D result=stats_;result.colliders=colliders_.size();for(const auto& [id,collider]:colliders_){(void)id;if(collider.trigger)++result.triggers;else ++result.solidColliders;}return result;}
}
