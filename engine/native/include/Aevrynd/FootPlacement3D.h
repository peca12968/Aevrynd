#pragma once
#include "CollisionWorld3D.h"
#include "HeightField3D.h"
#include "SkeletalIk3D.h"
#include "SkeletonSemantics3D.h"
#include <cstdint>
#include <span>

namespace Aevrynd {
enum class FootPlacementLod3D : std::uint8_t {Full,Reduced,Disabled};
enum class FootContactSource3D : std::uint8_t {None,HeightField,Collider};

struct FootPlacementFootProfile3D {
    // local sole 軸由資產 profile 明確提供，不猜測匯入骨架的座標慣例。
    Vec3 soleUpLocal{0,1,0},soleForwardLocal{0,0,1};
    float soleOffset{.08f};
};

struct MovingPlatformSample3D {
    ColliderHandle3D collider{};
    // userData 必須非零且代表 collider incarnation；handle 與 userData 必須同時相符。
    std::uint64_t userData{};
    Matrix4 previousLocalToWorld{Matrix4::Identity()},previousWorldToLocal{Matrix4::Identity()};
    Matrix4 currentLocalToWorld{Matrix4::Identity()},currentWorldToLocal{Matrix4::Identity()};
};

struct FootPlacementSettings3D {
    float probeUp{.3f},probeDown{.8f},probeRadius{.06f};
    FootPlacementFootProfile3D feet[2]{};
    float minimumNormalY{.25f},maximumPelvisDrop{.35f};
    float plantSpeed{20.f},releaseSpeed{8.f},pelvisSpeed{14.f};
    float fullLodDistance{30.f},reducedLodDistance{70.f};
    std::uint32_t reducedQueryInterval{4};
    CollisionQueryFilter3D collisionFilter{};
};

struct FootPlacementFrame3D {
    const CollisionWorld3D* collisionWorld{};
    const HeightField3D* heightField{};
    std::span<const MovingPlatformSample3D> movingPlatforms{};
    // poseRoot 建立 pose globals；modelToWorld/worldToModel 只負責 model 與 world query 的轉換。
    Matrix4 poseRoot{Matrix4::Identity()},modelToWorld{Matrix4::Identity()},worldToModel{Matrix4::Identity()};
    float deltaTime{1.f/60.f},cameraDistance{};
    std::uint64_t frameIndex{};
};

struct FootContactResult3D {
    FootContactSource3D source{FootContactSource3D::None};
    ColliderHandle3D collider{};
    std::uint64_t userData{};
    Vec3 contactWorld{},targetWorld{},normalWorld{0,1,0};
    float weight{};
};

struct FootPlacementResult3D {
    FootPlacementLod3D lod{FootPlacementLod3D::Disabled};
    FootContactResult3D left{},right{};
    float pelvisOffsetWorld{};
    std::uint32_t ikSolves{};
    bool applied{};
};

class FootPlacement3D {
public:
    bool Configure(FootPlacementSettings3D settings)noexcept;
    // Prepare 是唯一允許配置 scratch 的階段；失敗時保留既有 prepared 狀態。
    bool Prepare(const Skeleton3D& skeleton,const HumanoidSkeletonMap3D& map,SkeletonCompatibilityId3D expectedCompatibility);
    // Evaluate 的 pose、內部雙腳/骨盆 state 與 output 共用單一 transaction；任一失敗全部不變。
    bool Evaluate(const Skeleton3D& skeleton,const HumanoidSkeletonMap3D& map,const FootPlacementFrame3D& frame,
        AnimationPose3D& pose,FootPlacementResult3D* output=nullptr)noexcept;
    void Reset()noexcept;
    std::size_t TrimUnusedMemory();
    [[nodiscard]] ReusableMemoryStats MemoryStats()const noexcept;
    [[nodiscard]] const FootPlacementSettings3D& Settings()const noexcept{return settings_;}
private:
    struct FootState {
        FootContactSource3D source{FootContactSource3D::None};ColliderHandle3D collider{};std::uint64_t userData{};
        Vec3 contactWorld{},targetWorld{},normalWorld{0,1,0};float weight{};bool movingPlatform{};
    };
    FootPlacementSettings3D settings_{};
    AnimationPose3D scratchPose_{};
    FootState left_{},right_{};
    float pelvisOffsetWorld_{};
    SkeletonCompatibilityId3D compatibility_{};
    std::size_t jointCount_{},lastReleasedBytes_{};
    std::uint64_t trimCount_{};
};
}
