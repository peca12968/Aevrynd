#pragma once
#include <Aevrynd/CharacterController3D.h>

namespace Aevrynd {
enum class ThirdPersonCameraMotionStatus3D {TrackingVisible,TrackingOccluded,Held,ResetCut,Paused,InvalidInput,Blocked};
struct ThirdPersonCameraMotionResult3D {
    Camera3D camera{};
    ThirdPersonCameraMotionStatus3D status{ThirdPersonCameraMotionStatus3D::Blocked};
    float displacement{};
    bool accepted{},subjectVisible{};
};
// 以擴張碰撞盒與相交高度場格子的四角高度上界，保守驗證完整鏡頭體積路徑。
// 每次至多檢查 4096 格；超出或非法資料一律拒絕，不以抽樣漏過窄峰。
bool ThirdPersonCameraPathClear3D(Vec3 start,Vec3 end,std::span<const BoxCollider3D> solids,
    const HeightField3D& terrain,std::span<const BoxCollider3D> cameraOnly={},float radius=.12f);
class ThirdPersonCameraMotion3D {
public:
    void Reset()noexcept{initialized_=false;routeCount_=routeIndex_=0;routeDirection_=0;occlusionRecovery_=false;occlusionSeconds_=retrySeconds_=0;requestInitialized_=false;poseQuietSeconds_=0;}
    bool Initialized()const noexcept{return initialized_;}
    const Camera3D& Camera()const noexcept{return camera_;}
    // 每次固定更新只呼叫一次；繪圖只讀 Camera，不得重複消耗 dt。
    // 遮擋是可回報的過渡狀態，不等同相機穿模。Blocked/InvalidInput 不提交新相機。
    // 超過 2 公尺的目標瞬移必須由呼叫者明確 Reset；不以自動切鏡掩蓋突跳。
    // 投影與角色端點以 1280x720 驗證；鏡頭安全半徑至少包住此投影的近裁切面。
    // desired 保留玩家原始轉向；安全起始相機只用於 Reset 後第一個明確切鏡。
    ThirdPersonCameraMotionResult3D Update(Camera3D desired,std::span<const Vec3> subjectSamples,
        std::span<const BoxCollider3D> solids,std::span<const BoxCollider3D> ceilings,
        const HeightField3D& terrain,float dt,std::span<const BoxCollider3D> cameraOnly={});
private:
    Camera3D camera_{};
    bool initialized_{};
    std::array<Vec3,81> route_{};
    unsigned routeCount_{},routeIndex_{};
    float routeDirection_{};
    float routeWantedPitch_{},routeWantedRange_{};
    // 遮擋恢復與玩家轉向路線分開辨識，不以目前鏡頭 yaw 誤認新輸入。
    // 接受的原始請求姿態變化擁有一秒手動優先權；不把抵達目標當成停止輸入。
    bool requestInitialized_{};
    float requestedYaw_{},requestedPitch_{},requestedRange_{},poseQuietSeconds_{};
    bool occlusionRecovery_{};
    float recoveryRequestedYaw_{},occlusionSeconds_{},retrySeconds_{};
};
}
