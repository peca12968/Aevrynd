#pragma once
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/MeshCollisionVoxel3D.h>
#include <Aevrynd/MeshCollisionGridSolid3D.h>
#include <bit>

namespace Mistwatch::CliffCollision {
namespace Voxel=Aevrynd::MeshCollisionVoxel3D;
inline constexpr double Spacing=.20;
inline constexpr const char* Profile="cliff-world-surface-grid-020-v1";
struct Budgets {
    std::uint64_t vertices{200000},triangles{200000},gridCells{2000000},triangleCellChecks{20000000};
    std::uint64_t occupiedCells{200000},cellsPerBox{256},mergeChecks{600000},boxes{20000};
};
struct Result {
    std::vector<Aevrynd::BoxCollider3D> parts;
    std::vector<Voxel::CellIndex> cells;
    std::vector<Voxel::CellBox> cellBoxes; // 最大界線不包含該格。
    Voxel::AuthorStatistics statistics{};
    Budgets budgets{};
    std::uint64_t mergeChecks{},geometryIdentity{},cellIdentity{},boxIdentity{};
    double spacing{Spacing};
};
namespace Detail {
inline void Word(std::uint64_t& hash,std::uint64_t word) noexcept {
    for(unsigned byte=0;byte<8;++byte){hash^=(word>>(byte*8))&255u;hash*=1099511628211ull;}
}
inline constexpr std::uint64_t Offset=14695981039346656037ull;
inline void CheckBudgets(Budgets b){
    const Budgets hard{};
    const auto limit=[](std::uint64_t n,std::uint64_t maximum){
        if(!n||n>maximum)throw Voxel::AuthorError(Voxel::ErrorCode::InvalidSettings,"cliff collision budget outside fixed hard ceiling");
    };
    limit(b.vertices,hard.vertices);limit(b.triangles,hard.triangles);limit(b.gridCells,hard.gridCells);
    limit(b.triangleCellChecks,hard.triangleCellChecks);limit(b.occupiedCells,hard.occupiedCells);
    limit(b.cellsPerBox,hard.cellsPerBox);limit(b.mergeChecks,hard.mergeChecks);limit(b.boxes,hard.boxes);
}
}
// 輸入為最終變形後的世界座標 LOD0；此處不再套姿態，呼叫端需與繪圖及存檔身分一致。
// 這是 .20m 三角表面格的保守聯集，不是精確三角碰撞，也不填封閉體內部。
// 合併不跨空格；最大格對角包絡為 sqrt(3)*.20m，加上數值向外容差。
// 身分使用確定性 FNV 幾何校驗，不是密碼學雜湊。
inline Result Create(const Aevrynd::Mesh3D& worldMesh,Budgets budgets={}) {
    Detail::CheckBudgets(budgets);
    if(worldMesh.vertices.size()>budgets.vertices)
        throw Voxel::AuthorError(Voxel::ErrorCode::VertexBudget,"cliff source vertex budget");
    if(worldMesh.indices.size()/3>budgets.triangles)
        throw Voxel::AuthorError(Voxel::ErrorCode::TriangleBudget,"cliff source triangle budget");
    std::vector<Voxel::Vec3> vertices;vertices.reserve(worldMesh.vertices.size());
    auto geometry=Detail::Offset;Detail::Word(geometry,worldMesh.vertices.size());Detail::Word(geometry,worldMesh.indices.size());
    for(const auto& v:worldMesh.vertices){
        vertices.push_back({v.x,v.y,v.z});
        for(float n:{v.x,v.y,v.z})Detail::Word(geometry,std::bit_cast<std::uint32_t>(n));
    }
    for(auto index:worldMesh.indices)Detail::Word(geometry,index);
    Voxel::Settings settings;settings.spacing=Spacing;settings.maxVertices=budgets.vertices;
    settings.maxTriangles=budgets.triangles;settings.maxGridCells=budgets.gridCells;
    settings.maxTriangleCellChecks=budgets.triangleCellChecks;settings.maxOccupiedCells=budgets.occupiedCells;
    auto surface=Voxel::AuthorSurfaceUnion({vertices,worldMesh.indices},settings);
    const Voxel::MergeSettings merge{budgets.occupiedCells,budgets.cellsPerBox,budgets.mergeChecks,budgets.boxes};
    auto merged=Voxel::MergeOccupiedCells(surface.cells,merge);
    Result result;result.budgets=budgets;result.statistics=surface.statistics;result.mergeChecks=merged.pairChecks;
    result.geometryIdentity=geometry;result.cellIdentity=Detail::Offset;result.boxIdentity=Detail::Offset;
    Detail::Word(result.cellIdentity,std::bit_cast<std::uint64_t>(Spacing));
    Detail::Word(result.cellIdentity,surface.cells.size());
    for(const auto& c:surface.cells)for(auto n:{c.x,c.y,c.z})Detail::Word(result.cellIdentity,std::uint64_t(n));
    Detail::Word(result.boxIdentity,std::bit_cast<std::uint64_t>(Spacing));
    Detail::Word(result.boxIdentity,merged.boxes.size());
    result.parts.reserve(merged.boxes.size());
    for(const auto& b:merged.boxes){
        (void)Voxel::BoxBounds(b,Spacing); // 先驗整數範圍，才能轉成 int32。
        const Aevrynd::MeshCollisionGridSolid3D::CellBox inclusive{
            {std::int32_t(b.minimum.x),std::int32_t(b.minimum.y),std::int32_t(b.minimum.z)},
            {std::int32_t(b.maximumExclusive.x-1),std::int32_t(b.maximumExclusive.y-1),std::int32_t(b.maximumExclusive.z-1)}};
        const auto collider=Aevrynd::MeshCollisionGridSolid3D::ToCollider(inclusive,Spacing);
        for(const auto& c:{b.minimum,b.maximumExclusive})for(auto n:{c.x,c.y,c.z})Detail::Word(result.boxIdentity,std::uint64_t(n));
        for(float n:{collider.center.x,collider.center.y,collider.center.z,collider.extents.x,collider.extents.y,collider.extents.z})
            Detail::Word(result.boxIdentity,std::bit_cast<std::uint32_t>(n));
        result.parts.push_back(collider);
    }
    result.cells=std::move(surface.cells);result.cellBoxes=std::move(merged.boxes);return result;
}
}
