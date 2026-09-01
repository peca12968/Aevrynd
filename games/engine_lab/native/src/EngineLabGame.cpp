#include "EngineLabGame.h"
#include "EngineLabResources.h"
#include <fstream>
#include <Aevrynd/ArchitectureMeshes3D.h>
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <Aevrynd/MaterialQuality3D.h>
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/ProceduralPlacement3D.h>
#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>

using namespace Aevrynd;
namespace {
constexpr Color Cyan{.08f,.92f,.82f,1},Gold{1.f,.72f,.22f,1},Ink{.72f,.86f,.84f,1};
constexpr Surface3D GroundSurface{.94f,0,0,.9f},BeaconSurface{.16f,.72f,.2f,1};
constexpr std::array<const char*,4> CameraNames={"FIRST PERSON","THIRD PERSON","GOD VIEW","ISOMETRIC 2.5D"};
uint64_t AverageWaitMicroseconds(const RendererStats& stats){return stats.resourceWaits?stats.totalResourceWaitNanoseconds/stats.resourceWaits/1000:0;}
uint64_t AverageGpuMicroseconds(const RendererStats& stats){return stats.gpuTimingSamples?stats.totalGpuFrameNanoseconds/stats.gpuTimingSamples/1000:0;}
JointTransform3D AnimationJoint(Vec3 translation,Vec3 rotation={}){return{translation,rotation,{1,1,1}};}
AnimationTrack3D AnimationTrack(std::size_t joint,std::initializer_list<AnimationKeyframe3D> frames){return{joint,frames};}
Vec3 TerrainPoint(const HeightField3D& terrain,Vec3 position,float offset=0){position.y=terrain.SampleHeight(position.x,position.z)+offset;return position;}
float LandscapeNoise(int x,int z){std::uint32_t value=std::uint32_t(x)*0x9e3779b9u^std::uint32_t(z)*0x85ebca6bu^0x31aeu;value^=value>>16;value*=0x7feb352du;value^=value>>15;return float(value&0xffffu)/65535.f;}
MeshLodPolicy3D WorldLod(float switchDistance){return{switchDistance,(std::max)(3.f,switchDistance*.16f),MeshLodDistanceMetric3D::BoundsSurface};}
struct LandscapePlacementContext {const HeightField3D* terrain{};};
bool LandscapePlacementHeight(float x,float z,float& y,void* opaque){const auto* context=static_cast<const LandscapePlacementContext*>(opaque);if(!context||!context->terrain)return false;const float riverX=25.f+std::sin(z*.105f)*5.f;if(std::abs(x-riverX)<7.f||(x>-23&&x<3&&z>7&&z<25)||(x>-5&&x<18&&z>20&&z<52))return false;y=context->terrain->SampleHeight(x,z);return std::isfinite(y);}
struct ForestDensityContext {Vec3 clearingCenter{};float clearingRadius{};};
float ForestClusterAcceptance(float x,float z,void* opaque){
    const auto* context=static_cast<const ForestDensityContext*>(opaque);if(!context||context->clearingRadius<=0)return 0;
    // 以兩種空間尺度形成森林群落，並在地標周圍保留自然林間空地。
    const float broad=.5f+.5f*std::sin(x*.071f+std::cos(z*.039f)*1.7f),detail=.5f+.5f*std::sin(x*.173f-z*.137f+1.4f);
    const float dx=x-context->clearingCenter.x,dz=z-context->clearingCenter.z,distance=std::sqrt(dx*dx+dz*dz);
    const float clearing=std::clamp((distance-context->clearingRadius)/(context->clearingRadius*.85f),.08f,1.f);
    return std::clamp((.18f+broad*.62f+detail*.2f)*clearing,0.f,1.f);
}
Material3D LandscapeMaterial(Material3D material,Color tint,float textureScale){material.tint=tint;material.textureScale=textureScale;return material;}
MaterialQuality3D MaterialQualityFor(RenderQuality3D quality){switch(quality){case RenderQuality3D::Low:return MaterialQuality3D::Low;case RenderQuality3D::Medium:return MaterialQuality3D::Medium;case RenderQuality3D::High:return MaterialQuality3D::High;default:return MaterialQuality3D::Ultra;}}
std::string CreateGpuValidationRunId(){GUID value{};if(FAILED(CoCreateGuid(&value)))throw std::runtime_error("GPU validation run GUID unavailable");return std::format("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",value.Data1,value.Data2,value.Data3,value.Data4[0],value.Data4[1],value.Data4[2],value.Data4[3],value.Data4[4],value.Data4[5],value.Data4[6],value.Data4[7]);}
std::uint32_t ConfiguredShadowCascadeCount(){std::array<wchar_t,16> value{};const DWORD length=GetEnvironmentVariableW(L"AEVRYND_ENGINE_LAB_SHADOW_CASCADES",value.data(),DWORD(value.size()));return length&&std::wstring_view(value.data(),length)==L"2"?2u:4u;}
Mesh3D CreateLandscapeRiverMesh(const HeightField3D& terrain){
    std::vector<Vec3> centers;std::vector<float> widths;centers.reserve(39);widths.reserve(39);
    for(int segment=0;segment<=38;++segment){const float z=-25.f+float(segment)*3.f,x=25.f+std::sin(z*.105f)*5.f,width=3.f+LandscapeNoise(segment,83)*.42f;const float waterHeight=(std::max)({terrain.SampleHeight(x,z),terrain.SampleHeight(x-width,z),terrain.SampleHeight(x+width,z)});centers.push_back({x,waterHeight,z});widths.push_back(width);}
    return CreateCurvedRibbonMesh3D(centers,widths,.045f);
}
void AddTree(Scene3D& scene,const HeightField3D& terrain,const MeshResource3D& treeHigh,const MeshResource3D& treeLow,Vec3 position,float height,float yaw,Material3D material){
    position.y=terrain.SampleHeight(position.x,position.z);const float scale=height/2.85f;scene.MeshLod(treeHigh,treeLow,WorldLod(46),{{position.x,position.y,position.z},{0,yaw,0},{scale,scale,scale}},material);
}
void AddHouse(Scene3D& scene,const HeightField3D& terrain,const MeshResource3D& cottageHigh,const MeshResource3D& cottageLow,Vec3 center,float scale,float yaw,Material3D material){
    center.y=terrain.SampleHeight(center.x,center.z);scene.MeshLod(cottageHigh,cottageLow,WorldLod(52),{center,{0,yaw,0},{scale,scale,scale}},material);
}
void MergeSceneStats(Scene3DStats& target,const Scene3DStats& source){target.inputTriangles+=source.inputTriangles;target.visibleTriangles+=source.visibleTriangles;target.culledTriangles+=source.culledTriangles;target.inputLines+=source.inputLines;target.visibleLines+=source.visibleLines;target.meshInstances+=source.meshInstances;target.visibleMeshInstances+=source.visibleMeshInstances;target.culledMeshInstances+=source.culledMeshInstances;target.distanceCulledMeshInstances+=source.distanceCulledMeshInstances;target.screenSizeCulledMeshInstances+=source.screenSizeCulledMeshInstances;target.lodMeshInstances+=source.lodMeshInstances;target.nearLodInstances+=source.nearLodInstances;target.farLodInstances+=source.farLodInstances;target.lodTrianglesSaved+=source.lodTrianglesSaved;target.lodTransitionInstances+=source.lodTransitionInstances;target.lodDualSubmittedInstances+=source.lodDualSubmittedInstances;target.lodTransitionTriangles+=source.lodTransitionTriangles;target.lodTransitionBudgetFallbacks+=source.lodTransitionBudgetFallbacks;target.shadowCasters+=source.shadowCasters;target.shadowInstances+=source.shadowInstances;target.shadowSkipped+=source.shadowSkipped;target.shadowOnlyCandidates+=source.shadowOnlyCandidates;target.shadowOnlyInstances+=source.shadowOnlyInstances;target.shadowOnlyBudgetDropped+=source.shadowOnlyBudgetDropped;target.shadowDistanceRejected+=source.shadowDistanceRejected;target.cpuFallbackUnavailable+=source.cpuFallbackUnavailable;for(std::size_t level=0;level<MaximumMeshLodLevels3D;++level)target.selectedLodLevels[level]+=source.selectedLodLevels[level];}
struct BundledMaterialAsset {int resourceId;std::filesystem::path relativePath;};
std::filesystem::path BundledMaterialRoot(){
    const std::filesystem::path root=std::filesystem::temp_directory_path()/"Aevrynd"/"EngineLab"/"material-packs-v1";
    const std::array assets={BundledMaterialAsset{IDR_GROUND_BASE_COLOR,"ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg"},BundledMaterialAsset{IDR_GROUND_ROUGHNESS,"ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg"},BundledMaterialAsset{IDR_GROUND_NORMAL,"ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"},BundledMaterialAsset{IDR_SOIL_BASE_COLOR,"poly-haven/aerial_ground_rock/1k/base-color.jpg"},BundledMaterialAsset{IDR_SOIL_ROUGHNESS,"poly-haven/aerial_ground_rock/1k/roughness.jpg"},BundledMaterialAsset{IDR_SOIL_NORMAL,"poly-haven/aerial_ground_rock/1k/normal-dx.jpg"},BundledMaterialAsset{IDR_ROCK_BASE_COLOR,"poly-haven/rock_surface/1k/base-color.jpg"},BundledMaterialAsset{IDR_ROCK_ROUGHNESS,"poly-haven/rock_surface/1k/roughness.jpg"},BundledMaterialAsset{IDR_ROCK_NORMAL,"poly-haven/rock_surface/1k/normal-dx.jpg"},BundledMaterialAsset{IDR_WOOD_BASE_COLOR,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg"},BundledMaterialAsset{IDR_WOOD_ROUGHNESS,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg"},BundledMaterialAsset{IDR_WOOD_NORMAL,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"},BundledMaterialAsset{IDR_BRICK_BASE_COLOR,"poly-haven/brick_floor/1k/base-color.jpg"},BundledMaterialAsset{IDR_BRICK_ROUGHNESS,"poly-haven/brick_floor/1k/roughness.jpg"},BundledMaterialAsset{IDR_BRICK_NORMAL,"poly-haven/brick_floor/1k/normal-dx.jpg"}};
    const HMODULE module=GetModuleHandleW(nullptr);
    for(const BundledMaterialAsset& asset:assets){const HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(asset.resourceId),RT_RCDATA);if(!resource)throw std::runtime_error("Bundled material resource is missing");const DWORD bytes=SizeofResource(module,resource);const HGLOBAL loaded=LoadResource(module,resource);const void* data=LockResource(loaded);if(!data||!bytes)throw std::runtime_error("Bundled material resource is invalid");const std::filesystem::path destination=root/asset.relativePath;std::error_code sizeError;const auto existingBytes=std::filesystem::file_size(destination,sizeError);if(!sizeError&&existingBytes==bytes)continue;std::filesystem::create_directories(destination.parent_path());std::ofstream output(destination,std::ios::binary|std::ios::trunc);output.write(static_cast<const char*>(data),bytes);if(!output)throw std::runtime_error("Bundled material extraction failed");}
    return root;
}
}

Aevrynd::ReusableMemoryStats EngineLabGame::GetReusableMemoryStats()const{const ReusableMemoryStats scene=scene_.MemoryStats(),landscape=landscapeScene_.MemoryStats(),physics=physicsWorld_.MemoryStats(),streaming=worldStreamer_.MemoryStats(),navigation=navigationWorkspace_.MemoryStats();const std::size_t retained=scene.retainedCapacityBytes+landscape.retainedCapacityBytes+physics.retainedCapacityBytes+streaming.retainedCapacityBytes+navigation.retainedCapacityBytes+terrainClipmap_.CapacityBytes()+authoredDynamicObjects_.capacity()*sizeof(WorldObjectHandle3D),reclaimable=scene.reclaimableCapacityBytes+landscape.reclaimableCapacityBytes+physics.reclaimableCapacityBytes+streaming.reclaimableCapacityBytes+navigation.reclaimableCapacityBytes+(authoredDynamicObjects_.capacity()>authoredDynamicObjects_.size()?(authoredDynamicObjects_.capacity()-authoredDynamicObjects_.size())*sizeof(WorldObjectHandle3D):0);return{retained,reclaimable,lastReusableMemoryReleasedBytes_,reusableMemoryTrimCount_};}
std::size_t EngineLabGame::TrimReusableMemory(){const std::size_t before=GetReusableMemoryStats().retainedCapacityBytes;scene_.TrimUnusedMemory();landscapeScene_.TrimUnusedMemory();physicsWorld_.TrimUnusedMemory();worldStreamer_.TrimUnusedMemory();navigationWorkspace_.TrimUnusedMemory(std::size_t(navigationGrid_.Settings().width)*navigationGrid_.Settings().depth);std::vector<WorldObjectHandle3D>(authoredDynamicObjects_).swap(authoredDynamicObjects_);const std::size_t after=GetReusableMemoryStats().retainedCapacityBytes;lastReusableMemoryReleasedBytes_=before>after?before-after:0;++reusableMemoryTrimCount_;return lastReusableMemoryReleasedBytes_;}

void EngineLabGame::OnStart(Engine &engine) {
  gpuValidationRunId_=CreateGpuValidationRunId();
  shadowCascadeCount_=ConfiguredShadowCascadeCount();
  cameraMode_ = 0;
  effectCycle_ = -1;
  effects_.Clear();
  automatic_ = true;
  cullingEnabled_ = true;
  instancingEnabled_ = true;
  backfaceCulling_ = true;
  lightingEnabled_ = true;
  cameraRig_.Reset(CameraViewMode3D::FirstPerson, {0, 3.f, -12});
  camera_ = cameraRig_.Camera();
  worldStreamer_.Configure({16, 2, 3, 4, 8, 0x31aeu});
  for (int index = 0; index < 7; ++index)
    worldStreamingStats_ = worldStreamer_.Update({0, 0, 25});
  MeshLodChainSettings3D automaticLodSettings;
  automaticLodSettings.targetTriangleRatios = {.6f, .3f, .08f};
  MeshProcessingError3D automaticLodError;
  if (!engine.CreateMeshLodResourceChain3D(
          Mesh3D::CreateUnitSphere(32, 20), automaticSphereLod_,
          automaticLodSettings, &automaticLodError))
    throw std::runtime_error("Automatic GPU LOD chain creation failed");
  if (!automaticSphereLod_.levels[0].cpuMesh ||
      std::ranges::any_of(
          std::span{automaticSphereLod_.levels}.subspan(
              1, automaticSphereLod_.levelCount - 1),
          [](const MeshResource3D &level) { return bool(level.cpuMesh); }))
    throw std::runtime_error("Automatic GPU LOD CPU retention failed");
  const std::size_t compactLodCpuBytes =
      engine.GetMemoryStats().meshCpu.residentBytes;
  MeshLodResourceChain3D allCpuLodProbe;
  if (!engine.CreateMeshLodResourceChain3D(
          Mesh3D::CreateUnitSphere(32, 20), allCpuLodProbe,
          automaticLodSettings, &automaticLodError,
          MeshLodCpuRetention3D::AllLevels) ||
      std::ranges::any_of(
          std::span{allCpuLodProbe.levels}.first(allCpuLodProbe.levelCount),
          [](const MeshResource3D &level) { return !level.cpuMesh; }) ||
      engine.GetMemoryStats().meshCpu.residentBytes <= compactLodCpuBytes)
    throw std::runtime_error("Automatic GPU LOD full CPU retention failed");
  engine.DestroyMeshLodResourceChain3D(allCpuLodProbe);
  MeshLodResourceChain3D automaticLodCacheProbe;
  if (!engine.CreateMeshLodResourceChain3D(
          Mesh3D::CreateUnitSphere(32, 20), automaticLodCacheProbe,
          automaticLodSettings, &automaticLodError))
    throw std::runtime_error("Automatic GPU LOD cache probe failed");
  for (std::size_t level = 0; level < automaticSphereLod_.levelCount; ++level)
    if (automaticSphereLod_.levels[level].handle !=
            automaticLodCacheProbe.levels[level].handle ||
        automaticSphereLod_.levels[level].cpuMesh !=
            automaticLodCacheProbe.levels[level].cpuMesh)
      throw std::runtime_error("Automatic GPU LOD cache mismatch");
  engine.DestroyMeshLodResourceChain3D(automaticLodCacheProbe);
  Mesh3D budgetProbeSource = Mesh3D::CreateUnitSphere(32, 20);
  for (Vertex3D &vertex : budgetProbeSource.vertices)
    vertex.color ^= 0x00010101u;
  std::vector<Mesh3D> budgetProbeCpuLevels;
  if (!MeshProcessing3D::GenerateLodChain(
          budgetProbeSource, budgetProbeCpuLevels, automaticLodSettings,
          nullptr, &automaticLodError))
    throw std::runtime_error("Automatic GPU LOD budget fixture failed");
  const auto meshBytes = [](const Mesh3D &mesh) {
    return mesh.vertices.size() * sizeof(Vertex3D) +
           mesh.indices.size() * sizeof(std::uint32_t);
  };
  const RendererStats beforeBudgetProbe = engine.GetRendererStats();
  engine.SetMeshMemoryBudget(beforeBudgetProbe.residentMeshBytes +
                             meshBytes(budgetProbeSource) +
                             meshBytes(budgetProbeCpuLevels[0]));
  MeshLodResourceChain3D rejectedBudgetChain;
  if (engine.CreateMeshLodResourceChain3D(
          budgetProbeSource, rejectedBudgetChain, automaticLodSettings,
          &automaticLodError) ||
      automaticLodError.code != MeshProcessingErrorCode3D::LimitExceeded)
    throw std::runtime_error("Automatic GPU LOD budget rejection failed");
  const RendererStats afterBudgetProbe = engine.GetRendererStats();
  if (afterBudgetProbe.residentMeshResources !=
          beforeBudgetProbe.residentMeshResources ||
      afterBudgetProbe.residentMeshReferences !=
          beforeBudgetProbe.residentMeshReferences ||
      afterBudgetProbe.residentMeshBytes != beforeBudgetProbe.residentMeshBytes)
    throw std::runtime_error(
        "Automatic GPU LOD rollback leaked a resident resource");
  engine.SetMeshMemoryBudget(64 * 1024 * 1024);
  boxMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());
  pyramidMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitPyramid());
  cylinderMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitCylinder(12));
  sphereHighMesh_ =
      engine.CreateMeshResource3D(Mesh3D::CreateUnitSphere(12, 8));
  sphereLowMesh_ = engine.CreateMeshResource3D(Mesh3D::CreateUnitSphere(6, 3));
  grassHighMesh_ = engine.CreateMeshResource3D(CreateGrassClumpMesh3D(11, 3));
  grassLowMesh_ = engine.CreateMeshResource3D(CreateGrassClumpMesh3D(4, 2));
  treeHighMesh_ = engine.CreateMeshResource3D(
      CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Near, 0x31aeu));
  treeLowMesh_ = engine.CreateMeshResource3D(
      CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Far, 0x31aeu));
  coniferHighMesh_ = engine.CreateMeshResource3D(
      CreateConiferTreeMesh3D(EnvironmentMeshDetail3D::Near, 0x48d2u));
  coniferLowMesh_ = engine.CreateMeshResource3D(
      CreateConiferTreeMesh3D(EnvironmentMeshDetail3D::Far, 0x48d2u));
  CottageMeshSettings3D cottageSettings{};
  cottageSettings.width = 5.8f;
  cottageSettings.depth = 5.2f;
  cottageSettings.wallHeight = 3.4f;
  cottageSettings.roofHeight = 1.7f;
  cottageSettings.seed = 0x31aeu;
  cottageHighMesh_ =
      engine.CreateMeshResource3D(CreateCottageMesh3D(cottageSettings));
  cottageSettings.detail = ArchitectureMeshDetail3D::Far;
  cottageLowMesh_ =
      engine.CreateMeshResource3D(CreateCottageMesh3D(cottageSettings));
  terrain_.Configure({{-64, 0, -32}, 65, 65, 2});
  HeightFieldUltraSettings3D ultraTerrain{};
  ultraTerrain.amplitude = 8.5f;
  ultraTerrain.frequency = .012f;
  ultraTerrain.ridgeStrength = .54f;
  ultraTerrain.valleyStrength = .22f;
  ultraTerrain.domainWarp = 22.f;
  ultraTerrain.octaves = 6;
  ultraTerrain.talusSlope = .82f;
  ultraTerrain.depositionStrength = .15f;
  if (!terrain_.GenerateUltra(0x31aeu, ultraTerrain))
    throw std::runtime_error("Ultra terrain generation failed");
  terrain_.FlattenCircle({-12, 0, 18}, 13, 0, 7);
  terrain_.FlattenCircle({8, 0, 36}, 18, 0, 8);
  riverMesh_ = engine.CreateMeshResource3D(CreateLandscapeRiverMesh(terrain_));
  HeightFieldMeshSettings3D terrainMeshSettings{
      {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, .74f};
  terrainMeshSettings.macroBiomeSeed = 0x31aeu;
  terrainMeshSettings.macroBiomeScale = .014f;
  terrainMeshSettings.macroBiomeStrength = 0;
  terrainMeshSettings.biomeLayerStrength = 1;
  terrainMeshSettings.grassBiomeColor = {1, 1, 1, 1};
  terrainMeshSettings.soilBiomeColor = {1, 1, 1, 1};
  terrainMeshSettings.rockBiomeColor = {1, 1, 1, 1};
  terrainMeshSettings.soilAltitudeNormalized = .38f;
  terrainMeshSettings.rockAltitudeNormalized = .7f;
  terrainMeshSettings.altitudeBlend = .14f;
  terrainMeshSettings.slopeRockNormalY = .7f;
  terrainMeshSettings.slopeBlend = .13f;
  terrainChunks_.clear();
  terrainChunks_.reserve(16);
  std::size_t highTerrainIndices{}, mediumTerrainIndices{}, lowTerrainIndices{};
  for (unsigned chunkZ = 0; chunkZ < 4; ++chunkZ)
    for (unsigned chunkX = 0; chunkX < 4; ++chunkX) {
      HeightFieldChunkLodSettings3D highSettings{
          chunkX * 16, chunkZ * 16, 16, 16, {1, true, 3.f}},
          mediumSettings{chunkX * 16, chunkZ * 16, 16, 16, {2, true, 3.f}},
          lowSettings{chunkX * 16, chunkZ * 16, 16, 16, {4, true, 3.f}};
      const Mesh3D highDefinition = terrain_.CreateChunkLodMesh(
                       highSettings, terrainMeshSettings),
                   mediumDefinition = terrain_.CreateChunkLodMesh(
                       mediumSettings, terrainMeshSettings),
                   lowDefinition = terrain_.CreateChunkLodMesh(
                       lowSettings, terrainMeshSettings);
      if (!highDefinition.Valid() || !mediumDefinition.Valid() ||
          !lowDefinition.Valid())
        throw std::runtime_error("Terrain chunk LOD generation failed");
      highTerrainIndices += highDefinition.indices.size();
      mediumTerrainIndices += mediumDefinition.indices.size();
      lowTerrainIndices += lowDefinition.indices.size();
      terrainChunks_.push_back({engine.CreateMeshResource3D(highDefinition),
                                engine.CreateMeshResource3D(mediumDefinition),
                                engine.CreateMeshResource3D(lowDefinition)});
    }
  if (terrainChunks_.size() != 16 ||
      mediumTerrainIndices >= highTerrainIndices ||
      lowTerrainIndices >= mediumTerrainIndices)
    throw std::runtime_error("Terrain chunk LOD validation failed");
  constexpr std::array<float, 2> terrainLodBands = {42.f, 78.f};
  if (!terrainClipmap_.Configure(
          {{-64, 0, -32}, 4, 4, 32.f, terrainLodBands, 6.f, 16, 16, 3}))
    throw std::runtime_error("Terrain clipmap configuration failed");
  for (int convergence = 0; convergence < 3; ++convergence)
    if (!terrainClipmap_.Update({0, 0, 25}))
      throw std::runtime_error("Terrain clipmap initialization failed");
  terrainClipmapStats_ = terrainClipmap_.Stats();
  if (terrainClipmapStats_.deferredChunks)
    throw std::runtime_error("Terrain clipmap convergence failed");
  const Mesh3D cacheDefinition = terrain_.CreateChunkLodMesh(
      {0, 0, 16, 16, {1, true, 3.f}}, terrainMeshSettings);
  for (int cacheRequest = 0; cacheRequest < 16; ++cacheRequest) {
    const MeshResource3D sharedTerrain =
        engine.CreateMeshResource3D(cacheDefinition);
    if (sharedTerrain.handle != terrainChunks_.front().high.handle ||
        sharedTerrain.cpuMesh != terrainChunks_.front().high.cpuMesh)
      throw std::runtime_error("Mesh cache validation failed");
    engine.DestroyMesh3D(sharedTerrain.handle);
  }
  const MeshLoadResult3D probe = LoadObjMeshFile(engine.ExecutableDirectory() /
                                                 "assets" / "engine_probe.obj");
  if (probe)
    probeMesh_ = engine.CreateMeshResource3D(probe.mesh);
  scene_.SetBoxMesh(boxMesh_);
  scene_.Reserve({0, 128, 2048, 512, 320});
  audioPing_ = engine.Audio().CreateSound(GenerateTone(660, .09f, .16f));
  engine.Audio().Play(audioPing_);
  worldBuilder_.Initialize(engine);
  physicsWorld_.Clear();
  authoredDynamicObjects_.clear();
  physicsImpulseCycle_ = -1;
  engine.SetMaterialTextureMemoryBudget(96 * 1024 * 1024);
  const std::filesystem::path materialsRoot = BundledMaterialRoot();
  constexpr MaterialResolutionPolicy3D resolutionPolicy{};
  const unsigned environmentResolution =
      resolutionPolicy.Select(MaterialDetailClass3D::Environment,
                              MaterialQualityFor(engine.RenderQuality()));
  MaterialTextureSetDefinition3D groundDefinition{
      materialsRoot / "ambientcg" / "ground037" / "1k" /
          "Ground037_1K-JPG_Color.jpg",
      materialsRoot / "ambientcg" / "ground037" / "1k" /
          "Ground037_1K-JPG_Roughness.jpg",
      environmentResolution,
      materialsRoot / "ambientcg" / "ground037" / "1k" /
          "Ground037_1K-JPG_NormalDX.jpg"};
  groundDefinition.additionalLayers = {
      MaterialTextureLayerDefinition3D{
          materialsRoot / "poly-haven" / "aerial_ground_rock" / "1k" /
              "base-color.jpg",
          materialsRoot / "poly-haven" / "aerial_ground_rock" / "1k" /
              "roughness.jpg",
          materialsRoot / "poly-haven" / "aerial_ground_rock" / "1k" /
              "normal-dx.jpg"},
      MaterialTextureLayerDefinition3D{
          materialsRoot / "poly-haven" / "rock_surface" / "1k" /
              "base-color.jpg",
          materialsRoot / "poly-haven" / "rock_surface" / "1k" /
              "roughness.jpg",
          materialsRoot / "poly-haven" / "rock_surface" / "1k" /
              "normal-dx.jpg"}};
  const MaterialTextureSetHandle3D releaseProbe =
      engine.CreateMaterialTextureSet3D({groundDefinition.baseColorPath,
                                         groundDefinition.roughnessPath, 512,
                                         groundDefinition.normalPath});
  engine.DestroyMaterialTextureSet3D(releaseProbe);
  materialLibrary_.Initialize(engine, materialsRoot);
  const std::array<MaterialAssetDefinition3D, 3> materialDefinitions = {
      MaterialAssetDefinition3D{"ground", groundDefinition},
      MaterialAssetDefinition3D{
          "wood",
          {"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg",
           "ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",
           environmentResolution,
           "ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"}},
      MaterialAssetDefinition3D{"brick",
                                {"poly-haven/brick_floor/1k/base-color.jpg",
                                 "poly-haven/brick_floor/1k/roughness.jpg",
                                 environmentResolution,
                                 "poly-haven/brick_floor/1k/normal-dx.jpg"}}};
  if (!materialLibrary_.LoadAll(materialDefinitions))
    throw std::runtime_error("Material library validation failed");
  const MaterialTextureSetHandle3D groundTextures =
                                       materialLibrary_.Textures("ground"),
                                   woodTextures =
                                       materialLibrary_.Textures("wood"),
                                   brickTextures =
                                       materialLibrary_.Textures("brick");
  for (int cacheRequest = 0; cacheRequest < 32; ++cacheRequest) {
    const MaterialTextureSetHandle3D sharedGroundProbe =
        engine.CreateMaterialTextureSet3D(groundDefinition);
    engine.DestroyMaterialTextureSet3D(sharedGroundProbe);
  }
  materialLibraryStats_ = materialLibrary_.Stats();
  groundMaterial_ = *materialLibrary_.Find("ground");
  woodMaterial_ = *materialLibrary_.Find("wood");
  brickMaterial_ = *materialLibrary_.Find("brick");
  groundMaterial_.tint = {.66f, .7f, .62f, 1};
  groundMaterial_.surface = GroundSurface;
  groundMaterial_.castsShadow = false;
  groundMaterial_.textureScale = .055f;
  groundMaterial_.normalScale = .68f;
  groundMaterial_.materialClass = MaterialClass3D::Terrain;
  woodMaterial_.surface = {.78f, 0, 0, .92f};
  woodMaterial_.textureScale = .42f;
  woodMaterial_.normalScale = .82f;
  brickMaterial_.surface = {.88f, 0, 0, .94f};
  brickMaterial_.textureScale = .36f;
  brickMaterial_.normalScale = 1.08f;
  worldBuilder_.Create("authoring_floor", PrimitiveShape3D::Box,
                       {{-12, .25f, 18}, {}, {10, .5f, 8}},
                       MaterialPreset3D::Stone, {1, 1, 1, 1},
                       RigidBodyType3D::Static, &physicsWorld_, groundTextures,
                       .28f);
  worldBuilder_.Create("authoring_backstop", PrimitiveShape3D::Box,
                       {{-12, 1.5f, 21.8f}, {}, {10, 2.5f, .35f}},
                       MaterialPreset3D::Stone, {1, 1, 1, 1},
                       RigidBodyType3D::Static, &physicsWorld_, brickTextures,
                       .42f);
  worldBuilder_.Create("material_ground_panel", PrimitiveShape3D::Box,
                       {{-15.6f, 3.f, 16}, {}, {3.2f, 5.f, .35f}},
                       MaterialPreset3D::Stone, {1, 1, 1, 1},
                       RigidBodyType3D::Static, &physicsWorld_, groundTextures,
                       .34f);
  worldBuilder_.Create(
      "material_wood_panel", PrimitiveShape3D::Box,
      {{-12, 3.f, 16}, {}, {3.2f, 5.f, .35f}}, MaterialPreset3D::Wood,
      {1, 1, 1, 1}, RigidBodyType3D::Static, &physicsWorld_, woodTextures, .5f);
  worldBuilder_.Create("material_brick_panel", PrimitiveShape3D::Box,
                       {{-8.4f, 3.f, 16}, {}, {3.2f, 5.f, .35f}},
                       MaterialPreset3D::Stone, {1, 1, 1, 1},
                       RigidBodyType3D::Static, &physicsWorld_, brickTextures,
                       .48f);
  constexpr std::array<PrimitiveShape3D, 5> shapes = {
      PrimitiveShape3D::Box, PrimitiveShape3D::Sphere,
      PrimitiveShape3D::Cylinder, PrimitiveShape3D::Cone,
      PrimitiveShape3D::Pyramid};
  constexpr std::array<MaterialPreset3D, 5> materials = {
      MaterialPreset3D::Wood, MaterialPreset3D::Metal, MaterialPreset3D::Glass,
      MaterialPreset3D::Emissive, MaterialPreset3D::Matte};
  constexpr std::array<Color, 5> tints = {
      Color{.5f, .22f, .07f, 1}, Color{.62f, .68f, .72f, 1},
      Color{.2f, .68f, 1, .4f}, Color{.12f, .92f, .75f, 1},
      Color{.72f, .2f, .14f, 1}};
  for (int index = 0; index < 10; ++index) {
    const float x = -16.2f + float(index % 5) * 2.05f,
                z = 16.8f + float(index / 5) * 2.1f,
                y = 2.4f + float(index % 3) * 1.15f;
    const MaterialTextureSetHandle3D textures = index % 3 == 0 ? woodTextures
                                                : index % 3 == 1
                                                    ? brickTextures
                                                    : groundTextures;
    authoredDynamicObjects_.push_back(worldBuilder_.Create(
        std::format("runtime_object_{}", index),
        shapes[std::size_t(index) % shapes.size()],
        {{x, y, z}, {0, float(index) * .17f, 0}, {1.05f, 1.05f, 1.05f}},
        materials[std::size_t(index) % materials.size()],
        {.9f, .9f, .9f, tints[std::size_t(index) % tints.size()].a},
        RigidBodyType3D::Dynamic, &physicsWorld_, textures, .65f));
  }
  animationSkeleton_.joints = {{-1, AnimationJoint({0, 1, 0})},
                               {0, AnimationJoint({0, 1.2f, 0})},
                               {1, AnimationJoint({0, 1.1f, 0})}};
  animationClip_ = {
      2,
      true,
      {AnimationTrack(0, {{0, AnimationJoint({0, 1, 0}, {0, 0, -.35f})},
                          {1, AnimationJoint({0, 1, 0}, {0, 0, .35f})},
                          {2, AnimationJoint({0, 1, 0}, {0, 0, -.35f})}}),
       AnimationTrack(1, {{0, AnimationJoint({0, 1.2f, 0}, {.45f, 0, 0})},
                          {1, AnimationJoint({0, 1.2f, 0}, {-.45f, 0, 0})},
                          {2, AnimationJoint({0, 1.2f, 0}, {.45f, 0, 0})}}),
       AnimationTrack(2, {{0, AnimationJoint({0, 1.1f, 0}, {0, -.5f, 0})},
                          {1, AnimationJoint({0, 1.1f, 0}, {0, .5f, 0})},
                          {2, AnimationJoint({0, 1.1f, 0}, {0, -.5f, 0})}})}};
  navigationGrid_.Configure({{-20, 0, 5}, 40, 40, 1, 1600, true});
  for (int z = 0; z < 40; ++z)
    if (z < 18 || z > 21)
      navigationGrid_.SetBlocked({20, z});
  navigationPath_.stats = navigationGrid_.FindPathInto(
      {-15, 0, 10}, {15, 0, 40}, navigationWorkspace_, navigationPath_.points);
  skyDomeMesh_ = engine.CreateMeshResource3D(CreateSkyDomeMesh3D(32, 16));
  mountainHighMesh_ = engine.CreateMeshResource3D(
      CreateMountainRangeMesh3D(EnvironmentMeshDetail3D::Near, 0x31aeu));
  mountainLowMesh_ = engine.CreateMeshResource3D(
      CreateMountainRangeMesh3D(EnvironmentMeshDetail3D::Far, 0x31aeu));
  rockHighMesh_ = engine.CreateMeshResource3D(
      CreateRockMesh3D(EnvironmentMeshDetail3D::Near, 0x714bu));
  rockLowMesh_ = engine.CreateMeshResource3D(
      CreateRockMesh3D(EnvironmentMeshDetail3D::Far, 0x714bu));
  BuildLandscapeScene();
  if (engine.DLSSOn())
    engine.ToggleDLSS();
  if(!captureDirectory_.empty()&&!shadowCaptureMode_){
    LoopbackNetworkSettings3D settings{};settings.lossPermille=200;settings.duplicatePermille=80;settings.reorderPermille=120;settings.minimumJitterTicks=0;settings.maximumJitterTicks=3;settings.seed=0x2a54c0de;
    networkTransport_=std::make_unique<LoopbackNetworkTransport3D>(settings);networkTransport_->RegisterEndpoint({1});networkTransport_->RegisterEndpoint({2});networkTransport_->RegisterEndpoint({3});
    ReplicationRuntimeSettings3D runtime{};runtime.interestLoadRadius=1;runtime.interestRetainRadius=2;replicationServer_=std::make_unique<ReplicationServer3D>(*networkTransport_,NetworkEndpoint3D{1},0x254,runtime);replicationServer_->AddClient(1,{2},{{-2,0,-1},{8,0,4}});replicationServer_->AddClient(2,{3},{{-2,0,-1},{12,0,8}});replicationClientA_=std::make_unique<ReplicationClient3D>(*networkTransport_,NetworkEndpoint3D{2},NetworkEndpoint3D{1},0x254,1,runtime);replicationClientB_=std::make_unique<ReplicationClient3D>(*networkTransport_,NetworkEndpoint3D{3},NetworkEndpoint3D{1},0x254,2,runtime);
    WorldDeltaStoreSettings3D storeSettings{};networkStoreRoot_=std::filesystem::temp_directory_path()/"Aevrynd"/"EngineLab"/std::format("network-durable-v254-{}",GetCurrentProcessId());std::error_code networkError;std::filesystem::remove_all(networkStoreRoot_,networkError);storeSettings.root=networkStoreRoot_;mutationStore_=std::make_unique<WorldChunkDeltaStore3D>(storeSettings);mutationTarget_=std::make_shared<EngineLabMutationTarget>();const auto authorize=[](const ReplicationMutationRequest3D& request,const DurableMutationAuthorizationContext3D& context){return request.clientId==1&&context.clientId==1&&context.sessionEpoch==0x254;};mutationAuthority_=std::make_unique<DurableMutationAuthority3D>(*mutationStore_,mutationTarget_,DurableMutationAuthoritySettings3D{0x254,8,16,authorize});
  }
}

void EngineLabGame::BuildLandscapeScene(){
    landscapeScene_.Clear();landscapeScene_.Reserve({0,0,768,512,1024});for(const TerrainChunkLod3D& state:terrainClipmap_.Chunks()){const TerrainChunkResources3D& chunk=terrainChunks_[std::size_t(state.z)*4+state.x];const MeshResource3D& selected=state.lod==0?chunk.high:state.lod==1?chunk.medium:chunk.low;landscapeScene_.Mesh(selected,{},groundMaterial_);}
    std::array<MeshLodLevel3D,MaximumMeshLodLevels3D> automaticLevels{};for(std::size_t level=0;level<automaticLevels.size();++level){automaticLevels[level].mesh=automaticSphereLod_.levels[level];automaticLevels[level].material=LandscapeMaterial(woodMaterial_,{1.f,.68f,.2f,1},.18f);if(level+1<automaticLevels.size()){automaticLevels[level].switchDistance=18.f+float(level)*18.f;automaticLevels[level].transitionWidth=4.f;}}landscapeScene_.MeshLodChain(automaticLevels,MeshLodDistanceMetric3D::BoundsSurface,{{-12,terrain_.SampleHeight(-12,30)+5.f,30},{},{2.2f,2.2f,2.2f}});
    // 水面使用獨立透明 pass，保留深度測試並讓河床與岸邊可透視。
    Material3D water=LandscapeMaterial(Material3D{{1,1,1,.68f},{.12f,.08f,.04f,1},false},{.2f,.62f,.78f,.68f},.25f);water.materialClass=MaterialClass3D::Water;Material3D foliageA=LandscapeMaterial(groundMaterial_,{.38f,.64f,.3f,1},.09f),foliageB=LandscapeMaterial(groundMaterial_,{.52f,.74f,.38f,1},.075f),treeMaterial{{.76f,.84f,.7f,1},{.86f,0,.012f,.9f},true};foliageA.materialClass=foliageB.materialClass=treeMaterial.materialClass=MaterialClass3D::Foliage;Material3D coniferMaterial=treeMaterial;coniferMaterial.tint={.48f,.62f,.42f,1};const Material3D trunk=LandscapeMaterial(woodMaterial_,{.72f,.56f,.38f,1},.52f),stone=LandscapeMaterial(brickMaterial_,{.72f,.76f,.74f,1},.28f),roof=LandscapeMaterial(brickMaterial_,{.48f,.2f,.14f,1},.3f);
    const Material3D rockMaterial=LandscapeMaterial(Material3D{{.68f,.64f,.58f,1},{.9f,.01f,0,.92f},true},{.68f,.64f,.58f,1},.24f);
    const Material3D sky{{1,1,1,1},{1,0,0,1},false};landscapeScene_.Mesh(skyDomeMesh_,{{0,0,25},{},{360,260,360}},sky);landscapeScene_.Mesh(riverMesh_,{},water);for(int segment=0;segment<38;segment+=2){const float z=-25.f+float(segment)*3.f,centerX=25.f+std::sin(z*.105f)*5.f;for(float side:{-1.f,1.f}){const float rockX=centerX+side*3.7f,variation=.78f+LandscapeNoise(segment,int(side*17.f))*0.42f;landscapeScene_.MeshLod(rockHighMesh_,rockLowMesh_,WorldLod(42),{{rockX,terrain_.SampleHeight(rockX,z)+.18f*variation,z},{.08f*side,float(segment)*.47f,.04f*side},{variation,.62f*variation,.86f*variation}},rockMaterial);}}
    LandscapePlacementContext placementContext{&terrain_};ForestDensityContext forestDensity{{0,0,18},13.f};ProceduralPlacementSettings3D placementSettings;placementSettings.bounds={{-58,-8,-26},{58,8,94}};placementSettings.targetCount=210;placementSettings.minimumSpacing=4.4f;placementSettings.minimumScale=.78f;placementSettings.maximumScale=1.24f;placementSettings.seed=0x31aeu;placementSettings.attemptsPerInstance=96;placementSettings.heightCallback=LandscapePlacementHeight;placementSettings.heightContext=&placementContext;placementSettings.acceptanceCallback=ForestClusterAcceptance;placementSettings.acceptanceContext=&forestDensity;const ProceduralPlacementResult3D trees=GenerateProceduralPlacement3D(placementSettings);if(trees.instances.size()<190)throw std::runtime_error("Open-world tree placement validation failed");std::vector<PlacementExclusion3D> exclusions;exclusions.reserve(trees.instances.size()+64);for(std::size_t treeIndex=0;treeIndex<trees.instances.size();++treeIndex){const PlacementInstance3D& placement=trees.instances[treeIndex];const bool conifer=(treeIndex%5)==1||(treeIndex%11)==4;AddTree(landscapeScene_,terrain_,conifer?coniferHighMesh_:treeHighMesh_,conifer?coniferLowMesh_:treeLowMesh_,placement.position,conifer?5.6f+placement.scale*2.8f:4.2f+placement.scale*2.4f,placement.yaw,conifer?coniferMaterial:treeMaterial);exclusions.push_back({placement.position,1.5f});}
    placementSettings.targetCount=55;placementSettings.minimumSpacing=2.7f;placementSettings.minimumScale=.65f;placementSettings.maximumScale=1.35f;placementSettings.seed=0x714bu;placementSettings.attemptsPerInstance=64;placementSettings.exclusions=exclusions;placementSettings.acceptanceCallback=nullptr;placementSettings.acceptanceContext=nullptr;const ProceduralPlacementResult3D rocks=GenerateProceduralPlacement3D(placementSettings);if(rocks.instances.size()<50)throw std::runtime_error("Open-world rock placement validation failed");for(const PlacementInstance3D& placement:rocks.instances){const float scale=placement.scale;landscapeScene_.MeshLod(rockHighMesh_,rockLowMesh_,WorldLod(48),{{placement.position.x,placement.position.y+.2f*scale,placement.position.z},{.06f*std::sin(placement.yaw),placement.yaw,.05f*std::cos(placement.yaw)},{scale,.7f*scale,.9f*scale}},rockMaterial);exclusions.push_back({placement.position,.7f});}
    placementSettings.targetCount=200;placementSettings.minimumSpacing=1.65f;placementSettings.minimumScale=.62f;placementSettings.maximumScale=1.42f;placementSettings.seed=0x9c31u;placementSettings.exclusions=exclusions;const ProceduralPlacementResult3D grass=GenerateProceduralPlacement3D(placementSettings);if(grass.instances.size()<185)throw std::runtime_error("Open-world grass placement validation failed");for(std::size_t index=0;index<grass.instances.size();++index){const PlacementInstance3D& placement=grass.instances[index];landscapeScene_.MeshLod(grassHighMesh_,grassLowMesh_,WorldLod(28),{placement.position,{0,placement.yaw,0},{placement.scale,placement.scale,placement.scale}},index%3?foliageA:foliageB);}landscapePlacementInstances_=trees.instances.size()+rocks.instances.size()+grass.instances.size();landscapePlacementAttempts_=trees.attempts+rocks.attempts+grass.attempts;
    Material3D cottageMaterial{{.82f,.78f,.7f,1},{.84f,.02f,.01f,.92f},true};constexpr std::array<Vec3,7> homes={Vec3{3,0,25},Vec3{11,0,25},Vec3{4,0,35},Vec3{13,0,36},Vec3{-2,0,45},Vec3{8,0,47},Vec3{17,0,46}};for(std::size_t index=0;index<homes.size();++index)AddHouse(landscapeScene_,terrain_,cottageHighMesh_,cottageLowMesh_,homes[index],.88f+float(index%3)*.055f,index%2?1.5708f:0.f,cottageMaterial);
    const Material3D mountain{{.72f,.76f,.74f,1},{.97f,0,0,.9f},false};constexpr std::array<Vec3,6> mountainRanges={Vec3{-70,0,-22},Vec3{-72,0,24},Vec3{-69,0,70},Vec3{70,0,-20},Vec3{72,0,27},Vec3{69,0,75}};for(std::size_t index=0;index<mountainRanges.size();++index){Vec3 position=mountainRanges[index];const float height=20.f+LandscapeNoise(int(index),41)*9.f;position.y=terrain_.SampleHeight(position.x,position.z)-1.4f;landscapeScene_.MeshLod(mountainHighMesh_,mountainLowMesh_,WorldLod(95),{position,{0,index<3?1.5708f:-1.5708f,0},{42.f,height,36.f}},mountain);}
    const float towerGround=terrain_.SampleHeight(0,18);landscapeScene_.Mesh(boxMesh_,{{0,towerGround+.5f,18},{},{7,1,7}},stone);landscapeScene_.Mesh(boxMesh_,{{0,towerGround+4.8f,18},{},{4.6f,8.6f,4.6f}},brickMaterial_);for(float x:{-2.15f,2.15f})for(float z:{15.85f,20.15f})landscapeScene_.Mesh(boxMesh_,{{x,towerGround+5.4f,z},{},{.65f,10,.65f}},trunk);landscapeScene_.Mesh(pyramidMesh_,{{0,towerGround+10.1f,18},{},{4.1f,3.2f,4.1f}},roof);landscapeScene_.MeshLod(sphereHighMesh_,sphereLowMesh_,WorldLod(45),{{0,towerGround+12.2f,18},{},{1.15f,1.15f,1.15f}},LandscapeMaterial(woodMaterial_,{1.f,.68f,.2f,1},.18f));
}

void EngineLabGame::OnStop(Engine& engine){
    if(networkRestartStore_){networkRestartStore_->WaitIdle();networkRestartStore_->TrimUnusedMemory();networkRestartStore_.reset();}if(mutationAuthority_){mutationAuthority_->Close();mutationStore_->WaitIdle();mutationAuthority_->Poll();mutationAuthority_.reset();}if(mutationStore_){mutationStore_->TrimUnusedMemory();mutationStore_.reset();}mutationTarget_.reset();replicationClientA_.reset();replicationClientB_.reset();replicationServer_.reset();if(networkTransport_){networkTransport_->Reset();networkTransport_->TrimUnusedMemory();networkTransport_.reset();}if(!networkStoreRoot_.empty()){std::error_code networkError;std::filesystem::remove_all(networkStoreRoot_,networkError);networkStoreRoot_.clear();}
    engine.DestroyMeshLodResourceChain3D(automaticSphereLod_);
    landscapeScene_.Clear();worldBuilder_.Shutdown(engine,&physicsWorld_);physicsWorld_.Clear();authoredDynamicObjects_.clear();materialLibrary_.Shutdown();materialLibraryStats_={};groundMaterial_={};woodMaterial_={};brickMaterial_={};scene_.SetBoxMesh({});
    for(MeshHandle3D handle:{boxMesh_.handle,pyramidMesh_.handle,cylinderMesh_.handle,grassHighMesh_.handle,grassLowMesh_.handle,treeHighMesh_.handle,treeLowMesh_.handle,coniferHighMesh_.handle,coniferLowMesh_.handle,cottageHighMesh_.handle,cottageLowMesh_.handle,riverMesh_.handle,skyDomeMesh_.handle,mountainHighMesh_.handle,mountainLowMesh_.handle,rockHighMesh_.handle,rockLowMesh_.handle,probeMesh_.handle,sphereHighMesh_.handle,sphereLowMesh_.handle})engine.DestroyMesh3D(handle);for(const TerrainChunkResources3D& chunk:terrainChunks_){engine.DestroyMesh3D(chunk.high.handle);engine.DestroyMesh3D(chunk.medium.handle);engine.DestroyMesh3D(chunk.low.handle);}terrainChunks_.clear();terrainClipmap_={};terrainClipmapStats_={};
    engine.Audio().DestroySound(audioPing_);boxMesh_={};pyramidMesh_={};cylinderMesh_={};grassHighMesh_={};grassLowMesh_={};treeHighMesh_={};treeLowMesh_={};coniferHighMesh_={};coniferLowMesh_={};cottageHighMesh_={};cottageLowMesh_={};riverMesh_={};skyDomeMesh_={};mountainHighMesh_={};mountainLowMesh_={};rockHighMesh_={};rockLowMesh_={};probeMesh_={};sphereHighMesh_=sphereLowMesh_={};audioPing_={};
}

void EngineLabGame::OnUpdate(Engine& engine,float deltaTime){
    auto& input=engine.GetInput();if(input.Pressed(VK_ESCAPE))engine.Quit();if(input.Pressed(VK_SPACE)){automatic_=!automatic_;if(!automatic_){const Vec3 anchor=cameraMode_==0?Vec3{0,3.f,-12}:Vec3{0,1.5f,25};cameraRig_.Reset(CameraViewMode3D(cameraMode_),anchor);camera_=cameraRig_.Camera();}}if(input.Pressed(VK_F5))cullingEnabled_=!cullingEnabled_;if(input.Pressed(VK_F6))instancingEnabled_=!instancingEnabled_;if(input.Pressed(VK_F7))backfaceCulling_=!backfaceCulling_;if(input.Pressed(VK_F8))lightingEnabled_=!lightingEnabled_;if(input.Pressed(VK_F9))engine.SetRenderQuality(NextRenderQuality3D(engine.RequestedRenderQuality()));
    if(shadowCaptureMode_){
      ++captureFrameCounter_;const RendererStats shadow=engine.GetRendererStats();shadowCascadeSplits_=shadow.directionalShadowCascadeSplits;shadowMapBytes_=(std::max)(shadowMapBytes_,shadow.directionalShadowMapBytes);shadowGpuNanoseconds_=(std::max)(shadowGpuNanoseconds_,shadow.lastGpuFrameNanoseconds);shadowContactEvaluationPasses_=(std::max)(shadowContactEvaluationPasses_,shadow.contactShadowEvaluationPasses);shadowManualMasks_=(std::max)(shadowManualMasks_,shadow.lastDirectionalShadowManualMasks);shadowFarCascadeInstances_=(std::max)(shadowFarCascadeInstances_,shadow.lastDirectionalShadowCascadeInstances[shadowCascadeCount_-1]);for(std::size_t cascade=0;cascade<MaximumDirectionalShadowCascades3D;++cascade){shadowCascadeRendered_[cascade]=(std::max)(shadowCascadeRendered_[cascade],shadow.lastDirectionalShadowCascadeRendered[cascade]);shadowCascadeReused_[cascade]=(std::max)(shadowCascadeReused_[cascade],shadow.lastDirectionalShadowCascadeReused[cascade]);}
      if(shadow.successfulFrameCaptures>shadowCaptureCompleted_){shadowCaptureCompleted_=shadow.successfulFrameCaptures;if(shadowCaptureCompleted_==1)contactDisabledLuminance_=shadow.lastCapturedAverageLuminance;else if(shadowCaptureCompleted_==2)contactEnabledLuminance_=shadow.lastCapturedAverageLuminance;}
      if(captureIndex_==6){constexpr std::array<const wchar_t*,6> names={L"contact-shadow-disabled.png",L"contact-shadow-enabled.png",L"far-cascade-receiver.png",L"day-shadow.png",L"dusk-shadow.png",L"night-shadow.png"};bool ready=true;for(const wchar_t* name:names)ready&=std::filesystem::is_regular_file(captureDirectory_/name);if(ready){const RendererStats gpu=engine.GetRendererStats();const std::uint32_t farMask=1u<<(shadowCascadeCount_-1);const bool effective=gpu.lastDirectionalShadowCascades==shadowCascadeCount_&&gpu.contactShadowsEnabled&&shadowContactEvaluationPasses_>0&&shadowMapBytes_>0&&shadowManualMasks_>0&&shadowFarCascadeInstances_>0;std::ofstream receipt(captureDirectory_/"shadow-demo-runtime-receipt.json",std::ios::binary|std::ios::trunc);receipt<<std::format("{{\"runId\":\"{}\",\"phase\":\"shadow-v255\",\"cascadeConfigured\":{},\"cascadeCount\":{},\"cascadeResolution\":2048,\"cascadeSplits\":[{:.6f},{:.6f},{:.6f},{:.6f}],\"cascadeRendered\":[{},{},{},{}],\"cascadeReused\":[{},{},{},{}],\"farCascadeMask\":{},\"farCascadeInstances\":{},\"manualMasks\":{},\"contactRequested\":1,\"contactExecuted\":{},\"contactEvaluationPasses\":{},\"contactDisabledLuminance\":{:.8f},\"contactEnabledLuminance\":{:.8f},\"contactDarkenedLuminance\":{:.8f},\"shadowBytes\":{},\"gpuFrameNanoseconds\":{},\"effective\":{},\"gpuValidationRequested\":{},\"gpuValidationAvailable\":{},\"gpuValidationExecuted\":{},\"gpuValidationPassed\":{},\"debugLayerEnabled\":{},\"gpuValidationErrors\":{},\"gpuValidationWarnings\":{},\"gpuValidationFirstErrorId\":{},\"gpuValidationSource\":\"D3D12InfoQueue\",\"gpuValidationEntry\":\"EngineLabShadowCapture\"}}",gpuValidationRunId_,shadowCascadeCount_,gpu.lastDirectionalShadowCascades,shadowCascadeSplits_[0],shadowCascadeSplits_[1],shadowCascadeSplits_[2],shadowCascadeSplits_[3],shadowCascadeRendered_[0],shadowCascadeRendered_[1],shadowCascadeRendered_[2],shadowCascadeRendered_[3],shadowCascadeReused_[0],shadowCascadeReused_[1],shadowCascadeReused_[2],shadowCascadeReused_[3],farMask,shadowFarCascadeInstances_,shadowManualMasks_,shadowContactEvaluationPasses_?1:0,shadowContactEvaluationPasses_,contactDisabledLuminance_,contactEnabledLuminance_,contactDisabledLuminance_-contactEnabledLuminance_,shadowMapBytes_,shadowGpuNanoseconds_,effective?1:0,gpu.gpuValidationRequested?1:0,gpu.gpuValidationAvailable?1:0,gpu.gpuValidationExecuted?1:0,gpu.gpuValidationPassed?1:0,gpu.debugLayerEnabled?1:0,gpu.debugErrorCount,gpu.debugWarningCount,gpu.debugFirstErrorId);receipt.close();if(!effective||(gpu.gpuValidationRequested&&(!gpu.gpuValidationAvailable||!gpu.gpuValidationExecuted||!gpu.gpuValidationPassed||!gpu.debugLayerEnabled||gpu.debugErrorCount)))ExitProcess(87);engine.Quit();return;}}
    }
    if(networkTransport_&&networkTick_<120){++networkTick_;replicationClientA_->SubmitInput(1800,900);replicationClientB_->SubmitInput(-1200,1500);replicationClientA_->Tick();replicationClientB_->Tick();if(networkTick_==50)replicationServer_->SetClientPosition(2,{{20,0,0},{0,0,0}});if(networkTick_==80)replicationServer_->SetClientPosition(2,{{-2,0,-1},{12,0,8}});networkTransport_->AdvanceTo(networkTick_);replicationServer_->Tick();const auto a=replicationClientA_->Predicted(),b=replicationClientB_->Predicted();if(const auto telemetry=replicationServer_->ClientTelemetry(1)){networkServerDespawns_+=telemetry->lastSnapshotDespawns;networkServerSnapshotEntities_+=telemetry->lastSnapshotEntities;}networkInterestA_.Update(a.position,networkInterestDeltaA_);networkInterestB_.Update(b.position,networkInterestDeltaB_);networkInterestEnteredA_+=networkInterestDeltaA_.entered.size();networkInterestLeftA_+=networkInterestDeltaA_.left.size();networkInterestEnteredB_+=networkInterestDeltaB_.entered.size();networkInterestLeftB_+=networkInterestDeltaB_.left.size();if(!networkMutationSubmitted_){networkMutationSubmitted_=true;for(std::uint64_t index=0;index<3;++index){WorldChunkDeltaSnapshot3D snapshot{};snapshot.key={{0x254,1},1,1,{std::int64_t(index),0,0}};snapshot.base={1,1,0x254,1,{}};snapshot.durableGeneration=1;ReplicationMutationRequest3D request{};request.sessionEpoch=0x254;request.clientRequestId=100+index;request.clientId=1;request.key=snapshot.key;if(index==0)request.operation.payload=PlaceBuildingDelta3D{{{10,1},7,3,{{0,0,0},{4,0,4}},{},{1,1,1},1}};else if(index==1)request.operation.payload=SetTerrainHeightDelta3D{4,4,2.5f};else request.operation.payload=RemoveVegetationDelta3D{{20,2}};if(mutationAuthority_->Submit(request,snapshot,networkTick_))++networkAuthorizedMutations_;}WorldChunkDeltaSnapshot3D denied{};denied.key={{0x254,1},1,1,{8,0,0}};denied.base={1,1,0x254,1,{}};denied.durableGeneration=1;ReplicationMutationRequest3D request{};request.sessionEpoch=0x254;request.clientRequestId=999;request.clientId=2;request.key=denied.key;request.operation.payload=SetTerrainHeightDelta3D{0,0,99};if(!mutationAuthority_->Submit(request,denied,networkTick_))++networkDeniedMutations_;}mutationAuthority_->Poll();ReplicationMutationReceipt3D receipt;while(mutationAuthority_->TryPopReceipt(receipt))if(receipt.durable)++networkDurableReceipts_;if(networkDurableReceipts_==3&&!networkRestartRequested_){networkRestartRequested_=true;WorldDeltaStoreSettings3D reopened{};reopened.root=networkStoreRoot_;networkRestartStore_=std::make_unique<WorldChunkDeltaStore3D>(reopened);networkRestartStore_->EnqueueLoad(700,0x254,{{0x254,1},1,1,{0,0,0}},{1,1,0x254,1,{}});} }if(networkRestartRequested_&&!networkRestartVerified_){WorldDeltaLoadResult3D loaded;if(networkRestartStore_->TryPopLoad(loaded))networkRestartVerified_=loaded.status==WorldDeltaLoadStatus3D::Loaded&&loaded.snapshot.operations.size()==1;}
    for(int index=0;index<4;++index)if(input.Pressed(VK_F1+index)){cameraMode_=index;automatic_=false;const Vec3 anchor=index==0?Vec3{0,3.f,-12}:Vec3{0,1.5f,25};cameraRig_.Reset(CameraViewMode3D(index),anchor);camera_=cameraRig_.Camera();engine.Audio().Play(audioPing_,{.12f,1.f+float(index)*.12f,false});}if(shadowCaptureMode_){cameraMode_=1;automatic_=true;}else if(!captureDirectory_.empty()){cameraMode_=int((std::min)(captureIndex_,std::size_t{3}));automatic_=true;}else if(automatic_)cameraMode_=int(engine.Time()/6.f)%4;
    if(!automatic_){const Vec2 mouseLook=input.Down(VK_SHIFT)?input.MouseDelta()*-.035f:Vec2{};CameraControl3D control;control.movement={float(input.Down('D'))-float(input.Down('A'))+input.GamepadLeftX(),float(input.Down('E'))-float(input.Down('Q')),float(input.Down('W'))-float(input.Down('S'))+input.GamepadLeftY()};control.look={float(input.Down(VK_RIGHT))-float(input.Down(VK_LEFT))+input.GamepadRightX()-mouseLook.x,float(input.Down(VK_UP))-float(input.Down(VK_DOWN))+input.GamepadRightY()+mouseLook.y};control.zoom=float(input.Down(VK_OEM_PLUS)||input.Down(VK_ADD))-float(input.Down(VK_OEM_MINUS)||input.Down(VK_SUBTRACT))+input.MouseWheel()*8.f;camera_=cameraRig_.Update(control,deltaTime);}
    if (!captureDirectory_.empty() && captureIndex_ == 6) {
      bool capturesReady = true;
      constexpr std::array<const wchar_t *, 6> names = {
          L"first-person.png",
          L"third-person.png",
          L"god-view.png",
          L"isometric-2-5d.png",
          L"network-replication-overview.png",
          L"network-interest-durability.png"};
      for (const wchar_t *name : names)
        capturesReady &=
            std::filesystem::is_regular_file(captureDirectory_ / name);
      if (capturesReady) {
        const auto gpu = engine.GetRendererStats();
        const auto transport = networkTransport_->Stats();
        const auto server = replicationServer_->Stats();
        const auto a = replicationServer_->ClientTelemetry(1).value_or(
            ReplicationServerClientTelemetry3D{});
        const auto b = replicationServer_->ClientTelemetry(2).value_or(
            ReplicationServerClientTelemetry3D{});
        const auto authority = mutationAuthority_->Stats();
        std::ofstream receipt(captureDirectory_ /
                                  "network-runtime-receipt.json",
                              std::ios::binary | std::ios::trunc);
        const auto pa = replicationClientA_->Predicted();
        const auto pb = replicationClientB_->Predicted();
        receipt << std::format(
            "{{\"runId\":\"{}\",\"phase\":\"network-v254\",\"seed\":{},\"serverTick\":{},"
            "\"serverTickEpoch\":0,\"serverTickRelative\":{},\"serverTickStep\":1,"
            "\"serverTickSource\":\"ReplicationServer3D\","
            "\"clients\":{},\"snapshots\":{},\"full\":{},\"delta\":{},\"acks\":"
            "{},\"loss\":{},\"duplicate\":{},\"reorder\":{},"
            "\"client1Interest\":{},\"client2Interest\":{},\"visible1\":{},"
            "\"visible2\":{},\"interestEntered\":{},\"interestExited\":{},"
            "\"reentries\":{},\"despawns\":{},\"authorized\":{},\"durable\":{},"
            "\"denied\":{},\"committed\":{},\"aborted\":{},\"restart\":{},"
            "\"pending\":{},\"queuePackets\":{},\"packetCapacity\":{},"
            "\"queueBytes\":{},\"byteCapacity\":{},\"auth1ChunkX\":{},"
            "\"auth1OffsetX\":{:.3f},\"pred1ChunkX\":{},\"pred1OffsetX\":{:.3f}"
            ",\"auth2ChunkX\":{},\"auth2OffsetX\":{:.3f},\"pred2ChunkX\":{},"
            "\"pred2OffsetX\":{:.3f},\"gpuValidationRequested\":{},"
            "\"gpuValidationAvailable\":{},\"gpuValidationExecuted\":{},"
            "\"gpuValidationPassed\":{},\"debugLayerEnabled\":{},"
            "\"gpuValidationErrors\":{},\"gpuValidationWarnings\":{},"
            "\"gpuValidationFirstErrorId\":{},\"gpuValidationSource\":"
            "\"D3D12InfoQueue\",\"gpuValidationEntry\":\"EngineLabCapture\"}}",
            gpuValidationRunId_,0x2a54c0deull, server.ticks, server.ticks,
            replicationServer_->ClientCount(),
            server.snapshotsSent, server.fullSnapshots, server.deltaSnapshots,
            server.acks, transport.lost, transport.duplicated,
            transport.reordered, a.interestChunks, b.interestChunks,
            a.visibleEntities, b.visibleEntities,
            a.interestEntered + b.interestEntered,
            a.interestExited + b.interestExited,
            a.entityReentries + b.entityReentries,
            a.entityDespawns + b.entityDespawns, networkAuthorizedMutations_,
            networkDurableReceipts_, networkDeniedMutations_,
            mutationTarget_->commits, mutationTarget_->aborts,
            networkRestartVerified_ ? 1 : 0, authority.pending,
            transport.queuedPackets, networkTransport_->PacketCapacity(),
            transport.queuedBytes, networkTransport_->ByteCapacity(),
            a.authoritativePosition.chunk.x, a.authoritativePosition.offset.x,
            pa.position.chunk.x, pa.position.offset.x,
            b.authoritativePosition.chunk.x, b.authoritativePosition.offset.x,
            pb.position.chunk.x, pb.position.offset.x,
            gpu.gpuValidationRequested ? 1 : 0,
            gpu.gpuValidationAvailable ? 1 : 0,
            gpu.gpuValidationExecuted ? 1 : 0, gpu.gpuValidationPassed ? 1 : 0,
            gpu.debugLayerEnabled ? 1 : 0, gpu.debugErrorCount,
            gpu.debugWarningCount, gpu.debugFirstErrorId);
        receipt.close();
        {
          std::ofstream diagnostics(captureDirectory_ /
                                        "gpu-validation-diagnostics.txt",
                                    std::ios::binary | std::ios::trunc);
          diagnostics << "runId="<<gpuValidationRunId_<<"\nsource=D3D12InfoQueue\nentry=EngineLabCapture\nerrors=" << gpu.debugErrorCount
                      << " warnings=" << gpu.debugWarningCount << " ids=";
          for (size_t index = 0; index < gpu.debugErrorIdCount; ++index)
            diagnostics << '\n'
                        << gpu.debugErrorIds[index] << '\t'
                        << gpu.debugErrorDescriptions[index].substr(0, 512);
          diagnostics << "\nwarning-ids=";
          for (size_t index = 0; index < gpu.debugWarningIdCount; ++index)
            diagnostics << '\n'
                        << gpu.debugWarningIds[index] << '\t'
                        << gpu.debugWarningOccurrences[index] << '\t'
                        << gpu.debugWarningDescriptions[index].substr(0, 512);
        }
        if (gpu.gpuValidationRequested &&
            (!gpu.gpuValidationAvailable || !gpu.gpuValidationExecuted ||
             !gpu.gpuValidationPassed || !gpu.debugLayerEnabled ||
             gpu.debugErrorCount))
          ExitProcess(86);
        engine.Quit();
        return;
      }
    }
    physicsWorld_.Step(deltaTime,2);worldBuilder_.Synchronize(physicsWorld_);const int impulseCycle=int(engine.Time()/3.f);if(impulseCycle!=physicsImpulseCycle_&&!authoredDynamicObjects_.empty()){physicsImpulseCycle_=impulseCycle;const WorldObjectHandle3D handle=authoredDynamicObjects_[std::size_t(impulseCycle)%authoredDynamicObjects_.size()];if(const WorldObject3D* object=worldBuilder_.Get(handle))physicsWorld_.ApplyImpulse(object->rigidBody,{std::sin(engine.Time())*2.2f,4.8f,std::cos(engine.Time())*2.2f});}
    const int effectCycle=int(engine.Time()*2.f);if(effectCycle!=effectCycle_){effectCycle_=effectCycle;ParticleBurst2D effect{};effect.minimumSpeed=25;effect.maximumSpeed=75;effect.minimumLife=.35f;effect.maximumLife=.7f;effect.gravity=18;effect.shape=ParticleShape2D(effectCycle%3);effects_.EmitBurst({640+std::sin(engine.Time())*180,390},effectCycle%2?Cyan:Gold,24,effect,uint32_t(effectCycle+1));const Vec3 forward=Normalize(camera_.target-camera_.position),right=Normalize(Cross(camera_.up,forward)),source{std::sin(engine.Time())*18,3,25+std::cos(engine.Time())*18};engine.Audio().PlaySpatial(audioPing_,AudioSpatial3D{camera_.position,right,source,70,1.2f,.12f,1.f+float(effectCycle%4)*.08f});}effects_.Update(deltaTime);worldStreamingStats_=worldStreamer_.Update({std::sin(engine.Time()*.4f)*40,0,25+std::cos(engine.Time()*.31f)*40});const RendererStats renderer=engine.GetRendererStats();const AudioStats audio=engine.Audio().GetStats();
    engine.SetTitle(std::format(L"Aevrynd Engine Lab | {} | OUT {}x{} R{} | AUDIO {} E{} V{} P{}/{} | OBJ {}/{} | MAT {}/{} | HIT {:.1f} Q {}/{} | MESH {}/{} CULL {} | LOD N{} F{} S{} | CSM {} C{} D{} {}MB S{:.0f} | PLACE {}/{} | INST {} O{} V{} I{} | BUF {}/{}/{}KB H{} B{}MB R{} A{} P{} | TEX {}/{}/{}MB H{} B{}MB R{} S{}/{}MB A{} P{} | GPU {}/{}US | W {}/{}US DROP {}",cameraMode_+1,renderer.outputWidth,renderer.outputHeight,renderer.resizeCount,audio.deviceAvailable?L"ON":L"OFF",audio.initializationError,audio.activeVoices,audio.successfulPlays,audio.droppedPlays,probeMesh_.vertexCount,probeMesh_.TriangleCount(),renderer.lastSubmittedTexturedMeshInstances,renderer.lastSubmittedMeshInstances,sceneHit_?sceneHit_.distance:0.f,queryStats_.trianglesTested,queryStats_.boundsTested,sceneStats_.visibleMeshInstances,sceneStats_.meshInstances,sceneStats_.culledMeshInstances,sceneStats_.nearLodInstances,sceneStats_.farLodInstances,sceneStats_.lodTrianglesSaved,renderer.lastDirectionalShadowCascades,renderer.lastDirectionalShadowCasters,renderer.lastDirectionalShadowDrawCalls,renderer.directionalShadowMapBytes/1048576,renderer.directionalShadowCascadeSplit,landscapePlacementInstances_,landscapePlacementAttempts_,renderer.lastSubmittedMeshInstances,renderer.lastSubmittedOverlayInstances,renderer.lastSubmittedWorldVertices,renderer.lastSubmittedWorldIndices,renderer.residentMeshes,renderer.residentMeshReferences,(renderer.residentMeshBytes+512)/1024,renderer.meshCacheHits,renderer.meshMemoryBudgetBytes/1048576,renderer.meshBudgetRejections,renderer.meshUploadBufferAllocations,renderer.peakMeshUploadStagingResources,renderer.residentMaterialTextureSets,renderer.residentMaterialTextureReferences,(renderer.residentTextureBytes+524288)/1048576,renderer.materialTextureCacheHits,renderer.materialTextureMemoryBudgetBytes/1048576,renderer.materialTextureBudgetRejections,(renderer.peakMaterialTextureCpuStagingBytes+524288)/1048576,(renderer.peakMaterialTextureUploadStagingBytes+524288)/1048576,renderer.materialTextureUploadBufferAllocations,renderer.peakMaterialTextureUploadStagingResources,renderer.lastGpuFrameNanoseconds/1000,AverageGpuMicroseconds(renderer),renderer.resourceWaits,AverageWaitMicroseconds(renderer),renderer.totalDroppedVertices+renderer.totalDroppedWorldVertices+renderer.totalDroppedWorldIndices+renderer.totalDroppedMeshInstances));
}

void EngineLabGame::OnRender(Engine& engine,float){
    const auto ta=networkTransport_?replicationServer_->ClientTelemetry(1).value_or(ReplicationServerClientTelemetry3D{}):ReplicationServerClientTelemetry3D{};
    const auto tb=networkTransport_?replicationServer_->ClientTelemetry(2).value_or(ReplicationServerClientTelemetry3D{}):ReplicationServerClientTelemetry3D{};
    auto& draw=engine.Draw();const float time=engine.Time();const bool contactEnabled=!shadowCaptureMode_||captureIndex_!=0;DirectionalLight3D sun{{-.48f,-1.f,.26f},.42f,{1.f,.94f,.82f,1},1.08f};if(shadowCaptureMode_&&captureIndex_==4)sun={{-.72f,-.32f,.18f},.38f,{1.f,.54f,.25f,1},.78f};else if(shadowCaptureMode_&&captureIndex_==5)sun={{-.25f,-.6f,.42f},.55f,{.38f,.48f,1.f,1},.24f};draw.SetExposureCompensation(-.15f);draw.SetBackfaceCulling(backfaceCulling_);draw.SetDirectionalShadowMapEnabled(true);draw.SetDirectionalShadowCascades({shadowCascadeCount_,2048,.1f,180.f,.65f,.08f});draw.SetContactShadows({contactEnabled,12,1.5f,.04f,.8f});draw.SetDirectionalLight(lightingEnabled_?sun:DirectionalLight3D{{-.48f,-1.f,.26f},1.f,{1,1,1,1},0.f});if(lightingEnabled_){draw.AddPointLight({{-12,4.2f,14},18,{1.f,.68f,.34f,1},2.2f});draw.AddPointLight({{7,4.5f,29},16,{1.f,.58f,.26f,1},1.75f});draw.AddPointLight({{-7,4.f,42},15,{1.f,.72f,.4f,1},1.55f});}scene_.Clear();
    worldBuilder_.Submit(scene_);for(const WorldChunk3D& chunk:worldStreamer_.ResidentChunks()){const float half=worldStreamer_.Settings().chunkSize*.5f;const Color color=chunk.coordinate==worldStreamingStats_.observerChunk?Gold:Color{.1f,.55f,.5f,.48f};const float y=terrain_.SampleHeight(chunk.center.x,chunk.center.z)+.05f;scene_.Line({chunk.center.x-half,y,chunk.center.z-half},{chunk.center.x+half,y,chunk.center.z-half},1.4f,color);scene_.Line({chunk.center.x-half,y,chunk.center.z-half},{chunk.center.x-half,y,chunk.center.z+half},1.4f,color);}
    SampleAnimationClipInto(animationSkeleton_,animationClip_,shadowCaptureMode_?0.f:time,animationPose_,CreateTranslationMatrix({-6,0,18}));const AnimationPose3D& animated=animationPose_;for(std::size_t joint=0;joint<animated.globalMatrices.size();++joint){const Vec3 position=TransformPoint(animated.globalMatrices[joint],{});scene_.Mesh(boxMesh_,{position,animated.localTransforms[joint].rotation,{.48f,joint==0?1.7f:1.35f,.48f}},woodMaterial_);}for(std::size_t point=1;point<navigationPath_.points.size();++point)scene_.Line(TerrainPoint(terrain_,navigationPath_.points[point-1],.08f),TerrainPoint(terrain_,navigationPath_.points[point],.08f),4,Gold);Camera3D camera=camera_;const Vec3 focus{0,1.5f,25};
    for(int z=0;z<40;++z)if(navigationGrid_.TraversalCost({20,z})==0){const Vec3 position=TerrainPoint(terrain_,navigationGrid_.WorldPosition({20,z}),.5f);scene_.Mesh(boxMesh_,{position,{},{.82f,1,.82f}},brickMaterial_);}if(shadowCaptureMode_)camera=captureIndex_==2?MakeThirdPersonCamera({30,5,70},-.72f,.22f,88):MakeThirdPersonCamera(focus,.54f,.31f,38);else if(automatic_)switch(cameraMode_){case 0:camera=MakeFirstPersonCamera({20,3.f,-10},std::sin(time*.22f)*.08f,-.08f);break;case 1:camera=MakeThirdPersonCamera(focus,std::sin(time*.28f)*.65f,.35f,38);break;case 2:camera=MakeOverheadCamera(focus,42,true);break;default:camera=MakeIsometricCamera(focus,48,32);break;}
    if(!terrainClipmap_.Update(camera.position))throw std::runtime_error("Terrain clipmap update failed");terrainClipmapStats_=terrainClipmap_.Stats();if(terrainClipmapStats_.changedChunks)BuildLandscapeScene();
    const Ray3D pickRay=ScreenPointToRay(camera,{1280,720},{640,360});SceneQueryStats3D landscapeQuery{},dynamicQuery{};sceneHit_=landscapeScene_.Raycast(pickRay,camera.projection.farPlane,&landscapeQuery);const SceneHit3D dynamicHit=scene_.Raycast(pickRay,sceneHit_?sceneHit_.distance:camera.projection.farPlane,&dynamicQuery);if(dynamicHit)sceneHit_=dynamicHit;queryStats_={};queryStats_.boundsTested=landscapeQuery.boundsTested+dynamicQuery.boundsTested;queryStats_.boundsRejected=landscapeQuery.boundsRejected+dynamicQuery.boundsRejected;queryStats_.trianglesTested=landscapeQuery.trianglesTested+dynamicQuery.trianglesTested;queryStats_.bvhNodesTested=landscapeQuery.bvhNodesTested+dynamicQuery.bvhNodesTested;queryStats_.bvhNodesRejected=landscapeQuery.bvhNodesRejected+dynamicQuery.bvhNodesRejected;queryStats_.bvhLeavesVisited=landscapeQuery.bvhLeavesVisited+dynamicQuery.bvhLeavesVisited;queryStats_.bvhObjectCandidates=landscapeQuery.bvhObjectCandidates+dynamicQuery.bvhObjectCandidates;queryStats_.meshBvhNodesTested=landscapeQuery.meshBvhNodesTested+dynamicQuery.meshBvhNodesTested;queryStats_.meshBvhNodesRejected=landscapeQuery.meshBvhNodesRejected+dynamicQuery.meshBvhNodesRejected;queryStats_.meshBvhLeavesVisited=landscapeQuery.meshBvhLeavesVisited+dynamicQuery.meshBvhLeavesVisited;queryStats_.meshTriangleCandidates=landscapeQuery.meshTriangleCandidates+dynamicQuery.meshTriangleCandidates;queryStats_.bvhBuilds=landscapeQuery.bvhBuilds+dynamicQuery.bvhBuilds;queryStats_.meshBvhBuilds=landscapeQuery.meshBvhBuilds+dynamicQuery.meshBvhBuilds;if(sceneHit_){const float marker=.55f;scene_.Line(sceneHit_.position-Vec3{marker,0,0},sceneHit_.position+Vec3{marker,0,0},4,Gold);scene_.Line(sceneHit_.position-Vec3{0,marker,0},sceneHit_.position+Vec3{0,marker,0},4,Gold);scene_.Line(sceneHit_.position-Vec3{0,0,marker},sceneHit_.position+Vec3{0,0,marker},4,Gold);scene_.Line(sceneHit_.position,sceneHit_.position+sceneHit_.normal*1.8f,3,Cyan);}
    Color atmosphere{.34f,.43f,.5f,.72f};if(shadowCaptureMode_&&captureIndex_==4)atmosphere={.45f,.23f,.18f,.78f};else if(shadowCaptureMode_&&captureIndex_==5)atmosphere={.05f,.08f,.18f,.9f};engine.SetClearColor({atmosphere.r*.5f,atmosphere.g*.7f,atmosphere.b*.9f,1});draw.SetAtmosphere({atmosphere,camera.position,58,175,true});const Scene3DSubmissionOptions submission{cullingEnabled_,instancingEnabled_,{instancingEnabled_,.025f,.16f,82.f,512},{135.f,.28f}};sceneStats_={};MergeSceneStats(sceneStats_,landscapeScene_.Submit(draw,camera,{1280,720},submission));MergeSceneStats(sceneStats_,scene_.Submit(draw,camera,{1280,720},submission));if(shadowCaptureMode_){const uint8_t farMask=uint8_t(1u<<(shadowCascadeCount_-1));draw.MeshInstance(boxMesh_.handle,CreateTransformMatrix({30,12,105},{},{8,18,8}),Gold,GroundSurface,MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),true,1.f,0,false,farMask,ShadowCascadeSelection3D::Manual);}scene_.Clear();const RendererStats renderer=engine.GetRendererStats();
    effects_.Render(draw);draw.Quad({640,39},{1280,78},{.01f,.025f,.04f,.96f});draw.Text({28,18},"AEVRYND ENGINE LAB",3.8f,Cyan);draw.Text({420,18},CameraNames[cameraMode_],3.1f,Gold);draw.Text({730,18},std::format("DEV {} | CC0 PBR {}/{} | LIVE {}/{} | TX{}/{} {}/{}MB H{} D{}/{} {}MB",RenderQualityName3D(engine.RenderQuality()),materialLibraryStats_.materials,materialLibraryStats_.uniqueTextureSets,renderer.lastSubmittedTexturedMeshInstances,renderer.lastSubmittedMeshInstances,renderer.residentMaterialTextureSets,renderer.residentMaterialTextureReferences,(renderer.residentTextureBytes+524288)/1048576,(renderer.residentTextureAllocationBytes+524288)/1048576,renderer.materialTextureCacheHits,renderer.materialDerivedDataCacheHits,renderer.materialDerivedDataCacheMisses,(renderer.materialDerivedDataCacheResidentBytes+524288)/1048576),1.35f,materialLibraryStats_.materials==3?Cyan:Color{.92f,.22f,.14f,1});if(cameraMode_==0){draw.Quad({640,96},{760,38},{.01f,.025f,.04f,.82f});draw.Text({335,83},"GROUND     WOOD      BRICK",2.1f,Gold);}
    draw.Quad({640,674},{1280,92},{.01f,.025f,.04f,.96f});draw.Text({28,638},std::format("CSM{} SPLIT {:.1f}/{:.1f}/{:.1f}/{:.1f} R {}/{}/{}/{} U {}/{}/{}/{} CONTACT {} E{} DARK {:.4f} {}MB GPU {}/{}US",renderer.lastDirectionalShadowCascades,renderer.directionalShadowCascadeSplits[0],renderer.directionalShadowCascadeSplits[1],renderer.directionalShadowCascadeSplits[2],renderer.directionalShadowCascadeSplits[3],renderer.lastDirectionalShadowCascadeRendered[0],renderer.lastDirectionalShadowCascadeRendered[1],renderer.lastDirectionalShadowCascadeRendered[2],renderer.lastDirectionalShadowCascadeRendered[3],renderer.lastDirectionalShadowCascadeReused[0],renderer.lastDirectionalShadowCascadeReused[1],renderer.lastDirectionalShadowCascadeReused[2],renderer.lastDirectionalShadowCascadeReused[3],renderer.contactShadowsEnabled?"ON":"OFF",renderer.contactShadowEvaluationPasses,contactDisabledLuminance_-contactEnabledLuminance_,renderer.directionalShadowMapBytes/1048576,renderer.lastGpuFrameNanoseconds/1000,AverageGpuMicroseconds(renderer)),1.05f,Ink);draw.Text({28,674},std::format("WORLD V {} I {} INST {} DRAW {} PLACE {}/{} PICK {:.1f} BVH {}/{} C{} CAP {}/{} L{:.2f} DROP {} CLIP L{} C{} D{} GBV {}/{}/{}/{}/{} E{} W{}",renderer.lastSubmittedWorldVertices,renderer.lastSubmittedWorldIndices,renderer.lastSubmittedMeshInstances,renderer.lastDrawCalls,landscapePlacementInstances_,landscapePlacementAttempts_,sceneHit_?sceneHit_.distance:0.f,queryStats_.bvhNodesTested,queryStats_.bvhNodesRejected,queryStats_.bvhObjectCandidates,renderer.successfulFrameCaptures,renderer.rejectedFrameCaptures,renderer.lastCapturedAverageLuminance,renderer.totalDroppedVertices+renderer.totalDroppedWorldVertices+renderer.totalDroppedWorldIndices+renderer.totalDroppedMeshInstances+renderer.totalDroppedPointLights,terrainClipmapStats_.maximumLod,terrainClipmapStats_.changedChunks,terrainClipmapStats_.deferredChunks,renderer.gpuValidationRequested,renderer.gpuValidationAvailable,renderer.gpuValidationExecuted,renderer.debugLayerEnabled,renderer.gpuValidationPassed,renderer.debugErrorCount,renderer.debugWarningCount),1.05f,Gold);if(networkTransport_&&captureIndex_>=4){const auto net=networkTransport_->Stats();const auto server=replicationServer_->Stats();const auto a=replicationClientA_->Stats();const auto b=replicationClientB_->Stats();const auto ta=replicationServer_->ClientTelemetry(1).value_or(ReplicationServerClientTelemetry3D{});const auto tb=replicationServer_->ClientTelemetry(2).value_or(ReplicationServerClientTelemetry3D{});draw.Quad({640,520},{1160,180},{.01f,.025f,.04f,.94f});draw.Text({92,444},"DETERMINISTIC LOOPBACK VALIDATED WIP - NOT INTERNET TRANSPORT",1.8f,Cyan);draw.Text({92,478},std::format("SERVER T{} CLIENTS {} SNAP {} FULL {} DELTA {} ACK {} | LOSS {} DUP {} REORDER {} Q {}/{}B",server.ticks,replicationServer_->ClientCount(),server.snapshotsSent,server.fullSnapshots,server.deltaSnapshots,server.acks,net.lost,net.duplicated,net.reordered,net.queuedPackets,net.queuedBytes),1.45f,Gold);draw.Text({92,510},std::format("C1 SERVER INTEREST {} VIS {} DESPAWN {} RECON {} | C2 SERVER INTEREST {} VIS {} DESPAWN {} RECON {}",ta.interestChunks,ta.visibleEntities,ta.entityDespawns,a.reconciliations,tb.interestChunks,tb.visibleEntities,tb.entityDespawns,b.reconciliations),1.45f,Ink);draw.Text({92,542},std::format("DURABLE B/T/V {}/{} DENIED {} TARGET COMMIT {} ABORT {} RESTART {} TERMINAL {}",networkDurableReceipts_,networkAuthorizedMutations_,networkDeniedMutations_,mutationTarget_->commits,mutationTarget_->aborts,networkRestartVerified_?"PASS":"WAIT",mutationTarget_->commits+mutationTarget_->aborts),1.45f,networkDurableReceipts_==3?Cyan:Gold);}if(networkTransport_&&captureIndex_==4){const auto sa=replicationServer_->Player(1).value_or(ReplicationPlayerState3D{});const auto sb=replicationServer_->Player(2).value_or(ReplicationPlayerState3D{});const auto pa=replicationClientA_->Predicted();const auto pb=replicationClientB_->Predicted();draw.Quad({80,520},{16,180},Cyan);draw.Text({92,574},std::format("OVERVIEW AUTH C1 {}:{:.1f},{:.1f} PRED {}:{:.1f},{:.1f} | AUTH C2 {}:{:.1f},{:.1f} PRED {}:{:.1f},{:.1f}",sa.position.chunk.x,sa.position.offset.x,sa.position.offset.z,pa.position.chunk.x,pa.position.offset.x,pa.position.offset.z,sb.position.chunk.x,sb.position.offset.x,sb.position.offset.z,pb.position.chunk.x,pb.position.offset.x,pb.position.offset.z),1.25f,Cyan);}else if(networkTransport_&&captureIndex_==5){const auto authority=mutationAuthority_->Stats();const auto transport=networkTransport_->Stats();draw.Quad({1200,520},{16,180},Gold);draw.Text({92,574},std::format("INTEREST ENTER/EXIT {}/{} REENTRY {} DESPAWN {} | AUTH OK {} DURABLE {} DENY {} RESTART {} | QUEUE {}/{} PKT {}/{}B | MUT {}/{}",ta.interestEntered+tb.interestEntered,ta.interestExited+tb.interestExited,ta.entityReentries+tb.entityReentries,ta.entityDespawns+tb.entityDespawns,networkAuthorizedMutations_,networkDurableReceipts_,networkDeniedMutations_,networkRestartVerified_?"PASS":"WAIT",transport.queuedPackets,networkTransport_->PacketCapacity(),transport.queuedBytes,networkTransport_->ByteCapacity(),authority.pending,mutationAuthority_->PendingCapacity()),1.25f,Gold);}if(shadowCaptureMode_&&captureIndex_<6&&captureFrameCounter_>=(captureIndex_+1)*30){constexpr std::array<const wchar_t*,6> names={L"contact-shadow-disabled.png",L"contact-shadow-enabled.png",L"far-cascade-receiver.png",L"day-shadow.png",L"dusk-shadow.png",L"night-shadow.png"};std::filesystem::create_directories(captureDirectory_);if(engine.CaptureNextFrame3D(captureDirectory_/names[captureIndex_]))++captureIndex_;}else if(!shadowCaptureMode_&&!captureDirectory_.empty()&&captureIndex_<6&&networkTick_>=(captureIndex_+1)*20){constexpr std::array<const wchar_t*,6> names={L"first-person.png",L"third-person.png",L"god-view.png",L"isometric-2-5d.png",L"network-replication-overview.png",L"network-interest-durability.png"};std::filesystem::create_directories(captureDirectory_);if(engine.CaptureNextFrame3D(captureDirectory_/names[captureIndex_])){++captureIndex_;nextCaptureTime_=time+.5f;}}
}
