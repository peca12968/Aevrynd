#include <Aevrynd/StaticMeshAssetLibrary3D.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace Aevrynd {
namespace {

bool SameColor(Color left,Color right){return left.r==right.r&&left.g==right.g&&left.b==right.b&&left.a==right.a;}
bool SameSurface(Surface3D left,Surface3D right){return left.roughness==right.roughness&&left.metallic==right.metallic&&left.emissive==right.emissive&&left.occlusion==right.occlusion;}
bool SameLayer(const MaterialTextureLayerDefinition3D& left,const MaterialTextureLayerDefinition3D& right){return left.baseColorPath==right.baseColorPath&&left.roughnessPath==right.roughnessPath&&left.normalPath==right.normalPath&&left.surfaceEncoding==right.surfaceEncoding;}
bool SameMaterial(const MaterialAssetDefinition3D& left,const MaterialAssetDefinition3D& right){return left.name==right.name&&left.textures.baseColorPath==right.textures.baseColorPath&&left.textures.roughnessPath==right.textures.roughnessPath&&left.textures.normalPath==right.textures.normalPath&&left.textures.maximumResolution==right.textures.maximumResolution&&left.textures.alphaMode==right.textures.alphaMode&&left.textures.alphaCutoff==right.textures.alphaCutoff&&left.textures.surfaceEncoding==right.textures.surfaceEncoding&&SameLayer(left.textures.additionalLayers[0],right.textures.additionalLayers[0])&&SameLayer(left.textures.additionalLayers[1],right.textures.additionalLayers[1])&&SameColor(left.material.tint,right.material.tint)&&SameSurface(left.material.surface,right.material.surface)&&left.material.castsShadow==right.material.castsShadow&&left.material.textureScale==right.material.textureScale&&left.material.normalScale==right.material.normalScale&&left.material.useUv==right.material.useUv&&left.material.materialClass==right.material.materialClass&&left.material.shadingModel==right.material.shadingModel&&left.material.alphaMode==right.material.alphaMode&&left.material.alphaCutoff==right.material.alphaCutoff&&left.material.doubleSided==right.material.doubleSided;}
bool SameDefinition(const StaticMeshAssetDefinition3D& left,const StaticMeshAssetDefinition3D& right){return left.name==right.name&&left.meshPath==right.meshPath&&left.objOptions.defaultColor.r==right.objOptions.defaultColor.r&&left.objOptions.defaultColor.g==right.objOptions.defaultColor.g&&left.objOptions.defaultColor.b==right.objOptions.defaultColor.b&&left.objOptions.defaultColor.a==right.objOptions.defaultColor.a&&left.objOptions.convertRightHandedToLeftHanded==right.objOptions.convertRightHandedToLeftHanded&&SameMaterial(left.material,right.material)&&left.lodSettings.targetTriangleRatios==right.lodSettings.targetTriangleRatios&&left.lodSettings.generatedLevelCount==right.lodSettings.generatedLevelCount&&left.lodSettings.maximumGridResolution==right.lodSettings.maximumGridResolution&&left.lodSettings.reorderForVertexCache==right.lodSettings.reorderForVertexCache&&left.lodSettings.vertexCacheSize==right.lodSettings.vertexCacheSize&&left.lodSettings.maximumTotalOutputTriangles==right.lodSettings.maximumTotalOutputTriangles&&left.lodSwitchDistances==right.lodSwitchDistances&&left.lodTransitionWidths==right.lodTransitionWidths&&left.lodDistanceMetric==right.lodDistanceMetric&&left.cpuRetention==right.cpuRetention&&left.generateLods==right.generateLods;}
bool ValidLodPresentation(const StaticMeshAssetDefinition3D& definition){if(!definition.generateLods)return true;if(!definition.lodSettings.generatedLevelCount||definition.lodSettings.generatedLevelCount>MaximumGeneratedMeshLodLevels3D)return false;float previous{},previousEnd=-std::numeric_limits<float>::infinity();for(std::uint32_t index=0;index<definition.lodSettings.generatedLevelCount;++index){const float distance=definition.lodSwitchDistances[index],width=definition.lodTransitionWidths[index];if(!std::isfinite(distance)||distance<=previous||!std::isfinite(width)||width<0.f)return false;const float start=distance-width*.5f,end=distance+width*.5f;if(!std::isfinite(start)||!std::isfinite(end)||start<previousEnd)return false;previous=distance;previousEnd=end;}return definition.lodDistanceMetric==MeshLodDistanceMetric3D::Center||definition.lodDistanceMetric==MeshLodDistanceMetric3D::BoundsSurface;}
std::filesystem::path ResolvePath(const std::filesystem::path& root,const std::filesystem::path& value){if(value.empty())return{};std::error_code error;std::filesystem::path resolved=value.is_relative()?root/value:value;const std::filesystem::path canonical=std::filesystem::weakly_canonical(resolved,error);return error?resolved.lexically_normal():canonical;}
bool IsWithin(const std::filesystem::path& root,const std::filesystem::path& candidate){auto rootIt=root.begin(),candidateIt=candidate.begin();for(;rootIt!=root.end();++rootIt,++candidateIt)if(candidateIt==candidate.end()||*rootIt!=*candidateIt)return false;return true;}
bool Finite(Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}
bool ValidTransform(Transform3D value){return Finite(value.position)&&Finite(value.rotation)&&Finite(value.scale);}

}

bool StaticMeshAssetLibrary3D::Initialize(Engine& engine,SceneECSRegistry3D& registry,std::filesystem::path assetRoot,StaticMeshAssetLibraryLimits3D limits){assetRoot=ResolvePath({},assetRoot);if(!limits.maximumAssets||!limits.maximumInstances||!limits.maximumSourceFileBytes)return false;if(engine_&&(engine_!=&engine||registry_!=&registry))return false;if(!entries_.empty())return engine_==&engine&&registry_==&registry&&assetRoot_==assetRoot;if(!materials_.Initialize(engine,assetRoot))return false;engine_=&engine;registry_=&registry;assetRoot_=std::move(assetRoot);limits_=limits;return true;}

StaticMeshAssetDefinition3D StaticMeshAssetLibrary3D::Resolve(StaticMeshAssetDefinition3D definition)const{
    definition.meshPath=ResolvePath(assetRoot_,definition.meshPath);
    auto resolve=[this](std::filesystem::path& path){path=ResolvePath(assetRoot_,path);};
    resolve(definition.material.textures.baseColorPath);resolve(definition.material.textures.roughnessPath);resolve(definition.material.textures.normalPath);
    for(auto& layer:definition.material.textures.additionalLayers){resolve(layer.baseColorPath);resolve(layer.roughnessPath);resolve(layer.normalPath);}
    if(definition.material.name.empty())definition.material.name=definition.name+".material";
    return definition;
}

void StaticMeshAssetLibrary3D::DestroyMeshes(Entry& entry){if(!engine_)return;engine_->DestroyMeshLodResourceChain3D(entry.meshes);}

bool StaticMeshAssetLibrary3D::Load(StaticMeshAssetDefinition3D definition){
    ++stats_.loadRequests;if(!engine_||!registry_||definition.name.empty()||definition.meshPath.empty()){++stats_.loadFailures;return false;}definition=Resolve(std::move(definition));
    if(const auto found=entries_.find(definition.name);found!=entries_.end()){if(SameDefinition(found->second.definition,definition)){++stats_.cacheHits;return true;}++stats_.loadFailures;++stats_.nameConflicts;return false;}
    if(entries_.size()>=limits_.maximumAssets){++stats_.loadFailures;return false;}
    const auto pathInside=[this](const std::filesystem::path& path){return path.empty()||IsWithin(assetRoot_,path);};bool pathsValid=pathInside(definition.meshPath)&&pathInside(definition.material.textures.baseColorPath)&&pathInside(definition.material.textures.roughnessPath)&&pathInside(definition.material.textures.normalPath);for(const auto& layer:definition.material.textures.additionalLayers)pathsValid&=pathInside(layer.baseColorPath)&&pathInside(layer.roughnessPath)&&pathInside(layer.normalPath);std::error_code filesystemError;const auto sourceBytes=std::filesystem::file_size(definition.meshPath,filesystemError);
    if(!ValidLodPresentation(definition)||!pathsValid||filesystemError||sourceBytes>limits_.maximumSourceFileBytes){++stats_.loadFailures;return false;}
    std::wstring extension=definition.meshPath.extension().wstring();std::ranges::transform(extension,extension.begin(),[](wchar_t value){return wchar_t(std::towlower(value));});
    MeshLoadResult3D loaded=extension==L".obj"?LoadObjMeshFile(definition.meshPath,definition.objOptions):extension==L".aemesh"?LoadAevryndMeshFile(definition.meshPath):MeshLoadResult3D{{},"Unsupported static mesh extension"};
    if(!loaded){++stats_.loadFailures;return false;}
    Entry temporary{};temporary.definition=definition;MeshProcessingError3D processingError{};
    if(definition.generateLods){if(!engine_->CreateMeshLodResourceChain3D(loaded.mesh,temporary.meshes,definition.lodSettings,&processingError,definition.cpuRetention)){++stats_.loadFailures;return false;}}
    else{temporary.meshes.levels[0]=engine_->CreateMeshResource3D(loaded.mesh,definition.cpuRetention==MeshLodCpuRetention3D::None?MeshCpuRetention3D::Discard:MeshCpuRetention3D::RetainForQueries);temporary.meshes.levelCount=temporary.meshes.levels[0]?1u:0u;if(!temporary.meshes.levelCount){++stats_.loadFailures;return false;}}
    const bool materialExisted=materials_.Find(definition.material.name)!=nullptr;
    if(!materials_.Load(definition.material)){DestroyMeshes(temporary);++stats_.loadFailures;return false;}
    temporary.material=*materials_.Find(definition.material.name);
    try{auto [references,inserted]=materialReferences_.try_emplace(definition.material.name,0);++references->second;try{entries_.emplace(definition.name,std::move(temporary));}catch(...){if(!--references->second)materialReferences_.erase(references);throw;}}
    catch(const std::bad_alloc&){DestroyMeshes(temporary);if(!materialExisted)materials_.Unload(definition.material.name);++stats_.loadFailures;return false;}
    return true;
}

bool StaticMeshAssetLibrary3D::LoadAll(std::span<const StaticMeshAssetDefinition3D> definitions){std::vector<std::string> loaded;try{loaded.reserve(definitions.size());}catch(const std::bad_alloc&){++stats_.loadFailures;return false;}for(const auto& definition:definitions){const bool existed=Contains(definition.name);if(Load(definition)){if(!existed)loaded.push_back(definition.name);continue;}for(auto iterator=loaded.rbegin();iterator!=loaded.rend();++iterator)Unload(*iterator);++stats_.batchRollbacks;return false;}return true;}

bool StaticMeshAssetLibrary3D::Unload(std::string_view name){CollectDeadInstances();const auto found=entries_.find(std::string(name));if(found==entries_.end())return false;if(std::ranges::any_of(instances_,[&](const Instance& instance){return instance.assetName==name;})){++stats_.unloadRejections;return false;}const std::string materialName=found->second.definition.material.name;DestroyMeshes(found->second);entries_.erase(found);const auto references=materialReferences_.find(materialName);if(references!=materialReferences_.end()&&!--references->second){materials_.Unload(materialName);materialReferences_.erase(references);}++stats_.unloaded;return true;}

void StaticMeshAssetLibrary3D::Shutdown(){ClearInstances();if(engine_)for(auto& [name,entry]:entries_){(void)name;DestroyMeshes(entry);}std::unordered_map<std::string,Entry>{}.swap(entries_);std::unordered_map<std::string,std::size_t>{}.swap(materialReferences_);std::vector<Instance>{}.swap(instances_);materials_.Shutdown();engine_=nullptr;registry_=nullptr;assetRoot_.clear();}

bool StaticMeshAssetLibrary3D::Contains(std::string_view name)const{return entries_.contains(std::string(name));}

std::optional<SceneMeshRenderable3D> StaticMeshAssetLibrary3D::CreateRenderable(std::string_view name,std::uint32_t layer,bool visible)const{const auto found=entries_.find(std::string(name));if(found==entries_.end())return std::nullopt;const Entry& entry=found->second;SceneMeshRenderable3D result{};result.layer=layer;result.visible=visible;result.lodDistanceMetric=entry.definition.lodDistanceMetric;if(entry.meshes.levelCount==1){result.nearMesh=entry.meshes.levels[0];result.nearMaterial=entry.material;return result;}result.lodLevelCount=entry.meshes.levelCount;for(std::uint32_t level=0;level<entry.meshes.levelCount;++level){result.lodLevels[level].mesh=entry.meshes.levels[level];result.lodLevels[level].material=entry.material;if(level+1<entry.meshes.levelCount){result.lodLevels[level].switchDistance=entry.definition.lodSwitchDistances[level];result.lodLevels[level].transitionWidth=entry.definition.lodTransitionWidths[level];}}return result;}

void StaticMeshAssetLibrary3D::CollectDeadInstances(){if(!registry_)return;for(auto iterator=instances_.begin();iterator!=instances_.end();)if(!registry_->Alive(iterator->entity)){iterator=instances_.erase(iterator);++stats_.externallyDestroyedInstances;}else ++iterator;}

SceneEntity3D StaticMeshAssetLibrary3D::Instantiate(std::string_view name,Transform3D transform,std::uint32_t layer,bool visible){CollectDeadInstances();if(!registry_||instances_.size()>=limits_.maximumInstances||!ValidTransform(transform)){++stats_.instanceFailures;return{};}const auto renderable=CreateRenderable(name,layer,visible);if(!renderable){++stats_.instanceFailures;return{};}try{instances_.reserve(instances_.size()+1);}catch(const std::bad_alloc&){++stats_.instanceFailures;return{};}const SceneEntity3D entity=registry_->Create();if(!entity||!registry_->SetTransform(entity,{transform})||!registry_->SetRenderable(entity,*renderable)){if(entity)registry_->Destroy(entity);++stats_.instanceFailures;return{};}instances_.push_back({entity,std::string(name)});++stats_.instancesCreated;return entity;}

bool StaticMeshAssetLibrary3D::DestroyInstance(SceneEntity3D entity){CollectDeadInstances();const auto found=std::ranges::find(instances_,entity,&Instance::entity);if(found==instances_.end()||!registry_->Destroy(entity))return false;instances_.erase(found);++stats_.instancesDestroyed;return true;}

std::size_t StaticMeshAssetLibrary3D::ClearInstances(){CollectDeadInstances();const std::size_t count=instances_.size();if(registry_)for(const Instance& instance:instances_)if(registry_->Destroy(instance.entity))++stats_.instancesDestroyed;instances_.clear();return count;}

std::uint32_t StaticMeshAssetLibrary3D::LevelCount(std::string_view name)const{const auto found=entries_.find(std::string(name));return found==entries_.end()?0u:found->second.meshes.levelCount;}

StaticMeshAssetLibraryStats3D StaticMeshAssetLibrary3D::Stats()const{const_cast<StaticMeshAssetLibrary3D*>(this)->CollectDeadInstances();StaticMeshAssetLibraryStats3D result=stats_;result.assets=entries_.size();result.liveInstances=instances_.size();for(const auto& [name,references]:materialReferences_){(void)name;result.materialReferences+=references;}for(const auto& [name,entry]:entries_){(void)name;result.meshLevels+=entry.meshes.levelCount;if(entry.meshes.levelCount){result.sourceTriangles+=entry.meshes.levels[0].TriangleCount();for(std::uint32_t level=0;level<entry.meshes.levelCount;++level)result.residentTriangles+=entry.meshes.levels[level].TriangleCount();}}return result;}

}
