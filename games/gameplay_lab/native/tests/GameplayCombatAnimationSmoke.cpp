#include "GameplayAnimation3D.h"
#include "GameplayCombat3D.h"

#include <Aevrynd/CharacterPresentation3D.h>
#include <Aevrynd/FootPlacement3D.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
std::uint64_t HashPose(const AnimationPose3D& pose) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const Matrix4& matrix : pose.globalMatrices) for (const auto& row : matrix.values)
        for (float value : row) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
            for (std::size_t index = 0; index < sizeof(value); ++index) {
                hash ^= bytes[index];
                hash *= 1099511628211ull;
            }
        }
    return hash;
}

AnimationState3D StateForAttack(Attack3D attack) {
    return attack == Attack3D::Attack1 ? AnimationState3D::Attack1
        : attack == Attack3D::Attack2 ? AnimationState3D::Attack2
        : AnimationState3D::Attack3;
}
}

int main() {
    const auto source = std::filesystem::path{AEVRYND_SOURCE_DIR};
    const auto loaded = LoadSkinnedModelAssetFileDetailed(
        source / "games/character_lab/assets/model-packs/realistic-female/realistic-female.aeskin");
    GameplayAnimation3D animation;
    std::string error;
    if (!loaded || !animation.Initialize(loaded.asset, &error)) {
        std::cerr << "Gameplay F2 animation initialization failed: " << error << '\n';
        return 1;
    }
    std::set<std::uint64_t> poseHashes;
    AnimationPose3D pose;
    for (int value = 0; value < 11; ++value) {
        animation.SetState(AnimationState3D(value), true);
        animation.Tick(animation.Duration() * .47f);
        if (!animation.EvaluateInto(pose) || pose.globalMatrices.size() != 20) return 2;
        poseHashes.insert(HashPose(pose));
    }
    if (poseHashes.size() != 11) {
        std::cerr << "Gameplay animation states were not distinct\n";
        return 3;
    }
    if (std::abs(GameplayPresentationDelta3D(0.f)-1.f/60.f)>1e-6f ||
        std::abs(GameplayPresentationDelta3D(std::numeric_limits<float>::infinity())-1.f/60.f)>1e-6f ||
        std::abs(GameplayPresentationDelta3D(5000.f)-.1f)>1e-6f) return 7;
    FootPlacement3D noGroundPlacement;
    CollisionWorld3D noGroundWorld;
    if (!noGroundPlacement.Prepare(loaded.asset.model.skeleton,animation.Rig(),loaded.asset.model.skeletonCompatibilityId)) return 8;
    animation.SetState(AnimationState3D::Idle,true);
    animation.Tick(1.f/60.f);
    if (!animation.EvaluateInto(pose)) return 9;
    FootPlacementFrame3D noGroundFrame{};
    noGroundFrame.collisionWorld=&noGroundWorld;
    noGroundFrame.deltaTime=GameplayPresentationDelta3D(5000.f);
    noGroundFrame.cameraDistance=1.f;
    FootPlacementResult3D noGroundResult{};
    if (!noGroundPlacement.Evaluate(loaded.asset.model.skeleton,animation.Rig(),noGroundFrame,pose,&noGroundResult) || noGroundResult.applied) return 10;

    CollisionWorld3D collision;
    PhysicsWorld3D physics;
    GameplayCombat3D combat(collision, physics);
    combat.SpawnMonster({0, 0, 1.8f}, 100);
    CombatInput3D input{{0, 0, 0}, {0, 0, 1}, true};
    std::array<std::uint64_t, 3> trajectories{};
    float maximumPhaseError{};
    for (int attackIndex = 0; attackIndex < 3; ++attackIndex) {
        combat.Tick(input, .01f);
        input.attackPressed = false;
        animation.SetState(StateForAttack(combat.CurrentAttack()), true);
        std::uint64_t trajectory = 14695981039346656037ull;
        for (int frame = 0; frame < 54; ++frame) {
            combat.Tick(input, .01f);
            animation.Tick(.01f);
            animation.SynchronizeNormalizedPhase(combat.NormalizedAttackPhase());
            if (!animation.EvaluateInto(pose)) return 4;
            Matrix4 socket{};
            if (!CharacterSocketTransform3D(loaded.asset.model.skeleton, animation.Rig(),
                                            HumanoidJoint3D::RightHand, pose,
                                            Matrix4::Identity(), socket)) return 5;
            const Vec3 tip = TransformPoint(socket, {0, -.72f, 0});
            for (float value : {tip.x, tip.y, tip.z}) {
                const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
                for (std::size_t index = 0; index < sizeof(value); ++index) {
                    trajectory ^= bytes[index];
                    trajectory *= 1099511628211ull;
                }
            }
            maximumPhaseError = (std::max)(maximumPhaseError,
                std::abs(animation.NormalizedPhase() - combat.NormalizedAttackPhase()));
        }
        trajectories[std::size_t(attackIndex)] = trajectory;
        input.attackPressed = true;
    }
    if (combat.Monster().alive || combat.Stats().hits != 3 || combat.Stats().kills != 1 ||
        collision.Stats().colliders || physics.Stats().bodies ||
        trajectories[0] == trajectories[1] || trajectories[1] == trajectories[2] ||
        trajectories[0] == trajectories[2] || maximumPhaseError > .025f) {
        std::cerr << "Combat/animation/weapon synchronization gate failed phaseError="
                  << maximumPhaseError << '\n';
        return 6;
    }
    std::cout << "combatHits=3 kills=1 animationStates=11 joints=20 trajectories=3 phaseError="
              << maximumPhaseError << '\n';
}
