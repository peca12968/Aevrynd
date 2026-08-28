#include <Aevrynd/StaticMeshAssetLibrary3D.h>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

using namespace Aevrynd;
static_assert(std::is_move_constructible_v<MeshLodResourceChain3D>&&!std::is_move_assignable_v<MeshLodResourceChain3D>);
namespace {
int TestResult{};

MaterialAssetDefinition3D GroundMaterial(){MaterialAssetDefinition3D result;result.name="shared-ground";result.textures={"engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",512,"engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};result.material.textureScale=.18f;result.material.useUv=true;return result;}
StaticMeshAssetDefinition3D Rock(std::string name){StaticMeshAssetDefinition3D result;result.name=std::move(name);result.meshPath="engine/assets/model-packs/poly-haven/rock_09/rock_09.aemesh";result.material=GroundMaterial();result.lodSettings.generatedLevelCount=3;result.lodSwitchDistances={12,32,72};result.lodTransitionWidths={2,4,8};return result;}

class StaticAssetTestGame final:public IGame {
public:
    void OnStart(Engine& engine)override{
        engine.SetMeshMemoryBudget(128*1024*1024);engine.SetMaterialTextureMemoryBudget(64*1024*1024);const std::filesystem::path root{AEVRYND_SOURCE_DIR};if(!library_.Initialize(engine,registry_,root)){Fail("Static asset library initialization failed");return;}
        const auto rock=Rock("rock");if(!library_.Load(rock)||!library_.Load(rock)||library_.LevelCount("rock")!=4){Fail("AEMESH generated LOD load or idempotent cache failed");return;}
        auto conflict=rock;conflict.lodSwitchDistances[0]=14;if(library_.Load(conflict)||library_.LevelCount("rock")!=4){Fail("Conflicting asset definition was accepted");return;}
        auto alias=rock;alias.name="rock-alias";if(!library_.Load(alias)){Fail("Shared mesh and material alias failed");return;}
        auto probe=rock;probe.name="obj-probe";probe.meshPath="games/engine_lab/assets/engine_probe.obj";probe.generateLods=false;if(!library_.Load(probe)||library_.LevelCount("obj-probe")!=1){Fail("OBJ single-level asset load failed");return;}const SceneEntity3D probeEntity=library_.Instantiate("obj-probe");if(!probeEntity||!library_.DestroyInstance(probeEntity)||!library_.Unload("obj-probe")){Fail("OBJ asset instance lifecycle failed");return;}
        std::vector<SceneEntity3D> instances;instances.reserve(256);for(int index=0;index<256;++index){const SceneEntity3D entity=library_.Instantiate(index&1?"rock":"rock-alias",{{float(index%16)*2.f,0,float(index/16)*2.f},{},{1,1,1}},index&1?1u:2u);if(!entity){Fail("Static asset instantiation failed");return;}instances.push_back(entity);}
        Scene3D scene;DrawList draw;const auto collected=registry_.Collect(scene,draw);if(collected.submittedRenderables!=256||collected.submittedLodRenderables!=256||scene.MeshCount()!=256){Fail("Static asset ECS collection failed");return;}
        if(library_.Unload("rock")||library_.Stats().unloadRejections!=1){Fail("Live static asset unload was not rejected");return;}
        for(std::size_t index=0;index<128;++index)if(!library_.DestroyInstance(instances[index])){Fail("Tracked static instance destruction failed");return;}
        if(!registry_.Destroy(instances[128])){Fail("External instance destruction fixture failed");return;}const auto afterExternal=library_.Stats();if(afterExternal.liveInstances!=127||afterExternal.externallyDestroyedInstances!=1){Fail("External ECS destruction was not reconciled");return;}
        Transform3D invalid{};invalid.position.x=(std::numeric_limits<float>::quiet_NaN)();if(library_.Instantiate("rock",invalid)||library_.Stats().instanceFailures!=1){Fail("Invalid transform instantiation was accepted");return;}
        if(library_.ClearInstances()!=127||library_.Stats().liveInstances||!library_.Unload("rock")||!library_.Contains("rock-alias")){Fail("Instance clear or first shared asset unload failed");return;}
        const RendererStats sharedRenderer=engine.GetRendererStats();if(sharedRenderer.residentMaterialTextureSets!=1||sharedRenderer.residentMaterialTextureReferences!=1){Fail("Shared material was released before its final asset");return;}
        auto batchGood=rock;batchGood.name="batch-good";batchGood.generateLods=false;auto batchMissing=batchGood;batchMissing.name="batch-missing";batchMissing.meshPath="engine/assets/model-packs/missing.aemesh";const StaticMeshAssetDefinition3D batch[]{batchGood,batchMissing};if(library_.LoadAll(batch)||library_.Contains("batch-good")||library_.Contains("batch-missing")||!library_.Contains("rock-alias")||library_.Stats().batchRollbacks!=1){Fail("Static asset batch rollback was not atomic");return;}
        auto escaped=batchGood;escaped.name="escaped";escaped.meshPath="../outside.aemesh";if(library_.Load(escaped)||library_.Contains("escaped")){Fail("Asset-root traversal was accepted");return;}
        StaticMeshAssetLibrary3D bounded;StaticMeshAssetLibraryLimits3D tinyLimits;tinyLimits.maximumSourceFileBytes=1;if(!bounded.Initialize(engine,registry_,root,tinyLimits)||bounded.Load(batchGood)||bounded.Stats().loadFailures!=1){Fail("Static asset source-size limit failed");return;}bounded.Shutdown();
        if(!library_.Unload("rock-alias")){Fail("Final shared asset unload failed");return;}const RendererStats released=engine.GetRendererStats();if(released.residentMeshes||released.residentMeshReferences||released.residentMaterialTextureSets||released.residentMaterialTextureReferences){Fail("Static asset resources were not fully released");return;}
        const auto stats=library_.Stats();if(stats.assets||stats.cacheHits!=1||stats.nameConflicts!=1||stats.instancesCreated!=257||stats.instancesDestroyed!=256||stats.unloaded!=4){Fail("Static asset telemetry failed");return;}ready_=true;engine.Quit();
    }
    void OnUpdate(Engine& engine,float)override{if(TestResult)engine.Quit();}
    void OnRender(Engine&,float)override{}
    void OnStop(Engine&)override{library_.Shutdown();if(ready_)std::cout<<"formats=OBJ+AEMESH lodLevels=8 instances=257 sharedMaterial=1 rollback=1 released=all\n";}
private:
    static void Fail(const char* message){if(!TestResult){std::cerr<<message<<'\n';TestResult=1;}}
    SceneECSRegistry3D registry_{};StaticMeshAssetLibrary3D library_{};bool ready_{};
};
}

int main(){try{Engine engine({L"Aevrynd Static Mesh Asset Library Test",320,180,1.f/60.f,false,false,false});engine.Run(std::make_unique<StaticAssetTestGame>(),GetModuleHandleW(nullptr),SW_HIDE);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}return TestResult;}
