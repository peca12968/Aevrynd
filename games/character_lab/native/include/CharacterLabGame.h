#pragma once
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/SkinnedModelAsset.h>
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
    std::vector<std::string> femaleMaterialNames_{},maleMaterialNames_{};
    Aevrynd::SkinnedMeshResource3D gpuFemale_{},gpuMale_{};
    Aevrynd::AnimationClip3D femaleClip_{},maleClip_{};
    Aevrynd::AnimationPlayer3D femalePlayer_{},malePlayer_{};
    Aevrynd::AnimationPose3D femalePose_{},malePose_{};
    Aevrynd::SkinningWorkspace3D skinningWorkspace_{};
    Aevrynd::SoundHandle jumpSound_{};
    Aevrynd::Camera3D camera_{};
    float yaw_{},pitch_{.12f};
    int automaticPhase_{-1},lastJumpCycle_{-1};
    uint64_t modeSwitches_{},cameraObstructions_{};
    std::size_t lastReusableMemoryReleasedBytes_{};uint64_t reusableMemoryTrimCount_{};
    std::filesystem::path captureDirectory_{};
    std::uint64_t captureSuccessfulBaseline_{},captureRejectedBaseline_{};
    double nextCaptureTime_{.8};
    bool capturePending_{};
    bool firstPerson_{},automatic_{true},femaleActive_{true};
};
