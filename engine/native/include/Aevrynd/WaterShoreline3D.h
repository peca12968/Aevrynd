#pragma once
#include <Aevrynd/WaterMesh3D.h>
#include <algorithm>
#include <cmath>

namespace Aevrynd {
// The terrain triangle clip remains the authoritative shoreline. Water's
// existing dedicated vertex entry consumes normal.w as wave displacement
// weight; quantize downward so the encoded value never exceeds depth/fade.
// Callers must select fadeDepth >= their maximum vertical wave excursion.
inline Mesh3D CreateShoreWeightedHeightFieldWaterMesh3D(const HeightField3D& field,
    float height,float fadeDepth=.18f,float textureScale=.25f) {
    if(!std::isfinite(fadeDepth)||fadeDepth<=0)return {};
    auto mesh=CreateHeightFieldWaterMesh3D(field,height,textureScale);
    for(auto& v:mesh.vertices){
        const float depth=(std::max)(0.f,height-field.SampleHeight(v.x,v.z));
        const unsigned quantized=unsigned(std::floor((std::min)(1.f,depth/fadeDepth)*127.f));
        v.normal=PackNormalSnorm8({0,1,0},float(quantized)/127.f);
    }
    return mesh;
}
}
