#pragma once
#include <Aevrynd/Engine.h>
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
    Aevrynd::MeshHandle3D boxMesh_{};
    int cameraMode_{};
    bool automatic_{true};
    bool cullingEnabled_{true};
    bool instancingEnabled_{true};
};
