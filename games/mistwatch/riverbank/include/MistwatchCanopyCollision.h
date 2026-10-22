#pragma once
#include <Aevrynd/MeshCollisionVoxel3D.h>
#include <Aevrynd/MeshCollisionGridSolid3D.h>
#include <Aevrynd/Mesh3D.h>
#include <bit>

namespace Mistwatch {
struct CanopyWoodProxy {
    std::vector<Aevrynd::BoxCollider3D> parts;
    std::uint64_t sourceTriangles{},triangleCellChecks{},surfaceCells{},enclosedGridCells{},domainCells{};
    std::uint64_t fingerprint{14695981039346656037ull};
    double spacing{.15};
};
// 只用真正根幹及不透明木枝；葉片不形成整棵樹的空氣牆。
// 開放木枝不假設有可靠實體內外，填充僅定義保守碰撞格的封閉空間。
inline CanopyWoodProxy CreateCanopyWoodProxy(const Aevrynd::Mesh3D& rootedTrunk,
    const Aevrynd::Mesh3D& branches,const Aevrynd::Transform3D& pose) {
    namespace Author=Aevrynd::MeshCollisionVoxel3D;
    namespace Grid=Aevrynd::MeshCollisionGridSolid3D;
    const auto finite=[](Aevrynd::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    if(!finite(pose.position)||!finite(pose.rotation)||!finite(pose.scale)||pose.scale.x<=0||
       pose.scale.x!=pose.scale.y||pose.scale.x!=pose.scale.z||pose.scale.x>4)
        throw std::invalid_argument("Invalid canopy wood pose");
    std::vector<Author::Vec3> vertices;std::vector<std::uint32_t> indices;
    const auto matrix=pose.Matrix();
    for(const auto* mesh:{&rootedTrunk,&branches}){
        if(!mesh->Valid()||mesh->vertices.size()>600000||mesh->indices.size()>1800000)
            throw std::invalid_argument("Invalid bounded canopy wood mesh");
        const auto offset=std::uint32_t(vertices.size());
        for(const auto& v:mesh->vertices){const auto p=Aevrynd::TransformPoint(matrix,{v.x,v.y,v.z});vertices.push_back({p.x,p.y,p.z});}
        for(auto index:mesh->indices)indices.push_back(index+offset);
    }
    Author::Settings settings;
    settings.spacing=.15;settings.maxVertices=1200000;settings.maxTriangles=1200000;
    settings.maxOccupiedCells=100000;settings.maxGridCells=1000000;settings.maxTriangleCellChecks=10000000;
    const auto surface=Author::AuthorSurfaceUnion({vertices,indices},settings);
    std::vector<Grid::Cell> cells;cells.reserve(surface.cells.size());
    for(const auto& c:surface.cells){
        if(c.x<(std::numeric_limits<std::int32_t>::min)()||c.x>(std::numeric_limits<std::int32_t>::max)()||
           c.y<(std::numeric_limits<std::int32_t>::min)()||c.y>(std::numeric_limits<std::int32_t>::max)()||
           c.z<(std::numeric_limits<std::int32_t>::min)()||c.z>(std::numeric_limits<std::int32_t>::max)())
            throw std::invalid_argument("Canopy collision grid conversion overflow");
        cells.push_back({std::int32_t(c.x),std::int32_t(c.y),std::int32_t(c.z)});
    }
    const auto filled=Grid::FillEnclosed(cells,1000000);
    const auto packed=Grid::PackExact(filled.occupied,10000,10000000);
    CanopyWoodProxy result;
    result.sourceTriangles=surface.statistics.triangles;result.triangleCellChecks=surface.statistics.triangleCellChecks;
    result.surfaceCells=filled.surfaceCount;result.enclosedGridCells=filled.interiorCount;result.domainCells=filled.domainCount;
    result.parts.reserve(packed.size());
    for(const auto& box:packed){
        const auto collider=Grid::ToCollider(box,settings.spacing);result.parts.push_back(collider);
        for(float value:{collider.center.x,collider.center.y,collider.center.z,collider.extents.x,collider.extents.y,collider.extents.z}){
            const auto bits=std::bit_cast<std::uint32_t>(value);
            for(unsigned byte=0;byte<4;++byte){result.fingerprint^=(bits>>(byte*8))&255u;result.fingerprint*=1099511628211ull;}
        }
    }
    return result;
}
}
