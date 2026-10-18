#include "MistwatchCliffCollision.h"
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <iostream>
#include <set>
#include <limits>
namespace C=Mistwatch::CliffCollision;
using namespace Aevrynd;
unsigned Checks{},Negatives{};
void Need(bool value,const char* text){++Checks;if(!value)throw std::runtime_error(text);}
template<class F>void Reject(F f,C::Voxel::ErrorCode code){bool caught=false;try{f();}catch(const C::Voxel::AuthorError& e){caught=e.code==code;}Need(caught,"wrong/missing rejection");++Negatives;}
Mesh3D Wall(float x=.03f){Mesh3D m;for(auto p:{Vec3{x,0,-1},Vec3{x,2,-1},Vec3{x,2,1},Vec3{x,0,1}})m.vertices.push_back({p.x,p.y,p.z,0,0});m.indices={0,1,2,0,2,3};return m;}
void Union(const C::Result& r){
    std::set<C::Voxel::CellIndex> expanded;
    Need(r.parts.size()==r.cellBoxes.size(),"box correspondence");
    for(std::size_t i=0;i<r.cellBoxes.size();++i){const auto& b=r.cellBoxes[i];const auto& p=r.parts[i];
        const auto low=p.center-p.extents,high=p.center+p.extents;
        const auto actual=C::Voxel::BoxBounds(b,C::Spacing);
        Need(double(low.x)<=actual.minimum.x&&double(low.y)<=actual.minimum.y&&double(low.z)<=actual.minimum.z&&
             double(high.x)>=actual.maximum.x&&double(high.y)>=actual.maximum.y&&double(high.z)>=actual.maximum.z,"float enclosure");
        for(auto x=b.minimum.x;x<b.maximumExclusive.x;++x)for(auto y=b.minimum.y;y<b.maximumExclusive.y;++y)for(auto z=b.minimum.z;z<b.maximumExclusive.z;++z)
            Need(expanded.insert({x,y,z}).second,"merge duplicate/overlap");
    }
    Need(std::vector<C::Voxel::CellIndex>(expanded.begin(),expanded.end())==r.cells,"merge changed surface union");
}
int main(){try{
    const auto mesh=Wall();const auto r=C::Create(mesh);Union(r);
    Need(!r.parts.empty()&&r.statistics.triangles==2&&r.spacing==.20,"wall output");
    // 獨立列出矩形預期格，包含恰在 y/z 邊界的兩側格，不用產生器求預期值。
    std::vector<C::Voxel::CellIndex> literal;
    for(std::int64_t y=-1;y<=10;++y)for(std::int64_t z=-6;z<=5;++z)literal.push_back({0,y,z});
    Need(r.cells==literal,"known wall surface coverage");
    const auto shell=C::Create(Mesh3D::CreateUnitBox());Union(shell);
    Need(!std::binary_search(shell.cells.begin(),shell.cells.end(),C::Voxel::CellIndex{0,0,0}),"closed surface falsely interior filled");
    const auto again=C::Create(mesh);Need(r.geometryIdentity==again.geometryIdentity&&r.cellIdentity==again.cellIdentity&&r.boxIdentity==again.boxIdentity,"determinism");
    auto moved=mesh;for(auto& v:moved.vertices)v.x+=1;const auto mr=C::Create(moved);Union(mr);
    Need(mr.geometryIdentity!=r.geometryIdentity&&mr.cellIdentity!=r.cellIdentity&&mr.boxIdentity!=r.boxIdentity,"world geometry identity");
    // 兩個不相連表面：合併必須保留中間通道。
    auto split=Wall(-2.03f);auto right=Wall(2.03f);split.vertices.insert(split.vertices.end(),right.vertices.begin(),right.vertices.end());for(auto i:right.indices)split.indices.push_back(i+4);
    const auto sr=C::Create(split);Union(sr);for(const auto& b:sr.parts)Need(std::abs(b.center.x)>1.5f,"air gap bridged");
    HeightField3D ground({{-5,-1,-5},11,11,1});ground.Fill(-1);
    Need(!ThirdPersonCameraPathClear3D({-2,1,0},{2,1,0},r.parts,ground),"camera crossed wall");
    Need(ThirdPersonCameraPathClear3D({-2,1,2},{2,1,2},r.parts,ground),"camera distant path falsely blocked");
    Need(ThirdPersonCameraPathClear3D({0,1,-2},{0,1,2},sr.parts,ground),"camera gap falsely blocked");
    CharacterController3D player;player.Reset({-2,0,0});CharacterDirectDisplacementResult3D hit;
    Need(player.ApplyDirectDisplacementTick({1,{4,0,0},0},r.parts,&hit),"body sweep rejected");
    Need(hit.status==CharacterDirectDisplacementStatus3D::Blocked&&player.Position().x<0,"body crossed wall");
    player.Reset({-2,0,2});Need(player.ApplyDirectDisplacementTick({1,{4,0,0},0},r.parts,&hit),"body clear transaction");Need(player.Position().x>1.9f,"body distant path falsely blocked");
    CollisionWorld3D world;for(const auto& p:r.parts)Need(bool(world.CreateBox({p})),"register proxy");
    Need(bool(world.SweepSphere({-2,1,0},.12f,{4,0,0})),"sphere crossed wall");
    Need(!world.SweepSphere({-2,1,2},.12f,{4,0,0}),"sphere distant path falsely blocked");
    using E=C::Voxel::ErrorCode;
    Reject([]{(void)C::Create({});},E::EmptyMesh);
    auto bad=mesh;bad.indices.push_back(0);Reject([&]{(void)C::Create(bad);},E::BadIndexCount);
    bad=mesh;bad.indices[0]=900;Reject([&]{(void)C::Create(bad);},E::BadIndex);
    bad=mesh;bad.vertices[0].x=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)C::Create(bad);},E::NonfiniteVertex);
    bad=mesh;bad.vertices[0].y=std::numeric_limits<float>::infinity();Reject([&]{(void)C::Create(bad);},E::NonfiniteVertex);
    bad=mesh;bad.indices[1]=bad.indices[0];Reject([&]{(void)C::Create(bad);},E::DegenerateTriangle);
    for(unsigned which=0;which<8;++which){C::Budgets b;E expected=E::InvalidSettings;switch(which){
      case 0:b.vertices=1;expected=E::VertexBudget;break;case 1:b.triangles=1;expected=E::TriangleBudget;break;
      case 2:b.gridCells=1;expected=E::GridBudget;break;case 3:b.triangleCellChecks=1;expected=E::CheckBudget;break;
      case 4:b.occupiedCells=1;expected=E::OccupancyBudget;break;case 5:b.mergeChecks=1;expected=E::MergeCheckBudget;break;
      case 6:b.boxes=1;b.cellsPerBox=1;expected=E::MergeBoxBudget;break;case 7:b.vertices=200001;break;}
      Reject([&]{(void)C::Create(mesh,b);},expected);
    }
    C::Budgets zero;zero.gridCells=0;Reject([&]{(void)C::Create(mesh,zero);},E::InvalidSettings);
    std::cout<<"PASS checks="<<Checks<<" negative="<<Negatives<<" cells="<<r.cells.size()<<" boxes="<<r.parts.size()<<" conservative_surface_only=1\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" checks="<<Checks<<'\n';return 1;}}
