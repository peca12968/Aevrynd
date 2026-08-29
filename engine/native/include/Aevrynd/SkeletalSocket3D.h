#pragma once
#include "Animation3D.h"
#include "Renderer.h"
#include <cstddef>

namespace Aevrynd {
// 骨骼 socket 定義在 joint 的模型空間 global transform 之後；localOffset 是 joint-local 偏移。
struct SkeletalSocket3D {
    // compatibility 綁定 socket 所屬的 authoritative skeleton profile；零值永遠無效。
    SkeletonCompatibilityId3D compatibility{};
    // joint 是 pose.globalMatrices 的索引，必須落在當前 pose 範圍內。
    std::size_t joint{};
    // localOffset 在 joint-local 座標空間，允許平移、旋轉與有限縮放。
    Matrix4 localOffset{Matrix4::Identity()};
};

// outputWorld = modelToWorld * pose.globalMatrices[joint] * localOffset。
// compatibility 必須與 socket 完全相同；所有輸入與結果矩陣都必須有限，任何失敗 output 原子不變。
bool EvaluateSkeletalSocket3D(const SkeletalSocket3D& socket,SkeletonCompatibilityId3D compatibility,
    const AnimationPose3D& pose,const Matrix4& modelToWorld,Matrix4& output)noexcept;
}
