#pragma once
#include "MistwatchPlayerLocomotion.h"
#include "MistwatchLoadPreparation.h"

namespace Mistwatch {
enum class BridgeRepairPlayerReason {Accepted,BodyOverlap,CameraBlocked,PoseRejected,PreparationUnavailable};
struct BridgeRepairPlayerResult {
    BridgeRepairPlayerReason reason{BridgeRepairPlayerReason::PreparationUnavailable};
    explicit operator bool()const noexcept{return reason==BridgeRepairPlayerReason::Accepted;}
};

// 僅預檢修橋新增的世界；不修改Session、輸入、控制器、NPC或世界清單。
// 成功留下新世界的真腳部姿態，失敗只還原修橋前已合法的可寫角色姿態。
inline BridgeRepairPlayerResult PrepareBridgeRepairPlayer(const Aevrynd::CharacterController3D& controller,
    Explorer& explorer,std::span<const Aevrynd::BoxCollider3D> candidateWorld,
    std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,const Aevrynd::HeightField3D& terrain,
    const Aevrynd::Camera3D& camera,std::span<const Aevrynd::BoxCollider3D> cameraOnly={})noexcept {
    try {
        if(LoadPreparationDetail::BodyOverlaps(controller.Center(),controller.Extents(),candidateWorld,hulls))
            return {BridgeRepairPlayerReason::BodyOverlap};
        if(!Aevrynd::ThirdPersonCameraPathClear3D(camera.position,camera.position,candidateWorld,terrain,cameraOnly,.16f))
            return {BridgeRepairPlayerReason::CameraBlocked};
    }catch(...){return {BridgeRepairPlayerReason::PreparationUnavailable};}
    try {
        // 現有馬達的姿態快照涵蓋PlaceFeet的全部可寫欄位，不依賴Pair或整幀Guard。
        PlayerLocomotionPoseState previous(explorer);
        try {
            const bool crouched=controller.Settings().height<1.5f;
            // 以原步幅重建此刻基礎姿態，不從已解IK的骨盆再壓低一次。
            explorer.RebuildLocomotionPose(crouched);
            if(explorer.PlaceFeet(controller.Position(),controller.Grounded(),crouched,
                terrain,candidateWorld,hulls,nullptr,nullptr,ExplorerFootPlacementMode::RevalidateCurrent))
                return {BridgeRepairPlayerReason::Accepted};
        }catch(...){/* 真IK拒絕與例外均只撤銷此次修橋，不吞掉其他合法E互動。 */}
        previous.Restore(explorer);return {BridgeRepairPlayerReason::PoseRejected};
    }catch(...){return {BridgeRepairPlayerReason::PreparationUnavailable};}
}
#include "IndexedBridgeRepair.inl"

}
