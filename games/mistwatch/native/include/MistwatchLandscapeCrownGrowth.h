#pragma once
#include "MistwatchRootSupport.h"
#include <array>
#include <set>

namespace Mistwatch::LandscapeCrownGrowth {
inline constexpr std::string_view Profile="overstory-crown-growth-v1";
// 四個原始 LOD2 材質與 near twig 的完整解碼封印，包含作者法線、UV、色彩與索引。
inline constexpr std::array<std::uint64_t,5> SourceFingerprints{
    1415342071400289810ull,153207242796654823ull,11699678141493448690ull,
    9544599023395000417ull,6551883913766488749ull};
struct Result {
    Aevrynd::Mesh3D mesh;
    std::uint64_t sourceFingerprint{},derivedFingerprint{};
    unsigned movedVertices{},frozenRootFaces{},affectedNormals{};
};
namespace Detail {
namespace D=::Mistwatch::LandscapeSpeciesDetail;
inline void Need(bool yes,const char* reason){if(!yes)throw std::invalid_argument(reason);}
inline bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
using Key=std::array<std::uint32_t,3>;
inline Key PositionKey(const Aevrynd::Vertex3D& v){return {
    v.x==0?0:std::bit_cast<std::uint32_t>(v.x),v.y==0?0:std::bit_cast<std::uint32_t>(v.y),
    v.z==0?0:std::bit_cast<std::uint32_t>(v.z)};}
}
// 只允許明確的隔離 capture 或 replay，不將候選自動變成正式預設。
inline bool Resolve(bool capture,bool replay,std::wstring_view requested){
    if(requested.empty()||requested==L"original")return false;
    Detail::Need(requested==L"overstory-crown-growth-v1","CROWN_GROWTH_PROFILE");
    Detail::Need(capture!=replay,"CROWN_GROWTH_REQUIRED_CONTEXT");return true;
}
// 僅產生基底 520 株使用的新網格；不修改姿態、不建立新 Stand，也不碰原始 96 株小樹資產。
// 根部未移動不代表既有浮根已修好，地表／路徑／美術驗收仍由呼叫者分別執行。
inline Result Grow(const Aevrynd::Mesh3D& raw){
    using namespace Aevrynd;namespace F=Detail;namespace D=LandscapeSpeciesDetail;
    D::ValidateMesh(raw,false);Result out;out.sourceFingerprint=RootSupportDetail::MeshFingerprint(raw);
    F::Need(std::find(SourceFingerprints.begin(),SourceFingerprints.end(),out.sourceFingerprint)!=SourceFingerprints.end(),
        "CROWN_GROWTH_SOURCE_SEAL");out.mesh=raw;
    std::set<F::Key> frozen;
    for(unsigned f=0;f<raw.indices.size();f+=3){bool lower{};
        for(unsigned j=0;j<3;++j)lower|=raw.vertices[raw.indices[f+j]].y<.35f;
        if(!lower)continue;++out.frozenRootFaces;
        // 凍結原始下幹整面及其所有位置相同的 UV 接縫副本，不只凍結低點。
        for(unsigned j=0;j<3;++j)frozen.insert(F::PositionKey(raw.vertices[raw.indices[f+j]]));
    }
    std::vector<bool> moved(raw.vertices.size()),affected(raw.vertices.size());
    for(unsigned i=0;i<raw.vertices.size();++i){const auto& a=raw.vertices[i];auto& b=out.mesh.vertices[i];
        if(a.y<=3.f||frozen.contains(F::PositionKey(a)))continue;
        const float t=std::clamp((a.y-3.f)/3.f,0.f,1.f),blend=t*t*(3.f-2.f*t),width=1.f+.3f*blend;
        b.x=a.x*width;b.z=a.z*width;b.y=a.y+.3f*(a.y-3.f);
        F::Need(D::Finite({b.x,b.y,b.z}),"CROWN_GROWTH_POSITION_NONFINITE");
        moved[i]=!F::Same(a.x,b.x)||!F::Same(a.y,b.y)||!F::Same(a.z,b.z);
        out.movedVertices+=moved[i]?1u:0u;
    }
    std::vector<Vec3> normals(raw.vertices.size());
    for(unsigned f=0;f<raw.indices.size();f+=3){const auto a=raw.indices[f],b=raw.indices[f+1],c=raw.indices[f+2];
        const auto before=Cross(D::Vertex(raw,b)-D::Vertex(raw,a),D::Vertex(raw,c)-D::Vertex(raw,a));
        const auto after=Cross(D::Vertex(out.mesh,b)-D::Vertex(out.mesh,a),D::Vertex(out.mesh,c)-D::Vertex(out.mesh,a));
        F::Need(D::Finite(after)&&Length(after)>0&&Dot(before,after)>0,"CROWN_GROWTH_FACE_ORIENTATION");
        // 受影響頂點仍加總所有真正相鄰面，不能只加總有移動的面。
        normals[a]=normals[a]+after;normals[b]=normals[b]+after;normals[c]=normals[c]+after;
        if(moved[a]||moved[b]||moved[c])affected[a]=affected[b]=affected[c]=true;
    }
    for(unsigned i=0;i<raw.vertices.size();++i)if(affected[i]){
        F::Need(D::Finite(normals[i])&&Length(normals[i])>1.e-8f,"CROWN_GROWTH_NORMAL_DEGENERATE");
        out.mesh.vertices[i].normal=(PackNormalSnorm8(Normalize(normals[i]))&0x00ffffffu)|(raw.vertices[i].normal&0xff000000u);
        ++out.affectedNormals;
    }
    std::map<F::Key,F::Key> welded;
    for(unsigned i=0;i<raw.vertices.size();++i){const auto& a=raw.vertices[i];const auto& b=out.mesh.vertices[i];
        F::Need(F::Same(a.u,b.u)&&F::Same(a.v,b.v)&&a.color==b.color&&
            (a.normal&0xff000000u)==(b.normal&0xff000000u),"CROWN_GROWTH_MATERIAL_IDENTITY");
        if(a.y<=3.f||frozen.contains(F::PositionKey(a)))F::Need(F::Same(a.x,b.x)&&F::Same(a.y,b.y)&&F::Same(a.z,b.z),
            "CROWN_GROWTH_ROOT_MOVED");
        const auto [it,added]=welded.emplace(F::PositionKey(a),F::PositionKey(b));
        F::Need(added||it->second==F::PositionKey(b),"CROWN_GROWTH_SPLIT_WELD");
    }
    F::Need(out.mesh.indices==raw.indices,"CROWN_GROWTH_TOPOLOGY_CHANGED");
    F::Need(out.movedVertices>0&&out.affectedNormals>0,"CROWN_GROWTH_NO_CHANGE");
    out.derivedFingerprint=RootSupportDetail::MeshFingerprint(out.mesh);return out;
}
}
