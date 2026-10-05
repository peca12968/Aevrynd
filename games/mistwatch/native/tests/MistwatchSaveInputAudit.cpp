#define MISTWATCH_SAVE_INPUT_AUDIT 1
#define wWinMain MistwatchProductionEntryUnused
#include "../src/main.cpp"
#undef wWinMain
#include "MistwatchSaveGame.h"
#include "MistwatchLoadPreparation.h"
#include <shellapi.h>
#include <span>
#include <type_traits>

namespace {
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct AuditHash {
    std::uint64_t value=1469598103934665603ull;
    template<class T> void Add(const T& object){
        static_assert(std::is_trivially_copyable_v<T>);
        for(const auto byte:std::as_bytes(std::span{&object,1})){value^=std::to_integer<unsigned char>(byte);value*=1099511628211ull;}
    }
    template<class T> void Vector(const std::vector<T>& items){Add(items.size());for(const auto& item:items)Add(item);}
    void Text(std::string_view text){for(char byte:text){value^=static_cast<unsigned char>(byte);value*=1099511628211ull;}}
    void Pose(const AnimationPose3D& pose){Vector(pose.localTransforms);Vector(pose.globalMatrices);}
    void Model(const SkinnedModel3D& model){Vector(model.bindMesh.vertices);Vector(model.bindMesh.indices);Vector(model.skeleton.joints);
        Vector(model.influences);Vector(model.inverseBindMatrices);Vector(model.tangents);Add(model.skeletonCompatibilityId);}
};
std::string Read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);Require(bool(in),"isolated evidence file unavailable");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
void Write(const std::filesystem::path& path,std::string_view text){std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(text.data(),static_cast<std::streamsize>(text.size()));out.close();Require(bool(out),"isolated fixture write failed");}
Mistwatch::Session BridgeSession(){
    Mistwatch::Session s;
    // 僅建立可驗證的規則前置狀態，不宣稱玩家按鍵通關。
    for(unsigned i=0;i<3;++i){s.feet=Mistwatch::Session::supplies[i];Require(s.Collect(i),"seed supply failed");}
    for(unsigned i=0;i<3;++i){s.feet=Mistwatch::Session::repairSites[i];Require(s.CollectRepairMaterial(i,i==0),"seed repair material failed");}
    s.feet=Mistwatch::Session::bridge;Require(s.RepairBridge(),"seed bridge failed");s.feet=Mistwatch::Session::arena;return s;
}
Mistwatch::SaveGame BattleSeed(){
    Mistwatch::SaveGame s;s.session=BridgeSession();
    for(unsigned hit=0;hit<2;++hit){
        unsigned ticks{};
        while(s.encounter.phase!=Mistwatch::EnemyPhase::Recovery&&ticks++<300)s.session.Damage(s.encounter.Tick(s.session.feet,.02f,false));
        Require(s.encounter.phase==Mistwatch::EnemyPhase::Recovery,"seed counter window missing");
        s.session.Damage(s.encounter.Tick(s.session.feet,.02f,true));
        if(!hit){ticks=0;while(s.encounter.phase==Mistwatch::EnemyPhase::Recovery&&ticks++<300)
            s.session.Damage(s.encounter.Tick(s.session.feet,.02f,false));}
    }
    Require(s.encounter.health==1&&s.Valid(),"seed HP1 invalid");
    s.evasion.Tick(.1f,true,true,false,{1,0,.5f});s.evasion.Tick(.03f,false,true,false,{-1,0,0});
    Require(s.evasion.remaining>0&&s.evasion.cooldown>0&&s.Valid(),"bounded active evasion seed invalid");return s;
}
bool KnownCase(std::wstring_view value){
    for(const auto name:{L"enemy-slope-pursuit",L"enemy-slope-windup",L"enemy-slope-recovery"})if(value==name)return true;
    for(const auto name:{L"hp1-active",L"cooldown-only",L"legacy-v6",L"truncated",L"missing",L"body-overlap",L"middle-overlap",L"dead",L"complete",L"shelter",L"slope",L"terrain-edge",L"outside-save",L"retry-battle",L"retry-after-battle",L"enemy-y-below",L"enemy-y-above"})
        if(value==name)return true;
    return false;
}
}

// 友元只存在於此稽核翻譯單元；正式遊戲不包含輸入稽核掛鉤。
class MistwatchSaveInputProbe final : public IGame {
    MistwatchGame game;
    std::wstring caseName;
    std::filesystem::path output,capturePath;
    Mistwatch::SaveGame expected;
    unsigned phase{},f5Count{},f9Count{};
    bool initialized{},stopped{},passed{},snapshotEqual{},stateUnchanged{};
    bool audioOptionsUnchanged{},masterVolumeUnchanged{},audioSettingsUnchanged{};
    bool unsafeF5Rejected{},oldSavePreserved{};
    bool expectedMiddleLayer{};
    float beforeLoadWater{},initialMasterVolume{};
    std::string initialAudioSettings;
    std::string oldSaveBytes;
    Vec3 terrainEdgeFeet{};
    unsigned terrainEdgeControllerSteps{};
    Mistwatch::SaveGame Snapshot()const{return {game.session,game.encounter,game.evasion};}
    Mistwatch::SavedGameCommitTargets Targets(){return {game.session,game.encounter,game.evasion,game.controller,game.explorer,game.boar,game.orbit,
        game.solids,game.playerCameraMotion,game.lastPlayerFraming,game.lastCameraMotion,game.lastRenderedCamera};}
    void ValidateMiddleSolids()const {
        const std::size_t expectedPopulation=expectedMiddleLayer?24:0;
        Require(game.middleLayerEnabled==expectedMiddleLayer&&game.middleLayer.trees.size()==expectedPopulation&&
            game.middleLayerSolids.size()==expectedPopulation,"native branch does not match explicitly expected middle-layer mode");
        const auto base=Mistwatch::WorldSolids(game.session.bridgeRepaired);
        Require(game.solids.size()==base.size()+game.middleLayerSolids.size(),"native branch lost middle-layer solids");
        for(std::size_t i=0;i<game.middleLayerSolids.size();++i) {
            const auto& actual=game.solids[base.size()+i];const auto& expectedSolid=game.middleLayerSolids[i];
            Require(Same(actual.center,expectedSolid.center)&&Same(actual.extents,expectedSolid.extents),"native branch altered middle-layer solid");
        }
    }
    void Seed(const Mistwatch::SaveGame& value){
        const auto result=Mistwatch::PrepareSavedGame(value,{game.controller,game.explorer,game.boar,game.orbit},game.terrain,{},game.middleLayerSolids);
        if(!result)throw std::runtime_error("native audit seed preflight failed; error="+std::to_string(unsigned(result.error)));
        Mistwatch::SwapPreparedSavedGame(*result.candidate,Targets());
        Require(Snapshot().Serialize()==value.Serialize(),"native audit seed swap lost snapshot");
    }
    std::uint64_t Fingerprint()const {
        AuditHash h;h.Text(Snapshot().Serialize());h.Add(game.controller.StateHash());h.Add(game.controller.Settings());
        h.Model(game.explorer.model);h.Pose(game.explorer.pose);h.Add(game.explorer.phase);h.Add(game.explorer.yaw);
        h.Add(game.explorer.footTargets);h.Add(game.explorer.planted);h.Add(game.explorer.lastFeet);h.Add(game.explorer.feetInitialized);h.Add(game.explorer.locomotionDistance);
        h.Model(game.boar.model);h.Pose(game.boar.pose);h.Add(game.boar.yaw);h.Add(game.boar.cycle);h.Add(game.boar.displacement);h.Add(game.boar.brace);
        h.Add(game.boar.footTargets);h.Add(game.boar.planted);h.Add(game.boar.lastFeet);h.Add(game.boar.initialized);
        h.Vector(game.solids);h.Add(game.orbit);h.Add(game.playerCameraMotion.Initialized());h.Add(game.playerCameraMotion.Camera());
        h.Add(game.lastPlayerFraming);h.Add(game.lastCameraMotion);h.Add(game.lastRenderedCamera);h.Add(game.waterTime);
        h.Add(game.cameraFixedTicks);h.Add(game.captureCameraTicks);h.Add(game.cameraMaximumTickDisplacement);h.Add(game.cameraOcclusionSeconds);
        h.Add(game.started);h.Add(game.paused);h.Add(game.gameAudio.director);h.Add(game.gameAudio.requests);h.Add(game.gameAudio.options);
        return h.value;
    }
    void UnchangedCameraContinuation(const ThirdPersonCameraMotion3D& before){
        auto a=game.playerCameraMotion,b=before;const std::array roof{Mistwatch::RangerHutCameraRoofSolid()};
        for(unsigned i=0;i<3;++i){
            auto orbit=game.orbit;orbit.yaw+=.04f*float(i+1);
            const auto request=Mistwatch::BuildPlayerCameraMotionRequest(game.session.feet,game.controller.Settings().height,orbit,a.Initialized(),game.solids,game.terrain,roof);
            const auto samples=Mistwatch::PlayerCameraSamples(game.session.feet,game.controller.Settings().height);
            const auto x=a.Update(request,samples,game.solids,Mistwatch::CameraCeilingSolids(),game.terrain,1.f/60.f,roof);
            const auto y=b.Update(request,samples,game.solids,Mistwatch::CameraCeilingSolids(),game.terrain,1.f/60.f,roof);
            Require(x.accepted==y.accepted&&x.status==y.status&&x.subjectVisible==y.subjectVisible&&x.displacement==y.displacement&&
                Same(x.camera.position,y.camera.position)&&Same(x.camera.target,y.camera.target),"failed F9 altered private camera continuation");
        }
    }
    void ValidateLoaded(){
        ValidateMiddleSolids();
        Require(Snapshot().Valid()&&Snapshot().Serialize()==expected.Serialize(),"actual F9 did not restore exact complete snapshot");
        snapshotEqual=true;
        Require(Same(game.controller.Position(),expected.session.feet),"actual F9 relocated saved feet");
        Require(game.lastCameraMotion.accepted&&game.lastCameraMotion.subjectVisible&&game.playerCameraMotion.Initialized(),"actual F9 lacks accepted camera");
        const auto camera=game.playerCameraMotion.Camera();
        Require(Same(camera.position,game.lastRenderedCamera.position)&&Same(camera.target,game.lastRenderedCamera.target),"actual F9 render camera stale");
        CollisionWorld3D independent;
        for(const auto& solid:game.solids)Require(bool(independent.CreateBox({solid})),"independent body fixture creation failed");
        Require(!independent.OverlapsBox(game.controller.Center(),game.controller.Extents()),"actual F9 body overlaps solid");
        Require(bool(independent.CreateBox({Mistwatch::RangerHutCameraRoofSolid()})),"independent roof fixture creation failed");
        const float t=std::tan(camera.projection.verticalFieldOfView*.5f),aspect=1280.f/720.f;
        const float radius=(std::max)(.16f,.005f+camera.projection.nearPlane*std::sqrt(1.f+t*t*(1.f+aspect*aspect)));
        Require(!independent.SweepSphere(camera.position,radius,{}),"actual F9 projection volume overlaps solid");
        for(const auto sample:Mistwatch::PlayerCameraSamples(game.session.feet,game.controller.Settings().height))
            Require(ProjectToViewport(sample,camera,{1280,720}).visible,"actual F9 subject sample outside viewport");
        Require(Same(game.controller.Velocity(),{}),"loaded checkpoint retained stale motion");
        if(caseName==L"shelter")Require(game.controller.Settings().height==1.4f,"loaded shelter checkpoint stands through roof");
        if(caseName==L"dead")Require(!game.session.Alive(),"F9 plus R retried dead saved session");
        if(caseName==L"complete")Require(game.session.beaconLit,"F9 plus R reset completed saved session");
        if(caseName==L"terrain-edge")Require(Same(game.session.feet,terrainEdgeFeet)&&game.terrain.Contains(game.session.feet.x,game.session.feet.z)&&
            game.session.feet.x>49.69f&&game.session.feet.x<49.71f&&game.session.feet.y==game.terrain.SampleHeight(game.session.feet.x,game.session.feet.z),
            "terrain-edge actual F9 shifted feet or rejected runtime-legal center");
        if(caseName==L"outside-save")Require(game.terrain.Contains(game.session.feet.x,game.session.feet.z)&&game.session.feet.z==99.4f&&
            game.session.feet.y==game.terrain.SampleHeight(game.session.feet.x,game.session.feet.z)&&Read(game.save)==oldSaveBytes,
            "F9 after rejected unsafe F5 did not preserve original legal checkpoint");
        if(caseName==L"legacy-v6")Require(game.encounter.health==3&&game.encounter.phase==Mistwatch::EnemyPhase::Pursuit&&
            game.encounter.phaseTime==0&&game.encounter.playerCooldown==0&&game.evasion.remaining==0&&game.evasion.cooldown==0,
            "legacy Session6 documented encounter/action defaults differ");
    }
    void Keys(Engine& engine,std::span<const unsigned> keys,bool down){
        Require(engine.Window()!=nullptr,"audit owns no native window");
        DWORD process{};GetWindowThreadProcessId(engine.Window(),&process);
        Require(process==GetCurrentProcessId(),"input target is not this process window");
        if(down)SendMessageW(engine.Window(),WM_SETFOCUS,0,0);
        for(const auto key:keys)SendMessageW(engine.Window(),down?WM_KEYDOWN:WM_KEYUP,WPARAM(key),down?LPARAM(1):LPARAM(0xC0000001));
    }
    void NativeTick(Engine& engine,std::span<const unsigned> keys,float dt){
        engine.GetInput().ConsumePresses();Keys(engine,keys,true);
        for(const auto key:keys)Require(engine.GetInput().Pressed(key),"own-window native key press not observed");
        if(std::find(keys.begin(),keys.end(),unsigned(VK_F5))!=keys.end())++f5Count;
        if(std::find(keys.begin(),keys.end(),unsigned(VK_F9))!=keys.end())++f9Count;
        try{game.OnUpdate(engine,dt);}catch(...){Keys(engine,keys,false);engine.GetInput().ConsumePresses();throw;}
        Keys(engine,keys,false);engine.GetInput().ConsumePresses();
    }
    bool FailureCase()const{return caseName==L"truncated"||caseName==L"missing"||caseName==L"body-overlap"||caseName==L"middle-overlap"||caseName==L"enemy-y-below"||caseName==L"enemy-y-above";}
    void PrepareDiskAfterF5(){
        const auto saved=Read(game.save);
        if(caseName==L"outside-save"){
            Require(game.message=="Save position unavailable","unsafe actual F5 did not report unavailable save position");unsafeF5Rejected=true;
            Require(saved==oldSaveBytes&&expected.Serialize()==oldSaveBytes,"unsafe actual F5 replaced previous safe checkpoint");oldSavePreserved=true;
            Require(!game.terrain.Contains(game.session.feet.x,game.session.feet.z)&&Snapshot().Valid()&&Snapshot().Serialize()!=expected.Serialize(),
                "unsafe F5 fixture no longer has distinct outside runtime position");
            Write(output/L"after-f5.txt",saved);
            Require(Read(game.audioSettings)==initialAudioSettings,"unsafe actual F5 changed isolated audio settings");
            // 當前越界狀態已區別於舊快照；不能再用 Seed 預檢越界位置或要求磁碟等於目前狀態。
            return;
        }
        Require(expected.Restore(saved)&&expected.Serialize()==Snapshot().Serialize(),"actual F5 isolated disk differs from live snapshot");
        Require(saved.starts_with("MISTWATCH_SAVE 2"),"actual F5 did not save terrain-aware format");
        Write(output/L"after-f5.txt",saved);
        Require(Read(game.audioSettings)==initialAudioSettings,"actual F5 changed isolated audio settings");
        if(caseName==L"legacy-v6"){
            const auto legacy=expected.session.Serialize();Write(game.save,legacy);
            Mistwatch::SaveGame migration;Require(migration.Restore(legacy),"legacy fixture migration invalid");expected=migration;
        }else if(caseName==L"enemy-y-below"||caseName==L"enemy-y-above"){
            const auto begin=saved.find("\nENCOUNTER "),end=saved.find("\nEVASION ",begin);
            Require(begin!=std::string::npos&&end!=std::string::npos,"enemy-y serialized fixture missing records");
            std::istringstream in(saved.substr(begin+11,end-begin-11));std::array<std::string,11> fields;
            for(auto& field:fields)Require(bool(in>>field),"enemy-y serialized fixture lacks fields");
            fields[3]=caseName==L"enemy-y-below"?"-100":"100";
            std::string corrupt=saved.substr(0,begin)+"\nENCOUNTER";
            for(const auto& field:fields)corrupt+=' '+field;
            corrupt+=saved.substr(end);Write(game.save,corrupt);
        }else if(caseName==L"truncated")Write(game.save,saved.substr(0,saved.size()/2));
        else if(caseName==L"missing")Require(std::filesystem::remove(game.save),"isolated missing fixture removal failed");
        else if(caseName==L"body-overlap"){
            auto invalid=expected;invalid.session.feet={-7.25f,0,16.1f};Require(invalid.Valid(),"overlap fixture is invalid serialized game");
            Write(game.save,invalid.Serialize());
        }
        else if(caseName==L"middle-overlap"){
            Require(game.middleLayerSolids.size()==24,"middle-overlap requires explicit middle-layer mode");
            auto invalid=expected;const auto p=game.middleLayerSolids.front().center;
            invalid.session.feet={p.x,game.terrain.SampleHeight(p.x,p.z),p.z};
            Require(invalid.Valid(),"middle-overlap fixture must be format-valid");
            Write(game.save,invalid.Serialize());
        }
        if(!FailureCase()){
            auto changed=Snapshot();
            if(!changed.session.guardianDefeated){changed.encounter={};changed.evasion={};}
            if(changed.session.Alive())changed.session.health=changed.session.health==80?100:80;
            Seed(changed);
            Require(Snapshot().Serialize()!=expected.Serialize(),"live mutation failed to distinguish F9 restoration");
        }
    }
    void RestoreCapture(){
        game.capture=capturePath;game.captureStart=std::chrono::steady_clock::now();
        game.captureFrames=0;game.capturePending=false;game.captureCompleted=0;game.captureConsumed=0;
        game.captureCameraTicks=0;game.started=true;game.paused=false;
    }
    void ReachTerrainEdge(){
        // 起點合法且足底完整位於高度場內；以原控制器東行，不能把邊界腳位置直接傳送出來。
        Require(game.session.feet.x==49.4f&&game.controller.Grounded(),"terrain-edge starting checkpoint not grounded");
        while(game.controller.Position().x<49.69f&&terrainEdgeControllerSteps<4){
            const auto previous=game.controller.Position();
            game.controller.Step({{1,0,0},false,1},game.solids,game.terrain,1.f/60.f);++terrainEdgeControllerSteps;
            game.session.feet=game.controller.Position();
            Require(game.session.feet.x>previous.x&&game.controller.Grounded(),"terrain-edge actual controller failed to advance grounded");
            Require(game.session.feet.y==game.terrain.SampleHeight(game.session.feet.x,game.session.feet.z),"terrain-edge controller height was artificially raised");
            game.explorer.Update(game.session.feet-previous,1.f/60.f,false);
            Require(game.explorer.PlaceFeet(game.session.feet,true,false,game.terrain,game.solids),"terrain-edge actual foot IK failed");
            Require(game.UpdatePlayerCamera(1.f/60.f),"terrain-edge actual native camera transition rejected");
            game.lastRenderedCamera=game.lastCameraMotion.camera;
        }
        terrainEdgeFeet=game.session.feet;
        Require(terrainEdgeFeet.x>49.69f&&terrainEdgeFeet.x<49.71f&&Snapshot().Valid(),"terrain-edge actual controller did not reach legal save center");
        Write(output/L"terrain-edge-reach.txt","scope=actual-controller-step-not-native-walk\nsteps="+std::to_string(terrainEdgeControllerSteps)+
            "\nfeet_x="+std::to_string(terrainEdgeFeet.x)+"\nfeet_y="+std::to_string(terrainEdgeFeet.y)+"\nfeet_z="+std::to_string(terrainEdgeFeet.z)+
            "\ngrounded=1\ncenter_inside=1\ncamera_transition_accepted=1\nfeet_height_raised=0\n");
    }
    void ReachOutsideNorth(){
        Require(game.session.feet.z==99.4f&&game.controller.Grounded(),"outside-save legal north checkpoint not grounded");
        expected=Snapshot();oldSaveBytes=expected.Serialize();Require(!oldSaveBytes.empty(),"outside-save old snapshot invalid");
        // 這是隔離舊檔前置資料；唯一實際 F5 留給越界拒絕驗證，不偽稱此處使用玩家輸入。
        Write(game.save,oldSaveBytes);Write(output/L"before-unsafe-f5.txt",oldSaveBytes);
        unsigned steps{};
        const auto step=[&](float dt){
            const auto previous=game.controller.Position();
            game.controller.Step({{0,0,1},false,1},game.solids,game.terrain,dt);++steps;
            game.session.feet=game.controller.Position();
            Require(game.session.feet.z>previous.z,"outside-save real controller failed to move north");
            if(game.terrain.Contains(game.session.feet.x,game.session.feet.z))
                Require(game.controller.Grounded()&&game.session.feet.y==game.terrain.SampleHeight(game.session.feet.x,game.session.feet.z),
                    "outside-save approach artificially changed supported feet height");
            game.explorer.Update(game.session.feet-previous,dt,false);
            Require(game.explorer.PlaceFeet(game.session.feet,game.controller.Grounded(),false,game.terrain,game.solids),"outside-save actual explorer pose failed");
            Require(game.UpdatePlayerCamera(dt),"outside-save actual native camera transition rejected");game.lastRenderedCamera=game.lastCameraMotion.camera;
        };
        while(game.controller.Position().z<99.89f&&steps<6)step(1.f/60.f);
        Require(game.session.feet.z>99.89f&&game.session.feet.z<99.91f,"outside-save actual approach did not reach 99.9");
        step(.1f);
        Require(game.session.feet.z>100&&!game.terrain.Contains(game.session.feet.x,game.session.feet.z)&&Snapshot().Valid(),
            "outside-save actual bounded controller step did not produce format-valid outside position");
        std::ofstream out(output/L"outside-save-reach.txt");out.precision(std::numeric_limits<float>::max_digits10);
        out<<"scope=actual-controller-step-not-native-walk\nsteps="<<steps<<"\nold_checkpoint_z=99.4\nlast_step_dt=.1"
            <<"\nfeet="<<game.session.feet.x<<','<<game.session.feet.y<<','<<game.session.feet.z
            <<"\ncenter_inside=0\nsnapshot_valid=1\ncamera_transition_accepted=1\nold_save_direct_fixture=1\n";
        out.close();Require(bool(out),"outside-save real step receipt write failed");
    }
public:
    MistwatchSaveInputProbe(std::wstring name,std::filesystem::path directory,bool middle):caseName(std::move(name)),output(std::move(directory)),capturePath(output/L"native-after-load.png"),expectedMiddleLayer(middle){}
    void OnStart(Engine& engine)override {
        game.OnStart(engine);initialized=true;
        try{
            ValidateMiddleSolids();
            Require(game.capture==capturePath&&game.gameAudio.options.muted,"initial capture did not isolate or mute assets");
            game.capture.clear();game.save=output/L"isolated-save.txt";game.audioSettings=output/L"isolated-audio-settings.txt";game.started=true;game.paused=false;
            Require(!std::filesystem::exists(game.save),"audit attempted existing save path");
            Require(!std::filesystem::exists(game.audioSettings),"audit attempted existing audio settings path");
            initialAudioSettings=game.gameAudio.options.Serialize();Write(game.audioSettings,initialAudioSettings);
            initialMasterVolume=engine.Audio().GetStats().masterVolume;
            Require(initialMasterVolume==game.gameAudio.options.Gain(),"initial audio observer gain not bound to current options");
            auto seed=BattleSeed();
            if(caseName==L"cooldown-only"){
                for(unsigned i=0;i<2;++i)seed.evasion.Tick(.1f,false,true,false,{});
                Require(seed.evasion.remaining==0&&seed.evasion.cooldown>0&&seed.Valid(),"cooldown-only seed invalid");
            }else if(caseName==L"dead"||caseName==L"retry-battle"||caseName==L"retry-after-battle"){
                seed.session.health=0;seed.evasion={};
                if(caseName==L"retry-after-battle"){
                    seed.session.guardianDefeated=true;seed.encounter.health=0;
                    seed.encounter.phase=Mistwatch::EnemyPhase::Defeated;seed.encounter.countered=true;
                }
            }
            else if(caseName==L"complete"){
                seed.session.guardianDefeated=true;seed.session.fuseClueRead=true;seed.session.fuseDials=Mistwatch::Session::solvedFuseDials;
                seed.session.fuseCollected=true;seed.session.circuit=7;seed.session.powerRestored=true;seed.session.relayLinked=7;
                seed.session.relaySectors=0;for(unsigned i=0;i<3;++i)seed.session.relaySectors|=Mistwatch::Session::RelayTargetSector(i)<<(i*4);
                seed.session.beaconLit=true;seed.session.feet=Mistwatch::Session::beacon;seed.evasion={};
                seed.encounter.health=0;seed.encounter.phase=Mistwatch::EnemyPhase::Defeated;seed.encounter.countered=true;
            }else if(caseName==L"shelter"){
                seed={};seed.session.collected=7;seed.session.feet={-7.25f,0,18};
            }else if(caseName==L"slope"){
                seed.session.feet={-10.1f,0,62};
                seed.session.feet.y=game.terrain.SampleHeight(seed.session.feet.x,seed.session.feet.z);seed.evasion={};
            }else if(caseName==L"enemy-slope-pursuit"||caseName==L"enemy-slope-windup"||caseName==L"enemy-slope-recovery"){
                // 坡地快照前置不是按鍵行走證據；F5/F9 仍走正式原生分支且精確核對敵人位置與計時。
                seed.encounter.feet={18,game.terrain.SampleHeight(18,55),55};seed.evasion={};
                seed.encounter.phase=caseName==L"enemy-slope-pursuit"?Mistwatch::EnemyPhase::Pursuit:
                    caseName==L"enemy-slope-windup"?Mistwatch::EnemyPhase::Windup:Mistwatch::EnemyPhase::Recovery;
                seed.encounter.phaseTime=caseName==L"enemy-slope-windup"?.4f:caseName==L"enemy-slope-recovery"?.7f:0;
                seed.encounter.countered=false;
                game.captureBoarOverview=true;
            }else if(caseName==L"terrain-edge"){
                seed.session.feet={49.4f,0,0};seed.session.feet.y=game.terrain.SampleHeight(seed.session.feet.x,seed.session.feet.z);seed.evasion={};
            }else if(caseName==L"outside-save"){
                seed={};seed.session.feet={0,0,99.4f};seed.session.feet.y=game.terrain.SampleHeight(seed.session.feet.x,seed.session.feet.z);
            }
            Require(seed.Valid(),"native case seed invalid");Seed(seed);
            if(caseName==L"terrain-edge")ReachTerrainEdge();
            if(caseName==L"outside-save")ReachOutsideNorth();
        }catch(...){RestoreCapture();try{game.OnStop(engine);}catch(...){}stopped=true;throw;}
    }
    void OnUpdate(Engine& engine,float dt)override {
        if(phase==0){
            if(caseName==L"retry-battle"||caseName==L"retry-after-battle"){
                Require(!game.session.Alive(),"retry native fixture must start dead");
                const auto prior=game.session;
                const std::array retryKeys{unsigned('R')};NativeTick(engine,retryKeys,1.f/60.f);
                Require(game.session.Alive()&&game.session.health==100&&game.session.guardianDefeated==prior.guardianDefeated&&
                    game.session.bridgeRepaired==prior.bridgeRepaired&&game.session.collected==prior.collected&&
                    game.session.repairMaterials==prior.repairMaterials,"actual R retry changed completed mission progress");
                Require(Snapshot().Valid(),"actual R retry after completed battle made checkpoint invalid");
                ValidateMiddleSolids();
                Require(game.encounter.phase==(prior.guardianDefeated?Mistwatch::EnemyPhase::Defeated:Mistwatch::EnemyPhase::Pursuit)&&
                    game.encounter.health==(prior.guardianDefeated?0u:3u),"actual R retry reset encounter contrary to mission progress");
                Write(output/L"retry-native.txt","actual_R_count=1\nmission_progress_preserved=1\nencounter_consistent=1\ncheckpoint_valid=1\n");
            }
            const std::array keys{unsigned(VK_F5)};NativeTick(engine,keys,1.f/60.f);PrepareDiskAfterF5();++phase;return;
        }
        if(phase==1){
            const auto before=Fingerprint();const auto camera=game.playerCameraMotion;beforeLoadWater=game.waterTime;
            const auto beforeAudioOptions=game.gameAudio.options.Serialize();const auto beforeMaster=engine.Audio().GetStats().masterVolume;
            Require(beforeAudioOptions==initialAudioSettings&&Read(game.audioSettings)==initialAudioSettings&&beforeMaster==initialMasterVolume,
                "pre-F9 isolated audio state differs from initial options");
            const std::array keys{unsigned(VK_F9),unsigned('R'),unsigned('Q'),unsigned('F'),unsigned(VK_SPACE),unsigned('W'),
                unsigned('M'),unsigned(VK_PRIOR),unsigned(VK_NEXT),unsigned(VK_ESCAPE)};
            NativeTick(engine,keys,.1f);
            ValidateMiddleSolids();
            Require(game.gameAudio.options.Serialize()==beforeAudioOptions,"actual F9 plus audio keys changed options");audioOptionsUnchanged=true;
            Require(engine.Audio().GetStats().masterVolume==beforeMaster,"actual F9 plus audio keys changed effective master volume");masterVolumeUnchanged=true;
            Require(Read(game.audioSettings)==initialAudioSettings,"actual F9 plus audio keys rewrote isolated audio settings");audioSettingsUnchanged=true;
            Require(game.started&&!game.paused,"actual F9 plus Escape changed playback state");
            Require(game.waterTime==beforeLoadWater,"actual F9 consumed load-tick water time");
            if(FailureCase()){
                Require(game.message=="Save unavailable","invalid actual F9 did not take rejection branch");
                Require(Fingerprint()==before,"rejected actual F9 partially changed live CPU/audio state");
                UnchangedCameraContinuation(camera);stateUnchanged=true;
            }else {Require(game.message=="Loaded","valid actual F9 did not take load branch");ValidateLoaded();}
            Write(output/L"after-f9.txt",Snapshot().Serialize());passed=true;RestoreCapture();++phase;return;
        }
        game.OnUpdate(engine,dt);
    }
    void OnRender(Engine& engine,float alpha)override{game.OnRender(engine,alpha);}
    void OnStop(Engine& engine)override {
        if(stopped||!initialized)return;stopped=true;
        if(game.capture.empty())RestoreCapture();
        game.OnStop(engine);
        Require(passed&&f5Count==1&&f9Count==1,"native key audit did not finish exactly one save/load");
        Require(game.captureCompleted==1&&std::filesystem::is_regular_file(capturePath),"actual post-load capture not completed");
        const auto receipt=Read(std::filesystem::path(capturePath.wstring()+L".receipt.txt"));
        Require(receipt.find("capture=success")!=std::string::npos,"actual capture lacks production receipt");
        Require(audioOptionsUnchanged&&masterVolumeUnchanged&&audioSettingsUnchanged&&Read(game.audioSettings)==initialAudioSettings,
            "isolated audio settings changed before cleanup completed");
        std::string asciiCase;
        for(const wchar_t character:caseName){Require(character<=127,"receipt case is not ASCII");asciiCase.push_back(static_cast<char>(character));}
        std::ofstream out(output/L"save-input-audit-receipt.txt");
        out<<"schema=mistwatch-save-input-audit-2\ncase="<<asciiCase
            <<"\nstatus=passed\nactual_f5_count="<<f5Count<<"\nactual_f9_count="<<f9Count
            <<"\nsnapshot_equal="<<snapshotEqual<<"\nstate_unchanged="<<stateUnchanged
            <<"\nf9_same_tick_R_Q_F_SPACE_W_M_PAGEUP_PAGEDOWN_ESCAPE=1\nf9_same_tick_keys=R,Q,F,SPACE,W,M,PAGEUP,PAGEDOWN,ESCAPE"
            <<"\nload_tick_time_not_consumed=1\nisolated_save=1\nisolated_audio_settings=1\nisolated_audio_settings_bytes_unchanged="<<audioSettingsUnchanged
            <<"\naudio_options_unchanged="<<audioOptionsUnchanged<<"\neffective_master_volume_unchanged="<<masterVolumeUnchanged
            <<"\nplayer_audio_settings_touched=0\nterrain_edge_controller_steps="<<terrainEdgeControllerSteps
            <<"\nunsafe_f5_rejected="<<unsafeF5Rejected<<"\nold_save_preserved="<<oldSavePreserved
            <<"\nmiddle_layer_population="<<game.middleLayer.trees.size()<<"\nmiddle_layer_solids="<<game.middleLayerSolids.size()
            <<"\nworld_solids="<<game.solids.size()<<"\nmiddle_solids_preserved=1"
            <<"\nactual_load_branch_confirmed=1"
            <<"\nexpected_middle_layer_mode="<<expectedMiddleLayer<<"\nmode_matches_expected=1"
            <<"\nown_window_only=1\nglobal_input=0\nforeground_activation=0\nmuted=1\nhidden_window=1"
            <<"\nactual_capture=1\nactual_cleanup=1\nscope=native-key-branch-audit-not-human-playthrough"
            <<"\nload_semantics=checkpoint-not-exact-motion\n";
        out.close();Require(bool(out),"native audit receipt write failed");
    }
};

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    std::filesystem::path output;
    bool ownsOutput{};
    try{
        int count{};auto argv=CommandLineToArgvW(GetCommandLineW(),&count);Require(argv!=nullptr,"command line parsing failed");
        if(count!=3&&count!=4){LocalFree(argv);throw std::runtime_error("usage: CASE NEW_OUTPUT_DIRECTORY [--middle-layer|--baseline-middle-layer]");}
        const bool baseline=count==4&&std::wstring_view(argv[3])==L"--baseline-middle-layer";
        const bool middleLayer=!baseline;
        if(count==4&&!baseline&&std::wstring_view(argv[3])!=L"--middle-layer"){LocalFree(argv);throw std::runtime_error("unknown native audit option");}
        const std::wstring name=argv[1];output=argv[2];LocalFree(argv);
        Require(KnownCase(name),"unknown native audit case");
        Require(name!=L"middle-overlap"||middleLayer,"middle-overlap requires --middle-layer");
        Require(output.is_absolute()&&output!=output.root_path(),"audit output must be a dedicated absolute directory");
        Require(!std::filesystem::exists(output),"audit output already exists; refusing existing saves/evidence");
        Require(std::filesystem::create_directory(output),"fresh audit output creation failed");
        ownsOutput=true;
        // 只清除此測試程序的診斷環境，不更改系統設定或 GPU 驗證開關。
        for(const auto variable:{L"AEVRYND_MISTWATCH_SCENE",L"AEVRYND_MISTWATCH_CAPTURE_FRAME",L"AEVRYND_MISTWATCH_CAPTURE_SERIES",
            L"AEVRYND_MISTWATCH_BENCHMARK",L"AEVRYND_MISTWATCH_CROWN",L"AEVRYND_MISTWATCH_FERN_LEGACY",L"AEVRYND_MISTWATCH_GRASS_DIM",
            L"AEVRYND_MISTWATCH_GRASS_FIXTURE_ENABLED",L"AEVRYND_MISTWATCH_GRASS_FIXTURE",L"AEVRYND_MISTWATCH_GRASS_LEGACY",
            L"AEVRYND_MISTWATCH_GRASS_PROCEDURAL",L"AEVRYND_MISTWATCH_GROUND_LEGACY",L"AEVRYND_MISTWATCH_GROUND_NEUTRAL_BASE",
            L"AEVRYND_MISTWATCH_GROUND_NEUTRAL_SURFACE",L"AEVRYND_MISTWATCH_HDR_STAGE",L"AEVRYND_MISTWATCH_LEGACY_RANGER_HUT",
            L"AEVRYND_MISTWATCH_NO_AO",L"AEVRYND_MISTWATCH_NO_BLOOM",L"AEVRYND_MISTWATCH_NO_GROUND_NORMAL",
            L"AEVRYND_MISTWATCH_NO_GROUND_TEXTURES",L"AEVRYND_MISTWATCH_NO_MSAA",L"AEVRYND_MISTWATCH_NO_SHADOWS",
            L"AEVRYND_MISTWATCH_TREE_LOD",L"AEVRYND_MISTWATCH_TWIG_MODE",L"AEVRYND_MISTWATCH_MIDDLE_LAYER"})Require(SetEnvironmentVariableW(variable,nullptr)!=0,"audit environment reset failed");
        if(baseline)Require(SetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MIDDLE_LAYER",L"0")!=0,"explicit old baseline environment failed");
        const auto capture=output/L"native-after-load.png";
        Require(SetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",capture.c_str())!=0,"isolated initial capture environment failed");
        Engine engine({L"Aevrynd - Mistwatch Save Input Audit",1280,720});
        return engine.Run(std::make_unique<MistwatchSaveInputProbe>(name,output,middleLayer),instance,SW_HIDE);
    }catch(const std::exception& error){
        // 既有輸出目錄／參數錯誤不覆寫他人的紀錄。
        if(ownsOutput){
            auto failure=output/L"save-input-first-divergence.txt";
            if(!std::filesystem::exists(failure))Write(failure,std::string("FIRST_DIVERGENCE: ")+error.what()+"\n");
        }
        return 1;
    }
}
