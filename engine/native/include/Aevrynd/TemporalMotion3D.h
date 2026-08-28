#pragma once

#include <Aevrynd/Math.h>

#include <array>
#include <cstdint>
#include <span>

namespace Aevrynd {

inline constexpr float TemporalMotionMinimumClipW3D = 0.00001f;
inline constexpr std::uint32_t TemporalJitterSampleCount3D = 8;

struct TemporalJitterSample3D {
    Vec2 pixelOffset{};
    std::uint32_t sequenceIndex{};
};

struct TemporalMotionSettings3D {
    Vec2 outputSize{};
    bool historyValid{};
    bool cameraCut{};
    float minimumClipW{TemporalMotionMinimumClipW3D};
};

struct TemporalMotionSample3D {
    Vec4 currentClip{};
    Vec4 previousClip{};
    Vec2 ndcVelocity{};
    Vec2 uvVelocity{};
    Vec2 outputPixelVelocity{};
    bool valid{};
};

struct TemporalSkinInfluence3D {
    std::array<std::uint16_t, 4> joints{};
    std::array<float, 4> weights{};
};

// 固定八點 Halton 週期先扣除週期平均值，避免長時間累積相機偏移。
TemporalJitterSample3D CalculateTemporalJitterSample3D(std::uint64_t frameIndex);

// pixel offset 採左上原點；投影仍維持 DirectX NDC 的正 Y 向上。
Matrix4 ApplyTemporalProjectionJitter3D(
    const Matrix4& unjitteredViewProjection,
    Vec2 pixelOffset,
    Vec2 renderSize);

Vec2 ReprojectTemporalHistoryUv3D(
    Vec2 currentUv,
    Vec2 velocityPixels,
    Vec2 currentJitterPixels,
    Vec2 previousJitterPixels,
    Vec2 renderSize);

// UV 與輸出像素採左上原點，因此正 X 向右、正 Y 向下。
TemporalMotionSample3D EvaluateTemporalMotionClip3D(
    Vec4 currentClip,
    Vec4 previousClip,
    const TemporalMotionSettings3D& settings);

// view-projection 必須是不含 jitter 的矩陣，避免把取樣抖動誤判成場景速度。
TemporalMotionSample3D EvaluateRigidTemporalMotion3D(
    Vec3 localPosition,
    const Matrix4& currentModel,
    const Matrix4& previousModel,
    const Matrix4& currentUnjitteredViewProjection,
    const Matrix4& previousUnjitteredViewProjection,
    const TemporalMotionSettings3D& settings);

// CPU golden helper 會正規化有效權重，失敗時不修改輸出位置。
bool EvaluateSkinnedTemporalPositions3D(
    Vec3 bindPosition,
    const TemporalSkinInfluence3D& influence,
    std::span<const Matrix4> currentSkinMatrices,
    std::span<const Matrix4> previousSkinMatrices,
    Vec3& currentLocalPosition,
    Vec3& previousLocalPosition);

TemporalMotionSample3D EvaluateSkinnedTemporalMotion3D(
    Vec3 bindPosition,
    const TemporalSkinInfluence3D& influence,
    std::span<const Matrix4> currentSkinMatrices,
    std::span<const Matrix4> previousSkinMatrices,
    const Matrix4& currentModel,
    const Matrix4& previousModel,
    const Matrix4& currentUnjitteredViewProjection,
    const Matrix4& previousUnjitteredViewProjection,
    const TemporalMotionSettings3D& settings);

}
