#pragma once
#include <Aevrynd/Camera.h>
#include <Aevrynd/Math.h>
#include <Aevrynd/CharacterPresentation3D.h>
#include <array>
#include <cstdint>

namespace GameplayLab {

struct GameplayViewmodelLandmark3D {
    Aevrynd::Vec3 cameraMeters{};
    Aevrynd::Vec2 viewportPixels{};
    bool available{};
};

// 肩部使用 upper-arm 根節點；腕部與指根不是掌心，不推估缺少的掌心標記。
struct GameplayViewmodelGeometryDiagnostic3D {
    std::array<GameplayViewmodelLandmark3D, 2> shoulders{}, elbows{}, wrists{}, fingerRoots{};
    GameplayViewmodelLandmark3D actualGrip{}, declaredGrip{}, bladeApex{};
    float wristToActualGripMeters{}, actualGripToDeclaredSocketMeters{};
    std::uint32_t gripVertexCount{};
    bool valid{}, generatedSwordSupported{}, palmAvailable{};
};

// 僅診斷正式工廠的劍幾何，不改變姿勢、畫面或既有驗收閘門。
GameplayViewmodelGeometryDiagnostic3D MeasureGameplayViewmodelGeometry3D(
    const Aevrynd::Camera3D& camera, std::uint32_t viewportWidth, std::uint32_t viewportHeight,
    const Aevrynd::SkinnedModelAssetData& character, const Aevrynd::HumanoidSkeletonMap3D& rig,
    const Aevrynd::AnimationPose3D& pose, const Aevrynd::CharacterEquipmentAsset3D& weapon,
    Aevrynd::CharacterEquipmentShape3D shape,
    const Aevrynd::Matrix4& armsWorld, const Aevrynd::Matrix4& weaponWorld);

enum class GameplayViewmodelSex3D : std::uint8_t {Female,Male};

// 初始化一次的正式武器幾何快取；呼叫端僅能讀取，不能把合成包絡偽裝成模型。
class GameplayFirstPersonWeaponProjectionGeometry3D {
public:
    bool Valid() const noexcept { return valid_; }
    std::span<const Aevrynd::Vec3> HandLocalVertices() const noexcept { return vertices_; }
    const Aevrynd::CharacterSwordGeometry3D& Sword() const noexcept { return sword_; }
private:
    std::vector<Aevrynd::Vec3> vertices_{};
    Aevrynd::CharacterSwordGeometry3D sword_{};
    bool valid_{};
    friend GameplayFirstPersonWeaponProjectionGeometry3D BuildGameplayFirstPersonWeaponProjectionGeometry3D(const Aevrynd::CharacterEquipmentAsset3D&);
};

GameplayFirstPersonWeaponProjectionGeometry3D BuildGameplayFirstPersonWeaponProjectionGeometry3D(
    const Aevrynd::CharacterEquipmentAsset3D& equipment);

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
    bool actualWeaponGeometryMeasured{};
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
GameplayFirstPersonViewmodelProjection3D EvaluateGameplayFirstPersonViewmodelProjection3D(
    const Aevrynd::Camera3D& camera,const Aevrynd::Matrix4& handSocketWorld,
    const GameplayFirstPersonViewmodelLayout3D& layout,
    const GameplayFirstPersonWeaponProjectionGeometry3D& geometry,
    std::uint32_t viewportWidth=1280,std::uint32_t viewportHeight=720)noexcept;
GameplayFirstPersonViewmodelProjection3D EvaluateGameplayFirstPersonViewmodelProjection3D(
    const Aevrynd::Camera3D& camera,const Aevrynd::Matrix4& handSocketWorld,
    const GameplayFirstPersonViewmodelLayout3D& layout,
    const GameplayFirstPersonWeaponProjectionGeometry3D& geometry,
    std::span<const Aevrynd::Vertex3D> posedArms,const Aevrynd::Matrix4& armsWorld,
    std::uint32_t viewportWidth=1280,std::uint32_t viewportHeight=720)noexcept;

}
