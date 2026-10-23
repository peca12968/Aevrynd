#pragma once
#include "HeightField3D.h"
#include "Mesh3D.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace Aevrynd {
// Distinct, source-relative measurements. This is not a collision/adoption
// verdict: an overhanging or submerged vertex's terrain gap is not object height.
struct RigidTerrainMetrics3D {
    Vec3 worldMinimum{},worldMaximum{};
    double worldHeight{};
    double minimumTerrainGap{},maximumTerrainGap{};
    double maximumEmergence{};
    double minimumSupportGap{},maximumSupportGap{};
    std::size_t vertexCount{},supportVertexCount{};
};
// Support membership is supplied explicitly in source-vertex identity space.
// Emergence is max(y - max(terrain(x,z), optional waterPlane)); it is not an
// optical visibility/refraction claim. Vertex samples do not certify triangle,
// finite-thickness, continuous-time or global support/stability contacts.
// Invalid input returns false and leaves output unchanged.
inline bool TryMeasureRigidTerrainMetrics3D(const Mesh3D& mesh,const Transform3D& pose,
    const HeightField3D& terrain,std::span<const std::uint32_t> sourceSupportVertices,
    std::optional<float> waterPlane,RigidTerrainMetrics3D& output) {
    if(mesh.vertices.empty()||sourceSupportVertices.empty()||
       (waterPlane&&!std::isfinite(*waterPlane)))return false;
    for(float value:{pose.position.x,pose.position.y,pose.position.z,pose.rotation.x,pose.rotation.y,pose.rotation.z,
                     pose.scale.x,pose.scale.y,pose.scale.z})if(!std::isfinite(value))return false;
    if(pose.scale.x<=0||pose.scale.y<=0||pose.scale.z<=0)return false;
    std::vector<std::uint32_t> support(sourceSupportVertices.begin(),sourceSupportVertices.end());
    std::sort(support.begin(),support.end());
    if(support.back()>=mesh.vertices.size()||std::adjacent_find(support.begin(),support.end())!=support.end())return false;
    const auto matrix=pose.Matrix();
    for(const auto& row:matrix.values)for(float value:row)if(!std::isfinite(value))return false;
    RigidTerrainMetrics3D r;
    r.worldMinimum={std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()};
    r.worldMaximum={-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
    r.minimumTerrainGap=r.minimumSupportGap=std::numeric_limits<double>::infinity();
    r.maximumTerrainGap=r.maximumSupportGap=r.maximumEmergence=-std::numeric_limits<double>::infinity();
    std::size_t next{};
    for(std::size_t i=0;i<mesh.vertices.size();++i){
        const auto& v=mesh.vertices[i];
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return false;
        const auto p=TransformPoint(matrix,{v.x,v.y,v.z});
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!terrain.Contains(p.x,p.z))return false;
        const float bed=terrain.SampleHeight(p.x,p.z);if(!std::isfinite(bed))return false;
        const double gap=double(p.y)-bed;
        const double emergence=double(p.y)-(waterPlane?(std::max)(bed,*waterPlane):bed);
        r.worldMinimum={(std::min)(r.worldMinimum.x,p.x),(std::min)(r.worldMinimum.y,p.y),(std::min)(r.worldMinimum.z,p.z)};
        r.worldMaximum={(std::max)(r.worldMaximum.x,p.x),(std::max)(r.worldMaximum.y,p.y),(std::max)(r.worldMaximum.z,p.z)};
        r.minimumTerrainGap=(std::min)(r.minimumTerrainGap,gap);r.maximumTerrainGap=(std::max)(r.maximumTerrainGap,gap);
        r.maximumEmergence=(std::max)(r.maximumEmergence,emergence);
        if(next<support.size()&&support[next]==i){
            r.minimumSupportGap=(std::min)(r.minimumSupportGap,gap);r.maximumSupportGap=(std::max)(r.maximumSupportGap,gap);++next;
        }
    }
    r.worldHeight=double(r.worldMaximum.y)-r.worldMinimum.y;
    r.vertexCount=mesh.vertices.size();r.supportVertexCount=next;
    if(next!=support.size()||!std::isfinite(r.worldHeight)||!std::isfinite(r.maximumEmergence))return false;
    output=r;return true;
}
}
