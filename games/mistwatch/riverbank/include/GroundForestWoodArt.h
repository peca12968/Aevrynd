#pragma once
#include "MistwatchGroundMaterial.h"
#include <string_view>

namespace Mistwatch {
// 只替換ForestWood三層Terrain的基層；不改遮罩、材質參數或任何地物。
inline void ApplyGroundForestWoodArtBase(Aevrynd::MaterialAssetDefinition3D& definition,
    bool forestWood,bool neutralBase=false,bool neutralSurface=false) {
    if(!forestWood||definition.material.materialClass!=Aevrynd::MaterialClass3D::Terrain)return;
    auto& textures=definition.textures;
    if(!neutralBase) {
        textures.baseColorPath="ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg";
        textures.integrity[0]={2187764,"d92bec8a35b9cb16ec79b421ca0f572a7400286eae109beafa266629ef49a6cf"};
    }
    if(!neutralSurface) {
        textures.roughnessPath="ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg";
        textures.integrity[1]={694944,"3786b088e981aae562f06b6477972d379cffd09a5cc542ab42f9ffd94f9bcb0e"};
    }
    // 真來源已為DirectX normal；不反轉綠通道、不把高度圖當法線。
    textures.normalPath="ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg";
    textures.integrity[2]={2474498,"0e2d613d117d9231aa0aa61f39b19755f891b4ed1d1599a004cb018a8224e039"};
    textures.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR;
}
inline Aevrynd::MaterialAssetDefinition3D GroundForestWoodArtDefinition(bool forestWood,
    bool legacy=false,bool singleSurface=false) {
    auto definition=GroundMaterialDefinition(legacy,singleSurface);
    ApplyGroundForestWoodArtBase(definition,forestWood);
    return definition;
}
inline Aevrynd::MaterialAssetDefinition3D GroundForestWoodArtDiagnosticDefinition(bool forestWood,
    bool legacy,bool neutralBase,bool neutralSurface,bool singleSurface=false) {
    auto definition=GroundDiagnosticDefinition(legacy,neutralBase,neutralSurface,singleSurface);
    ApplyGroundForestWoodArtBase(definition,forestWood,neutralBase,neutralSurface);
    return definition;
}
// 收據取最終實際definition，不由請求flag猜測材質。
inline std::string_view GroundForestWoodArtSource(const Aevrynd::MaterialAssetDefinition3D& definition) {
    if(definition.textures.baseColorPath=="diagnostics/neutral-base.bmp")return "neutral-base";
    if(definition.textures.baseColorPath=="ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg")
        return definition.material.materialClass==Aevrynd::MaterialClass3D::Terrain?"ground037-moss-floor":"ground037";
    if(definition.textures.baseColorPath=="poly-haven/forest-ground-04/slot-0-base.dds")return "forest-ground-04";
    return "unrecognized";
}
}
