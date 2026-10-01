#include "MistwatchSession.h"
#include "MistwatchWorld.h"
#include "MistwatchEncounter.h"
#include "MistwatchCamera.h"
#include "MistwatchExplorer.h"
#include "MistwatchForestMaterial.h"
#include "MistwatchEvasion.h"
#include "MistwatchRiver.h"
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

using namespace Aevrynd;
class MistwatchGame final : public IGame {
    Mistwatch::Session session;
    CharacterController3D controller;
    Scene3D scene;
    MeshResource3D box;
    std::array<MeshResource3D,4> firMeshes;
    std::array<Material3D,4> firMaterials;
    MaterialLibrary3D forestMaterials;
    HeightField3D terrain;
    MeshResource3D terrainMesh;
    MeshResource3D riverMesh;
    float waterTime{};
    std::vector<Vec3> trees;
    std::vector<Vec3> groundCover;
    MeshResource3D grassMesh;
    MeshResource3D fernMesh;
    Mistwatch::ForestUnderstory understory;
    unsigned submittedFerns{},submittedSaplings{};
    std::array<MeshResource3D,4> propMeshes;
    std::vector<Mistwatch::ForestProp> forestProps;
    MaterialLibrary3D materials;
    Material3D groundMaterial, barkMaterial, woodMaterial, rockMaterial;
    std::vector<BoxCollider3D> solids;
    Mistwatch::OrbitCamera orbit;
    Mistwatch::Encounter encounter;
    Mistwatch::Evasion evasion;
    Mistwatch::Explorer explorer;
    SkinnedMeshResource3D explorerMesh;
    SkinningWorkspace3D explorerSkinning;
    bool started{}, paused{};
    std::string message="ENTER: begin expedition";
    std::filesystem::path save;
    std::filesystem::path capture;
    unsigned captureFrames{};
    unsigned captureSequenceFrames{},captureCompleted{};
    bool capturePending{};
    bool diagnosticNoAo{};
    bool diagnosticCrown{};
    bool captureWalk{},captureCrouch{};
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
    }
    std::filesystem::path CapturePath()const {
        if(!captureSequenceFrames)return capture;
        const auto ordinal=std::to_wstring(captureCompleted);
        return capture.parent_path()/(capture.stem().wstring()+L"-"+std::wstring(3-ordinal.size(),L'0')+ordinal+L".png");
    }
public:
    void OnStart(Engine& e) override {
        if(!explorer.model.Valid())throw std::runtime_error("Explorer model invalid");
        explorerMesh=e.CreateSkinnedMeshResource3D(explorer.model);
        if(!explorerMesh)throw std::runtime_error("Explorer GPU upload failed");
        box=e.CreateMeshResource3D(Mesh3D::CreateUnitBox());
        scene.SetBoxMesh(box);
        materials.Initialize(e,e.ExecutableDirectory()/"assets"/"mistwatch-materials");
        const std::array<MaterialAssetDefinition3D,4> definitions{{
            {"ground",{"ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",1024,"ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.92f,0,0,.8f},false,{},.25f}},
            {"bark",{"poly-haven/bark_brown_02/1k/base-color.jpg","poly-haven/bark_brown_02/1k/roughness.jpg",1024,"poly-haven/bark_brown_02/1k/normal-dx.jpg"},{{1,1,1,1},{.85f,0,0,.8f},true,{},1.f}},
            {"wood",{"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",1024,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.8f,0,0,.8f},true,{},.5f}},
            {"rock",{"poly-haven/rock_surface/1k/base-color.jpg","poly-haven/rock_surface/1k/roughness.jpg",1024,"poly-haven/rock_surface/1k/normal-dx.jpg"},{{1,1,1,1},{.9f,0,0,.85f},true,{},1.f}}
        }};
        if(!materials.LoadAll(definitions)) throw std::runtime_error("Mistwatch CC0 materials unavailable");
        groundMaterial=*materials.Find("ground");
        barkMaterial=*materials.Find("bark");
        woodMaterial=*materials.Find("wood");
        rockMaterial=*materials.Find("rock");
        groundMaterial.normalScale=.65f;
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
        }
        terrain=Mistwatch::CreateTerrain();
        terrainMesh=e.CreateMeshResource3D(Mistwatch::CreateTerrainMesh(terrain));
        riverMesh=e.CreateMeshResource3D(Mistwatch::CreateRiverMesh(terrain));
        if(!riverMesh)throw std::runtime_error("River GPU upload failed");
        trees=Mistwatch::TreePositions();
        forestProps=Mistwatch::ForestProps(terrain,trees);
        understory=Mistwatch::CreateUnderstory(terrain,trees,forestProps);
        fernMesh=e.CreateMeshResource3D(CreateFernMesh3D(EnvironmentMeshDetail3D::Near,17));
        if(!fernMesh)throw std::runtime_error("Fern GPU upload failed");
        for(unsigned mesh=0;mesh<propMeshes.size();++mesh) {
            propMeshes[mesh]=e.CreateMeshResource3D(Mistwatch::ForestPropMeshes()[mesh]);
            if(!propMeshes[mesh])throw std::runtime_error("Forest prop GPU upload failed");
        }
        groundCover=Mistwatch::GroundCoverPositions(terrain);
        grassMesh=e.CreateMeshResource3D(CreateGrassClumpMesh3D(9,3));
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
                else if(name==L"strike") fixture="MISTWATCH 3 7 100 1 0 0 0 0 46.5 0 0 0";
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
                else if(name==L"walk" || name==L"crouch") {captureWalk=true;captureCrouch=name==L"crouch";}
                else throw std::runtime_error("Unknown capture scenario");
                if(!fixture.empty() && !session.Restore(fixture)) throw std::runtime_error("Invalid capture fixture");
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
                if(name==L"strike") {
                    encounter.Tick(session.feet,1.f/60.f,false);
                    for(unsigned tick=0;tick<4;++tick)encounter.Tick(session.feet,.1f,false);
                }
            }
        }
        wchar_t noAo[8]{};
        diagnosticNoAo=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_NO_AO",noAo,8)>0 && noAo[0]==L'1';
        wchar_t crown[8]{};
        diagnosticCrown=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CROWN",crown,8)>0 && crown[0]==L'1';
    }
    void OnUpdate(Engine& e,float dt) override {
        if(!capture.empty()) {
            if(captureWalk && !capturePending){
                const auto previous=controller.Position();
                controller.TrySetHeight(captureCrouch?1.4f:1.8f,solids);
                controller.Step({{0,0,1},false,captureCrouch?.45f:1.f},solids,terrain,1.f/60.f);
                session.feet=controller.Position();
                explorer.Update(session.feet-previous,1.f/60.f,captureCrouch);
                if(!explorer.PlaceFeet(session.feet,controller.Grounded(),captureCrouch,terrain,solids))
                    throw std::runtime_error("Capture locomotion IK failed");
            }
            const auto stats=e.GetRendererStats();
            // 排除啟動前六十幀；僅採用已完成且序號不重複的 GPU timestamp 收據。
            const auto& timing=stats.lastCompletedGpuTimingReceipt;
            if(captureBenchmark && timing.serial>60 && timing.serial>benchmarkLastSerial &&
               timing.Complete() && benchmarkGpuMilliseconds.size()<120) {
                benchmarkLastSerial=timing.serial;
                benchmarkGpuMilliseconds.push_back(double(timing.nanoseconds)/1000000.0);
            }
            if(capturePending && stats.rejectedFrameCaptures)throw std::runtime_error("Frame capture rejected");
            if(capturePending && stats.successfulFrameCaptures>captureCompleted) {
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
                out<<"camera_source="<<((diagnosticCrown||captureRiverOverview||captureShelterOverview)?"diagnostic":"player")
                    <<"\ncamera_distance="<<Length(lastRenderedCamera.position-lastRenderedCamera.target)
                    <<"\ncamera_height="<<lastRenderedCamera.position.y
                    <<"\ncamera_anchor_above_feet="<<lastRenderedCamera.target.y-session.feet.y<<'\n';
                if(captureBenchmark) {
                    auto ordered=benchmarkGpuMilliseconds;std::sort(ordered.begin(),ordered.end());
                    out<<"benchmark_mode=fixed_scene_gpu_only\ngpu_samples="<<ordered.size()
                        <<"\ngpu_mean_ms="<<std::accumulate(ordered.begin(),ordered.end(),0.0)/double(ordered.size())
                        <<"\ngpu_p50_ms="<<ordered[59]<<"\ngpu_p95_ms="<<ordered[113]
                        <<"\ngpu_max_ms="<<ordered.back()<<'\n';
                }
                out.close();if(!out)throw std::runtime_error("Capture receipt write failed");
                ++captureCompleted;
                if(!captureSequenceFrames || captureCompleted>=captureSequenceFrames)e.Quit();
                else {capturePending=false;waterTime=2.5f+float(captureCompleted)*.1f;}
            }
            // GPU 驗證會插入額外 shader 檢查；診斷等待不作為正式效能數據。
            const auto captureTimeout=std::chrono::seconds(stats.gpuValidationRequested?120:30);
            if(std::chrono::steady_clock::now()-captureStart>captureTimeout)
                throw std::runtime_error("Frame capture timeout; rendered="+std::to_string(captureFrames)+
                    "; submitted="+std::to_string(stats.submittedFrames)+"; gpu-validation="+std::to_string(stats.gpuValidationRequested));
            return;
        }
        auto& in=e.GetInput();
        if(in.Pressed(VK_ESCAPE)) paused=!paused;
        if(in.Pressed(VK_RETURN)) started=true;
        if(!started || paused) return;
        if(std::isfinite(dt) && dt>0)waterTime=std::fmod(waterTime+(std::min)(dt,.1f),3600.f);
        if(in.Pressed(VK_F9) && !save.empty()) {
            std::ifstream file(save,std::ios::binary);
            if(session.RestoreStream(file)){solids=Mistwatch::WorldSolids(session.bridgeRepaired);ResetController();encounter={};evasion={};message="Loaded";}else message="Save unavailable";
        }
        if(in.Pressed('R') && session.beaconLit) {
            session={};controller.Reset({});solids=Mistwatch::WorldSolids(false);encounter={};evasion={};orbit={};message="New expedition";
        }
        if(in.Pressed('R') && !session.Alive()) {session.Retry();ResetController();encounter={};evasion={};}
        if(!session.Alive() || session.beaconLit) return;
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
        if(session.feet.y < -5.f) {session.Damage(100);message="Fell into the ravine - R to retry";return;}
        for(unsigned lever=0;lever<3;++lever)if(in.Pressed('1'+lever)) {
            if(session.CycleFuseDial(lever))message="Cabinet dial turned - match the ranger log, then E";
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
            changed=session.LightBeacon()||changed;
            message=changed?"Objective updated":session.CurrentStage()==Mistwatch::Stage::RepairMaterials?
                "Find tools in west shelter (CTRL), and both timber caches":
                session.CurrentStage()==Mistwatch::Stage::Fuse && session.NearInterior(Mistwatch::Session::fuse)?
                "Cabinet locked - read ranger log, set dials 1 / 2 / 3":"Move closer to the objective";
        }
        if(session.CurrentStage()==Mistwatch::Stage::Encounter) {
            session.Damage(encounter.Tick(session.feet,dt,in.Pressed('F') && !motion.dashing));
            if(!encounter.health) session.FinishEncounter(encounter.feet);
            if(encounter.phase==Mistwatch::EnemyPhase::Windup) message="Leave the marked circle - Q: evade";
            else if(encounter.phase==Mistwatch::EnemyPhase::Recovery) message="Guardian recovering - strike with F";
        }
        if(in.Pressed(VK_F5) && !save.empty()) {
            std::error_code error;
            std::filesystem::create_directories(save.parent_path(),error);
            auto temporary=save; temporary+=".tmp";
            std::ofstream out(temporary); out<<session.Serialize(); out.close();
            message=out && MoveFileExW(temporary.c_str(),save.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?"Saved":"Save failed";
        }
    }
    void OnRender(Engine& e,float) override {
        scene.Clear();
        scene.Mesh(terrainMesh,{},groundMaterial);
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
        submittedFerns=0;
        for(auto plant:understory.ferns) {
            const float visibility=Mistwatch::UnderstoryVisibility(Length(plant.position-session.feet));
            if(visibility<=0)continue;
            plant.scale=plant.scale*visibility;
            scene.Mesh(fernMesh,plant,fern);
            ++submittedFerns;
        }
        Material3D grass{{.32f,.43f,.18f,1},{.9f,0,0,.9f}};
        grass.materialClass=MaterialClass3D::Foliage;
        grass.doubleSided=true;
        for(const auto plant:groundCover) {
            if(Length(plant-session.feet)>35.f) continue;
            const float scale=.5f+.25f*std::sin(plant.x*1.7f+plant.z);
            scene.Mesh(grassMesh,{plant,{0,plant.x+plant.z,0},{scale,scale,scale}},grass);
        }
        for(const auto& prop:forestProps)scene.Mesh(propMeshes[prop.mesh],prop.transform,prop.FallenLog()?barkMaterial:rockMaterial);
        for(unsigned i=0;i<3;++i) if(!(session.collected&(1u<<i)))
            scene.Box(Mistwatch::Session::supplies[i]+Vec3{0,.4f,0},{.8f,.8f,.8f},{.7f,.45f,.12f,1});
        if(session.bridgeRepaired) scene.MaterialBox({0,-.1f,36},{4,.2f,8},woodMaterial);
        else scene.Box(Mistwatch::Session::bridge+Vec3{1,.35f,0},{1,.7f,1},{.3f,.12f,.08f,1});
        for(const auto solid:Mistwatch::RepairShelterSolids())scene.MaterialBox(solid.center,solid.extents*2.f,woodMaterial);
        for(unsigned site=0;site<3;++site) {
            const auto point=Mistwatch::Session::repairSites[site];
            if(site==0)scene.Box(point+Vec3{0,.49f,0},{.5f,.06f,.3f},(session.repairMaterials&1)?Color{.3f,.3f,.25f,1}:Color{.82f,.65f,.15f,1});
            if(session.repairMaterials&(1u<<site))continue;
            if(site!=0)for(unsigned beam=0;beam<3;++beam)
                scene.Mesh(propMeshes[3],{point+Vec3{0,.18f+.22f*float(beam),0},{0,0,1.5707963f},{.22f,1.8f,.22f}},woodMaterial);
            // 小型路標讓材料位置可辨識，不用懸浮的大型目標方塊遮住場景。
            const auto sign=point+Vec3{site==0?2.8f:1.3f,0,0};
            scene.MaterialBox(sign+Vec3{0,.6f,0},{.12f,1.2f,.12f},woodMaterial);
            scene.Box(sign+Vec3{0,1.12f,0},{.7f,.34f,.1f},{.78f,.58f,.15f,1});
        }
        if(!session.guardianDefeated) scene.Box(encounter.feet+Vec3{0,.8f,0},{1,1.6f,1},
            encounter.phase==Mistwatch::EnemyPhase::Windup?Color{1,.6f,.1f,1}:Color{.5f,.12f,.08f,1});
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
        auto camera=Mistwatch::BuildPlayerCamera(session.feet,controller.Settings().height,orbit,solids,terrain);
        if(diagnosticCrown && !capture.empty()) camera=MakeThirdPersonCamera({-7,6,-5},0,.1f,8);
        // 遠景只用於地形影像驗收，不改變玩家可操作鏡頭的距離上限。
        if(captureRiverOverview && !capture.empty()) {camera.position={-22,28,8};camera.target={0,-2,36};}
        if(captureShelterOverview && !capture.empty()) {camera.position={-3,1.1f,17};camera.target={-7.25f,.7f,18};}
        lastRenderedCamera=camera;
        auto& draw=e.Draw();
        draw.SetEnvironmentTime(waterTime);
        draw.SetWaterSurface(Mistwatch::RiverSurface());
        draw.SetWaterOptics({true,.018f,.35f,.035f});
        draw.SetScreenSpaceReflections({true,.7f,32.f,.65f,.28f,12});
        if(diagnosticNoAo) draw.SetAmbientOcclusion({false});
        draw.SetDirectionalLight({{.4f,-.85f,.3f},.55f,{1.f,.95f,.85f,1},1.5f});
        draw.SetExposureCompensation(.5f);
        draw.SetAtmosphere({{.42f,.53f,.58f,.65f},camera.position,35,110,true,.0035f,.09f,-2.f,.76f});
        scene.Submit(draw,camera,{1280,720});
        const auto palette=AddSkinPaletteFromPose(draw,explorer.model,explorer.pose,explorerSkinning);
        if(!draw.SkinnedMeshInstance(explorerMesh.handle,palette,CreateTransformMatrix(session.feet,{0,explorer.yaw,0},{1,1,1})))
            throw std::runtime_error("Explorer skin submission failed");
        draw.Quad({640,38},{1280,76},{.02f,.04f,.04f,.9f});
        constexpr const char* objectives[]{"Find three supply crates","Find shelter tools and two timber caches [E]","Repair the bridge [E]","Evade guardian strikes [Q], then counter [F]","Investigate north ranger hut, unlock west workshop cabinet","Repair the east generator [E]","Light the rescue beacon [E]","Expedition complete - R for a new expedition"};
        draw.Text({20,12},"MISTWATCH - playable blockout",2.5f,{.8f,.95f,.85f,1});
        draw.Text({20,42},!started?"ENTER: begin expedition":paused?"PAUSED - ESC to resume":!session.Alive()?"Expedition failed - R to retry":objectives[unsigned(session.CurrentStage())],2.f,{1,1,1,1});
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
            context+=" | Guardian HP "+std::to_string(encounter.health)+" / 3";
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
        else if(session.CurrentStage()==Mistwatch::Stage::Beacon)
            context=session.Near(Mistwatch::Session::beacon)?"Power restored | E: activate rescue beacon":"Power restored | Follow the trail north to the rescue beacon";
        else if(session.CurrentStage()==Mistwatch::Stage::Complete)
            context="Rescue signal active | Expedition complete | R: start again";
        draw.Text({20,608},context,2.f,{1,.9f,.65f,1});
        draw.Text({20,655},"WASD move | SHIFT run | SPACE jump | CTRL crouch | Q evade | E use | F counter | Drag / wheel camera",1.5f,{1,1,1,1});
        draw.Text({20,682},"F5 save / F9 load | HP "+std::to_string(session.health)+" | "+message,1.5f,{1,1,1,1});
        if(!capture.empty() && !capturePending && ++captureFrames>=45 &&
           (!captureBenchmark || benchmarkGpuMilliseconds.size()==120))
            capturePending=e.CaptureNextFrame3D(CapturePath());
    }
    void OnStop(Engine& e) override {scene.Clear();scene.SetBoxMesh({});e.DestroySkinnedMesh3D(explorerMesh.handle);materials.Shutdown();forestMaterials.Shutdown();e.DestroyMesh3D(box.handle);for(auto& mesh:firMeshes)e.DestroyMesh3D(mesh.handle);for(auto& mesh:propMeshes)e.DestroyMesh3D(mesh.handle);e.DestroyMesh3D(terrainMesh.handle);e.DestroyMesh3D(riverMesh.handle);e.DestroyMesh3D(grassMesh.handle);}
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
