#pragma once
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/Mesh3D.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
namespace Mistwatch {
inline constexpr float GrassAssetHeight=1.02338f;
inline constexpr float GrassMinimumRadius=.584f;
inline constexpr float GrassSelectionDistance=35.f;
inline bool GrassRootSelected(Aevrynd::Vec3 root,Aevrynd::Vec3 feet) {
    // 選擇範圍量根部到玩家腳的三維距離，與鏡頭投影距離分開；正好邊界仍包含。
    const float distance=Aevrynd::Length(root-feet);
    return std::isfinite(distance)&&distance<=GrassSelectionDistance;
}
inline constexpr std::array<unsigned,5> GrassVertexCounts{645,1575,1072,2197,1542};
inline constexpr std::array<unsigned,5> GrassTriangleCounts{714,1760,1215,2489,1664};
inline std::string GrassMeshName(unsigned variant) {
    if(variant>=5)return {};
    return "grass-medium-02-lod0-node"+std::to_string(variant)+"-mesh"+std::to_string(variant)+"-primitive0-material0-grass-medium-02.aemesh";
}
inline float NormalizeGrassAsset(Aevrynd::Mesh3D& mesh) {
    if(!mesh.Valid())throw std::runtime_error("Invalid scanned grass mesh");
    float height{},radius{};
    for(const auto& v:mesh.vertices){height=(std::max)(height,v.y);radius=(std::max)(radius,std::hypot(v.x,v.z));}
    if(!std::isfinite(height)||height<=0.f||!std::isfinite(radius)||radius<=0.f)throw std::runtime_error("Invalid scanned grass bounds");
    const float scale=(std::max)(GrassAssetHeight/height,GrassMinimumRadius/radius);
    for(auto& v:mesh.vertices){
        // 保留原 local pivot、UV、XYZ 法線與拓樸；等比維持既有尖高及最小足跡，不靠縮小草叢改善。
        const float weight=1.f-(std::clamp)(v.y/height,0.f,1.f);
        v.x*=scale;v.y*=scale;v.z*=scale;
        // normal.w 是引擎風權重的反值；根固定、尖可動，不把原檔固定 W=1 當成風擺。
        v.normal=(v.normal&0x00ffffffu)|(Aevrynd::PackNormalSnorm8({},weight)&0xff000000u);
    }
    return scale;
}
inline Aevrynd::MaterialAssetDefinition3D GrassMaterialDefinition() {
    using namespace Aevrynd;
    MaterialTextureSetDefinition3D textures{"materials/grass-base.dds","materials/grass-arm.dds",2048,"materials/grass-normal.dds"};
    textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
    textures.alphaMode=MaterialAlphaMode3D::Mask;
    textures.integrity[0]={5592580,"db0aed48e90590d5e606cc3a29844398c015fa72fa1058641082dbb7db5b4ca9"};
    textures.integrity[1]={5592580,"2bc94e52366159f6528689f5c813ec94e2be0ee92d193582c947526ef0de9b44"};
    textures.integrity[2]={5592580,"c4b2d6d7815092947b186dd57b99af218506ef5fa0408d5e6b476802498836d5"};
    Material3D surface{{1,1,1,1},{1,0,0,1},true,{},1.f,true};
    surface.materialClass=MaterialClass3D::Foliage;
    surface.alphaMode=MaterialAlphaMode3D::Mask;surface.alphaCutoff=.5f;surface.doubleSided=true;
    return {"grass",textures,surface};
}
}
