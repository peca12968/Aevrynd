#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>
#if defined(AEVRYND_HLOD_HZB_HOOKS_OFF_PUBLIC_SURFACE_COMPILE)

template<class Stats>
concept HasHlodHzbIdentityReceipt = requires(Stats stats) {
    stats.lastHlodHzbIdentityReceipt;
};

static_assert(!HasHlodHzbIdentityReceipt<Aevrynd::RendererStats>,
              "Internal HZB identity receipts must not be exposed when renderer test hooks are disabled.");

#else
#include <array>
#include <iostream>
#include <limits>
#include <windows.h>
using namespace Aevrynd;
namespace {
constexpr HlodClusterId3D ClusterId{0x484c4f445f434150ull,1};
constexpr std::array<std::size_t,5> PhaseCounts{65535,65536,65537,2048,2048};
int Result{};
void Fail(const char* message){if(!Result){std::cerr<<message<<'\n';Result=1;}}
bool Validation(const RendererStats& stats){wchar_t expected[8]{};const DWORD count=GetEnvironmentVariableW(L"AEVRYND_EXPECT_GPU_VALIDATION_MODE",expected,DWORD(std::size(expected)));if(count==6&&lstrcmpW(expected,L"normal")==0)return!stats.gpuValidationRequested&&!stats.gpuValidationAvailable&&!stats.gpuValidationExecuted&&!stats.debugLayerEnabled&&!stats.gpuValidationPassed&&!stats.debugErrorCount&&!stats.debugWarningCount;if(count==3&&lstrcmpW(expected,L"gbv")==0)return stats.gpuValidationRequested&&stats.gpuValidationAvailable&&stats.gpuValidationExecuted&&stats.debugLayerEnabled&&stats.gpuValidationPassed&&!stats.debugErrorCount&&!stats.debugWarningCount;return false;}
MaterialAssetDefinition3D Material(){MaterialAssetDefinition3D value;value.textures={"engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",64,"engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};value.material.useUv=true;return value;}
StaticMeshAssetDefinition3D Asset(std::string name,std::filesystem::path path){StaticMeshAssetDefinition3D value;value.name=std::move(name);value.meshPath=std::move(path);value.material=Material();value.generateLods=false;return value;}
Mesh3D OccluderCard(){Mesh3D mesh;const auto normal=PackNormalSnorm8({0,0,-1});const auto white=PackColorRgba8({1,1,1,1});mesh.vertices={{-2,-1,0,normal,white,0,1},{-2,1,0,normal,white,0,0},{2,1,0,normal,white,1,0},{2,-1,0,normal,white,1,1}};mesh.indices={0,1,2,0,2,3};return mesh;}
bool BindSourceResourceContracts(StaticMeshAssetLibrary3D& library,std::span<HlodSourceBuildInput3D> sources){const auto lease=library.ResolveRenderable("hlod-source",1,true);if(!lease||!lease->Valid()||lease->Generation()>(std::numeric_limits<std::uint32_t>::max)())return false;const auto& renderable=lease->Renderable();const auto& mesh=renderable.lodLevelCount?renderable.lodLevels[0].mesh:renderable.nearMesh;const auto& material=renderable.lodLevelCount?renderable.lodLevels[0].material:renderable.nearMaterial;for(auto& source:sources){source.meshHandle=mesh.handle.value;source.materialHandle=material.textures.value;source.meshGeneration=static_cast<std::uint32_t>(lease->Generation());source.materialGeneration=source.meshGeneration;source.meshContentPrimary=mesh.handle.contentPrimary;source.meshContentSecondary=mesh.handle.contentSecondary;source.materialSemanticIdentity=material.textures.semanticIdentity;source.resourceOwnerIdentity=renderable.resourceOwnerIdentity;}return true;}
class ImmediateReceiptSource final:public IGpuRetirementReceiptSource3D{public:GpuRetirementReceipt3D CaptureGraphicsRetirementReceipt3D()override{return MakeReceipt(91,next_++);}bool IsGraphicsRetirementComplete3D(GpuRetirementReceipt3D)const override{return true;}private:std::uint64_t next_{1};};
std::shared_ptr<TerrainVegetationChunkPayload3D> MakeFixture(StaticMeshAssetLibrary3D& library,TerrainVegetationChunkSettings3D settings){
 auto proxy=library.PrepareHlodProxyCpu3D(settings.worldId,{},ClusterId,1,Asset({},"engine/assets/model-packs/poly-haven/rock_09/rock_09.aemesh"));if(!proxy)return{};
 std::array<HlodSourceBuildInput3D,2> sources{};for(std::size_t index=0;index<sources.size();++index){sources[index].reference.globalObjectId={0x534f555243455f30ull+index,index+1};sources[index].generation=1;sources[index].localBoundsExtents={1,1,1};sources[index].position={index?2.f:-2.f,0,0};}if(!BindSourceResourceContracts(library,sources))return{};
 HlodClusterBuildInput3D build{};build.asset={ClusterId,1,{},5,1,.8f,1.2f,2,HlodCompatibility3D::OpaqueCompatible};build.resource={1,1,proxy->reservedPhysicalBytes,1,1,100,20};build.instance={ClusterId,{},{},{1,1,1},{}};build.sources=sources;const std::array builds{build};auto data=BuildHlodData3D(builds);if(!data)return{};
 TerrainVegetationHlodCluster3D manifest{};manifest.id=ClusterId;manifest.generation=1;manifest.proxy=*proxy;for(std::size_t index=0;index<sources.size();++index)manifest.sources.push_back({sources[index].reference,1,"hlod-source",{{},{index?2.f:-2.f,0,0}},{},{1,1,1}});const std::array manifests{manifest};auto immutableManifest=BuildTerrainVegetationHlodManifest3D(manifests,*data);if(!immutableManifest)return{};
 auto payload=std::make_shared<TerrainVegetationChunkPayload3D>();payload->globalChunk={};HeightField3D field({{},5,5,2});field.Generate(73,1,.05f,2);payload->heightField=field;payload->terrain=field.CreateMesh();payload->hlod=std::move(data);payload->hlodManifest=std::move(immutableManifest);payload->cpuBytes=payload->terrain.vertices.size()*sizeof(Vertex3D)+payload->terrain.indices.size()*sizeof(std::uint32_t);return payload;
}
class Game final:public IGame{
public:
 void OnStart(Engine& engine)override{
  settings_.chunkSize=16;settings_.terrainCells=4;settings_.vegetationPerChunk=0;settings_.vegetationLods={Mesh3D::CreateUnitPyramid(),Mesh3D::CreateUnitPyramid(),Mesh3D::CreateUnitPyramid()};
  if(!library_.Initialize(engine,scene_,AEVRYND_SOURCE_DIR)||!library_.Load(Asset("hlod-source","games/engine_lab/assets/engine_probe.obj"))){Fail("HLOD capacity fixture library initialization failed");engine.Quit();return;}
  target_=std::make_unique<TerrainVegetationWorldChunkActivationTarget3D>(engine,scene_,vegetation_,&library_,nullptr,receipts_,settings_);const auto payload=MakeFixture(library_,settings_);if(!payload){Fail("HLOD capacity fixture construction failed");engine.Quit();return;}activation_=target_->Activate({{},1,1,1,1},{payload,payload->cpuBytes,payload->cpuBytes});
  view_.camera={{0,0,-160},{0,0,-159},{0,1,0},{}};view_.camera.projection.farPlane=500;view_.viewport={640,360};view_.chunkSize=settings_.chunkSize;runtime_.quality.maximumScreenError=2;runtime_.maximumVisibilityTransitionsPerFrame=1;runtime_.cameraTeleportDistance=1000;
  const bool selected=activation_&&target_->SelectHlodFrame3D(1,view_,runtime_);const auto selectionStats=target_->HlodRuntimeStats();if(!selected||selectionStats.transitionClusters!=1||!(token_=target_->FreezeHlodFrame3D(1))){std::cerr<<"hlod_transition_setup="<<bool(activation_)<<'/'<<selected<<" modes="<<selectionStats.sourceClusters<<'/'<<selectionStats.transitionClusters<<'/'<<selectionStats.proxyClusters<<" applied="<<selectionStats.appliedTransitions<<'\n';Fail("World Partition did not freeze a transition HLOD cluster");engine.Quit();return;}
  filler_=engine.CreateMesh3D(Mesh3D::CreateUnitBox());wall_=engine.CreateMesh3D(OccluderCard());if(!filler_||!wall_){Fail("HLOD capacity GPU fixtures were not created");engine.Quit();return;}
  engine.SetGpuInstanceCullingSettings3D({true,5000,GpuInstanceSubmissionMode3D::Aggregate});engine.SetHzbOcclusionSettings3D({HzbOcclusionQuality3D::Off,1024,16,2,64,.00001f});
 }
 void OnUpdate(Engine&,float)override{}
 void OnRender(Engine& engine,float)override{
  if(Result||!target_||!token_){engine.Quit();return;}
  if(phase_==2&&frames_){const auto receipt=engine.GetRendererStats().lastHlodCapacityReceipt;if(receipt.consideredClusters){if(!phaseHash_)phaseHash_=receipt.decisionHash;else if(phaseHash_!=receipt.decisionHash){Fail("HLOD capacity decision receipt was not deterministic");engine.Quit();return;}}}
  if(frames_>=D3D12Renderer::BufferedFrames+3){if(!baselineValidated_){if(!ValidateBaseline(engine.GetRendererStats())){engine.Quit();return;}baselineValidated_=true;frames_=0;engine.SetHzbOcclusionSettings3D({HzbOcclusionQuality3D::Conservative,1024,16,2,64,.00001f});}else{if(!ValidatePhase(engine.GetRendererStats())){engine.Quit();return;}++phase_;frames_=0;phaseHash_=0;baselineValidated_=false;baselineCandidates_.clear();submission_={};engine.SetHzbOcclusionSettings3D({HzbOcclusionQuality3D::Off,1024,16,2,64,.00001f});if(phase_==PhaseCounts.size()){std::cout<<"hlodCapacity=65535/65536/65537 adapterFreeze=passed hzbIdentity=transition/source/proxy validation=passed\n";engine.Quit();return;}if(phase_>=3&&!SelectHardMode(phase_==3?HlodSelectionMode3D::Source:HlodSelectionMode3D::Proxy)){engine.Quit();return;}}}
  if(!submission_&&!BuildSubmission()){engine.Quit();return;}engine.Draw().SetStaticMeshSubmission3D(submission_);++frames_;
 }
 void OnStop(Engine& engine)override{submission_={};if(target_&&activation_)target_->Rollback(activation_);target_.reset();library_.ClearInstances();if(library_.Initialized())library_.Shutdown();if(wall_)engine.DestroyMesh3D(wall_);if(filler_)engine.DestroyMesh3D(filler_);engine.WaitRendererIdle3D();}
private:
 bool BuildSubmission(){
  DrawList staging;staging.SetViewProjection(CreateViewProjectionMatrix(view_.camera,view_.viewport.x/view_.viewport.y));staging.SetBackfaceCulling(false);const std::size_t total=PhaseCounts[phase_],expectedSources=phase_==4?0:2,expectedProxies=phase_==3?0:1,expectedHlod=expectedSources+expectedProxies,fillerCount=total-expectedHlod-1;
  if(!staging.MeshInstance(wall_,CreateTranslationMatrix({0,0,-100})*CreateScaleMatrix({100,100,1}),{1,1,1,1},{},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false)){Fail("HZB wall submission failed");return false;}
  for(std::size_t index=0;index<fillerCount;++index){const Vec3 position{float(index%64)*.001f,float((index/64)%64)*.001f,0};if(!staging.MeshInstance(filler_,CreateTranslationMatrix(position)*CreateScaleMatrix({.02f,.02f,.02f}),{1,1,1,1},{},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false)){Fail("HZB filler submission failed");return false;}}
  Scene3D frozenScene;const auto collected=target_->CollectFrozenHlodFrame3D(*token_,frozenScene,staging);Scene3DSubmissionOptions options{};options.frustumCulling=false;const auto submitted=frozenScene.Submit(staging,view_.camera,view_.viewport,options);
  std::size_t sources{},proxies{},inverted{};std::uint8_t seed=255;float threshold=-1;for(const auto& instance:staging.meshInstances){if(instance.identity.hlodClusterId!=ClusterId)continue;if(phase_<3&&!instance.identity.sharedLodDither){Fail("HLOD transition metadata was not propagated");return false;}if(phase_>=3&&instance.identity.sharedLodDither){Fail("HLOD hard mode retained transition metadata");return false;}if(seed==255){seed=std::uint8_t(instance.lodSeed&7u);threshold=instance.lodCoverage;}else if(seed!=(instance.lodSeed&7u)||threshold!=instance.lodCoverage){Fail("HLOD threshold or seed diverged");return false;}if(instance.identity.hlodProxy){++proxies;inverted+=(instance.lodSeed&8u)!=0;}else{++sources;if(instance.lodSeed&8u){Fail("HLOD source used proxy dither inversion");return false;}}}
  const bool raster=phase_<3?inverted==1&&threshold>0&&threshold<1:!inverted&&threshold==1.f;if(!collected||submitted.visibleMeshInstances!=expectedHlod||sources!=expectedSources||proxies!=expectedProxies||!raster||staging.meshInstances.size()!=total){Fail("Adapter frozen HLOD mode did not reach the DrawList atomically");return false;}submission_=staging.FreezeMeshInstances3D();return bool(submission_)&&submission_.Instances().size()==total;
 }
 bool SelectHardMode(HlodSelectionMode3D mode){HlodSelectionView3D selectionView=view_;selectionView.camera.position={0,0,mode==HlodSelectionMode3D::Source?-5.f:-500.f};selectionView.camera.target={0,0,selectionView.camera.position.z+1.f};selectionView.cameraCut=true;const std::uint64_t frame=phase_+2;if(!target_->SelectHlodFrame3D(frame,selectionView,runtime_)){Fail("HLOD hard-mode selection failed");return false;}const auto stats=target_->HlodRuntimeStats();const bool selected=mode==HlodSelectionMode3D::Source?stats.sourceClusters==1:stats.proxyClusters==1;if(!selected||(token_=target_->FreezeHlodFrame3D(frame))==std::nullopt){std::cerr<<"hlod_hard_mode="<<unsigned(mode)<<" modes="<<stats.sourceClusters<<'/'<<stats.transitionClusters<<'/'<<stats.proxyClusters<<'\n';Fail("HLOD hard mode did not freeze");return false;}return true;}
 bool ValidateBaseline(const RendererStats& stats){
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  const auto& identity=stats.lastHlodHzbIdentityReceipt;const std::size_t expectedSources=phase_==4?0:2,expectedProxies=phase_==3?0:1;const std::size_t sourceCount=std::size_t(std::ranges::count_if(identity.candidates,[](const RenderObjectIdentity3D& value){return!value.hlodProxy;})),proxyCount=std::size_t(std::ranges::count_if(identity.candidates,[](const RenderObjectIdentity3D& value){return value.hlodProxy;}));const bool passed=identity.complete&&identity.submissionRevision==submission_.ContentRevision()&&!identity.hzbActive&&identity.candidates.size()==expectedSources+expectedProxies&&identity.visible.size()==identity.candidates.size()&&sourceCount==expectedSources&&proxyCount==expectedProxies&&std::ranges::all_of(identity.candidates,[](const RenderObjectIdentity3D& value){return value.hlodClusterId==ClusterId;});if(passed)baselineCandidates_=identity.candidates;else std::cerr<<"hlod_baseline_phase="<<phase_<<" receipt="<<identity.complete<<'/'<<identity.submissionRevision<<'/'<<identity.readbackSerial<<'/'<<identity.tested<<'/'<<identity.hzbActive<<'/'<<identity.candidates.size()<<'/'<<identity.visible.size()<<" submission="<<submission_.ContentRevision()<<'\n';return passed;
#else
  (void)stats;return false;
#endif
 }
 bool ValidatePhase(const RendererStats& stats){
  const std::size_t total=PhaseCounts[phase_];const auto& capacity=stats.lastHlodCapacityReceipt;const bool boundary=phase_<2?stats.lastSubmittedMeshInstances==total&&!capacity.consideredClusters&&!capacity.droppedInstances:phase_==2?stats.lastSubmittedMeshInstances==65536&&capacity.consideredClusters==1&&capacity.sourceFallbackClusters==1&&!capacity.proxyFallbackClusters&&!capacity.rejectedClusters&&capacity.droppedInstances==1&&capacity.decisionHash!=1469598103934665603ull:stats.lastSubmittedMeshInstances==total&&!capacity.consideredClusters&&!capacity.droppedInstances;
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  const auto& identity=stats.lastHlodHzbIdentityReceipt;const std::size_t expectedSources=phase_==4?0:2,expectedProxies=phase_==3?0:1;const std::size_t sourceCount=std::size_t(std::ranges::count_if(identity.candidates,[](const RenderObjectIdentity3D& value){return!value.hlodProxy;})),proxyCount=std::size_t(std::ranges::count_if(identity.candidates,[](const RenderObjectIdentity3D& value){return value.hlodProxy;}));const bool identities=identity.complete&&identity.submissionRevision==submission_.ContentRevision()&&identity.hzbActive&&identity.tested==stats.lastSubmittedMeshInstances&&identity.hzbOccluded>=baselineCandidates_.size()&&identity.totalVisible<identity.tested&&identity.candidates==baselineCandidates_&&identity.candidates.size()==expectedSources+expectedProxies&&sourceCount==expectedSources&&proxyCount==expectedProxies&&identity.visible.empty()&&std::ranges::all_of(identity.candidates,[](const RenderObjectIdentity3D& value){return value.hlodClusterId==ClusterId;});
#else
  const bool identities=false;
#endif
  const bool passed=Validation(stats)&&boundary&&identities;
  if(!passed){
   std::cerr<<"hlod_phase="<<phase_<<" total="<<total<<" submitted="<<stats.lastSubmittedMeshInstances<<" clusters="<<capacity.consideredClusters<<'/'<<capacity.acceptedClusters<<'/'<<capacity.sourceFallbackClusters<<'/'<<capacity.proxyFallbackClusters<<'/'<<capacity.rejectedClusters<<" dropped="<<capacity.droppedInstances<<" identity="<<identities;
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
   std::cerr<<" receipt="<<identity.complete<<'/'<<identity.submissionRevision<<'/'<<identity.readbackSerial<<'/'<<identity.tested<<'/'<<identity.hzbOccluded<<'/'<<identity.totalVisible<<'/'<<identity.candidates.size()<<'/'<<identity.visible.size();
#endif
   std::cerr<<" submission="<<submission_.ContentRevision()<<" debug="<<stats.debugErrorCount<<'/'<<stats.debugWarningCount<<'\n';
  }
  return passed;
 }
 TerrainVegetationChunkSettings3D settings_{};SceneECSRegistry3D scene_{};ChunkedVegetationWorld3D vegetation_{};StaticMeshAssetLibrary3D library_{};ImmediateReceiptSource receipts_{};std::unique_ptr<TerrainVegetationWorldChunkActivationTarget3D> target_{};WorldChunkActivation3D activation_{};std::optional<HlodFrozenFrameToken3D> token_{};HlodSelectionView3D view_{};HlodRuntimeSelectionSettings3D runtime_{};MeshHandle3D filler_{},wall_{};StaticMeshSubmission3D submission_{};std::vector<RenderObjectIdentity3D> baselineCandidates_{};std::size_t phase_{},frames_{};std::uint64_t phaseHash_{};bool baselineValidated_{};
};
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,wchar_t*,int){try{EngineConfig config{};config.title=L"Aevrynd HLOD capacity and HZB";config.width=640;config.height=360;config.renderQuality=RenderQuality3D::Low;config.enableAutomaticMemoryTrim=false;Engine engine(config);engine.Run(std::make_unique<Game>(),instance,SW_HIDE);}catch(const std::exception& error){std::cerr<<"hlod_capacity_exception="<<error.what()<<'\n';return 20;}return Result;}
#endif
