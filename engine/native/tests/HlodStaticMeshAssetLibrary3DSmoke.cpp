#include <Aevrynd/StaticMeshAssetLibrary3D.h>
#include <iostream>
#include <memory>

using namespace Aevrynd;

namespace {
int Result{};
void Fail(const char* message){if(!Result){std::cerr<<message<<'\n';Result=1;}}
bool ValidationReceiptMatches(const RendererStats& stats){wchar_t expected[8]{};const DWORD length=GetEnvironmentVariableW(L"AEVRYND_EXPECT_GPU_VALIDATION_MODE",expected,DWORD(std::size(expected)));if(length==6&&lstrcmpW(expected,L"normal")==0)return!stats.gpuValidationRequested&&!stats.gpuValidationAvailable&&!stats.gpuValidationExecuted&&!stats.debugLayerEnabled&&!stats.gpuValidationPassed&&!stats.debugErrorCount&&!stats.debugWarningCount;if(length==3&&lstrcmpW(expected,L"gbv")==0)return stats.gpuValidationRequested&&stats.gpuValidationAvailable&&stats.gpuValidationExecuted&&stats.debugLayerEnabled&&stats.gpuValidationPassed&&!stats.debugErrorCount&&!stats.debugWarningCount;return false;}

MaterialAssetDefinition3D ProxyMaterial(){MaterialAssetDefinition3D value;value.textures={"engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",512,"engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};value.material.useUv=true;return value;}
StaticMeshAssetDefinition3D ProxyDefinition(){StaticMeshAssetDefinition3D value;value.meshPath="engine/assets/model-packs/poly-haven/rock_09/rock_09.aemesh";value.material=ProxyMaterial();value.generateLods=false;return value;}

class HlodLibraryGame final:public IGame {
public:
 void OnStart(Engine& engine)override{
  const std::filesystem::path root{AEVRYND_SOURCE_DIR};if(!library_.Initialize(engine,registry_,root)){Fail("HLOD library initialization failed");engine.Quit();return;}
  const StableObjectId3D world{0x12,0x34};const HlodClusterId3D cluster{0x56,0x78};const GlobalChunkCoordinate3D chunk{-4,5,-6};
  auto prepared=library_.PrepareHlodProxyCpu3D(world,chunk,cluster,7,ProxyDefinition());
  const std::string expected="hlod/0000000000000012-0000000000000034/-4-5--6/0000000000000056-0000000000000078/g7";
  if(!prepared||prepared->logicalName!=expected||prepared->prepared.definition.name!=expected||!prepared->reservedPhysicalBytes||prepared->reservedPhysicalBytes==SIZE_MAX){Fail("Deterministic HLOD logical name or reservation failed");engine.Quit();return;}
  HlodProxyLease3D dirty{};dirty.clusterId=cluster;if(library_.AcquireHlodProxyAtomic3D(*prepared,dirty)){Fail("Non-empty HLOD output was accepted");engine.Quit();return;}dirty.Reset();
  auto wrongWorld=*prepared;wrongWorld.worldId.low++;wrongWorld.logicalName[37]='5';wrongWorld.prepared.definition.name=wrongWorld.logicalName;HlodProxyLease3D wrongWorldLease{};if(library_.AcquireHlodProxyAtomic3D(std::move(wrongWorld),wrongWorldLease)||wrongWorldLease){Fail("Tampered HLOD world identity was accepted");engine.Quit();return;}
  auto wrongContent=*prepared;wrongContent.prepared.cpuMeshes[0].vertices[0].x+=1.f;HlodProxyLease3D wrongContentLease{};if(library_.AcquireHlodProxyAtomic3D(std::move(wrongContent),wrongContentLease)||wrongContentLease){Fail("Tampered HLOD prepared content was accepted");engine.Quit();return;}
  const auto rendererBaseline=engine.GetRendererStats();
  for(int checkpoint=0;checkpoint<=4;++checkpoint){library_.SetHlodProxyFailureCheckpointForTesting(checkpoint);HlodProxyLease3D rejected{};if(library_.AcquireHlodProxyAtomic3D(*prepared,rejected)||rejected||library_.Contains(expected)){Fail("Injected HLOD acquire failure was not atomic");engine.Quit();return;}const auto after=engine.GetRendererStats();if(after.residentMeshBytes!=rendererBaseline.residentMeshBytes||after.residentTextureAllocationBytes!=rendererBaseline.residentTextureAllocationBytes){Fail("Injected HLOD acquire leaked resident resources");engine.Quit();return;}}
  library_.SetHlodProxyFailureCheckpointForTesting(-1);HlodProxyLease3D first{};if(!library_.AcquireHlodProxyAtomic3D(*prepared,first)||!first.renderable.Valid()||first.actualPhysicalBytes>prepared->reservedPhysicalBytes||!first.actualPhysicalBytes){Fail("First HLOD proxy acquire failed");engine.Quit();return;}
  first.worldId.low++;if(library_.ReleaseHlodProxyAlias3D(first)||!library_.Contains(first.logicalName)||!first.renderable.Valid()){Fail("Tampered HLOD lease released an alias");engine.Quit();return;}first.worldId.low--;
  if(library_.Unload(first.logicalName)||library_.Shutdown()){Fail("Live HLOD lease did not reject unload or shutdown");engine.Quit();return;}
  auto shared=library_.PrepareHlodProxyCpu3D(world,chunk,cluster,8,ProxyDefinition());HlodProxyLease3D second{};if(!shared||shared->logicalName==prepared->logicalName||shared->reservedPhysicalBytes){Fail("HLOD generation did not produce a distinct deduplicated alias");engine.Quit();return;}
  auto stale=*shared;stale.clusterGeneration=9;HlodProxyLease3D staleLease{};if(library_.AcquireHlodProxyAtomic3D(std::move(stale),staleLease)||staleLease){Fail("Stale HLOD generation identity was accepted");engine.Quit();return;}
  if(!library_.AcquireHlodProxyAtomic3D(*shared,second)||second.actualPhysicalBytes){Fail("Shared HLOD proxy physical dedup failed");engine.Quit();return;}
  auto conflictDefinition=ProxyDefinition();conflictDefinition.meshPath="games/engine_lab/assets/engine_probe.obj";auto conflict=library_.PrepareHlodProxyCpu3D(world,chunk,cluster,7,std::move(conflictDefinition));HlodProxyLease3D conflictLease{};if(!conflict||library_.AcquireHlodProxyAtomic3D(std::move(*conflict),conflictLease)){Fail("HLOD logical name content conflict was accepted");engine.Quit();return;}
  const auto firstName=first.logicalName,secondName=second.logicalName;first=std::move(second);if(first.logicalName!=secondName||second.logicalName!=firstName||!first||!second){Fail("HLOD move assignment lost an owned alias");engine.Quit();return;}
  if(!library_.ReleaseHlodProxyAlias3D(second)||second||!first.renderable.Valid()||!library_.Contains(first.logicalName)){Fail("First shared HLOD release damaged the second lease");engine.Quit();return;}
  auto heldAsset=first.renderable.Renderable().assetLease;if(library_.ReleaseHlodProxyAlias3D(first)||!first||first.renderable.Valid()||!library_.Contains(secondName)){Fail("Failed HLOD unload did not remain retryable");engine.Quit();return;}heldAsset.reset();if(!library_.ReleaseHlodProxyAlias3D(first)||first||library_.Contains(secondName)){Fail("Final HLOD proxy alias release failed\n");engine.Quit();return;}
  engine.WaitRendererIdle3D();engine.TrimUnusedMemory();const auto released=engine.GetRendererStats();if(released.residentMeshBytes!=rendererBaseline.residentMeshBytes||released.residentMeshResources!=rendererBaseline.residentMeshResources||released.residentTextureAllocationBytes!=rendererBaseline.residentTextureAllocationBytes||released.residentMaterialTextureSets!=rendererBaseline.residentMaterialTextureSets||released.residentMaterialTextureReferences!=rendererBaseline.residentMaterialTextureReferences||released.pendingGpuReleaseBytes||library_.Stats().assets||!library_.Shutdown()){Fail("HLOD proxy resources did not return to baseline");engine.Quit();return;}
  readyForValidation_=true;
 }
 void OnUpdate(Engine&,float)override{}
 void OnRender(Engine& engine,float)override{if(!readyForValidation_)return;if(renderCalls_++){const auto stats=engine.GetRendererStats();if(!ValidationReceiptMatches(stats))Fail("HLOD D3D12 validation receipt failed");else std::cout<<"hlodProxyLease=atomic sharedDelta=0 addressableGenerations=2 rollbackCheckpoints=5 validation=passed\n";engine.Quit();}}
 void OnStop(Engine&)override{if(library_.Initialized())library_.Shutdown();}
private:SceneECSRegistry3D registry_{};StaticMeshAssetLibrary3D library_{};bool readyForValidation_{};unsigned renderCalls_{};
};
}

int main(){try{Engine engine({L"Aevrynd HLOD Static Mesh Asset Library Test",320,180,1.f/60.f,false,false,false});engine.Run(std::make_unique<HlodLibraryGame>(),GetModuleHandleW(nullptr),SW_HIDE);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}return Result;}
