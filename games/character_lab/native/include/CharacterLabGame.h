#pragma once
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/Scene3D.h>

class CharacterLabGame final:public Aevrynd::IGame {
public:
    void OnStart(Aevrynd::Engine&)override;
    void OnUpdate(Aevrynd::Engine&,float)override;
    void OnRender(Aevrynd::Engine&,float)override;
    void OnStop(Aevrynd::Engine&)override;
private:
    void BuildEnvironment();
    Aevrynd::CharacterController3D controller_{};
    Aevrynd::CollisionWorld3D collisionWorld_;
    Aevrynd::Scene3D scene_;
    Aevrynd::Scene3DStats sceneStats_{};
    Aevrynd::SceneQueryStats3D queryStats_{};
    Aevrynd::SceneHit3D aimHit_{};
    Aevrynd::MeshResource3D boxMesh_{};
    Aevrynd::SoundHandle jumpSound_{};
    Aevrynd::Camera3D camera_{};
    float yaw_{},pitch_{.12f};
    int automaticPhase_{-1},lastJumpCycle_{-1};
    uint64_t modeSwitches_{},cameraObstructions_{};
    bool firstPerson_{},automatic_{true};
};
