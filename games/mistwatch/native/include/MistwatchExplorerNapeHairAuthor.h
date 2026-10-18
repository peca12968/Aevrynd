#pragma once
#include "MistwatchExplorerArtMesh.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <vector>

namespace Mistwatch::ExplorerNapeHairAuthor {
// 來源作者為純資料轉換；驗證端可轉送既有單次budget，不在此重設時鐘或pair計數。
struct Checkpoint {
    void (*callback)(void*){};
    void* context{};
    void operator()()const {if(callback)callback(context);}
};
struct Result {Aevrynd::SkinnedModel3D model;ExplorerArtReceipt art;};
namespace Detail {
using namespace Aevrynd;
inline void Need(bool ok,const char* name){if(!ok)throw std::runtime_error(name);}
// BEGIN_EXTRACT_RAW_HELPERS
inline bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool SameVec(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
inline bool VertexRaw(const Vertex3D& a,const Vertex3D& b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal&&a.color==b.color&&Bits(a.u,b.u)&&Bits(a.v,b.v);}
// END_EXTRACT_RAW_HELPERS
// BEGIN_EXTRACT_SOURCE_MATH
struct D {double x{},y{},z{};D operator+(D b)const{return{x+b.x,y+b.y,z+b.z};}D operator-(D b)const{return{x-b.x,y-b.y,z-b.z};}D operator*(double s)const{return{x*s,y*s,z*s};}};
inline D Precise(const Vertex3D& v){return{v.x,v.y,v.z};}
inline double DotD(D a,D b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline D CrossD(D a,D b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
using Face=std::array<std::uint32_t,3>;using Triangle=std::array<D,3>;
inline Triangle Points(const Mesh3D& mesh,Face f){return{Precise(mesh.vertices.at(f[0])),Precise(mesh.vertices.at(f[1])),Precise(mesh.vertices.at(f[2]))};}
// END_EXTRACT_SOURCE_MATH
// BEGIN_EXTRACT_SOURCE_UNIT
inline D UnitD(D v){const double q=DotD(v,v);Need(std::isfinite(q)&&q>0,"source derivative finite nonzero domain");return v*(1./std::sqrt(q));}

// END_EXTRACT_SOURCE_UNIT
// BEGIN_EXTRACT_RECIPE_TYPES
using Key=std::array<std::uint32_t,2>;
constexpr unsigned OriginalCount=7861;
constexpr std::array<unsigned,29> Bottom{{32,32,32,32,32,31,31,28,23,20,18,17,16,15,15,15,16,17,18,20,23,28,31,31,32,32,32,32,32}};
constexpr std::array<float,42> Rows{{1.535f,1.542f,1.550f,1.558f,1.565f,1.572f,1.579f,1.583f,1.586f,1.589f,1.591f,1.593f,1.595f,1.598f,1.602f,1.607f,1.612f,1.618f,1.622f,1.626f,1.630f,1.635f,1.640f,1.646f,1.651f,1.656f,1.660f,1.664f,1.668f,1.672f,1.676f,1.680f,1.684f,1.689f,1.695f,1.702f,1.710f,1.720f,1.735f,1.750f,1.765f,1.775f}};
constexpr double Top=1.740,Inner=.003,Outer=.0035;
struct Node {Key key{};std::uint32_t sourceIndex{};Face source{};std::array<double,3> bary{};D p{},n{};double u{},v{};};
struct OwnedRecord {std::uint32_t id{},alias{},layer{},a{},b{};};
struct Recipe {std::vector<Node> nodes;std::vector<Face> outer;std::vector<OwnedRecord> records;std::vector<std::uint32_t> indices,oldIds;std::size_t oldStart{};};
// END_EXTRACT_RECIPE_TYPES
// BEGIN_EXTRACT_SOURCE_RECIPE
struct Source {std::map<std::uint32_t,std::array<unsigned,2>> grid;std::map<std::array<unsigned,2>,std::uint32_t> ids;std::map<std::uint32_t,D> normals;std::vector<std::pair<std::size_t,Face>> selected;std::vector<std::uint32_t> hair;std::size_t hairStart{};};
inline Source ReadSource(const SkinnedModel3D& old,const Mistwatch::ExplorerArtReceipt& art,const Checkpoint& checkpoint){
    checkpoint();Need(old.Valid()&&old.bindMesh.vertices.size()==7861&&old.bindMesh.indices.size()==31776,"original separate 7861/10592 model");Source s;
    const auto& m=old.bindMesh;const auto& skin=art.ranges[2];
    // 原HeadAndHands先造42-record neck Loft，再按row/column順序造SkinShell。
    Need(!art.criticalVertices[2].empty(),"original Head inventory origin");std::uint32_t next=art.criticalVertices[2].front()+42u;
    for(unsigned r=0;r<42;++r)for(unsigned c=0;c<48;++c){if(r>24&&r<31&&((c>1&&c<7)||(c>41&&c<47)))continue;
        const auto found=next++;const auto& p=m.vertices.at(found);const auto& w=old.influences.at(found);const float u=static_cast<float>(c)/48.f,v=(Rows[r]-1.535f)/.240f;
        Need(Bits(p.y,Rows[r])&&Bits(p.u,u)&&Bits(p.v,v)&&w.joints[0]==2&&w.weights[0]==65535&&!w.weights[1]&&!w.weights[2]&&!w.weights[3],"exact original stored SkinShell sequence/UV/y/rigid2 witness");s.grid[found]={r,c};s.ids[{r,c}]=found;
    }
    Need(s.grid.size()==1956,"exact original removed-socket Head grid count");
    std::map<std::array<unsigned,2>,unsigned> cells;
    for(std::size_t k=skin.firstIndex;k<std::size_t(skin.firstIndex)+skin.indexCount;k+=3){Face f{m.indices[k],m.indices[k+1],m.indices[k+2]};unsigned in{};for(auto id:f)in+=s.grid.contains(id);
        if(in==3){const auto area=CrossD(Precise(m.vertices[f[1]])-Precise(m.vertices[f[0]]),Precise(m.vertices[f[2]])-Precise(m.vertices[f[0]]));for(auto id:f)s.normals[id]=s.normals[id]+area;
            unsigned r=42,c=48,hiR{},hiC{};for(auto id:f){const auto rc=s.grid.at(id);r=(std::min)(r,rc[0]);c=(std::min)(c,rc[1]);hiR=(std::max)(hiR,rc[0]);hiC=(std::max)(hiC,rc[1]);}
            if(c>=10&&c<38&&hiC==c+1&&hiR==r+1&&r>=std::max(Bottom[c-10],Bottom[c+1-10])&&r<=38){s.selected.push_back({k,f});++cells[{r,c}];}
        }else if(in==2){bool cap=true;for(auto id:f)if(s.grid.contains(id)){const auto r=s.grid.at(id)[0];cap=cap&&(r==0||r==41);}if(cap){const auto area=CrossD(Precise(m.vertices[f[1]])-Precise(m.vertices[f[0]]),Precise(m.vertices[f[2]])-Precise(m.vertices[f[0]]));for(auto id:f)if(s.grid.contains(id))s.normals[id]=s.normals[id]+area;}}
    }
    for(unsigned c=10;c<38;++c)for(unsigned r=std::max(Bottom[c-10],Bottom[c+1-10]);r<=38;++r)Need(cells[{r,c}]==2,"all exact source cells two directed triangles");
    for(auto& [id,n]:s.normals){(void)id;n=UnitD(n);}
    const auto& hair=art.ranges[3];std::map<std::uint32_t,std::set<std::uint32_t>> adjacency;
    for(std::size_t k=hair.firstIndex;k<std::size_t(hair.firstIndex)+hair.indexCount;k+=3){Face f{m.indices[k],m.indices[k+1],m.indices[k+2]};for(auto a:f)for(auto b:f)adjacency[a].insert(b);}
    std::set<std::uint32_t> seen;unsigned matches{};
    for(const auto& [seed,neighbours]:adjacency){(void)neighbours;if(seen.contains(seed))continue;std::set<std::uint32_t> component;std::vector<std::uint32_t> pending{seed};while(!pending.empty()){const auto id=pending.back();pending.pop_back();if(!component.insert(id).second)continue;seen.insert(id);for(auto other:adjacency.at(id))pending.push_back(other);}if(component.size()!=74)continue;
        std::vector<std::size_t> faces;for(std::size_t k=hair.firstIndex;k<std::size_t(hair.firstIndex)+hair.indexCount;k+=3)if(component.contains(m.indices[k])){Need(component.contains(m.indices[k+1])&&component.contains(m.indices[k+2]),"whole hair component");faces.push_back(k);}
        Need(faces.size()==144,"original74 hair144 triangles, not Brows");for(std::size_t i=0;i<faces.size();++i)Need(faces[i]==faces.front()+i*3,"old Loft contiguous directed faces");s.hair.assign(component.begin(),component.end());s.hairStart=faces.front();++matches;
    }
    Need(matches==1,"one unique original74hair Loft ownership");return s;
}
inline Node SourceNode(const Source& s,const SkinnedModel3D& old,std::size_t sourceIndex,Face source,Key key){
    Node n;n.key=key;n.sourceIndex=static_cast<std::uint32_t>(sourceIndex);n.source=source;
    const auto& m=old.bindMesh;double t{};
    if(key[0]!=key[1]){const double a=m.vertices[key[0]].y,b=m.vertices[key[1]].y;t=(Top-a)/(b-a);Need(std::isfinite(t)&&t>0&&t<1,"strict top source-edge intersection");}
    for(unsigned j=0;j<3;++j){if(source[j]==key[0])n.bary[j]=1.-t;if(key[1]!=key[0]&&source[j]==key[1])n.bary[j]=t;}
    Need(std::accumulate(n.bary.begin(),n.bary.end(),0.)==1.,"source bary exact unit sum");
    for(unsigned j=0;j<3;++j){const auto& v=m.vertices[source[j]];n.p=n.p+Precise(v)*n.bary[j];n.n=n.n+s.normals.at(source[j])*n.bary[j];n.u+=double(v.u)*n.bary[j];n.v+=double(v.v)*n.bary[j];}n.n=UnitD(n.n);return n;
}
inline Recipe Build(const SkinnedModel3D& old,const Mistwatch::ExplorerArtReceipt& art,const Checkpoint& checkpoint){
    const auto s=ReadSource(old,art,checkpoint);Recipe q;q.oldIds=s.hair;q.oldStart=s.hairStart;std::map<Key,std::uint32_t> keys;
    const auto node=[&](Key key,std::size_t k,Face f){if(key[1]<key[0])std::swap(key[0],key[1]);auto it=keys.find(key);if(it!=keys.end())return it->second;const auto id=static_cast<std::uint32_t>(q.nodes.size());q.nodes.push_back(SourceNode(s,old,k,f,key));keys[key]=id;return id;};
    for(const auto& [k,f]:s.selected){checkpoint();std::vector<Key> polygon;
        for(unsigned j=0;j<3;++j){const auto a=f[j],b=f[(j+1u)%3u];const double ya=old.bindMesh.vertices[a].y,yb=old.bindMesh.vertices[b].y;
            if(ya<Top)polygon.push_back({a,a});if((ya<Top)!=(yb<Top)){Need(ya!=Top&&yb!=Top,"no unapproved top coincidence");polygon.push_back({a,b});}}
        Need(polygon.size()==3||polygon.size()==4,"one clipped source triangle polygon");std::vector<std::uint32_t> ids;for(auto key:polygon)ids.push_back(node(key,k,f));
        for(std::size_t j=1;j+1<ids.size();++j)q.outer.push_back({ids[0],ids[j],ids[j+1]});
    }
    Need(q.nodes.size()>=37,"new source domain enough nodes to reuse74 records");
    const auto idFor=[&](unsigned local){return local<74?q.oldIds.at(local):OriginalCount+local-74;};
    for(unsigned layer=0;layer<2;++layer)for(unsigned i=0;i<q.nodes.size();++i){const auto local=static_cast<unsigned>(q.records.size());q.records.push_back({idFor(local),local,layer,i,i});}
    const auto count=static_cast<unsigned>(q.nodes.size());
    for(auto f:q.outer){q.indices.insert(q.indices.end(),{idFor(f[0]),idFor(f[1]),idFor(f[2])});q.indices.insert(q.indices.end(),{idFor(count+f[0]),idFor(count+f[2]),idFor(count+f[1])});}
    std::map<Key,std::vector<Key>> edges;for(auto f:q.outer)for(unsigned j=0;j<3;++j){const Key directed{f[j],f[(j+1u)%3u]},key{std::min(directed[0],directed[1]),std::max(directed[0],directed[1])};edges[key].push_back(directed);}
    for(const auto& [key,uses]:edges){(void)key;Need(uses.size()==1||uses.size()==2,"source chart manifold edge count");if(uses.size()==2){Need(uses[0][0]==uses[1][1]&&uses[0][1]==uses[1][0],"source chart directed internal edge");continue;}
        const auto a=uses[0][0],b=uses[0][1];std::array<std::uint32_t,4> wall{};
        for(unsigned j=0;j<4;++j){const auto local=static_cast<unsigned>(q.records.size());wall[j]=idFor(local);const auto ni=j%2u?b:a;const auto alias=j<2?ni:count+ni;q.records.push_back({wall[j],alias,2,a,b});}
        q.indices.insert(q.indices.end(),{wall[0],wall[2],wall[1],wall[1],wall[2],wall[3]});
    }
    Need(q.records.size()<=2048&&q.indices.size()/3<=4096,"fixed source-owned2048/4096 caps");return q;
}
// END_EXTRACT_SOURCE_RECIPE
// BEGIN_EXTRACT_APPLY_RECIPE
inline Recipe Apply(Mistwatch::ExplorerArtAuthor::Builder& b,const SkinnedModel3D& baseline,const Checkpoint& checkpoint){
    Need(baseline.Valid(),"explicit original source model valid");
    Mistwatch::ExplorerArtReceipt art;std::uint32_t cursor{};for(unsigned i=0;i<5;++i){const auto size=static_cast<std::uint32_t>(b.triangles[i].size());art.ranges[i]={cursor,size,static_cast<Mistwatch::ExplorerArtMaterial>(i)};cursor+=size;}
    art.criticalVertices[2]=b.receipt.criticalVertices[2];auto q=Build(baseline,art,checkpoint);const auto color=PackColorRgba8(Mistwatch::ExplorerArtAuthor::Hair);const auto rigid=Mistwatch::ExplorerArtAuthor::Rigid(2);
    for(const auto& record:q.records){checkpoint();const auto& source=q.nodes.at(record.alias%q.nodes.size());const bool inner=record.alias>=q.nodes.size();const auto p=source.p+source.n*(inner?Inner:Outer);double u=source.u,v=source.v;
        if(record.layer==2){const auto& a=q.nodes[record.a];const auto& z=q.nodes[record.b];const double length=std::sqrt(DotD(z.p-a.p,z.p-a.p));Need(std::isfinite(length)&&length>0,"wall actual edge chart domain");u=(record.alias%q.nodes.size()==record.b)?length:0.;v=inner?Inner:Outer;}
        Vertex3D vertex{static_cast<float>(p.x),static_cast<float>(p.y),static_cast<float>(p.z),0,color,static_cast<float>(u),static_cast<float>(v)};
        const Vec3 hint{static_cast<float>(source.n.x),static_cast<float>(source.n.y),static_cast<float>(source.n.z)};
        Need(std::isfinite(vertex.x)&&std::isfinite(vertex.y)&&std::isfinite(vertex.z)&&std::isfinite(vertex.u)&&std::isfinite(vertex.v),"finalf32 owned finite");
        if(record.id<OriginalCount){b.model.bindMesh.vertices.at(record.id)=vertex;b.model.influences.at(record.id)=rigid;b.hints.at(record.id)=inner?hint*-1.f:hint;}
        else{Need(record.id==b.model.bindMesh.vertices.size(),"append owned IDs only");b.model.bindMesh.vertices.push_back(vertex);b.model.influences.push_back(rigid);b.hints.push_back(inner?hint*-1.f:hint);b.receipt.criticalVertices[2].push_back(record.id);}
    }
    auto& hair=b.triangles[3];const auto local=q.oldStart-art.ranges[3].firstIndex;Need(local+432<=hair.size(),"old144 face span");hair.erase(hair.begin()+static_cast<std::ptrdiff_t>(local),hair.begin()+static_cast<std::ptrdiff_t>(local+432));hair.insert(hair.begin()+static_cast<std::ptrdiff_t>(local),q.indices.begin(),q.indices.end());
    return q;
}
// END_EXTRACT_APPLY_RECIPE

// 下列為明示輸入與protected欄位守門，不改搬移的幾何recipe。
inline void SourceContract(const SkinnedModel3D& original,const ExplorerArtReceipt& art,
    const ExplorerArtAuthor::Builder& b) {
    Need(original.Valid()&&original.bindMesh.vertices.size()==7861&&original.bindMesh.indices.size()==31776&&
        original.influences.size()==7861&&original.tangents.empty()&&original.skeleton.joints.size()==15&&
        original.inverseBindMatrices.size()==15,"official7861 original source contract");
    Need(b.model.bindMesh.vertices.size()==7861&&b.model.influences.size()==7861&&art.joins.size()==14,
        "unchanged original author population and14joins");
    std::uint32_t cursor{};
    for(unsigned group=0;group<5;++group){const auto& r=art.ranges[group];const auto& indices=b.triangles[group];
        Need(r.material==static_cast<ExplorerArtMaterial>(group)&&r.firstIndex==cursor&&
            r.indexCount==indices.size(),"all five source material ranges exact");
        Need(std::equal(indices.begin(),indices.end(),original.bindMesh.indices.begin()+cursor),
            "all original directed index streams exact before owned replacement");
        cursor+=r.indexCount;
    }
    Need(cursor==original.bindMesh.indices.size(),"complete original source index population");
    for(unsigned side=0;side<2;++side){const auto& source=art.eyeLensSpans[side];const auto& local=b.receipt.eyeLensSpans[side];
        Need(source.firstIndex==local.firstIndex+art.ranges[2].firstIndex&&source.indexCount==local.indexCount&&
            source.firstVertex==local.firstVertex&&source.vertexCount==local.vertexCount,
            "source lens spans exact original bookkeeping");}
    Need(art.skinColorVertices==b.receipt.skinColorVertices&&art.criticalVertices==b.receipt.criticalVertices&&
        art.joins.size()==b.receipt.joins.size(),"source skinColor/critical15/join inventory exact");
    for(std::size_t i=0;i<art.joins.size();++i)Need(art.joins[i].boundary==b.receipt.joins[i].boundary&&
        art.joins[i].joint==b.receipt.joins[i].joint,"source14 join records exact");
    for(std::size_t i=0;i<original.bindMesh.vertices.size();++i){const auto& a=original.bindMesh.vertices[i];const auto& z=b.model.bindMesh.vertices[i];
        Need(Bits(a.x,z.x)&&Bits(a.y,z.y)&&Bits(a.z,z.z)&&Bits(a.u,z.u)&&Bits(a.v,z.v)&&a.color==z.color&&
            original.influences[i].joints==b.model.influences[i].joints&&
            original.influences[i].weights==b.model.influences[i].weights,
            "original geometry UV/color/influence raw source matches unchanged authors");}
}
inline void ProtectedContract(const SkinnedModel3D& source,const SkinnedModel3D& candidate,const Recipe& q) {
    Need(candidate.Valid()&&candidate.bindMesh.vertices.size()==9225&&candidate.bindMesh.indices.size()==36792&&
        candidate.influences.size()==9225&&candidate.tangents.empty(),"fixed accepted recipe output population");
    const std::set<std::uint32_t> owned(q.oldIds.begin(),q.oldIds.end());
    Need(owned.size()==74,"one fixed74-record original hair ownership");
    for(std::uint32_t i=0;i<OriginalCount;++i)if(!owned.contains(i))Need(VertexRaw(source.bindMesh.vertices[i],candidate.bindMesh.vertices[i])&&
        source.influences[i].joints==candidate.influences[i].joints&&source.influences[i].weights==candidate.influences[i].weights,
        "all7787 nonowned raw vertex and influence fields exact");
}
}
// caller須提供同一原官方Explorer model/art；不修改caller、pose或renderer資源。
// /fp:strict及原完整物理驗證仍由整合／驗證TU明示保護，結果不是物理資格receipt。
inline Result Author(const Aevrynd::SkinnedModel3D& original,const ExplorerArtReceipt& originalArt,
    const Checkpoint& checkpoint={}) {
    checkpoint();
    Detail::Need(original.Valid()&&original.bindMesh.vertices.size()==7861&&original.bindMesh.indices.size()==31776,
        "official original7861 Explorer required");
    Result result;result.model=original;
    ExplorerArtAuthor::Builder builder(result.model);ExplorerArtAuthor::CoatBody(builder);
    ExplorerArtAuthor::PantsAndBoots(builder);ExplorerArtAuthor::HeadAndHands(builder);ExplorerArtAuthor::Backpack(builder);
    Detail::SourceContract(original,originalArt,builder);
    const auto recipe=Detail::Apply(builder,original,checkpoint);
    result.art=builder.Finish();
    Detail::ProtectedContract(original,result.model,recipe);
    Detail::Need(result.art.joins.size()==14&&result.art.criticalVertices.size()==15&&
        result.art.skinColorVertices==originalArt.skinColorVertices,"returned whole art metadata contract");
    checkpoint();return result;
}
}
