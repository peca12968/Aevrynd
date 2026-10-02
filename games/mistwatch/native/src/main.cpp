#include "MistwatchSession.h"
#include "MistwatchWorld.h"
#include "MistwatchEncounter.h"
#include "MistwatchCamera.h"
#include "MistwatchExplorer.h"
#include "MistwatchForestMaterial.h"
#include "MistwatchGroundMaterial.h"
#include "MistwatchFern.h"
#include "MistwatchScannedGrass.h"
#include "MistwatchEvasion.h"
#include "MistwatchRiver.h"
#include "MistwatchBoar.h"
#include "MistwatchLandscape.h"
#include "MistwatchAudio.h"
#include "MistwatchPlayback.h"
#include "MistwatchSupplyCrate.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <fstream>
#include <iterator>
#include <memory>
#include <chrono>
#include <numeric>
#include <iomanip>

using namespace Aevrynd;
class MistwatchGame final : public IGame {
    Mistwatch::Session session;
    CharacterController3D controller;
    Scene3D scene;
    MeshResource3D box;
    std::array<MeshResource3D,2> supplyMeshes;
    unsigned submittedSupplies{};
    std::array<MeshResource3D,4> firMeshes;
    std::array<MeshResource3D,4> landscapeFirMeshes;
    std::array<Material3D,4> firMaterials;
    MaterialLibrary3D forestMaterials;
    MaterialLibrary3D broadleafMaterials;
    std::array<MeshResource3D,3> broadleafMeshes;
    std::array<Material3D,3> broadleafSurface;
    std::vector<Mistwatch::BroadleafTree> broadleafTrees;
    std::size_t broadleafTriangles{};
    HeightField3D terrain;
    MeshResource3D terrainMesh;
    MeshResource3D landscapeMesh;
    std::vector<Transform3D> landscapeTrees;
    MeshResource3D riverMesh;
    float waterTime{};
    std::vector<Vec3> trees;
    std::vector<Vec3> groundCover;
    MeshResource3D grassMesh;
    std::array<MeshResource3D,5> scannedGrass;
    MaterialLibrary3D grassMaterials;
    Material3D grassSurface;
    std::size_t submittedGrassTriangles{};
    MeshResource3D fernMesh;
    std::array<MeshResource3D,4> scannedFerns;
    MaterialLibrary3D fernMaterials;
    Material3D fernSurface;
    std::size_t submittedFernTriangles{};
    MeshResource3D relayDish;
    Mistwatch::ForestUnderstory understory;
    unsigned submittedFerns{},submittedSaplings{};
    // 所有岩石共用一份 GPU 網格，倒木與修復木梁共用另一份。
    std::array<MeshResource3D,2> propMeshes;
    MaterialLibrary3D boulderMaterials;
    Material3D boulderMaterial;
    std::vector<Mistwatch::ForestProp> forestProps;
    MaterialLibrary3D materials;
    MaterialAssetDefinition3D groundDefinition;
    Material3D groundMaterial, barkMaterial, woodMaterial, rockMaterial;
    std::vector<BoxCollider3D> solids;
    Mistwatch::OrbitCamera orbit;
    Mistwatch::Encounter encounter;
    Mistwatch::Evasion evasion;
    Mistwatch::Explorer explorer;
    SkinnedMeshResource3D explorerMesh;
    SkinningWorkspace3D explorerSkinning;
    Mistwatch::Boar boar;
    SkinnedMeshResource3D boarMesh;
    SkinningWorkspace3D boarSkinning;
    bool started{}, paused{};
    Mistwatch::GameAudio gameAudio;
    std::filesystem::path audioSettings;
    std::string message="ENTER: begin expedition";
    std::filesystem::path save;
    std::filesystem::path capture;
    unsigned captureFrames{};
    unsigned captureSequenceFrames{},captureCompleted{},captureConsumed{};
    bool capturePending{};
    bool diagnosticNoAo{};
    bool diagnosticNoGroundNormal{},diagnosticNoMsaa{};
    unsigned diagnosticHdrStage{};
    bool diagnosticNoShadows{},diagnosticNoBloom{};
    bool diagnosticNoGroundTextures{};
    bool diagnosticNeutralGroundBase{},diagnosticNeutralGroundSurface{};
    bool diagnosticLegacyGround{};
    bool diagnosticLegacyGrass{};
    bool diagnosticDimGrass{};
    bool diagnosticProceduralGrass{};
    bool diagnosticLegacyFern{};
    bool diagnosticCrown{};
    bool captureWalk{},captureCrouch{};
    bool captureWildlife{};
    bool captureBoarOverview{};
    bool captureBoulderOverview{};
    bool captureGrassOverview{};
    bool captureSupplyOverview{};
    bool captureGrassDistance{},grassFixtureEnabled{true};
    std::size_t grassFixtureIndex{};
    float grassFixturePlayerDistance{2.f},grassFixtureCameraDistance{12.f};
    struct GrassFixtureState {
        Vec3 root{},feet{};
        float scale{},yaw{},playerDistance{},cameraDistance{};
        bool selected{},sceneAccepted{};
        unsigned opaque{},shadowOnly{},triangles{};
        std::uint64_t sceneSerial{},cameraHash{};
    } grassFixtureState,grassFixtureFrozen;
    bool captureFernOverview{};
    unsigned submittedGrass{};
    bool captureRelayMotion{};
    bool captureLandscapeMotion{};
    bool captureAudioProbe{};
    unsigned captureAudioTicks{};
    bool captureFocusProbe{},captureFocusComplete{};
    unsigned captureFocusTicks{},captureFocusFrozenTicks{};
    std::string captureFocusSession;
    Vec3 captureFocusFeet{};
    float captureFocusWater{},captureFocusEnemyTime{};
    bool captureRiverOverview{};
    bool captureShelterOverview{};
    Camera3D lastRenderedCamera{};
    bool captureBenchmark{};
    std::uint64_t benchmarkLastSerial{};
    std::vector<double> benchmarkGpuMilliseconds;
    std::chrono::steady_clock::time_point captureStart;
    void ResetController() {
        controller.Reset(session.feet);
        // 讀檔／重試時先採低姿，再嘗試站起，避免載入棚內存檔後頭部穿過屋頂。
        controller.SetHeight(1.4f);
        controller.TrySetHeight(1.8f,solids);
        gameAudio.director.Reset();
    }
    void TickAudio(Engine& e,float dt) {
        Mistwatch::AudioState state;
        state.session=session;state.enemyPhase=encounter.phase;state.enemyHealth=encounter.health;state.enemyFeet=encounter.feet;
        state.active=started&&!paused&&session.Alive();state.grounded=controller.Grounded();state.crouched=controller.Settings().height<1.5f;
        const auto listener=lastRenderedCamera.position;
        const auto right=Mistwatch::AudioListenerRight(lastRenderedCamera);
        gameAudio.Tick(e.Audio(),state,listener,right,dt);
    }
    void SaveAudioOptions() {
        if(audioSettings.empty())return;
        std::error_code error;std::filesystem::create_directories(audioSettings.parent_path(),error);
        auto temporary=audioSettings;temporary+=".tmp";
        std::ofstream out(temporary);out<<gameAudio.options.Serialize();out.close();
        if(!out||!MoveFileExW(temporary.c_str(),audioSettings.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))message="Audio settings save failed";
    }
    void StopGameAudio(Engine& e) {
        gameAudio.Shutdown(e.Audio());
        const auto stats=e.Audio().GetStats();
        if(stats.residentSounds||stats.residentBytes||stats.activeVoices)throw std::runtime_error("Game audio resources not released");
        if(!capture.empty()) {
            auto receipt=capture;receipt+=".audio-cleanup.txt";
            std::ofstream out(receipt);out<<"sounds=0\nbytes=0\nvoices=0\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Audio cleanup receipt failed");
        }
    }
    void ResetBoarPose() {
        boar.initialized=false;
        if(!boar.Update({},0,encounter.phase,(std::max)(0.f,encounter.phaseTime),{0,0,-1})||
            !boar.PlaceFeet(encounter.feet,terrain))throw std::runtime_error("Boar reset pose failed");
    }
    std::filesystem::path CapturePath()const {
        if(!captureSequenceFrames)return capture;
        const auto ordinal=std::to_wstring(captureCompleted);
        return capture.parent_path()/(capture.stem().wstring()+L"-"+std::wstring(3-ordinal.size(),L'0')+ordinal+L".png");
    }
public:
    void OnStart(Engine& e) override {
        const auto wildlife=LoadSkinnedModelAssetFile(e.ExecutableDirectory()/"assets"/"mistwatch-wildlife"/"boar.aeskin");
        if(!wildlife||wildlife.model.skeleton.joints.size()!=16||
            wildlife.model.bindMesh.indices!=boar.model.bindMesh.indices)throw std::runtime_error("Boar asset unavailable or incompatible");
        boar.model=wildlife.model;
        boarMesh=e.CreateSkinnedMeshResource3D(boar.model);
        if(!boarMesh)throw std::runtime_error("Boar GPU upload failed");
        if(!explorer.model.Valid())throw std::runtime_error("Explorer model invalid");
        explorerMesh=e.CreateSkinnedMeshResource3D(explorer.model);
        if(!explorerMesh)throw std::runtime_error("Explorer GPU upload failed");
        box=e.CreateMeshResource3D(Mesh3D::CreateUnitBox());
        scene.SetBoxMesh(box);
        const auto supplyGeometry=Mistwatch::CreateSupplyCrate();
        supplyMeshes[0]=e.CreateMeshResource3D(supplyGeometry.wood);
        supplyMeshes[1]=e.CreateMeshResource3D(supplyGeometry.metal);
        if(!supplyMeshes[0]||!supplyMeshes[1])throw std::runtime_error("Supply crate GPU upload failed");
        materials.Initialize(e,e.ExecutableDirectory()/"assets"/"mistwatch-materials");
        wchar_t groundCapture[2]{},legacyGround[3]{};
        if(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",groundCapture,2)>0) {
            const auto length=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GROUND_LEGACY",legacyGround,3);
            if(length&&(length!=1||(legacyGround[0]!=L'0'&&legacyGround[0]!=L'1')))
                throw std::runtime_error("Invalid ground comparison flag");
            diagnosticLegacyGround=legacyGround[0]==L'1';
            wchar_t legacyGrass[3]{};
            const auto grassLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_LEGACY",legacyGrass,3);
            if(grassLength&&(grassLength!=1||(legacyGrass[0]!=L'0'&&legacyGrass[0]!=L'1')))
                throw std::runtime_error("Invalid grass comparison flag");
            diagnosticLegacyGrass=legacyGrass[0]==L'1';
            wchar_t dimGrass[3]{};
            const auto dimLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_DIM",dimGrass,3);
            if(dimLength&&(dimLength!=1||(dimGrass[0]!=L'0'&&dimGrass[0]!=L'1')))
                throw std::runtime_error("Invalid grass brightness comparison flag");
            diagnosticDimGrass=dimGrass[0]==L'1';
            wchar_t proceduralGrass[3]{};
            const auto proceduralLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_PROCEDURAL",proceduralGrass,3);
            if(proceduralLength&&(proceduralLength!=1||(proceduralGrass[0]!=L'0'&&proceduralGrass[0]!=L'1')))
                throw std::runtime_error("Invalid procedural grass comparison flag");
            diagnosticProceduralGrass=proceduralGrass[0]==L'1'||diagnosticLegacyGrass||diagnosticDimGrass;
            wchar_t legacyFern[3]{};
            const auto fernLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_FERN_LEGACY",legacyFern,3);
            if(fernLength&&(fernLength!=1||(legacyFern[0]!=L'0'&&legacyFern[0]!=L'1')))
                throw std::runtime_error("Invalid fern comparison flag");
            diagnosticLegacyFern=legacyFern[0]==L'1';
        }
        groundDefinition=Mistwatch::GroundMaterialDefinition(diagnosticLegacyGround);
        const std::array<MaterialAssetDefinition3D,4> definitions{{
            groundDefinition,
            {"bark",{"poly-haven/bark_brown_02/1k/base-color.jpg","poly-haven/bark_brown_02/1k/roughness.jpg",1024,"poly-haven/bark_brown_02/1k/normal-dx.jpg"},{{1,1,1,1},{.85f,0,0,.8f},true,{},1.f}},
            {"wood",{"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",1024,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.8f,0,0,.8f},true,{},.5f}},
            {"rock",{"poly-haven/rock_surface/1k/base-color.jpg","poly-haven/rock_surface/1k/roughness.jpg",1024,"poly-haven/rock_surface/1k/normal-dx.jpg"},{{1,1,1,1},{.9f,0,0,.85f},true,{},1.f}}
        }};
        if(!materials.LoadAll(definitions)) throw std::runtime_error("Mistwatch CC0 materials unavailable");
        groundMaterial=*materials.Find("ground");
        barkMaterial=*materials.Find("bark");
        woodMaterial=*materials.Find("wood");
        rockMaterial=*materials.Find("rock");
        const auto forestRoot=e.ExecutableDirectory()/"assets"/"mistwatch-forest";
        forestMaterials.Initialize(e,forestRoot);
        constexpr const char* slots[]{"bark","twig","dead-branches","trunk-c"};
        wchar_t lodOverride[8]{};
        GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_TREE_LOD",lodOverride,8);
        const std::string treeLod=lodOverride[0]==L'0'?"0":"1";
        wchar_t twigMode[16]{},captureFlag[2]{};
        const bool treeDiagnostic=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",captureFlag,2)>0;
        if(treeDiagnostic)GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_TWIG_MODE",twigMode,16);
        constexpr const char* hashes[3][3]{
            {"49fd9058e6a5059e6bced88360e44d0bcf303a164a3c0f4c08a151ddb236015a","a1d895baa855d31dd705e10be6b3c8ed56e2e663665b8a7001c0cf39e128a3e8","21291b807a30761309f810086d6732b752546c951e2a2a091101dd4f7cdb29f3"},
            {"b1a458c5584b5fa40ce04144f341e013b1a999881bf17fff2880032363d58d5e","37af8ba7a3a7f94edb8a58f3bb716c978ca9631bd04027625ea5818be0223e10","3f656dfa0f462f4812c17ff0487261c770a637d8d7c97536c9d98a36f429d3bb"},
            {"c5f308b9a1ebc6fbf4a1eb44750309e1cc191fd829bfdc50098d69b817f8a168","6b894f1c1da10361f2f07c1211cb1902342fbdee10afef66c03150c995836dee","22726faaab8c104237ffe7c8f17ccfa50637561222b4148b61eda0b0fcc03344"}};
        for(unsigned slot=0;slot<4;++slot) {
            const std::string name=slots[slot];
            MaterialTextureSetDefinition3D textures{
                std::filesystem::path("materials")/(name+"-base.dds"),
                std::filesystem::path("materials")/(name+"-arm.dds"),1024,
                std::filesystem::path("materials")/(name+"-normal.dds")};
            textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
            for(unsigned channel=0;channel<3;++channel)
                textures.integrity[channel]={1398276,hashes[slot==3?2:slot==1?1:0][channel]};
            Material3D material{{1,1,1,1},{.85f,0,0,.8f},true,{},1.f,true};
            if(slot==1) {
                textures.alphaMode=MaterialAlphaMode3D::Mask;
                material=Mistwatch::FirTwigMaterial();
                if(std::wstring(twigMode)==L"opaque")material.alphaMode=MaterialAlphaMode3D::Opaque;
                else if(std::wstring(twigMode)==L"low")material.alphaCutoff=.15f;
                else if(std::wstring(twigMode)==L"original")material.alphaCutoff=.5f;
            }
            if(!forestMaterials.Load({name,textures,material})) throw std::runtime_error("Fir material unavailable");
            firMaterials[slot]=*forestMaterials.Find(name);
            const std::string meshName="fir-tree-01-c-lod"+treeLod+"-node0-mesh0-primitive"+std::to_string(slot)+"-material"+std::to_string(slot)+"-fir-tree-01-"+name+".aemesh";
            const auto loaded=LoadAevryndMeshFile(forestRoot/"meshes"/meshName);
            if(!loaded) throw std::runtime_error("Fir mesh unavailable");
            firMeshes[slot]=e.CreateMeshResource3D(loaded.mesh);
            if(!firMeshes[slot]) throw std::runtime_error("Fir GPU upload failed");
            const std::string distantName="fir-tree-01-c-lod2-node0-mesh0-primitive"+std::to_string(slot)+"-material"+std::to_string(slot)+"-fir-tree-01-"+name+".aemesh";
            const auto distant=LoadAevryndMeshFile(forestRoot/"meshes"/distantName);
            if(!distant)throw std::runtime_error("Landscape fir mesh unavailable");
            landscapeFirMeshes[slot]=e.CreateMeshResource3D(distant.mesh);
            if(!landscapeFirMeshes[slot])throw std::runtime_error("Landscape fir GPU upload failed");
        }
        const auto broadleafRoot=e.ExecutableDirectory()/"assets"/"mistwatch-broadleaf";
        broadleafMaterials.Initialize(e,broadleafRoot);
        constexpr const char* broadleafSlots[]{"branches","leaves","trunk"};
        constexpr const char* broadleafHashes[3][3]{
            {"cf4f6fcc9329e66f05b7b2e86d20a38b2d0e8e054013cf3e0431a4cbc1e17d0a","1c54a88ec7a17c55528a99319378283d61b4800002ab4e63ca0fe30ebb1249ea","ea8a046e64ce7eacce435e64f31fc45501624fd131c7e6c8492bfd9de9fb1beb"},
            {"88fce8dc2f1815c421a0c7c4d9cc7d9cacb4024f6c51e8fc928a5e938b7996a9","c8a6a8747023a64ed506ad62b8e5c0ae16bf14e0353d5e67f8d2511967aaf2ed","320d6df57bf075dbd701acab52740a623f0c86d298bd3048f3986d1988bb0cd9"},
            {"513d07ece694d03e3c095c4148d7da4649ffdf6db54826cf1bf1754a368b1453","716c128da96feb1f3ad4ea041ecd9d5c167115a7aa0671f517ebaefbc9963b7d","099e981864b1e90b2c0500b0bcb0a92fcdc38ee196f00f7161905dc9bf762712"}};
        for(unsigned slot=0;slot<3;++slot) {
            const std::string name=broadleafSlots[slot];
            MaterialTextureSetDefinition3D textures{std::filesystem::path("materials")/(name+"-base.dds"),
                std::filesystem::path("materials")/(name+"-arm.dds"),1024,
                std::filesystem::path("materials")/(name+"-normal.dds")};
            textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
            for(unsigned channel=0;channel<3;++channel)textures.integrity[channel]={1398276,broadleafHashes[slot][channel]};
            Material3D material{{1,1,1,1},{.85f,0,0,1},true,{},1.f,true};
            if(slot==1){textures.alphaMode=MaterialAlphaMode3D::Mask;material.alphaMode=MaterialAlphaMode3D::Mask;
                material.alphaCutoff=.5f;material.doubleSided=true;material.materialClass=MaterialClass3D::Foliage;}
            if(!broadleafMaterials.Load({name,textures,material}))throw std::runtime_error("Broadleaf material unavailable");
            broadleafSurface[slot]=*broadleafMaterials.Find(name);
            auto loaded=LoadAevryndMeshFile(broadleafRoot/"meshes"/Mistwatch::BroadleafMeshName(slot));
            if(!loaded)throw std::runtime_error("Broadleaf mesh unavailable");
            Mistwatch::RootBroadleafMesh(loaded.mesh);
            broadleafTriangles+=loaded.mesh.indices.size()/3;
            broadleafMeshes[slot]=e.CreateMeshResource3D(loaded.mesh);
            if(!broadleafMeshes[slot])throw std::runtime_error("Broadleaf upload failed");
        }
        const auto boulderRoot=e.ExecutableDirectory()/"assets"/"mistwatch-boulders";
        boulderMaterials.Initialize(e,boulderRoot);
        MaterialTextureSetDefinition3D boulderTextures{"materials/slot-0-base.dds","materials/slot-0-arm.dds",1024,
            "materials/slot-0-normal.dds"};
        boulderTextures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
        constexpr const char* boulderHashes[]{
            "5a847a3174e09505b3a404c93716cbae576a5cb8bd32f69ec6395e8c95008d36",
            "564be8685c266fe7fa03acda76c6c06f706911c4344777ceab6563d743284a1e",
            "68fbbe04ffdb1c6d327f4262b9230438623f0d3b4b17a0a5155eb13b9ed21e83"};
        for(unsigned channel=0;channel<3;++channel)boulderTextures.integrity[channel]={1398276,boulderHashes[channel]};
        if(!boulderMaterials.Load({"boulder",boulderTextures,{{1,1,1,1},{.9f,0,0,1},true,{},1.f,true}}))
            throw std::runtime_error("Boulder material unavailable");
        boulderMaterial=*boulderMaterials.Find("boulder");
        auto boulder=LoadAevryndMeshFile(boulderRoot/"meshes"/Mistwatch::BoulderMeshName);
        if(!boulder||boulder.mesh.vertices.size()!=56938||boulder.mesh.indices.size()!=79833)
            throw std::runtime_error("Boulder geometry unavailable");
        Mistwatch::RootBoulderMesh(boulder.mesh);
        const auto boulderBounds=boulder.mesh.Bounds(),boulderProfile=Mistwatch::BoulderBounds();
        if(Length(boulderBounds.center-boulderProfile.center)>.00001f||Length(boulderBounds.extents-boulderProfile.extents)>.00001f)
            throw std::runtime_error("Boulder collision profile mismatch");
        propMeshes[0]=e.CreateMeshResource3D(boulder.mesh);
        propMeshes[1]=e.CreateMeshResource3D(Mistwatch::ForestPropLogMesh());
        if(!propMeshes[0]||!propMeshes[1])throw std::runtime_error("Forest prop GPU upload failed");
        terrain=Mistwatch::CreateTerrain();
        terrainMesh=e.CreateMeshResource3D(Mistwatch::CreateTerrainMesh(terrain));
        landscapeMesh=e.CreateMeshResource3D(Mistwatch::CreateLandscapeMesh(terrain));
        if(!landscapeMesh)throw std::runtime_error("Landscape GPU upload failed");
        landscapeTrees=Mistwatch::LandscapeTrees(terrain);
        riverMesh=e.CreateMeshResource3D(Mistwatch::CreateRiverMesh(terrain));
        if(!riverMesh)throw std::runtime_error("River GPU upload failed");
        trees=Mistwatch::TreePositions();
        forestProps=Mistwatch::ForestProps(terrain,trees);
        understory=Mistwatch::CreateUnderstory(terrain,trees,forestProps);
        broadleafTrees=Mistwatch::CreateBroadleafTrees(terrain,trees,forestProps,understory);
        if(broadleafTrees.size()!=12||broadleafTriangles!=82854)throw std::runtime_error("Broadleaf population/geometry invalid");
        fernMesh=e.CreateMeshResource3D(CreateFernMesh3D(EnvironmentMeshDetail3D::Near,17));
        if(!fernMesh)throw std::runtime_error("Fern GPU upload failed");
        const auto fernRoot=e.ExecutableDirectory()/"assets"/"mistwatch-ferns";
        fernMaterials.Initialize(e,fernRoot);
        if(!fernMaterials.Load(Mistwatch::FernMaterialDefinition()))throw std::runtime_error("Fern material unavailable");
        fernSurface=*fernMaterials.Find("fern");
        for(unsigned variant=0;variant<4;++variant) {
            auto loaded=LoadAevryndMeshFile(fernRoot/"meshes"/Mistwatch::FernMeshName(variant));
            if(!loaded||loaded.mesh.vertices.size()!=Mistwatch::FernVertexCounts[variant]||
                loaded.mesh.indices.size()!=3u*Mistwatch::FernTriangleCounts[variant])throw std::runtime_error("Fern variant unavailable");
            Mistwatch::NormalizeFernAsset(loaded.mesh);
            scannedFerns[variant]=e.CreateMeshResource3D(loaded.mesh);
            if(!scannedFerns[variant])throw std::runtime_error("Scanned fern upload failed");
        }
        groundCover=Mistwatch::GroundCoverPositions(terrain);
        grassMesh=e.CreateMeshResource3D(CreateGrassClumpMesh3D(9,diagnosticLegacyGrass?3:5,diagnosticLegacyGrass?GrassBladeStyle3D::Upright:GrassBladeStyle3D::Curved));
        const auto grassRoot=e.ExecutableDirectory()/"assets"/"mistwatch-grass";
        grassMaterials.Initialize(e,grassRoot);
        if(!grassMaterials.Load(Mistwatch::GrassMaterialDefinition()))throw std::runtime_error("Grass material unavailable");
        grassSurface=*grassMaterials.Find("grass");
        for(unsigned variant=0;variant<scannedGrass.size();++variant){
            auto loaded=LoadAevryndMeshFile(grassRoot/"meshes"/Mistwatch::GrassMeshName(variant));
            if(!loaded||loaded.mesh.vertices.size()!=Mistwatch::GrassVertexCounts[variant]||
                loaded.mesh.indices.size()!=3u*Mistwatch::GrassTriangleCounts[variant])throw std::runtime_error("Scanned grass topology changed");
            Mistwatch::NormalizeGrassAsset(loaded.mesh);
            scannedGrass[variant]=e.CreateMeshResource3D(loaded.mesh);
            if(!scannedGrass[variant])throw std::runtime_error("Scanned grass upload failed");
        }
        relayDish=e.CreateMeshResource3D(Mistwatch::CreateRelayDish());
        if(!relayDish)throw std::runtime_error("Relay dish GPU upload failed");
        solids=Mistwatch::WorldSolids(session.bridgeRepaired);
        controller.Reset({});
        e.SetClearColor({.16f,.23f,.25f,1});
        wchar_t local[32768]{};
        const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
        if(count && count<32768) save=std::filesystem::path(local)/"Aevrynd"/"Mistwatch"/"save.txt";
        wchar_t output[32768]{};
        const auto outputCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",output,32768);
        if(outputCount && outputCount<32768) {
            capture=output;
            captureStart=std::chrono::steady_clock::now();
            started=true;
            waterTime=2.5f;
            wchar_t sequence[4]{};
            if(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE_SERIES",sequence,4)>0 && sequence[0]==L'1')captureSequenceFrames=36;
            wchar_t benchmark[4]{};
            captureBenchmark=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_BENCHMARK",benchmark,4)>0 && benchmark[0]==L'1';
            if(captureBenchmark && captureSequenceFrames)throw std::runtime_error("Benchmark and capture series must run separately");
            if(captureBenchmark)benchmarkGpuMilliseconds.reserve(120);
            message="Capture mode - initial playable scene";
            // 階段截圖只使用記憶體測試資料，不讀寫玩家存檔。
            wchar_t scenario[32]{};
            const auto scenarioCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_SCENE",scenario,32);
            if(scenarioCount) {
                if(scenarioCount>=32) throw std::runtime_error("Capture scenario too long");
                const std::wstring name=scenario;
                std::string fixture;
                if(name==L"panel") fixture="MISTWATCH 3 7 80 1 1 0 5.4 0 65 1 0 3";
                else if(name==L"powered") fixture="MISTWATCH 3 7 80 1 1 0 5.4 0 65 1 1 7";
                else if(name==L"complete") fixture="MISTWATCH 3 7 80 1 1 1 0 0 69 1 1 7";
                else if(name==L"relay-west"||name==L"relay-aligned"||name==L"relay-linked"||name==L"relay-turn"||name==L"ridge-pan") {
                    fixture="MISTWATCH 5 7 80 1 1 0 0 0 65 1 1 7 7 1 50";
                    captureRelayMotion=name==L"relay-turn";
                    captureLandscapeMotion=name==L"ridge-pan";
                }
                else if(name==L"strike"||name==L"boar-recovery"||name==L"boar-motion"||name==L"boar-profile") {
                    fixture=name==L"boar-motion"?"MISTWATCH 3 7 100 1 0 0 0 0 43 0 0 0":"MISTWATCH 3 7 100 1 0 0 0 0 46.5 0 0 0";
                    captureWildlife=name==L"boar-motion";
                    captureBoarOverview=name==L"boar-profile";
                }
                else if(name==L"river" || name==L"river-overlook") {
                    fixture="MISTWATCH 3 7 100 0 0 0 0 0 30 0 0 0";
                    captureRiverOverview=name==L"river-overlook";
                }
                else if(name==L"repair-shelter" || name==L"repair-player-camera") fixture="MISTWATCH 4 7 100 0 0 0 -7.25 0 18 0 0 0 0";
                else if(name==L"repair-ready") fixture="MISTWATCH 4 7 100 0 0 0 0 0 30 0 0 0 7";
                else if(name==L"settlement-clue") fixture="MISTWATCH 5 7 100 1 1 0 7.15 0 79 0 0 0 7 0 0";
                else if(name==L"settlement-clue-read") fixture="MISTWATCH 5 7 100 1 1 0 7.15 0 79 0 0 0 7 1 0";
                else if(name==L"fuse-lock-wrong") fixture="MISTWATCH 5 7 100 1 1 0 -6.9 0 59 0 0 0 7 1 0";
                else if(name==L"fuse-lock-ready") fixture="MISTWATCH 5 7 100 1 1 0 -6.9 0 59 0 0 0 7 1 50";
                else if(name==L"focus-pause") {captureFocusProbe=true;fixture="MISTWATCH 3 7 100 1 0 0 0 0 46.5 0 0 0";}
                else if(name==L"walk" || name==L"crouch"||name==L"audio-check") {captureWalk=true;captureCrouch=name==L"crouch";captureAudioProbe=name==L"audio-check";}
                else if(name==L"boulder-profile")captureBoulderOverview=true;
                else if(name==L"grass-profile")captureGrassOverview=true;
                else if(name==L"supply-profile"||name==L"supply-collected") {
                    captureSupplyOverview=true;
                    session.feet=Mistwatch::Session::supplies[0]+Vec3{0,0,-2};
                    if(name==L"supply-collected"&&!session.Collect(0))throw std::runtime_error("Supply collection fixture failed");
                }
                else if(name==L"grass-distance") {
                    captureGrassDistance=true;
                    wchar_t mode[32]{},enabled[3]{};
                    const auto modeLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_FIXTURE",mode,32);
                    const auto enabledLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_FIXTURE_ENABLED",enabled,3);
                    if(!modeLength||modeLength>=32||enabledLength!=1||(enabled[0]!=L'0'&&enabled[0]!=L'1')||
                        captureSequenceFrames||captureBenchmark||diagnosticProceduralGrass||diagnosticLegacyFern)
                        throw std::runtime_error("Invalid grass distance fixture configuration");
                    grassFixtureEnabled=enabled[0]==L'1';
                    const std::wstring_view distanceMode=mode;
                    if(distanceMode==L"player-near")grassFixturePlayerDistance=2.f;
                    else if(distanceMode==L"player-inside")grassFixturePlayerDistance=34.99f;
                    else if(distanceMode==L"player-edge")grassFixturePlayerDistance=35.f;
                    else if(distanceMode==L"player-outside")grassFixturePlayerDistance=35.01f;
                    else if(distanceMode==L"image-near")grassFixtureCameraDistance=2.f;
                    else if(distanceMode==L"image-middle")grassFixtureCameraDistance=12.f;
                    else if(distanceMode==L"image-far")grassFixtureCameraDistance=35.f;
                    else throw std::runtime_error("Unknown grass distance fixture mode");
                    // 每個程序從相同初始原配置選取同一株，不因腳或鏡頭移動重新尋找最近植株。
                    const auto plant=std::find_if(groundCover.begin(),groundCover.end(),[](Vec3 p){return Length(p)<12.f;});
                    if(plant==groundCover.end())throw std::runtime_error("Grass distance root missing");
                    grassFixtureIndex=std::size_t(plant-groundCover.begin());
                    session.feet=*plant+Vec3{grassFixturePlayerDistance,0,0};
                }
                else if(name==L"fern-profile")captureFernOverview=true;
                else throw std::runtime_error("Unknown capture scenario");
                if(!fixture.empty() && !session.Restore(fixture)) throw std::runtime_error("Invalid capture fixture");
                if(name==L"relay-west"||name==L"relay-aligned"||name==L"relay-linked"||name==L"relay-turn"||name==L"ridge-pan") {
                    session.feet=Mistwatch::Session::relaySites[0]+Vec3{1.15f,0,0};
                    session.feet.y=terrain.SampleHeight(session.feet.x,session.feet.z);
                    if(name==L"relay-aligned"||name==L"relay-linked") {
                        for(unsigned turn=0;turn<Mistwatch::Session::RelayTargetSector(0);++turn)
                            if(!session.TuneRelay(0,1))throw std::runtime_error("Relay fixture tuning failed");
                        if(name==L"relay-linked"&&!session.ConnectRelay(0))throw std::runtime_error("Relay fixture link failed");
                    }
                }
                controller.Reset(session.feet);
                solids=Mistwatch::WorldSolids(session.bridgeRepaired);
                ResetController();
                if(name==L"repair-shelter" || name==L"repair-player-camera") {
                    captureShelterOverview=name==L"repair-shelter";
                    explorer.Update({},1.f/60.f,true);
                    if(!explorer.PlaceFeet(session.feet,true,true,terrain,solids))throw std::runtime_error("Shelter capture IK failed");
                }
                orbit.yaw=captureWalk?-.6f:name==L"complete"?0.f:-.65f;
                message="TEST FIXTURE - not a playthrough or player save";
                if(name==L"river") {orbit.yaw=.5f;orbit.pitch=1.f;orbit.distance=10.f;}
                if(name==L"settlement-clue" || name==L"settlement-clue-read")orbit.yaw=-1.25f;
                if(name==L"fuse-lock-wrong" || name==L"fuse-lock-ready")orbit.yaw=1.25f;
                if(name==L"relay-west"||name==L"relay-aligned"||name==L"relay-linked"||name==L"relay-turn"||name==L"ridge-pan")orbit.yaw=1.25f;
                if(name==L"strike"||name==L"focus-pause") {
                    encounter.Tick(session.feet,1.f/60.f,false);
                    for(unsigned tick=0;tick<4;++tick)encounter.Tick(session.feet,.1f,false);
                }
                if(name==L"boar-recovery") {
                    encounter.Tick(session.feet,1.f/60.f,false);
                    for(unsigned tick=0;tick<9;++tick)encounter.Tick(session.feet,.1f,false);
                }
            }
        }
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
        wchar_t selectedFrame[4]{};
        const auto frameLength=!capture.empty()?GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE_FRAME",selectedFrame,4):0;
        if(frameLength) {
            const std::wstring_view frame=selectedFrame;
            if(frameLength>=4||!captureLandscapeMotion||captureSequenceFrames||captureBenchmark||
                (frame!=L"18"&&frame!=L"27"))throw std::runtime_error("Invalid selected ridge capture frame");
            captureCompleted=frame==L"18"?18u:27u;captureSequenceFrames=captureCompleted+1;
            orbit.yaw=1.25f+float(captureCompleted)*6.2831853f/36.f;
            waterTime=2.5f+float(captureCompleted)*.1f;
        }
#endif
        wchar_t noAo[8]{};
        diagnosticNoAo=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_NO_AO",noAo,8)>0 && noAo[0]==L'1';
        // 僅影像診斷允許單變量隔離；不改玩家預設品質或正式玩法。
        if(!capture.empty()) {
            const auto enabled=[](const wchar_t* name) {
                wchar_t value[3]{};const auto length=GetEnvironmentVariableW(name,value,3);
                if(!length)return false;
                if(length!=1||(value[0]!=L'0'&&value[0]!=L'1'))throw std::runtime_error("Invalid lighting diagnostic flag");
                return value[0]==L'1';
            };
            diagnosticNoGroundNormal=enabled(L"AEVRYND_MISTWATCH_NO_GROUND_NORMAL");
            diagnosticNoMsaa=enabled(L"AEVRYND_MISTWATCH_NO_MSAA");
            diagnosticNoShadows=enabled(L"AEVRYND_MISTWATCH_NO_SHADOWS");
            diagnosticNoBloom=enabled(L"AEVRYND_MISTWATCH_NO_BLOOM");
            diagnosticNoGroundTextures=enabled(L"AEVRYND_MISTWATCH_NO_GROUND_TEXTURES");
            if(diagnosticNoGroundTextures)groundMaterial.textures={};
            diagnosticNeutralGroundBase=enabled(L"AEVRYND_MISTWATCH_GROUND_NEUTRAL_BASE");
            diagnosticNeutralGroundSurface=enabled(L"AEVRYND_MISTWATCH_GROUND_NEUTRAL_SURFACE");
            if(diagnosticNeutralGroundBase||diagnosticNeutralGroundSurface) {
                // 中性測試圖由診斷收據固定；其他貼圖與品質不變。
                auto definition=Mistwatch::GroundDiagnosticDefinition(diagnosticLegacyGround,diagnosticNeutralGroundBase,diagnosticNeutralGroundSurface);
                if(!materials.Load(definition))throw std::runtime_error("Ground channel diagnostic material unavailable");
                groundMaterial.textures=materials.Textures(definition.name);
            }
            wchar_t stage[8]{};const auto stageLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_HDR_STAGE",stage,8);
            if(stageLength) {
                const std::wstring_view name=stage;
                if(stageLength>=8||!captureSequenceFrames)throw std::runtime_error("Invalid HDR diagnostic stage or capture mode");
                if(name==L"pre")diagnosticHdrStage=1;
                else if(name==L"post")diagnosticHdrStage=2;
                else if(name==L"final")diagnosticHdrStage=3;
                else throw std::runtime_error("Unknown HDR diagnostic stage");
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
                if(diagnosticHdrStage==1&&!EngineAtmosphereDiagnosticAccess3D::SetPreAtmosphereHdrCapture(e,true))throw std::runtime_error("Pre-atmosphere diagnostic setup failed");
                if(diagnosticHdrStage==2&&!EngineAtmosphereDiagnosticAccess3D::SetPostAtmosphereHdrCapture(e,true))throw std::runtime_error("Post-atmosphere diagnostic setup failed");
#else
                if(diagnosticHdrStage!=3)throw std::runtime_error("HDR stages require atmosphere diagnostic build");
#endif
            }
            if(diagnosticNoGroundNormal)groundMaterial.normalScale=0;
            if(diagnosticNoMsaa) {
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
                if(!EngineRendererTestAccess3D::SetMultisampling(e,false))throw std::runtime_error("MSAA diagnostic setup failed");
#else
                throw std::runtime_error("MSAA diagnostic requires internal renderer test hooks");
#endif
            }
        }
        wchar_t crown[8]{};
        diagnosticCrown=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CROWN",crown,8)>0 && crown[0]==L'1';
        if(!boar.Update({},0,encounter.phase,encounter.phaseTime,session.feet-encounter.feet)||
            !boar.PlaceFeet(encounter.feet,terrain))throw std::runtime_error("Boar initial pose failed");
        if(capture.empty()&&!save.empty()) {
            audioSettings=save.parent_path()/"audio-settings.txt";
            std::ifstream file(audioSettings);std::string line;
            if(std::getline(file,line))gameAudio.options.Restore(line);
        }
        // 自動影像驗證靜音，不寫玩家音量設定；播放收據仍記錄真實裝置與 voice 結果。
        if(!capture.empty())gameAudio.options.muted=true;
        gameAudio.Initialize(e.Audio());
    }
    void OnUpdate(Engine& e,float dt) override {
        if(captureFocusProbe&&!captureFocusComplete) {
            // 測試只送訊息給自己的隱藏視窗；不切換使用者前景視窗、不讀寫玩家存檔。
            const auto tick=++captureFocusTicks;
            if(tick==1){SendMessageW(e.Window(),WM_SETFOCUS,0,0);e.GetInput().ConsumePresses();SendMessageW(e.Window(),WM_KEYDOWN,'W',0);}
            if(tick==2){
                captureFocusSession=session.Serialize();captureFocusFeet=session.feet;
                captureFocusWater=waterTime;captureFocusEnemyTime=encounter.phaseTime;
                SendMessageW(e.Window(),WM_KILLFOCUS,0,0);
            }
            if(tick>=3&&tick<=19){
                if(!paused||session.Serialize()!=captureFocusSession||waterTime!=captureFocusWater||encounter.phaseTime!=captureFocusEnemyTime)
                    throw std::runtime_error("Focus pause failed: world advanced while paused");
                ++captureFocusFrozenTicks;
            }
            if(tick==18){SendMessageW(e.Window(),WM_SETFOCUS,0,0);SendMessageW(e.Window(),WM_KEYDOWN,'W',0);}
            if(tick==20){SendMessageW(e.Window(),WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(e.Window(),WM_KEYUP,VK_ESCAPE,0);}
            if(tick==21){
                if(paused||Length(session.feet-captureFocusFeet)<.001f||waterTime==captureFocusWater||encounter.phaseTime==captureFocusEnemyTime)
                    throw std::runtime_error("Focus resume failed: explicit ESC did not resume simulation");
                SendMessageW(e.Window(),WM_KEYUP,'W',0);
                captureFocusComplete=true;paused=true;
                message="FOCUS PAUSE VERIFIED - ESC resumes expedition";
            }
        }
        if(!capture.empty()&&(!captureFocusProbe||captureFocusComplete)) {
            if(captureAudioProbe&&!capturePending){++captureAudioTicks;paused=captureAudioTicks>=60&&captureAudioTicks<80;}
            if(captureWildlife&&!capturePending) {
                const auto previous=encounter.feet;
                session.Damage(encounter.Tick(session.feet,1.f/60.f,false));
                const auto look=(encounter.phase==Mistwatch::EnemyPhase::Windup?encounter.strikeTarget:session.feet)-encounter.feet;
                if(!boar.Update(encounter.feet-previous,1.f/60.f,encounter.phase,(std::max)(0.f,encounter.phaseTime),look)||
                    !boar.PlaceFeet(encounter.feet,terrain))throw std::runtime_error("Boar capture pose failed");
            }
            if(captureWalk && !capturePending&&!paused&&(!captureAudioProbe||captureAudioTicks<=180)){
                const auto previous=controller.Position();
                controller.TrySetHeight(captureCrouch?1.4f:1.8f,solids);
                controller.Step({{captureAudioProbe?.55f:0.f,0,1},false,captureCrouch?.45f:1.f},solids,terrain,1.f/60.f);
                session.feet=controller.Position();
                explorer.Update(session.feet-previous,1.f/60.f,captureCrouch);
                if(!explorer.PlaceFeet(session.feet,controller.Grounded(),captureCrouch,terrain,solids))
                    throw std::runtime_error("Capture locomotion IK failed");
                if(captureAudioProbe)for(unsigned index=0;index<3;++index)session.Collect(index);
            }
            if(captureAudioProbe)TickAudio(e,1.f/60.f);
            const auto stats=e.GetRendererStats();
            // 排除啟動前六十幀；僅採用已完成且序號不重複的 GPU timestamp 收據。
            const auto& timing=stats.lastCompletedGpuTimingReceipt;
            if(captureBenchmark && timing.serial>60 && timing.serial>benchmarkLastSerial &&
               timing.Complete() && benchmarkGpuMilliseconds.size()<120) {
                benchmarkLastSerial=timing.serial;
                benchmarkGpuMilliseconds.push_back(double(timing.nanoseconds)/1000000.0);
            }
            if(capturePending && stats.rejectedFrameCaptures)throw std::runtime_error("Frame capture rejected");
            if(capturePending && stats.successfulFrameCaptures>captureConsumed) {
                if(diagnosticHdrStage&&(captureCompleted==18||captureCompleted==27)) {
                    e.WaitRendererIdle3D();
                    const auto hdr=e.TakeLinearHdrReadback3D();
                    if(!hdr||!hdr->Complete())throw std::runtime_error("HDR diagnostic readback incomplete");
                    auto path=CapturePath();path+=L".hdr-half";
                    std::ofstream raw(path,std::ios::binary);
                    raw.write(reinterpret_cast<const char*>(hdr->rawHalfBytes.data()),std::streamsize(hdr->rawHalfBytes.size()));
                    raw.close();if(!raw)throw std::runtime_error("HDR diagnostic write failed");
                    path+=L".receipt.txt";std::ofstream proof(path);
                    proof<<"diagnostic_only=1\nstage="<<diagnosticHdrStage<<"\nframe_serial="<<hdr->receipt.rasterProvenance.frameSerial
                        <<"\nwidth="<<hdr->receipt.width<<"\nheight="<<hdr->receipt.height<<"\nbytes="<<hdr->rawHalfBytes.size()
                        <<"\nfinite_channels="<<hdr->receipt.statistics.finiteChannelCount<<"\nnan_channels="<<hdr->receipt.statistics.nanChannelCount
                        <<"\ncamera_hash="<<hdr->receipt.requestMetadata.cameraHash<<"\nstate_hash="<<hdr->receipt.requestMetadata.stateHash<<'\n';
                    proof.close();if(!proof)throw std::runtime_error("HDR diagnostic receipt write failed");
                }
                auto receipt=CapturePath();receipt+=".receipt.txt";
                std::ofstream out(receipt);
                out<<"capture=success\nwater_phase_seconds="<<waterTime
                    <<"\nwater_surface_fingerprint="<<WaterSurfaceFingerprint3D(Mistwatch::RiverSurface())
                    <<"\nwater_single_composite="<<stats.waterSingleCompositeEnabled
                    <<"\nwater_composite_draws="<<stats.waterSingleCompositeDraws
                    <<"\nrefraction="<<stats.screenSpaceWaterRefractionEnabled
                    <<"\nrefraction_passes="<<stats.screenSpaceWaterRefractionPasses
                    <<"\nscreen_space_reflections="<<stats.screenSpaceReflectionsEnabled
                    <<"\nreflection_passes="<<stats.screenSpaceReflectionPasses<<'\n';
                out<<"output_width="<<stats.outputWidth<<"\noutput_height="<<stats.outputHeight
                    <<"\nadapter_luid_low="<<stats.adapterLuidLow<<"\nadapter_luid_high="<<stats.adapterLuidHigh
                    <<"\ndebug_errors="<<stats.debugErrorCount<<"\ndebug_warnings="<<stats.debugWarningCount<<'\n';
                out<<"debug_layer_enabled="<<stats.debugLayerEnabled<<"\ngpu_validation_requested="<<stats.gpuValidationRequested
                    <<"\ngpu_validation_available="<<stats.gpuValidationAvailable<<"\ngpu_validation_executed="<<stats.gpuValidationExecuted
                    <<"\ngpu_validation_passed="<<stats.gpuValidationPassed<<'\n';
                out<<"mission_stage="<<unsigned(session.CurrentStage())<<"\nrepair_materials="<<session.repairMaterials
                    <<"\ncharacter_height="<<controller.Settings().height<<'\n';
                out<<"fuse_clue_read="<<session.fuseClueRead<<"\nfuse_dials="<<session.fuseDials
                    <<"\nfuse_collected="<<session.fuseCollected<<'\n';
                out<<"understory_ferns="<<understory.ferns.size()<<"\nunderstory_saplings="<<understory.saplings.size()
                    <<"\nsubmitted_ferns="<<submittedFerns<<"\nsubmitted_saplings="<<submittedSaplings<<'\n';
                out<<"fern_source="<<(diagnosticLegacyFern?"procedural":"poly-haven-fern-02")
                    <<"\nfern_variants=4\nfern_asset_scale="<<Mistwatch::FernAssetScale
                    <<"\nfern_materials="<<fernMaterials.Stats().materials<<"\nsubmitted_fern_triangles="<<submittedFernTriangles
                    <<"\nfern_alpha_cutoff="<<fernSurface.alphaCutoff<<"\nfern_use_uv="<<fernSurface.useUv<<'\n';
                out<<"grass_tint_scale="<<(diagnosticDimGrass?.7f:1.f)<<'\n';
                out<<"supply_visual=planked_wood_crate\nsupply_mesh_resources=2\nsupply_wood_triangles="<<supplyMeshes[0].TriangleCount()
                    <<"\nsupply_metal_triangles="<<supplyMeshes[1].TriangleCount()<<"\nsubmitted_supply_crates="<<submittedSupplies
                    <<"\nsupply_collected_mask="<<session.collected<<"\nsupply_wood_textured="<<bool(woodMaterial.textures)<<'\n';
                out<<"grass_source="<<(diagnosticProceduralGrass?"procedural":"poly-haven-grass-medium-02")
                    <<"\ngrass_variants="<<(diagnosticProceduralGrass?1:5)<<"\ngrass_minimum_asset_height="<<Mistwatch::GrassAssetHeight
                    <<"\ngrass_materials="<<grassMaterials.Stats().materials<<"\nsubmitted_grass_triangles="<<submittedGrassTriangles
                    <<"\ngrass_alpha_cutoff="<<grassSurface.alphaCutoff<<"\ngrass_use_uv="<<grassSurface.useUv<<'\n';
                out<<"enemy_visual=skinned_boar\nenemy_joints="<<boar.model.skeleton.joints.size()
                    <<"\nenemy_phase="<<unsigned(encounter.phase)<<"\nenemy_brace="<<boar.brace<<'\n';
                out<<"relay_linked="<<session.relayLinked<<"\nrelay_sectors="<<session.relaySectors
                    <<"\nrelay_target_west="<<Mistwatch::Session::RelayTargetSector(0)<<"\nrelay_signal_west="<<session.RelaySignal(0)<<'\n';
                out<<"landscape_vertices="<<landscapeMesh.vertexCount<<"\nlandscape_triangles="<<landscapeMesh.TriangleCount()
                    <<"\nlandscape_trees="<<landscapeTrees.size()<<"\ncamera_orbit_yaw="<<orbit.yaw<<'\n';
                out<<"landscape_river_release_m=24\n";
                out<<"grass_style="<<(diagnosticProceduralGrass?(diagnosticLegacyGrass?"upright":"curved"):"scanned")
                    <<"\ngrass_blades="<<(diagnosticProceduralGrass?"9":"source-geometry")<<"\ngrass_segments="<<(diagnosticProceduralGrass?(diagnosticLegacyGrass?"3":"5"):"source-geometry")
                    <<"\ngrass_vertices="<<(diagnosticProceduralGrass?grassMesh.vertexCount:7031)
                    <<"\ngrass_triangles="<<(diagnosticProceduralGrass?grassMesh.TriangleCount():7842)<<"\nground_cover_instances="<<groundCover.size()
                    <<"\nsubmitted_grass="<<submittedGrass<<'\n';
                out<<"broadleaf_trees="<<broadleafTrees.size()<<"\nbroadleaf_mesh_triangles="<<broadleafTriangles
                    <<"\nbroadleaf_materials="<<broadleafMaterials.Stats().materials<<'\n';
                out<<"boulder_instances="<<std::count_if(forestProps.begin(),forestProps.end(),[](const auto& p){return !p.FallenLog();})
                    <<"\nboulder_mesh_triangles="<<propMeshes[0].TriangleCount()<<"\nboulder_materials="<<boulderMaterials.Stats().materials
                    <<"\nboulder_use_uv="<<boulderMaterial.useUv<<"\nboulder_texture_scale="<<boulderMaterial.textureScale
                    <<"\nboulder_gpu_resources="<<unsigned(bool(propMeshes[0]))<<'\n';
                if(captureFocusProbe)out<<"focus_pause_verified="<<captureFocusComplete<<"\nfocus_frozen_ticks="<<captureFocusFrozenTicks
                    <<"\nfocus_explicit_resume_verified="<<captureFocusComplete<<"\npaused="<<paused<<'\n';
                out<<"ground_normal_scale="<<groundMaterial.normalScale<<"\ndiagnostic_no_msaa="<<diagnosticNoMsaa
                    <<"\nground_textured="<<bool(groundMaterial.textures)
                    <<"\nground_source="<<(diagnosticLegacyGround?"ground037":"forest-ground-04")
                    <<"\nground_maximum_resolution="<<groundDefinition.textures.maximumResolution
                    <<"\nground_packed_orm="<<(groundDefinition.textures.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrm)
                    <<"\nground_texture_scale="<<groundMaterial.textureScale<<"\nground_use_uv="<<groundMaterial.useUv
                    <<"\nlast_draw_calls="<<stats.lastDrawCalls<<"\nresident_texture_bytes="<<stats.residentTextureBytes
                    <<"\nterrain_vertices="<<terrainMesh.vertexCount<<"\nterrain_triangles="<<terrainMesh.TriangleCount()
                    <<"\nground_neutral_base="<<diagnosticNeutralGroundBase<<"\nground_neutral_surface="<<diagnosticNeutralGroundSurface
                    <<"\ndiagnostic_no_shadows="<<diagnosticNoShadows<<"\ndiagnostic_no_bloom="<<diagnosticNoBloom
                    <<"\ndiagnostic_hdr_stage="<<diagnosticHdrStage
                    <<"\nmultisample_count="<<stats.multisampleCount<<"\nquality_shadows="<<stats.effectiveQuality.directionalShadows
                    <<"\nquality_ao="<<stats.effectiveQuality.ambientOcclusion<<"\nquality_post_aa="<<stats.effectiveQuality.postProcessAntiAliasing<<'\n';
                out<<"shadow_cascades="<<stats.lastDirectionalShadowCascades<<"\nbloom_enabled="<<stats.bloomEnabled
                    <<"\nvolumetric_passes="<<stats.volumetricAtmosphereCompositePasses<<"\nvolumetric_scale="<<stats.volumetricAtmosphereResolutionScale
                    <<"\nvolumetric_width="<<stats.volumetricAtmosphereWidth<<"\nvolumetric_height="<<stats.volumetricAtmosphereHeight<<'\n';
                const auto audio=e.Audio().GetStats();
                std::uint64_t steps{};for(unsigned i=0;i<6;++i)steps+=gameAudio.requests[i];
                out<<"audio_device="<<audio.deviceAvailable<<"\naudio_sounds="<<audio.residentSounds
                    <<"\naudio_bytes="<<audio.residentBytes<<"\naudio_plays="<<audio.successfulPlays<<"\naudio_dropped="<<audio.droppedPlays
                    <<"\naudio_peak_voices="<<audio.peakActiveVoices<<"\naudio_master="<<audio.masterVolume
                    <<"\naudio_step_requests="<<steps<<"\naudio_interact_requests="<<gameAudio.requests[unsigned(Mistwatch::Sound::Interact)]
                    <<"\naudio_wind_requests="<<gameAudio.requests[unsigned(Mistwatch::Sound::Wind)]<<"\naudio_probe_ticks="<<captureAudioTicks<<'\n';
                out<<"camera_source="<<((diagnosticCrown||captureRiverOverview||captureShelterOverview||captureBoarOverview||captureBoulderOverview||captureGrassOverview||captureGrassDistance||captureFernOverview||captureSupplyOverview)?"diagnostic":"player")
                    <<"\ncamera_distance="<<Length(lastRenderedCamera.position-lastRenderedCamera.target)
                    <<"\ncamera_height="<<lastRenderedCamera.position.y
                    <<"\ncamera_anchor_above_feet="<<lastRenderedCamera.target.y-session.feet.y<<'\n';
                out<<"camera_position="<<lastRenderedCamera.position.x<<','<<lastRenderedCamera.position.y<<','<<lastRenderedCamera.position.z
                    <<"\ncamera_target="<<lastRenderedCamera.target.x<<','<<lastRenderedCamera.target.y<<','<<lastRenderedCamera.target.z
                    <<"\ncamera_up="<<lastRenderedCamera.up.x<<','<<lastRenderedCamera.up.y<<','<<lastRenderedCamera.up.z
                    <<"\ncamera_fov="<<lastRenderedCamera.projection.verticalFieldOfView<<'\n';
                if(captureGrassDistance) {
                    const auto& proof=grassFixtureFrozen;
                    out<<std::setprecision(std::numeric_limits<float>::max_digits10)
                        <<"grass_fixture_schema=1\ngrass_fixture_index="<<grassFixtureIndex
                        <<"\ngrass_fixture_variant="<<grassFixtureIndex%5
                        <<"\ngrass_fixture_identity_high=5572150615523553875\ngrass_fixture_identity_low="<<grassFixtureIndex+1
                        <<"\ngrass_fixture_generation=1\ngrass_fixture_enabled="<<grassFixtureEnabled
                        <<"\ngrass_fixture_feet_mode=synthetic_same_height\ngrass_fixture_root="<<proof.root.x<<','<<proof.root.y<<','<<proof.root.z
                        <<"\ngrass_fixture_feet="<<proof.feet.x<<','<<proof.feet.y<<','<<proof.feet.z
                        <<"\ngrass_fixture_scale="<<proof.scale<<"\ngrass_fixture_yaw="<<proof.yaw
                        <<"\ngrass_fixture_player_distance="<<proof.playerDistance
                        <<"\ngrass_fixture_camera_distance="<<proof.cameraDistance
                        <<"\ngrass_fixture_gate_selected="<<proof.selected
                        <<"\ngrass_fixture_scene_accepted="<<proof.sceneAccepted
                        <<"\ngrass_fixture_opaque_draw_instances="<<proof.opaque
                        <<"\ngrass_fixture_shadow_only_instances="<<proof.shadowOnly
                        <<"\ngrass_fixture_draw_input_triangles="<<proof.triangles
                        <<"\ngrass_fixture_scene_serial="<<proof.sceneSerial
                        <<"\ngrass_fixture_camera_hash="<<proof.cameraHash
                        <<"\ngrass_fixture_selection_limit=35\ngrass_fixture_image_kind=production_final_image_not_direct_pixel_count\n"
                        <<std::setprecision(6);
                }
                if(captureBenchmark) {
                    auto ordered=benchmarkGpuMilliseconds;std::sort(ordered.begin(),ordered.end());
                    out<<"benchmark_mode=fixed_scene_gpu_only\ngpu_samples="<<ordered.size()
                        <<"\ngpu_mean_ms="<<std::accumulate(ordered.begin(),ordered.end(),0.0)/double(ordered.size())
                        <<"\ngpu_p50_ms="<<ordered[59]<<"\ngpu_p95_ms="<<ordered[113]
                        <<"\ngpu_max_ms="<<ordered.back()<<'\n';
                }
                out.close();if(!out)throw std::runtime_error("Capture receipt write failed");
                ++captureCompleted;++captureConsumed;
                if(!captureSequenceFrames || captureCompleted>=captureSequenceFrames)e.Quit();
                else {
                    capturePending=false;waterTime=2.5f+float(captureCompleted)*.1f;
                    // 固定按鍵輸入展示完整一圈，再回到訊號方向；不冒充人工遊玩。
                    if(captureRelayMotion&&captureCompleted<20&&!session.TuneRelay(0,1))throw std::runtime_error("Relay motion input failed");
                    if(captureRelayMotion&&captureCompleted==20&&!session.ConnectRelay(0))throw std::runtime_error("Relay motion link failed");
                    if(captureLandscapeMotion)orbit.yaw=1.25f+float(captureCompleted)*6.2831853f/36.f;
                }
            }
            // GPU 驗證會插入額外 shader 檢查；診斷等待不作為正式效能數據。
            const auto captureTimeout=std::chrono::seconds(stats.gpuValidationRequested?120:30);
            if(std::chrono::steady_clock::now()-captureStart>captureTimeout)
                throw std::runtime_error("Frame capture timeout; rendered="+std::to_string(captureFrames)+
                    "; submitted="+std::to_string(stats.submittedFrames)+"; gpu-validation="+std::to_string(stats.gpuValidationRequested));
            return;
        }
        auto& in=e.GetInput();
        Mistwatch::UpdatePlaybackControl(started,paused,in.Focused()&&!in.FocusLost(),in.Pressed(VK_RETURN),in.Pressed(VK_ESCAPE));
        bool changedAudio=false;
        if(in.Pressed('M')){gameAudio.options.muted=!gameAudio.options.muted;changedAudio=true;}
        if(in.Pressed(VK_PRIOR)){gameAudio.options.volume=std::clamp(gameAudio.options.volume+.1f,0.f,1.f);changedAudio=true;}
        if(in.Pressed(VK_NEXT)){gameAudio.options.volume=std::clamp(gameAudio.options.volume-.1f,0.f,1.f);changedAudio=true;}
        if(changedAudio){e.Audio().SetMasterVolume(gameAudio.options.Gain());SaveAudioOptions();}
        if(!started || paused){TickAudio(e,dt);return;}
        if(std::isfinite(dt) && dt>0)waterTime=std::fmod(waterTime+(std::min)(dt,.1f),3600.f);
        if(in.Pressed(VK_F9) && !save.empty()) {
            std::ifstream file(save,std::ios::binary);
            if(session.RestoreStream(file)){solids=Mistwatch::WorldSolids(session.bridgeRepaired);ResetController();encounter={};ResetBoarPose();evasion={};message="Loaded";}else message="Save unavailable";
        }
        if(in.Pressed('R') && session.beaconLit) {
            session={};controller.Reset({});gameAudio.director.Reset();solids=Mistwatch::WorldSolids(false);encounter={};ResetBoarPose();evasion={};orbit={};message="New expedition";
        }
        if(in.Pressed('R') && !session.Alive()) {session.Retry();ResetController();encounter={};ResetBoarPose();evasion={};}
        if(!session.Alive() || session.beaconLit){TickAudio(e,dt);return;}
        orbit.Update(in.MouseDown() && in.MouseInsideCanvas()?in.MouseDelta():Vec2{},in.MouseWheel(),float(in.Down(VK_RIGHT))-float(in.Down(VK_LEFT)),dt);
        const float yaw=orbit.yaw;
        const float x=float(in.Down('D'))-float(in.Down('A'));
        const float z=float(in.Down('W'))-float(in.Down('S'));
        if(!controller.TrySetHeight(in.Down(VK_CONTROL)?1.4f:1.8f,solids))
            message="Low ceiling - move clear before standing";
        const bool crouched=controller.Settings().height<1.5f;
        const auto previousFeet=controller.Position();
        Vec3 movement{x*std::cos(yaw)-z*std::sin(yaw),0,x*std::sin(yaw)+z*std::cos(yaw)};
        // 無方向輸入時 Q 向鏡頭後方閃避；非閃避時維持原移動輸入。
        const bool evade=in.Pressed('Q');
        const Vec3 dodgeDirection=evade && Length(movement)<.001f?Vec3{std::sin(yaw),0,-std::cos(yaw)}:movement;
        const auto motion=evasion.Tick(dt,evade,controller.Grounded(),crouched,dodgeDirection);
        controller.Step({motion.dashing?motion.movement:movement,
            in.Pressed(VK_SPACE) && !crouched && !motion.dashing,
            motion.dashing?Mistwatch::Evasion::SpeedMultiplier:crouched?.45f:in.Down(VK_SHIFT)?1.5f:1.f},solids,terrain,dt);
        session.feet=controller.Position();
        explorer.Update(session.feet-previousFeet,dt,crouched);
        if(!explorer.PlaceFeet(session.feet,controller.Grounded(),crouched,terrain,solids))
            throw std::runtime_error("Explorer foot IK failed");
        if(session.feet.y < -5.f) {session.Damage(100);message="Fell into the ravine - R to retry";TickAudio(e,dt);return;}
        for(unsigned lever=0;lever<3;++lever)if(in.Pressed('1'+lever)) {
            if(session.CurrentStage()==Mistwatch::Stage::Relays) {
                for(unsigned index=0;index<3;++index)if(lever<2&&session.TuneRelay(index,lever==0?-1:1))message="Antenna rotated - find the green signal lock, then E";
            }
            else if(session.CycleFuseDial(lever))message="Cabinet dial turned - match the ranger log, then E";
            else if(session.ToggleCircuit(lever))message="Circuit switched - light all three lamps";
        }
        if(in.Pressed('E')) {
            bool changed=false;
            for(unsigned i=0;i<3;++i) changed=session.Collect(i)||changed;
            for(unsigned i=0;i<3;++i)changed=session.CollectRepairMaterial(i,crouched)||changed;
            if(session.RepairBridge()) {solids=Mistwatch::WorldSolids(true);changed=true;}
            changed=session.ReadFuseClue()||changed;
            changed=session.CollectFuse()||changed;
            changed=session.RepairGenerator()||changed;
            for(unsigned index=0;index<3;++index)changed=session.ConnectRelay(index)||changed;
            changed=session.LightBeacon()||changed;
            message=changed?"Objective updated":session.CurrentStage()==Mistwatch::Stage::RepairMaterials?
                "Find tools in west shelter (CTRL), and both timber caches":
                session.CurrentStage()==Mistwatch::Stage::Fuse && session.NearInterior(Mistwatch::Session::fuse)?
                "Cabinet locked - read ranger log, set dials 1 / 2 / 3":
                session.CurrentStage()==Mistwatch::Stage::Relays?"Ridge relay: 1 left / 2 right, green signal lock, then E":"Move closer to the objective";
        }
        if(session.CurrentStage()==Mistwatch::Stage::Encounter) {
            const auto previous=encounter.feet;
            session.Damage(encounter.Tick(session.feet,dt,in.Pressed('F') && !motion.dashing));
            const auto look=(encounter.phase==Mistwatch::EnemyPhase::Windup?encounter.strikeTarget:session.feet)-encounter.feet;
            if(!boar.Update(encounter.feet-previous,(std::min)(dt,.1f),encounter.phase,(std::max)(0.f,encounter.phaseTime),look)||
                !boar.PlaceFeet(encounter.feet,terrain))throw std::runtime_error("Boar locomotion IK failed");
            if(!encounter.health) session.FinishEncounter(encounter.feet);
            if(encounter.phase==Mistwatch::EnemyPhase::Windup) message="Leave the marked circle - Q: evade";
            else if(encounter.phase==Mistwatch::EnemyPhase::Recovery) message="Boar recovering - counter with F";
        }
        if(in.Pressed(VK_F5) && !save.empty()) {
            std::error_code error;
            std::filesystem::create_directories(save.parent_path(),error);
            auto temporary=save; temporary+=".tmp";
            std::ofstream out(temporary); out<<session.Serialize(); out.close();
            message=out && MoveFileExW(temporary.c_str(),save.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?"Saved":"Save failed";
        }
        TickAudio(e,dt);
    }
    void OnRender(Engine& e,float) override {
        scene.Clear();
        scene.Mesh(terrainMesh,{},groundMaterial);
        scene.Mesh(landscapeMesh,{},groundMaterial);
        // 背景林使用既有 CC0 冷杉最低網格細節；不加入任務區的角色碰撞體。
        for(const auto& tree:landscapeTrees)for(unsigned slot=0;slot<4;++slot)
            scene.Mesh(landscapeFirMeshes[slot],tree,firMaterials[slot]);
        for(const auto& tree:broadleafTrees)for(unsigned slot=0;slot<3;++slot)
            scene.Mesh(broadleafMeshes[slot],tree.transform,broadleafSurface[slot]);
        scene.Mesh(riverMesh,{},Mistwatch::RiverMaterial());
        for(const auto tree:trees) {
            for(unsigned slot=0;slot<4;++slot)
                scene.Mesh(firMeshes[slot],{tree,{0,tree.z,0},{.65f,.65f,.65f}},firMaterials[slot]);
        }
        submittedSaplings=0;
        for(const auto& tree:understory.saplings) {
            for(unsigned slot=0;slot<4;++slot)scene.Mesh(firMeshes[slot],tree,firMaterials[slot]);
            ++submittedSaplings;
        }
        Material3D fern{{1,1,1,1},{.92f,0,0,.65f}};
        fern.materialClass=MaterialClass3D::Foliage;
        fern.doubleSided=true;
        submittedFerns=0;submittedFernTriangles=0;
        for(std::size_t index=0;index<understory.ferns.size();++index) {
            auto plant=understory.ferns[index];
            const float visibility=Mistwatch::UnderstoryVisibility(Length(plant.position-session.feet));
            if(visibility<=0)continue;
            plant.scale=plant.scale*visibility;
            const auto& mesh=diagnosticLegacyFern?fernMesh:scannedFerns[index%4];
            scene.Mesh(mesh,plant,diagnosticLegacyFern?fern:fernSurface);
            submittedFernTriangles+=mesh.TriangleCount();
            ++submittedFerns;
        }
        const float grassTintScale=diagnosticDimGrass?.7f:1.f;
        Material3D grass{{.32f*grassTintScale,.43f*grassTintScale,.18f*grassTintScale,1},{.9f,0,0,.9f}};
        grass.materialClass=MaterialClass3D::Foliage;
        grass.doubleSided=true;
        submittedGrass=0;
        submittedGrassTriangles=0;
        grassFixtureState={};
        for(std::size_t index=0;index<groundCover.size();++index) {
            const auto plant=groundCover[index];
            const float scale=.5f+.25f*std::sin(plant.x*1.7f+plant.z);
            const bool selected=Mistwatch::GrassRootSelected(plant,session.feet);
            const bool fixtureTarget=captureGrassDistance&&index==grassFixtureIndex;
            if(fixtureTarget){grassFixtureState.root=plant;grassFixtureState.feet=session.feet;
                grassFixtureState.scale=scale;grassFixtureState.yaw=plant.x+plant.z;
                grassFixtureState.playerDistance=Length(plant-session.feet);grassFixtureState.selected=selected;}
            if(!selected||(fixtureTarget&&!grassFixtureEnabled))continue;
            const auto& mesh=diagnosticProceduralGrass?grassMesh:scannedGrass[index%5];
            const RenderObjectIdentity3D identity=fixtureTarget?RenderObjectIdentity3D{{5572150615523553875ull,index+1},1}:RenderObjectIdentity3D{};
            const auto accepted=scene.Mesh(mesh,{plant,{0,plant.x+plant.z,0},{scale,scale,scale}},diagnosticProceduralGrass?grass:grassSurface,{},identity);
            if(!accepted)throw std::runtime_error("Grass scene insertion failed");
            if(fixtureTarget)grassFixtureState.sceneAccepted=true;
            submittedGrassTriangles+=mesh.TriangleCount();
            ++submittedGrass;
        }
        for(const auto& prop:forestProps)scene.Mesh(propMeshes[prop.FallenLog()?1:0],prop.transform,prop.FallenLog()?barkMaterial:boulderMaterial);
        submittedSupplies=0;
        Material3D supplyMetal{{1,1,1,1},{.65f,.7f,0,1}};
        for(unsigned i=0;i<3;++i) if(!(session.collected&(1u<<i))) {
            const Transform3D transform{Mistwatch::Session::supplies[i],{},{1,1,1}};
            if(!scene.Mesh(supplyMeshes[0],transform,woodMaterial)||!scene.Mesh(supplyMeshes[1],transform,supplyMetal))
                throw std::runtime_error("Supply crate scene insertion failed");
            ++submittedSupplies;
        }
        if(session.bridgeRepaired) scene.MaterialBox({0,-.1f,36},{4,.2f,8},woodMaterial);
        else scene.Box(Mistwatch::Session::bridge+Vec3{1,.35f,0},{1,.7f,1},{.3f,.12f,.08f,1});
        for(const auto solid:Mistwatch::RepairShelterSolids())scene.MaterialBox(solid.center,solid.extents*2.f,woodMaterial);
        for(unsigned site=0;site<3;++site) {
            const auto point=Mistwatch::Session::repairSites[site];
            if(site==0)scene.Box(point+Vec3{0,.49f,0},{.5f,.06f,.3f},(session.repairMaterials&1)?Color{.3f,.3f,.25f,1}:Color{.82f,.65f,.15f,1});
            if(session.repairMaterials&(1u<<site))continue;
            if(site!=0)for(unsigned beam=0;beam<3;++beam)
                scene.Mesh(propMeshes[1],{point+Vec3{0,.18f+.22f*float(beam),0},{0,0,1.5707963f},{.22f,1.8f,.22f}},woodMaterial);
            // 小型路標讓材料位置可辨識，不用懸浮的大型目標方塊遮住場景。
            const auto sign=point+Vec3{site==0?2.8f:1.3f,0,0};
            scene.MaterialBox(sign+Vec3{0,.6f,0},{.12f,1.2f,.12f},woodMaterial);
            scene.Box(sign+Vec3{0,1.12f,0},{.7f,.34f,.1f},{.78f,.58f,.15f,1});
        }
        if(session.CurrentStage()==Mistwatch::Stage::Encounter && encounter.phase==Mistwatch::EnemyPhase::Windup) {
            const float progress=std::clamp(1.f-encounter.phaseTime/Mistwatch::Encounter::WindupDuration,0.f,1.f);
            for(unsigned segment=0;segment<48;++segment) {
                const auto point=[&](unsigned index,float radius){
                    const float angle=float(index)*6.2831853f/48.f;
                    Vec3 p=encounter.strikeTarget+Vec3{std::cos(angle)*radius,0,std::sin(angle)*radius};
                    p.y=terrain.SampleHeight(p.x,p.z)+.06f;return p;
                };
                scene.Line(point(segment,Mistwatch::Encounter::StrikeRadius),point(segment+1,Mistwatch::Encounter::StrikeRadius),4.f,{1,.25f,.04f,1});
                scene.Line(point(segment,Mistwatch::Encounter::StrikeRadius*progress),point(segment+1,Mistwatch::Encounter::StrikeRadius*progress),2.f,{1,.75f,.12f,1});
            }
        }
        scene.Box(Mistwatch::Session::beacon+Vec3{0,2,0},{.7f,4,.7f},session.beaconLit?Color{1,.7f,.2f,1}:Color{.3f,.35f,.4f,1});
        const auto workshop=Mistwatch::Session::fuse;
        for(const auto solid:Mistwatch::WorkshopSolids()) scene.MaterialBox(solid.center,solid.extents*2.f,woodMaterial);
        const auto cabinet=Mistwatch::FuseCaseSolid();
        scene.Box(cabinet.center,cabinet.extents*2.f,{.18f,.25f,.26f,1});
        for(unsigned wheel=0;wheel<3;++wheel) {
            const float z=workshop.z-.25f+float(wheel)*.25f;
            scene.Box({cabinet.center.x+.36f,.7f,z},{.03f,.14f,.14f},
                session.fuseCollected?Color{.2f,1,.3f,1}:Color{1,.65f,.12f,1});
        }
        for(const auto solid:Mistwatch::RangerHutSolids())scene.MaterialBox(solid.center,solid.extents*2.f,woodMaterial);
        scene.Box(Mistwatch::Session::rangerLog+Vec3{.9f,1.02f,0},{.55f,.03f,.65f},{.85f,.82f,.65f,1});
        scene.MaterialBox({2,.8f,79},{.12f,1.6f,.12f},woodMaterial);
        scene.Box({2,1.5f,79},{1,.35f,.12f},{.65f,.62f,.25f,1});
        const auto generatorSolid=Mistwatch::GeneratorSolid();
        scene.Box(generatorSolid.center,generatorSolid.extents*2.f,{.25f,.3f,.28f,1});
        for(unsigned lamp=0;lamp<3;++lamp)
            scene.Box(Mistwatch::Session::generator+Vec3{-.5f+float(lamp)*.5f,1.3f,0},{.2f,.2f,.2f},session.circuit&(1u<<lamp)?Color{.2f,1,.3f,1}:Color{1,.15f,.1f,1});
        Material3D antenna{{1,1,1,1},{.45f,.65f,0,1},false};antenna.doubleSided=true;
        for(unsigned index=0;index<3;++index) {
            const auto p=Mistwatch::Session::relaySites[index];
            const auto relaySolids=Mistwatch::RelaySolids(index);
            scene.MaterialBox(relaySolids[0].center,relaySolids[0].extents*2.f,rockMaterial);
            scene.Box(relaySolids[1].center,relaySolids[1].extents*2.f,{.24f,.28f,.26f,1});
            const float angle=float(session.RelaySector(index))*6.2831853f/16.f;
            const Vec3 direction{std::sin(angle),0,std::cos(angle)};
            scene.Mesh(relayDish,{p+Vec3{0,2.15f,0}+direction*.5f,{0,angle,0},{1,1,1}},antenna);
            scene.Mesh(box,{p+Vec3{0,2.15f,0}+direction*.15f,{0,angle,0},{.065f,.065f,.3f}},antenna);
            const bool linked=(session.relayLinked&(1u<<index))!=0;
            const bool aligned=session.RelaySector(index)==Mistwatch::Session::RelayTargetSector(index);
            const Color lamp=linked?Color{.1f,1,.4f,1}:session.powerRestored&&aligned?Color{.4f,1,.2f,1}:Color{1,.55f,.08f,1};
            scene.Box(p+Vec3{index==1?-.69f:.69f,.7f,0},{.08f,.18f,.25f},lamp);
            scene.Line(p+Vec3{0,.74f,0},p+Vec3{direction.x,.74f,direction.z},3.f,lamp);
        }
        auto camera=Mistwatch::BuildPlayerCamera(session.feet,controller.Settings().height,orbit,solids,terrain);
        if(diagnosticCrown && !capture.empty()) camera=MakeThirdPersonCamera({-7,6,-5},0,.1f,8);
        // 遠景只用於地形影像驗收，不改變玩家可操作鏡頭的距離上限。
        if(captureRiverOverview && !capture.empty()) {camera.position={-22,28,8};camera.target={0,-2,36};}
        if(captureShelterOverview && !capture.empty()) {camera.position={-3,1.1f,17};camera.target={-7.25f,.7f,18};}
        if(captureBoarOverview && !capture.empty()) {camera.position=encounter.feet+Vec3{3,1.4f,1};camera.target=encounter.feet+Vec3{0,.55f,0};}
        if(captureBoulderOverview && !capture.empty()) {
            const auto rock=std::find_if(forestProps.begin(),forestProps.end(),[](const auto& p){return !p.FallenLog();});
            if(rock==forestProps.end())throw std::runtime_error("Boulder detail fixture missing");
            camera.target=rock->collider.center;
            camera.position=camera.target+Vec3{2.f,1.1f,-2.f};
        }
        if(captureGrassOverview && !capture.empty()) {
            const auto plant=std::find_if(groundCover.begin(),groundCover.end(),[&](Vec3 p){return Length(p-session.feet)<12.f;});
            if(plant==groundCover.end())throw std::runtime_error("Grass detail fixture missing");
            camera.target=*plant+Vec3{0,.3f,0};
            camera.position=camera.target+Vec3{1.15f,.55f,-1.15f};
        }
        if(captureSupplyOverview&&!capture.empty()) {
            camera.target=Mistwatch::Session::supplies[0]+Vec3{0,.4f,0};
            camera.position=camera.target+Vec3{1.25f,.8f,-1.6f};
        }
        if(captureGrassDistance&&!capture.empty()) {
            camera.target=groundCover[grassFixtureIndex]+Vec3{0,.3f,0};
            // 固定方向與目標，僅改鏡頭距離；不把合成腳位置當可行走地形驗收。
            camera.position=camera.target+Normalize(Vec3{1.15f,.55f,-1.15f})*grassFixtureCameraDistance;
            grassFixtureState.cameraDistance=Length(camera.position-camera.target);
        }
        if(captureFernOverview && !capture.empty()) {
            const auto plant=std::min_element(understory.ferns.begin(),understory.ferns.end(),[&](const auto& a,const auto& b){
                return Length(a.position-session.feet)<Length(b.position-session.feet);
            });
            if(plant==understory.ferns.end())throw std::runtime_error("Fern detail fixture missing");
            camera.target=plant->position+Vec3{0,.2f,0};
            camera.position=camera.target+Vec3{.95f,.55f,-1.2f};
        }
        lastRenderedCamera=camera;
        auto& draw=e.Draw();
        draw.SetEnvironmentTime(waterTime);
        draw.SetWaterSurface(Mistwatch::RiverSurface());
        draw.SetWaterOptics({true,.018f,.35f,.035f});
        draw.SetScreenSpaceReflections({true,.7f,32.f,.65f,.28f,12});
        if(diagnosticNoAo) draw.SetAmbientOcclusion({false});
        if(diagnosticNoShadows)draw.SetDirectionalShadowMapEnabled(false);
        if(diagnosticNoBloom)draw.SetBloom({false});
        draw.SetDirectionalLight({{.4f,-.85f,.3f},.55f,{1.f,.95f,.85f,1},1.5f});
        draw.SetExposureCompensation(.5f);
        draw.SetAtmosphere({{.42f,.53f,.58f,.65f},camera.position,45,200,true,.0025f,.09f,-2.f,.76f});
        const auto sceneStats=scene.Submit(draw,camera,{1280,720});
        if(captureGrassDistance) {
            grassFixtureState.sceneSerial=sceneStats.submissionSerial;
            for(const auto& item:draw.meshInstances)if(item.identity==RenderObjectIdentity3D{{5572150615523553875ull,grassFixtureIndex+1},1}){
                if(item.layer==MeshRenderLayer3D::Opaque)++grassFixtureState.opaque;
                else if(item.layer==MeshRenderLayer3D::ShadowOnly)++grassFixtureState.shadowOnly;
                grassFixtureState.triangles+=Mistwatch::GrassTriangleCounts[grassFixtureIndex%5];
            }
            auto hash=1469598103934665603ull;
            const auto& matrix=draw.ViewProjection();
            for(const auto byte:std::as_bytes(std::span{&matrix,1})){hash^=std::to_integer<std::uint8_t>(byte);hash*=1099511628211ull;}
            grassFixtureState.cameraHash=hash;
        }
        if(!session.guardianDefeated) {
            const auto animalPalette=AddSkinPaletteFromPose(draw,boar.model,boar.pose,boarSkinning);
            if(!draw.SkinnedMeshInstance(boarMesh.handle,animalPalette,CreateTransformMatrix(encounter.feet,{0,boar.yaw,0},{1,1,1})))
                throw std::runtime_error("Boar skin submission failed");
        }
        const auto palette=AddSkinPaletteFromPose(draw,explorer.model,explorer.pose,explorerSkinning);
        if(!draw.SkinnedMeshInstance(explorerMesh.handle,palette,CreateTransformMatrix(session.feet,{0,explorer.yaw,0},{1,1,1})))
            throw std::runtime_error("Explorer skin submission failed");
        draw.Quad({640,38},{1280,76},{.02f,.04f,.04f,.9f});
        constexpr const char* objectives[]{"Find three supply crates","Find shelter tools and two timber caches [E]","Repair the bridge [E]","Evade wild boar strikes [Q], then counter [F]","Investigate north ranger hut, unlock west workshop cabinet","Repair the east generator [E]","Restore three ridge radio relays [1 / 2 / E]","Light the rescue beacon [E]","Expedition complete - R for a new expedition"};
        draw.Text({20,12},"MISTWATCH - playable blockout",2.5f,{.8f,.95f,.85f,1});
        draw.Text({20,42},!started?"ENTER: begin expedition":paused?"PAUSED - ESC to resume | M mute, PgUp/PgDn volume":!session.Alive()?"Expedition failed - R to retry":objectives[unsigned(session.CurrentStage())],2.f,{1,1,1,1});
        if(!started||paused)draw.Text({20,90},std::string(e.Audio().GetStats().deviceAvailable?"AUDIO ":"AUDIO DEVICE UNAVAILABLE | ")+
            (gameAudio.options.muted?"MUTED":std::to_string(unsigned(gameAudio.options.volume*100+.5f))+"%")+" | M mute | PgUp/PgDn volume",1.5f,{1,1,1,1});
        const unsigned supplies=unsigned(bool(session.collected&1))+unsigned(bool(session.collected&2))+unsigned(bool(session.collected&4));
        draw.Quad({640,618},{1280,42},{.02f,.04f,.04f,.85f});
        std::string context="Supplies "+std::to_string(supplies)+" / 3";
        if(session.CurrentStage()==Mistwatch::Stage::Supplies) {
            for(unsigned i=0;i<3;++i) if(!(session.collected&(1u<<i)) && session.Near(Mistwatch::Session::supplies[i])) context+=" | E: collect nearby supplies";
        } else if(session.CurrentStage()==Mistwatch::Stage::RepairMaterials) {
            context="Repair kit: ";
            context+=(session.repairMaterials&1)?"tools OK":"tools missing";
            const unsigned timber=unsigned(bool(session.repairMaterials&2))+unsigned(bool(session.repairMaterials&4));
            context+=" | Timber "+std::to_string(timber)+" / 2";
            unsigned next=0;float nearest=10000.f;
            for(unsigned site=0;site<3;++site)if(!(session.repairMaterials&(1u<<site))) {
                const auto point=Mistwatch::Session::repairSites[site];
                const auto distance=std::hypot(point.x-session.feet.x,point.z-session.feet.z);
                if(distance<nearest){nearest=distance;next=site;}
            }
            constexpr const char* names[]{"west shelter tools - CTRL crouch inside","south-east timber","west-bank timber"};
            draw.Quad({640,565},{1280,42},{.02f,.04f,.04f,.85f});
            draw.Text({20,555},std::string("Nearest: ")+names[next]+" | "+std::to_string(unsigned(nearest))+" m | E: collect",1.5f,{.7f,1,.7f,1});
        }
        else if(session.CurrentStage()==Mistwatch::Stage::Bridge && session.Near(Mistwatch::Session::bridge)) context+=" | Tools and timber ready | E: repair bridge";
        else if(session.CurrentStage()==Mistwatch::Stage::Encounter) {
            context+=" | Wild boar HP "+std::to_string(encounter.health)+" / 3";
            context+=encounter.phase==Mistwatch::EnemyPhase::Recovery?" | F: counter once":" | Wait for a counter opening";
            context+=evasion.cooldown>0?" | Q recharging":" | Q ready";
        }
        else if(session.CurrentStage()==Mistwatch::Stage::Fuse) {
            const auto destination=session.fuseClueRead?Mistwatch::Session::fuse:Mistwatch::Session::rangerLog;
            const auto range=std::hypot(destination.x-session.feet.x,destination.z-session.feet.z);
            context=session.fuseClueRead?"Ranger log: cabinet code 2 - 0 - 3 | West workshop":"Find north ranger hut | Enter west doorway, E: read log";
            context+=" | "+std::to_string(unsigned(range))+" m";
            if(session.NearInterior(Mistwatch::Session::fuse)) {
                draw.Quad({640,565},{1280,42},{.02f,.04f,.04f,.85f});
                const std::string dials="CABINET: [1] "+std::to_string(session.FuseDial(0))+"  [2] "+
                    std::to_string(session.FuseDial(1))+"  [3] "+std::to_string(session.FuseDial(2))+" | 1 / 2 / 3 turn, E: open";
                draw.Text({20,555},dials,1.5f,{.7f,1,.7f,1});
            }
            if(session.NearInterior(Mistwatch::Session::rangerLog)) {
                draw.Quad({640,565},{1280,42},{.02f,.04f,.04f,.85f});
                draw.Text({20,555},session.fuseClueRead?"RANGER LOG: replacement fuse cabinet, west workshop: 2 - 0 - 3":"RANGER LOG on desk | E: examine and keep cabinet instructions",1.5f,{.7f,1,.7f,1});
            }
        }
        else if(session.CurrentStage()==Mistwatch::Stage::Generator && session.Near(Mistwatch::Session::generator)) {
            context="Panel: 1 toggles A+B | 2 toggles B+C | 3 toggles C | Light all, then E";
            draw.Quad({640,565},{1280,42},{.02f,.04f,.04f,.85f});
            std::string lamps="CIRCUITS: ";
            for(unsigned lamp=0;lamp<3;++lamp) lamps+=(session.circuit&(1u<<lamp))?"[ON] ":"[OFF] ";
            draw.Text({20,555},lamps,2.f,{.7f,1,.7f,1});
        }
        else if(session.CurrentStage()==Mistwatch::Stage::Relays) {
            unsigned next=0,connected=0;float nearest=10000.f;
            for(unsigned index=0;index<3;++index) {
                if(session.relayLinked&(1u<<index)){++connected;continue;}
                const auto p=Mistwatch::Session::relaySites[index];
                const float range=std::hypot(p.x-session.feet.x,p.z-session.feet.z);
                if(range<nearest){nearest=range;next=index;}
            }
            constexpr const char* names[]{"west ridge","east lookout","north-west ridge"};
            context="Radio links "+std::to_string(connected)+" / 3 | "+names[next]+" | "+std::to_string(unsigned(nearest))+" m";
            draw.Quad({640,565},{1280,42},{.02f,.04f,.04f,.85f});
            std::string radio="Follow the uphill dirt branch to the relay mast";
            if(session.NearRelay(next))radio="ANTENNA "+std::to_string(unsigned(float(session.RelaySector(next))*22.5f))+(session.RelaySector(next)%2?".5":"")+
                " deg | SIGNAL "+std::to_string(session.RelaySignal(next))+" | 1 left / 2 right | "+
                (session.RelaySector(next)==Mistwatch::Session::RelayTargetSector(next)?"GREEN LOCK - E: link":"Weak signal - aim toward rescue mast");
            draw.Text({20,555},radio,1.5f,{.7f,1,.7f,1});
        }
        else if(session.CurrentStage()==Mistwatch::Stage::Beacon)
            context=session.Near(Mistwatch::Session::beacon)?"Power restored | E: activate rescue beacon":"Power restored | Follow the trail north to the rescue beacon";
        else if(session.CurrentStage()==Mistwatch::Stage::Complete)
            context="Rescue signal active | Expedition complete | R: start again";
        draw.Text({20,608},context,2.f,{1,.9f,.65f,1});
        draw.Text({20,655},"WASD move | SHIFT run | SPACE jump | CTRL crouch | Q evade | E use | F counter | Drag / wheel camera",1.5f,{1,1,1,1});
        draw.Text({20,682},"F5 save / F9 load | HP "+std::to_string(session.health)+" | "+message,1.5f,{1,1,1,1});
        if(!capture.empty() && !capturePending && ++captureFrames>=45 &&
           (!captureAudioProbe||captureAudioTicks>=180) &&
           (!captureBenchmark || benchmarkGpuMilliseconds.size()==120)) {
            if(diagnosticHdrStage&&(captureCompleted==18||captureCompleted==27)) {
                const auto hash=[](const auto& value) {
                    std::uint64_t result=1469598103934665603ull;
                    const auto bytes=std::as_bytes(std::span{&value,1});
                    for(const auto byte:bytes){result^=std::to_integer<std::uint8_t>(byte);result*=1099511628211ull;}
                    return result;
                };
                const auto cameraHash=hash(draw.ViewProjection());
                const std::array<std::uint64_t,3> state{hash(waterTime),std::uint64_t(session.CurrentStage()),session.repairMaterials};
                if(!e.RequestLinearHdrReadback3D({captureFrames,cameraHash,hash(state)}))throw std::runtime_error("HDR diagnostic request rejected");
            }
            if(captureGrassDistance)grassFixtureFrozen=grassFixtureState;
            capturePending=e.CaptureNextFrame3D(CapturePath());
            if(!capturePending)throw std::runtime_error("Frame capture request rejected");
        }
    }
    void OnStop(Engine& e) override {
        StopGameAudio(e);scene.Clear();scene.SetBoxMesh({});
        unsigned suppliesDestroyed{};
        for(auto& mesh:supplyMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++suppliesDestroyed;}mesh={};}
        if(suppliesDestroyed!=2)throw std::runtime_error("Supply crate cleanup incomplete");
        if(!capture.empty()) {
            auto path=capture;path+=".supply-cleanup.txt";
            std::ofstream out(path);out<<"mesh_destroy_calls=2\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Supply cleanup receipt failed");
        }
        e.DestroySkinnedMesh3D(explorerMesh.handle);e.DestroySkinnedMesh3D(boarMesh.handle);
        materials.Shutdown();forestMaterials.Shutdown();broadleafMaterials.Shutdown();boulderMaterials.Shutdown();
        fernMaterials.Shutdown();
        grassMaterials.Shutdown();
        unsigned grassDestroyed{};
        for(auto& mesh:scannedGrass){if(mesh){e.DestroyMesh3D(mesh.handle);++grassDestroyed;}mesh={};}
        if(grassMaterials.Stats().materials||grassDestroyed!=5)throw std::runtime_error("Scanned grass cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".grass-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls=5\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Grass cleanup receipt failed");
        }
        unsigned fernDestroyed{};
        for(auto& mesh:scannedFerns){if(mesh){e.DestroyMesh3D(mesh.handle);++fernDestroyed;}mesh={};}
        unsigned broadleafDestroyed{};
        for(auto& mesh:broadleafMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++broadleafDestroyed;}mesh={};}
        if(broadleafMaterials.Stats().materials||broadleafDestroyed!=3)throw std::runtime_error("Broadleaf cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".broadleaf-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls=3\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Broadleaf cleanup receipt failed");
        }
        e.DestroyMesh3D(box.handle);for(auto& mesh:firMeshes)e.DestroyMesh3D(mesh.handle);
        for(auto& mesh:landscapeFirMeshes)e.DestroyMesh3D(mesh.handle);
        unsigned propsDestroyed{};
        for(auto& mesh:propMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++propsDestroyed;}mesh={};}
        if(boulderMaterials.Stats().materials||propsDestroyed!=2)throw std::runtime_error("Forest prop cleanup incomplete");
        if(!capture.empty()) {
            auto path=capture;path+=".boulder-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls=2\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Boulder cleanup receipt failed");
        }
        e.DestroyMesh3D(terrainMesh.handle);e.DestroyMesh3D(landscapeMesh.handle);e.DestroyMesh3D(riverMesh.handle);
        e.DestroyMesh3D(grassMesh.handle);e.DestroyMesh3D(fernMesh.handle);e.DestroyMesh3D(relayDish.handle);
        // 新蕨株檢查放在其餘正常釋放之後；receipt 不宣稱全引擎 GPU 零洩漏。
        if(fernMaterials.Stats().materials||fernDestroyed!=4)throw std::runtime_error("Scanned fern cleanup incomplete");
        if(!capture.empty()) {
            auto path=capture;path+=".fern-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls=4\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Fern cleanup receipt failed");
        }
    }
};
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    try {
        Engine engine({L"Aevrynd - Mistwatch",1280,720});
        return engine.Run(std::make_unique<MistwatchGame>(),instance,show);
    } catch(const std::exception& error) {
        std::ofstream log("mistwatch-startup-error.txt");
        log<<error.what()<<'\n';
        return 1;
    }
}
