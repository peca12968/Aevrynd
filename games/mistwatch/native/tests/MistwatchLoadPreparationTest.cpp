#include "MistwatchLoadPreparation.h"
#include "MistwatchMiddleLayer.h"
#include <bit>
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
using namespace Mistwatch;
using namespace Aevrynd;
unsigned checks{},prepared{},rejections{};
unsigned reachableSlopeAccepted{};
void Require(bool value,std::string_view reason){++checks;if(!value)throw std::runtime_error(std::string(reason));}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct Hash {
    std::uint64_t value=1469598103934665603ull;
    template<class T> void Add(const T& object){
        static_assert(std::is_trivially_copyable_v<T>);
        for(auto byte:std::as_bytes(std::span{&object,1})){value^=std::to_integer<unsigned char>(byte);value*=1099511628211ull;}
    }
    template<class T> void Vector(const std::vector<T>& objects){Add(objects.size());for(const auto& item:objects)Add(item);}
    void Text(std::string_view text){for(char byte:text){value^=static_cast<unsigned char>(byte);value*=1099511628211ull;}}
    void Model(const SkinnedModel3D& model){
        Vector(model.bindMesh.vertices);Vector(model.bindMesh.indices);Vector(model.skeleton.joints);
        Vector(model.influences);Vector(model.inverseBindMatrices);Vector(model.tangents);Add(model.skeletonCompatibilityId);
    }
    void Pose(const AnimationPose3D& pose){Vector(pose.localTransforms);Vector(pose.globalMatrices);}
};
struct Live {
    SaveGame game;
    CharacterController3D controller;
    Explorer explorer;
    Boar boar;
    OrbitCamera orbit{-.65f,.3f,6};
    std::vector<BoxCollider3D> solids=WorldSolids(false);
    ThirdPersonCameraMotion3D motion;
    ThirdPersonFramingResult3D framing;
    ThirdPersonCameraMotionResult3D result;
    Camera3D rendered;
    LoadPreparationInputs Inputs()const{return {controller,explorer,boar,orbit};}
    SavedGameCommitTargets Targets(){return {game.session,game.encounter,game.evasion,controller,explorer,boar,orbit,solids,motion,framing,result,rendered};}
    std::uint64_t Fingerprint()const {
        Hash h;h.Text(game.Serialize());h.Add(controller.StateHash());h.Add(controller.Settings());
        h.Model(explorer.model);h.Pose(explorer.pose);h.Add(explorer.phase);h.Add(explorer.yaw);h.Add(explorer.footTargets);
        h.Add(explorer.planted);h.Add(explorer.lastFeet);h.Add(explorer.feetInitialized);h.Add(explorer.locomotionDistance);
        h.Model(boar.model);h.Pose(boar.pose);h.Add(boar.yaw);h.Add(boar.cycle);h.Add(boar.displacement);h.Add(boar.brace);
        h.Add(boar.footTargets);h.Add(boar.planted);h.Add(boar.lastFeet);h.Add(boar.initialized);
        h.Add(orbit);h.Vector(solids);h.Add(motion.Initialized());h.Add(motion.Camera());h.Add(framing);h.Add(result);h.Add(rendered);
        return h.value;
    }
};
bool SameResult(const ThirdPersonCameraMotionResult3D& a,const ThirdPersonCameraMotionResult3D& b){
    return a.accepted==b.accepted&&a.subjectVisible==b.subjectVisible&&a.status==b.status&&Same(a.camera.position,b.camera.position)&&
        Same(a.camera.target,b.camera.target)&&a.displacement==b.displacement;
}
void SameFutureCamera(const Live& live,const ThirdPersonCameraMotion3D& before,const HeightField3D& terrain){
    auto a=live.motion,b=before;
    const std::array extra{RangerHutCameraRoofSolid()};
    for(unsigned i=0;i<6;++i){
        auto orbit=live.orbit;orbit.yaw+=.04f*float(i+1);
        const auto request=BuildPlayerCameraMotionRequest(live.game.session.feet,live.controller.Settings().height,orbit,a.Initialized(),live.solids,terrain,extra);
        const auto samples=PlayerCameraSamples(live.game.session.feet,live.controller.Settings().height);
        const auto x=a.Update(request,samples,live.solids,CameraCeilingSolids(),terrain,1.f/60.f,extra);
        const auto y=b.Update(request,samples,live.solids,CameraCeilingSolids(),terrain,1.f/60.f,extra);
        Require(SameResult(x,y),"preparation changed private camera continuation");
    }
}
LoadPreparationResult PrepareChecked(const SaveGame& save,const Live& live,const HeightField3D& terrain,
    std::span<const BoxCollider3D> extra={}) {
    const auto before=live.Fingerprint();const auto priorCamera=live.motion;
    Hash snapshot; snapshot.Add(save);const auto savedHash=snapshot.value;
    auto result=PrepareSavedGame(save,live.Inputs(),terrain,extra);
    Require(live.Fingerprint()==before,"preparation mutated live CPU inputs");
    Hash after;after.Add(save);Require(after.value==savedHash,"preparation mutated input save");
    SameFutureCamera(live,priorCamera,terrain);
    return result;
}
void ValidateCandidate(const PreparedSavedGame& p,const SaveGame& original,const HeightField3D& terrain){
    Require(p.snapshot.Serialize()==original.Serialize(),"preparation changed saved Session/Encounter/Evasion fields");
    Require(Same(p.controller.Position(),original.session.feet),"checkpoint silently relocated saved feet");
    Require(Same(p.controller.Velocity(),{}),"checkpoint velocity not explicitly reset");
    Require(p.supported==p.controller.Grounded(),"support receipt differs from candidate controller");
    Require(p.explorerIkApplied==p.supported&&p.airbornePoseValidated==!p.supported,"airborne shortcut falsely labeled IK applied");
    Require(std::isfinite(p.explorerFootTargetError)&&p.explorerFootTargetError>=0,"foot target error diagnostic invalid");
    if(p.supported){Require(p.supportProbeUsed&&(p.supportProbeSeconds==0||p.supportProbeSeconds==1.f/60.f),"real support probe evidence missing");
        Require(p.explorer.planted[0]&&p.explorer.planted[1],"supported checkpoint did not solve planted feet");}
    else {Require(!p.supportProbeUsed&&p.supportProbeSeconds==0,"airborne checkpoint advanced fake support time");
        Require(!p.explorer.planted[0]&&!p.explorer.planted[1],"airborne checkpoint retained ground foot locks");}
    CollisionWorld3D independent;
    for(const auto& box:p.solids)Require(bool(independent.CreateBox({box})),"independent collision fixture creation failed");
    Require(!independent.OverlapsBox(p.controller.Center(),p.controller.Extents()),"accepted body overlaps current-world box");
    const auto camera=p.cameraMotion.Camera();
    Require(p.cameraResult.accepted&&p.cameraResult.status==ThirdPersonCameraMotionStatus3D::ResetCut&&p.cameraResult.subjectVisible,"accepted checkpoint lacks visible ResetCut");
    const float tangent=std::tan(camera.projection.verticalFieldOfView*.5f);
    const float radius=(std::max)(.16f,.005f+camera.projection.nearPlane*std::sqrt(1.f+tangent*tangent*(1.f+1280.f/720.f*1280.f/720.f)));
    Require(bool(independent.CreateBox({RangerHutCameraRoofSolid()})),"independent camera roof fixture creation failed");
    Require(!independent.SweepSphere(camera.position,radius,{}),"independent full projection-radius camera overlaps a solid");
    for(const auto sample:PlayerCameraSamples(p.snapshot.session.feet,p.controller.Settings().height))
        Require(ProjectToViewport(sample,camera,{1280,720}).visible,"accepted loaded subject sample outside viewport");
    Require(Same(p.renderedCamera.position,camera.position)&&Same(p.renderedCamera.target,camera.target),"audio/render listener camera would be stale");
    Require(p.orbit.yaw==PlayerCameraMovementYaw(camera,p.orbit.yaw),"safe cut yaw not adopted");
    Require(std::isfinite(terrain.SampleHeight(camera.position.x,camera.position.z)),"accepted camera terrain not finite");
    ++prepared;
}
Session BridgeSession(){
    Session s;
    // 合成規則測試位置，不是原生按鍵通關證據。
    for(unsigned i=0;i<3;++i){s.feet=Session::supplies[i];Require(s.Collect(i),"fixture supply failed");}
    for(unsigned i=0;i<3;++i){s.feet=Session::repairSites[i];Require(s.CollectRepairMaterial(i,i==0),"fixture repair material failed");}
    s.feet=Session::bridge;Require(s.RepairBridge(),"fixture bridge repair failed");s.feet=Session::arena;return s;
}
SaveGame HpOne(){
    SaveGame save;save.session=BridgeSession();
    for(unsigned hit=0;hit<2;++hit){
        unsigned ticks{};
        while(save.encounter.phase!=EnemyPhase::Recovery&&ticks++<300)save.session.Damage(save.encounter.Tick(save.session.feet,.02f,false));
        Require(save.encounter.phase==EnemyPhase::Recovery,"HP1 fixture counter window missing");
        save.session.Damage(save.encounter.Tick(save.session.feet,.02f,true));
        if(!hit){ticks=0;while(save.encounter.phase==EnemyPhase::Recovery&&ticks++<300)save.session.Damage(save.encounter.Tick(save.session.feet,.02f,false));}
    }
    Require(save.encounter.health==1&&save.Valid(),"HP1 fixture invalid");
    save.evasion.Tick(.02f,true,true,false,{1,0,.5f});Require(save.Valid(),"active dash fixture invalid");return save;
}
void Reject(const SaveGame& save,const Live& live,const HeightField3D& terrain,LoadPreparationError error,const char* name,
    std::span<const BoxCollider3D> extra={}){
    const auto result=PrepareChecked(save,live,terrain,extra);
    if(result.error!=error)std::cerr<<"expected_error="<<unsigned(error)<<" actual_error="<<unsigned(result.error)<<" case="<<name<<'\n';
    Require(!result&&!result.candidate&&result.error==error,name);++rejections;
}
void AcceptedAndRollback(const SaveGame& save,Live& live,const HeightField3D& terrain,bool supported,float height){
    auto preparedResult=PrepareChecked(save,live,terrain);Require(bool(preparedResult),"expected valid checkpoint preparation failed");
    auto& p=*preparedResult.candidate;ValidateCandidate(p,save,terrain);
    Require(p.supported==supported&&p.controller.Settings().height==height,"checkpoint posture/support semantics differ");
    const auto before=live.Fingerprint();const auto cameraBefore=live.motion;
    SwapPreparedSavedGame(p,live.Targets());
    Require(live.game.Serialize()==save.Serialize()&&Same(live.controller.Position(),save.session.feet),"noexcept commit lost snapshot or feet");
    Require(live.motion.Initialized()&&Same(live.rendered.position,live.motion.Camera().position),"commit left stale rendered camera");
    SwapPreparedSavedGame(p,live.Targets());
    Require(live.Fingerprint()==before,"immediate noexcept swap rollback lost old complete CPU state");
    SameFutureCamera(live,cameraBefore,terrain);
}
void ReachableSlopeBaseline(const Live& live,const HeightField3D& terrain){
    // 使用目前世界與原控制器的實際步進，不以 Reset／Teleport 假造斜坡接地。
    // 修橋旗標由既有規則合成；這是 CPU 可達性證據，不是原生按鍵通關證據。
    SaveGame save;save.session=BridgeSession();save.session.feet={};
    const auto solids=WorldSolids(true);
    CharacterController3D controller(live.controller.Settings());controller.Reset({});
    controller.Step({},solids,terrain,0);
    Require(controller.Grounded(),"reachable slope baseline initial grounding failed");
    unsigned ticks{};bool firstLogged{},slopeLogged{};
    const std::array waypoints{Vec3{0,0,62},Vec3{-12,0,62}};
    for(const auto waypoint:waypoints){
        unsigned legTicks{};
        while(std::hypot(controller.Position().x-waypoint.x,controller.Position().z-waypoint.z)>.11f&&legTicks++<900){
            const auto feetBefore=controller.Position();
            const auto direction=Normalize(Vec3{waypoint.x-feetBefore.x,0,waypoint.z-feetBefore.z});
            controller.Step({direction,false,1},solids,terrain,1.f/60.f);++ticks;
            const auto feet=controller.Position();
            Require(Length(feet-feetBefore)>0,"reachable route made no controller progress");
            Require(controller.Grounded(),"reachable route unexpectedly airborne");
            Require(!LoadPreparationDetail::BodyOverlaps(controller.Center(),controller.Extents(),solids),"reachable controller overlaps current solid");
            double highest{};
            Require(LoadPreparationDetail::Footprint(terrain,feet,controller.Settings().radius,highest)==LoadPreparationError::None,
                "reachable route footprint invalid");
            const auto center=terrain.SampleHeight(feet.x,feet.z);
            Require(feet.y>=center,"reachable grounded controller is below center terrain height");
            if(feet.y!=center){
                // 橋面由實體盒支撐，可以高於溪谷中心；列印支撐盒，不誤認成地形分歧。
                bool boxSupport{};
                for(std::size_t i=0;i<solids.size();++i){
                    const auto& box=solids[i];
                    const float solvedFeet=box.center.y+box.extents.y+controller.Extents().y-controller.Extents().y;
                    if(feet.y!=solvedFeet||std::abs(feet.x-box.center.x)>=box.extents.x+controller.Settings().radius||
                        std::abs(feet.z-box.center.z)>=box.extents.z+controller.Settings().radius)continue;
                    boxSupport=true;
                    if(ticks%20==0)std::cout<<std::setprecision(12)<<"REACHABLE_BOX_SUPPORT tick="<<ticks
                        <<" feet="<<feet.x<<','<<feet.y<<','<<feet.z<<" center="<<center<<" footprint_highest="<<highest
                        <<" grounded="<<controller.Grounded()<<" solid="<<i<<" top="<<box.center.y+box.extents.y
                        <<" solved_feet="<<solvedFeet<<" solid_center="<<box.center.x<<','<<box.center.y<<','<<box.center.z
                        <<" solid_extents="<<box.extents.x<<','<<box.extents.y<<','<<box.extents.z<<'\n';
                }
                if(!boxSupport)std::cerr<<std::setprecision(12)<<"REACHABLE_UNEXPLAINED_SUPPORT tick="<<ticks
                    <<" feet="<<feet.x<<','<<feet.y<<','<<feet.z<<" center="<<center<<" footprint_highest="<<highest
                    <<" grounded="<<controller.Grounded()<<'\n';
                Require(boxSupport,"reachable grounded point has neither center-terrain nor exact solved box support");
            }
            if(highest<=double(feet.y))continue;
            const bool first=!firstLogged,centerSlope=center>0&&!slopeLogged;
            const bool slopeSample=center>0&&legTicks%5==0;
            if(!first&&!centerSlope&&!slopeSample)continue;
            save.session.feet=feet;Require(save.Valid(),"reachable slope serialized snapshot invalid");
            const auto accepted=PrepareChecked(save,live,terrain);
            if(!accepted)std::cerr<<"reachable_load_error="<<unsigned(accepted.error)<<" tick="<<ticks<<'\n';
            Require(bool(accepted),"valid actual-controller slope checkpoint rejected");
            ValidateCandidate(*accepted.candidate,save,terrain);
            const auto& p=*accepted.candidate;
            Require(p.supported&&p.supportProbeSeconds==0,"actual center-grounded slope was shifted or fake advanced");
            ++reachableSlopeAccepted;
            std::cout<<std::setprecision(12)<<"REACHABLE_SLOPE_ACCEPTED kind="<<(first?"first_footprint_edge":centerSlope?"center_slope":"slope_sample")
                <<" tick="<<ticks<<" feet="<<feet.x<<','<<feet.y<<','<<feet.z
                <<" center="<<center<<" footprint_highest="<<highest<<" penetration="<<highest-double(feet.y)
                <<" grounded="<<controller.Grounded()<<" save_valid="<<save.Valid()
                <<" body_overlap=0 camera_reset_accepted="<<p.cameraResult.accepted
                <<" camera_subject_visible="<<p.cameraResult.subjectVisible
                <<" foot_target_max_error="<<p.explorerFootTargetError
                <<" ik_clamped_status=not_exposed_by_Explorer actual_native_input=0\n";
            for(unsigned side=0;side<2;++side){
                const auto target=p.explorer.footTargets[side];
                const auto toWorld=CreateTransformMatrix(feet,{0,p.explorer.yaw,0},{1,1,1});
                const auto ankle=TransformPoint(toWorld,TransformPoint(p.explorer.pose.globalMatrices[11+side*3],{}));
                std::cout<<"REACHABLE_SLOPE_FOOT side="<<side<<" target="<<target.x<<','<<target.y<<','<<target.z
                    <<" ankle="<<ankle.x<<','<<ankle.y<<','<<ankle.z<<" error="<<Length(ankle-target)<<'\n';
            }
            firstLogged|=first;slopeLogged|=centerSlope;
        }
        Require(std::hypot(controller.Position().x-waypoint.x,controller.Position().z-waypoint.z)<=.11f,
            "bounded actual-controller route failed to reach waypoint");
    }
    Require(firstLogged&&slopeLogged&&reachableSlopeAccepted>=3,"actual-world slope acceptance lacks edge/center/multiple samples");
}
void Run(){
    const auto terrain=CreateTerrain();Live live;
    auto initial=PrepareChecked(live.game,live,terrain);Require(bool(initial),"baseline start checkpoint failed");
    ValidateCandidate(*initial.candidate,live.game,terrain);SwapPreparedSavedGame(*initial.candidate,live.Targets());
    // 先建立非零移動與鏡頭狀態，避免不變性／回滾只測空白物件。
    const auto beforeFeet=live.controller.Position();live.controller.Step({{1,0,1},false,1},live.solids,terrain,.02f);
    live.game.session.feet=live.controller.Position();live.explorer.Update(live.game.session.feet-beforeFeet,.02f,false);
    Require(live.explorer.PlaceFeet(live.game.session.feet,live.controller.Grounded(),false,terrain,live.solids),"live baseline IK failed");
    const std::array roof{RangerHutCameraRoofSolid()};
    live.result=live.motion.Update(BuildPlayerCameraMotionRequest(live.game.session.feet,1.8f,live.orbit,true,live.solids,terrain,roof),
        PlayerCameraSamples(live.game.session.feet,1.8f),live.solids,CameraCeilingSolids(),terrain,1.f/60.f,roof);
    Require(live.result.accepted,"live baseline camera rejected");live.rendered=live.result.camera;
    Require(Length(live.controller.Velocity())>0&&live.explorer.phase>0,"live fixture lacks motion state to preserve");
    SaveGame start;AcceptedAndRollback(start,live,terrain,true,1.8f);
    // 真中層樹幹必須保留於載入候選及原子提交；相交存檔拒絕且 live 不變。
    const auto conifers=TreePositions();const auto props=ForestProps(terrain,conifers);
    const auto understory=CreateUnderstory(terrain,conifers,props);
    const auto original=CreateBroadleafTrees(terrain,conifers,props,understory);
    const auto middle=CreateMiddleLayer(terrain,conifers,props,understory,original,WorldSolids(false));
    Require(middle.reachedBudget&&middle.trees.size()==24,"middle-layer regression fixture incomplete");
    std::vector<BoxCollider3D> extraSolids;
    for(const auto& tree:middle.trees)extraSolids.push_back(tree.trunk);
    const auto beforeMiddle=live.Fingerprint();
    auto middlePrepared=PrepareSavedGame(start,live.Inputs(),terrain,{},extraSolids);
    Require(bool(middlePrepared)&&middlePrepared.candidate->solids.size()==245,"load omitted real middle-layer trunks");
    Require(live.Fingerprint()==beforeMiddle,"extra-solid preparation changed live state");
    for(std::size_t i=0;i<extraSolids.size();++i){
        const auto& actual=middlePrepared.candidate->solids[221+i];
        Require(Same(actual.center,extraSolids[i].center)&&Same(actual.extents,extraSolids[i].extents),"load changed additional trunk collision");
    }
    SwapPreparedSavedGame(*middlePrepared.candidate,live.Targets());
    Require(live.solids.size()==245,"commit dropped extra solids");
    SwapPreparedSavedGame(*middlePrepared.candidate,live.Targets());
    Require(live.Fingerprint()==beforeMiddle,"extra-solid commit rollback not exact");
    for(const auto& tree:middle.trees){
        auto overlapping=start;const auto p=tree.trunk.center;
        overlapping.session.feet={p.x,terrain.SampleHeight(p.x,p.z),p.z};
        const auto rejected=PrepareSavedGame(overlapping,live.Inputs(),terrain,{},extraSolids);
        Require(!rejected&&rejected.error==LoadPreparationError::BodyOverlap,"load inside added trunk accepted");
        Require(live.Fingerprint()==beforeMiddle,"added-trunk rejection changed live state");
    }
    for(const float extent:{0.f,-1.f,(std::numeric_limits<float>::infinity)(),(std::numeric_limits<float>::quiet_NaN)()}){
        const std::array badExtra{BoxCollider3D{{12,0,12},{extent,1,1}}};
        const auto rejected=PrepareSavedGame(start,live.Inputs(),terrain,{},badExtra);
        Require(!rejected&&rejected.error==LoadPreparationError::InvalidExtraSolids,"invalid extra extent accepted");
        Require(live.Fingerprint()==beforeMiddle,"invalid extra extent changed live state");
    }
    const std::array badCenter{BoxCollider3D{{(std::numeric_limits<float>::quiet_NaN)(),0,12},{1,1,1}}};
    const auto rejectedCenter=PrepareSavedGame(start,live.Inputs(),terrain,{},badCenter);
    Require(!rejectedCenter&&rejectedCenter.error==LoadPreparationError::InvalidExtraSolids,"nonfinite extra center accepted");
    SaveGame beforeBridge;beforeBridge.session.collected=7;beforeBridge.session.repairMaterials=7;beforeBridge.session.feet=Session::bridge;
    Require(beforeBridge.Valid(),"before-bridge snapshot invalid");AcceptedAndRollback(beforeBridge,live,terrain,true,1.8f);
    SaveGame onBridge;onBridge.session=BridgeSession();onBridge.session.feet={0,0,36};AcceptedAndRollback(onBridge,live,terrain,true,1.8f);
    auto preBridgeVoid=onBridge;preBridgeVoid.session.bridgeRepaired=false;
    // 未修橋時相同腳座標位於溪谷上空，不虛構橋面接地。
    Require(preBridgeVoid.Valid(),"pre-bridge air fixture invalid");AcceptedAndRollback(preBridgeVoid,live,terrain,false,1.8f);
    SaveGame shelter;shelter.session.collected=7;shelter.session.feet={-7.25f,0,18};
    AcceptedAndRollback(shelter,live,terrain,true,1.4f);
    auto battle=HpOne();AcceptedAndRollback(battle,live,terrain,true,1.8f);
    for(const float enemyY:{-100.f,100.f}){
        auto corruptEnemy=battle;corruptEnemy.encounter.feet.y=enemyY;
        const auto checked=PrepareChecked(corruptEnemy,live,terrain);
        std::cout<<"CORRUPT_ENEMY_VERTICAL feet_y="<<enemyY<<" snapshot_valid="<<corruptEnemy.Valid()
            <<" load_accepted="<<bool(checked)<<" error="<<unsigned(checked.error)<<'\n';
        Require(!checked&&checked.error==LoadPreparationError::InvalidSnapshot,
            "unreachable vertical boar checkpoint accepted and permanently disables combat");++rejections;
    }
    auto air=start;air.session.feet={0,2,0};AcceptedAndRollback(air,live,terrain,false,1.8f);
    auto underground=start;underground.session.feet={0,-.1f,0};Reject(underground,live,terrain,LoadPreparationError::BelowTerrainCenter,"underground checkpoint accepted");
    auto wall=shelter;wall.session.feet={-7.25f,0,16.1f};
    double wallHighest{};
    Require(LoadPreparationDetail::Footprint(terrain,wall.session.feet,.45f,wallHighest)==LoadPreparationError::None&&wallHighest==0,
        "isolated wall fixture also violates terrain footprint");
    Require(LoadPreparationDetail::BodyOverlaps(wall.session.feet+Vec3{0,.7f,0},{.45f,.7f,.45f},WorldSolids(false)),
        "isolated wall fixture does not overlap shelter wall");
    Reject(wall,live,terrain,LoadPreparationError::BodyOverlap,"shelter wall checkpoint accepted");
    auto invalid=start;invalid.session.feet.x=(std::numeric_limits<float>::quiet_NaN)();Reject(invalid,live,terrain,LoadPreparationError::InvalidSnapshot,"nonfinite snapshot accepted");
    invalid=start;invalid.encounter.health=4;Reject(invalid,live,terrain,LoadPreparationError::InvalidSnapshot,"bad HP snapshot accepted");
    invalid=start;invalid.evasion.direction={};invalid.evasion.remaining=.1f;invalid.evasion.cooldown=1;Reject(invalid,live,terrain,LoadPreparationError::InvalidSnapshot,"bad dash snapshot accepted");
    // 實際控制器可走入邊界內側半徑帶；先重現 x=49.7 存得下卻讀不回，再封住四側回歸。
    const std::array edgeStarts{Vec3{49.4f,0,0},Vec3{-49.4f,0,0},Vec3{0,0,99.4f},Vec3{0,0,-19.4f}};
    const std::array edgeDirections{Vec3{1,0,0},Vec3{-1,0,0},Vec3{0,0,1},Vec3{0,0,-1}};
    for(unsigned side=0;side<edgeStarts.size();++side){
        auto edge=start;auto edgeController=live.controller;auto begin=edgeStarts[side];
        begin.y=terrain.SampleHeight(begin.x,begin.z);edgeController.Reset(begin);
        const auto edgeSolids=WorldSolids(false);edgeController.Step({},edgeSolids,terrain,0);
        edgeController.Step({edgeDirections[side],false,1},edgeSolids,terrain,.05f);
        edge.session.feet=edgeController.Position();
        Require(Length(edge.session.feet-begin)>.2f&&terrain.Contains(edge.session.feet.x,edge.session.feet.z)&&
            edgeController.Grounded()&&edge.Valid(),"actual controller failed to reach valid terrain edge");
        const auto loaded=PrepareChecked(edge,live,terrain);
        Require(bool(loaded),"runtime-reachable saved terrain edge rejected by loader");
        ValidateCandidate(*loaded.candidate,edge,terrain);
        Require(Same(loaded.candidate->controller.Position(),edge.session.feet),"edge load relocated saved feet");
        std::cout<<"REACHABLE_TERRAIN_EDGE side="<<side<<" feet="<<edge.session.feet.x<<','<<edge.session.feet.y<<','<<edge.session.feet.z
            <<" controller_step=1 saved_valid=1 restored_without_relocation=1\n";
    }
    // 中心 x=50 為 Contains 的合法終點；真正場外中心仍不得載入。
    auto outside=start;outside.session.feet={50.01f,0,0};Reject(outside,live,terrain,LoadPreparationError::OutsideTerrain,"center outside terrain accepted");
    auto peak=terrain;Require(peak.SetHeight(101,40,3),"peak fixture write failed");
    Require(peak.SampleHeight(0,0)==0,"peak fixture polluted center instead of footprint edge");
    double peakHighest{};Require(LoadPreparationDetail::Footprint(peak,start.session.feet,.45f,peakHighest)==LoadPreparationError::None,
        "finite peak footprint fixture invalid");
    std::cout<<std::setprecision(12)<<"FINITE_PEAK_BASELINE center="<<peak.SampleHeight(0,0)<<" footprint_highest="<<peakHighest
        <<" saved_feet_y=0 expected_body_top=1.8 precise_terrain_collision_claim=0\n";
    Reject(start,live,peak,LoadPreparationError::TerrainAboveBodyTop,"center-safe body-top crossing finite narrow terrain peak accepted");
    Explorer peakPose=live.explorer;peakPose.phase=0;peakPose.yaw=0;peakPose.planted={};peakPose.feetInitialized=false;
    peakPose.Update({},0,false);
    const bool peakIk=peakPose.PlaceFeet({},true,false,peak,WorldSolids(false));
    std::cout<<"FINITE_PEAK_IK_DIAGNOSTIC applied="<<peakIk<<" clamped_status=not_exposed_by_Explorer precise_contact_claim=0\n";
    Require(peakIk&&LoadPreparationDetail::FinitePose(peakPose.model.skeleton,peakPose.pose),"peak IK diagnostic unexpectedly invalid");
    for(unsigned side=0;side<2;++side){
        const auto target=peakPose.footTargets[side],ankle=TransformPoint(peakPose.pose.globalMatrices[11+side*3],{});
        std::cout<<"FINITE_PEAK_FOOT side="<<side<<" target="<<target.x<<','<<target.y<<','<<target.z
            <<" ankle="<<ankle.x<<','<<ankle.y<<','<<ankle.z<<" error="<<Length(ankle-target)<<'\n';
        Require(std::isfinite(Length(ankle-target)),"peak ankle diagnostic not finite");
    }
    auto corrupt=terrain;Require(corrupt.SetHeight(101,40,(std::numeric_limits<float>::quiet_NaN)()),"corrupt fixture write failed");
    Reject(start,live,corrupt,LoadPreparationError::InvalidTerrain,"nonfinite footprint cell accepted");
    const std::array noCamera{BoxCollider3D{{0,0,0},{1000,1000,1000}}};
    Reject(start,live,terrain,LoadPreparationError::CameraUnavailable,"unavailable full-volume camera accepted",noCamera);
    auto wrongOrbit=live;wrongOrbit.orbit.pitch=(std::numeric_limits<float>::infinity)();
    Reject(start,wrongOrbit,terrain,LoadPreparationError::InvalidOrbit,"invalid orbit accepted");
    auto wrongExplorer=live;wrongExplorer.explorer.model.skeleton.joints.clear();
    Reject(start,wrongExplorer,terrain,LoadPreparationError::InvalidExplorerModel,"invalid Explorer model accepted");
    auto wrongBoar=live;wrongBoar.boar.model.skeleton.joints.clear();
    Reject(start,wrongBoar,terrain,LoadPreparationError::InvalidBoarModel,"invalid Boar model accepted");
    // 檢查點預檢不呼叫閃避 Tick，不消耗保存的動作計時。
    auto active=PrepareChecked(battle,live,terrain);Require(bool(active),"active battle checkpoint rejected");
    const auto& saved=active.candidate->snapshot;
    Require(saved.encounter.health==1&&saved.encounter.phaseTime==battle.encounter.phaseTime&&
        saved.encounter.playerCooldown==battle.encounter.playerCooldown&&saved.evasion.remaining==battle.evasion.remaining&&
        saved.evasion.cooldown==battle.evasion.cooldown&&Same(saved.evasion.direction,battle.evasion.direction),"support probe consumed game/action timers");
    auto expectedAction=battle.evasion,loadedAction=saved.evasion;
    const auto expected=expectedAction.Tick(.02f,true,true,false,{-1,0,0}),loaded=loadedAction.Tick(.02f,true,true,false,{-1,0,0});
    Require(expected.dashing&&loaded.dashing&&Same(expected.movement,loaded.movement)&&loaded.movement.x>0&&
        expectedAction.remaining==loadedAction.remaining&&expectedAction.cooldown==loadedAction.cooldown,"reload bypassed active direction or cooldown");
    ReachableSlopeBaseline(live,terrain);
    std::cout<<"LOAD_PREPARATION_CPU_COVERAGE_PASS checks="<<checks<<" accepted="<<prepared<<" rejected="<<rejections
        <<" semantics=checkpoint_reset_not_exact_motion native_F5_F9_tested=0 audio_GPU_tested=0\n";
    std::cout<<"LOAD_PREPARATION_LIMITATIONS reachable_slope_samples="<<reachableSlopeAccepted
        <<" arbitrary_triangle_body_collision=0 precise_ankle_contact=0 exact_motion_snapshot=0\n";
}
}
int main(){try{Run();return 0;}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
