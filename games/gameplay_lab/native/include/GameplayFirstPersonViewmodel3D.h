#pragma once
#include <Aevrynd/Camera.h>
#include <Aevrynd/Math.h>
#include <array>
#include <cstdint>

namespace GameplayLab {

enum class GameplayViewmodelSex3D : std::uint8_t {Female,Male};

struct GameplayFirstPersonViewmodelLayout3D {
    Aevrynd::Vec3 gripTranslation{};
    Aevrynd::Vec3 gripRotation{};
    float armsScale{},weaponScale{};
    float armsMinimumX{},armsMaximumX{};
    float keyIntensity{},fillIntensity{};
};

struct GameplayFirstPersonViewmodelProjection3D {
    float weaponCoverage{},minimumDepth{},gripX{},gripY{},bladeY{};
    float weaponMinimumX{},weaponMinimumY{},weaponMaximumX{},weaponMaximumY{};
    float armsMinimumX{},armsMinimumY{},armsMaximumX{},armsMaximumY{};
    bool centerExcluded{},nearClipClear{},hudClear{},gripEdgeAnchored{},bladeAboveGrip{};
    bool weaponMarginClear{},armsMarginClear{};
    bool Complete()const noexcept;
};

GameplayFirstPersonViewmodelLayout3D GameplayFirstPersonViewmodelLayout(GameplayViewmodelSex3D sex)noexcept;
Aevrynd::Matrix4 GameplayFirstPersonDesiredGripTransform3D(const GameplayFirstPersonViewmodelLayout3D& layout)noexcept;
bool ComposeGameplayFirstPersonWeaponWorld3D(const Aevrynd::Camera3D& camera,
    const Aevrynd::Matrix4& modelHandSocket,const GameplayFirstPersonViewmodelLayout3D& layout,
    bool arms,Aevrynd::Matrix4& weaponWorld)noexcept;
GameplayFirstPersonViewmodelProjection3D EvaluateGameplayFirstPersonViewmodelProjection3D(
    const Aevrynd::Camera3D& camera,const Aevrynd::Matrix4& handSocketWorld,
    const GameplayFirstPersonViewmodelLayout3D& layout)noexcept;

}
