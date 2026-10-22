#pragma once
#include "MistwatchRangerHut.h"
#include <Aevrynd/MaterialLibrary3D.h>

namespace Mistwatch {
// 全部引用原始CC0 Worn Planks，不改像素；只新增巡林屋的獨立材質槽。
inline Aevrynd::MaterialAssetDefinition3D HutWeatheredWoodDefinition() {
    Aevrynd::MaterialAssetDefinition3D definition;
    definition.name="hut-weathered-wood";
    auto& textures=definition.textures;
    textures.baseColorPath="poly-haven/worn_planks/1k/base-color.jpg";
    textures.roughnessPath="poly-haven/worn_planks/1k/roughness.jpg";
    textures.normalPath="poly-haven/worn_planks/1k/normal-dx.png";
    textures.maximumResolution=1024;
    textures.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR;
    textures.integrity[0]={943056,"698c09b337375a9aae12a76ad09ff2fda11d735e6c1bbb18d72875bd61988db6"};
    textures.integrity[1]={807719,"c79f640dc2d7ef81d69de463ef6676b7c540d598fb0e462a6eb39eb5462770a9"};
    textures.integrity[2]={4886175,"743411acf7332d8176d4e402237bb4b2d28a89fdc9ba160bb1eee2de6fbfb74b"};
    definition.material={{1,1,1,1},{.8f,0,0,.8f},true,{},1.f,true};
    definition.material.normalScale=.22f;
    return definition;
}
// 真載入的handle由MaterialLibrary給出，不能將原共用木材handle冒充新atlas。
inline Aevrynd::Material3D HutWeatheredWoodMaterial(Aevrynd::Material3D originalWood,
    Aevrynd::Material3D loadedHut,RangerHutArtSettings settings,bool noNormal=false) {
    if(!(settings.authoredUv&&settings.visibleTrim))return RangerHutMaterial(originalWood,settings,noNormal);
    if(!loadedHut.textures)throw std::invalid_argument("Weathered hut texture set absent");
    loadedHut.useUv=true;loadedHut.textureScale=1.f;loadedHut.normalScale=noNormal?0.f:.22f;
    return loadedHut;
}
}
