#pragma once
#include <Aevrynd/Animation3D.h>
#include <cstddef>
#include <cstdint>

namespace Aevrynd {
struct TwoBoneIkChain3D {std::size_t upper{},middle{},end{};};
struct TwoBoneIkTarget3D {
    // positionModel 與 poleModel 和 pose.globalMatrices 位於同一模型空間，該空間已包含 poseRoot。
    Vec3 positionModel{},poleModel{};
    float weight{1.f};
    // softness 是完整鏈長的比例；0 代表硬限制。
    float softness{};
};
enum class TwoBoneIkStatus3D : std::uint8_t {Solved,Clamped,InvalidSkeleton,InvalidChain,InvalidPose,InvalidTarget,Degenerate};
struct TwoBoneIkResult3D {
    TwoBoneIkStatus3D status{TwoBoneIkStatus3D::InvalidPose};
    Vec3 solvedPositionModel{},bendNormalModel{};
    float endError{};
    bool clamped{};
    explicit constexpr operator bool()const noexcept{return status==TwoBoneIkStatus3D::Solved||status==TwoBoneIkStatus3D::Clamped;}
};

// poseRoot 必須與建立 pose.globalMatrices 時使用的根矩陣完全相同；失敗時 pose 保持逐位元不變。
bool SolveTwoBoneIk3D(const Skeleton3D& skeleton,TwoBoneIkChain3D chain,
    TwoBoneIkTarget3D target,AnimationPose3D& pose,TwoBoneIkResult3D* result=nullptr,
    Matrix4 poseRoot=Matrix4::Identity())noexcept;

// 保留末端在模型空間的原動畫朝向，只調整鏈節位置。
bool SolveTwoBoneIkKeepEndOrientation3D(const Skeleton3D& skeleton,TwoBoneIkChain3D chain,
    TwoBoneIkTarget3D target,AnimationPose3D& pose,TwoBoneIkResult3D* result=nullptr,
    Matrix4 poseRoot=Matrix4::Identity())noexcept;
}
