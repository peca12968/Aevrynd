#include <Aevrynd/MeshCollisionVoxel3D.h>
#include <iostream>
#include <numeric>
#include <string_view>

using namespace Aevrynd::MeshCollisionVoxel3D;
#ifndef VOXEL_TEST_GROUP
#define VOXEL_TEST_GROUP 0
#endif

namespace {
unsigned checks{}, negatives{};
void Check(bool condition,const char* reason) {
    if (!condition) throw std::runtime_error(reason);
    ++checks;
}
template<class F> void Reject(ErrorCode expected,F&& action) {
    try { action(); }
    catch (const AuthorError& error) {
        Check(error.code==expected,"wrong strict-failure classification"); ++negatives; return;
    }
    throw std::runtime_error("invalid mesh or exceeded budget returned usable output");
}
struct OwnedMesh {
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
    MeshView View() const { return {vertices,indices}; }
};
OwnedMesh Triangle(Vec3 a,Vec3 b,Vec3 c) { return {{a,b,c},{0,1,2}}; }
double Component(Vec3 p,unsigned axis) { return axis==0?p.x:axis==1?p.y:p.z; }
// 獨立六平面 Sutherland-Hodgman 裁切，不使用作者 SAT、候選範圍或軸測試。
bool ClipIntersection(std::array<Vec3,3> triangle,Bounds box) {
    std::vector<Vec3> polygon(triangle.begin(),triangle.end());
    for (unsigned axis=0;axis<3;++axis) for (bool upper : {false,true}) {
        if (polygon.empty()) return false;
        const double boundary=Component(upper?box.maximum:box.minimum,axis);
        const auto signedDistance=[&](Vec3 p){return upper?boundary-Component(p,axis):Component(p,axis)-boundary;};
        std::vector<Vec3> clipped;
        for (std::size_t i=0;i<polygon.size();++i) {
            const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
            const double da=signedDistance(a),db=signedDistance(b);
            const bool insideA=da>=0,insideB=db>=0;
            if (insideA) clipped.push_back(a);
            if (insideA!=insideB) {
                const double t=da/(da-db);
                Vec3 intersection{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
                // 交點被獨立指定回本裁切平面，避免裁切器自身的重複捨入掉點。
                if (axis==0) intersection.x=boundary;
                else if (axis==1) intersection.y=boundary;
                else intersection.z=boundary;
                clipped.push_back(intersection);
            }
        }
        polygon=std::move(clipped);
    }
    return !polygon.empty();
}
Bounds ReferenceBounds(CellIndex c,double spacing) {
    return {{double(c.x)*spacing,double(c.y)*spacing,double(c.z)*spacing},
        {double(c.x+1)*spacing,double(c.y+1)*spacing,double(c.z+1)*spacing}};
}
SurfaceUnion CoverageCase(std::string_view name,const OwnedMesh& mesh,CellBox independentRange,double spacing) {
    Settings settings;settings.spacing=spacing;
    const auto result=AuthorSurfaceUnion(mesh.View(),settings);
    Check(!result.cells.empty(),"nondegenerate surface produced no cells");
    Check(std::is_sorted(result.cells.begin(),result.cells.end()),"cells are not sorted");
    Check(std::adjacent_find(result.cells.begin(),result.cells.end())==result.cells.end(),"duplicate cells");
    Check(result.statistics.occupiedCells==result.cells.size(),"occupied author count differs");
    Check(result.statistics.triangleCellChecks<=settings.maxTriangleCellChecks,"author exceeded check budget");
    Check(AuthorSurfaceUnion(mesh.View(),settings).cells==result.cells,"nondeterministic cells");
    std::uint64_t referenceCount{},interiorFaceCells{};
    for (auto x=independentRange.minimum.x;x<independentRange.maximumExclusive.x;++x)
        for (auto y=independentRange.minimum.y;y<independentRange.maximumExclusive.y;++y)
            for (auto z=independentRange.minimum.z;z<independentRange.maximumExclusive.z;++z) {
                const CellIndex cell{x,y,z}; const auto box=ReferenceBounds(cell,spacing);
                bool intersects=false;
                for (std::size_t face=0;face<mesh.indices.size();face+=3) {
                    intersects|=ClipIntersection({mesh.vertices[mesh.indices[face]],mesh.vertices[mesh.indices[face+1]],mesh.vertices[mesh.indices[face+2]]},box);
                }
                if (!intersects) continue;
                ++referenceCount;
                Check(std::binary_search(result.cells.begin(),result.cells.end(),cell),"independent clipped surface is missing from occupied cells");
                bool containsVertex=false;
                for (auto p : mesh.vertices) containsVertex|=p.x>=box.minimum.x&&p.x<=box.maximum.x&&
                    p.y>=box.minimum.y&&p.y<=box.maximum.y&&p.z>=box.minimum.z&&p.z<=box.maximum.z;
                interiorFaceCells+=!containsVertex;
            }
    Check(referenceCount>0,"independent clip reference is empty");
    for (auto c : result.cells) {
        const auto bounds=CellBounds(c,spacing),expected=ReferenceBounds(c,spacing);
        Check(bounds.minimum.x==expected.minimum.x&&bounds.minimum.y==expected.minimum.y&&bounds.minimum.z==expected.minimum.z&&
            bounds.maximum.x==expected.maximum.x&&bounds.maximum.y==expected.maximum.y&&bounds.maximum.z==expected.maximum.z,"world axis-aligned cell bounds differ");
    }
    std::cout<<"COVERAGE case="<<name<<" reference_cells="<<referenceCount<<" author_cells="<<result.cells.size()
        <<" no_vertex_cells="<<interiorFaceCells<<" candidate_checks="<<result.statistics.triangleCellChecks<<" axes="<<result.statistics.axisChecks<<'\n';
    return result;
}
OwnedMesh CubeSurface() {
    OwnedMesh result;
    for (double x : {-.5,.5}) for (double y : {-.5,.5}) for (double z : {-.5,.5}) result.vertices.push_back({x,y,z});
    result.indices={0,1,3,0,3,2,4,6,7,4,7,5,0,4,5,0,5,1,2,3,7,2,7,6,0,2,6,0,6,4,1,5,7,1,7,3};
    return result;
}
void CoverageTests() {
    const auto large=Triangle({-.32,.037,-.16},{1.61,.037,.05},{.21,.041,1.84});
    const auto largeResult=CoverageCase("large_cross_grid",large,{{-5,-3,-4},{16,4,18}},.125);
    Check(largeResult.cells.size()>60,"large triangle reduced to vertex cells");
    Check(largeResult.cells.size()*2<largeResult.statistics.gridCells,"author returned a complete triangle AABB instead of surface cells");
    const auto boundary=CoverageCase("negative_boundary_plane",Triangle({-.25,0,-.25},{0,0,-.25},{-.25,0,0}),{{-4,-2,-4},{2,2,2}},.125);
    Check(std::any_of(boundary.cells.begin(),boundary.cells.end(),[](auto c){return c.y==-1;}),"closed lower boundary face missing");
    Check(std::any_of(boundary.cells.begin(),boundary.cells.end(),[](auto c){return c.y==0;}),"closed upper boundary face missing");
    CoverageCase("thin_twig",Triangle({-.2,.049,-.002},{.4,.052,.002},{.399,.053,.0021}),{{-4,-2,-2},{6,3,3}},.125);
    CoverageCase("oblique_13axis",Triangle({-.041,-.019,.006},{.7,.63,.01},{.04,.92,.78}),{{-3,-3,-3},{9,11,10}},.125);
    CoverageCase("nonbinary_spacing",large,{{-4,-3,-3},{11,4,12}},.20);
    CoverageCase("translated_large_coordinates",Triangle({1'000'000,1'000'000,1'000'000},
        {1'000'000.5,1'000'000.03,1'000'000.1},{1'000'000.1,1'000'000.04,1'000'000.5}),
        {{7'999'998,7'999'998,7'999'998},{8'000'007,8'000'003,8'000'007}},.125);
    const auto shell=CoverageCase("closed_cube_surface_only",CubeSurface(),{{-6,-6,-6},{6,6,6}},.125);
    Check(!std::binary_search(shell.cells.begin(),shell.cells.end(),CellIndex{0,0,0}),"surface author silently filled solid interior");
    const auto one=Triangle({.01,.02,.03},{.07,.02,.03},{.01,.08,.03});
    const auto alone=AuthorSurfaceUnion(one.View());
    auto duplicate=one;duplicate.indices.insert(duplicate.indices.end(),{0,1,2});
    Check(AuthorSurfaceUnion(duplicate.View()).cells==alone.cells,"repeated triangles duplicate occupied cells");
}
void StrictTests() {
    const auto valid=Triangle({.01,.02,.03},{.30,.02,.03},{.01,.30,.03});
    Settings settings;
    Reject(ErrorCode::EmptyMesh,[&]{(void)AuthorSurfaceUnion({{},{}},settings);});
    auto bad=valid;bad.indices.push_back(0);
    Reject(ErrorCode::BadIndexCount,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    bad=valid;bad.indices[2]=3;
    Reject(ErrorCode::BadIndex,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    bad=valid;bad.vertices.push_back({std::numeric_limits<double>::quiet_NaN(),0,0});
    Reject(ErrorCode::NonfiniteVertex,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    bad=valid;bad.vertices[0].y=std::numeric_limits<double>::infinity();
    Reject(ErrorCode::NonfiniteVertex,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    bad=Triangle({0,0,0},{.125,.125,.125},{.25,.25,.25});
    Reject(ErrorCode::DegenerateTriangle,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    bad=valid;bad.indices[2]=1;
    Reject(ErrorCode::DegenerateTriangle,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    bad=Triangle({1e15,0,0},{1e15,.125,0},{1e15,0,.125});
    Reject(ErrorCode::GridCoordinateRange,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    for (double spacing : {.0,.124,.201,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        settings=Settings{};settings.spacing=spacing;
        Reject(ErrorCode::InvalidSettings,[&]{(void)AuthorSurfaceUnion(valid.View(),settings);});
    }
    settings=Settings{};settings.maxVertices=2;
    Reject(ErrorCode::VertexBudget,[&]{(void)AuthorSurfaceUnion(valid.View(),settings);});
    bad=valid;bad.indices.insert(bad.indices.end(),{0,1,2});
    settings=Settings{};settings.maxTriangles=1;
    Reject(ErrorCode::TriangleBudget,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    settings=Settings{};settings.maxGridCells=2;
    Reject(ErrorCode::GridBudget,[&]{(void)AuthorSurfaceUnion(valid.View(),settings);});
    settings=Settings{};settings.maxTriangleCellChecks=1;
    Reject(ErrorCode::CheckBudget,[&]{(void)AuthorSurfaceUnion(valid.View(),settings);});
    settings=Settings{};settings.maxOccupiedCells=1;
    Reject(ErrorCode::OccupancyBudget,[&]{(void)AuthorSurfaceUnion(valid.View(),settings);});
    settings=Settings{};settings.maxGridCells=0;
    Reject(ErrorCode::InvalidSettings,[&]{(void)AuthorSurfaceUnion(valid.View(),settings);});
    settings=Settings{};const auto once=AuthorSurfaceUnion(valid.View(),settings);
    settings.maxTriangleCellChecks=once.statistics.triangleCellChecks;
    Reject(ErrorCode::CheckBudget,[&]{(void)AuthorSurfaceUnion(bad.View(),settings);});
    settings=Settings{};settings.maxGridCells=once.statistics.gridCells;settings.maxTriangleCellChecks=once.statistics.triangleCellChecks;settings.maxOccupiedCells=once.cells.size();
    Check(AuthorSurfaceUnion(valid.View(),settings).cells==once.cells,"exact author budget rejected");
    Reject(ErrorCode::GridCoordinateRange,[]{(void)CellBounds({std::numeric_limits<std::int64_t>::max(),0,0},.125);});
}
std::uint64_t BoxCellCount(CellBox box) {
    return static_cast<std::uint64_t>(box.maximumExclusive.x-box.minimum.x)*
        static_cast<std::uint64_t>(box.maximumExclusive.y-box.minimum.y)*static_cast<std::uint64_t>(box.maximumExclusive.z-box.minimum.z);
}
void PackingCase(std::string_view name,std::vector<CellIndex> cells,std::uint64_t maxCellsPerBox) {
    std::sort(cells.begin(),cells.end());MergeSettings settings;settings.maxCellsPerBox=maxCellsPerBox;
    const auto merged=MergeOccupiedCells(cells,settings);
    std::set<CellIndex> expanded;
    for (auto box : merged.boxes) {
        Check(BoxCellCount(box)<=maxCellsPerBox,"packing exceeded per-box cell limit");
        const auto bounds=BoxBounds(box,.125);
        Check(bounds.minimum.x==double(box.minimum.x)*.125&&bounds.maximum.z==double(box.maximumExclusive.z)*.125,"packed world bounds differ");
        for (auto x=box.minimum.x;x<box.maximumExclusive.x;++x)
            for (auto y=box.minimum.y;y<box.maximumExclusive.y;++y)
                for (auto z=box.minimum.z;z<box.maximumExclusive.z;++z) {
                    Check(expanded.insert({x,y,z}).second,"packed boxes overlap");
                }
    }
    Check(std::vector<CellIndex>(expanded.begin(),expanded.end())==cells,"packing filled a hole or lost an occupied cell");
    Check(merged.pairChecks<=3*cells.size(),"packing comparison count exceeded three linear passes");
    const auto repeat=MergeOccupiedCells(cells,settings);
    Check(repeat.boxes.size()==merged.boxes.size()&&repeat.pairChecks==merged.pairChecks,"packing nondeterministic");
    for (std::size_t i=0;i<repeat.boxes.size();++i) Check(repeat.boxes[i].minimum==merged.boxes[i].minimum&&repeat.boxes[i].maximumExclusive==merged.boxes[i].maximumExclusive,"packing box order differs");
    if (name=="solid_4cube") Check(merged.boxes.size()==1,"full 4-cube not packed into one bounded box");
    std::cout<<"PACKING case="<<name<<" input_cells="<<cells.size()<<" boxes="<<merged.boxes.size()<<" pair_checks="<<merged.pairChecks<<" cap="<<maxCellsPerBox<<'\n';
}
void MergeTests() {
    std::vector<CellIndex> cells;
    for (std::int64_t x=0;x<4;++x) for (std::int64_t y=0;y<4;++y) for (std::int64_t z=0;z<4;++z) cells.push_back({x,y,z});
    PackingCase("solid_4cube",cells,64);
    cells.clear();for (std::int64_t x=0;x<8;++x) for (std::int64_t y=0;y<8;++y) for (std::int64_t z=0;z<8;++z) cells.push_back({x,y,z});
    PackingCase("bounded_8cube",cells,32);
    cells.clear();for (std::int64_t x=-3;x<0;++x) for (std::int64_t y=-2;y<1;++y) for (std::int64_t z=-1;z<2;++z)
        if (CellIndex{x,y,z}!=CellIndex{-2,-1,0}) cells.push_back({x,y,z});
    PackingCase("negative_center_hole",cells,64);
    PackingCase("missing_adjacent_cell",{{0,0,0},{0,0,2}},64);
    PackingCase("L_shape_no_corner_fill",{{0,0,0},{0,0,1},{0,1,0}},64);
    PackingCase("one_cell_per_box",{{0,0,0},{0,0,1},{0,1,0}},1);
    std::vector<CellIndex> bad{{0,0,0},{0,0,0}};
    Reject(ErrorCode::InvalidCellOrder,[&]{(void)MergeOccupiedCells(bad);});
    bad={{0,0,1},{0,0,0}};
    Reject(ErrorCode::InvalidCellOrder,[&]{(void)MergeOccupiedCells(bad);});
    bad={{0,0,0},{0,0,2}};MergeSettings settings;settings.maxInputCells=1;
    Reject(ErrorCode::MergeInputBudget,[&]{(void)MergeOccupiedCells(bad,settings);});
    settings=MergeSettings{};settings.maxPairChecks=1;
    Reject(ErrorCode::MergeCheckBudget,[&]{(void)MergeOccupiedCells(bad,settings);});
    settings=MergeSettings{};settings.maxBoxes=1;
    Reject(ErrorCode::MergeBoxBudget,[&]{(void)MergeOccupiedCells(bad,settings);});
    settings=MergeSettings{};settings.maxCellsPerBox=0;
    Reject(ErrorCode::InvalidSettings,[&]{(void)MergeOccupiedCells(bad,settings);});
    Reject(ErrorCode::GridCoordinateRange,[]{(void)BoxBounds({{1,0,0},{0,1,1}},.125);});
}
} // 獨立測試

int main() {
    try {
        if constexpr (VOXEL_TEST_GROUP==0 || VOXEL_TEST_GROUP==1) CoverageTests();
        if constexpr (VOXEL_TEST_GROUP==0 || VOXEL_TEST_GROUP==2) StrictTests();
        if constexpr (VOXEL_TEST_GROUP==0 || VOXEL_TEST_GROUP==3) MergeTests();
        std::cout<<"MESH_COLLISION_VOXEL_V100_PASS group="<<VOXEL_TEST_GROUP<<" checks="<<checks<<" strict_rejections="<<negatives<<'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"MESH_COLLISION_VOXEL_V100_FIRST_DIVERGENCE "<<error.what()<<'\n'; return 1;
    }
}
