#pragma once
#include "MistwatchHabitatLeafMaterial.h"
#include <stdexcept>

namespace Mistwatch {
// 兩個分區共用落葉與岩層；交界以近乎完整的共用覆蓋淡出不同基層。
inline Aevrynd::MaterialAssetDefinition3D EntranceRoadMaterialDefinition(
    const Aevrynd::MaterialAssetDefinition3D& forest) {
    if(HabitatGroundLayerSource(forest)!="forest-leaves-04-habitat"||
       forest.textures.baseColorPath!="ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg")
        throw std::invalid_argument("Entrance road requires moss, leaf and rock habitat material");
    auto road=forest;
    road.name="entrance-road";
    const auto soil=GroundMaterialDefinition(false,true);
    const auto layers=road.textures.additionalLayers;
    road.textures=soil.textures;
    road.textures.additionalLayers=layers;
    return road;
}
}
