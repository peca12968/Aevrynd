#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Aevrynd::MeshCollisionVoxel3D {
struct Vec3 { double x{}, y{}, z{}; };
struct Bounds { Vec3 minimum, maximum; };
struct CellIndex {
    std::int64_t x{}, y{}, z{};
    auto operator<=>(const CellIndex&) const = default;
};
struct MeshView { std::span<const Vec3> vertices; std::span<const std::uint32_t> indices; };
struct Settings {
    double spacing{.15};
    std::uint64_t maxVertices{2'000'000}, maxTriangles{2'000'000};
    std::uint64_t maxGridCells{10'000'000}, maxTriangleCellChecks{50'000'000};
    std::uint64_t maxOccupiedCells{1'000'000};
};
enum class ErrorCode {
    InvalidSettings, EmptyMesh, BadIndexCount, BadIndex, NonfiniteVertex,
    DegenerateTriangle, NonfiniteCalculation, GridCoordinateRange,
    VertexBudget, TriangleBudget, GridBudget, CheckBudget, OccupancyBudget,
    InvalidCellOrder, MergeInputBudget, MergeCheckBudget, MergeBoxBudget
};
class AuthorError : public std::runtime_error {
public:
    AuthorError(ErrorCode value, const char* text) : std::runtime_error(text), code(value) {}
    ErrorCode code;
};
struct AuthorStatistics {
    std::uint64_t sourceVertices{}, triangles{}, gridCells{}, triangleCellChecks{};
    std::uint64_t axisChecks{}, occupiedCells{};
    Bounds indexedBounds{};
};
struct SurfaceUnion {
    // 只有閉合格盒覆蓋三角表面；沒有宣稱根部或其他實體內部已填滿。
    std::vector<CellIndex> cells;
    AuthorStatistics statistics;
    double spacing{};
};
struct MergeSettings {
    std::uint64_t maxInputCells{100'000}, maxCellsPerBox{64};
    std::uint64_t maxPairChecks{5'000'000}, maxBoxes{100'000};
};
struct CellBox { CellIndex minimum, maximumExclusive; };
struct MergedUnion { std::vector<CellBox> boxes; std::uint64_t pairChecks{}; };

namespace Detail {
inline void Require(bool value, ErrorCode code, const char* reason) {
    if (!value) throw AuthorError(code, reason);
}
inline bool Finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline Vec3 Sub(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline double Dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline void ValidateSpacing(double spacing) {
    Require(std::isfinite(spacing) && spacing >= .125 && spacing <= .20,
        ErrorCode::InvalidSettings, "spacing must be finite and inside [0.125,0.20]");
}
// 世界格網固定錨在原點；限制整數範圍，以保留 double 的格界分辨率。
inline constexpr std::int64_t CoordinateLimit = std::int64_t{1} << 30;
inline std::int64_t FloorCoordinate(double coordinate, double spacing) {
    const double value=std::floor(coordinate/spacing);
    Require(std::isfinite(value) && value >= -double(CoordinateLimit)+2 && value <= double(CoordinateLimit)-2,
        ErrorCode::GridCoordinateRange, "world grid coordinate is not representable in author range");
    return static_cast<std::int64_t>(value);
}
inline CellBox CandidateRange(Bounds bounds, double spacing) {
    // 多留兩側一格，涵蓋除法捨入與剛好落在閉合格面上的三角形。
    return {{FloorCoordinate(bounds.minimum.x,spacing)-1,FloorCoordinate(bounds.minimum.y,spacing)-1,FloorCoordinate(bounds.minimum.z,spacing)-1},
        {FloorCoordinate(bounds.maximum.x,spacing)+2,FloorCoordinate(bounds.maximum.y,spacing)+2,FloorCoordinate(bounds.maximum.z,spacing)+2}};
}
inline std::uint64_t Volume(CellBox box, std::uint64_t limit, ErrorCode code) {
    std::uint64_t result=1;
    for (auto length : {box.maximumExclusive.x-box.minimum.x,box.maximumExclusive.y-box.minimum.y,box.maximumExclusive.z-box.minimum.z}) {
        Require(length>0,ErrorCode::GridCoordinateRange,"invalid integer grid range");
        const auto n=static_cast<std::uint64_t>(length);
        Require(n<=limit/result,code,"integer grid volume exceeds author budget");
        result*=n;
    }
    return result;
}
inline Bounds TriangleBounds(const std::array<Vec3,3>& triangle) {
    Bounds result{triangle[0],triangle[0]};
    for (auto p : triangle) {
        result.minimum={std::min(result.minimum.x,p.x),std::min(result.minimum.y,p.y),std::min(result.minimum.z,p.z)};
        result.maximum={std::max(result.maximum.x,p.x),std::max(result.maximum.y,p.y),std::max(result.maximum.z,p.z)};
    }
    return result;
}
inline Bounds IntegerBounds(CellBox box, double spacing) {
    const Bounds result{{double(box.minimum.x)*spacing,double(box.minimum.y)*spacing,double(box.minimum.z)*spacing},
        {double(box.maximumExclusive.x)*spacing,double(box.maximumExclusive.y)*spacing,double(box.maximumExclusive.z)*spacing}};
    Require(Finite(result.minimum)&&Finite(result.maximum),ErrorCode::NonfiniteCalculation,"nonfinite cell bounds");
    return result;
}
inline bool Intersects(const std::array<Vec3,3>& triangle, CellIndex cell, double spacing, AuthorStatistics& statistics) {
    const Vec3 center{(double(cell.x)+.5)*spacing,(double(cell.y)+.5)*spacing,(double(cell.z)+.5)*spacing};
    const std::array<Vec3,3> p{Sub(triangle[0],center),Sub(triangle[1],center),Sub(triangle[2],center)};
    const std::array<Vec3,3> edges{Sub(triangle[1],triangle[0]),Sub(triangle[2],triangle[1]),Sub(triangle[0],triangle[2])};
    const double coordinateMagnitude=std::max({std::abs(center.x),std::abs(center.y),std::abs(center.z),
        std::abs(triangle[0].x),std::abs(triangle[0].y),std::abs(triangle[0].z),
        std::abs(triangle[1].x),std::abs(triangle[1].y),std::abs(triangle[1].z),
        std::abs(triangle[2].x),std::abs(triangle[2].y),std::abs(triangle[2].z),spacing});
    const auto separated=[&](Vec3 axis) {
        Require(Finite(axis),ErrorCode::NonfiniteCalculation,"nonfinite SAT axis");
        if (axis.x==0 && axis.y==0 && axis.z==0) return false;
        ++statistics.axisChecks;
        const double a=Dot(p[0],axis), b=Dot(p[1],axis), c=Dot(p[2],axis);
        const double axisMagnitude=std::abs(axis.x)+std::abs(axis.y)+std::abs(axis.z);
        const double radius=spacing*.5*axisMagnitude;
        // 將座標平移、投影與格界運算誤差向外擴充；不以 epsilon 收縮表面。
        const double tolerance=128*std::numeric_limits<double>::epsilon()*
            (coordinateMagnitude*axisMagnitude+std::max({std::abs(a),std::abs(b),std::abs(c),radius}));
        Require(std::isfinite(a)&&std::isfinite(b)&&std::isfinite(c)&&std::isfinite(radius)&&std::isfinite(tolerance),
            ErrorCode::NonfiniteCalculation,"nonfinite SAT projection");
        return std::min({a,b,c})>radius+tolerance || std::max({a,b,c})<-radius-tolerance;
    };
    constexpr std::array<Vec3,3> boxAxes{{{1,0,0},{0,1,0},{0,0,1}}};
    for (auto axis : boxAxes) if (separated(axis)) return false;
    if (separated(Cross(edges[0],Sub(triangle[2],triangle[0])))) return false;
    for (auto edge : edges) for (auto axis : boxAxes) if (separated(Cross(edge,axis))) return false;
    return true;
}
} // 內部作者工具

inline Bounds CellBounds(CellIndex cell, double spacing) {
    Detail::ValidateSpacing(spacing);
    for (auto value : {cell.x,cell.y,cell.z}) Detail::Require(value>=-Detail::CoordinateLimit && value<Detail::CoordinateLimit,
        ErrorCode::GridCoordinateRange,"cell coordinate outside supported grid");
    return Detail::IntegerBounds({cell,{cell.x+1,cell.y+1,cell.z+1}},spacing);
}
inline SurfaceUnion AuthorSurfaceUnion(MeshView mesh, const Settings& settings={}) {
    using namespace Detail;
    ValidateSpacing(settings.spacing);
    Require(settings.maxVertices&&settings.maxTriangles&&settings.maxGridCells&&settings.maxTriangleCellChecks&&settings.maxOccupiedCells,
        ErrorCode::InvalidSettings,"all author budgets must be nonzero");
    Require(!mesh.vertices.empty()&&!mesh.indices.empty(),ErrorCode::EmptyMesh,"empty mesh");
    Require(mesh.indices.size()%3==0,ErrorCode::BadIndexCount,"indices must contain complete triangles");
    Require(mesh.vertices.size()<=settings.maxVertices,ErrorCode::VertexBudget,"source vertex budget exceeded");
    Require(mesh.indices.size()/3<=settings.maxTriangles,ErrorCode::TriangleBudget,"source triangle budget exceeded");
    for (auto p : mesh.vertices) Require(Finite(p),ErrorCode::NonfiniteVertex,"nonfinite source vertex");
    for (auto i : mesh.indices) Require(i<mesh.vertices.size(),ErrorCode::BadIndex,"source index out of range");
    SurfaceUnion result; result.spacing=settings.spacing;
    auto& stats=result.statistics; stats.sourceVertices=mesh.vertices.size(); stats.triangles=mesh.indices.size()/3;
    stats.indexedBounds={mesh.vertices[mesh.indices[0]],mesh.vertices[mesh.indices[0]]};
    for (auto i : mesh.indices) {
        const auto p=mesh.vertices[i]; auto& bounds=stats.indexedBounds;
        bounds.minimum={std::min(bounds.minimum.x,p.x),std::min(bounds.minimum.y,p.y),std::min(bounds.minimum.z,p.z)};
        bounds.maximum={std::max(bounds.maximum.x,p.x),std::max(bounds.maximum.y,p.y),std::max(bounds.maximum.z,p.z)};
    }
    stats.gridCells=Volume(CandidateRange(stats.indexedBounds,settings.spacing),settings.maxGridCells,ErrorCode::GridBudget);
    std::set<CellIndex> occupied;
    for (std::size_t face=0;face<mesh.indices.size();face+=3) {
        const std::array<Vec3,3> triangle{mesh.vertices[mesh.indices[face]],mesh.vertices[mesh.indices[face+1]],mesh.vertices[mesh.indices[face+2]]};
        const auto normal=Cross(Sub(triangle[1],triangle[0]),Sub(triangle[2],triangle[0]));
        Require(Finite(normal),ErrorCode::NonfiniteCalculation,"nonfinite triangle normal");
        Require(normal.x!=0||normal.y!=0||normal.z!=0,ErrorCode::DegenerateTriangle,"degenerate or unrepresentable triangle");
        const auto range=CandidateRange(TriangleBounds(triangle),settings.spacing);
        const auto checks=Volume(range,settings.maxTriangleCellChecks,ErrorCode::CheckBudget);
        Require(checks<=settings.maxTriangleCellChecks-stats.triangleCellChecks,ErrorCode::CheckBudget,"cumulative triangle-cell check budget exceeded");
        for (auto x=range.minimum.x;x<range.maximumExclusive.x;++x)
            for (auto y=range.minimum.y;y<range.maximumExclusive.y;++y)
                for (auto z=range.minimum.z;z<range.maximumExclusive.z;++z) {
                    ++stats.triangleCellChecks;
                    if (Intersects(triangle,{x,y,z},settings.spacing,stats)) {
                        occupied.insert({x,y,z});
                        Require(occupied.size()<=settings.maxOccupiedCells,ErrorCode::OccupancyBudget,"occupied-cell budget exceeded");
                    }
                }
    }
    result.cells.assign(occupied.begin(),occupied.end()); stats.occupiedCells=result.cells.size();
    return result;
}
inline MergedUnion MergeOccupiedCells(std::span<const CellIndex> cells, const MergeSettings& settings={}) {
    using namespace Detail;
    Require(settings.maxInputCells&&settings.maxCellsPerBox&&settings.maxPairChecks&&settings.maxBoxes,
        ErrorCode::InvalidSettings,"all merge budgets must be nonzero");
    Require(cells.size()<=settings.maxInputCells,ErrorCode::MergeInputBudget,"merge input-cell budget exceeded");
    MergedUnion result;
    for (std::size_t i=0;i<cells.size();++i) {
        Require(i==0||cells[i-1]<cells[i],ErrorCode::InvalidCellOrder,"merge cells must be sorted and unique");
        (void)CellBounds(cells[i],.125);
        const auto c=cells[i];result.boxes.push_back({c,{c.x+1,c.y+1,c.z+1}});
    }
    // 固定三軸各一遍排序與相鄰掃描；檢查次數不超過輸入格數的三倍。
    for (unsigned axis : {2u,1u,0u}) {
        const auto key=[axis](const CellBox& box) {
            const std::array<std::int64_t,3> low{box.minimum.x,box.minimum.y,box.minimum.z};
            const std::array<std::int64_t,3> high{box.maximumExclusive.x,box.maximumExclusive.y,box.maximumExclusive.z};
            const unsigned a=(axis+1)%3,b=(axis+2)%3;
            return std::array<std::int64_t,6>{low[a],high[a],low[b],high[b],low[axis],high[axis]};
        };
        std::sort(result.boxes.begin(),result.boxes.end(),[&](const auto& a,const auto& b){return key(a)<key(b);});
        std::vector<CellBox> packed; packed.reserve(result.boxes.size());
        for (const auto& box : result.boxes) {
            if (packed.empty()) { packed.push_back(box); continue; }
            Require(result.pairChecks<settings.maxPairChecks,ErrorCode::MergeCheckBudget,"merge pair-check budget exceeded");
            ++result.pairChecks;
            const auto previousKey=key(packed.back()),nextKey=key(box);
            bool same=true;
            for (unsigned i=0;i<4;++i) same&=previousKey[i]==nextKey[i];
            const auto av=Volume(packed.back(),settings.maxCellsPerBox,ErrorCode::MergeInputBudget);
            const auto bv=Volume(box,settings.maxCellsPerBox,ErrorCode::MergeInputBudget);
            if (!same||previousKey[5]!=nextKey[4]||av>settings.maxCellsPerBox-bv) {
                packed.push_back(box); continue;
            }
            auto& previous=packed.back();
            CellBox combined{previous.minimum,{std::max(previous.maximumExclusive.x,box.maximumExclusive.x),
                std::max(previous.maximumExclusive.y,box.maximumExclusive.y),std::max(previous.maximumExclusive.z,box.maximumExclusive.z)}};
            // 初始盒只有一個佔用格；另外兩軸完全一致的相鄰滿盒合併，體積必須等於兩者相加。
            Require(Volume(combined,settings.maxCellsPerBox,ErrorCode::MergeInputBudget)==av+bv,
                ErrorCode::MergeInputBudget,"merge would fill an unoccupied cell");
            previous=combined;
        }
        result.boxes=std::move(packed);
    }
    Require(result.boxes.size()<=settings.maxBoxes,ErrorCode::MergeBoxBudget,"merged-box budget exceeded");
    std::sort(result.boxes.begin(),result.boxes.end(),[](const auto& a,const auto& b){return std::tie(a.minimum,a.maximumExclusive)<std::tie(b.minimum,b.maximumExclusive);});
    return result;
}
inline Bounds BoxBounds(CellBox box, double spacing) {
    Detail::ValidateSpacing(spacing);
    for (auto c : {box.minimum,box.maximumExclusive}) for (auto n : {c.x,c.y,c.z})
        Detail::Require(n>=-Detail::CoordinateLimit&&n<=Detail::CoordinateLimit,ErrorCode::GridCoordinateRange,"box outside grid range");
    (void)Detail::Volume(box,std::numeric_limits<std::uint64_t>::max(),ErrorCode::GridBudget);
    return Detail::IntegerBounds(box,spacing);
}
} // 保守表面分格作者工具
