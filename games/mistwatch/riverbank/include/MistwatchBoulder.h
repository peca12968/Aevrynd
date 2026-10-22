#pragma once
#include <Aevrynd/Mesh3D.h>
namespace Mistwatch {
// CC0 boulder-01 LOD2 左手系；以最長水平跨度一公尺保留掃描比例。
inline constexpr Aevrynd::Vec3 BoulderRoot{-.11363622546f,-.07774113119f,.02950257063f};
inline constexpr float BoulderScale=.54416590434f;
inline constexpr const char* BoulderMeshName="boulder-01-lod2-node0-mesh0-primitive0-material0-boulder-01.aemesh";
inline Aevrynd::MeshBounds3D BoulderBounds() {
    const Aevrynd::Vec3 extents{.3469667836f,.2747509063f,.5f};
    return {{0,.2747509063f,0},Aevrynd::Length(extents),extents};
}
inline void RootBoulderMesh(Aevrynd::Mesh3D& mesh) {
    for(auto& v:mesh.vertices) {
        v.x=(v.x-BoulderRoot.x)*BoulderScale;
        v.y=(v.y-BoulderRoot.y)*BoulderScale;
        v.z=(v.z-BoulderRoot.z)*BoulderScale;
    }
    mesh.InvalidateRaycastIndex();
}
}
