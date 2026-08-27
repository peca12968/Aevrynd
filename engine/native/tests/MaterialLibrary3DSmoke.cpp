#include <Aevrynd/MaterialLibrary3D.h>
#include <exception>
#include <iostream>
#include <memory>

using namespace Aevrynd;
namespace {
int TestResult{};
class MaterialLibraryTestGame final:public IGame {
public:
    void OnStart(Engine& engine)override{
        engine.SetMaterialTextureMemoryBudget(32*1024*1024);const std::filesystem::path root=std::filesystem::path{AEVRYND_SOURCE_DIR}/"engine"/"assets"/"material-packs";if(!library_.Initialize(engine,root)){Fail("Material library initialization failed");return;}MaterialAssetDefinition3D ground{"ground",{"ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",1024,"ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.96f,0,.12f,.92f},false,{},.08f}};if(!library_.Load(ground)||!library_.Load(ground)){Fail("Named material load or cache reuse failed");return;}MaterialAssetDefinition3D alias=ground;alias.name="ground_alias";if(!library_.Load(alias)){Fail("Renderer texture sharing through aliases failed");return;}MaterialAssetDefinition3D conflict=ground;conflict.textures.maximumResolution=512;if(library_.Load(conflict)){Fail("Conflicting material name was accepted");return;}MaterialAssetDefinition3D wood{"wood",{"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",1024,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.82f,0,.09f,.88f},true,{},.5f}},missing{"missing",{"missing-color.jpg","missing-roughness.jpg",1024,"missing-normal.jpg"},{}};const MaterialAssetDefinition3D batch[]{wood,missing};if(library_.LoadAll(batch)||library_.Find("wood")||library_.Find("missing")){Fail("Failed material batch was not rolled back");return;}const MaterialLibraryStats3D stats=library_.Stats();const RendererStats renderer=engine.GetRendererStats();if(!library_.Find("ground")||!library_.Textures("ground")||stats.materials!=2||stats.uniqueTextureSets!=1||stats.cacheHits!=1||stats.nameConflicts!=1||stats.batchRollbacks!=1||stats.unloaded!=1||renderer.residentMaterialTextureSets!=1||renderer.residentMaterialTextureReferences!=2){Fail("Material library telemetry or shared references are invalid");return;}ready_=true;}
    void OnUpdate(Engine& engine,float)override{engine.Quit();}
    void OnRender(Engine&,float)override{}
    void OnStop(Engine& engine)override{if(!ready_)library_.Shutdown();else{library_.Shutdown();const RendererStats renderer=engine.GetRendererStats();if(renderer.residentMaterialTextureSets||renderer.residentMaterialTextureReferences)Fail("Material library shutdown did not release textures");else std::cout<<"materials=2 uniqueTextures=1 cacheHits=1 conflicts=1 rollbacks=1 released=2\n";}}
private:
    static void Fail(const char* message){if(!TestResult){std::cerr<<message<<'\n';TestResult=1;}}
    MaterialLibrary3D library_{};bool ready_{};
};
}

int main(){try{Engine engine({L"Aevrynd Material Library Test",320,180,1.f/60.f,false,false,false});engine.Run(std::make_unique<MaterialLibraryTestGame>(),GetModuleHandleW(nullptr),SW_HIDE);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}return TestResult;}
