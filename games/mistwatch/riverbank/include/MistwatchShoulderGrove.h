#pragma once
#include "MistwatchBroadleaf.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace Mistwatch::ShoulderGrove {
inline constexpr std::size_t Target=16,SourceCount=616;
inline constexpr std::string_view Profile="shoulder-deformed-root-v1";
inline constexpr std::uint64_t InstanceNamespace=0x4d57534847525631ull;
// 此值只證明離線幾何驗收；不代表原生畫面、完整自交或可達性已驗收。
inline constexpr bool IndependentGeometryVerified=true,DefaultEnabled=false;
inline constexpr std::array<std::uint64_t,3> SourceFingerprints{
    12442856667128630539ull,11046199509515305128ull,12155645616207841595ull};
inline constexpr std::array<std::string_view,3> RawAssetSha256{
    "4182C1D2D3E726F5C77DD84ADF59EA5D4E022D49B1F211B34B488DBC165B68CC",
    "C46ECA29C84D6EA9DB320444D01ED4F16403A2A7F50729178E70C7886B28BC5B",
    "1C765173EBC17A3BE167C35EF1B8DF8329FB4CF98F468F4DA13FD42533B677FD"};
struct Plant {std::uint32_t sourceOrdinal{},region{};Aevrynd::Transform3D pose{};Aevrynd::Mesh3D derivedTrunk;std::uint64_t derivedFingerprint{};};
struct Retained {std::uint32_t sourceOrdinal{};Aevrynd::Transform3D pose{};};
struct Plan {
    std::vector<Plant> plants;std::vector<Retained> retained;std::array<bool,SourceCount> selected{};
    std::uint64_t sourcePoseFingerprint{},surfaceGeometryFingerprint{},surfaceFingerprint{};
    std::array<std::uint64_t,3> sourceFingerprints{};std::array<std::uint64_t,Target> derivedFingerprints{};
    bool complete{};
};
#include "MistwatchShoulderGroveData.inl"
namespace Detail {
using namespace Aevrynd;
inline void Need(bool condition,const char* error){if(!condition)throw std::invalid_argument(error);}
inline void Word(std::uint64_t& h,std::uint32_t w){for(unsigned b=0;b<4;++b){h^=(w>>(8*b))&255u;h*=1099511628211ull;}}
inline float Float(std::uint32_t w){return std::bit_cast<float>(w);}
inline std::array<std::uint32_t,9> PoseWords(const Transform3D& p){return {
    std::bit_cast<std::uint32_t>(p.position.x),std::bit_cast<std::uint32_t>(p.position.y),std::bit_cast<std::uint32_t>(p.position.z),
    std::bit_cast<std::uint32_t>(p.rotation.x),std::bit_cast<std::uint32_t>(p.rotation.y),std::bit_cast<std::uint32_t>(p.rotation.z),
    std::bit_cast<std::uint32_t>(p.scale.x),std::bit_cast<std::uint32_t>(p.scale.y),std::bit_cast<std::uint32_t>(p.scale.z)};}
inline Transform3D Pose(const std::array<std::uint32_t,9>& w){return {{Float(w[0]),Float(w[1]),Float(w[2])},{Float(w[3]),Float(w[4]),Float(w[5])},{Float(w[6]),Float(w[7]),Float(w[8])}};}
inline std::uint64_t PoseFingerprint(const std::vector<Transform3D>& poses){std::uint64_t h=14695981039346656037ull;
    Word(h,static_cast<std::uint32_t>(poses.size()));for(std::uint32_t i=0;i<poses.size();++i){Word(h,i);for(auto w:PoseWords(poses[i]))Word(h,w);}return h;}
inline std::uint64_t MeshFingerprint(const Mesh3D& mesh,bool geometryOnly=false){std::uint64_t h=14695981039346656037ull;
    Word(h,static_cast<std::uint32_t>(mesh.vertices.size()));Word(h,static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z})Word(h,std::bit_cast<std::uint32_t>(f));
        if(!geometryOnly){Word(h,std::bit_cast<std::uint32_t>(v.u));Word(h,std::bit_cast<std::uint32_t>(v.v));Word(h,v.normal);Word(h,v.color);}}
    for(auto i:mesh.indices)Word(h,i);return h;}
inline void ValidateMesh(const Mesh3D& mesh,bool normals){Need(mesh.Valid(),"SHOULDER_MESH_TOPOLOGY");
    for(const auto& v:mesh.vertices)for(float f:{v.x,v.y,v.z,v.u,v.v})Need(std::isfinite(f),"SHOULDER_NONFINITE_VERTEX");
    if(normals)for(auto i:mesh.indices){const auto n=UnpackNormalSnorm8(mesh.vertices[i].normal);
        const double length=std::sqrt(double(n.x)*n.x+double(n.y)*n.y+double(n.z)*n.z);
        Need(length>=.98&&length<=1.02,"SHOULDER_DERIVED_NORMAL");}}
inline void ValidateInputs(const std::vector<Transform3D>& original,const Mesh3D& ground,const std::array<Mesh3D,3>& sources){
    Need(original.size()==SourceCount,"SHOULDER_POPULATION_COUNT");
    for(std::size_t i=0;i<SourceCount;++i)Need(PoseWords(original[i])==OriginalPoseWords[i],"SHOULDER_POPULATION_POSE_ORDER");
    Need(PoseFingerprint(original)==PopulationSeal,"SHOULDER_POPULATION_SEAL");
    // 原始 SHA 為離線資產來源記錄；本 API 收到的是已根部化網格，核對其完整內容而非假稱重新讀檔。
    constexpr std::array<std::size_t,3> nv{4306,99050,1329},ni{10704,232725,5133};
    Need(std::bit_cast<std::uint32_t>(BroadleafRoot.x)==std::bit_cast<std::uint32_t>(.0056121834f)&&
        std::bit_cast<std::uint32_t>(BroadleafRoot.y)==std::bit_cast<std::uint32_t>(-.0222652573f)&&
        std::bit_cast<std::uint32_t>(BroadleafRoot.z)==std::bit_cast<std::uint32_t>(.0014146231f),"SHOULDER_ROOT_METADATA");
    constexpr std::array<std::string_view,3> names{"branches","leaves","trunk"};
    for(unsigned s=0;s<3;++s){Need(BroadleafMeshName(s)=="tree-small-02-lod2-node0-mesh0-primitive"+std::to_string(s)+"-material"+std::to_string(s)+"-tree-small-02-"+std::string(names[s])+".aemesh","SHOULDER_ASSET_METADATA");
        Need(sources[s].vertices.size()==nv[s]&&sources[s].indices.size()==ni[s],"SHOULDER_SOURCE_COUNTS");ValidateMesh(sources[s],false);
        Need(MeshFingerprint(sources[s])==SourceFingerprints[s],"SHOULDER_SOURCE_SEAL");}
    ValidateMesh(ground,false);Need(MeshFingerprint(ground,true)==TerrainGeometrySeal,"SHOULDER_TERRAIN_GEOMETRY");
}
inline Mesh3D Rebuild(const Mesh3D& source,const Recipe& r){Mesh3D mesh=source;
    std::uint32_t previous=0;bool first=true;
    for(const auto d:r.delta){Need(d.index<source.vertices.size()&&(first||d.index>previous)&&source.vertices[d.index].y<1.f,"SHOULDER_DELTA_DOMAIN");
        mesh.vertices[d.index].y=Float(d.y);mesh.vertices[d.index].normal=d.normal;previous=d.index;first=false;}
    for(const auto& v:r.append){const auto& w=v.words;mesh.vertices.push_back({Float(w[0]),Float(w[1]),Float(w[2]),w[5],w[6],Float(w[3]),Float(w[4])});}
    mesh.indices.clear();std::size_t next=0;
    for(std::uint32_t off=0;off<source.indices.size();off+=3){if(next<r.faces.size()&&r.faces[next].sourceOffset==off){const auto f=r.faces[next++];
            Need(f.count>=3&&f.count%3==0&&std::size_t(f.first)+f.count<=r.triangles.size(),"SHOULDER_FACE_RECIPE");
            mesh.indices.insert(mesh.indices.end(),r.triangles.begin()+f.first,r.triangles.begin()+f.first+f.count);
        }else mesh.indices.insert(mesh.indices.end(),source.indices.begin()+off,source.indices.begin()+off+3);}
    Need(next==r.faces.size(),"SHOULDER_FACE_RECIPE_ORDER");ValidateMesh(mesh,true);
    Need(MeshFingerprint(mesh)==r.seal,"SHOULDER_DERIVED_SEAL");return mesh;
}
}
inline bool IsSelected(const Plan& plan,std::size_t ordinal){Detail::Need(ordinal<SourceCount,"SHOULDER_ORDINAL_RANGE");return plan.selected[ordinal];}
// 可在上傳前重核真實網格與配方；不重新執行離線接地、位置搜尋或調和解算。
inline void Validate(const Plan& plan,const std::vector<Aevrynd::Transform3D>& original,const Aevrynd::Mesh3D& ground,const std::array<Aevrynd::Mesh3D,3>& sources){
    using namespace Detail;ValidateInputs(original,ground,sources);
    Need(plan.complete&&plan.plants.size()==Target&&plan.retained.size()==SourceCount-Target,"SHOULDER_PLAN_COUNT");
    Need(plan.sourcePoseFingerprint==PopulationSeal&&plan.surfaceGeometryFingerprint==TerrainGeometrySeal&&
        plan.surfaceFingerprint==MeshFingerprint(ground)&&plan.sourceFingerprints==SourceFingerprints,"SHOULDER_PLAN_IDENTITY");
    std::array<bool,SourceCount> selected{};std::array<unsigned,4> quota{};
    for(std::size_t i=0;i<Target;++i){const auto& p=plan.plants[i];const auto& r=Recipes[i];
        Need(p.sourceOrdinal<SourceCount&&!selected[p.sourceOrdinal],"SHOULDER_DUPLICATE_ORDINAL");selected[p.sourceOrdinal]=true;
        Need(p.sourceOrdinal==r.ordinal&&p.region==r.region&&p.region<4&&PoseWords(p.pose)==r.pose,"SHOULDER_PLANT_RECIPE");++quota[p.region];
        ValidateMesh(p.derivedTrunk,true);Need(p.derivedFingerprint==r.seal&&plan.derivedFingerprints[i]==r.seal&&MeshFingerprint(p.derivedTrunk)==r.seal,"SHOULDER_DERIVED_SEAL");}
    Need(quota==std::array<unsigned,4>{4,4,4,4}&&selected==plan.selected,"SHOULDER_QUOTA_SELECTION");
    std::size_t retained=0;for(std::uint32_t i=0;i<SourceCount;++i)if(!selected[i]){const auto& r=plan.retained[retained++];
        Need(r.sourceOrdinal==i&&PoseWords(r.pose)==PoseWords(original[i]),"SHOULDER_RETAINED_IDENTITY");}
}
inline Plan Create(const std::vector<Aevrynd::Transform3D>& original,const Aevrynd::Mesh3D& finalTerrain,const std::array<Aevrynd::Mesh3D,3>& rootedSources){
    using namespace Detail;ValidateInputs(original,finalTerrain,rootedSources);Plan p;
    p.sourcePoseFingerprint=PoseFingerprint(original);p.surfaceGeometryFingerprint=MeshFingerprint(finalTerrain,true);p.surfaceFingerprint=MeshFingerprint(finalTerrain);p.sourceFingerprints=SourceFingerprints;
    p.plants.reserve(Target);p.retained.reserve(SourceCount-Target);
    for(std::size_t i=0;i<Target;++i){const auto& r=Recipes[i];Need(r.ordinal<SourceCount&&!p.selected[r.ordinal],"SHOULDER_DUPLICATE_ORDINAL");p.selected[r.ordinal]=true;
        p.plants.push_back({r.ordinal,r.region,Pose(r.pose),Rebuild(rootedSources[2],r),r.seal});p.derivedFingerprints[i]=r.seal;}
    for(std::uint32_t i=0;i<SourceCount;++i)if(!p.selected[i])p.retained.push_back({i,original[i]});
    p.complete=true;Validate(p,original,finalTerrain,rootedSources);return p;
}
}
