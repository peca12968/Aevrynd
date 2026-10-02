#include "MistwatchWorld.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/CharacterController3D.h>
#include <filesystem>
#include <iostream>
#include <limits>
int main() {
    using namespace Aevrynd;
    const auto root=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02";
    std::size_t triangles{};
    Vec3 crownMinimum{100,100,100},crownMaximum{-100,-100,-100};
    Mesh3D stem;
    for(unsigned slot=0;slot<3;++slot) {
        auto loaded=LoadAevryndMeshFile(root/Mistwatch::BroadleafMeshName(slot));
        if(!loaded){std::cerr<<loaded.error<<'\n';return 1;}
        const auto before=loaded.mesh;
        Mistwatch::RootBroadleafMesh(loaded.mesh);
        if(before.indices!=loaded.mesh.indices)return 2;
        triangles+=loaded.mesh.indices.size()/3;
        for(std::size_t i=0;i<loaded.mesh.vertices.size();++i) {
            const auto& a=before.vertices[i];const auto& b=loaded.mesh.vertices[i];
            if(a.u!=b.u||a.v!=b.v||a.normal!=b.normal||a.color!=b.color||
                std::abs(b.x-(a.x-Mistwatch::BroadleafRoot.x))>.00001f||
                std::abs(b.y-(a.y-Mistwatch::BroadleafRoot.y))>.00001f||
                std::abs(b.z-(a.z-Mistwatch::BroadleafRoot.z))>.00001f)return 3;
            crownMinimum={std::min(crownMinimum.x,b.x),std::min(crownMinimum.y,b.y),std::min(crownMinimum.z,b.z)};
            crownMaximum={std::max(crownMaximum.x,b.x),std::max(crownMaximum.y,b.y),std::max(crownMaximum.z,b.z)};
        }
        if(slot==2)stem=std::move(loaded.mesh);
    }
    if(triangles!=82854||!Mistwatch::BroadleafMeshName(3).empty())return 4;
    const auto crown=Mistwatch::BroadleafBounds(),lower=Mistwatch::BroadleafLowerTrunkBounds();
    if(Length(crownMinimum-(crown.center-crown.extents))>.00001f||Length(crownMaximum-(crown.center+crown.extents))>.00001f)return 5;
    unsigned stemVertices{};
    Vec3 stemMinimum{100,100,100},stemMaximum{-100,-100,-100};
    std::vector<Vec3> clippedStem;
    const auto recordStem=[&](Vec3 p){
        clippedStem.push_back(p);
        stemMinimum={std::min(stemMinimum.x,p.x),std::min(stemMinimum.y,p.y),std::min(stemMinimum.z,p.z)};
        stemMaximum={std::max(stemMaximum.x,p.x),std::max(stemMaximum.y,p.y),std::max(stemMaximum.z,p.z)};
    };
    for(const auto& v:stem.vertices)if(v.y<=2.f) {
        if(std::abs(v.x-lower.center.x)>lower.extents.x+.00001f||
            std::abs(v.y-lower.center.y)>lower.extents.y+.00001f||
            std::abs(v.z-lower.center.z)>lower.extents.z+.00001f)return 6;
        ++stemVertices;
        recordStem({v.x,v.y,v.z});
    }
    if(stemVertices<100)return 7;
    // 三角面可能跨越上界；只檢查上界以下的原頂點會漏掉真實樹幹面片。
    unsigned crossingEdges{};
    for(std::size_t i=0;i<stem.indices.size();i+=3)for(unsigned edge=0;edge<3;++edge){
        const auto& a=stem.vertices[stem.indices[i+edge]];
        const auto& b=stem.vertices[stem.indices[i+(edge+1)%3]];
        if((a.y<2.f&&b.y>2.f)||(b.y<2.f&&a.y>2.f)){
            const float t=(2.f-a.y)/(b.y-a.y);
            recordStem({a.x+t*(b.x-a.x),2.f,a.z+t*(b.z-a.z)});
            ++crossingEdges;
        }
    }
    if(crossingEdges==0||Length(stemMinimum-(lower.center-lower.extents))>.00001f||
        Length(stemMaximum-(lower.center+lower.extents))>.00001f)return 18;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto conifers=Mistwatch::TreePositions();
    const auto props=Mistwatch::ForestProps(terrain,conifers);
    const auto understory=Mistwatch::CreateUnderstory(terrain,conifers,props);
    const auto trees=Mistwatch::CreateBroadleafTrees(terrain,conifers,props,understory);
    const auto again=Mistwatch::CreateBroadleafTrees(terrain,conifers,props,understory);
    if(trees.size()!=12||again.size()!=trees.size()){std::cerr<<"Broadleaf count="<<trees.size()<<'\n';return 8;}
    unsigned collisions{};
    for(std::size_t i=0;i<trees.size();++i) {
        const auto& tree=trees[i];const auto a=tree.transform.Matrix(),b=again[i].transform.Matrix();
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)if(a[row][col]!=b[row][col])return 9;
        const auto worldCrown=TransformBounds(crown,tree.transform);
        if(!Mistwatch::PropClearsObjectives({worldCrown.center,worldCrown.extents})||!Mistwatch::BroadleafClearsSettlement(worldCrown))return 10;
        const auto actualStem=TransformBounds(lower,tree.transform);
        if(Length(actualStem.center-tree.trunk.center)>.00001f||Length(actualStem.extents-tree.trunk.extents)>.00001f)return 11;
        for(const auto vertex:clippedStem){
            const auto point=TransformPoint(a,vertex)-tree.trunk.center;
            if(std::abs(point.x)>tree.trunk.extents.x+.00002f||std::abs(point.y)>tree.trunk.extents.y+.00002f||
                std::abs(point.z)>tree.trunk.extents.z+.00002f)return 22;
        }
        if(std::abs(tree.transform.position.y+.045f-terrain.SampleHeight(tree.transform.position.x,tree.transform.position.z))>.00001f)return 12;
        for(const auto& v:stem.vertices)if(v.y<.02f) {
            const auto p=TransformPoint(a,{v.x,v.y,v.z});
            if(std::abs(p.y-terrain.SampleHeight(p.x,p.z))>.15f)return 13;
        }
        for(bool repaired:{false,true}) {
            const auto solids=Mistwatch::WorldSolids(repaired);
            unsigned found{};
            for(const auto& solid:solids)if(Length(solid.center-tree.trunk.center)<.00001f&&Length(solid.extents-tree.trunk.extents)<.00001f)++found;
            if(found!=1)return 14;
        }
        for(const auto p:conifers)if(std::hypot(tree.transform.position.x-p.x,tree.transform.position.z-p.z)<3.f)return 15;
        for(const auto& p:props)if(std::abs(tree.trunk.center.x-p.collider.center.x)<tree.trunk.extents.x+p.collider.extents.x+.4f&&
            std::abs(tree.trunk.center.z-p.collider.center.z)<tree.trunk.extents.z+p.collider.extents.z+.4f)return 16;
        for(const auto& p:understory.saplings)if(std::hypot(tree.transform.position.x-p.position.x,tree.transform.position.z-p.position.z)<1.5f)return 19;
        for(const auto& p:understory.ferns)if(std::abs(tree.trunk.center.x-p.position.x)<tree.trunk.extents.x+.4f&&
            std::abs(tree.trunk.center.z-p.position.z)<tree.trunk.extents.z+.4f)return 20;
        for(std::size_t j=0;j<i;++j)if(std::hypot(tree.transform.position.x-trees[j].transform.position.x,
            tree.transform.position.z-trees[j].transform.position.z)<5.f)return 21;
        const float startX=tree.trunk.center.x-tree.trunk.extents.x-1.3f;
        CharacterController3D player;
        player.Reset({startX,terrain.SampleHeight(startX,tree.trunk.center.z),tree.trunk.center.z});
        const std::array<BoxCollider3D,1> obstacle{tree.trunk};
        for(unsigned tick=0;tick<120;++tick)player.Step({{1,0,0},false,1},obstacle,terrain,1.f/60.f);
        if(player.Position().x<=startX+.1f||player.Position().x>tree.trunk.center.x-tree.trunk.extents.x-player.Settings().radius+.02f)return 17;
        ++collisions;
    }
    std::cout<<"Broadleaf PASS; trees="<<trees.size()<<" asset_triangles="<<triangles<<" stem_vertices="<<stemVertices<<" controller_checks="<<collisions<<'\n';
}
