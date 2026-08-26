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
    Aevrynd::MeshResource3D boxMesh_{};
    Aevrynd::MeshResource3D pyramidMesh_{};
    int cameraMode_{};
    bool automatic_{true};
    bool cullingEnabled_{true};
    bool instancingEnabled_{true};
    bool backfaceCulling_{true};
    bool lightingEnabled_{true};
};
