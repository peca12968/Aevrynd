#pragma once
#include "MistwatchWorld.h"
#include "MistwatchEntranceWorldAuthor.h"
#include "MistwatchSaveGame.h"
#include "MistwatchBoar.h"
#include "MistwatchEncounterMotion.h"
#include "MistwatchCamera.h"
#include "MistwatchExplorer.h"
#include "MistwatchPlayerLocomotion.h"
#include "MistwatchWestCliffRelief.h"
#include "MistwatchRangerHut.h"
#include "MistwatchValleySaveDomain.h"
#include "IndexedValleySaveDomain.h"
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <algorithm>
#include <array>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace Mistwatch {
// 僅預檢 CPU 檢查點：不操作音效、GPU、按鍵邊沿、檔案或執行中的狀態。
// Session1..6 遷移由 SaveGame 負責；本層另驗證目前世界的載入安全。
// 舊檔採明確重設；v3 恢復物理、動作擁有者與動畫方向／相位。
// 足部擺動軌跡與鏡頭路線仍重新建立，不宣稱完整畫面逐位元續接。
enum class LoadPreparationError {
    None, InvalidSnapshot, InvalidControllerSettings, InvalidOrbit, InvalidTerrain,
    OutsideTerrain, BelowTerrainCenter, BodyOverlap, SupportProbeFailed,
    SupportWouldMoveFeet, InvalidExplorerModel, ExplorerPoseFailed,
    InvalidBoarModel, BoarOutsideTerrain, BoarPoseFailed,
    CameraUnavailable, AllocationFailure, PreparationException, TerrainAboveBodyTop, InvalidExtraSolids,
    BoarUnsupported, BoarBodyOverlap, BoarTerrainAboveBodyTop, MotionRestoreRejected, LocomotionRestoreRejected
};
// 衍生地形是另一個明確版本；預設路徑仍維持既有 canonical 驗證。
enum class LoadTerrainProfile { Canonical, WestCliffLedgesV1, WestCliffSlope45V1, WestCliffStanding45V1, ValleyContactV7, ValleyIndexedContactV1 };
struct LoadPreparationInputs {
    const Aevrynd::CharacterController3D& controller;
    const Explorer& explorer;
    const Boar& boar;
    const OrbitCamera& orbit;
};
struct PreparedSavedGame {
    SaveGame snapshot;
    Aevrynd::CharacterController3D controller;
    PlayerLocomotion locomotion;
    Explorer explorer;
    Boar boar;
    OrbitCamera orbit;
    std::vector<Aevrynd::BoxCollider3D> solids;
    std::vector<Aevrynd::BoxCollider3D> enemySolids,cameraSolids;
    Aevrynd::ThirdPersonCameraMotion3D cameraMotion;
    Aevrynd::ThirdPersonFramingResult3D framing;
    Aevrynd::ThirdPersonCameraMotionResult3D cameraResult;
    Aevrynd::Camera3D renderedCamera;
    bool supported{}, explorerIkApplied{}, airbornePoseValidated{}, supportProbeUsed{};
    bool explorerDeathPoseApplied{}, motionRestored{};
    float footprintHighest{}, supportProbeSeconds{}, explorerFootTargetError{};
    PreparedSavedGame(const SaveGame& saved,const LoadPreparationInputs& live)
        :snapshot(saved),controller(live.controller),explorer(live.explorer),boar(live.boar),orbit(live.orbit){}
};
struct LoadPreparationResult {
    LoadPreparationError error{LoadPreparationError::PreparationException};
    std::unique_ptr<PreparedSavedGame> candidate;
    explicit operator bool() const noexcept {return error==LoadPreparationError::None&&bool(candidate);}
};

namespace LoadPreparationDetail {
inline bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b) noexcept {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
inline bool BodyOverlaps(Aevrynd::Vec3 center,Aevrynd::Vec3 extents,
    std::span<const Aevrynd::BoxCollider3D> solids,
    std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls={}) noexcept {
    // 嚴格相交，與控制器零位移 AABB 重疊語意一致。
    for(const auto& box:solids){
        const auto low=box.center-box.extents-extents,high=box.center+box.extents+extents;
        if(center.x>low.x&&center.x<high.x&&center.y>low.y&&center.y<high.y&&center.z>low.z&&center.z<high.z)return true;
    }
    for(const auto* hull:hulls){Aevrynd::ConvexAabbSweep3D::Result query;
        if(!hull||!hull->Sweep(center,extents,{},query)||query.status==Aevrynd::ConvexAabbSweepStatus3D::StartedOverlapping)return true;
    }
    return false;
}
struct TerrainVertex {double x{},z{},y{};};
// 以高度場相同的 a/c/b、b/c/d 三角形精確裁切身體底面。
// 平面在凸多邊形內的最大值落於頂點；檢查每個相關三角形的有限性與最高峰。
// 最高值不作平底接地門檻：原控制器按腳中心接地，雙足 IK 分別調適坡地。
// 另拒絕達到身體頂部的明顯尖峰；此不是任意三角形／完整人體的地形碰撞解算。
inline double ClippedHeight(std::array<TerrainVertex,3> triangle,double minX,double maxX,double minZ,double maxZ) {
    std::array<TerrainVertex,12> polygon{},next{};
    std::copy(triangle.begin(),triangle.end(),polygon.begin());std::size_t count=3;
    for(unsigned plane=0;plane<4;++plane){
        const bool xAxis=plane<2,lower=plane==0||plane==2;
        const double edge=plane==0?minX:plane==1?maxX:plane==2?minZ:maxZ;
        const auto coordinate=[&](TerrainVertex p){return xAxis?p.x:p.z;};
        const auto inside=[&](TerrainVertex p){return lower?coordinate(p)>=edge:coordinate(p)<=edge;};
        std::size_t size{};
        if(!count)return -(std::numeric_limits<double>::infinity)();
        auto prior=polygon[count-1];bool priorInside=inside(prior);
        for(std::size_t i=0;i<count;++i){
            const auto current=polygon[i];const bool currentInside=inside(current);
            if(currentInside!=priorInside){
                const double t=(edge-coordinate(prior))/(coordinate(current)-coordinate(prior));
                next[size++]={prior.x+(current.x-prior.x)*t,prior.z+(current.z-prior.z)*t,prior.y+(current.y-prior.y)*t};
            }
            if(currentInside)next[size++]=current;
            prior=current;priorInside=currentInside;
        }
        polygon=next;count=size;
    }
    double highest=-(std::numeric_limits<double>::infinity)();
    for(std::size_t i=0;i<count;++i)highest=(std::max)(highest,polygon[i].y);
    return highest;
}
inline LoadPreparationError Footprint(const Aevrynd::HeightField3D& terrain,Aevrynd::Vec3 feet,
    float radius,double& highest) {
    const auto& grid=terrain.Settings();
    if(!Session::Finite(grid.origin)||grid.width<2||grid.depth<2||!std::isfinite(grid.cellSize)||grid.cellSize<=0||
        terrain.SampleCount()!=std::size_t(grid.width)*grid.depth)return LoadPreparationError::InvalidTerrain;
    const double endX=double(grid.origin.x)+double(grid.width-1)*grid.cellSize,endZ=double(grid.origin.z)+double(grid.depth-1)*grid.cellSize;
    // 原控制器以腳中心 Contains 判定地形支撐，合法邊界帶不能存得下卻讀不回。
    // 中心越界仍拒絕；底面僅裁切已知高度場的交集，不捏造場外高度或移動保存位置。
    if(!terrain.Contains(feet.x,feet.z))return LoadPreparationError::OutsideTerrain;
    const double minX=(std::max)(double(feet.x)-radius,double(grid.origin.x));
    const double maxX=(std::min)(double(feet.x)+radius,endX);
    const double minZ=(std::max)(double(feet.z)-radius,double(grid.origin.z));
    const double maxZ=(std::min)(double(feet.z)+radius,endZ);
    const auto cell=[&](double p,float origin,unsigned count){
        return static_cast<unsigned>((std::min)(std::floor((p-origin)/grid.cellSize),double(count-2)));
    };
    const unsigned x0=cell(minX,grid.origin.x,grid.width),x1=cell(maxX,grid.origin.x,grid.width);
    const unsigned z0=cell(minZ,grid.origin.z,grid.depth),z1=cell(maxZ,grid.origin.z,grid.depth);
    if(std::size_t(x1-x0+1)*(z1-z0+1)>4096)return LoadPreparationError::InvalidTerrain;
    highest=-(std::numeric_limits<double>::infinity)();
    for(unsigned z=z0;z<=z1;++z)for(unsigned x=x0;x<=x1;++x){
        const double a=terrain.Height(x,z),b=terrain.Height(x+1,z),c=terrain.Height(x,z+1),d=terrain.Height(x+1,z+1);
        if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c)||!std::isfinite(d))return LoadPreparationError::InvalidTerrain;
        const double left=double(grid.origin.x)+double(x)*grid.cellSize,back=double(grid.origin.z)+double(z)*grid.cellSize;
        const TerrainVertex va{left,back,a},vb{left+grid.cellSize,back,b},vc{left,back+grid.cellSize,c},vd{left+grid.cellSize,back+grid.cellSize,d};
        highest=(std::max)({highest,ClippedHeight({va,vc,vb},minX,maxX,minZ,maxZ),ClippedHeight({vb,vc,vd},minX,maxX,minZ,maxZ)});
    }
    return std::isfinite(highest)?LoadPreparationError::None:LoadPreparationError::InvalidTerrain;
}
inline bool FinitePose(const Aevrynd::Skeleton3D& skeleton,const Aevrynd::AnimationPose3D& pose) {
    if(pose.localTransforms.size()!=skeleton.joints.size()||pose.globalMatrices.size()!=skeleton.joints.size())return false;
    for(const auto& matrix:pose.globalMatrices)for(const auto& row:matrix.values)for(float value:row)if(!std::isfinite(value))return false;
    return true;
}
inline bool ValidSettings(const Aevrynd::CharacterControllerSettings3D& s) {
    return std::isfinite(s.radius)&&s.radius>0&&std::isfinite(s.height)&&s.height>=.6f&&s.height<=3&&
        std::isfinite(s.moveSpeed)&&s.moveSpeed>0&&std::isfinite(s.jumpSpeed)&&s.jumpSpeed>=0&&
        std::isfinite(s.gravity)&&s.gravity>0&&std::isfinite(s.maximumFallSpeed)&&s.maximumFallSpeed>0&&
        std::isfinite(s.groundSnapDistance)&&s.groundSnapDistance>=0&&std::isfinite(s.maximumDirectDisplacement)&&s.maximumDirectDisplacement>0&&
        s.maximumSlideIterations>=1&&s.maximumSlideIterations<=3&&
        std::isfinite(s.maximumTerrainUphillGrade)&&s.maximumTerrainUphillGrade>=0&&s.maximumTerrainUphillGrade<=16&&
        std::isfinite(s.maximumTerrainStandingGrade)&&s.maximumTerrainStandingGrade>=0&&s.maximumTerrainStandingGrade<=16;
}
}

inline LoadPreparationResult PrepareSavedGame(const SaveGame& saved,const LoadPreparationInputs& live,
    const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> extraCameraOnly,
    std::span<const Aevrynd::BoxCollider3D> extraSolids,FirLayoutSelector firLayout,
    FallenLogProfile fallenProfile,const EntranceWorldAuthor* authored=nullptr,
    std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls={},
    LoadTerrainProfile terrainProfile=LoadTerrainProfile::Canonical,const ValleySaveDomain* valley=nullptr,
    const IndexedValleySaveDomain* indexed=nullptr) noexcept {
    using namespace Aevrynd;
    using namespace LoadPreparationDetail;
    const auto fail=[](LoadPreparationError error){return LoadPreparationResult{error,{}};};
    try {
        ValidateFallenLogRoute(firLayout,fallenProfile);
        if(indexed&&(authored||valley||!extraSolids.empty()))return fail(LoadPreparationError::InvalidExtraSolids);
        if((terrainProfile==LoadTerrainProfile::ValleyIndexedContactV1)!=(indexed!=nullptr))return fail(LoadPreparationError::InvalidTerrain);
        if(terrainProfile==LoadTerrainProfile::ValleyIndexedContactV1){
            if(!indexed->MatchesTerrain(terrain)||!indexed->MatchesAttachments(hulls,extraCameraOnly)||!saved.worldIdentity||*saved.worldIdentity!=indexed->Identity()||
                firLayout!=FirLayoutSelector::ForestWood||fallenProfile!=FallenLogProfile::ScannedStraightV1)return fail(LoadPreparationError::InvalidTerrain);
        }else if(terrainProfile==LoadTerrainProfile::ValleyContactV7){
            if(!valley||!valley->MatchesTerrain(terrain)||!valley->MatchesAttachments(hulls,extraCameraOnly)||!saved.worldIdentity||*saved.worldIdentity!=valley->Identity()||
                firLayout!=FirLayoutSelector::ForestWood||fallenProfile!=FallenLogProfile::ScannedStraightV1)
                return fail(LoadPreparationError::InvalidTerrain);
        }else if(saved.worldIdentity)return fail(LoadPreparationError::InvalidTerrain);
        else if(terrainProfile==LoadTerrainProfile::Canonical){
            if(fallenProfile==FallenLogProfile::ScannedStraightV1)ValidateFallenLogTerrain(terrain);
        }else if(terrainProfile==LoadTerrainProfile::WestCliffLedgesV1||terrainProfile==LoadTerrainProfile::WestCliffSlope45V1||
            terrainProfile==LoadTerrainProfile::WestCliffStanding45V1){
            if(terrainProfile==LoadTerrainProfile::WestCliffSlope45V1&&
                (live.controller.Settings().maximumTerrainUphillGrade!=1.f||live.controller.Settings().maximumTerrainStandingGrade!=0.f))
                return fail(LoadPreparationError::InvalidControllerSettings);
            if(terrainProfile==LoadTerrainProfile::WestCliffStanding45V1&&
                (live.controller.Settings().maximumTerrainUphillGrade!=1.f||live.controller.Settings().maximumTerrainStandingGrade!=1.f))
                return fail(LoadPreparationError::InvalidControllerSettings);
            if(firLayout!=FirLayoutSelector::ForestWood||fallenProfile!=FallenLogProfile::ScannedStraightV1)
                return fail(LoadPreparationError::InvalidTerrain);
            // 重新構造整張已知地形、逐位元核對；以下支撐／碰撞只能使用實際衍生場。
            try { WestCliffRelief::ValidateExact(terrain); }
            catch(const std::invalid_argument&) {return fail(LoadPreparationError::InvalidTerrain);}
        }else return fail(LoadPreparationError::InvalidTerrain);
        if(!saved.Valid())return fail(LoadPreparationError::InvalidSnapshot);
        // 附加世界物件必須參與整個預檢及提交，不得讀檔後只保留渲染而遺失碰撞。
        for(const auto& box:extraSolids)if(!Session::Finite(box.center)||!Session::Finite(box.extents)||
            box.extents.x<=0||box.extents.y<=0||box.extents.z<=0)return fail(LoadPreparationError::InvalidExtraSolids);
        for(const auto* hull:hulls)if(!hull||!hull->AxisCount())return fail(LoadPreparationError::InvalidExtraSolids);
        if(!ValidSettings(live.controller.Settings()))return fail(LoadPreparationError::InvalidControllerSettings);
        if(!std::isfinite(live.orbit.yaw)||!std::isfinite(live.orbit.pitch)||!std::isfinite(live.orbit.distance)||
            live.orbit.pitch<-.15f||live.orbit.pitch>1.1f||live.orbit.distance<2.5f||live.orbit.distance>10)return fail(LoadPreparationError::InvalidOrbit);
        if(!live.explorer.model.Valid()||live.explorer.model.skeleton.joints.size()!=15)return fail(LoadPreparationError::InvalidExplorerModel);
        if(!live.boar.model.Valid()||live.boar.model.skeleton.joints.size()!=16)return fail(LoadPreparationError::InvalidBoarModel);
        auto candidate=std::make_unique<PreparedSavedGame>(saved,live);
        auto& p=*candidate;
        if(authored&&(!authored->applied||firLayout!=FirLayoutSelector::ForestWood||
            authored->fallenProfile!=fallenProfile))return fail(LoadPreparationError::PreparationException);
        if(indexed){
            if(!indexed->Body(saved.session.bridgeRepaired,saved.session.feet,p.solids)||
                !indexed->Body(saved.session.bridgeRepaired,saved.encounter.feet,p.enemySolids)||
                !indexed->Camera(saved.session.bridgeRepaired,saved.session.feet,p.orbit.distance,p.cameraSolids))return fail(LoadPreparationError::InvalidExtraSolids);
        }else{
        p.solids=authored?authored->WorldSolids(saved.session.bridgeRepaired):
            WorldSolids(saved.session.bridgeRepaired,firLayout,fallenProfile);
        p.solids.insert(p.solids.end(),extraSolids.begin(),extraSolids.end());
        if(terrainProfile==LoadTerrainProfile::ValleyContactV7&&!valley->MatchesWorld(saved.session.bridgeRepaired,p.solids))
            return fail(LoadPreparationError::InvalidExtraSolids);
        }
        if(saved.motion){
            // 不積分重力、不先落地再重跳。引擎以保存的真實體積／支撐驗證並恢復速度。
            double highest{};
            const auto terrainError=Footprint(terrain,saved.session.feet,p.controller.Settings().radius,highest);
            if(terrainError!=LoadPreparationError::None)return fail(terrainError);
            p.footprintHighest=static_cast<float>(highest);
            if(!p.controller.TryRestoreMotionState(saved.motion->controller,p.solids,hulls,terrain))
                return fail(LoadPreparationError::MotionRestoreRejected);
            if(!Same(p.controller.Position(),saved.session.feet))return fail(LoadPreparationError::SupportWouldMoveFeet);
            if(!p.locomotion.TryRestoreMotionState(saved.motion->locomotion,p.controller,saved.evasion))
                return fail(LoadPreparationError::LocomotionRestoreRejected);
            p.motionRestored=true;
        }else{
        p.controller.Reset(saved.session.feet);
        if(!p.controller.SetHeight(1.4f))return fail(LoadPreparationError::InvalidControllerSettings);
        double highest{};
        const auto terrainError=Footprint(terrain,saved.session.feet,p.controller.Settings().radius,highest);
        if(terrainError!=LoadPreparationError::None)return fail(terrainError);
        p.footprintHighest=static_cast<float>(highest);
        const float ground=terrain.SampleHeight(saved.session.feet.x,saved.session.feet.z);
        if(!std::isfinite(ground))return fail(LoadPreparationError::InvalidTerrain);
        if(saved.session.feet.y<ground)return fail(LoadPreparationError::BelowTerrainCenter);
        if(BodyOverlaps(p.controller.Center(),p.controller.Extents(),p.solids,hulls))return fail(LoadPreparationError::BodyOverlap);
        // 無法站起可保留蹲姿；不得因此忽略完整體積安全。
        p.controller.TrySetHeight(1.8f,p.solids,hulls);
        if(BodyOverlaps(p.controller.Center(),p.controller.Extents(),p.solids,hulls))return fail(LoadPreparationError::BodyOverlap);
        if(highest>=double(saved.session.feet.y)+p.controller.Settings().height)return fail(LoadPreparationError::TerrainAboveBodyTop);
        bool contact=saved.session.feet.y==ground;
        for(const auto& box:p.solids){
            const float top=box.center.y+box.extents.y;
            contact|=saved.session.feet.y==top&&std::abs(saved.session.feet.x-box.center.x)<p.controller.Settings().radius+box.extents.x&&
                std::abs(saved.session.feet.z-box.center.z)<p.controller.Settings().radius+box.extents.z;
        }
        if(!hulls.empty()){
            // 凸形支撐包含保存的0.1mm skin；零時間只重建0.2mm內的支撐，不能用重力步挪動存檔。
            p.supportProbeUsed=true;
            if(!p.controller.Step({},p.solids,hulls,terrain,0))return fail(LoadPreparationError::SupportProbeFailed);
            if(!Same(p.controller.Position(),saved.session.feet))return fail(LoadPreparationError::SupportWouldMoveFeet);
            if((contact&&!p.controller.Grounded())||!Same(p.controller.Velocity(),{}))return fail(LoadPreparationError::SupportProbeFailed);
        }else if(contact){
            // 地形同高先使用原控制器零時間步；橋面支撐無法由零時間建立才做有限重力步。
            // 兩者均只改候選控制器；不推進 Session／Encounter／Evasion，腳座標必須完全不變。
            p.supportProbeUsed=true;
            p.controller.Step({},p.solids,terrain,0);
            if(!Same(p.controller.Position(),saved.session.feet))return fail(LoadPreparationError::SupportWouldMoveFeet);
            if(!p.controller.Grounded()){
                p.supportProbeSeconds=1.f/60.f;
                p.controller.Step({},p.solids,terrain,p.supportProbeSeconds);
            }
            if(!Same(p.controller.Position(),saved.session.feet))return fail(LoadPreparationError::SupportWouldMoveFeet);
            if(!p.controller.Grounded()||!Same(p.controller.Velocity(),{}))return fail(LoadPreparationError::SupportProbeFailed);
        }
        }
        p.supported=p.controller.Grounded();
        const bool crouched=p.controller.Settings().height<1.5f;
        p.explorer.ResetDeath();
        p.explorer.phase=0;p.explorer.yaw=0;p.explorer.ResetLocomotion();
        if(saved.motion){
            p.explorer.phase=saved.motion->explorerPhase;p.explorer.yaw=saved.motion->explorerYaw;
            p.explorer.requestedYaw=saved.motion->explorerRequestedYaw;
            p.explorer.locomotionDistance=saved.motion->explorerDistance;
            p.explorer.locomotionSeconds=saved.motion->explorerSeconds;
            p.explorer.locomotionVelocity=saved.motion->locomotion.motor.velocity;
            // 僅重建原有肢體旋轉，不以虛構位移推進相位或轉向；隨後仍驗證真實靴底淨空。
            p.explorer.RebuildLocomotionPose(crouched);
        }else p.explorer.Update({},0,crouched);
        if(p.supported){
            if(!p.explorer.PlaceFeet(saved.session.feet,true,crouched,terrain,p.solids,hulls))return fail(LoadPreparationError::ExplorerPoseFailed);
            // 既有求解器 Clamped 仍回傳成功；旗標僅代表實際套用 IK，不保證踝端點誤差為零。
            p.explorerIkApplied=true;
            const auto toWorld=CreateTransformMatrix(saved.session.feet,{0,p.explorer.yaw,0},{1,1,1});
            for(unsigned side=0;side<2;++side){
                const auto ankle=TransformPoint(toWorld,TransformPoint(p.explorer.pose.globalMatrices[11+side*3],{}));
                const float error=Length(ankle-p.explorer.footTargets[side]);
                if(!std::isfinite(error))return fail(LoadPreparationError::ExplorerPoseFailed);
                p.explorerFootTargetError=(std::max)(p.explorerFootTargetError,error);
            }
        } else {
            // 空中也執行真正的鞋底淨空檢查；不建立接地旗標或移動存檔根位置。
            if(!p.explorer.PlaceFeet(saved.session.feet,false,crouched,terrain,p.solids,hulls))return fail(LoadPreparationError::ExplorerPoseFailed);
            p.airbornePoseValidated=true;
        }
        if(!saved.session.Alive()){
            // 檢查點未保存死亡動畫時鐘，明確重建終點；不改變保存的世界位置與任務。
            if(!p.explorer.UpdateDeath(Explorer::DeathDuration,saved.session.feet,terrain,p.solids)||
               (!p.explorer.DeathFinished()&&!p.explorer.DeathLimited()))
                return fail(LoadPreparationError::ExplorerPoseFailed);
            p.explorerDeathPoseApplied=true;
            p.explorerIkApplied=false;
        }
        if(!FinitePose(p.explorer.model.skeleton,p.explorer.pose))return fail(LoadPreparationError::ExplorerPoseFailed);
        double enemyTerrain{};
        const auto enemyTerrainError=Footprint(terrain,saved.encounter.feet,EncounterBodyRadius,enemyTerrain);
        if(enemyTerrainError==LoadPreparationError::OutsideTerrain)return fail(LoadPreparationError::BoarOutsideTerrain);
        if(enemyTerrainError!=LoadPreparationError::None)return fail(enemyTerrainError);
        if(!EncounterSupported(saved.encounter.feet,terrain))return fail(LoadPreparationError::BoarUnsupported);
        const std::span<const BoxCollider3D> enemySolids=indexed?std::span<const BoxCollider3D>{p.enemySolids}:std::span<const BoxCollider3D>{p.solids};
        if(EncounterBodyOverlaps(saved.encounter.feet,enemySolids))return fail(LoadPreparationError::BoarBodyOverlap);
        if(enemyTerrain>=double(saved.encounter.feet.y)+EncounterBodyBottom+EncounterBodyHeight)
            return fail(LoadPreparationError::BoarTerrainAboveBodyTop);
        p.boar.initialized=false;p.boar.cycle=0;p.boar.planted={};p.boar.footTargets={};p.boar.lastFeet={};
        const auto look=(saved.encounter.phase==EnemyPhase::Windup?saved.encounter.strikeTarget:saved.session.feet)-saved.encounter.feet;
        if(!p.boar.Update({},0,saved.encounter.phase,(std::max)(0.f,saved.encounter.phaseTime),look)||
            !p.boar.PlaceFeet(saved.encounter.feet,terrain)||!FinitePose(p.boar.model.skeleton,p.boar.pose))return fail(LoadPreparationError::BoarPoseFailed);
        const auto enemyWorld=CreateTransformMatrix(saved.encounter.feet,{0,p.boar.yaw,0},{1,1,1});
        for(unsigned leg=0;leg<4;++leg){
            const auto ankle=TransformPoint(enemyWorld*p.boar.pose.globalMatrices[5+leg*3],{});
            const float error=Length(ankle-p.boar.footTargets[leg]);
            if(!std::isfinite(error)||error>.005f)return fail(LoadPreparationError::BoarPoseFailed);
        }
        std::vector<BoxCollider3D> cameraOnly{RangerHutCameraRoofSolid()};
        cameraOnly.insert(cameraOnly.end(),extraCameraOnly.begin(),extraCameraOnly.end());
        p.cameraMotion.Reset();
        const std::span<const BoxCollider3D> cameraSolids=indexed?std::span<const BoxCollider3D>{p.cameraSolids}:std::span<const BoxCollider3D>{p.solids};
        p.framing=BuildPlayerCameraFraming(saved.session.feet,p.controller.Settings().height,p.orbit,cameraSolids,terrain,cameraOnly);
        p.cameraResult=p.cameraMotion.Update(p.framing.camera,PlayerCameraSamples(saved.session.feet,p.controller.Settings().height),
            cameraSolids,CameraCeilingSolids(),terrain,1.f/60.f,cameraOnly);
        if(!p.cameraResult.accepted||!p.cameraResult.subjectVisible||p.cameraResult.status!=ThirdPersonCameraMotionStatus3D::ResetCut)
            return fail(LoadPreparationError::CameraUnavailable);
        p.orbit.yaw=PlayerCameraMovementYaw(p.cameraResult.camera,p.orbit.yaw);
        p.renderedCamera=p.cameraResult.camera;
        return {LoadPreparationError::None,std::move(candidate)};
    } catch(const std::bad_alloc&) {return fail(LoadPreparationError::AllocationFailure);}
      catch(...) {return fail(LoadPreparationError::PreparationException);}
}

// 新模式只接受唯一完整世界來源，不收會被忽略的舊extras或author。
inline LoadPreparationResult PrepareIndexedSavedGame(const SaveGame& saved,const LoadPreparationInputs& live,
    const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> extraCameraOnly,
    std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,FirLayoutSelector firLayout,
    FallenLogProfile fallenProfile,const IndexedValleySaveDomain& source) noexcept {
    return PrepareSavedGame(saved,live,terrain,extraCameraOnly,{},firLayout,fallenProfile,nullptr,hulls,
        LoadTerrainProfile::ValleyIndexedContactV1,nullptr,&source);
}

inline LoadPreparationResult PrepareSavedGame(const SaveGame& saved,const LoadPreparationInputs& live,
    const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> extraCameraOnly,
    std::span<const Aevrynd::BoxCollider3D> extraSolids,FirLayoutSelector firLayout) noexcept {
    return PrepareSavedGame(saved,live,terrain,extraCameraOnly,extraSolids,firLayout,FallenLogProfile::LegacyCylinder);
}

inline LoadPreparationResult PrepareSavedGame(const SaveGame& saved,const LoadPreparationInputs& live,
    const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> extraCameraOnly={},
    std::span<const Aevrynd::BoxCollider3D> extraSolids={}) noexcept {
    return PrepareSavedGame(saved,live,terrain,extraCameraOnly,extraSolids,FirLayoutSelector::Legacy);
}

struct SavedGameCommitTargets {
    Session& session;Encounter& encounter;Evasion& evasion;
    Aevrynd::CharacterController3D& controller;Explorer& explorer;Boar& boar;OrbitCamera& orbit;
    std::vector<Aevrynd::BoxCollider3D>& solids;
    Aevrynd::ThirdPersonCameraMotion3D& cameraMotion;
    Aevrynd::ThirdPersonFramingResult3D& framing;
    Aevrynd::ThirdPersonCameraMotionResult3D& cameraResult;
    Aevrynd::Camera3D& renderedCamera;
    // 舊 CPU 呼叫可省略；正式遊戲必須提供，讓物理與動作擁有者同時交換。
    PlayerLocomotion* locomotion{};
};
// 僅成功的 PrepareSavedGame 候選可進入單執行緒、無失敗點的提交。
// 交換後 p 保留舊 CPU 狀態，可立即再次交換回滾；診斷旗標仍描述首次預檢，不描述舊狀態。
// 交換期間及要求回滾之前，不得執行模擬、音效或 GPU 回呼。
// 主代理另處理訊息、resetTick、鏡頭統計及提交後音效觀測器初始化。
inline void SwapPreparedSavedGame(PreparedSavedGame& p,SavedGameCommitTargets live) noexcept {
    static_assert(std::is_nothrow_swappable_v<Session> && std::is_nothrow_swappable_v<Encounter> && std::is_nothrow_swappable_v<Evasion>);
    static_assert(std::is_nothrow_swappable_v<Aevrynd::CharacterController3D>);
    static_assert(std::is_nothrow_swappable_v<PlayerLocomotion>);
    static_assert(std::is_nothrow_swappable_v<Explorer> && std::is_nothrow_swappable_v<Boar>,"CPU models must support noexcept swap; otherwise no transactional integration claim");
    static_assert(std::is_nothrow_swappable_v<OrbitCamera> && std::is_nothrow_swappable_v<std::vector<Aevrynd::BoxCollider3D>>);
    static_assert(std::is_nothrow_swappable_v<Aevrynd::ThirdPersonCameraMotion3D> && std::is_nothrow_swappable_v<Aevrynd::ThirdPersonFramingResult3D> &&
        std::is_nothrow_swappable_v<Aevrynd::ThirdPersonCameraMotionResult3D> && std::is_nothrow_swappable_v<Aevrynd::Camera3D>);
    using std::swap;
    swap(p.snapshot.session,live.session);swap(p.snapshot.encounter,live.encounter);swap(p.snapshot.evasion,live.evasion);
    swap(p.controller,live.controller);swap(p.explorer,live.explorer);swap(p.boar,live.boar);swap(p.orbit,live.orbit);
    swap(p.solids,live.solids);swap(p.cameraMotion,live.cameraMotion);swap(p.framing,live.framing);
    swap(p.cameraResult,live.cameraResult);swap(p.renderedCamera,live.renderedCamera);
    if(live.locomotion)swap(p.locomotion,*live.locomotion);
}
}
