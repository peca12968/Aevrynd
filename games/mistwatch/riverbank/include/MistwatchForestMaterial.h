#pragma once
#include <Aevrynd/Mesh3D.h>

namespace Mistwatch {
inline Aevrynd::Material3D FirTwigMaterial() {
    Aevrynd::Material3D material{{1,1,1,1},{.85f,0,0,.8f},true,{},1.f,true};
    material.alphaMode=Aevrynd::MaterialAlphaMode3D::Mask;
    // 本作冷杉針葉在實機遠景採樣下的材質調校；不變更共用遮罩或測試門檻。
    material.alphaCutoff=.25f;
    material.doubleSided=true;
    material.materialClass=Aevrynd::MaterialClass3D::Foliage;
    return material;
}
}
