#include "Aevrynd/CollisionWorld3D.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Aevrynd {
namespace {
Vec3 PositiveExtents(Vec3 value){return{std::abs(value.x),std::abs(value.y),std::abs(value.z)};}
bool Overlaps(Vec3 leftCenter,Vec3 leftExtents,Vec3 rightCenter,Vec3 rightExtents){const Vec3 delta=leftCenter-rightCenter;return std::abs(delta.x)<leftExtents.x+rightExtents.x&&std::abs(delta.y)<leftExtents.y+rightExtents.y&&std::abs(delta.z)<leftExtents.z+rightExtents.z;}
Vec3 HitNormal(const Ray3D& ray,const BoxCollider3D& box,float distance){const Vec3 point=ray.origin+ray.direction*distance,delta=point-box.center,extents=PositiveExtents(box.extents);const float x=extents.x>0?std::abs(std::abs(delta.x)-extents.x):0,y=extents.y>0?std::abs(std::abs(delta.y)-extents.y):0,z=extents.z>0?std::abs(std::abs(delta.z)-extents.z):0;if(x<=y&&x<=z)return{delta.x>=0?1.f:-1.f,0,0};if(y<=z)return{0,delta.y>=0?1.f:-1.f,0};return{0,0,delta.z>=0?1.f:-1.f};}
bool Finite(Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}
Vec3 ClosestPoint(Vec3 point,Vec3 minimum,Vec3 maximum){return{Clamp(point.x,minimum.x,maximum.x),Clamp(point.y,minimum.y,maximum.y),Clamp(point.z,minimum.z,maximum.z)};}
Vec3 OverlapNormal(Vec3 point,Vec3 minimum,Vec3 maximum,Vec3 displacement){
    const Vec3 closest=ClosestPoint(point,minimum,maximum),separation=point-closest;const double squared=double(separation.x)*separation.x+double(separation.y)*separation.y+double(separation.z)*separation.z;if(std::isfinite(squared)&&squared>0){const double inverseLength=1/std::sqrt(squared);return{float(double(separation.x)*inverseLength),float(double(separation.y)*inverseLength),float(double(separation.z)*inverseLength)};}
    const std::array<float,6> distances{point.x-minimum.x,maximum.x-point.x,point.y-minimum.y,maximum.y-point.y,point.z-minimum.z,maximum.z-point.z};std::size_t face=0;for(std::size_t index=1;index<distances.size();++index)if(distances[index]<distances[face])face=index;
    (void)displacement;return face==0?Vec3{-1,0,0}:face==1?Vec3{1,0,0}:face==2?Vec3{0,-1,0}:face==3?Vec3{0,1,0}:face==4?Vec3{0,0,-1}:Vec3{0,0,1};
}
bool SphereBoxToi(Vec3 start,Vec3 displacement,float radius,const BoxCollider3D& box,double& fraction,Vec3& normal,Vec3& contact,bool& overlapping){
    const Vec3 extents=PositiveExtents(box.extents),minimum=box.center-extents,maximum=box.center+extents,initialClosest=ClosestPoint(start,minimum,maximum),initialDelta=start-initialClosest;const double radiusSquared=double(radius)*radius,initialSquared=double(initialDelta.x)*initialDelta.x+double(initialDelta.y)*initialDelta.y+double(initialDelta.z)*initialDelta.z;
    if(!Finite(extents)||!Finite(minimum)||!Finite(maximum))return false;
    if(initialSquared<=radiusSquared){fraction=0;overlapping=true;normal=OverlapNormal(start,minimum,maximum,displacement);contact=initialClosest;if(initialSquared==0){if(normal.x<-.5f)contact.x=minimum.x;else if(normal.x>.5f)contact.x=maximum.x;else if(normal.y<-.5f)contact.y=minimum.y;else if(normal.y>.5f)contact.y=maximum.y;else if(normal.z<-.5f)contact.z=minimum.z;else contact.z=maximum.z;}return Finite(normal)&&Finite(contact);}
    std::array<double,8> breaks{};std::size_t count=0;breaks[count++]=0;breaks[count++]=1;
    const std::array<double,3> origins{start.x,start.y,start.z},motions{displacement.x,displacement.y,displacement.z},lower{minimum.x,minimum.y,minimum.z},upper{maximum.x,maximum.y,maximum.z};
    for(std::size_t axis=0;axis<3;++axis)if(motions[axis]!=0)for(double plane:{lower[axis],upper[axis]}){const double time=(plane-origins[axis])/motions[axis];if(time>0&&time<1)breaks[count++]=time;}
    std::sort(breaks.begin(),breaks.begin()+count);std::size_t uniqueCount=1;for(std::size_t index=1;index<count;++index)if(breaks[index]!=breaks[uniqueCount-1])breaks[uniqueCount++]=breaks[index];
    double earliest=2;
    for(std::size_t interval=0;interval+1<uniqueCount;++interval){const double lo=breaks[interval],hi=breaks[interval+1],mid=(lo+hi)*.5;double a{},b{},c=-radiusSquared;
        for(std::size_t axis=0;axis<3;++axis){const double sample=origins[axis]+motions[axis]*mid;double slope{},offset{};if(sample<lower[axis]){slope=-motions[axis];offset=lower[axis]-origins[axis];}else if(sample>upper[axis]){slope=motions[axis];offset=origins[axis]-upper[axis];}else continue;a+=slope*slope;b+=2*slope*offset;c+=offset*offset;}
        const auto accept=[&](double value){const double lowerBound=std::nextafter(lo,-std::numeric_limits<double>::infinity()),upperBound=std::nextafter(hi,std::numeric_limits<double>::infinity());if(value>=lowerBound&&value<=upperBound&&value>=0&&value<=1)earliest=(std::min)(earliest,(std::clamp)(value,lo,hi));};
        const double atLo=std::fma(a,lo*lo,std::fma(b,lo,c));if(atLo<=0){accept(lo);break;}if(a==0)continue;const double bb=b*b,fourAc=4*a*c,discriminant=std::fma(b,b,-fourAc),discriminantScale=std::abs(bb)+std::abs(fourAc),discriminantTolerance=std::numeric_limits<double>::epsilon()*8*discriminantScale;if(discriminant< -discriminantTolerance)continue;const double root=std::sqrt((std::max)(0.0,discriminant)),q=-.5*(b+std::copysign(root,b));if(q==0)accept(-b/(2*a));else{accept(q/a);accept(c/q);}if(earliest<=hi)break;
    }
    if(earliest>1)return false;fraction=(std::clamp)(earliest,0.0,1.0);overlapping=false;const Vec3 center=start+displacement*float(fraction);contact=ClosestPoint(center,minimum,maximum);const Vec3 separation=center-contact;const double separationSquared=double(separation.x)*separation.x+double(separation.y)*separation.y+double(separation.z)*separation.z;if(!std::isfinite(separationSquared))return false;if(separationSquared>0){const double inverseLength=1/std::sqrt(separationSquared);normal={float(double(separation.x)*inverseLength),float(double(separation.y)*inverseLength),float(double(separation.z)*inverseLength)};}else normal=OverlapNormal(center,minimum,maximum,displacement);return Finite(normal)&&Finite(contact)&&Finite(center);
}
}

ColliderHandle3D CollisionWorld3D::CreateBox(StaticBoxCollider3D collider){collider.bounds.extents=PositiveExtents(collider.bounds.extents);const ColliderHandle3D handle{nextHandle_++};colliders_.emplace(handle.value,collider);return handle;}
bool CollisionWorld3D::UpdateBox(ColliderHandle3D handle,StaticBoxCollider3D collider){auto found=colliders_.find(handle.value);if(found==colliders_.end())return false;collider.bounds.extents=PositiveExtents(collider.bounds.extents);found->second=collider;return true;}
bool CollisionWorld3D::Destroy(ColliderHandle3D handle){return colliders_.erase(handle.value)>0;}
void CollisionWorld3D::Clear(){colliders_.clear();}
ReusableMemoryStats CollisionWorld3D::MemoryStats()const{const std::size_t bytes=colliders_.bucket_count()*sizeof(void*)+colliders_.size()*sizeof(std::pair<const uint32_t,StaticBoxCollider3D>),retainedBuckets=(std::max)(std::size_t(8),colliders_.size()*2),reclaimable=colliders_.bucket_count()>retainedBuckets?(colliders_.bucket_count()-retainedBuckets)*sizeof(void*):0;return{bytes,reclaimable,lastReleasedBytes_,trimCount_};}
std::size_t CollisionWorld3D::TrimUnusedMemory(){const std::size_t before=MemoryStats().retainedCapacityBytes;if(colliders_.empty())decltype(colliders_){}.swap(colliders_);else colliders_.rehash(colliders_.size());const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;}
const StaticBoxCollider3D* CollisionWorld3D::Get(ColliderHandle3D handle)const{const auto found=colliders_.find(handle.value);return found==colliders_.end()?nullptr:&found->second;}
bool CollisionWorld3D::Matches(const StaticBoxCollider3D& collider,CollisionQueryFilter3D filter)const{return(filter.includeTriggers||!collider.trigger)&&(collider.layer&filter.layerMask)&&(collider.collisionMask&filter.queryLayer);}

CollisionRayHit3D CollisionWorld3D::Raycast(Ray3D ray,float maximumDistance,CollisionQueryFilter3D filter)const{++stats_.queries;ray.direction=Normalize(ray.direction);if(Length(ray.direction)<.0001f)return{};CollisionRayHit3D closest{};const float limit=(std::max)(0.f,maximumDistance);for(const auto& [id,collider]:colliders_){if(!Matches(collider,filter))continue;++stats_.boundsTested;float distance{};if(!RayIntersectsAabb(ray,collider.bounds.center,collider.bounds.extents,(std::min)(limit,closest.distance),&distance))continue;closest={ColliderHandle3D{id},distance,ray.origin+ray.direction*distance,HitNormal(ray,collider.bounds,distance),collider.userData};}if(closest)++stats_.hits;return closest;}
CollisionSphereSweepHit3D CollisionWorld3D::SweepSphere(Vec3 center,float radius,Vec3 displacement,CollisionQueryFilter3D filter)const{
    ++stats_.queries;const float displacementLength=Length(displacement);if(!Finite(center)||!Finite(displacement)||!std::isfinite(radius)||radius<0||!std::isfinite(displacementLength))return{};CollisionSphereSweepHit3D closest{};double closestFraction=std::numeric_limits<double>::infinity();
    for(const auto& [id,collider]:colliders_){if(!Matches(collider,filter)||!Finite(collider.bounds.center)||!Finite(collider.bounds.extents))continue;++stats_.boundsTested;double fraction{};Vec3 normal{},contact{};bool overlapping{};if(!SphereBoxToi(center,displacement,radius,collider.bounds,fraction,normal,contact,overlapping))continue;if(closest&&(fraction>closestFraction||(fraction==closestFraction&&id>=closest.collider.value)))continue;closestFraction=fraction;const float reportedFraction=float(fraction);closest={ColliderHandle3D{id},reportedFraction,displacementLength*reportedFraction,center+displacement*reportedFraction,contact,normal,collider.userData,overlapping};
    }if(closest)++stats_.hits;return closest;
}
std::vector<ColliderHandle3D> CollisionWorld3D::QueryBox(Vec3 center,Vec3 extents,CollisionQueryFilter3D filter)const{++stats_.queries;extents=PositiveExtents(extents);std::vector<ColliderHandle3D> results;for(const auto& [id,collider]:colliders_){if(!Matches(collider,filter))continue;++stats_.boundsTested;if(Overlaps(center,extents,collider.bounds.center,collider.bounds.extents))results.push_back({id});}stats_.hits+=results.size();return results;}
bool CollisionWorld3D::OverlapsBox(Vec3 center,Vec3 extents,CollisionQueryFilter3D filter)const{++stats_.queries;extents=PositiveExtents(extents);for(const auto& [id,collider]:colliders_){(void)id;if(!Matches(collider,filter))continue;++stats_.boundsTested;if(Overlaps(center,extents,collider.bounds.center,collider.bounds.extents)){++stats_.hits;return true;}}return false;}
void CollisionWorld3D::CollectBoxes(std::vector<BoxCollider3D>& output,CollisionQueryFilter3D filter)const{++stats_.queries;output.clear();output.reserve(colliders_.size());for(const auto& [id,collider]:colliders_){(void)id;if(!Matches(collider,filter))continue;++stats_.boundsTested;output.push_back(collider.bounds);}stats_.hits+=output.size();}
CollisionWorldStats3D CollisionWorld3D::Stats()const{CollisionWorldStats3D result=stats_;result.colliders=colliders_.size();for(const auto& [id,collider]:colliders_){(void)id;if(collider.trigger)++result.triggers;else ++result.solidColliders;}return result;}
// 位移前先驗證全部碰撞盒，確保 commit 階段不會留下半套座標。
bool CollisionWorld3D::CanRebaseOrigin(Vec3 delta)const noexcept{if(!std::isfinite(delta.x)||!std::isfinite(delta.y)||!std::isfinite(delta.z))return false;for(const auto& [id,value]:colliders_){(void)id;const Vec3 center=value.bounds.center-delta;if(!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(center.z))return false;}return true;}
void CollisionWorld3D::RebaseOrigin(Vec3 delta)noexcept{for(auto& [id,value]:colliders_){(void)id;value.bounds.center=value.bounds.center-delta;}}
}
