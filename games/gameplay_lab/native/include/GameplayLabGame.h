#pragma once
#include "GameplayRules3D.h"
#include "GameplayInputReplay3D.h"
#include "GameplayAnimation3D.h"
#include "GameplayCombat3D.h"
#include "GameplayShowcaseEnvironment3D.h"
#include "GameplayObjectPaletteUi3D.h"
#include "ObjectPaletteAdapter3D.h"
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/EnvironmentCollisionStreaming3D.h>
#include <Aevrynd/PhysicsWorld3D.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/FootPlacement3D.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/SkinnedModel3D.h>
#include <Aevrynd/SkinnedRenderable3D.h>
#include <filesystem>
#include <string>

std::string CreateGameplayLabRunId();

class GameplayLabGame final:public Aevrynd::IGame {
public:
    explicit GameplayLabGame(std::filesystem::path captureDirectory={}):combat_(std::make_unique<GameplayLab::GameplayCombat3D>(collisions_,physics_)),palette_(std::make_unique<GameplayLab::ObjectPaletteAdapter3D>(rules_,collisions_,physics_,sceneEcs_)),paletteUi_(std::make_unique<GameplayLab::GameplayObjectPaletteUi3D>(*palette_)),captureDirectory_(std::move(captureDirectory)),runId_(CreateGameplayLabRunId()){}
    void OnStart(Aevrynd::Engine&)override;void OnUpdate(Aevrynd::Engine&,float)override;void OnRender(Aevrynd::Engine&,float)override;void OnStop(Aevrynd::Engine&)override;
    Aevrynd::ReusableMemoryStats GetReusableMemoryStats()const override;std::size_t TrimReusableMemory()override;
private:
    void BuildWorld(Aevrynd::Engine&);void AddShowcaseEnvironment();void UpdateCamera();void UpdateCapture(Aevrynd::Engine&);bool InitializeEnvironmentCollision();bool SyncEnvironmentCollision(Aevrynd::Vec3 observer);void ReleaseEnvironmentGameplayColliders();
    GameplayLab::Rules3D rules_{};Aevrynd::CharacterController3D controller_{};Aevrynd::CollisionWorld3D collisions_{};Aevrynd::PhysicsWorld3D physics_{};Aevrynd::SceneECSRegistry3D sceneEcs_{};Aevrynd::Scene3D scene_{};
    // 僅保留未來除錯剛體的可視化清單；所有可互動物件的擁有權都在 palette_。
    Aevrynd::MeshResource3D boxMesh_{},sphereMesh_{};std::vector<Aevrynd::RigidBodyHandle3D> bodies_{};Aevrynd::Camera3D camera_{};float yaw_{},pitch_{.2f},simulationDelta_{1.f/60.f};
    GameplayLab::GameplayShowcaseEnvironment3D showcaseEnvironment_{};Aevrynd::HeightField3D showcaseTerrain_{};std::unique_ptr<Aevrynd::EnvironmentCollisionStreaming3D> environmentCollisions_{};std::vector<Aevrynd::EnvironmentColliderDescriptor3D> environmentCollisionDescriptors_{};std::vector<Aevrynd::ColliderHandle3D> environmentGameplayColliders_{};Aevrynd::EnvironmentCollisionStreamingReceipt3D environmentCollisionActiveReceipt_{};Aevrynd::MeshResource3D showcaseTerrainMesh_{},showcaseCylinderMesh_{},showcaseConeHighMesh_{},showcaseConeLowMesh_{},showcaseGrassHighMesh_{},showcaseGrassLowMesh_{},showcaseTreeLowMesh_{},showcaseRockLowMesh_{},showcaseQuiverTreeMesh_{},showcaseRock09Mesh_{};Aevrynd::MaterialLibrary3D showcaseMaterialLibrary_{},showcaseModelMaterialLibrary_{};Aevrynd::Material3D showcaseGroundMaterial_{},showcaseWoodMaterial_{},showcaseBarkMaterial_{},showcaseRockMaterial_{},showcaseQuiverTreeMaterial_{},showcaseRock09Material_{};Aevrynd::RendererStats showcaseRendererBaseline_{};std::size_t showcaseSubmittedDescriptors_{};bool showcaseExecutionObserved_{},environmentCollisionReloadStable_{},environmentStableIdEvidence_{},environmentRayEvidence_{},environmentMeleeQueryEvidence_{};
    Aevrynd::SkinnedModel3D femaleModel_{},maleModel_{};std::vector<Aevrynd::SkinnedSubmesh3D> femaleSubmeshes_{},maleSubmeshes_{},femaleWeaponSubmeshes_{},maleWeaponSubmeshes_{};std::vector<Aevrynd::Material3D> femaleMaterials_{},maleMaterials_{};std::array<Aevrynd::Material3D,2> femaleWeaponMaterials_{},maleWeaponMaterials_{};Aevrynd::MaterialLibrary3D femaleMaterialLibrary_{},maleMaterialLibrary_{};Aevrynd::SkinnedMeshResource3D femaleMesh_{},maleMesh_{},femaleWeaponMesh_{},maleWeaponMesh_{};Aevrynd::AnimationPose3D femalePose_{},malePose_{};Aevrynd::SkinningWorkspace3D skinning_{};
    std::unique_ptr<GameplayLab::GameplayCombat3D> combat_{};GameplayLab::GameplayAnimation3D femaleAnimation_{},maleAnimation_{},monsterAnimation_{};Aevrynd::AnimationPose3D monsterPose_{};Aevrynd::FootPlacement3D femaleFootPlacement_{},maleFootPlacement_{};Aevrynd::SkinnedRenderableGroupHistory3D playerHistory_{},monsterHistory_{};Aevrynd::SkinnedRenderableGroupStats3D skinnedGroupStats_{};std::array<std::uint64_t,11> femalePoseHashes_{},malePoseHashes_{};
    std::unique_ptr<GameplayLab::ObjectPaletteAdapter3D> palette_{};std::unique_ptr<GameplayLab::GameplayObjectPaletteUi3D> paletteUi_{};std::uint64_t paletteUiDrawnFrames_{};bool palettePointerInteraction_{};
    std::filesystem::path captureDirectory_{};std::optional<Aevrynd::Vec3> captureValidGhost_{},captureInvalidGhost_{};std::size_t captureIndex_{};bool capturePending_{},captureCrouchTapPending_{},combatSpawned_{},crouchFeetInvariant_{true},weaponAttachmentPassed_{},weaponHitWindowEvidence_{},weaponAnimationSyncPassed_{true},firstPersonBodyHidden_{true},captureGrabSeeded_{},sourceFingerprintPassed_{true};Aevrynd::ColliderHandle3D captureCeiling_{},captureEvidenceCollider_{};float minimumControllerHeight_{1.8f},maximumWeaponPhaseError_{};double nextCapture_{.5};std::uint64_t captureSuccess_{},captureReject_{},femaleGpuDraws_{},maleGpuDraws_{},weaponPbrDraws_{},captureAuthoritativePhysicsSteps_{},weaponTrajectoryHash_{14695981039346656037ull};std::array<std::uint64_t,3> attackTrajectoryHashes_{{14695981039346656037ull,14695981039346656037ull,14695981039346656037ull}};std::array<std::uint32_t,11> captureBodyMaterialSlots_{},captureWeaponMaterialRanges_{};std::array<float,11> captureWeaponAnchorErrors_{},captureWeaponScreenCoverage_{},captureWeaponMinimumDepth_{},captureWeaponGripX_{},captureWeaponGripY_{},captureWeaponBladeY_{};std::array<std::uint8_t,11> captureWeaponCenterExcluded_{},captureWeaponNearClipClear_{},captureWeaponHudOverlap_{},captureWeaponGripEdgeAnchored_{},captureWeaponBladeAboveGrip_{};std::size_t released_{};std::uint64_t trims_{};std::string runId_{},expectedSourceFingerprint_{},actualSourceFingerprint_{AEVRYND_CHARACTER_BUILD_FINGERPRINT};
};
