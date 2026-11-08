#pragma once
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/WaterMesh3D.h>
#include "MistwatchRiverProfile.h"
namespace Mistwatch {
inline float RiverShoreZ(const Aevrynd::HeightField3D& terrain,float x,bool farBank) {
    const float center=RiverCenterZ(x);
    float dry=center+(farBank?1.f:-1.f)*RiverOuterWidth(x),wet=center;
    for(unsigned step=0;step<24;++step) {
        const float middle=(dry+wet)*.5f;
        if(terrain.SampleHeight(x,middle)>RiverHeight)dry=middle;else wet=middle;
    }
    return (dry+wet)*.5f;
}
inline Aevrynd::Mesh3D CreateRiverMesh(const Aevrynd::HeightField3D& terrain) {
    return Aevrynd::CreateHeightFieldWaterMesh3D(terrain,RiverHeight);
}
inline Aevrynd::WaterSurfaceSettings3D RiverSurface() {
    Aevrynd::WaterSurfaceSettings3D surface;
    // 以暴雨後懸浮泥砂為美術參考。係數為本場景調色，河面反射率與岸邊泡沫沿用原設定。
    // https://www.usgs.gov/water-science-school/science/turbidity-and-water
    surface.absorption={.24f,.35f,.47f};surface.deepColor={.038f,.034f,.022f};
    surface.minimumRoughness=.065f;surface.maximumRoughness=.21f;
    surface.broadWaveStrength=.025f;surface.detailWaveStrength=.012f;
    surface.foamStartDepth=.02f;surface.foamEndDepth=.16f;surface.foamStrength=.18f;
    surface.shallowAlpha=.38f;surface.deepAlpha=.86f;
    return Aevrynd::NormalizeWaterSurfaceSettings3D(surface);
}
inline Aevrynd::Material3D RiverMaterial() {
    const auto surface=RiverSurface();
    Aevrynd::Material3D material{{surface.deepColor.x,surface.deepColor.y,surface.deepColor.z,surface.deepAlpha},
        {surface.maximumRoughness,0,0,1},false};
    material.materialClass=Aevrynd::MaterialClass3D::Water;
    // 引擎 Water 類別自動送往 Transparent；通用 Blend 模式目前不接受靜態實例。
    material.alphaMode=Aevrynd::MaterialAlphaMode3D::Opaque;
    material.doubleSided=true;
    return material;
}
}
