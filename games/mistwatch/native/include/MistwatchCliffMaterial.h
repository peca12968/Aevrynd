#pragma once
#include <Aevrynd/MaterialLibrary3D.h>

namespace Mistwatch::CliffMaterial {
// 只替換地形岩層的三張原始官方貼圖；不改道路、地形頂點、岩層權重或投影尺度。
// 套用整個 core/ring 岩層，不宣稱僅限西側地理範圍；原地面定義仍供道路使用。
inline Aevrynd::MaterialAssetDefinition3D Definition(Aevrynd::MaterialAssetDefinition3D original) {
    if(original.material.materialClass!=Aevrynd::MaterialClass3D::Terrain||
       original.textures.maximumResolution!=2048||original.textures.additionalLayers[1].baseColorPath!=
           std::filesystem::path("poly-haven/rock_surface/1k/base-color.jpg"))
        throw std::runtime_error("Cliff material requires canonical layered ground");
    original.name="cliff-rock-face-03-2k";
    auto& layer=original.textures.additionalLayers[1];
    layer.baseColorPath="poly-haven/rock_face_03/2k/base-color.jpg";
    layer.roughnessPath="poly-haven/rock_face_03/2k/roughness.jpg";
    layer.normalPath="poly-haven/rock_face_03/2k/normal-dx.jpg";
    layer.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR;
    layer.integrity={{{3814362,"644ee79df7ffbf5f6afc330b64dd3f33794bab8820915fe61ffe0c88ecfd7ece"},
        {2601552,"9ca3e9e196685f702328f24d66f039b72f5330fc35cf2a2c265cfa50a7916301"},
        {4462567,"83b039065bff5de79ca55674c59361152d808fe3779498e5c2ee0725c1172faa"}}};
    return original;
}
}
