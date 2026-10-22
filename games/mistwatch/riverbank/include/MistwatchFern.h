#pragma once
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/Mesh3D.h>
#include <array>
#include <string>
namespace Mistwatch {
inline constexpr float FernAssetScale=1.6f;
inline constexpr std::array<unsigned,4> FernVertexCounts{1660,1568,544,565};
inline constexpr std::array<unsigned,4> FernTriangleCounts{2384,2248,784,816};
inline std::string FernMeshName(unsigned variant) {
    if(variant>=4)return {};
    return "fern-02-lod0-node"+std::to_string(variant)+"-mesh"+std::to_string(variant)+"-primitive0-material0-fern-02.aemesh";
}
inline void NormalizeFernAsset(Aevrynd::Mesh3D& mesh) {
    // 僅等比放大 local geometry；不以最低葉卡頂點移動根部或套用原陳列偏移。
    for(auto& vertex:mesh.vertices){vertex.x*=FernAssetScale;vertex.y*=FernAssetScale;vertex.z*=FernAssetScale;}
}
inline Aevrynd::MaterialAssetDefinition3D FernMaterialDefinition() {
    using namespace Aevrynd;
    MaterialTextureSetDefinition3D textures{"materials/fern-base.dds","materials/fern-arm.dds",2048,"materials/fern-normal.dds"};
    textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
    textures.alphaMode=MaterialAlphaMode3D::Mask;
    textures.integrity[0]={5592580,"faebcc3eab0bc4a7c334e40e4229e7d35e5745119c2d0a50eef0db22e1e8d563"};
    textures.integrity[1]={5592580,"f3f3eaf5c28f1c70ecf466042dc10fc62ae0c66169746ba9833f9e63a1a79cf9"};
    textures.integrity[2]={5592580,"980126cb53f2e3407f14a066b1b96e9f9205eba67cff74b761d4ba11263bb860"};
    Material3D surface{{1,1,1,1},{1,0,0,1},true,{},1.f,true};
    surface.materialClass=MaterialClass3D::Foliage;
    surface.alphaMode=MaterialAlphaMode3D::Mask;surface.alphaCutoff=.5f;surface.doubleSided=true;
    return {"fern",textures,surface};
}
}
