#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Aevrynd {
namespace {
constexpr std::size_t GpuResourceAlignment=64u*1024u;
bool Add(std::size_t& value,std::size_t amount){if(value>SIZE_MAX-amount)return false;value+=amount;return true;}
bool Multiply(std::size_t left,std::size_t right,std::size_t& result){if(left&&right>SIZE_MAX/left)return false;result=left*right;return true;}
std::size_t MeshCpuBytes(const Mesh3D& mesh){return mesh.vertices.size()*sizeof(Vertex3D)+mesh.indices.size()*sizeof(std::uint32_t);}
std::size_t MeshGpuReservation(const Mesh3D& mesh){const auto bytes=MeshCpuBytes(mesh);if(!bytes)return 0;if(bytes>SIZE_MAX-(GpuResourceAlignment-1))return SIZE_MAX;return((bytes+GpuResourceAlignment-1)/GpuResourceAlignment)*GpuResourceAlignment;}
bool HasTextures(const MaterialTextureSetDefinition3D& value){return!value.baseColorPath.empty()||!value.roughnessPath.empty()||!value.normalPath.empty();}
std::size_t Delta(std::size_t after,std::size_t before){return after>=before?after-before:0;}
bool ValidSettings(const TerrainVegetationChunkSettings3D& value){return std::isfinite(value.chunkSize)&&value.chunkSize>0&&std::isfinite(value.terrainAmplitude)&&value.terrainAmplitude>=0&&value.terrainCells>0&&value.terrainCells<=2048&&value.vegetationPerChunk<=MaximumMeshInstancesPerFrame3D;}
bool FailCheckpoint(int& checkpoint){if(checkpoint<0)return false;if(checkpoint==0){checkpoint=-1;return true;}--checkpoint;return false;}
std::uint64_t Hash(std::uint64_t value){value^=value>>30;value*=0xbf58476d1ce4e5b9ull;value^=value>>27;value*=0x94d049bb133111ebull;return value^(value>>31);}
const TerrainVegetationChunkPayload3D* Payload(const WorldChunkPayload3D& value){return static_cast<const TerrainVegetationChunkPayload3D*>(value.data.get());}
}

TerrainVegetationWorldChunkSource3D::TerrainVegetationWorldChunkSource3D(TerrainVegetationChunkSettings3D settings,std::uint32_t workers):settings_(std::move(settings)){
    workers=(std::max)(1u,workers);workers_.reserve(workers);for(std::uint32_t i=0;i<workers;++i)workers_.emplace_back([this](std::stop_token stop){Worker(stop);});
}
TerrainVegetationWorldChunkSource3D::~TerrainVegetationWorldChunkSource3D(){for(auto& worker:workers_)worker.request_stop();wake_.notify_all();workers_.clear();}
bool TerrainVegetationWorldChunkSource3D::BeginLoad(WorldChunkRequest3D request,std::stop_token cancellation){if(cancellation.stop_requested()||!ValidSettings(settings_))return false;const double worldX=double(request.coordinate.x)*double(settings_.chunkSize),worldZ=double(request.coordinate.z)*double(settings_.chunkSize);if(!std::isfinite(worldX)||!std::isfinite(worldZ)||std::abs(worldX)>std::numeric_limits<float>::max()||std::abs(worldZ)>std::numeric_limits<float>::max())return false;{std::lock_guard lock(mutex_);work_.push_back({request,cancellation});}wake_.notify_one();return true;}
void TerrainVegetationWorldChunkSource3D::Worker(std::stop_token stop){
    for(;;){Work work{};{std::unique_lock lock(mutex_);wake_.wait(lock,stop,[&]{return!work_.empty();});if(stop.stop_requested())return;work=work_.front();work_.pop_front();++active_;}
        WorldChunkLoadCompletion3D result{work.request,{},false};
        if(!work.cancellation.stop_requested())try{
            auto payload=std::make_shared<TerrainVegetationChunkPayload3D>();payload->globalChunk={work.request.coordinate.x,0,work.request.coordinate.z};const auto cells=settings_.terrainCells;const float cell=settings_.chunkSize/float(cells);
            HeightField3D field({{},cells+1,cells+1,cell});
            field.Generate(work.request.seed,settings_.terrainAmplitude,.02f,5);payload->terrain=field.CreateMesh();
            payload->vegetation.reserve(settings_.vegetationPerChunk);
            for(std::uint32_t i=0;i<settings_.vegetationPerChunk&&!work.cancellation.stop_requested();++i){const auto h=Hash(std::uint64_t(work.request.seed)^std::uint64_t(std::uint32_t(work.request.coordinate.x))*0x9e3779b9u^std::uint64_t(std::uint32_t(work.request.coordinate.z))*0x85ebca6bu^i);const float x=(float(h&0xffffu)+.5f)/65536.f*settings_.chunkSize;const float z=(float((h>>16)&0xffffu)+.5f)/65536.f*settings_.chunkSize;Transform3D transform{};transform.position={x,field.SampleHeight(x,z),z};const float scale=.75f+float((h>>32)&255u)/512.f;transform.scale={scale,scale,scale};const Matrix4 worldTransform=transform.Matrix();const MeshBounds3D worldBounds=TransformSphereBounds({{0,1,0},2.25f,{1,2.25f,1}},transform);payload->vegetation.push_back({Hash((work.request.requestId<<32)^i)|1u,0,worldTransform,worldBounds,{.8f+.2f*float((h>>40)&255u)/255.f,1,.8f,1}});}
            std::size_t vegetationBytes{};if(!Multiply(payload->vegetation.capacity(),sizeof(VegetationInstance3D),vegetationBytes)){result.succeeded=false;throw std::overflow_error("vegetation cpu byte overflow");}payload->cpuBytes=MeshCpuBytes(payload->terrain);if(!Add(payload->cpuBytes,vegetationBytes)){result.succeeded=false;throw std::overflow_error("chunk cpu byte overflow");}payload->conservativeGpuBytes=MeshGpuReservation(payload->terrain);for(const auto& mesh:settings_.vegetationLods)if(!Add(payload->conservativeGpuBytes,MeshGpuReservation(mesh)))payload->conservativeGpuBytes=SIZE_MAX;
            if(payload->terrain.Valid()&&!work.cancellation.stop_requested()){result.payload={payload,payload->cpuBytes,payload->conservativeGpuBytes};result.succeeded=true;}
        }catch(...){}
        {std::lock_guard lock(mutex_);completions_.push_back(std::move(result));--active_;if(work_.empty()&&!active_)idle_.notify_all();}
    }
}
bool TerrainVegetationWorldChunkSource3D::TryPopCompletion(WorldChunkLoadCompletion3D& completion){std::lock_guard lock(mutex_);if(completions_.empty())return false;completion=std::move(completions_.front());completions_.pop_front();return true;}
void TerrainVegetationWorldChunkSource3D::WaitIdle(){std::unique_lock lock(mutex_);idle_.wait(lock,[&]{return work_.empty()&&!active_;});}
std::size_t TerrainVegetationWorldChunkSource3D::TrimUnusedMemory(){std::lock_guard lock(mutex_);const auto before=work_.size()*sizeof(Work)+completions_.size()*sizeof(WorldChunkLoadCompletion3D);if(work_.empty())decltype(work_){}.swap(work_);if(completions_.empty())decltype(completions_){}.swap(completions_);return before;}

TerrainVegetationWorldChunkActivationTarget3D::TerrainVegetationWorldChunkActivationTarget3D(Engine& engine,SceneECSRegistry3D& scene,ChunkedVegetationWorld3D& vegetation,TerrainVegetationChunkSettings3D settings):engine_(engine),scene_(scene),vegetation_(vegetation),settings_(std::move(settings)),ownerThread_(std::this_thread::get_id()){}
bool TerrainVegetationWorldChunkActivationTarget3D::SetWorldOriginSnapshot3D(WorldOrigin3D origin)noexcept{if(!OnOwnerThread()||!origin.generation||!NormalizeGlobalPosition3D(origin.position,settings_.chunkSize))return false;origin_=origin;return true;}
bool TerrainVegetationWorldChunkActivationTarget3D::PrepareWorldOriginRebase3D(const WorldOriginRebaseEvent3D& event){if(!OnOwnerThread()||!event||event.previous!=origin_||!event.current.generation)return false;preparedOrigin_=event.current;return NormalizeGlobalPosition3D(preparedOrigin_.position,settings_.chunkSize);}
void TerrainVegetationWorldChunkActivationTarget3D::CommitWorldOriginRebase3D(const WorldOriginRebaseEvent3D& event)noexcept{if(OnOwnerThread()&&preparedOrigin_==event.current)origin_=preparedOrigin_;preparedOrigin_={};}
TerrainVegetationWorldChunkActivationTarget3D::~TerrainVegetationWorldChunkActivationTarget3D(){for(auto& [id,record]:records_){(void)id;ReleaseRecord(record);}engine_.WaitRendererIdle3D();records_.clear();}
std::size_t TerrainVegetationWorldChunkActivationTarget3D::PhysicalAllocationBytes(WorldChunkRequest3D,const WorldChunkPayload3D& value)const{if(!OnOwnerThread()||!ValidSettings(settings_))return SIZE_MAX;const auto* payload=Payload(value);if(!payload)return SIZE_MAX;std::size_t bytes=MeshGpuReservation(payload->terrain);if(!shared_.references){for(const auto& mesh:settings_.vegetationLods)if(!Add(bytes,MeshGpuReservation(mesh)))return SIZE_MAX;if(HasTextures(settings_.vegetationTextures)){const auto estimate=engine_.EstimateMaterialTextureSetAllocationBytes3D(settings_.vegetationTextures);if(!estimate||!Add(bytes,*estimate))return SIZE_MAX;}}if(HasTextures(settings_.terrainTextures)){const auto estimate=engine_.EstimateMaterialTextureSetAllocationBytes3D(settings_.terrainTextures);if(!estimate||!Add(bytes,*estimate))return SIZE_MAX;}return bytes;}
bool TerrainVegetationWorldChunkActivationTarget3D::EnsureShared(const TerrainVegetationChunkPayload3D&,std::size_t& physical){if(shared_.references){if(shared_.references==SIZE_MAX)return false;++shared_.references;return true;}const auto before=engine_.GetRendererStats();try{for(std::size_t i=0;i<3;++i){shared_.meshes[i]=engine_.CreateMeshResource3D(settings_.vegetationLods[i],MeshCpuRetention3D::Discard);if(!shared_.meshes[i])throw std::runtime_error("vegetation mesh upload failed");}if(HasTextures(settings_.vegetationTextures)){shared_.textures=engine_.CreateMaterialTextureSet3D(settings_.vegetationTextures);if(!shared_.textures)throw std::runtime_error("vegetation texture upload failed");}shared_.lease=std::make_shared<AssetLeaseState3D>(AssetLeaseState3D{Hash(next_)});shared_.references=1;const auto after=engine_.GetRendererStats();physical=Delta(after.residentMeshBytes,before.residentMeshBytes)+Delta(after.residentTextureAllocationBytes,before.residentTextureAllocationBytes);shared_.physicalBytes=physical;return true;}catch(...){for(auto& mesh:shared_.meshes)if(mesh.handle)engine_.DestroyMesh3D(mesh.handle);if(shared_.textures)engine_.DestroyMaterialTextureSet3D(shared_.textures);shared_={};return false;}}
WorldChunkActivation3D TerrainVegetationWorldChunkActivationTarget3D::Activate(WorldChunkRequest3D request,const WorldChunkPayload3D& value){
    if(!OnOwnerThread()||!ValidSettings(settings_))return{};
    const auto* payload=Payload(value);if(!payload||payload->globalChunk!=GlobalChunkCoordinate3D{request.coordinate.x,0,request.coordinate.z}||!next_||next_==(std::numeric_limits<std::uint64_t>::max)())return{};
    Vec3 chunkOrigin{};if(!ToLocalPosition3D({payload->globalChunk,{}},origin_,chunkOrigin,settings_.chunkSize))return{};
    Record record{};record.request=request;std::size_t sharedBytes{};bool sharedAcquired{};
    try{
        if(!EnsureShared(*payload,sharedBytes))return{};sharedAcquired=true;
        if(FailCheckpoint(failureCheckpoint_))throw std::runtime_error("injected");
        const auto before=engine_.GetRendererStats();
        record.terrain=engine_.CreateMeshResource3D(payload->terrain,MeshCpuRetention3D::Discard);if(!record.terrain)throw std::runtime_error("terrain upload failed");
        if(HasTextures(settings_.terrainTextures)){record.terrainTextures=engine_.CreateMaterialTextureSet3D(settings_.terrainTextures);if(!record.terrainTextures)throw std::runtime_error("terrain textures failed");}
        if(FailCheckpoint(failureCheckpoint_))throw std::runtime_error("injected");
        SceneEntityDefinition3D definition{};definition.transform=SceneTransformComponent3D{};definition.transform->value.position=chunkOrigin;definition.renderable=SceneMeshRenderable3D{{},record.terrain,{},40,0,MeshLodDistanceMetric3D::Center,Material3D{{1,1,1,1},{},true,record.terrainTextures,.25f,true,MaterialClass3D::Terrain},{},1,true};
        std::vector<SceneEntity3D> entities;if(!scene_.CreateBatchAtomic(std::span(&definition,1),entities))throw std::runtime_error("terrain publish failed");record.terrainEntity=entities.front();
        if(FailCheckpoint(failureCheckpoint_))throw std::runtime_error("injected");
        std::vector<VegetationInstance3D> localVegetation=payload->vegetation;const Matrix4 translation=CreateTranslationMatrix(chunkOrigin);for(auto& instance:localVegetation){instance.transform=translation*instance.transform;instance.bounds.center=instance.bounds.center+chunkOrigin;}
        std::array<VegetationAsset3D,1> assets{{{{shared_.meshes[0].handle,shared_.meshes[1].handle,shared_.meshes[2].handle}}}};
        if(!vegetation_.AttachChunkAtomic(request.requestId,assets,localVegetation,&record.vegetation))throw std::runtime_error("vegetation publish failed");
        if(FailCheckpoint(failureCheckpoint_))throw std::runtime_error("injected");
        record.receipt=engine_.SubmitPendingUploads3D();const auto after=engine_.GetRendererStats();record.physicalBytes=sharedBytes+Delta(after.residentMeshBytes,before.residentMeshBytes)+Delta(after.residentTextureAllocationBytes,before.residentTextureAllocationBytes);
        const auto id=next_++;records_.emplace(id,std::move(record));return{id,records_.at(id).physicalBytes};
    }catch(...){
        if(record.vegetation)vegetation_.DetachChunk(record.vegetation);
        if(record.terrainEntity)scene_.Destroy(record.terrainEntity);
        if(record.terrainTextures)engine_.DestroyMaterialTextureSet3D(record.terrainTextures);
        if(record.terrain.handle)engine_.DestroyMesh3D(record.terrain.handle);
        if(sharedAcquired&&shared_.references&&!--shared_.references){for(auto& mesh:shared_.meshes)if(mesh.handle)engine_.DestroyMesh3D(mesh.handle);if(shared_.textures)engine_.DestroyMaterialTextureSet3D(shared_.textures);shared_={};}
        return{};
    }
}
bool TerrainVegetationWorldChunkActivationTarget3D::IsActivationComplete(WorldChunkActivation3D activation)const{if(!OnOwnerThread())return false;const auto found=records_.find(activation.value);return found!=records_.end()&&!found->second.retiring&&engine_.IsUploadComplete3D(found->second.receipt);}
void TerrainVegetationWorldChunkActivationTarget3D::ReleaseRecord(Record& record){if(record.released)return;if(record.vegetation)vegetation_.DetachChunk(record.vegetation);if(record.terrainEntity)scene_.DestroyBatchAtomic(std::span(&record.terrainEntity,1));if(record.terrainTextures)engine_.DestroyMaterialTextureSet3D(record.terrainTextures);if(record.terrain.handle)engine_.DestroyMesh3D(record.terrain.handle);if(shared_.references&&!--shared_.references){for(auto& mesh:shared_.meshes)if(mesh.handle)engine_.DestroyMesh3D(mesh.handle);if(shared_.textures)engine_.DestroyMaterialTextureSet3D(shared_.textures);shared_={};}record.released=true;}
void TerrainVegetationWorldChunkActivationTarget3D::Rollback(WorldChunkActivation3D activation){if(!OnOwnerThread())return;const auto found=records_.find(activation.value);if(found==records_.end())return;ReleaseRecord(found->second);records_.erase(found);}
WorldChunkActivation3D TerrainVegetationWorldChunkActivationTarget3D::BeginRetire(WorldChunkActivation3D activation){if(!OnOwnerThread())return{};const auto found=records_.find(activation.value);if(found==records_.end())return{};ReleaseRecord(found->second);found->second.retiring=true;return activation;}
bool TerrainVegetationWorldChunkActivationTarget3D::IsRetirementComplete(WorldChunkActivation3D retirement)const{if(!OnOwnerThread())return false;const auto found=records_.find(retirement.value);if(found==records_.end())return true;if(!found->second.retiring||engine_.GetRendererStats().pendingGpuReleaseBytes)return false;records_.erase(found);return true;}
void TerrainVegetationWorldChunkActivationTarget3D::WaitIdle(){if(!OnOwnerThread())return;engine_.WaitRendererIdle3D();for(auto it=records_.begin();it!=records_.end();)if(it->second.retiring){ReleaseRecord(it->second);it=records_.erase(it);}else ++it;}
std::size_t TerrainVegetationWorldChunkActivationTarget3D::TrimUnusedMemory(){if(!OnOwnerThread())return 0;const auto a=records_.bucket_count();records_.rehash(records_.size());return a>records_.bucket_count()?(a-records_.bucket_count())*sizeof(void*):0;}
std::size_t TerrainVegetationWorldChunkActivationTarget3D::PendingRetirements()const noexcept{std::size_t n{};for(const auto& [id,r]:records_){(void)id;if(r.retiring)++n;}return n;}
}
