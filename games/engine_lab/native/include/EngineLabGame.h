#pragma once
#include <Aevrynd/Engine.h>
#include <Aevrynd/Effects2D.h>
#include <Aevrynd/Scene3D.h>

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
    Aevrynd::SoundHandle audioPing_{};
    Aevrynd::CameraRig3D cameraRig_{};
    Aevrynd::Camera3D camera_{};
    Aevrynd::ParticleSystem2D effects_{512};
    int cameraMode_{};
    int effectCycle_{-1};
    bool automatic_{true};
    bool cullingEnabled_{true};
    bool instancingEnabled_{true};
    bool backfaceCulling_{true};
    bool lightingEnabled_{true};
};
