#include "MistwatchSettlementSurface.h"
#include <iostream>
#include <limits>
#include <map>

namespace {
using namespace Aevrynd;
namespace S=Mistwatch::SettlementSurface;
unsigned checks{},negatives{};
void Need(bool condition,const char* reason){++checks;if(!condition)throw std::runtime_error(reason);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Geometry(const Vertex3D& a,const Vertex3D& b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal&&a.color==b.color;}
bool Vertex(const Vertex3D& a,const Vertex3D& b){return Geometry(a,b)&&Bits(a.u,b.u)&&Bits(a.v,b.v);}
bool Layer(const MaterialTextureLayerDefinition3D& a,const MaterialTextureLayerDefinition3D& b){
    if(a.baseColorPath!=b.baseColorPath||a.roughnessPath!=b.roughnessPath||a.normalPath!=b.normalPath||a.surfaceEncoding!=b.surfaceEncoding)return false;
    for(unsigned c=0;c<3;++c)if(a.integrity[c].expectedBytes!=b.integrity[c].expectedBytes||a.integrity[c].expectedSha256!=b.integrity[c].expectedSha256)return false;
    return true;
}
bool Material(const Material3D& a,const Material3D& b){
    return Bits(a.tint.r,b.tint.r)&&Bits(a.tint.g,b.tint.g)&&Bits(a.tint.b,b.tint.b)&&Bits(a.tint.a,b.tint.a)&&
        Bits(a.surface.roughness,b.surface.roughness)&&Bits(a.surface.metallic,b.surface.metallic)&&
        Bits(a.surface.emissive,b.surface.emissive)&&Bits(a.surface.occlusion,b.surface.occlusion)&&
        a.castsShadow==b.castsShadow&&a.textures==b.textures&&Bits(a.textureScale,b.textureScale)&&a.useUv==b.useUv&&
        a.materialClass==b.materialClass&&a.shadingModel==b.shadingModel&&a.alphaMode==b.alphaMode&&
        Bits(a.alphaCutoff,b.alphaCutoff)&&a.doubleSided==b.doubleSided&&Bits(a.normalScale,b.normalScale)&&
        Bits(a.baseTextureScale,b.baseTextureScale)&&Bits(a.detailTextureScale,b.detailTextureScale);
}
template<class Fn>void Reject(Fn fn,const char* reason){bool rejected=false;try{fn();}catch(const std::invalid_argument&){rejected=true;}++negatives;Need(rejected,reason);}
using Triangle=std::array<std::uint32_t,3>;
Triangle At(const Mesh3D& mesh,std::size_t t){return {mesh.indices[t*3],mesh.indices[t*3+1],mesh.indices[t*3+2]};}
void Fixture(const Mesh3D& source){
    const auto preserved=source;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto colliders=Mistwatch::WorkshopSolids();
    const auto entrance=Mistwatch::CreateEntranceRoadDomain(source);
    const auto settlement=Mistwatch::SettlementGround::Create(source,entrance);
    const auto a=S::Create(source,entrance,settlement),b=S::Create(source,entrance,settlement);
    Need(Mistwatch::SettlementGround::SameMesh(source,preserved),"Source mutated");
    Need(a.sourceDomains==b.sourceDomains&&a.fingerprints==b.fingerprints,"Repeat domain identity");
    const std::array<const Mesh3D*,3> meshes{&a.forest,&a.entrance,&a.gravel};
    const std::array<const Mesh3D*,3> repeated{&b.forest,&b.entrance,&b.gravel};
    std::map<Triangle,std::size_t> sourceTriangles;
    for(std::size_t t=0;t<source.indices.size()/3;++t)Need(sourceTriangles.emplace(At(source,t),t).second,"Independent source triangle uniqueness");
    std::vector<unsigned> seen(source.indices.size()/3),membership(source.vertices.size());
    for(unsigned domain=0;domain<3;++domain){const auto& mesh=*meshes[domain];
        Need(mesh.Valid()&&mesh.vertices.size()==source.vertices.size(),"Valid full identity vertex table");
        Need(Mistwatch::SettlementGround::SameMesh(mesh,*repeated[domain]),"Repeat all vertex and index bits");
        for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];
            Need(Geometry(v,source.vertices[i])&&Bits(v.v,source.vertices[i].v),"Geometry or rock coverage changed");
            Need(std::isfinite(v.u)&&std::isfinite(v.v)&&v.u>=0&&v.v>=0&&v.u+v.v<=1,"Invalid layer coverage");
        }
        std::size_t previous{};bool first=true;
        for(std::size_t t=0;t<mesh.indices.size()/3;++t){const auto triangle=At(mesh,t);const auto found=sourceTriangles.find(triangle);
            Need(found!=sourceTriangles.end(),"Triangle identity or winding changed");const auto original=found->second;
            Need(++seen[original]==1,"Overlapping material submissions");
            Need(first||original>previous,"Source relative triangle order");first=false;previous=original;
            Need(static_cast<unsigned>(a.sourceDomains[original])==domain,"Domain receipt differs from actual mesh");
            for(const auto index:triangle)membership[index]|=1u<<domain;
            if(domain==2){for(const auto index:triangle){const auto& v=source.vertices[index];
                // 獨立的工棚原始範圍；不以候選 Inside() 自證。
                const float extension=(std::max)(0.f,v.x+4.5f);
                Need(v.x>=-10.f&&v.x<=-1.f&&std::abs(v.z-59.f)<=2.5f-extension/3.5f,"Gravel outside existing workshop domain");
            }}
        }
    }
    Need(std::all_of(seen.begin(),seen.end(),[](unsigned count){return count==1;}),"Missing source triangle");
    Need(Mistwatch::SettlementGround::SameMesh(a.entrance,entrance.road),"Original entrance changed");
    Need(Mistwatch::SettlementGround::SameMesh(a.forest,settlement.partition.forest),"Forest changed beyond existing ground authoring");
    Need(a.gravelSourceTriangles==settlement.movedSourceTriangles&&a.gravelBoundaryVertices==settlement.newBoundaryVertices,"Gravel source mapping");
    Need(a.gravel.indices.size()/3==a.gravelSourceTriangles.size()&&!a.gravelSourceTriangles.empty(),"Gravel membership count");
    for(std::size_t i=0;i<a.gravelSourceTriangles.size();++i)Need(At(a.gravel,i)==At(source,a.gravelSourceTriangles[i]),"Gravel ordered source identity");
    std::vector<std::uint32_t> boundary;
    for(std::size_t i=0;i<membership.size();++i){
        if((membership[i]&4u)&&(membership[i]&3u)){
            boundary.push_back(static_cast<std::uint32_t>(i));const auto& g=a.gravel.vertices[i];
            const float expected=(std::max)(0.f,1.f-source.vertices[i].v-1.f/4096.f);
            Need(Bits(g.u,expected),"Boundary shared layer not dominant");
            Need(1.f-g.u-g.v<=1.f/4096.f+1.e-7f,"Boundary differing base remains visible");
            if(membership[i]&1u)Need(Vertex(g,a.forest.vertices[i]),"Forest gravel seam fields differ");
            if(membership[i]&2u)Need(Vertex(g,a.entrance.vertices[i]),"Road gravel seam fields differ");
        }
    }
    Need(boundary==a.gravelBoundaryVertices&&!boundary.empty(),"Independent shared vertex incidence");
    const auto after=Mistwatch::WorkshopSolids();Need(after.size()==colliders.size(),"Collider count changed");
    for(std::size_t i=0;i<after.size();++i)for(unsigned axis=0;axis<3;++axis){
        const auto component=[&](Vec3 v){return axis==0?v.x:axis==1?v.y:v.z;};
        Need(Bits(component(after[i].center),component(colliders[i].center))&&Bits(component(after[i].extents),component(colliders[i].extents)),"Collider modified");
    }
    const auto terrainAfter=Mistwatch::CreateTerrain();
    for(unsigned z=0;z<terrain.Settings().depth;++z)for(unsigned x=0;x<terrain.Settings().width;++x)
        Need(Bits(terrain.Height(x,z),terrainAfter.Height(x,z)),"Terrain height modified");
    Reject([&]{auto bad=entrance;bad.receipt.roadTriangles++;(void)S::Create(source,bad,settlement);},"Forged entrance receipt accepted");
    Reject([&]{auto bad=entrance;bad.road.indices[0]=bad.road.indices[1];(void)S::Create(source,bad,settlement);},"Corrupt entrance triangle accepted");
    Reject([&]{auto bad=entrance;bad.road.vertices[0].u+=.125f;(void)S::Create(source,bad,settlement);},"Foreign entrance coverage accepted");
    Reject([&]{auto bad=settlement;bad.partition.road.indices.pop_back();(void)S::Create(source,entrance,bad);},"Missing settlement triangle accepted");
    Reject([&]{auto bad=settlement;bad.partition.road.indices.back()=0;(void)S::Create(source,entrance,bad);},"Foreign settlement triangle accepted");
    Reject([&]{auto bad=settlement;bad.partition.forest.vertices[0].x+=.01f;(void)S::Create(source,entrance,bad);},"Settlement geometry mutation accepted");
    Reject([&]{auto bad=settlement;bad.newBoundaryVertices.pop_back();(void)S::Create(source,entrance,bad);},"Missing seam ID accepted");
    Reject([&]{auto bad=settlement;bad.movedSourceTriangles[0]++;(void)S::Create(source,entrance,bad);},"Wrong moved source ID accepted");
    Reject([&]{auto bad=settlement;bad.changedForestVertices++;(void)S::Create(source,entrance,bad);},"Forged changed count accepted");
    Reject([&]{auto bad=settlement;bad.partition.receipt.roadFingerprint^=1;(void)S::Create(source,entrance,bad);},"Forged settlement fingerprint accepted");
    Reject([&]{auto bad=source;bad.vertices[0].u=std::numeric_limits<float>::quiet_NaN();(void)S::Create(bad,entrance,settlement);},"NaN source accepted");
    Reject([&]{(void)S::Create(source,settlement.partition,settlement);},"Already authored entrance accepted");
    std::cout<<"SETTLEMENT_SURFACE_FIXTURE forest="<<a.forest.indices.size()/3<<" entrance="<<a.entrance.indices.size()/3
        <<" gravel="<<a.gravel.indices.size()/3<<" boundary="<<boundary.size()<<'\n';
}
void Materials(){
    const auto forest=Mistwatch::HabitatGroundDefinition(true,true),before=forest;
    const auto gravel=S::MaterialDefinition(forest),ground=Mistwatch::GroundMaterialDefinition();
    const MaterialTextureLayerDefinition3D actual{gravel.textures.baseColorPath,gravel.textures.roughnessPath,
        gravel.textures.normalPath,gravel.textures.surfaceEncoding,gravel.textures.integrity};
    Need(gravel.name=="settlement-gravel"&&Layer(actual,ground.textures.additionalLayers[0]),"Complete ground108 base metadata");
    Need(actual.baseColorPath=="ambientcg/ground108/slot-0-base.dds"&&actual.roughnessPath=="ambientcg/ground108/slot-0-arm.dds"&&
        actual.normalPath=="ambientcg/ground108/slot-0-normal.dds"&&actual.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrm,"Independent gravel channel semantics");
    constexpr const char* hashes[]{"2703b20ee77d556daa64535af15433bb3157083174e6d71671aa052ccae11589",
        "ce0c636264a9c1c5cb5055259dde80b92c9bea3145dc8ea8f28a7f9f3903e5ed","e63efd69f4d1e7978f1cd152c5b2115151bcbc208f9f661bc35af0ec6af80032"};
    for(unsigned c=0;c<3;++c)Need(actual.integrity[c].expectedBytes==1398276&&actual.integrity[c].expectedSha256==hashes[c],"Independent gravel seals");
    for(unsigned layer=0;layer<2;++layer)Need(Layer(gravel.textures.additionalLayers[layer],forest.textures.additionalLayers[layer]),"Shared seam layer changed");
    Need(Material(gravel.material,forest.material)&&Material(forest.material,before.material),"Render parameters changed");
    Need(gravel.textures.maximumResolution==forest.textures.maximumResolution&&gravel.textures.alphaMode==forest.textures.alphaMode&&
        Bits(gravel.textures.alphaCutoff,forest.textures.alphaCutoff),"Texture set metadata changed");
    Reject([&]{auto bad=forest;bad.textures.additionalLayers[0].integrity[0].expectedBytes++;(void)S::MaterialDefinition(bad);},"Leaf seal mutation accepted");
    Reject([&]{auto bad=forest;bad.textures.additionalLayers[1].normalPath="wrong.png";(void)S::MaterialDefinition(bad);},"Rock seam mismatch accepted");
    Reject([&]{auto bad=forest;bad.material.materialClass=MaterialClass3D::Surface;(void)S::MaterialDefinition(bad);},"Non terrain accepted");
    Reject([&]{auto bad=forest;bad.textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;(void)S::MaterialDefinition(bad);},"Wrong forest base encoding accepted");
    Reject([&]{auto bad=forest;bad.textures.alphaCutoff=std::numeric_limits<float>::quiet_NaN();(void)S::MaterialDefinition(bad);},"Invalid alpha metadata accepted");
}
}
int main()try{
    const auto terrain=Mistwatch::CreateTerrain();
    const auto source=Mistwatch::CreateTerrainMesh(terrain,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    Fixture(source);
    auto varied=source;
    // 第二組有限覆蓋夾具不是實際樹根作者環境，只驗證分域不依赖固定UV常數。
    for(auto& v:varied.vertices){v.v=.12f+.08f*(.5f+.5f*std::sin(v.x*.21f+v.z*.17f));v.u=.2f*(1.f-v.v);}
    Fixture(varied);Materials();
    std::cout<<"SETTLEMENT_SURFACE_CPU_PASS fixtures=2 checks="<<checks<<" negatives="<<negatives<<" native=0 art=0 runtime_assets_verified=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
