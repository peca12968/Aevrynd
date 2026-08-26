#pragma once
#include <Aevrynd/Animation3D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/Effects2D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/Navigation3D.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/WorldStreamer3D.h>

class EngineLabGame final:public Aevrynd::IGame {
public:
    void OnStart(Aevrynd::Engine&)override;
    void OnUpdate(Aevrynd::Engine&,float)override;
    void OnRender(Aevrynd::Engine&,float)override;
    void OnStop(Aevrynd::Engine&)override;
private:
    Aevrynd::Scene3D scene_;
    Aevrynd::Scene3DStats sceneStats_{};
    Aevrynd::SceneQueryStats3D queryStats_{};
    Aevrynd::SceneHit3D sceneHit_{};
    Aevrynd::MeshResource3D boxMesh_{};
    Aevrynd::MeshResource3D pyramidMesh_{};
    Aevrynd::MeshResource3D probeMesh_{};
    Aevrynd::MeshResource3D sphereHighMesh_{};
    Aevrynd::MeshResource3D sphereLowMesh_{};
    Aevrynd::MeshResource3D terrainMesh_{};
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
    int cameraMode_{};
    int effectCycle_{-1};
    bool automatic_{true};
    bool cullingEnabled_{true};
    bool instancingEnabled_{true};
    bool backfaceCulling_{true};
    bool lightingEnabled_{true};
};
