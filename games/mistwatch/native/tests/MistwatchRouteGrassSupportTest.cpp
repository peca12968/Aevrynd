// 五真來源／合成支撐測試；不代表完整森林、碰撞、GPU或美術通過。
#include "MistwatchRouteGrassSupport.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
using namespace Aevrynd;
namespace S=Mistwatch::RouteGrassSupport;
namespace C=Mistwatch::CoreHerbClusters;
namespace D=Mistwatch::LandscapeSpeciesDetail;
unsigned checks{},negatives{},safeFixtures{},floatingFixtures{},buriedFixtures{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
template<class Work>void Throws(Work&& action,std::string_view reason){
    bool caught=false;
    try{action();}catch(const std::invalid_argument& e){Need(std::string_view(e.what())==reason,"WRONG_NEGATIVE_REASON");caught=true;}
    Need(caught,"INVALID_INPUT_ACCEPTED");++negatives;
}
std::uint64_t IndependentFingerprint(const Mesh3D& mesh){
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t value){for(unsigned byte=0;byte<4;++byte){hash^=(value>>(8*byte))&255u;hash*=1099511628211ull;}};
    word(static_cast<std::uint32_t>(mesh.vertices.size()));word(static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& vertex:mesh.vertices){
        for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})word(std::bit_cast<std::uint32_t>(value));
        word(vertex.normal);word(vertex.color);
    }
    for(auto index:mesh.indices)word(index);return hash;
}
float MinimumY(const Mesh3D& mesh){
    float value=mesh.vertices.front().y;for(const auto& v:mesh.vertices)value=(std::min)(value,v.y);return value;
}
// 獨立展開逆向SNORM8權重、source-height smoothstep與W量化，不用受測函式產生預期。
std::uint32_t IndependentWind(std::uint32_t normal,double height){
    const unsigned byte=normal>>24;const int signedByte=byte>=128?int(byte)-256:int(byte);
    const float weight=float(signedByte)/127.f;
    if(weight<0||height>=double(.35f))return normal&0xff000000u;
    const double t=std::clamp((height-double(.035f))/double(.35f-.035f),0.,1.);
    const double wind=(1.-double(std::clamp(weight,0.f,1.f)))*t*t*(3.-2.*t);
    const float inverse=static_cast<float>(1.-wind);
    const int quantized=int(inverse*127.f+.5f);
    return std::uint32_t(std::uint8_t(std::int8_t(quantized)))<<24;
}
void AuthorWind(const Mesh3D& source,Mesh3D& derived,float scale){
    const float minimum=MinimumY(source);
    unsigned pinned{},fading{},upper{},moving{};
    for(unsigned i=0;i<source.vertices.size();++i){
        const auto& raw=source.vertices[i];const double height=(double(raw.y)-minimum)*scale;
        const auto expected=IndependentWind(raw.normal,height);
        Need(C::Detail::RootWindBits(raw.normal,height)==expected,"SOURCE_WIND_POLICY_ORACLE_DIFFERENCE");
        auto& actual=derived.vertices[i];const auto xyz=actual.normal&0x00ffffffu;
        actual.normal=xyz|C::Detail::RootWindBits(raw.normal,height);
        Need((actual.normal&0x00ffffffu)==xyz&&(actual.normal&0xff000000u)==expected,"WIND_CHANGED_DERIVED_NORMAL_XYZ");
        if(height<=double(.035f)){++pinned;Need(UnpackNormalSnorm8(actual.normal).w==1.f,"SOURCE_ROOT_NOT_PINNED");}
        else if(height<double(.35f))++fading;
        else{++upper;Need((actual.normal&0xff000000u)==(raw.normal&0xff000000u),"UPPER_SOURCE_WIND_CHANGED");}
        if(UnpackNormalSnorm8(actual.normal).w<1.f)++moving;
    }
    Need(pinned&&fading&&upper&&moving,"TRUE_SOURCE_ROOT_FADE_UPPER_WIND_MISSING");
}
Mesh3D Plane(float xSlope,float zSlope){
    Mesh3D mesh;
    for(const auto point:{Vec3{-4,0,-4},Vec3{4,0,-4},Vec3{4,0,4},Vec3{-4,0,4}}){
        Vertex3D v{};v.x=point.x;v.z=point.z;v.y=xSlope*v.x+zSlope*v.z;
        v.normal=PackNormalSnorm8(Normalize({-xSlope,1,-zSlope}));v.color=0xffffffffu;
        mesh.vertices.push_back(v);
    }
    mesh.indices={0,2,1,0,3,2};return mesh;
}
void SameProofGeometry(const S::Proof& a,const S::Proof& b){
    const auto& x=a.contact;const auto& y=b.contact;
    Need(x.safe==y.safe&&x.rejection==y.rejection&&a.allFaces==b.allFaces&&
        a.sourceFootPieces==b.sourceFootPieces&&a.windPinnedVertices==b.windPinnedVertices&&
        a.sourceFingerprint==b.sourceFingerprint&&a.derivedFingerprint==b.derivedFingerprint&&
        a.surfaceFingerprint==b.surfaceFingerprint,"REFERENCE_PROOF_IDENTITY_OR_DECISION_CHANGED");
    Need(Bits(x.minimumGapBound,y.minimumGapBound)&&Bits(x.maximumFootGapBound,y.maximumFootGapBound)&&
        Bits(x.minimumNormalY,y.minimumNormalY)&&Bits(x.maximumHeightDelta,y.maximumHeightDelta)&&
        x.footprintFaces==y.footprintFaces&&x.evaluatedFaces==y.evaluatedFaces&&x.skippedFaces==y.skippedFaces&&
        x.footPieces==y.footPieces,"REFERENCE_PROOF_GEOMETRY_CHANGED");
    // broad-phase/cache操作數不是幾何結果，不要求 cachedGroundFaces/aabbRejected 相同。
}
void IdentityNegatives(unsigned slot,const S::Ground& ground,const Mesh3D& source,const Mesh3D& derived,
                       const Transform3D& pose,Vec3 root,float radius){
    const auto type=static_cast<S::Source>(slot);
    const auto check=[&](const Mesh3D& raw,const Mesh3D& mesh){return S::Check(type,ground,raw,mesh,pose,root,radius);};
    auto raw=source;raw.vertices.front().u+=.125f;
    Throws([&]{(void)check(raw,derived);},"ROUTE_GRASS_SUPPORT_SOURCE_IDENTITY");
    raw=source;raw.vertices.front().color^=1u;
    Throws([&]{(void)check(raw,derived);},"ROUTE_GRASS_SUPPORT_SOURCE_IDENTITY");
    raw=source;raw.vertices.front().normal^=0x01000000u;
    Throws([&]{(void)check(raw,derived);},"ROUTE_GRASS_SUPPORT_SOURCE_IDENTITY");
    raw=source;std::swap(raw.indices[1],raw.indices[2]);
    Throws([&]{(void)check(raw,derived);},"ROUTE_GRASS_SUPPORT_SOURCE_IDENTITY");
    raw=source;raw.vertices.push_back(raw.vertices.front());
    Throws([&]{(void)check(raw,derived);},"ROUTE_GRASS_SUPPORT_SOURCE_TOPOLOGY");
    auto wrong=derived;wrong.vertices.front().x=std::nextafter(wrong.vertices.front().x,std::numeric_limits<float>::infinity());
    Throws([&]{(void)check(source,wrong);},"ROUTE_GRASS_SUPPORT_MATERIAL_OR_XZ_CHANGED");
    wrong=derived;wrong.vertices.front().z=std::nextafter(wrong.vertices.front().z,std::numeric_limits<float>::infinity());
    Throws([&]{(void)check(source,wrong);},"ROUTE_GRASS_SUPPORT_MATERIAL_OR_XZ_CHANGED");
    wrong=derived;wrong.vertices.front().u+=.125f;
    Throws([&]{(void)check(source,wrong);},"ROUTE_GRASS_SUPPORT_MATERIAL_OR_XZ_CHANGED");
    wrong=derived;wrong.vertices.front().v+=.125f;
    Throws([&]{(void)check(source,wrong);},"ROUTE_GRASS_SUPPORT_MATERIAL_OR_XZ_CHANGED");
    wrong=derived;wrong.vertices.front().color^=1u;
    Throws([&]{(void)check(source,wrong);},"ROUTE_GRASS_SUPPORT_MATERIAL_OR_XZ_CHANGED");
    wrong=derived;std::swap(wrong.indices[1],wrong.indices[2]);
    Throws([&]{(void)check(source,wrong);},"ROUTE_GRASS_SUPPORT_INPUT");
    wrong=derived;wrong.vertices.push_back(wrong.vertices.front());
    Throws([&]{(void)check(source,wrong);},"ROUTE_GRASS_SUPPORT_INPUT");
    wrong=derived;wrong.vertices.front().normal^=0x01000000u;
    Throws([&]{(void)check(source,wrong);},"ROUTE_GRASS_SUPPORT_WIND_IDENTITY");
    wrong=derived;wrong.vertices.front().u=std::numeric_limits<float>::quiet_NaN();
    Throws([&]{(void)check(source,wrong);},"LANDSCAPE_SPECIES_NONFINITE_MESH");
    Throws([&]{(void)S::Check(static_cast<S::Source>(255),ground,source,derived,pose,root,radius);},"ROUTE_GRASS_SUPPORT_SOURCE_SLOT");
    Throws([&]{(void)S::Check(static_cast<S::Source>((slot+1)%5),ground,source,derived,pose,root,radius);},"ROUTE_GRASS_SUPPORT_SOURCE_TOPOLOGY");
    auto invalidPose=pose;invalidPose.scale.y*=2.f;
    Throws([&]{(void)S::Check(type,ground,source,derived,invalidPose,root,radius);},"ROUTE_GRASS_SUPPORT_INPUT");
    Throws([&]{(void)S::Check(type,ground,source,derived,pose,root,0);},"ROUTE_GRASS_SUPPORT_INPUT");
}
void RunFixture(unsigned slot,const Mesh3D& source,float slopeX,float slopeZ){
    const auto rawFingerprint=IndependentFingerprint(source);
    auto callerGround=Plane(slopeX,slopeZ);const auto groundFingerprint=IndependentFingerprint(callerGround);
    const S::Ground ground(callerGround);
    Need(ground.Fingerprint()==groundFingerprint,"GROUND_INTERNAL_SEAL_NOT_ACTUAL_MESH");
    const float scale=.5f,minimum=MinimumY(source);
    const Transform3D pose{{0,-minimum*scale,0},{0,.37f,0},{scale,scale,scale}};
    const auto sample=[&](float x,float z)->std::optional<double>{return ground.Surface().At({x,0,z}).Approximate();};
    auto conformed=CreateGroundConformedMesh3D(source,pose,sample);
    Need(conformed.root.x==0&&conformed.root.z==0&&std::abs(conformed.root.y)<.00005f,"FIXTURE_ROOT_NOT_ATTACHED");
    Need(conformed.sampledVertices>0&&conformed.mesh.indices==source.indices&&
        conformed.mesh.vertices.size()==source.vertices.size(),"GROUND_CONFORM_SOURCE_IDENTITY_LOST");
    if(slopeX!=0||slopeZ!=0)Need(conformed.movedVertices>0&&conformed.affectedFaces>0&&conformed.rebuiltNormals>0,
        "SLOPED_FIXTURE_DID_NOT_USE_REAL_CONFORM");
    auto derived=std::move(conformed.mesh);AuthorWind(source,derived,scale);
    float localRadius{};for(const auto& v:source.vertices)localRadius=(std::max)(localRadius,std::hypot(v.x,v.z));
    const float radius=std::nextafter(localRadius*scale+.19f,std::numeric_limits<float>::infinity());
    const auto root=conformed.root;const auto type=static_cast<S::Source>(slot);
    const auto derivedFingerprint=IndependentFingerprint(derived);
    const auto check=[&](const Mesh3D& mesh){return S::Check(type,ground,source,mesh,pose,root,radius);};
    const auto normal=check(derived);
    if(slopeX==0&&slopeZ==0)Throws([&]{(void)Mistwatch::CoreHerbSupport::Check(ground.Surface(),source,derived,
        pose,root,radius,ground.Fingerprint());},"CORE_HERB_SUPPORT_SOURCE_IDENTITY");
    Need(normal.contact.safe,"TRUE_GRASS_SYNTHETIC_FIXTURE_NOT_SAFE");++safeFixtures;
    Need(normal.allFaces==source.indices.size()/3&&normal.sourceFootPieces>0&&normal.windPinnedVertices>0&&
        normal.contact.footprintFaces>0,"SAFE_PROOF_SKIPPED_SOURCE_FACES_OR_ROOT_PIECES");
    Need(normal.sourceFingerprint==rawFingerprint&&normal.sourceFingerprint==S::SourceFingerprints[slot]&&
        normal.derivedFingerprint==derivedFingerprint&&normal.surfaceFingerprint==groundFingerprint,"SAFE_PROOF_SEAL_DIFFERS");
    SameProofGeometry(normal,S::Check(type,ground,source,derived,pose,root,radius,true,nullptr,false,false));
    SameProofGeometry(normal,check(derived));
    const auto noBounds=S::Check(type,ground,source,derived,pose,root,radius,false);
    Need(noBounds.contact.safe&&noBounds.allFaces==normal.allFaces&&noBounds.sourceFootPieces==normal.sourceFootPieces,
        "UNPRUNED_EXACT_PROOF_CHANGED_SAFE_DECISION");
    // 所有來源面都平移；失敗允許first divergence早停，不假稱負控走完全部面。
    auto floating=derived;for(auto& v:floating.vertices)v.y+=.12f/scale;
    const auto high=check(floating);
    Need(!high.contact.safe&&high.contact.rejection==Mistwatch::OuterUnderstory::Reject::Foot&&
        high.contact.maximumFootGapBound>.04f,"ALL_VERTICES_FLOATING_NOT_REJECTED");++negatives;++floatingFixtures;
    SameProofGeometry(high,S::Check(type,ground,source,floating,pose,root,radius,true,nullptr,false,false));
    auto buried=derived;for(auto& v:buried.vertices)v.y-=.12f/scale;
    const auto low=check(buried);
    Need(!low.contact.safe&&low.contact.minimumGapBound<-.04f,"ALL_VERTICES_BURIED_NOT_REJECTED");++negatives;++buriedFixtures;
    SameProofGeometry(low,S::Check(type,ground,source,buried,pose,root,radius,true,nullptr,false,false));
    IdentityNegatives(slot,ground,source,derived,pose,root,radius);
    for(auto& v:callerGround.vertices)v.y+=1.f;
    Need(IndependentFingerprint(callerGround)!=groundFingerprint,"CALLER_GROUND_MUTATION_FIXTURE_NOT_CHANGED");
    Need(ground.Fingerprint()==groundFingerprint&&Compare(ground.Surface().At({0,0,0}),D::R{})==0,
        "OWNING_GROUND_ALIASED_MUTABLE_CALLER");
    SameProofGeometry(normal,check(derived));
    Need(IndependentFingerprint(source)==rawFingerprint&&IndependentFingerprint(derived)==derivedFingerprint,"CHECK_MUTATED_RAW_OR_DERIVED");
    std::cout<<"fixture_source="<<slot<<" slope="<<slopeX<<','<<slopeZ<<" source_fp="<<rawFingerprint
        <<" derived_fp="<<derivedFingerprint<<" ground_fp="<<groundFingerprint<<" faces="<<normal.allFaces
        <<" foot_pieces="<<normal.sourceFootPieces<<" pinned="<<normal.windPinnedVertices
        <<" min_gap="<<normal.contact.minimumGapBound<<" max_foot_gap="<<normal.contact.maximumFootGapBound<<'\n';
}
}
int main(int argc,char** argv){try{
    static_assert(!std::is_copy_constructible_v<S::Ground>&&!std::is_move_constructible_v<S::Ground>);
    if(argc!=3||std::string_view(argv[1])!="--runtime-assets")throw std::runtime_error("USE --runtime-assets ABS");
    const std::filesystem::path assets(argv[2]);Need(assets.is_absolute(),"ABSOLUTE_RUNTIME_ASSETS_REQUIRED");
    Need(Bits(Mistwatch::OuterUnderstory::SlopeLimit,.88f)&&Bits(Mistwatch::OuterUnderstory::HeightLimit,.24f)&&
        Bits(Mistwatch::OuterUnderstory::ContactLimit,.04f)&&Bits(Mistwatch::OuterUnderstory::WindRadius,.19f)&&
        Bits(Mistwatch::OuterUnderstory::WindUp,.011f)&&Bits(C::RootPinnedHeight,.035f)&&Bits(C::RootWindFadeHeight,.35f),
        "SOURCE_FOOT_WIND_OR_GROUND_GATES_CHANGED");
    for(unsigned slot=0;slot<5;++slot){
        auto loaded=LoadAevryndMeshFile(assets/"mistwatch-grass/meshes"/Mistwatch::GrassMeshName(slot));
        Need(bool(loaded),"TRUE_RUNTIME_GRASS_MISSING");
        Need(loaded.mesh.vertices.size()==Mistwatch::GrassVertexCounts[slot]&&
            loaded.mesh.indices.size()==3u*Mistwatch::GrassTriangleCounts[slot],"TRUE_RUNTIME_GRASS_TOPOLOGY_CHANGED");
        Mistwatch::NormalizeGrassAsset(loaded.mesh);
        Need(IndependentFingerprint(loaded.mesh)==S::SourceFingerprints[slot],"TRUE_NORMALIZED_GRASS_IDENTITY_CHANGED");
        RunFixture(slot,loaded.mesh,0,0);
        RunFixture(slot,loaded.mesh,.015f,0);
        RunFixture(slot,loaded.mesh,-.015f,.0075f);
    }
    auto downward=Plane(0,0);std::swap(downward.indices[1],downward.indices[2]);
    Throws([&]{const S::Ground invalid(downward);},"LANDSCAPE_SPECIES_SURFACE_NOT_UPWARD");
    Need(safeFixtures==15&&floatingFixtures==15&&buriedFixtures==15,"INCOMPLETE_FIVE_SOURCE_FIXTURE_SET");
    std::cout<<"ROUTE_GRASS_SUPPORT_PROTOTYPE_FIXTURE_OK; checks="<<checks<<"; negatives="<<negatives
        <<"; safe_fixtures="<<safeFixtures<<"; floating_fixtures="<<floatingFixtures<<"; buried_fixtures="<<buriedFixtures
        <<"; full_world_verified=0; actual_gpu_ground_verified=0; collision_verified=0; native_verified=0; art_accepted=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"ROUTE_GRASS_SUPPORT_PROTOTYPE_FAILED: "<<e.what()<<'\n';return 1;}}
