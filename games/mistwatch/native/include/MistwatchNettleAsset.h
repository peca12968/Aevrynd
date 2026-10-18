#pragma once
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/Mesh3D.h>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Mistwatch {
inline constexpr unsigned NettleVariantCount=6;
inline constexpr float NettleAssetScale=4.f;
inline constexpr std::string_view NettleAssetProfile="official-cc0-nettle-4x-v1";
inline constexpr std::array<unsigned,6> NettleVertexCounts{4585,4336,2736,849,5259,6258};
inline constexpr std::array<unsigned,6> NettleTriangleCounts{5896,5664,3592,1088,6984,8080};
struct NettleAssetBounds {
    Aevrynd::Vec3 minimum{},maximum{};
    float radius{},minimumY{},maximumY{};
};
inline std::string NettleMeshName(unsigned variant) {
    if(variant>=NettleVariantCount)return {};
    return "nettle-plant-lod0-node"+std::to_string(variant)+"-mesh"+std::to_string(variant)+
        "-primitive0-material0-nettle-plant.aemesh";
}
inline NettleAssetBounds MeasureNettleAssetBounds(const Aevrynd::Mesh3D& mesh) {
    if(!mesh.Valid()||mesh.vertices.size()>10000||mesh.indices.size()>30000)
        throw std::invalid_argument("Invalid nettle mesh topology");
    NettleAssetBounds result;const auto& first=mesh.vertices.front();result.minimum=result.maximum={first.x,first.y,first.z};
    for(const auto& vertex:mesh.vertices) {
        if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z)||
            !std::isfinite(vertex.u)||!std::isfinite(vertex.v))throw std::invalid_argument("Nonfinite nettle mesh vertex");
        const auto normal=Aevrynd::UnpackNormalSnorm8(vertex.normal);const float length=Aevrynd::Length({normal.x,normal.y,normal.z});
        if(!std::isfinite(length)||length<.95f||length>1.05f)throw std::invalid_argument("Invalid nettle mesh normal XYZ");
        result.minimum.x=(std::min)(result.minimum.x,vertex.x);result.minimum.y=(std::min)(result.minimum.y,vertex.y);
        result.minimum.z=(std::min)(result.minimum.z,vertex.z);result.maximum.x=(std::max)(result.maximum.x,vertex.x);
        result.maximum.y=(std::max)(result.maximum.y,vertex.y);result.maximum.z=(std::max)(result.maximum.z,vertex.z);
        result.radius=(std::max)(result.radius,std::hypot(vertex.x,vertex.z));
    }
    result.minimumY=result.minimum.y;result.maximumY=result.maximum.y;
    if(!std::isfinite(result.radius)||result.radius<=0||result.maximumY<=0||result.minimumY>=result.maximumY)
        throw std::invalid_argument("Flat or zero-radius nettle mesh");
    return result;
}
inline float NormalizeNettleAsset(Aevrynd::Mesh3D& mesh,unsigned variant) {
    if(variant>=NettleVariantCount||mesh.vertices.size()!=NettleVertexCounts[variant]||
        mesh.indices.size()!=std::size_t(NettleTriangleCounts[variant])*3)
        throw std::invalid_argument("Nettle variant or exact source topology mismatch");
    const auto original=MeasureNettleAssetBounds(mesh);
    auto candidate=mesh;
    for(auto& vertex:candidate.vertices) {
        const float inverseHeight=1.f-std::clamp(vertex.y/original.maximumY,0.f,1.f);
        // 明確作者等比四倍；保留真 UV、原 pivot、XYZ 法線、色彩與完整六型拓樸。
        vertex.x*=NettleAssetScale;vertex.y*=NettleAssetScale;vertex.z*=NettleAssetScale;
        // W 是風權重的反值；根部不動、尖端可動，不重新雕刻掃描葉面。
        vertex.normal=(vertex.normal&0x00ffffffu)|(Aevrynd::PackNormalSnorm8({},inverseHeight)&0xff000000u);
    }
    (void)MeasureNettleAssetBounds(candidate);candidate.InvalidateRaycastIndex();mesh=std::move(candidate);
    return NettleAssetScale;
}
inline Aevrynd::Mesh3D CreateNettleAsset(const Aevrynd::Mesh3D& source,unsigned variant) {
    auto result=source;(void)NormalizeNettleAsset(result,variant);return result;
}
inline Aevrynd::MaterialAssetDefinition3D NettleMaterialDefinition() {
    using namespace Aevrynd;
    MaterialTextureSetDefinition3D textures{"materials/nettle-base.dds","materials/nettle-arm.dds",2048,"materials/nettle-normal.dds"};
    textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;textures.alphaMode=MaterialAlphaMode3D::Mask;
    // 來源 cook 收據的順序為 base/normal/arm；此介面明確映射引擎的 base/arm/normal 順序。
    textures.integrity[0]={5592580,"1e875aa8967262dfef1cb8b3b372527eee308071948865489d263630fb826fb1"};
    textures.integrity[1]={5592580,"895365555a197cf5f86feb088858cb0700cfc27f0ba05477bbd9407783cbacd0"};
    textures.integrity[2]={5592580,"85729c0b6920f4d2c640c7fc038506c9071450de103a44ca2b6f2bde5451790f"};
    Material3D surface{{1,1,1,1},{1,0,0,1},true,{},1.f,true};
    surface.materialClass=MaterialClass3D::Foliage;surface.alphaMode=MaterialAlphaMode3D::Mask;
    surface.alphaCutoff=.5f;surface.doubleSided=true;
    return {"nettle",textures,surface};
}
}
