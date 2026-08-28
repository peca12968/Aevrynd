#include "Aevrynd/Vegetation3D.h"
#include <algorithm>
#include <cmath>
#include <new>

namespace Aevrynd {
namespace {
bool ValidBounds(MeshBounds3D bounds){return std::isfinite(bounds.center.x)&&std::isfinite(bounds.center.y)&&std::isfinite(bounds.center.z)&&std::isfinite(bounds.radius)&&bounds.radius>=0.f;}
bool ValidTransform(Matrix4 transform){for(const auto& row:transform.values)for(float value:row)if(!std::isfinite(value))return false;return true;}
float SmoothCoverage(float distance,float boundary,float width){const float half=(std::max)(.001f,width)*.5f;return Clamp((boundary+half-distance)/(half*2.f),0.f,1.f);}
}

bool VegetationWorld3D::Build(std::span<const VegetationAsset3D> assets,std::span<const VegetationInstance3D> instances){
    if(instances.size()>MaximumMeshInstancesPerFrame3D)return false;
    try{
    std::vector<VisibilityBvhItem3D> items;items.reserve(instances.size());
    std::vector<VegetationInstance3D> accepted;accepted.reserve(instances.size());
    for(const VegetationInstance3D& instance:instances){
        if(!instance.key||instance.asset>=assets.size()||!ValidBounds(instance.bounds)||!ValidTransform(instance.transform)||!std::isfinite(instance.tint.r)||!std::isfinite(instance.tint.g)||!std::isfinite(instance.tint.b)||!std::isfinite(instance.tint.a))return false;
        const std::uint32_t payload=std::uint32_t(accepted.size());accepted.push_back(instance);items.push_back({instance.key,instance.bounds,payload});
    }
    std::vector<VegetationAsset3D> normalized(assets.begin(),assets.end());
    for(auto& asset:normalized){asset.nearEnd=(std::max)(0.f,std::isfinite(asset.nearEnd)?asset.nearEnd:34.f);asset.farEnd=(std::max)(asset.nearEnd+.001f,std::isfinite(asset.farEnd)?asset.farEnd:120.f);asset.transitionWidth=Clamp(std::isfinite(asset.transitionWidth)?asset.transitionWidth:10.f,.001f,(std::max)(.001f,asset.farEnd-asset.nearEnd));}
    VisibilityBvh3D replacement;if(!replacement.Build(items))return false;
    std::vector<VegetationDraw3D> preparedDraws,preparedTransitions;std::vector<std::size_t> preparedTransitionIndices;preparedDraws.reserve(instances.size());preparedTransitions.reserve(instances.size());preparedTransitionIndices.reserve(instances.size());
    assets_=std::move(normalized);instances_=std::move(accepted);visibility_=std::move(replacement);draws_.swap(preparedDraws);transitionDraws_.swap(preparedTransitions);transitionPrimaryIndices_.swap(preparedTransitionIndices);
    const std::uint64_t builds=stats_.builds+1,refits=stats_.refits,removals=stats_.removals;stats_={};stats_.instances=instances_.size();stats_.builds=builds;stats_.refits=refits;stats_.removals=removals;return true;
    }catch(const std::bad_alloc&){return false;}
}

bool VegetationWorld3D::Update(VisibilityEntityKey3D key,Matrix4 transform,MeshBounds3D bounds){
    if(!key||!ValidBounds(bounds)||!ValidTransform(transform))return false;const auto found=std::find_if(instances_.begin(),instances_.end(),[&](const auto& value){return value.key==key;});if(found==instances_.end())return false;
    if(!visibility_.Update(key,bounds)||!visibility_.Refit())return false;found->transform=transform;found->bounds=bounds;++stats_.refits;return true;
}

bool VegetationWorld3D::Remove(VisibilityEntityKey3D key){
    const auto found=std::find_if(instances_.begin(),instances_.end(),[&](const auto& value){return value.key==key;});if(found==instances_.end())return false;
    try{rebuildItems_.clear();rebuildItems_.reserve(instances_.size()-1);std::vector<VegetationInstance3D> retained;retained.reserve(instances_.size()-1);for(const auto& instance:instances_)if(instance.key!=key){rebuildItems_.push_back({instance.key,instance.bounds,std::uint32_t(retained.size())});retained.push_back(instance);}
    VisibilityBvh3D replacement;if(!replacement.Build(rebuildItems_))return false;instances_=std::move(retained);visibility_=std::move(replacement);stats_.instances=instances_.size();++stats_.removals;return true;}catch(const std::bad_alloc&){return false;}
}

std::span<const VegetationDraw3D> VegetationWorld3D::Query(const VegetationView3D& source){
    draws_.clear();transitionDraws_.clear();transitionPrimaryIndices_.clear();++stats_.queries;stats_.candidates=stats_.submitted=stats_.nearInstances=stats_.farInstances=stats_.impostorInstances=stats_.transitionInstances=stats_.transitionDrawsDropped=stats_.shadowCasters=stats_.frustumRejected=stats_.distanceRejected=stats_.missingAssetRejected=0;
    if(!source.enabled||instances_.empty())return draws_;
    VegetationView3D view=source;view.aspect=(std::max)(.001f,std::isfinite(view.aspect)?view.aspect:1.f);view.maximumDistance=(std::max)(0.f,std::isfinite(view.maximumDistance)?view.maximumDistance:420.f);
    const auto candidates=visibility_.Query({view.camera,view.aspect,view.maximumDistance,0,view.frustumCulling});const auto& visibilityStats=visibility_.Stats();stats_.candidates=candidates.size();stats_.frustumRejected=visibilityStats.frustumRejectedObjects;stats_.distanceRejected=visibilityStats.distanceRejectedObjects;
    const auto append=[&](const VegetationInstance3D& instance,const VegetationAsset3D& asset,VegetationLod3D lod,float coverage,bool invert,bool secondary=false){const MeshHandle3D mesh=asset.meshes[std::size_t(lod)];if(!mesh){++stats_.missingAssetRejected;return;}const bool shadowOwner=secondary?coverage<.5f:coverage>=.5f;VegetationDraw3D draw{mesh,instance.transform,instance.tint,lod,coverage,std::uint32_t(instance.key^(instance.key>>32)),invert,shadowOwner};if(secondary){transitionDraws_.push_back(draw);transitionPrimaryIndices_.push_back(draws_.size()-1);}else draws_.push_back(draw);};
    for(std::uint32_t payload:candidates){const VegetationInstance3D& instance=instances_[payload];const VegetationAsset3D& asset=assets_[instance.asset];const float distance=(std::max)(0.f,Length(instance.bounds.center-view.camera.position)-instance.bounds.radius),half=asset.transitionWidth*.5f;
        if(distance<asset.nearEnd-half)append(instance,asset,VegetationLod3D::Near,1,false);
        else if(distance<=asset.nearEnd+half){const float nearCoverage=SmoothCoverage(distance,asset.nearEnd,asset.transitionWidth);const std::size_t primaryIndex=draws_.size(),secondaryIndex=transitionDraws_.size();append(instance,asset,VegetationLod3D::Near,nearCoverage,false);if(draws_.size()>primaryIndex){append(instance,asset,VegetationLod3D::Far,nearCoverage,true,true);if(transitionDraws_.size()==secondaryIndex){draws_[primaryIndex].lodCoverage=1.f;draws_[primaryIndex].lodInvert=false;draws_[primaryIndex].castsShadow=true;}}++stats_.transitionInstances;}
        else if(distance<asset.farEnd-half)append(instance,asset,VegetationLod3D::Far,1,false);
        else if(distance<=asset.farEnd+half){const float farCoverage=SmoothCoverage(distance,asset.farEnd,asset.transitionWidth);const std::size_t primaryIndex=draws_.size(),secondaryIndex=transitionDraws_.size();append(instance,asset,VegetationLod3D::Far,farCoverage,false);if(draws_.size()>primaryIndex){append(instance,asset,VegetationLod3D::Impostor,farCoverage,true,true);if(transitionDraws_.size()==secondaryIndex){draws_[primaryIndex].lodCoverage=1.f;draws_[primaryIndex].lodInvert=false;draws_[primaryIndex].castsShadow=true;}}++stats_.transitionInstances;}
        else append(instance,asset,VegetationLod3D::Impostor,1,false);
    }
    const std::size_t available=draws_.size()<MaximumMeshInstancesPerFrame3D?MaximumMeshInstancesPerFrame3D-draws_.size():0,secondaryCount=(std::min)(available,transitionDraws_.size());draws_.insert(draws_.end(),transitionDraws_.begin(),transitionDraws_.begin()+secondaryCount);stats_.transitionDrawsDropped=transitionDraws_.size()-secondaryCount;for(std::size_t index=secondaryCount;index<transitionPrimaryIndices_.size();++index){VegetationDraw3D& primary=draws_[transitionPrimaryIndices_[index]];primary.lodCoverage=1.f;primary.lodInvert=false;primary.castsShadow=true;}stats_.submitted=draws_.size();for(const auto& draw:draws_){stats_.shadowCasters+=draw.castsShadow;if(draw.lod==VegetationLod3D::Near)++stats_.nearInstances;else if(draw.lod==VegetationLod3D::Far)++stats_.farInstances;else ++stats_.impostorInstances;}return draws_;
}

void VegetationWorld3D::Clear(){assets_.clear();instances_.clear();draws_.clear();transitionDraws_.clear();transitionPrimaryIndices_.clear();rebuildItems_.clear();visibility_.Clear();stats_={};}
bool VegetationWorld3D::Reserve(std::size_t instances){if(instances>MaximumMeshInstancesPerFrame3D)return false;try{instances_.reserve(instances);draws_.reserve(instances);transitionDraws_.reserve(instances);transitionPrimaryIndices_.reserve(instances);return visibility_.Reserve(instances);}catch(...){return false;}}
std::size_t VegetationWorld3D::TrimUnusedMemory(){const std::size_t before=MemoryStats().retainedCapacityBytes;std::vector<VegetationInstance3D>(instances_).swap(instances_);std::vector<VegetationDraw3D>(draws_).swap(draws_);std::vector<VegetationDraw3D>(transitionDraws_).swap(transitionDraws_);std::vector<std::size_t>(transitionPrimaryIndices_).swap(transitionPrimaryIndices_);std::vector<VegetationAsset3D>(assets_).swap(assets_);std::vector<VisibilityBvhItem3D>(rebuildItems_).swap(rebuildItems_);visibility_.TrimUnusedMemory();const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;}
ReusableMemoryStats VegetationWorld3D::MemoryStats()const{const auto visibility=visibility_.MemoryStats();const std::size_t bytes=assets_.capacity()*sizeof(VegetationAsset3D)+instances_.capacity()*sizeof(VegetationInstance3D)+(draws_.capacity()+transitionDraws_.capacity())*sizeof(VegetationDraw3D)+transitionPrimaryIndices_.capacity()*sizeof(std::size_t)+rebuildItems_.capacity()*sizeof(VisibilityBvhItem3D)+visibility.retainedCapacityBytes;const std::size_t reclaimable=(assets_.capacity()>assets_.size()?assets_.capacity()-assets_.size():0)*sizeof(VegetationAsset3D)+(instances_.capacity()>instances_.size()?instances_.capacity()-instances_.size():0)*sizeof(VegetationInstance3D)+(draws_.capacity()>draws_.size()?draws_.capacity()-draws_.size():0)*sizeof(VegetationDraw3D)+(transitionDraws_.capacity()>transitionDraws_.size()?transitionDraws_.capacity()-transitionDraws_.size():0)*sizeof(VegetationDraw3D)+(transitionPrimaryIndices_.capacity()>transitionPrimaryIndices_.size()?transitionPrimaryIndices_.capacity()-transitionPrimaryIndices_.size():0)*sizeof(std::size_t)+rebuildItems_.capacity()*sizeof(VisibilityBvhItem3D)+visibility.reclaimableCapacityBytes;return{bytes,reclaimable,lastReleasedBytes_,trimCount_};}
}
