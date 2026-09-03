#pragma once
#include "Camera.h"
#include "Geometry3D.h"
#include "Memory.h"
#include "Mesh3D.h"
#include "VisibilityBvh3D.h"
#include <algorithm>
#include <array>
#include <bit>
#include <iterator>
#include <limits>
#include <new>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Aevrynd {
struct Triangle3D {Vec3 a{},b{},c{};Color color{};};
struct Line3D {Vec3 a{},b{};float width{1.f};Color color{};};
struct Box3D {Transform3D transform{};Material3D material{};RenderObjectIdentity3D identity{};float occlusionOpacity{1.f};};
struct MeshObject3D {MeshResource3D mesh{};Transform3D transform{};Material3D material{};AssetLease3D assetLease{};RenderObjectIdentity3D identity{};float occlusionOpacity{1.f},lodCoverage{1.f};std::uint8_t lodSeed{};bool lodInvert{};};
enum class MeshLodDistanceMetric3D : std::uint8_t {Center,BoundsSurface};
struct MeshLodPolicy3D {float switchDistance{40.f},transitionWidth{};MeshLodDistanceMetric3D distanceMetric{MeshLodDistanceMetric3D::Center};};
struct MeshLodSelection3D {float nearCoverage{1.f},farCoverage{};bool submitNear{true},submitFar{};};
inline MeshLodSelection3D EvaluateMeshLod3D(float centerDistance,float boundsRadius,MeshLodPolicy3D policy){
    if(!std::isfinite(centerDistance)||centerDistance<0.f)return{};const float switchDistance=std::isfinite(policy.switchDistance)?(std::max)(.001f,policy.switchDistance):40.f,width=std::isfinite(policy.transitionWidth)?(std::max)(0.f,policy.transitionWidth):0.f,distance=policy.distanceMetric==MeshLodDistanceMetric3D::BoundsSurface?(std::max)(0.f,centerDistance-(std::isfinite(boundsRadius)?(std::max)(0.f,boundsRadius):0.f)):centerDistance;if(width<=.001f)return distance>=switchDistance?MeshLodSelection3D{0,1,false,true}:MeshLodSelection3D{};const float start=switchDistance-width*.5f,end=switchDistance+width*.5f;if(distance<=start)return{};if(distance>=end)return{0,1,false,true};float blend=Clamp((distance-start)/(std::max)(.001f,end-start),0.f,1.f);blend=blend*blend*(3.f-2.f*blend);return{1.f-blend,blend,true,true};
}
static constexpr std::size_t MaximumMeshLodLevels3D=4;
struct MeshLodLevel3D {MeshResource3D mesh{};Material3D material{};float switchDistance{},transitionWidth{};};
struct MeshLodChainSelection3D {std::size_t primaryLevel{},nearLevel{},farLevel{};float farCoverage{};bool transition{};};
inline MeshLodChainSelection3D EvaluateMeshLodChain3D(float centerDistance,float boundsRadius,std::span<const MeshLodLevel3D> levels,MeshLodDistanceMetric3D metric){
    if(levels.size()<2)return{};for(std::size_t level=0;level+1<levels.size();++level){const MeshLodSelection3D selection=EvaluateMeshLod3D(centerDistance,boundsRadius,{levels[level].switchDistance,levels[level].transitionWidth,metric});if(selection.submitNear){const bool transition=selection.submitFar;return{selection.farCoverage>=selection.nearCoverage?level+1:level,level,level+1,selection.farCoverage,transition};}}const std::size_t last=levels.size()-1;return{last,last,last,1.f,false};
}
struct MeshLodObject3D {std::array<MeshLodLevel3D,MaximumMeshLodLevels3D> levels{};std::size_t levelCount{};MeshLodDistanceMetric3D distanceMetric{MeshLodDistanceMetric3D::Center};Transform3D transform{};MeshBounds3D bounds{};AssetLease3D assetLease{};RenderObjectIdentity3D identity{};float occlusionOpacity{1.f};};
struct ProjectedShadows3D {bool enabled{};float planeHeight{.025f},opacity{.22f},maximumDistance{55.f};std::size_t maximumCasters{256};};
struct SceneVisibility3D {float maximumDistance{},minimumProjectedRadiusPixels{};};
struct Scene3DSubmissionOptions {bool frustumCulling{true};bool gpuInstancing{true};ProjectedShadows3D shadows{};SceneVisibility3D visibility{};std::size_t maximumLodTransitionInstances{512},maximumShadowOnlyInstances{2048};bool linearVisibilityReference{};};
struct Scene3DStats {std::uint64_t submissionSerial{},drawListRevision{},acceptedIdentityOrderFingerprint{};std::size_t acceptedIdentityCount{},inputTriangles{},visibleTriangles{},culledTriangles{},inputLines{},visibleLines{},meshInstances{},visibleMeshInstances{},culledMeshInstances{},distanceCulledMeshInstances{},screenSizeCulledMeshInstances{},lodMeshInstances{},nearLodInstances{},farLodInstances{},lodTrianglesSaved{},lodTransitionInstances{},lodDualSubmittedInstances{},lodTransitionTriangles{},lodTransitionBudgetFallbacks{},shadowCasters{},shadowInstances{},shadowSkipped{},shadowOnlyCandidates{},shadowOnlyInstances{},shadowOnlyBudgetDropped{},shadowDistanceRejected{},cpuFallbackUnavailable{},visibilityCandidates{},visibilityNodesTested{},visibilityNodesRejected{};std::array<std::size_t,MaximumMeshLodLevels3D> selectedLodLevels{};bool canonicalTransformValid{true};};
struct Scene3DCapacity {std::size_t triangles{},lines{},boxes{},meshes{},lodMeshes{};};
enum class SceneObjectType3D {None,Triangle,Box,Mesh};
struct SceneQueryStats3D {std::size_t boundsTested{},boundsRejected{},trianglesTested{},bvhNodesTested{},bvhNodesRejected{},bvhLeavesVisited{},bvhObjectCandidates{},meshBvhNodesTested{},meshBvhNodesRejected{},meshBvhLeavesVisited{},meshTriangleCandidates{};std::uint64_t bvhBuilds{},meshBvhBuilds{};};
struct SceneHit3D {SceneObjectType3D objectType{SceneObjectType3D::None};std::size_t objectIndex{},triangleIndex{};float distance{std::numeric_limits<float>::infinity()};Vec3 position{},normal{},barycentric{};explicit operator bool()const{return objectType!=SceneObjectType3D::None;}};

class Scene3D {
public:
    void Clear(){triangles_.clear();lines_.clear();boxes_.clear();meshes_.clear();lodMeshes_.clear();boxKeys_.clear();meshKeys_.clear();lodMeshKeys_.clear();visibilityLocations_.clear();visibilityIndex_.Clear();visibilityDirty_=true;InvalidateRaycastIndex();}
    bool Reserve(Scene3DCapacity capacity){const auto add=[](std::size_t a,std::size_t b,std::size_t& result){if(b>(std::numeric_limits<std::size_t>::max)()-a)return false;result=a+b;return true;};std::size_t visibilityObjects{},raycastObjects{};if(!add(capacity.boxes,capacity.meshes,visibilityObjects)||!add(visibilityObjects,capacity.lodMeshes,visibilityObjects)||!add(capacity.triangles,visibilityObjects,raycastObjects)||raycastObjects>(std::numeric_limits<std::size_t>::max)()/2+1)return false;try{triangles_.reserve(capacity.triangles);lines_.reserve(capacity.lines);boxes_.reserve(capacity.boxes);meshes_.reserve(capacity.meshes);lodMeshes_.reserve(capacity.lodMeshes);boxKeys_.reserve(capacity.boxes);meshKeys_.reserve(capacity.meshes);lodMeshKeys_.reserve(capacity.lodMeshes);visibilityLocations_.reserve(visibilityObjects);if(!visibilityIndex_.Reserve(visibilityObjects))return false;bvhEntries_.reserve(raycastObjects);bvhNodes_.reserve(raycastObjects>1?raycastObjects*2-1:raycastObjects);bvhTraversal_.reserve(raycastObjects>1?64:raycastObjects);meshBvhTraversal_.reserve(64);retainedCapacity_=capacity;return true;}catch(const std::bad_alloc&){return false;}}
    [[nodiscard]] ReusableMemoryStats MemoryStats()const{
        std::size_t meshIndexBytes{};ForEachUniqueRaycastMesh([&](const Mesh3D& mesh){meshIndexBytes+=mesh.RaycastIndexBytes();});
        const ReusableMemoryStats visibilityMemory=visibilityIndex_.MemoryStats();const std::size_t locationBytes=visibilityLocations_.bucket_count()*sizeof(void*)+visibilityLocations_.size()*(sizeof(std::pair<const VisibilityEntityKey3D,VisibilityLocation>)+sizeof(void*)*2),retained=triangles_.capacity()*sizeof(Triangle3D)+lines_.capacity()*sizeof(Line3D)+boxes_.capacity()*sizeof(Box3D)+meshes_.capacity()*sizeof(MeshObject3D)+lodMeshes_.capacity()*sizeof(MeshLodObject3D)+boxKeys_.capacity()*sizeof(VisibilityEntityKey3D)+meshKeys_.capacity()*sizeof(VisibilityEntityKey3D)+lodMeshKeys_.capacity()*sizeof(VisibilityEntityKey3D)+visibilityBuildScratch_.capacity()*sizeof(VisibilityBvhItem3D)+bvhEntries_.capacity()*sizeof(BvhEntry)+bvhNodes_.capacity()*sizeof(BvhNode)+bvhTraversal_.capacity()*sizeof(BvhTraversalItem)+meshBvhTraversal_.capacity()*sizeof(BvhTraversalItem)+meshIndexBytes+visibilityMemory.retainedCapacityBytes+locationBytes;
        const auto excess=[](std::size_t capacity,std::size_t size,std::size_t baseline,std::size_t element){const std::size_t target=(std::max)(size,baseline);return capacity>target?(capacity-target)*element:std::size_t{};};
        const std::size_t baselineObjects=retainedCapacity_.triangles+retainedCapacity_.boxes+retainedCapacity_.meshes+retainedCapacity_.lodMeshes,currentObjects=triangles_.size()+boxes_.size()+meshes_.size()+lodMeshes_.size(),retainedObjects=(std::max)(baselineObjects,currentObjects),retainedNodes=retainedObjects>1?retainedObjects*2-1:retainedObjects;
        const std::size_t reclaimable=excess(triangles_.capacity(),triangles_.size(),retainedCapacity_.triangles,sizeof(Triangle3D))+excess(lines_.capacity(),lines_.size(),retainedCapacity_.lines,sizeof(Line3D))+excess(boxes_.capacity(),boxes_.size(),retainedCapacity_.boxes,sizeof(Box3D))+excess(meshes_.capacity(),meshes_.size(),retainedCapacity_.meshes,sizeof(MeshObject3D))+excess(lodMeshes_.capacity(),lodMeshes_.size(),retainedCapacity_.lodMeshes,sizeof(MeshLodObject3D))+excess(boxKeys_.capacity(),boxKeys_.size(),retainedCapacity_.boxes,sizeof(VisibilityEntityKey3D))+excess(meshKeys_.capacity(),meshKeys_.size(),retainedCapacity_.meshes,sizeof(VisibilityEntityKey3D))+excess(lodMeshKeys_.capacity(),lodMeshKeys_.size(),retainedCapacity_.lodMeshes,sizeof(VisibilityEntityKey3D))+excess(visibilityBuildScratch_.capacity(),visibilityBuildScratch_.size(),retainedObjects,sizeof(VisibilityBvhItem3D))+excess(bvhEntries_.capacity(),bvhEntries_.size(),retainedObjects,sizeof(BvhEntry))+excess(bvhNodes_.capacity(),bvhNodes_.size(),retainedNodes,sizeof(BvhNode))+excess(bvhTraversal_.capacity(),bvhTraversal_.size(),std::size_t{64},sizeof(BvhTraversalItem))+excess(meshBvhTraversal_.capacity(),meshBvhTraversal_.size(),std::size_t{64},sizeof(BvhTraversalItem))+meshIndexBytes+visibilityMemory.reclaimableCapacityBytes;
        return{retained,reclaimable,lastReleasedBytes_,trimCount_};
    }
    std::size_t TrimUnusedMemory(){
        const std::size_t before=MemoryStats().retainedCapacityBytes;
        auto compact=[]<class T>(std::vector<T>& values,std::size_t baseline){std::vector<T> replacement;replacement.reserve((std::max)(values.size(),baseline));replacement.insert(replacement.end(),std::make_move_iterator(values.begin()),std::make_move_iterator(values.end()));values.swap(replacement);};
        ForEachUniqueRaycastMesh([](const Mesh3D& mesh){mesh.TrimRaycastIndex();});compact(triangles_,retainedCapacity_.triangles);compact(lines_,retainedCapacity_.lines);compact(boxes_,retainedCapacity_.boxes);compact(meshes_,retainedCapacity_.meshes);compact(lodMeshes_,retainedCapacity_.lodMeshes);compact(boxKeys_,retainedCapacity_.boxes);compact(meshKeys_,retainedCapacity_.meshes);compact(lodMeshKeys_,retainedCapacity_.lodMeshes);const std::size_t baselineObjects=retainedCapacity_.triangles+retainedCapacity_.boxes+retainedCapacity_.meshes+retainedCapacity_.lodMeshes,currentObjects=triangles_.size()+boxes_.size()+meshes_.size()+lodMeshes_.size(),retainedObjects=(std::max)(baselineObjects,currentObjects),retainedNodes=retainedObjects>1?retainedObjects*2-1:retainedObjects;compact(visibilityBuildScratch_,retainedObjects);visibilityIndex_.TrimUnusedMemory();std::unordered_map<VisibilityEntityKey3D,VisibilityLocation> compactLocations;compactLocations.reserve((std::max)(retainedCapacity_.boxes+retainedCapacity_.meshes+retainedCapacity_.lodMeshes,visibilityLocations_.size()));compactLocations.insert(visibilityLocations_.begin(),visibilityLocations_.end());visibilityLocations_.swap(compactLocations);compact(bvhEntries_,retainedObjects);compact(bvhNodes_,retainedNodes);bvhTraversal_.clear();compact(bvhTraversal_,64);meshBvhTraversal_.clear();compact(meshBvhTraversal_,64);
        const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;
    }
    void Triangle(Vec3 a,Vec3 b,Vec3 c,Color color){triangles_.push_back({a,b,c,color});InvalidateRaycastIndex();}
    void Line(Vec3 a,Vec3 b,float width,Color color){lines_.push_back({a,b,width,color});}
    VisibilityEntityKey3D Box(Vec3 center,Vec3 size,Color color){return MaterialBox(center,size,Material3D{color});}
    VisibilityEntityKey3D MaterialBox(Vec3 center,Vec3 size,Material3D material,RenderObjectIdentity3D identity={},float occlusionOpacity=1.f){const Transform3D transform{center,{},size};if(!ValidVisibilityBounds(TransformSphereBounds(UnitBoxBounds(),transform))||!ValidOcclusionOpacity3D(occlusionOpacity)||(occlusionOpacity<1.f&&!identity)||(identity.id&&!identity.generation))return 0;const auto key=NextVisibilityKey(),index=boxes_.size();if(!key)return 0;boxes_.push_back({transform,material,identity,occlusionOpacity});try{boxKeys_.push_back(key);visibilityLocations_.emplace(key,VisibilityLocation{VisibilityKind::Box,index});}catch(...){if(boxKeys_.size()>index)boxKeys_.pop_back();boxes_.pop_back();throw;}visibilityDirty_=true;InvalidateRaycastIndex();return key;}
    VisibilityEntityKey3D Mesh(const MeshResource3D& mesh,Transform3D transform,Material3D material={},AssetLease3D assetLease={},RenderObjectIdentity3D identity={},float occlusionOpacity=1.f,float lodCoverage=1.f,std::uint8_t lodSeed=0,bool lodInvert=false){if(!mesh||!ValidVisibilityBounds(TransformSphereBounds(mesh.bounds,transform))||!ValidOcclusionOpacity3D(occlusionOpacity)||(occlusionOpacity<1.f&&!identity)||(identity.id&&!identity.generation)||!std::isfinite(lodCoverage)||lodCoverage<0.f||lodCoverage>1.f||lodSeed>7)return 0;const auto key=NextVisibilityKey(),index=meshes_.size();if(!key)return 0;meshes_.push_back({mesh,transform,material,std::move(assetLease),identity,occlusionOpacity,lodCoverage,lodSeed,lodInvert});try{meshKeys_.push_back(key);visibilityLocations_.emplace(key,VisibilityLocation{VisibilityKind::Mesh,index});}catch(...){if(meshKeys_.size()>index)meshKeys_.pop_back();meshes_.pop_back();throw;}visibilityDirty_=true;InvalidateRaycastIndex();return key;}
    bool UpdateTransform(VisibilityEntityKey3D key,Transform3D transform){const auto found=visibilityLocations_.find(key);if(found==visibilityLocations_.end())return false;MeshBounds3D bounds{};switch(found->second.kind){case VisibilityKind::Box:bounds=TransformSphereBounds(UnitBoxBounds(),transform);break;case VisibilityKind::Mesh:bounds=TransformSphereBounds(meshes_[found->second.index].mesh.bounds,transform);break;case VisibilityKind::Lod:bounds=TransformSphereBounds(lodMeshes_[found->second.index].bounds,transform);break;}if(!ValidVisibilityBounds(bounds))return false;if(!visibilityDirty_&&!visibilityIndex_.Update(key,bounds))return false;switch(found->second.kind){case VisibilityKind::Box:boxes_[found->second.index].transform=transform;break;case VisibilityKind::Mesh:meshes_[found->second.index].transform=transform;break;case VisibilityKind::Lod:lodMeshes_[found->second.index].transform=transform;break;}visibilityNeedsRefit_|=!visibilityDirty_;InvalidateRaycastIndex();return true;}
    bool UpdateMeshTransform(VisibilityEntityKey3D key,Transform3D transform){const auto found=visibilityLocations_.find(key);return found!=visibilityLocations_.end()&&found->second.kind==VisibilityKind::Mesh&&UpdateTransform(key,transform);}
    bool Remove(VisibilityEntityKey3D key){const auto found=visibilityLocations_.find(key);if(found==visibilityLocations_.end())return false;const VisibilityLocation location=found->second;const auto swapRemove=[&](auto& values,auto& keys){const std::size_t last=values.size()-1;if(location.index!=last){values[location.index]=std::move(values[last]);keys[location.index]=keys[last];visibilityLocations_[keys[location.index]].index=location.index;}values.pop_back();keys.pop_back();};switch(location.kind){case VisibilityKind::Box:swapRemove(boxes_,boxKeys_);break;case VisibilityKind::Mesh:swapRemove(meshes_,meshKeys_);break;case VisibilityKind::Lod:swapRemove(lodMeshes_,lodMeshKeys_);break;}visibilityLocations_.erase(found);visibilityDirty_=true;visibilityNeedsRefit_=false;InvalidateRaycastIndex();return true;}
    void MeshLod(const MeshResource3D& nearMesh,const MeshResource3D& farMesh,float switchDistance,Transform3D transform,Material3D material={}){MeshLod(nearMesh,farMesh,MeshLodPolicy3D{switchDistance},transform,material,material);}
    void MeshLod(const MeshResource3D& nearMesh,const MeshResource3D& farMesh,float switchDistance,Transform3D transform,Material3D nearMaterial,Material3D farMaterial){MeshLod(nearMesh,farMesh,MeshLodPolicy3D{switchDistance},transform,nearMaterial,farMaterial);}
    void MeshLod(const MeshResource3D& nearMesh,const MeshResource3D& farMesh,MeshLodPolicy3D policy,Transform3D transform,Material3D material={}){MeshLod(nearMesh,farMesh,policy,transform,material,material);}
    void MeshLod(const MeshResource3D& nearMesh,const MeshResource3D& farMesh,MeshLodPolicy3D policy,Transform3D transform,Material3D nearMaterial,Material3D farMaterial,AssetLease3D assetLease={},RenderObjectIdentity3D identity={},float occlusionOpacity=1.f){if(!std::isfinite(policy.switchDistance)||policy.switchDistance<=0.f)policy.switchDistance=40.f;if(!std::isfinite(policy.transitionWidth)||policy.transitionWidth<0.f)policy.transitionWidth=0.f;const std::array<MeshLodLevel3D,2> levels{{{nearMesh,nearMaterial,policy.switchDistance,policy.transitionWidth},{farMesh,farMaterial}}};MeshLodChain(levels,policy.distanceMetric,transform,std::move(assetLease),identity,occlusionOpacity);}
    VisibilityEntityKey3D MeshLodChain(std::span<const MeshLodLevel3D> levels,MeshLodDistanceMetric3D distanceMetric,Transform3D transform,AssetLease3D assetLease={},RenderObjectIdentity3D identity={},float occlusionOpacity=1.f){
        if(!ValidOcclusionOpacity3D(occlusionOpacity)||(occlusionOpacity<1.f&&!identity)||(identity.id&&!identity.generation))return 0;
        if(levels.size()<2||levels.size()>MaximumMeshLodLevels3D||(distanceMetric!=MeshLodDistanceMetric3D::Center&&distanceMetric!=MeshLodDistanceMetric3D::BoundsSurface))return 0;float previousSwitch{},previousEnd=-std::numeric_limits<float>::infinity();std::size_t previousTriangles=(std::numeric_limits<std::size_t>::max)();for(std::size_t level=0;level<levels.size();++level){const MeshLodLevel3D& value=levels[level];if(!value.mesh||value.mesh.TriangleCount()>previousTriangles)return 0;previousTriangles=value.mesh.TriangleCount();if(level+1<levels.size()){if(!std::isfinite(value.switchDistance)||value.switchDistance<=previousSwitch||!std::isfinite(value.transitionWidth)||value.transitionWidth<0.f)return 0;const float halfWidth=value.transitionWidth*.5f,start=value.switchDistance-halfWidth,end=value.switchDistance+halfWidth;if(!std::isfinite(halfWidth)||!std::isfinite(start)||!std::isfinite(end)||start<previousEnd)return 0;previousSwitch=value.switchDistance;previousEnd=end;}}
        MeshLodObject3D object{};object.identity=identity;object.occlusionOpacity=occlusionOpacity;object.levelCount=levels.size();object.distanceMetric=distanceMetric;object.transform=transform;object.assetLease=std::move(assetLease);for(std::size_t level=0;level<levels.size();++level)object.levels[level]=levels[level];Vec3 minimum=levels[0].mesh.bounds.center-levels[0].mesh.bounds.extents,maximum=levels[0].mesh.bounds.center+levels[0].mesh.bounds.extents;for(std::size_t level=1;level<levels.size();++level){const MeshBounds3D& bounds=levels[level].mesh.bounds;const Vec3 levelMinimum=bounds.center-bounds.extents,levelMaximum=bounds.center+bounds.extents;minimum.x=(std::min)(minimum.x,levelMinimum.x);minimum.y=(std::min)(minimum.y,levelMinimum.y);minimum.z=(std::min)(minimum.z,levelMinimum.z);maximum.x=(std::max)(maximum.x,levelMaximum.x);maximum.y=(std::max)(maximum.y,levelMaximum.y);maximum.z=(std::max)(maximum.z,levelMaximum.z);}object.bounds.center=(minimum+maximum)*.5f;object.bounds.extents=(maximum-minimum)*.5f;object.bounds.radius=Length(object.bounds.extents);if(!ValidVisibilityBounds(TransformSphereBounds(object.bounds,transform)))return 0;const auto key=NextVisibilityKey(),index=lodMeshes_.size();if(!key)return 0;lodMeshes_.push_back(std::move(object));try{lodMeshKeys_.push_back(key);visibilityLocations_.emplace(key,VisibilityLocation{VisibilityKind::Lod,index});}catch(...){if(lodMeshKeys_.size()>index)lodMeshKeys_.pop_back();lodMeshes_.pop_back();throw;}visibilityDirty_=true;InvalidateRaycastIndex();return key;
    }
    ObjectOcclusionOpacityUpdate3D SetObjectOcclusionOpacity(RenderObjectIdentity3D identity,float opacity){ObjectOcclusionOpacityUpdate3D result{};if(!identity||!ValidOcclusionOpacity3D(opacity))return result;const auto countConflicts=[&](const auto& values){for(const auto& value:values)if(value.identity.id==identity.id&&value.identity.generation!=identity.generation)++result.generationConflicts;};countConflicts(boxes_);countConflicts(meshes_);countConflicts(lodMeshes_);if(result.generationConflicts)return result;const auto update=[&](auto& values){for(auto& value:values)if(value.identity==identity){value.occlusionOpacity=opacity;++result.matchedStatic;}};update(boxes_);update(meshes_);update(lodMeshes_);result.matchedMultipart=result.matchedStatic>1?result.matchedStatic:0;result.missing=result.matchedStatic==0;return result;}
    void SetBoxMesh(MeshResource3D mesh){boxMesh_=std::move(mesh);}
    const MeshResource3D& BoxMesh()const{return boxMesh_;}

    SceneHit3D Raycast(Ray3D ray,float maximumDistance=std::numeric_limits<float>::infinity(),SceneQueryStats3D* queryStats=nullptr,bool cullBackfaces=false)const{
        SceneQueryStats3D stats{};SceneHit3D closest{};ray.direction=Normalize(ray.direction);if(Length(ray.direction)<.0001f){if(queryStats)*queryStats=stats;return closest;}const float limit=(std::max)(0.f,maximumDistance);if(bvhDirty_){BuildRaycastIndex();stats.bvhBuilds=1;}if(bvhNodes_.empty()){if(queryStats)*queryStats=stats;return closest;}
        auto testTriangle=[&](Vec3 a,Vec3 b,Vec3 c,SceneObjectType3D type,std::size_t objectIndex,std::size_t triangleIndex){++stats.trianglesTested;const RayTriangleHit3D hit=RayIntersectsTriangle(ray,a,b,c,(std::min)(limit,closest.distance),cullBackfaces);if(!hit)return;closest={type,objectIndex,triangleIndex,hit.distance,ray.origin+ray.direction*hit.distance,hit.normal,hit.barycentric};};
        auto testMesh=[&](const Mesh3D& mesh,const MeshBounds3D& localBounds,const Transform3D& transform,SceneObjectType3D type,std::size_t objectIndex){
            ++stats.boundsTested;const MeshBounds3D bounds=TransformBounds(localBounds,transform);if(!RayIntersectsAabb(ray,bounds.center,bounds.extents,(std::min)(limit,closest.distance))){++stats.boundsRejected;return;}
            bool built{};const auto index=GetMeshRaycastIndex(mesh,built);if(built)++stats.meshBvhBuilds;if(!index||index->nodes.empty())return;const Matrix4 matrix=transform.Matrix();
            const auto worldBounds=[&](const MeshRaycastNode3D& node){const Vec3& e=node.extents;return MeshBounds3D{TransformPoint(matrix,node.center),0,{std::abs(matrix[0][0])*e.x+std::abs(matrix[0][1])*e.y+std::abs(matrix[0][2])*e.z,std::abs(matrix[1][0])*e.x+std::abs(matrix[1][1])*e.y+std::abs(matrix[1][2])*e.z,std::abs(matrix[2][0])*e.x+std::abs(matrix[2][1])*e.y+std::abs(matrix[2][2])*e.z}};};
            meshBvhTraversal_.clear();float rootDistance{};const MeshBounds3D rootBounds=worldBounds(index->nodes.front());++stats.meshBvhNodesTested;if(RayIntersectsAabb(ray,rootBounds.center,rootBounds.extents,(std::min)(limit,closest.distance),&rootDistance))meshBvhTraversal_.push_back({0,rootDistance});else{++stats.meshBvhNodesRejected;return;}
            while(!meshBvhTraversal_.empty()){const BvhTraversalItem item=meshBvhTraversal_.back();meshBvhTraversal_.pop_back();if(item.distance>(std::min)(limit,closest.distance))continue;const MeshRaycastNode3D& node=index->nodes[item.node];if(node.count){++stats.meshBvhLeavesVisited;for(std::uint32_t offset=0;offset<node.count;++offset){const std::size_t triangleIndex=index->entries[node.first+offset].triangleIndex,first=triangleIndex*3;++stats.meshTriangleCandidates;const auto point=[&](std::uint32_t vertexIndex){const Vertex3D& vertex=mesh.vertices[vertexIndex];return TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});};testTriangle(point(mesh.indices[first]),point(mesh.indices[first+1]),point(mesh.indices[first+2]),type,objectIndex,triangleIndex);}continue;}
                float leftDistance{},rightDistance{};const float closestLimit=(std::min)(limit,closest.distance);const MeshBounds3D leftBounds=worldBounds(index->nodes[node.left]),rightBounds=worldBounds(index->nodes[node.right]);++stats.meshBvhNodesTested;const bool hitLeft=RayIntersectsAabb(ray,leftBounds.center,leftBounds.extents,closestLimit,&leftDistance);if(!hitLeft)++stats.meshBvhNodesRejected;++stats.meshBvhNodesTested;const bool hitRight=RayIntersectsAabb(ray,rightBounds.center,rightBounds.extents,closestLimit,&rightDistance);if(!hitRight)++stats.meshBvhNodesRejected;if(hitLeft&&hitRight){if(leftDistance<rightDistance){meshBvhTraversal_.push_back({node.right,rightDistance});meshBvhTraversal_.push_back({node.left,leftDistance});}else{meshBvhTraversal_.push_back({node.left,leftDistance});meshBvhTraversal_.push_back({node.right,rightDistance});}}else if(hitLeft)meshBvhTraversal_.push_back({node.left,leftDistance});else if(hitRight)meshBvhTraversal_.push_back({node.right,rightDistance});}
        };
        bvhTraversal_.clear();float rootDistance{};++stats.bvhNodesTested;if(RayIntersectsAabb(ray,bvhNodes_.front().center,bvhNodes_.front().extents,limit,&rootDistance))bvhTraversal_.push_back({0,rootDistance});else{++stats.bvhNodesRejected;++stats.boundsTested;++stats.boundsRejected;}
        while(!bvhTraversal_.empty()){const BvhTraversalItem item=bvhTraversal_.back();bvhTraversal_.pop_back();if(item.distance>(std::min)(limit,closest.distance))continue;const BvhNode& node=bvhNodes_[item.node];if(node.count){++stats.bvhLeavesVisited;for(std::uint32_t offset=0;offset<node.count;++offset){const BvhEntry& entry=bvhEntries_[node.first+offset];++stats.bvhObjectCandidates;switch(entry.kind){case BvhEntryKind::Triangle:{const Triangle3D& triangle=triangles_[entry.objectIndex];testTriangle(triangle.a,triangle.b,triangle.c,SceneObjectType3D::Triangle,entry.objectIndex,0);break;}case BvhEntryKind::Box:{const Box3D& box=boxes_[entry.objectIndex];testMesh(UnitBox(),UnitBoxBounds(),box.transform,SceneObjectType3D::Box,entry.objectIndex);break;}case BvhEntryKind::Mesh:{const MeshObject3D& mesh=meshes_[entry.objectIndex];testMesh(*mesh.mesh.cpuMesh,mesh.mesh.bounds,mesh.transform,SceneObjectType3D::Mesh,entry.objectIndex);break;}case BvhEntryKind::LodMesh:{const MeshLodObject3D& mesh=lodMeshes_[entry.objectIndex];const MeshLodLevel3D& level=mesh.levels[0];testMesh(*level.mesh.cpuMesh,level.mesh.bounds,mesh.transform,SceneObjectType3D::Mesh,meshes_.size()+entry.objectIndex);break;}}}continue;}float leftDistance{},rightDistance{};const float closestLimit=(std::min)(limit,closest.distance);++stats.bvhNodesTested;const bool hitLeft=RayIntersectsAabb(ray,bvhNodes_[node.left].center,bvhNodes_[node.left].extents,closestLimit,&leftDistance);if(!hitLeft)++stats.bvhNodesRejected;++stats.bvhNodesTested;const bool hitRight=RayIntersectsAabb(ray,bvhNodes_[node.right].center,bvhNodes_[node.right].extents,closestLimit,&rightDistance);if(!hitRight)++stats.bvhNodesRejected;if(hitLeft&&hitRight){if(leftDistance<rightDistance){bvhTraversal_.push_back({node.right,rightDistance});bvhTraversal_.push_back({node.left,leftDistance});}else{bvhTraversal_.push_back({node.left,leftDistance});bvhTraversal_.push_back({node.right,rightDistance});}}else if(hitLeft)bvhTraversal_.push_back({node.left,leftDistance});else if(hitRight)bvhTraversal_.push_back({node.right,rightDistance});}
        if(queryStats)*queryStats=stats;return closest;
    }

    // CPU 路徑保留作為投影與排序的參考實作，方便對照硬體深度結果。
    Scene3DStats Render(DrawList& draw,const Camera3D& camera,Vec2 viewport)const{
        struct VisibleTriangle {ProjectedPoint a,b,c;float depth{};Color color{};};
        std::vector<VisibleTriangle> visible;
        visible.reserve(TriangleCount());
        ForEachTriangle([&](Vec3 a,Vec3 b,Vec3 c,Color color){ProjectedPoint pa=ProjectToViewport(a,camera,viewport),pb=ProjectToViewport(b,camera,viewport),pc=ProjectToViewport(c,camera,viewport);if(pa.visible&&pb.visible&&pc.visible)visible.push_back({pa,pb,pc,(pa.depth+pb.depth+pc.depth)/3.f,color});});
        std::stable_sort(visible.begin(),visible.end(),[](const auto& left,const auto& right){return left.depth>right.depth;});
        for(const auto& triangle:visible)draw.Triangle(triangle.a.position,triangle.b.position,triangle.c.position,triangle.color);
        Scene3DStats stats{};stats.inputTriangles=TriangleCount();stats.visibleTriangles=visible.size();stats.culledTriangles=stats.inputTriangles-stats.visibleTriangles;stats.inputLines=lines_.size();stats.meshInstances=MeshCount();
        for(const auto& line:lines_){ProjectedPoint a=ProjectToViewport(line.a,camera,viewport),b=ProjectToViewport(line.b,camera,viewport);if(!a.visible||!b.visible)continue;draw.Line(a.position,b.position,line.width,line.color);++stats.visibleLines;}
        return stats;
    }

    // 網格與線段合併成單一索引批次，由 GPU 深度測試處理所有遮擋。
    Scene3DStats Submit(DrawList &draw, const Camera3D &camera, Vec2 viewport,
                        Scene3DSubmissionOptions options = {}) const {
      const std::size_t meshSubmissionBegin=draw.meshInstances.size(),worldVertexBegin=draw.worldVertices.size(),worldIndexBegin=draw.worldIndices.size();
      draw.SetViewProjection(CreateViewProjectionMatrix(
          camera, viewport.x / (std::max)(1.f, viewport.y)));
      for (const auto &triangle : triangles_)
        draw.WorldTriangle(triangle.a, triangle.b, triangle.c, triangle.color);
      Scene3DStats stats{};
      stats.submissionSerial=++submissionSerial_;if(!stats.submissionSerial)stats.submissionSerial=++submissionSerial_;
      const auto finiteTransform=[](const Transform3D& value){return std::isfinite(value.position.x)&&std::isfinite(value.position.y)&&std::isfinite(value.position.z)&&std::isfinite(value.rotation.x)&&std::isfinite(value.rotation.y)&&std::isfinite(value.rotation.z)&&std::isfinite(value.scale.x)&&std::isfinite(value.scale.y)&&std::isfinite(value.scale.z);};
      stats.canonicalTransformValid=std::ranges::all_of(boxes_,finiteTransform,&Box3D::transform)&&std::ranges::all_of(meshes_,finiteTransform,&MeshObject3D::transform)&&std::ranges::all_of(lodMeshes_,finiteTransform,&MeshLodObject3D::transform);
      stats.inputTriangles = TriangleCount();
      stats.visibleTriangles = triangles_.size();
      stats.inputLines = lines_.size();
      stats.meshInstances = MeshCount();
      const float aspect = viewport.x / (std::max)(1.f, viewport.y);
      const Mesh3D &fallbackBox = UnitBox();
      const Vec3 shadowDirection = draw.DirectionalLight().direction;
      const bool validShadowDirection = std::abs(shadowDirection.y) > .05f;
      const Matrix4 shadowProjection = CreateDirectionalShadowMatrix(
                        shadowDirection, options.shadows.planeHeight),
                    viewMatrix = CreateViewMatrix(camera);
      const float projectionScalePixels =
          camera.projection.mode == ProjectionMode::Orthographic
              ? viewport.y /
                    (std::max)(.001f, camera.projection.orthographicHeight)
              : viewport.y * .5f /
                    std::tan(Clamp(camera.projection.verticalFieldOfView, .01f,
                                   3.13f) *
                             .5f);
      auto visible = [&](MeshBounds3D worldBounds) {
        const float distanceToCamera =
            Length(worldBounds.center - camera.position);
        if (options.visibility.maximumDistance > 0.f &&
            distanceToCamera - worldBounds.radius >
                options.visibility.maximumDistance) {
          ++stats.culledMeshInstances;
          ++stats.distanceCulledMeshInstances;
          return false;
        }
        if (options.frustumCulling &&
            !SphereIntersectsFrustum(worldBounds.center, worldBounds.radius,
                                     camera, aspect)) {
          ++stats.culledMeshInstances;
          return false;
        }
        if (options.visibility.minimumProjectedRadiusPixels > 0.f) {
          const float viewDepth =
              camera.projection.mode == ProjectionMode::Orthographic
                  ? 1.f
                  : TransformPoint(viewMatrix, worldBounds.center).z;
          // 球體跨越相機平面時中心可在相機後方，但可見表面仍可能覆蓋整個畫面；
          // 此時不得以中心深度為零把大型地形誤降為僅陰影物件。
          const float projectedRadius =
              camera.projection.mode == ProjectionMode::Orthographic
                  ? worldBounds.radius * projectionScalePixels
                  : viewDepth + worldBounds.radius <= .001f
                        ? 0.f
                        : viewDepth - worldBounds.radius <= .001f
                              ? (std::numeric_limits<float>::infinity)()
                              : worldBounds.radius * projectionScalePixels /
                                    viewDepth;
          if (projectedRadius <
              options.visibility.minimumProjectedRadiusPixels) {
            ++stats.culledMeshInstances;
            ++stats.screenSizeCulledMeshInstances;
            return false;
          }
        }
        return true;
      };
      const auto shadowEligible = [&](MeshBounds3D worldBounds,
                                      Material3D material,
                                      const MeshResource3D &resource) {
        if (!options.shadows.enabled || !draw.DirectionalShadowMapEnabled() ||
            !options.gpuInstancing || !resource || !material.castsShadow ||
            material.materialClass == MaterialClass3D::Water)
          return false;
        return Length(worldBounds.center - camera.position) -
                   worldBounds.radius <=
               options.shadows.maximumDistance;
      };
      auto submit = [&](const MeshResource3D &resource, const Mesh3D *fallback,
                        const MeshBounds3D *fallbackBounds,
                        const Transform3D &transform, Material3D material,
                        float lodCoverage = 1.f, std::uint32_t lodSeed = 0,
                        bool allowShadow = true,
                        bool visibilityAccepted = false,
                        bool lodInvert = false,
                        MeshRenderLayer3D layer = MeshRenderLayer3D::Opaque,
                        AssetLease3D assetLease = {},RenderObjectIdentity3D identity={},float occlusionOpacity=1.f) {
        const MeshBounds3D localBounds =
                               resource ? resource.bounds : *fallbackBounds,
                           worldBounds =
                               TransformSphereBounds(localBounds, transform);
        const std::size_t triangles =
            resource ? resource.TriangleCount() : fallback->indices.size() / 3;
        const float distanceToCamera =
            Length(worldBounds.center - camera.position);
        if (layer != MeshRenderLayer3D::ShadowOnly && !visibilityAccepted &&
            !visible(worldBounds))
          return false;
        const bool castsShadow = material.castsShadow && allowShadow;
        if (options.shadows.enabled && !draw.DirectionalShadowMapEnabled() &&
            castsShadow && resource) {
          ++stats.shadowCasters;
          if (validShadowDirection &&
              stats.shadowInstances < options.shadows.maximumCasters &&
              distanceToCamera <= options.shadows.maximumDistance) {
            draw.MeshInstance(resource.handle,
                              shadowProjection * transform.Matrix(),
                              {0, 0, 0, options.shadows.opacity}, {1, 1, 0, 1},
                              MeshRenderLayer3D::Overlay);
            ++stats.shadowInstances;
          } else
            ++stats.shadowSkipped;
        }
        if (options.gpuInstancing && resource)
          draw.MeshInstance(resource.handle, transform.Matrix(), material.tint,
                            material.surface, layer,
                            material.textures, material.textureScale,
                            material.useUv, material.materialClass,
                            std::numeric_limits<float>::quiet_NaN(),
                            castsShadow, lodCoverage, lodSeed, lodInvert,
                            DirectionalShadowAllCascades3D,
                            ShadowCascadeSelection3D::Automatic,
                            Matrix4::Identity(),false,material.normalScale,
                            material.alphaMode,material.alphaCutoff,
                            material.doubleSided,identity,occlusionOpacity);
        else if (resource.cpuMesh)
          SubmitMesh(draw, *resource.cpuMesh, transform, material);
        else if (fallback)
          SubmitMesh(draw, *fallback, transform, material);
        else {
          ++stats.cpuFallbackUnavailable;
          return false;
        }
        draw.RetainAssetLease(std::move(assetLease));
        if (layer == MeshRenderLayer3D::ShadowOnly)
          ++stats.shadowOnlyInstances;
        else {
          ++stats.visibleMeshInstances;
          stats.visibleTriangles += triangles;
        }
        return true;
      };
      const auto submitObject = [&](const MeshResource3D &resource,
                                    const Mesh3D *fallback,
                                    const MeshBounds3D *fallbackBounds,
                                    const Transform3D &transform,
                                    Material3D material,
                                    AssetLease3D assetLease = {},RenderObjectIdentity3D identity={},float occlusionOpacity=1.f,float lodCoverage=1.f,std::uint8_t lodSeed=0,bool lodInvert=false) {
        const MeshBounds3D localBounds =
                               resource ? resource.bounds : *fallbackBounds,
                           worldBounds =
                               TransformSphereBounds(localBounds, transform);
        if (visible(worldBounds)) {
          const bool withinShadowDistance =
              !options.shadows.enabled || !draw.DirectionalShadowMapEnabled() ||
              Length(worldBounds.center - camera.position) -
                      worldBounds.radius <=
                  options.shadows.maximumDistance;
          if (material.castsShadow && !withinShadowDistance)
            ++stats.shadowDistanceRejected;
          submit(resource, fallback, fallbackBounds, transform, material, lodCoverage,
                 lodSeed, withinShadowDistance, true, lodInvert,
                 MeshRenderLayer3D::Opaque, std::move(assetLease),identity,occlusionOpacity);
          return;
        }
        if (!shadowEligible(worldBounds, material, resource))
          return;
        ++stats.shadowOnlyCandidates;
        if (stats.shadowOnlyInstances >= options.maximumShadowOnlyInstances) {
          ++stats.shadowOnlyBudgetDropped;
          return;
        }
        submit(resource, fallback, fallbackBounds, transform, material, lodCoverage, lodSeed,
               true, true, lodInvert, MeshRenderLayer3D::ShadowOnly,
               std::move(assetLease),identity,occlusionOpacity);
      };
      const auto submitVisibilityObject = [&](std::size_t objectIndex) {
        if(objectIndex<boxes_.size()){const Box3D& box=boxes_[objectIndex];submitObject(boxMesh_,&fallbackBox,&UnitBoxBounds(),box.transform,box.material,{},box.identity,box.occlusionOpacity);return;}
        objectIndex-=boxes_.size();if(objectIndex<meshes_.size()){const MeshObject3D& mesh=meshes_[objectIndex];submitObject(mesh.mesh,mesh.mesh.cpuMesh.get(),nullptr,mesh.transform,mesh.material,mesh.assetLease,mesh.identity,mesh.occlusionOpacity,mesh.lodCoverage,mesh.lodSeed,mesh.lodInvert);return;}
        const std::size_t lodIndex=objectIndex-meshes_.size();if(lodIndex>=lodMeshes_.size())return;
        const MeshLodObject3D &lod = lodMeshes_[lodIndex];
        ++stats.lodMeshInstances;
        const MeshBounds3D worldBounds =
            TransformSphereBounds(lod.bounds, lod.transform);
        MeshLodChainSelection3D selection = EvaluateMeshLodChain3D(
            Length(worldBounds.center - camera.position), worldBounds.radius,
            std::span<const MeshLodLevel3D>{lod.levels.data(), lod.levelCount},
            lod.distanceMetric);
        const MeshLodLevel3D &nearLevel = lod.levels[selection.nearLevel];
        const MeshLodLevel3D &farLevel = lod.levels[selection.farLevel];
        const bool primaryFar = selection.primaryLevel > 0,
                   transparent = nearLevel.material.materialClass ==
                                     MaterialClass3D::Water ||
                                 farLevel.material.materialClass ==
                                     MaterialClass3D::Water;
        bool transition =
            selection.transition && options.gpuInstancing && !transparent;
        if (!visible(worldBounds)) {
          const MeshLodLevel3D &selected = lod.levels[selection.primaryLevel];
          if (shadowEligible(worldBounds, selected.material, selected.mesh)) {
            ++stats.shadowOnlyCandidates;
            if (stats.shadowOnlyInstances <
                options.maximumShadowOnlyInstances)
              submit(selected.mesh, selected.mesh.cpuMesh.get(), nullptr,
                     lod.transform, selected.material, 1.f, 0, true, true,
                     false, MeshRenderLayer3D::ShadowOnly, lod.assetLease,lod.identity,lod.occlusionOpacity);
            else
              ++stats.shadowOnlyBudgetDropped;
          }
          return;
        }
        if (primaryFar)
          ++stats.farLodInstances;
        else
          ++stats.nearLodInstances;
        ++stats.selectedLodLevels[selection.primaryLevel];
        if (transition &&
            (stats.lodTransitionInstances >=
                 options.maximumLodTransitionInstances ||
             draw.meshInstances.size() > MaximumMeshInstancesPerFrame3D - 4))
          transition = false;
        if (selection.transition && !transition) {
          ++stats.lodTransitionBudgetFallbacks;
        }
        if (transition) {
          const bool withinShadowDistance =
              !options.shadows.enabled || !draw.DirectionalShadowMapEnabled() ||
              Length(worldBounds.center - camera.position) -
                      worldBounds.radius <=
                  options.shadows.maximumDistance;
          if ((selection.primaryLevel == selection.nearLevel
                   ? nearLevel.material.castsShadow
                   : farLevel.material.castsShadow) &&
              !withinShadowDistance)
            ++stats.shadowDistanceRejected;
          ++stats.lodTransitionInstances;
          ++stats.lodDualSubmittedInstances;
          stats.lodTransitionTriangles +=
              nearLevel.mesh.TriangleCount() + farLevel.mesh.TriangleCount();
          const std::uint32_t transitionCode = std::uint32_t(
              (lodIndex * 5u + selection.nearLevel * 3u + 3u) & 7u);
          submit(nearLevel.mesh, nearLevel.mesh.cpuMesh.get(), nullptr,
                 lod.transform, nearLevel.material, selection.farCoverage,
                 transitionCode,
                 withinShadowDistance &&
                     selection.primaryLevel == selection.nearLevel,
                 true, true, MeshRenderLayer3D::Opaque, lod.assetLease,lod.identity,lod.occlusionOpacity);
          submit(farLevel.mesh, farLevel.mesh.cpuMesh.get(), nullptr,
                 lod.transform, farLevel.material, selection.farCoverage,
                 transitionCode,
                 withinShadowDistance &&
                     selection.primaryLevel == selection.farLevel,
                 true, false, MeshRenderLayer3D::Opaque, lod.assetLease,lod.identity,lod.occlusionOpacity);
        } else {
          const MeshLodLevel3D *selected = &lod.levels[selection.primaryLevel];
          if (!options.gpuInstancing && !selected->mesh.cpuMesh)
            for (std::size_t level = selection.primaryLevel + 1; level-- > 0;)
              if (lod.levels[level].mesh.cpuMesh) {
                selected = &lod.levels[level];
                break;
              }
          const bool withinShadowDistance =
              !options.shadows.enabled || !draw.DirectionalShadowMapEnabled() ||
              Length(worldBounds.center - camera.position) -
                      worldBounds.radius <=
                  options.shadows.maximumDistance;
          if (selected->material.castsShadow && !withinShadowDistance)
            ++stats.shadowDistanceRejected;
          if (submit(selected->mesh, selected->mesh.cpuMesh.get(), nullptr,
                     lod.transform, selected->material, 1.f, 0,
                     withinShadowDistance, true, false,
                     MeshRenderLayer3D::Opaque, lod.assetLease,lod.identity,lod.occlusionOpacity) &&
              lod.levels[0].mesh.TriangleCount() >
                  selected->mesh.TriangleCount())
            stats.lodTrianglesSaved += lod.levels[0].mesh.TriangleCount() -
                                       selected->mesh.TriangleCount();
        }
      };
      const std::size_t visibilityObjectCount=boxes_.size()+meshes_.size()+lodMeshes_.size();
      if(options.linearVisibilityReference){for(std::size_t index=0;index<visibilityObjectCount;++index)submitVisibilityObject(index);stats.visibilityCandidates=visibilityObjectCount;}
      else if(!EnsureVisibilityIndex()){for(std::size_t index=0;index<visibilityObjectCount;++index)submitVisibilityObject(index);stats.visibilityCandidates=visibilityObjectCount;}
      else{const float shadowDistance=options.shadows.enabled&&draw.DirectionalShadowMapEnabled()?options.shadows.maximumDistance:0.f;const auto candidates=visibilityIndex_.Query({camera,aspect,options.visibility.maximumDistance,shadowDistance,options.frustumCulling});const VisibilityBvhStats3D visibilityStats=visibilityIndex_.Stats();stats.visibilityCandidates=candidates.size();stats.visibilityNodesTested=visibilityStats.nodesTested;stats.visibilityNodesRejected=visibilityStats.nodesRejected;stats.distanceCulledMeshInstances+=visibilityStats.distanceRejectedObjects;stats.culledMeshInstances+=visibilityStats.distanceRejectedObjects+visibilityStats.frustumRejectedObjects;for(std::uint32_t index:candidates)submitVisibilityObject(index);}
      stats.culledTriangles =
          stats.inputTriangles > stats.visibleTriangles
              ? stats.inputTriangles - stats.visibleTriangles
              : 0;
      const Vec3 cameraForward = Normalize(camera.target - camera.position);
      for (const auto &line : lines_) {
        const Vec3 direction = Normalize(line.b - line.a);
        if (Length(direction) < .0001f)
          continue;
        Vec3 side = Normalize(Cross(direction, cameraForward));
        if (Length(side) < .0001f)
          side = Normalize(Cross(direction, camera.up));
        if (Length(side) < .0001f)
          continue;
        side = side * (line.width * .0125f);
        const uint32_t base = uint32_t(draw.worldVertices.size()),
                       color = PackColorRgba8(line.color),
                       normal = PackNormalSnorm8({0, 1, 0}, 0);
        for (Vec3 point :
             {line.a - side, line.a + side, line.b + side, line.b - side})
          draw.worldVertices.push_back(
              {point.x, point.y, point.z, normal, color});
        draw.worldIndices.insert(
            draw.worldIndices.end(),
            {base, base + 1, base + 2, base, base + 2, base + 3});
        ++stats.visibleLines;
      }
      const auto mix=[](std::uint64_t& hash,std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};const auto mixFloat=[&](std::uint64_t& hash,float value){if(!std::isfinite(value)){stats.canonicalTransformValid=false;return;}const float canonical=value==0.f?0.f:value;mix(hash,std::bit_cast<std::uint32_t>(canonical));};
      stats.drawListRevision=14695981039346656037ull;stats.acceptedIdentityOrderFingerprint=14695981039346656037ull;
      mix(stats.drawListRevision,draw.meshInstances.size()-meshSubmissionBegin);mix(stats.drawListRevision,draw.worldVertices.size()-worldVertexBegin);mix(stats.drawListRevision,draw.worldIndices.size()-worldIndexBegin);
      for(std::size_t index=meshSubmissionBegin;index<draw.meshInstances.size();++index){const auto& instance=draw.meshInstances[index];mix(stats.drawListRevision,instance.mesh.contentPrimary);mix(stats.drawListRevision,instance.mesh.contentSecondary);mix(stats.drawListRevision,instance.mesh.vertexCount);mix(stats.drawListRevision,instance.mesh.indexCount);mix(stats.drawListRevision,instance.textures.semanticIdentity);mix(stats.drawListRevision,std::uint64_t(instance.layer));mix(stats.drawListRevision,instance.tint);mix(stats.drawListRevision,instance.surface);mix(stats.drawListRevision,std::uint64_t(instance.materialClass));mix(stats.drawListRevision,std::uint64_t(instance.alphaMode));mixFloat(stats.drawListRevision,instance.alphaCutoff);mixFloat(stats.drawListRevision,instance.normalScale);mixFloat(stats.drawListRevision,instance.textureScale);mixFloat(stats.drawListRevision,instance.lodCoverage);mix(stats.drawListRevision,instance.lodSeed);mix(stats.drawListRevision,instance.identity.id.high);mix(stats.drawListRevision,instance.identity.id.low);mix(stats.drawListRevision,instance.identity.generation);for(const auto& row:instance.transform.values)for(float value:row)mixFloat(stats.drawListRevision,value);for(const auto& row:instance.normalTransform.values)for(float value:row)mixFloat(stats.drawListRevision,value);if(instance.identity){mix(stats.acceptedIdentityOrderFingerprint,instance.identity.id.high);mix(stats.acceptedIdentityOrderFingerprint,instance.identity.id.low);mix(stats.acceptedIdentityOrderFingerprint,instance.identity.generation);++stats.acceptedIdentityCount;}}
      if(!stats.canonicalTransformValid)stats.drawListRevision=0;if(!stats.acceptedIdentityCount)stats.acceptedIdentityOrderFingerprint=0;
      return stats;
    }

    std::size_t TriangleCount()const{std::size_t count=triangles_.size()+boxes_.size()*12;for(const MeshObject3D& mesh:meshes_)count+=mesh.mesh.TriangleCount();for(const MeshLodObject3D& mesh:lodMeshes_)count+=mesh.levels[0].mesh.TriangleCount();return count;}
    std::size_t LineCount()const{return lines_.size();}
    std::size_t MeshCount()const{return boxes_.size()+meshes_.size()+lodMeshes_.size();}
private:
    enum class VisibilityKind : std::uint8_t {Box,Mesh,Lod};
    struct VisibilityLocation {VisibilityKind kind{};std::size_t index{};};
    static bool ValidVisibilityBounds(MeshBounds3D bounds){constexpr float limit=(std::numeric_limits<float>::max)()/8.f;return std::isfinite(bounds.center.x)&&std::isfinite(bounds.center.y)&&std::isfinite(bounds.center.z)&&std::abs(bounds.center.x)<=limit&&std::abs(bounds.center.y)<=limit&&std::abs(bounds.center.z)<=limit&&std::isfinite(bounds.radius)&&bounds.radius>=0&&bounds.radius<=limit&&std::isfinite(bounds.extents.x)&&std::isfinite(bounds.extents.y)&&std::isfinite(bounds.extents.z)&&bounds.extents.x>=0&&bounds.extents.y>=0&&bounds.extents.z>=0&&bounds.extents.x<=limit&&bounds.extents.y<=limit&&bounds.extents.z<=limit;}
    VisibilityEntityKey3D NextVisibilityKey(){return AllocateVisibilityEntityKey3D(nextVisibilityKey_);}
    bool EnsureVisibilityIndex()const{if(visibilityDirty_){visibilityBuildScratch_.clear();try{visibilityBuildScratch_.reserve(boxes_.size()+meshes_.size()+lodMeshes_.size());}catch(const std::bad_alloc&){return false;}std::uint32_t payload{};for(std::size_t i=0;i<boxes_.size();++i)visibilityBuildScratch_.push_back({boxKeys_[i],TransformSphereBounds(UnitBoxBounds(),boxes_[i].transform),payload++});for(std::size_t i=0;i<meshes_.size();++i)visibilityBuildScratch_.push_back({meshKeys_[i],TransformSphereBounds(meshes_[i].mesh.bounds,meshes_[i].transform),payload++});for(std::size_t i=0;i<lodMeshes_.size();++i)visibilityBuildScratch_.push_back({lodMeshKeys_[i],TransformSphereBounds(lodMeshes_[i].bounds,lodMeshes_[i].transform),payload++});if(!visibilityIndex_.Build(visibilityBuildScratch_))return false;visibilityDirty_=false;visibilityNeedsRefit_=false;}else if(visibilityNeedsRefit_){if(!visibilityIndex_.Refit())return false;visibilityNeedsRefit_=false;}return true;}
    enum class BvhEntryKind : std::uint8_t {Triangle,Box,Mesh,LodMesh};
    struct BvhEntry {BvhEntryKind kind{};std::uint32_t objectIndex{};Vec3 center{},extents{};};
    struct BvhNode {Vec3 center{},extents{};std::uint32_t first{},count{},left{},right{};};
    struct BvhTraversalItem {std::uint32_t node{};float distance{};};

    template<class Callback>void ForEachUniqueRaycastMesh(Callback&& callback)const{
        std::vector<const Mesh3D*> unique;unique.reserve(meshes_.size()+lodMeshes_.size()+1);const auto visit=[&](const Mesh3D* mesh){if(mesh&&std::find(unique.begin(),unique.end(),mesh)==unique.end()){unique.push_back(mesh);callback(*mesh);}};if(!boxes_.empty())visit(&UnitBox());for(const MeshObject3D& mesh:meshes_)visit(mesh.mesh.cpuMesh.get());for(const MeshLodObject3D& mesh:lodMeshes_)visit(mesh.levels[0].mesh.cpuMesh.get());
    }
    static std::uint32_t BuildMeshBvhNode(MeshRaycastIndex3D& index,std::uint32_t first,std::uint32_t count){
        Vec3 minimum=index.entries[first].center-index.entries[first].extents,maximum=index.entries[first].center+index.entries[first].extents,centroidMinimum=index.entries[first].center,centroidMaximum=centroidMinimum;for(std::uint32_t offset=1;offset<count;++offset){const MeshRaycastEntry3D& entry=index.entries[first+offset];const Vec3 entryMinimum=entry.center-entry.extents,entryMaximum=entry.center+entry.extents;minimum.x=(std::min)(minimum.x,entryMinimum.x);minimum.y=(std::min)(minimum.y,entryMinimum.y);minimum.z=(std::min)(minimum.z,entryMinimum.z);maximum.x=(std::max)(maximum.x,entryMaximum.x);maximum.y=(std::max)(maximum.y,entryMaximum.y);maximum.z=(std::max)(maximum.z,entryMaximum.z);centroidMinimum.x=(std::min)(centroidMinimum.x,entry.center.x);centroidMinimum.y=(std::min)(centroidMinimum.y,entry.center.y);centroidMinimum.z=(std::min)(centroidMinimum.z,entry.center.z);centroidMaximum.x=(std::max)(centroidMaximum.x,entry.center.x);centroidMaximum.y=(std::max)(centroidMaximum.y,entry.center.y);centroidMaximum.z=(std::max)(centroidMaximum.z,entry.center.z);}
        const std::uint32_t nodeIndex=static_cast<std::uint32_t>(index.nodes.size());index.nodes.push_back({(minimum+maximum)*.5f,(maximum-minimum)*.5f,first,count,0,0});if(count<=16)return nodeIndex;const Vec3 extent=centroidMaximum-centroidMinimum;const int axis=extent.y>extent.x?(extent.z>extent.y?2:1):(extent.z>extent.x?2:0);const std::uint32_t leftCount=count/2,midpoint=first+leftCount;std::nth_element(index.entries.begin()+first,index.entries.begin()+midpoint,index.entries.begin()+first+count,[axis](const MeshRaycastEntry3D& left,const MeshRaycastEntry3D& right){return axis==0?left.center.x<right.center.x:axis==1?left.center.y<right.center.y:left.center.z<right.center.z;});const std::uint32_t left=BuildMeshBvhNode(index,first,leftCount),right=BuildMeshBvhNode(index,midpoint,count-leftCount);MeshRaycastNode3D& node=index.nodes[nodeIndex];node.count=0;node.left=left;node.right=right;return nodeIndex;
    }
    static std::shared_ptr<const MeshRaycastIndex3D> GetMeshRaycastIndex(const Mesh3D& mesh,bool& built){
        built=false;if(auto existing=mesh.raycastCache->index.load(std::memory_order_acquire))return existing;auto candidate=std::make_shared<MeshRaycastIndex3D>();const std::size_t triangleCount=mesh.indices.size()/3;constexpr std::size_t MaximumMeshRaycastTriangles=4u*1024u*1024u;if(mesh.Valid()&&triangleCount<=MaximumMeshRaycastTriangles&&triangleCount<=(std::numeric_limits<std::uint32_t>::max)()/2){candidate->entries.reserve(triangleCount);candidate->nodes.reserve(triangleCount>1?triangleCount*2-1:triangleCount);for(std::uint32_t triangleIndex=0;triangleIndex<triangleCount;++triangleIndex){const std::size_t first=std::size_t{triangleIndex}*3;const auto position=[&](std::uint32_t vertexIndex){const Vertex3D& vertex=mesh.vertices[vertexIndex];return Vec3{vertex.x,vertex.y,vertex.z};};const Vec3 a=position(mesh.indices[first]),b=position(mesh.indices[first+1]),c=position(mesh.indices[first+2]);if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z)||!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||!std::isfinite(c.x)||!std::isfinite(c.y)||!std::isfinite(c.z)){candidate->entries.clear();break;}const Vec3 minimum{(std::min)({a.x,b.x,c.x}),(std::min)({a.y,b.y,c.y}),(std::min)({a.z,b.z,c.z})},maximum{(std::max)({a.x,b.x,c.x}),(std::max)({a.y,b.y,c.y}),(std::max)({a.z,b.z,c.z})};candidate->entries.push_back({(minimum+maximum)*.5f,(maximum-minimum)*.5f,triangleIndex});}if(candidate->entries.size()==triangleCount&&!candidate->entries.empty())BuildMeshBvhNode(*candidate,0,static_cast<std::uint32_t>(triangleCount));}
        std::shared_ptr<const MeshRaycastIndex3D> published=candidate,expected;if(mesh.raycastCache->index.compare_exchange_strong(expected,published,std::memory_order_release,std::memory_order_acquire)){built=true;return published;}return expected;
    }
    void InvalidateRaycastIndex(){bvhDirty_=true;bvhEntries_.clear();bvhNodes_.clear();bvhTraversal_.clear();}
    std::uint32_t BuildBvhNode(std::uint32_t first,std::uint32_t count)const{
        Vec3 minimum=bvhEntries_[first].center-bvhEntries_[first].extents,maximum=bvhEntries_[first].center+bvhEntries_[first].extents,centroidMinimum=bvhEntries_[first].center,centroidMaximum=centroidMinimum;
        for(std::uint32_t offset=1;offset<count;++offset){const BvhEntry& entry=bvhEntries_[first+offset];const Vec3 entryMinimum=entry.center-entry.extents,entryMaximum=entry.center+entry.extents;minimum.x=(std::min)(minimum.x,entryMinimum.x);minimum.y=(std::min)(minimum.y,entryMinimum.y);minimum.z=(std::min)(minimum.z,entryMinimum.z);maximum.x=(std::max)(maximum.x,entryMaximum.x);maximum.y=(std::max)(maximum.y,entryMaximum.y);maximum.z=(std::max)(maximum.z,entryMaximum.z);centroidMinimum.x=(std::min)(centroidMinimum.x,entry.center.x);centroidMinimum.y=(std::min)(centroidMinimum.y,entry.center.y);centroidMinimum.z=(std::min)(centroidMinimum.z,entry.center.z);centroidMaximum.x=(std::max)(centroidMaximum.x,entry.center.x);centroidMaximum.y=(std::max)(centroidMaximum.y,entry.center.y);centroidMaximum.z=(std::max)(centroidMaximum.z,entry.center.z);}
        const std::uint32_t nodeIndex=static_cast<std::uint32_t>(bvhNodes_.size());bvhNodes_.push_back({(minimum+maximum)*.5f,(maximum-minimum)*.5f,first,count,0,0});if(count<=4)return nodeIndex;const Vec3 centroidExtent=centroidMaximum-centroidMinimum;const int axis=centroidExtent.y>centroidExtent.x?(centroidExtent.z>centroidExtent.y?2:1):(centroidExtent.z>centroidExtent.x?2:0);const std::uint32_t leftCount=count/2,midpoint=first+leftCount;std::nth_element(bvhEntries_.begin()+first,bvhEntries_.begin()+midpoint,bvhEntries_.begin()+first+count,[axis](const BvhEntry& left,const BvhEntry& right){return axis==0?left.center.x<right.center.x:axis==1?left.center.y<right.center.y:left.center.z<right.center.z;});const std::uint32_t left=BuildBvhNode(first,leftCount),right=BuildBvhNode(midpoint,count-leftCount);BvhNode& node=bvhNodes_[nodeIndex];node.count=0;node.left=left;node.right=right;return nodeIndex;
    }
    void BuildRaycastIndex()const{
        bvhEntries_.clear();bvhNodes_.clear();bvhTraversal_.clear();const std::size_t count=triangles_.size()+boxes_.size()+meshes_.size()+lodMeshes_.size();if(count>(std::numeric_limits<std::uint32_t>::max)()){bvhDirty_=false;return;}bvhEntries_.reserve(count);
        for(std::uint32_t index=0;index<triangles_.size();++index){const Triangle3D& triangle=triangles_[index];Vec3 minimum{(std::min)({triangle.a.x,triangle.b.x,triangle.c.x}),(std::min)({triangle.a.y,triangle.b.y,triangle.c.y}),(std::min)({triangle.a.z,triangle.b.z,triangle.c.z})},maximum{(std::max)({triangle.a.x,triangle.b.x,triangle.c.x}),(std::max)({triangle.a.y,triangle.b.y,triangle.c.y}),(std::max)({triangle.a.z,triangle.b.z,triangle.c.z})};bvhEntries_.push_back({BvhEntryKind::Triangle,index,(minimum+maximum)*.5f,(maximum-minimum)*.5f});}
        const MeshBounds3D unitBounds=UnitBoxBounds();for(std::uint32_t index=0;index<boxes_.size();++index){const MeshBounds3D bounds=TransformBounds(unitBounds,boxes_[index].transform);bvhEntries_.push_back({BvhEntryKind::Box,index,bounds.center,bounds.extents});}for(std::uint32_t index=0;index<meshes_.size();++index){const MeshObject3D& mesh=meshes_[index];if(!mesh.mesh.cpuMesh)continue;const MeshBounds3D bounds=TransformBounds(mesh.mesh.bounds,mesh.transform);bvhEntries_.push_back({BvhEntryKind::Mesh,index,bounds.center,bounds.extents});}for(std::uint32_t index=0;index<lodMeshes_.size();++index){const MeshLodObject3D& mesh=lodMeshes_[index];if(!mesh.levels[0].mesh.cpuMesh)continue;const MeshBounds3D bounds=TransformBounds(mesh.bounds,mesh.transform);bvhEntries_.push_back({BvhEntryKind::LodMesh,index,bounds.center,bounds.extents});}
        if(!bvhEntries_.empty()){bvhNodes_.reserve(bvhEntries_.size()>1?bvhEntries_.size()*2-1:1);BuildBvhNode(0,static_cast<std::uint32_t>(bvhEntries_.size()));bvhTraversal_.reserve((std::min)(std::size_t{64},bvhNodes_.size()));}bvhDirty_=false;
    }
    static const Mesh3D& UnitBox(){static const Mesh3D mesh=Mesh3D::CreateUnitBox();return mesh;}
    static const MeshBounds3D& UnitBoxBounds(){static const MeshBounds3D bounds=UnitBox().Bounds();return bounds;}
    template<class Callback>void ForEachTriangle(Callback&& callback)const{
        for(const auto& triangle:triangles_)callback(triangle.a,triangle.b,triangle.c,triangle.color);
        const Mesh3D& boxMesh=UnitBox();for(const Box3D& box:boxes_)ForEachMeshTriangle(boxMesh,box.transform,box.material,callback);for(const MeshObject3D& mesh:meshes_)if(mesh.mesh.cpuMesh)ForEachMeshTriangle(*mesh.mesh.cpuMesh,mesh.transform,mesh.material,callback);for(const MeshLodObject3D& mesh:lodMeshes_)if(mesh.levels[0].mesh.cpuMesh)ForEachMeshTriangle(*mesh.levels[0].mesh.cpuMesh,mesh.transform,mesh.levels[0].material,callback);
    }
    template<class Callback>static void ForEachMeshTriangle(const Mesh3D& mesh,const Transform3D& transform,Material3D material,Callback& callback){const Matrix4 matrix=transform.Matrix();for(std::size_t index=0;index<mesh.indices.size();index+=3){auto vertex=[&](uint32_t item){const Vertex3D& source=mesh.vertices[item];return TransformPoint(matrix,{source.x,source.y,source.z});};Color source=UnpackColorRgba8(mesh.vertices[mesh.indices[index]].color);Color color{source.r*material.tint.r,source.g*material.tint.g,source.b*material.tint.b,source.a*material.tint.a};callback(vertex(mesh.indices[index]),vertex(mesh.indices[index+1]),vertex(mesh.indices[index+2]),color);}}
    std::vector<Triangle3D> triangles_;
    std::vector<Line3D> lines_;
    std::vector<Box3D> boxes_;
    std::vector<MeshObject3D> meshes_;
    std::vector<MeshLodObject3D> lodMeshes_;
    std::vector<VisibilityEntityKey3D> boxKeys_,meshKeys_,lodMeshKeys_;
    std::unordered_map<VisibilityEntityKey3D,VisibilityLocation> visibilityLocations_{};
    mutable VisibilityBvh3D visibilityIndex_{};
    mutable std::vector<VisibilityBvhItem3D> visibilityBuildScratch_{};
    VisibilityEntityKey3D nextVisibilityKey_{};
    mutable bool visibilityDirty_{true},visibilityNeedsRefit_{};
    mutable std::vector<BvhEntry> bvhEntries_;
    mutable std::vector<BvhNode> bvhNodes_;
    mutable std::vector<BvhTraversalItem> bvhTraversal_;
    mutable std::vector<BvhTraversalItem> meshBvhTraversal_;
    mutable bool bvhDirty_{true};
    MeshResource3D boxMesh_{};
    Scene3DCapacity retainedCapacity_{};
    std::size_t lastReleasedBytes_{};
    std::uint64_t trimCount_{};
    mutable std::uint64_t submissionSerial_{};
};
}
