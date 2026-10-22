#pragma once
#include "GroundForestWoodArt.h"

namespace Mistwatch {
// 棲地分區使用真正落葉土；原苔地基層、岩層與所有材質參數保留。
inline void ApplyHabitatLeafLayer(Aevrynd::MaterialAssetDefinition3D& definition,
    bool authoredHabitat,bool neutralBase=false,bool neutralSurface=false) {
    if(!authoredHabitat||definition.material.materialClass!=Aevrynd::MaterialClass3D::Terrain)return;
    auto& leaf=definition.textures.additionalLayers[0];
    if(!neutralBase) {
        leaf.baseColorPath="poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png";
        leaf.integrity[0]={10402792,"40b1786afa17c9930ccae93a9ca75880fc52fc34b96453489c767b2cc82d7ee9"};
    }
    if(!neutralSurface) {
        leaf.roughnessPath="poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png";
        leaf.integrity[1]={6083648,"713874257d143af49c482ce8b7648cb7c5c748ca6540db0e44563a04a9fe09ce"};
    }
    leaf.normalPath="poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png";
    leaf.integrity[2]={11539651,"19bbaa0eb67eda146129946b9605cfc9b472a302ba1f75eb8ea0b48aa21baa33"};
    leaf.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR;
}
inline Aevrynd::MaterialAssetDefinition3D HabitatGroundDefinition(bool forestWood,bool authoredHabitat,
    bool legacy=false,bool singleSurface=false) {
    auto definition=GroundForestWoodArtDefinition(forestWood,legacy,singleSurface);
    ApplyHabitatLeafLayer(definition,forestWood&&authoredHabitat);
    return definition;
}
inline Aevrynd::MaterialAssetDefinition3D HabitatGroundDiagnosticDefinition(bool forestWood,bool authoredHabitat,
    bool legacy,bool neutralBase,bool neutralSurface,bool singleSurface=false) {
    auto definition=GroundForestWoodArtDiagnosticDefinition(forestWood,legacy,neutralBase,neutralSurface,singleSurface);
    ApplyHabitatLeafLayer(definition,forestWood&&authoredHabitat,neutralBase,neutralSurface);
    return definition;
}
// 描述實際槽與封印，而不是以請求旗標冒稱成功上傳。
inline std::string_view HabitatGroundLayerSource(const Aevrynd::MaterialAssetDefinition3D& definition) {
    const auto& layer=definition.textures.additionalLayers[0];
    if(layer.baseColorPath.empty())return "absent";
    if(layer.baseColorPath=="diagnostics/neutral-base.bmp")return "neutral-base";
    if(layer.baseColorPath=="ambientcg/ground108/slot-0-base.dds")return "ground108-gravel";
    if(definition.material.materialClass!=Aevrynd::MaterialClass3D::Terrain||
        layer.baseColorPath!="poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png"||
        layer.integrity[0].expectedBytes!=10402792||layer.integrity[0].expectedSha256!="40b1786afa17c9930ccae93a9ca75880fc52fc34b96453489c767b2cc82d7ee9"||
        layer.normalPath!="poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png"||
        layer.integrity[2].expectedBytes!=11539651||layer.integrity[2].expectedSha256!="19bbaa0eb67eda146129946b9605cfc9b472a302ba1f75eb8ea0b48aa21baa33"||
        layer.surfaceEncoding!=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR)return "unrecognized";
    const bool originalSurface=layer.roughnessPath=="poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png"&&
        layer.integrity[1].expectedBytes==6083648&&layer.integrity[1].expectedSha256=="713874257d143af49c482ce8b7648cb7c5c748ca6540db0e44563a04a9fe09ce";
    const bool neutralSurface=layer.roughnessPath=="diagnostics/neutral-surface.bmp"&&
        layer.integrity[1].expectedBytes==0&&layer.integrity[1].expectedSha256.empty();
    return originalSurface||neutralSurface?"forest-leaves-04-habitat":"unrecognized";
}
}
