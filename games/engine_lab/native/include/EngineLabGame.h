#pragma once
#include <Aevrynd/Animation3D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/Effects2D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/Navigation3D.h>
#include <Aevrynd/PhysicsWorld3D.h>
#include <Aevrynd/ReplicationRuntime3D.h>
#include <Aevrynd/ReplicationWorldAdapter3D.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/TerrainClipmap3D.h>
#include <Aevrynd/WorldBuilder3D.h>
#include <Aevrynd/WorldStreamer3D.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class EngineLabMutationTarget final:public Aevrynd::IDurableMutationTarget3D {
public:
    bool Prepare(std::uint64_t,const Aevrynd::WorldChunkDeltaSnapshot3D&)override{return true;}
    void Commit(std::uint64_t,const Aevrynd::WorldChunkDeltaSnapshot3D&)noexcept override{++commits;}
    void Abort(std::uint64_t)noexcept override{++aborts;}
    std::uint64_t commits{},aborts{};
};

class EngineLabGame final:public Aevrynd::IGame {
public:
    explicit EngineLabGame(std::filesystem::path captureDirectory={}):captureDirectory_(std::move(captureDirectory)){}
    void OnStart(Aevrynd::Engine&)override;
    void OnUpdate(Aevrynd::Engine&,float)override;
    void OnRender(Aevrynd::Engine&,float)override;
    void OnStop(Aevrynd::Engine&)override;
    Aevrynd::ReusableMemoryStats GetReusableMemoryStats()const override;
    std::size_t TrimReusableMemory()override;
private:
    void BuildLandscapeScene();
    Aevrynd::Scene3D scene_;
    Aevrynd::Scene3D landscapeScene_;
    Aevrynd::Scene3DStats sceneStats_{};
    Aevrynd::SceneQueryStats3D queryStats_{};
    Aevrynd::SceneHit3D sceneHit_{};
    Aevrynd::MeshResource3D boxMesh_{};
    Aevrynd::MeshResource3D pyramidMesh_{};
    Aevrynd::MeshResource3D cylinderMesh_{};
    Aevrynd::MeshResource3D grassHighMesh_{};
    Aevrynd::MeshResource3D grassLowMesh_{};
    Aevrynd::MeshResource3D treeHighMesh_{};
    Aevrynd::MeshResource3D treeLowMesh_{};
    Aevrynd::MeshResource3D coniferHighMesh_{};
    Aevrynd::MeshResource3D coniferLowMesh_{};
    Aevrynd::MeshResource3D cottageHighMesh_{};
    Aevrynd::MeshResource3D cottageLowMesh_{};
    Aevrynd::MeshResource3D riverMesh_{};
    Aevrynd::MeshResource3D skyDomeMesh_{};
    Aevrynd::MeshResource3D mountainHighMesh_{};
    Aevrynd::MeshResource3D mountainLowMesh_{};
    Aevrynd::MeshResource3D rockHighMesh_{};
    Aevrynd::MeshResource3D rockLowMesh_{};
    Aevrynd::MeshResource3D probeMesh_{};
    Aevrynd::MeshResource3D sphereHighMesh_{};
    Aevrynd::MeshResource3D sphereLowMesh_{};
    Aevrynd::MeshLodResourceChain3D automaticSphereLod_{};
    struct TerrainChunkResources3D {Aevrynd::MeshResource3D high{},medium{},low{};};
    std::vector<TerrainChunkResources3D> terrainChunks_{};
    Aevrynd::TerrainClipmap3D terrainClipmap_{};
    Aevrynd::TerrainClipmapStats3D terrainClipmapStats_{};
    Aevrynd::HeightField3D terrain_{};
    Aevrynd::SoundHandle audioPing_{};
    Aevrynd::CameraRig3D cameraRig_{};
    Aevrynd::Camera3D camera_{};
    Aevrynd::ParticleSystem2D effects_{512};
    Aevrynd::Skeleton3D animationSkeleton_{};
    Aevrynd::AnimationClip3D animationClip_{};
    Aevrynd::AnimationPose3D animationPose_{};
    Aevrynd::WorldStreamer3D worldStreamer_{};
    Aevrynd::WorldStreamingStats3D worldStreamingStats_{};
    Aevrynd::NavigationGrid3D navigationGrid_{};
    Aevrynd::NavigationPath3D navigationPath_{};
    Aevrynd::NavigationQueryWorkspace3D navigationWorkspace_{};
    Aevrynd::PhysicsWorld3D physicsWorld_{};
    Aevrynd::WorldBuilder3D worldBuilder_{};
    std::vector<Aevrynd::WorldObjectHandle3D> authoredDynamicObjects_{};
    Aevrynd::MaterialLibrary3D materialLibrary_{};
    Aevrynd::MaterialLibraryStats3D materialLibraryStats_{};
    Aevrynd::Material3D groundMaterial_{};
    Aevrynd::Material3D woodMaterial_{};
    Aevrynd::Material3D brickMaterial_{};
    std::size_t landscapePlacementInstances_{},landscapePlacementAttempts_{};
    std::size_t lastReusableMemoryReleasedBytes_{};std::uint64_t reusableMemoryTrimCount_{};
    int cameraMode_{};
    int effectCycle_{-1};
    int physicsImpulseCycle_{-1};
    bool automatic_{true};
    bool cullingEnabled_{true};
    bool instancingEnabled_{true};
    bool backfaceCulling_{true};
    bool lightingEnabled_{true};
    std::filesystem::path captureDirectory_{};
    std::string gpuValidationRunId_{};
    std::size_t captureIndex_{};
    std::uint64_t captureFrameCounter_{};
    float nextCaptureTime_{1.5f};
    std::unique_ptr<Aevrynd::LoopbackNetworkTransport3D> networkTransport_{};
    std::unique_ptr<Aevrynd::ReplicationServer3D> replicationServer_{};
    std::unique_ptr<Aevrynd::ReplicationClient3D> replicationClientA_{},replicationClientB_{};
    std::unique_ptr<Aevrynd::WorldChunkDeltaStore3D> mutationStore_{};
    std::filesystem::path networkStoreRoot_{};
    std::unique_ptr<Aevrynd::WorldChunkDeltaStore3D> networkRestartStore_{};
    std::shared_ptr<EngineLabMutationTarget> mutationTarget_{};
    Aevrynd::ReplicationChunkInterest3D networkInterestA_{{1,2,32}},networkInterestB_{{1,2,32}};
    Aevrynd::ReplicationInterestDelta3D networkInterestDeltaA_{},networkInterestDeltaB_{};
    std::uint64_t networkInterestEnteredA_{},networkInterestLeftA_{},networkInterestEnteredB_{},networkInterestLeftB_{};
    std::uint64_t networkServerDespawns_{},networkServerSnapshotEntities_{};
    std::unique_ptr<Aevrynd::DurableMutationAuthority3D> mutationAuthority_{};
    std::uint64_t networkTick_{},networkDurableReceipts_{},networkDeniedMutations_{};
    std::uint64_t networkAuthorizedMutations_{};
    bool networkMutationSubmitted_{};
    bool networkRestartRequested_{},networkRestartVerified_{};
};
