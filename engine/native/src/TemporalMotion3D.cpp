#include <Aevrynd/TemporalMotion3D.h>

#include <cmath>

namespace Aevrynd {
namespace {

bool IsFinite(float value) {
    return std::isfinite(value);
}

bool IsFinite(Vec3 value) {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

bool IsFinite(Vec4 value) {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z) && IsFinite(value.w);
}

bool IsFinite(const Matrix4& matrix) {
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            if (!IsFinite(matrix[row][column])) return false;
        }
    }
    return true;
}

Vec3 TransformAffinePoint(const Matrix4& matrix, Vec3 point) {
    const Vec4 transformed = matrix * Vec4{point.x, point.y, point.z, 1.0f};
    return {transformed.x, transformed.y, transformed.z};
}

float RadicalInverse(std::uint32_t index, std::uint32_t base) {
    float result = 0.0f;
    float fraction = 1.0f / static_cast<float>(base);
    while (index) {
        result += static_cast<float>(index % base) * fraction;
        index /= base;
        fraction /= static_cast<float>(base);
    }
    return result;
}

}

TemporalJitterSample3D CalculateTemporalJitterSample3D(std::uint64_t frameIndex) {
    const std::uint32_t sequenceIndex = static_cast<std::uint32_t>(frameIndex % TemporalJitterSampleCount3D);
    Vec2 mean{};
    for (std::uint32_t index = 0; index < TemporalJitterSampleCount3D; ++index) {
        mean.x += RadicalInverse(index + 1, 2);
        mean.y += RadicalInverse(index + 1, 3);
    }
    mean = mean * (1.0f / static_cast<float>(TemporalJitterSampleCount3D));
    return {{RadicalInverse(sequenceIndex + 1, 2) - mean.x,
             RadicalInverse(sequenceIndex + 1, 3) - mean.y},
            sequenceIndex};
}

Matrix4 ApplyTemporalProjectionJitter3D(
    const Matrix4& unjitteredViewProjection,
    Vec2 pixelOffset,
    Vec2 renderSize) {
    if (!IsFinite(pixelOffset.x) || !IsFinite(pixelOffset.y) ||
        !IsFinite(renderSize.x) || !IsFinite(renderSize.y) ||
        renderSize.x <= 0.0f || renderSize.y <= 0.0f ||
        !IsFinite(unjitteredViewProjection)) {
        return unjitteredViewProjection;
    }
    Matrix4 result = unjitteredViewProjection;
    const float jitterNdcX = pixelOffset.x * 2.0f / renderSize.x;
    const float jitterNdcY = pixelOffset.y * -2.0f / renderSize.y;
    for (std::size_t column = 0; column < 4; ++column) {
        result[0][column] += jitterNdcX * unjitteredViewProjection[3][column];
        result[1][column] += jitterNdcY * unjitteredViewProjection[3][column];
    }
    return result;
}

Vec2 ReprojectTemporalHistoryUv3D(
    Vec2 currentUv,
    Vec2 velocityPixels,
    Vec2 currentJitterPixels,
    Vec2 previousJitterPixels,
    Vec2 renderSize) {
    if (!IsFinite(currentUv.x) || !IsFinite(currentUv.y) ||
        !IsFinite(velocityPixels.x) || !IsFinite(velocityPixels.y) ||
        !IsFinite(currentJitterPixels.x) || !IsFinite(currentJitterPixels.y) ||
        !IsFinite(previousJitterPixels.x) || !IsFinite(previousJitterPixels.y) ||
        !IsFinite(renderSize.x) || !IsFinite(renderSize.y) ||
        renderSize.x <= 0.0f || renderSize.y <= 0.0f) {
        return currentUv;
    }
    const Vec2 inverseSize{1.0f / renderSize.x, 1.0f / renderSize.y};
    return {currentUv.x - velocityPixels.x * inverseSize.x +
                (previousJitterPixels.x - currentJitterPixels.x) * inverseSize.x,
            currentUv.y - velocityPixels.y * inverseSize.y +
                (previousJitterPixels.y - currentJitterPixels.y) * inverseSize.y};
}

TemporalMotionSample3D EvaluateTemporalMotionClip3D(
    Vec4 currentClip,
    Vec4 previousClip,
    const TemporalMotionSettings3D& settings) {
    TemporalMotionSample3D result{};
    result.currentClip = currentClip;
    result.previousClip = previousClip;

    if (!settings.historyValid || settings.cameraCut ||
        !IsFinite(settings.outputSize.x) || !IsFinite(settings.outputSize.y) ||
        settings.outputSize.x <= 0.0f || settings.outputSize.y <= 0.0f ||
        !IsFinite(settings.minimumClipW) || settings.minimumClipW <= 0.0f ||
        !IsFinite(currentClip) || !IsFinite(previousClip) ||
        currentClip.w <= settings.minimumClipW || previousClip.w <= settings.minimumClipW) {
        return result;
    }

    const Vec2 currentNdc{currentClip.x / currentClip.w, currentClip.y / currentClip.w};
    const Vec2 previousNdc{previousClip.x / previousClip.w, previousClip.y / previousClip.w};
    const Vec2 ndcVelocity = currentNdc - previousNdc;
    const Vec2 uvVelocity{ndcVelocity.x * 0.5f, ndcVelocity.y * -0.5f};
    const Vec2 pixelVelocity{
        uvVelocity.x * settings.outputSize.x,
        uvVelocity.y * settings.outputSize.y};
    if (!IsFinite(ndcVelocity.x) || !IsFinite(ndcVelocity.y) ||
        !IsFinite(uvVelocity.x) || !IsFinite(uvVelocity.y) ||
        !IsFinite(pixelVelocity.x) || !IsFinite(pixelVelocity.y)) {
        return result;
    }

    result.ndcVelocity = ndcVelocity;
    result.uvVelocity = uvVelocity;
    result.outputPixelVelocity = pixelVelocity;
    result.valid = true;
    return result;
}

TemporalMotionSample3D EvaluateRigidTemporalMotion3D(
    Vec3 localPosition,
    const Matrix4& currentModel,
    const Matrix4& previousModel,
    const Matrix4& currentUnjitteredViewProjection,
    const Matrix4& previousUnjitteredViewProjection,
    const TemporalMotionSettings3D& settings) {
    if (!IsFinite(localPosition) || !IsFinite(currentModel) || !IsFinite(previousModel) ||
        !IsFinite(currentUnjitteredViewProjection) || !IsFinite(previousUnjitteredViewProjection)) {
        return {};
    }
    const Vec4 point{localPosition.x, localPosition.y, localPosition.z, 1.0f};
    return EvaluateTemporalMotionClip3D(
        currentUnjitteredViewProjection * (currentModel * point),
        previousUnjitteredViewProjection * (previousModel * point),
        settings);
}

bool EvaluateSkinnedTemporalPositions3D(
    Vec3 bindPosition,
    const TemporalSkinInfluence3D& influence,
    std::span<const Matrix4> currentSkinMatrices,
    std::span<const Matrix4> previousSkinMatrices,
    Vec3& currentLocalPosition,
    Vec3& previousLocalPosition) {
    if (!IsFinite(bindPosition)) return false;

    float totalWeight = 0.0f;
    Vec3 current{};
    Vec3 previous{};
    for (std::size_t index = 0; index < influence.weights.size(); ++index) {
        const float weight = influence.weights[index];
        if (!IsFinite(weight) || weight < 0.0f) return false;
        if (weight == 0.0f) continue;
        const std::size_t joint = influence.joints[index];
        if (joint >= currentSkinMatrices.size() || joint >= previousSkinMatrices.size() ||
            !IsFinite(currentSkinMatrices[joint]) || !IsFinite(previousSkinMatrices[joint])) {
            return false;
        }
        current = current + TransformAffinePoint(currentSkinMatrices[joint], bindPosition) * weight;
        previous = previous + TransformAffinePoint(previousSkinMatrices[joint], bindPosition) * weight;
        totalWeight += weight;
    }
    if (!IsFinite(totalWeight) || totalWeight <= 0.000001f) return false;

    current = current * (1.0f / totalWeight);
    previous = previous * (1.0f / totalWeight);
    if (!IsFinite(current) || !IsFinite(previous)) return false;
    currentLocalPosition = current;
    previousLocalPosition = previous;
    return true;
}

TemporalMotionSample3D EvaluateSkinnedTemporalMotion3D(
    Vec3 bindPosition,
    const TemporalSkinInfluence3D& influence,
    std::span<const Matrix4> currentSkinMatrices,
    std::span<const Matrix4> previousSkinMatrices,
    const Matrix4& currentModel,
    const Matrix4& previousModel,
    const Matrix4& currentUnjitteredViewProjection,
    const Matrix4& previousUnjitteredViewProjection,
    const TemporalMotionSettings3D& settings) {
    Vec3 currentLocal{};
    Vec3 previousLocal{};
    if (!EvaluateSkinnedTemporalPositions3D(bindPosition, influence, currentSkinMatrices,
                                            previousSkinMatrices, currentLocal, previousLocal)) {
        return {};
    }
    if (!IsFinite(currentModel) || !IsFinite(previousModel) ||
        !IsFinite(currentUnjitteredViewProjection) || !IsFinite(previousUnjitteredViewProjection)) {
        return {};
    }
    const Vec4 currentPoint{currentLocal.x, currentLocal.y, currentLocal.z, 1.0f};
    const Vec4 previousPoint{previousLocal.x, previousLocal.y, previousLocal.z, 1.0f};
    return EvaluateTemporalMotionClip3D(
        currentUnjitteredViewProjection * (currentModel * currentPoint),
        previousUnjitteredViewProjection * (previousModel * previousPoint),
        settings);
}

}
