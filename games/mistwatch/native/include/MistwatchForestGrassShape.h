#pragma once
#include "MistwatchScannedGrass.h"
#include <bit>
#include <limits>
#include <string_view>

namespace Mistwatch {
inline constexpr std::string_view ForestGrassShapeProfile="source-uv-curved-v1";
struct ForestGrassShapeBounds {
    Aevrynd::Vec3 minimum{},maximum{};
    float rootMaximumRadius{},maximumHeight{};
};
inline ForestGrassShapeBounds MeasureForestGrassShapeBounds(const Aevrynd::Mesh3D& mesh) {
    if(!mesh.Valid()||mesh.vertices.size()>10000||mesh.indices.size()>60000)
        throw std::invalid_argument("Invalid forest grass shape topology");
    ForestGrassShapeBounds result;const auto& first=mesh.vertices.front();
    result.minimum=result.maximum={first.x,first.y,first.z};
    for(const auto& vertex:mesh.vertices) {
        if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z)||
            !std::isfinite(vertex.u)||!std::isfinite(vertex.v))
            throw std::invalid_argument("Nonfinite forest grass shape vertex");
        result.minimum.x=(std::min)(result.minimum.x,vertex.x);result.minimum.y=(std::min)(result.minimum.y,vertex.y);
        result.minimum.z=(std::min)(result.minimum.z,vertex.z);result.maximum.x=(std::max)(result.maximum.x,vertex.x);
        result.maximum.y=(std::max)(result.maximum.y,vertex.y);result.maximum.z=(std::max)(result.maximum.z,vertex.z);
        result.rootMaximumRadius=(std::max)(result.rootMaximumRadius,std::hypot(vertex.x,vertex.z));
        result.maximumHeight=(std::max)(result.maximumHeight,vertex.y);
    }
    if(!std::isfinite(result.rootMaximumRadius)||result.rootMaximumRadius<=0||result.maximumHeight<=0)
        throw std::invalid_argument("Flat or zero-radius forest grass shape");
    return result;
}
inline std::uint64_t ForestGrassShapeFingerprint(const Aevrynd::Mesh3D& mesh) {
    (void)MeasureForestGrassShapeBounds(mesh);std::uint64_t hash=1469598103934665603ull;
    const auto mix=[&](std::uint32_t word){for(unsigned byte=0;byte<4;++byte){hash^=(word>>(byte*8))&255u;hash*=1099511628211ull;}};
    mix(static_cast<std::uint32_t>(mesh.vertices.size()));mix(static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& vertex:mesh.vertices) {
        for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})mix(std::bit_cast<std::uint32_t>(value));
        mix(vertex.color);mix(vertex.normal);
    }
    for(const auto index:mesh.indices)mix(index);return hash;
}
namespace ForestGrassShapeDetail {
struct NormalSum {double x{},y{},z{};};
inline std::vector<NormalSum> GeometryNormals(const Aevrynd::Mesh3D& mesh) {
    std::vector<NormalSum> normals(mesh.vertices.size());
    for(std::size_t triangle=0;triangle<mesh.indices.size();triangle+=3) {
        const auto a=mesh.indices[triangle],b=mesh.indices[triangle+1],c=mesh.indices[triangle+2];
        const auto& p=mesh.vertices[a];const auto& q=mesh.vertices[b];const auto& r=mesh.vertices[c];
        const double ux=double(q.x)-p.x,uy=double(q.y)-p.y,uz=double(q.z)-p.z;
        const double vx=double(r.x)-p.x,vy=double(r.y)-p.y,vz=double(r.z)-p.z;
        const NormalSum face{uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx};
        const double squared=face.x*face.x+face.y*face.y+face.z*face.z;
        if(!std::isfinite(squared)||squared<=1e-30)
            throw std::invalid_argument("Degenerate forest grass shape triangle");
        // 不焊接頂點或猜測新 UV 島；硬邊、分離葉面與原拓樸保留。
        for(const auto index:{a,b,c}) {normals[index].x+=face.x;normals[index].y+=face.y;normals[index].z+=face.z;}
    }
    for(const auto normal:normals) {
        const double squared=normal.x*normal.x+normal.y*normal.y+normal.z*normal.z;
        if(!std::isfinite(squared)||squared<=1e-30)
            throw std::invalid_argument("Orphan or cancelling forest grass shape normal");
    }
    return normals;
}
}
inline Aevrynd::Mesh3D CreateForestGrassShape(const Aevrynd::Mesh3D& normalizedSource,unsigned variant) {
    if(variant>=5||normalizedSource.vertices.size()!=GrassVertexCounts[variant]||
        normalizedSource.indices.size()!=std::size_t(GrassTriangleCounts[variant])*3)
        throw std::invalid_argument("Forest grass shape variant or exact topology mismatch");
    const auto original=MeasureForestGrassShapeBounds(normalizedSource);
    if(original.minimum.x>0||original.maximum.x<0||original.minimum.z>0||original.maximum.z<0||
        original.minimum.y>0||original.maximumHeight<GrassAssetHeight-.0001f||
        original.rootMaximumRadius<GrassMinimumRadius-.0001f)
        throw std::invalid_argument("Forest grass shape requires normalized rooted source");
    (void)ForestGrassShapeDetail::GeometryNormals(normalizedSource);
    auto candidate=normalizedSource;
    constexpr std::array<float,5> compression{.36f,.32f,.38f,.34f,.35f};
    constexpr std::array<float,5> bendDirection{.7f,2.1f,3.4f,4.8f,5.9f};
    const float directionX=std::cos(bendDirection[variant]),directionZ=std::sin(bendDirection[variant]);
    // 尚未證實來源葉島語意，故只做真 UV 保留的連續形變，不偽造寬葉或重排貼圖。
    // 根部以下保持原位置；高處非線性壓短，中心线逐步彎向各型固定方向。
    for(auto& vertex:candidate.vertices)if(vertex.y>0) {
        const float t=std::clamp(vertex.y/original.maximumHeight,0.f,1.f),curve=t*t;
        vertex.y*=1.f-compression[variant]*t;
        const float inward=1.f-.18f*curve,bend=.10f*original.rootMaximumRadius*curve;
        vertex.x=std::clamp(vertex.x*inward+directionX*bend,original.minimum.x,original.maximum.x);
        vertex.z=std::clamp(vertex.z*inward+directionZ*bend,original.minimum.z,original.maximum.z);
        // 捨入亦不能超出原保守根半徑；固定一次修正，不以重試迴圈掩蓋失敗。
        const float radius=std::hypot(vertex.x,vertex.z);
        if(radius>original.rootMaximumRadius) {
            const float scale=std::nextafter(original.rootMaximumRadius/radius,0.f);vertex.x*=scale;vertex.z*=scale;
        }
    }
    const auto changed=MeasureForestGrassShapeBounds(candidate);
    if(changed.maximumHeight>=original.maximumHeight||changed.rootMaximumRadius>original.rootMaximumRadius)
        throw std::invalid_argument("Forest grass shape conservative envelope exceeded");
    const auto normals=ForestGrassShapeDetail::GeometryNormals(candidate);
    for(std::size_t index=0;index<candidate.vertices.size();++index) {
        auto& vertex=candidate.vertices[index];const auto sum=normals[index];
        const double length=std::sqrt(sum.x*sum.x+sum.y*sum.y+sum.z*sum.z);
        const Aevrynd::Vec3 normal{static_cast<float>(sum.x/length),static_cast<float>(sum.y/length),static_cast<float>(sum.z/length)};
        const float inverseHeight=1.f-std::clamp(vertex.y/changed.maximumHeight,0.f,1.f);
        vertex.normal=Aevrynd::PackNormalSnorm8(normal,inverseHeight);
        if(vertex.x<original.minimum.x||vertex.x>original.maximum.x||vertex.y<original.minimum.y||vertex.y>original.maximum.y||
            vertex.z<original.minimum.z||vertex.z>original.maximum.z)
            throw std::invalid_argument("Forest grass shape original AABB exceeded");
    }
    candidate.InvalidateRaycastIndex();return candidate;
}
}
