#pragma once
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/AnimationGraph3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/SkinnedRenderable3D.h>
#include <filesystem>

class CharacterLabGame final:public Aevrynd::IGame {
public:
    explicit CharacterLabGame(std::filesystem::path captureDirectory={}):captureDirectory_(std::move(captureDirectory)){}
    void OnStart(Aevrynd::Engine&)override;
    void OnUpdate(Aevrynd::Engine&,float)override;
    void OnRender(Aevrynd::Engine&,float)override;
    void OnStop(Aevrynd::Engine&)override;
    Aevrynd::ReusableMemoryStats GetReusableMemoryStats()const override;
    std::size_t TrimReusableMemory()override;
private:
    void BuildEnvironment();
    Aevrynd::CharacterController3D controller_{};
    Aevrynd::CollisionWorld3D collisionWorld_;
    Aevrynd::Scene3D scene_;
    Aevrynd::Scene3DStats sceneStats_{};
    Aevrynd::SceneQueryStats3D queryStats_{};
    Aevrynd::SceneHit3D aimHit_{};
    Aevrynd::MeshResource3D boxMesh_{};
    Aevrynd::SkinnedModel3D femaleModel_{},maleModel_{};
    std::vector<Aevrynd::SkinnedSubmesh3D> femaleSubmeshes_{},maleSubmeshes_{};
    std::vector<Aevrynd::Material3D> femaleMaterials_{},maleMaterials_{};
    Aevrynd::MaterialLibrary3D femaleMaterialLibrary_{},maleMaterialLibrary_{};
    Aevrynd::SkinnedMeshResource3D gpuFemale_{},gpuMale_{};
    Aevrynd::SkinnedMeshResource3D femaleShoulder_{},femaleSword_{},femaleAxe_{};
    Aevrynd::SkinnedMeshResource3D maleShoulder_{},maleSword_{},maleAxe_{};
    std::array<Aevrynd::Material3D,1> femaleEquipmentMaterial_{},maleEquipmentMaterial_{};
    Aevrynd::SkinnedRenderableGroupHistory3D femaleGroupHistory_{},maleGroupHistory_{};
    Aevrynd::SkinnedRenderableGroupStats3D equipmentGroupStats_{};
    std::array<Aevrynd::AnimationClip3D,4> femaleGraphClips_{},maleGraphClips_{};
    std::vector<float> femaleUpperMask_{},maleUpperMask_{};
    Aevrynd::AnimationGraph3D femaleGraph_{},maleGraph_{};
    Aevrynd::AnimationPlayer3D femalePlayer_{},malePlayer_{};
    Aevrynd::AnimationPose3D femalePose_{},malePose_{};
    Aevrynd::SkinningWorkspace3D skinningWorkspace_{};
    Aevrynd::SoundHandle jumpSound_{};
    Aevrynd::Camera3D camera_{};
    float yaw_{},pitch_{.12f};
    float leftFootMinimum_{},rightFootMinimum_{},footPhaseDifference_{},contactDrift_{},maximumJointStep_{};
    std::uint64_t animationPoseHash_{};
    int automaticPhase_{-1},lastJumpCycle_{-1};
    uint64_t modeSwitches_{},cameraObstructions_{};
    std::uint64_t equipmentRevision_{},renderFrame_{};
    std::size_t lastEquipmentCaptureIndex_{static_cast<std::size_t>(-1)};
    std::size_t lastReusableMemoryReleasedBytes_{};uint64_t reusableMemoryTrimCount_{};
    std::filesystem::path captureDirectory_{};
    std::uint64_t captureSuccessfulBaseline_{},captureRejectedBaseline_{};
    std::size_t captureIndex_{};
    double nextCaptureTime_{.8};
    bool capturePending_{};
    bool firstPerson_{},automatic_{true},femaleActive_{true},armorVisible_{true},axeEquipped_{},lastRenderedFirstPerson_{};
};
