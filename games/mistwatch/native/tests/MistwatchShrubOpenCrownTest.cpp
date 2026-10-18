#include "MistwatchShrubCanopy.h"
#include <Aevrynd/MeshLoader.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
unsigned checks{},rejections{};
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static_assert(sizeof(Vertex3D)==28);
void ExactMesh(const Mesh3D& a,const Mesh3D& b) {
    Require(a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"Original source topology changed");
    for(std::size_t i=0;i<a.vertices.size();++i)
        Require(std::memcmp(&a.vertices[i],&b.vertices[i],sizeof(Vertex3D))==0,"Original raw vertex changed");
}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void ExactAuthor(const ShrubMesh& a,const ShrubMesh& b) {
    ExactMesh(a.stems,b.stems);ExactMesh(a.leaves,b.leaves);
    Require(SameFloat(a.radius,b.radius)&&SameFloat(a.height,b.height)&&SameFloat(a.minimumY,b.minimumY)&&SameFloat(a.maximumY,b.maximumY)&&
        a.leafCount==b.leafCount&&a.stemCount==b.stemCount&&a.attachments.size()==b.attachments.size(),
        "Original complete author metadata changed");
    for(std::size_t i=0;i<a.attachments.size();++i) {
        const auto& p=a.attachments[i];const auto& q=b.attachments[i];
        Require(p.vertex==q.vertex&&SameFloat(p.centre.x,q.centre.x)&&SameFloat(p.centre.y,q.centre.y)&&SameFloat(p.centre.z,q.centre.z)&&
            SameFloat(p.branchRadius,q.branchRadius),"Original attachment changed");
    }
}
std::vector<unsigned> IndependentMembership(const Mesh3D& mesh) {
    // 獨立DFS，不使用產品helper的union-find或固定leaf頂點／三角步長。
    std::vector<std::vector<unsigned>> adjacency(mesh.vertices.size());
    for(std::size_t f=0;f<mesh.indices.size();f+=3)for(unsigned i=0;i<3;++i) {
        const auto a=mesh.indices[f+i],b=mesh.indices[f+(i+1)%3];
        adjacency[a].push_back(b);adjacency[b].push_back(a);
    }
    std::vector<unsigned> group(mesh.vertices.size(),~0u),pending;
    unsigned next{};
    for(unsigned start=0;start<group.size();++start)if(group[start]==~0u) {
        group[start]=next;pending.push_back(start);
        while(!pending.empty()) {
            const auto a=pending.back();pending.pop_back();
            for(const auto b:adjacency[a])if(group[b]==~0u){group[b]=next;pending.push_back(b);}
        }
        ++next;
    }
    Require(next==504,"Independent whole-leaf membership changed");return group;
}
void VerifyMesh(const Mesh3D& mesh) {
    Require(mesh.Valid(),"Invalid render mesh/index bounds");
    for(const auto& v:mesh.vertices) {
        const auto n=UnpackNormalSnorm8(v.normal);
        Require(Session::Finite({v.x,v.y,v.z})&&std::isfinite(v.u)&&std::isfinite(v.v)&&
            Session::Finite({n.x,n.y,n.z})&&Length(Vec3{n.x,n.y,n.z})>0,"Nonfinite render position/UV/normal");
    }
    for(std::size_t f=0;f<mesh.indices.size();f+=3) {
        const auto a=mesh.indices[f],b=mesh.indices[f+1],c=mesh.indices[f+2];
        Require(a!=b&&a!=c&&b!=c,"Duplicate render triangle index");
        const auto& p=mesh.vertices[a];const auto& q=mesh.vertices[b];const auto& r=mesh.vertices[c];
        const auto area=Length(Cross(Vec3{q.x-p.x,q.y-p.y,q.z-p.z},Vec3{r.x-p.x,r.y-p.y,r.z-p.z}));
        Require(std::isfinite(area)&&area>0,"Degenerate render triangle");
    }
}
void VerifyProfile(const ShrubMesh& source,const ShrubOpenCrown& render,unsigned variant) {
    ExactMesh(source.stems,render.stems);
    Require(render.authorRadius==source.radius&&render.authorHeight==source.height&&
        render.authorMinimumY==source.minimumY&&render.authorMaximumY==source.maximumY&&
        render.authorStemCount==7&&render.authorLeafCount==504&&render.renderLeafCount==224&&
        render.attachments.size()==224&&render.sourceLeafIds.size()==224,"Full author/render metadata confused");
    const auto group=IndependentMembership(source.leaves);
    std::array<unsigned,504> owner;owner.fill(~0u);
    for(unsigned leaf=0;leaf<504;++leaf) {
        const auto component=group[source.attachments[leaf].vertex];
        Require(owner[component]==~0u,"Independent duplicate attachment owner");owner[component]=leaf;
    }
    std::array<bool,504> chosen{};std::array<unsigned,56> twigCount{};
    for(const auto leaf:render.sourceLeafIds) {
        Require(leaf<504&&!chosen[leaf],"Selected leaf duplicate/outside source");
        chosen[leaf]=true;++twigCount[leaf/9];
    }
    for(const auto count:twigCount)Require(count==4,"Not exactly four complete leaves per original nine-leaf twig");
    Require(std::is_sorted(render.sourceLeafIds.begin(),render.sourceLeafIds.end()),"Source leaf order changed");
    // 直接按已採用作者契約算選葉序列；不讀產品helper內部會員或selector。
    constexpr std::array<std::array<unsigned,4>,3> patterns{{{{0,2,5,8}},{{1,3,6,8}},{{0,3,5,7}}}};
    std::vector<unsigned> specified;
    for(unsigned twig=0;twig<56;++twig) {
        std::uint32_t hash=0x4f50454eu^(twig*0x9e3779b9u)^(variant*977u);
        hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;
        for(const auto leaf:patterns[hash%3])specified.push_back(twig*9+leaf);
    }
    Require(render.sourceLeafIds==specified,"Staggered whole-leaf selection contract changed");

    std::vector<unsigned> expectedVertices,expectedFaces,remap(source.leaves.vertices.size(),~0u);
    for(unsigned v=0;v<group.size();++v)if(chosen[owner[group[v]]]) {
        remap[v]=unsigned(expectedVertices.size());expectedVertices.push_back(v);
    }
    for(unsigned f=0;f<source.leaves.indices.size();f+=3)if(chosen[owner[group[source.leaves.indices[f]]]])expectedFaces.push_back(f);
    Require(expectedVertices==render.sourceVertexIds&&expectedFaces==render.sourceFaceOffsets&&
        render.leaves.vertices.size()==expectedVertices.size()&&render.leaves.indices.size()==expectedFaces.size()*3,
        "Component cut or nonselected source data entered render subset");
    for(unsigned v=0;v<expectedVertices.size();++v)
        Require(std::memcmp(&render.leaves.vertices[v],&source.leaves.vertices[expectedVertices[v]],sizeof(Vertex3D))==0,
            "Selected raw XYZ/packednormal/color/UV altered");
    for(unsigned f=0;f<expectedFaces.size();++f)for(unsigned corner=0;corner<3;++corner)
        Require(render.leaves.indices[f*3+corner]==remap[source.leaves.indices[expectedFaces[f]+corner]],
            "Whole source triangle index/winding/order altered");
    for(unsigned leaf=0;leaf<224;++leaf) {
        const auto& p=render.attachments[leaf];const auto& q=source.attachments[render.sourceLeafIds[leaf]];
        Require(p.vertex==remap[q.vertex]&&p.centre.x==q.centre.x&&p.centre.y==q.centre.y&&p.centre.z==q.centre.z&&
            p.branchRadius==q.branchRadius,"Whole-leaf attachment remap altered branch connection");
    }
    VerifyMesh(render.stems);VerifyMesh(render.leaves);
    CanopyVolume::PlantLocalVolume whole,actual;
    Require(CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(source.stems.vertices),
        std::span<const Vertex3D>(source.leaves.vertices),whole)&&
        CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(render.stems.vertices),
        std::span<const Vertex3D>(render.leaves.vertices),actual),"Actual render volume invalid");
    Require(actual.radius<=whole.radius&&actual.minimumY>=whole.minimumY&&actual.maximumY<=whole.maximumY&&
        actual.radius==render.actualRenderVolume.radius&&actual.minimumY==render.actualRenderVolume.minimumY&&
        actual.maximumY==render.actualRenderVolume.maximumY,"Actual subset escaped immutable conservative author volume");
    const auto sameBounds=[](const MeshBounds3D& a,const MeshBounds3D& b) {
        return a.center.x==b.center.x&&a.center.y==b.center.y&&a.center.z==b.center.z&&a.radius==b.radius&&
            a.extents.x==b.extents.x&&a.extents.y==b.extents.y&&a.extents.z==b.extents.z;
    };
    Require(sameBounds(render.actualStemBounds,render.stems.Bounds())&&sameBounds(render.actualLeafBounds,render.leaves.Bounds()),
        "Actual render bounds stale or replaced author footprint");
}

void ExactRender(const ShrubOpenCrown& a,const ShrubOpenCrown& b) {
    ExactMesh(a.stems,b.stems);ExactMesh(a.leaves,b.leaves);
    Require(SameFloat(a.authorRadius,b.authorRadius)&&SameFloat(a.authorHeight,b.authorHeight)&&
        SameFloat(a.authorMinimumY,b.authorMinimumY)&&SameFloat(a.authorMaximumY,b.authorMaximumY)&&
        a.authorLeafCount==b.authorLeafCount&&a.renderLeafCount==b.renderLeafCount&&a.authorStemCount==b.authorStemCount&&
        a.sourceLeafIds==b.sourceLeafIds&&a.sourceVertexIds==b.sourceVertexIds&&a.sourceFaceOffsets==b.sourceFaceOffsets&&
        a.attachments.size()==b.attachments.size(),"Prior valid output changed on failure");
    const auto sameBounds=[](const MeshBounds3D& p,const MeshBounds3D& q) {
        return SameFloat(p.center.x,q.center.x)&&SameFloat(p.center.y,q.center.y)&&SameFloat(p.center.z,q.center.z)&&
            SameFloat(p.radius,q.radius)&&SameFloat(p.extents.x,q.extents.x)&&SameFloat(p.extents.y,q.extents.y)&&SameFloat(p.extents.z,q.extents.z);
    };
    Require(sameBounds(a.actualStemBounds,b.actualStemBounds)&&sameBounds(a.actualLeafBounds,b.actualLeafBounds)&&
        SameFloat(a.actualRenderVolume.radius,b.actualRenderVolume.radius)&&
        SameFloat(a.actualRenderVolume.minimumY,b.actualRenderVolume.minimumY)&&
        SameFloat(a.actualRenderVolume.maximumY,b.actualRenderVolume.maximumY),"Prior actual render bounds changed on failure");
    for(std::size_t i=0;i<a.attachments.size();++i) {
        const auto& p=a.attachments[i];const auto& q=b.attachments[i];
        Require(p.vertex==q.vertex&&SameFloat(p.centre.x,q.centre.x)&&SameFloat(p.centre.y,q.centre.y)&&
            SameFloat(p.centre.z,q.centre.z)&&SameFloat(p.branchRadius,q.branchRadius),"Prior remapped attachment changed on failure");
    }
}
void RejectTransaction(ShrubMesh bad,unsigned variant,const ShrubOpenCrown& valid) {
    const auto before=bad;auto destination=valid;
    bool rejected=false;
    try {destination=CreateShrubOpenCrown(bad,variant);}
    catch(const std::invalid_argument&){rejected=true;}
    Require(rejected,"Invalid render input accepted or wrong failure type");
    ExactAuthor(bad,before);ExactRender(destination,valid);++rejections;
}
void Negative(const ShrubMesh& source,unsigned variant,const ShrubOpenCrown& valid) {
    // 每種失敗都驗原輸入（含NaN payload）及已存在的有效結果原樣；不只看throw。
    for(unsigned kind=0;kind<13;++kind) {
        auto bad=source;
        if(kind==0)bad.attachments[1].vertex=bad.attachments[0].vertex;
        if(kind==1)bad.attachments.pop_back();
        if(kind==2)bad.leafCount=503;
        if(kind==3)bad.leaves.indices[0]=unsigned(bad.leaves.vertices.size());
        if(kind==4)bad.leaves.indices[0]=bad.leaves.indices[1];
        if(kind==5)bad.leaves.indices[0]=bad.attachments[1].vertex;
        if(kind==6)bad.leaves.vertices.push_back(bad.leaves.vertices.front());
        if(kind==7)bad.leaves.vertices.front().u=std::numeric_limits<float>::quiet_NaN();
        if(kind==8)bad.attachments.front().centre.x=std::numeric_limits<float>::quiet_NaN();
        if(kind==9)std::swap(bad.attachments[0],bad.attachments[1]);
        if(kind==10)bad.radius=std::nextafter(bad.radius,std::numeric_limits<float>::infinity());
        if(kind==11)bad.leaves.vertices.front().normal=0;
        if(kind==12)bad.stems.vertices.front().x=std::numeric_limits<float>::infinity();
        RejectTransaction(std::move(bad),variant,valid);
    }
    RejectTransaction(source,4,valid);
}
}
int main(){try {
    const std::filesystem::path repo=MISTWATCH_SOURCE_ROOT;
    const auto leaf=LoadAevryndMeshFile(repo/"engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02"/
        "tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh");
    Require(bool(leaf),"Original legal whole-leaf source unavailable");
    const auto cards=ShrubDetail::LeafComponents(leaf.mesh);
    Require(cards.size()==16,"Original complete source cards changed");
    double helperSeconds{};
    for(unsigned variant=0;variant<4;++variant) {
        const auto source=ShrubDetail::Build(cards,variant),before=source;
        Require(source.leafCount==504&&source.attachments.size()==504&&source.stemCount==7,"Original author counts changed");
        const auto tick=std::chrono::steady_clock::now();
        const auto render=CreateShrubOpenCrown(source,variant);
        const auto repeat=CreateShrubOpenCrown(source,variant);
        helperSeconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-tick).count();
        VerifyProfile(before,render,variant);VerifyProfile(before,repeat,variant);ExactRender(render,repeat);
        ExactAuthor(source,before);ExactAuthor(source,ShrubDetail::Build(cards,variant));
        Negative(source,variant,render);ExactAuthor(source,before);
        std::cout<<"open_crown variant="<<variant<<" author_leaves="<<source.leafCount<<" render_leaves="<<render.renderLeafCount
            <<" stem_indices="<<render.stems.indices.size()<<" leaf_indices="<<render.leaves.indices.size()<<'\n';
    }
    Require(rejections==56,"Complete invalid transaction matrix did not execute");
    Require(helperSeconds<=5.0,"Eight pure helper calls exceeded bounded five-second CPU budget");
    std::cout<<"MISTWATCH_SHRUB_OPEN_CROWN_PASS checks="<<checks<<" rejected_transactions="<<rejections
        <<" helper_seconds="<<helperSeconds<<" source_author_raw_unchanged=1\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"MISTWATCH_SHRUB_OPEN_CROWN_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
