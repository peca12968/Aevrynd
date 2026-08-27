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
        engine.SetMaterialTextureMemoryBudget(64*1024*1024);const std::filesystem::path root=std::filesystem::path{AEVRYND_SOURCE_DIR}/"engine"/"assets"/"material-packs";if(!library_.Initialize(engine,root)){Fail("Material library initialization failed");return;}MaterialAssetDefinition3D ground{"ground",{"ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",1024,"ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.96f,0,.12f,.92f},false,{},.08f}};if(!library_.Load(ground)||!library_.Load(ground)){Fail("Named material load or cache reuse failed");return;}MaterialAssetDefinition3D absoluteGround=ground;absoluteGround.textures.baseColorPath=root/ground.textures.baseColorPath;absoluteGround.textures.roughnessPath=root/ground.textures.roughnessPath;absoluteGround.textures.normalPath=root/ground.textures.normalPath;if(!library_.Load(absoluteGround)){Fail("Relative and absolute single-layer definitions did not normalize equally");return;}MaterialAssetDefinition3D alias=ground;alias.name="ground_alias";if(!library_.Load(alias)){Fail("Renderer texture sharing through aliases failed");return;}
        MaterialAssetDefinition3D terrain=ground;terrain.name="terrain";terrain.material.materialClass=MaterialClass3D::Terrain;terrain.textures.additionalLayers[0]={"poly-haven/aerial_ground_rock/1k/base-color.jpg","poly-haven/aerial_ground_rock/1k/roughness.jpg","poly-haven/aerial_ground_rock/1k/normal-dx.jpg"};terrain.textures.additionalLayers[1]={"poly-haven/rock_surface/1k/base-color.jpg","poly-haven/rock_surface/1k/roughness.jpg","poly-haven/rock_surface/1k/normal-dx.jpg"};if(!library_.Load(terrain)||!library_.Load(terrain)){Fail("Three-layer relative material load or cache reuse failed");return;}MaterialAssetDefinition3D terrainAlias=terrain;terrainAlias.name="terrain_alias";if(!library_.Load(terrainAlias)||library_.Textures("terrain_alias")!=library_.Textures("terrain")){Fail("Three-layer renderer cache sharing failed");return;}MaterialAssetDefinition3D conflict=terrain;conflict.textures.additionalLayers[1].normalPath="poly-haven/brick_floor/1k/normal-dx.jpg";if(library_.Load(conflict)){Fail("Conflicting additional layer was accepted");return;}
        MaterialAssetDefinition3D wood{"wood",{"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",1024,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.82f,0,.09f,.88f},true,{},.5f}},missing=terrain;missing.name="missing";missing.textures.additionalLayers[1].normalPath="missing-third-layer-normal.jpg";const MaterialAssetDefinition3D batch[]{wood,missing};if(library_.LoadAll(batch)||library_.Find("wood")||library_.Find("missing")){Fail("Missing additional layer batch was not rolled back atomically");return;}const MaterialLibraryStats3D stats=library_.Stats();const RendererStats renderer=engine.GetRendererStats();if(!library_.Find("ground")||!library_.Textures("ground")||!library_.Find("terrain")||stats.materials!=4||stats.uniqueTextureSets!=2||stats.cacheHits!=3||stats.nameConflicts!=1||stats.batchRollbacks!=1||stats.unloaded!=1||renderer.residentMaterialTextureSets!=2||renderer.residentMaterialTextureReferences!=4){Fail("Material library telemetry or layered references are invalid");return;}ready_=true;}
    void OnUpdate(Engine& engine,float)override{engine.Quit();}
    void OnRender(Engine&,float)override{}
    void OnStop(Engine& engine)override{if(!ready_)library_.Shutdown();else{library_.Shutdown();const RendererStats renderer=engine.GetRendererStats();if(renderer.residentMaterialTextureSets||renderer.residentMaterialTextureReferences)Fail("Material library shutdown did not release textures");else std::cout<<"materials=4 uniqueTextures=2 cacheHits=3 conflicts=1 rollbacks=1 released=4 layers=3\n";}}
private:
    static void Fail(const char* message){if(!TestResult){std::cerr<<message<<'\n';TestResult=1;}}
    MaterialLibrary3D library_{};bool ready_{};
};
}

int main(){try{Engine engine({L"Aevrynd Material Library Test",320,180,1.f/60.f,false,false,false});engine.Run(std::make_unique<MaterialLibraryTestGame>(),GetModuleHandleW(nullptr),SW_HIDE);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}return TestResult;}
