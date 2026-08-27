#include <Aevrynd/TemporalMotion3D.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

bool Near(float left, float right, float tolerance = 0.0001f) {
    return std::abs(left - right) <= tolerance;
}

bool Zero(const Aevrynd::TemporalMotionSample3D& sample) {
    return !sample.valid && Near(sample.ndcVelocity.x, 0.0f) && Near(sample.ndcVelocity.y, 0.0f) &&
           Near(sample.uvVelocity.x, 0.0f) && Near(sample.uvVelocity.y, 0.0f) &&
           Near(sample.outputPixelVelocity.x, 0.0f) && Near(sample.outputPixelVelocity.y, 0.0f);
}

bool PixelVelocity(const Aevrynd::TemporalMotionSample3D& sample, float x, float y) {
    return sample.valid && Near(sample.outputPixelVelocity.x, x) && Near(sample.outputPixelVelocity.y, y);
}

}

int main() {
    using namespace Aevrynd;
    const Matrix4 identity = Matrix4::Identity();
    TemporalMotionSettings3D settings{{100.0f, 100.0f}, true, false};

    if (!PixelVelocity(EvaluateRigidTemporalMotion3D({}, identity, identity, identity, identity, settings), 0.0f, 0.0f)) return 1;

    // 相機向右移動一像素時，靜態世界在螢幕上向左一像素。
    const Matrix4 cameraOnePixelRight = CreateTranslationMatrix({-0.02f, 0.0f, 0.0f});
    if (!PixelVelocity(EvaluateRigidTemporalMotion3D({}, identity, identity, cameraOnePixelRight, identity, settings), -1.0f, 0.0f)) return 2;

    const Matrix4 objectTwoPixelsRight = CreateTranslationMatrix({0.04f, 0.0f, 0.0f});
    if (!PixelVelocity(EvaluateRigidTemporalMotion3D({}, objectTwoPixelsRight, identity, identity, identity, settings), 2.0f, 0.0f)) return 3;
    if (!PixelVelocity(EvaluateRigidTemporalMotion3D({}, objectTwoPixelsRight, identity,
                                                     CreateTranslationMatrix({-0.04f, 0.0f, 0.0f}), identity, settings),
                       0.0f, 0.0f)) return 4;

    TemporalMotionSettings3D noHistory = settings;
    noHistory.historyValid = false;
    if (!Zero(EvaluateRigidTemporalMotion3D({}, objectTwoPixelsRight, identity, identity, identity, noHistory))) return 5;
    TemporalMotionSettings3D cameraCut = settings;
    cameraCut.cameraCut = true;
    if (!Zero(EvaluateRigidTemporalMotion3D({}, objectTwoPixelsRight, identity, identity, identity, cameraCut))) return 6;

    const Matrix4 mirroredNonUniform = CreateScaleMatrix({-2.0f, 3.0f, 0.5f});
    if (!PixelVelocity(EvaluateRigidTemporalMotion3D({1.0f, 2.0f, 3.0f}, mirroredNonUniform,
                                                     mirroredNonUniform, identity, identity, settings), 0.0f, 0.0f)) return 7;

    Matrix4 zeroW = identity;
    zeroW[3][3] = 0.0f;
    if (!Zero(EvaluateRigidTemporalMotion3D({}, identity, identity, zeroW, identity, settings))) return 8;
    Matrix4 behind = identity;
    behind[3][3] = -1.0f;
    if (!Zero(EvaluateRigidTemporalMotion3D({}, identity, identity, behind, identity, settings))) return 9;
    Matrix4 notFinite = identity;
    notFinite[0][0] = std::numeric_limits<float>::infinity();
    if (!Zero(EvaluateRigidTemporalMotion3D({}, identity, identity, notFinite, identity, settings))) return 10;

    const std::vector<Matrix4> previousSkin{identity, identity};
    const std::vector<Matrix4> currentSingle{CreateTranslationMatrix({0.06f, 0.0f, 0.0f}), identity};
    TemporalSkinInfluence3D single{};
    single.weights[0] = 1.0f;
    if (!PixelVelocity(EvaluateSkinnedTemporalMotion3D({}, single, currentSingle, previousSkin,
                                                       identity, identity, identity, identity, settings), 3.0f, 0.0f)) return 11;

    const std::vector<Matrix4> currentBlend{
        CreateTranslationMatrix({0.04f, 0.0f, 0.0f}),
        CreateTranslationMatrix({0.08f, 0.0f, 0.0f})};
    TemporalSkinInfluence3D blend{};
    blend.joints = {0, 1, 0, 0};
    blend.weights = {0.5f, 0.5f, 0.0f, 0.0f};
    Vec3 currentPosition{99.0f, 99.0f, 99.0f};
    Vec3 previousPosition{99.0f, 99.0f, 99.0f};
    if (!EvaluateSkinnedTemporalPositions3D({}, blend, currentBlend, previousSkin, currentPosition, previousPosition) ||
        !Near(currentPosition.x, 0.06f) || !Near(previousPosition.x, 0.0f)) return 12;
    if (!PixelVelocity(EvaluateSkinnedTemporalMotion3D({}, blend, currentBlend, previousSkin,
                                                       identity, identity, identity, identity, settings), 3.0f, 0.0f)) return 13;

    TemporalSkinInfluence3D invalid = single;
    invalid.joints[0] = 9;
    currentPosition = {7.0f, 8.0f, 9.0f};
    previousPosition = currentPosition;
    if (EvaluateSkinnedTemporalPositions3D({}, invalid, currentBlend, previousSkin, currentPosition, previousPosition) ||
        !Near(currentPosition.x, 7.0f) || !Near(previousPosition.x, 7.0f)) return 14;

    std::cout << "TemporalMotion3D smoke test passed\n";
    return 0;
}
