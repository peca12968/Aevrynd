#pragma once
#include <Aevrynd/Mesh3D.h>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace Mistwatch {

struct BridgeWorksiteGeometry {
    Aevrynd::Mesh3D wood;
    Aevrynd::Mesh3D metal;
    std::uint64_t fingerprint{};
};
inline constexpr std::uint64_t BridgeWorksiteFingerprintV1=550614854349441485ull;

namespace BridgeWorksiteDetail {
inline void Mix(std::uint64_t& hash,std::uint32_t word) noexcept {
    for(unsigned byte=0;byte<4;++byte){hash^=(word>>(byte*8))&255u;hash*=1099511628211ull;}
}
inline void Mix(std::uint64_t& hash,float value) noexcept {Mix(hash,std::bit_cast<std::uint32_t>(value));}
inline void AppendBox(Aevrynd::Mesh3D& destination,Aevrynd::Vec3 center,Aevrynd::Vec3 size,
    Aevrynd::Color tint,Aevrynd::Vec3 rotation={}) {
    using namespace Aevrynd;
    if(size.x<=0||size.y<=0||size.z<=0)throw std::invalid_argument("Invalid bridge-worksite part size");
    const auto source=Mesh3D::CreateUnitBox();
    const auto transform=CreateTransformMatrix(center,rotation,size);
    const auto normals=CreateNormalMatrix(transform);
    const auto base=static_cast<std::uint32_t>(destination.vertices.size());
    constexpr std::array<Vec2,4> faceUv{{{0,1},{1,1},{1,0},{0,0}}};
    for(std::size_t index=0;index<source.vertices.size();++index) {
        auto vertex=source.vertices[index];
        const auto point=TransformPoint(transform,{vertex.x,vertex.y,vertex.z});
        const auto unpacked=UnpackNormalSnorm8(vertex.normal);
        vertex.x=point.x;vertex.y=point.y;vertex.z=point.z;
        vertex.normal=PackNormalSnorm8(TransformNormal(normals,{unpacked.x,unpacked.y,unpacked.z}));
        vertex.color=PackColorRgba8(tint);
        vertex.u=faceUv[index%4].x;vertex.v=faceUv[index%4].y;
        destination.vertices.push_back(vertex);
    }
    for(const auto index:source.indices)destination.indices.push_back(base+index);
}
}

inline std::uint64_t BridgeWorksiteFingerprint(const BridgeWorksiteGeometry& geometry) noexcept {
    using namespace BridgeWorksiteDetail;
    std::uint64_t hash=14695981039346656037ull;
    for(const auto* mesh:{&geometry.wood,&geometry.metal}) {
        Mix(hash,static_cast<std::uint32_t>(mesh->vertices.size()));
        Mix(hash,static_cast<std::uint32_t>(mesh->indices.size()));
        for(const auto& vertex:mesh->vertices) {
            for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})Mix(hash,value);
            Mix(hash,vertex.normal);Mix(hash,vertex.color);
        }
        for(const auto index:mesh->indices)Mix(hash,index);
    }
    return hash;
}

// 幾何以舊方塊底面中央為局部原點；正式接線只平移到 Session::bridge+{1,0,0}。
// 所有可見頂點都留在 x/z ±0.5、y 0..0.7 內，不擴張碰撞或互動範圍。
inline BridgeWorksiteGeometry CreateBridgeWorksite() {
    using namespace Aevrynd;
    using BridgeWorksiteDetail::AppendBox;
    BridgeWorksiteGeometry result;
    const Color paleWood{.88f,.84f,.76f,1},middleWood{.78f,.70f,.59f,1},darkWood{.58f,.48f,.37f,1};
    const Color iron{.22f,.24f,.23f,1},toolSteel{.42f,.44f,.41f,1};

    // 兩根接地枕木與交錯備料形成「待施工」輪廓，不假裝已鋪成橋面。
    for(const float x:{-.28f,.28f})AppendBox(result.wood,{x,.05f,0},{.12f,.10f,.76f},darkWood);
    for(unsigned row=0;row<4;++row) {
        const float z=-.27f+.18f*float(row);
        const float shade=.94f-.07f*float(row%3);
        AppendBox(result.wood,{0,.16f,z},{.82f,.12f,.13f},{middleWood.r*shade,middleWood.g*shade,middleWood.b*shade,1});
    }
    for(unsigned row=0;row<3;++row) {
        const float z=-.17f+.17f*float(row);
        AppendBox(result.wood,{0,.27f,z},{.70f,.10f,.12f},row==1?paleWood:middleWood);
    }

    // 小型作業板與背架仍在原 1×0.7×1 公尺視覺盒內。
    AppendBox(result.wood,{0,.39f,.08f},{.78f,.08f,.42f},paleWood);
    for(const float x:{-.31f,.31f})AppendBox(result.wood,{x,.49f,.29f},{.06f,.28f,.06f},darkWood);
    AppendBox(result.wood,{0,.59f,.29f},{.68f,.08f,.07f},middleWood);

    // 束帶與一把橫放的短柄工具使用第二個既有金屬材質 mesh。
    for(const float x:{-.22f,.22f})AppendBox(result.metal,{x,.225f,-.337f},{.04f,.33f,.012f},iron);
    const float yaw=.42f;
    AppendBox(result.wood,{.02f,.455f,-.01f},{.42f,.035f,.035f},darkWood,{0,yaw,0});
    AppendBox(result.metal,{.215f,.457f,.075f},{.11f,.055f,.15f},toolSteel,{0,yaw,0});

    if(!result.wood.Valid()||!result.metal.Valid())throw std::runtime_error("Invalid bridge-worksite geometry");
    result.fingerprint=BridgeWorksiteFingerprint(result);
    if(result.fingerprint!=BridgeWorksiteFingerprintV1)throw std::runtime_error("Bridge-worksite geometry fingerprint changed");
    return result;
}

}
