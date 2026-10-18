#include "MistwatchEntranceRoadDomain.h"
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchEntranceRoadMaterial.h"
#include "fallen/MistwatchAuthoredNativeExpected.h"
#include <iostream>
#include <string_view>

namespace {
void Need(bool okay,const char* reason){if(!okay)throw std::runtime_error(reason);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool GeometrySame(const Aevrynd::Vertex3D& a,const Aevrynd::Vertex3D& b){
    return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.normal==b.normal&&a.color==b.color;
}
bool VertexSame(const Aevrynd::Vertex3D& a,const Aevrynd::Vertex3D& b){
    return GeometrySame(a,b)&&Same(a.u,b.u)&&Same(a.v,b.v);
}
}
int wmain(int argc,wchar_t** argv)try{
    using namespace Mistwatch;using namespace Aevrynd;
    namespace Q=::Mistwatch::AuthoredQualification;
    namespace W=::Mistwatch::FallenWorldSeat;
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","Absolute runtime usage");
    const auto root=std::filesystem::path(argv[2]);Need(root.is_absolute(),"Absolute assets required");
    const auto terrain=CreateTerrain();
    const auto assets=Q::LoadAssets(root);W::Budget budget;
    const auto sourceContext=Q::CreateSourceContext(terrain,assets,budget);std::vector<W::Placement> placements;
    for(const auto& p:FallenLogPlacements()){W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;placements.push_back(q);}
    const auto context=Q::CreateContext(terrain,assets,sourceContext,placements,budget);
    Q::ValidateContext(context,terrain,assets,budget);const auto contextSeal=Q::ContextFingerprint(context);
    auto source=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    std::vector<Vec3> canopyRoots;for(const auto& plant:context.canopy.plants)canopyRoots.push_back(plant.pose.position);
    (void)ApplyForestHabitatFloor(source,terrain,{context.author.firs.roots,canopyRoots});
    const auto preserved=source;
    const auto result=CreateEntranceRoadDomain(source),repeat=CreateEntranceRoadDomain(source);
    Need(source.indices==preserved.indices&&source.vertices.size()==preserved.vertices.size(),"Source topology changed");
    Need(result.forest.vertices.size()==source.vertices.size()&&result.road.vertices.size()==source.vertices.size(),"Vertex IDs changed");
    std::vector<std::uint8_t> membership(source.vertices.size());
    std::size_t roadOffset{},forestOffset{};
    Need(result.triangleIsRoad.size()==source.indices.size()/3,"Missing triangle membership");
    for(std::size_t triangle=0;triangle<result.triangleIsRoad.size();++triangle){
        const bool road=result.triangleIsRoad[triangle]!=0;
        const auto& indices=road?result.road.indices:result.forest.indices;
        auto& offset=road?roadOffset:forestOffset;
        Need(offset+3<=indices.size(),"Partition missing source triangle");
        for(unsigned corner=0;corner<3;++corner){const auto original=source.indices[triangle*3+corner];
            Need(indices[offset+corner]==original,"Triangle identity or winding changed");
            membership[original]=std::uint8_t(membership[original]|(road?2u:1u));
            if(road){const auto& v=source.vertices[original];
                Need(v.z>=-10.f&&v.z<=28.f&&std::abs(v.x-.35f*std::sin(v.z*.19f))<2.10f+.15f*std::sin(v.z*.31f),
                    "Road escaped entrance into bank or bridge");}
        }
        offset+=3;
    }
    Need(roadOffset==result.road.indices.size()&&forestOffset==result.forest.indices.size()&&
        roadOffset+forestOffset==source.indices.size(),"Duplicate or extra rendered triangle");
    std::vector<std::uint32_t> independentShared;
    for(std::size_t i=0;i<membership.size();++i)if(membership[i]==3u)independentShared.push_back(std::uint32_t(i));
    Need(independentShared==result.sharedBoundaryVertices&&!independentShared.empty(),"Seam incidence mismatch");
    std::size_t unchangedFar{},changedForest{},changedRoad{},strongRoad{};
    for(std::size_t i=0;i<source.vertices.size();++i){const auto& a=source.vertices[i];
        Need(VertexSame(a,preserved.vertices[i]),"Input was mutated");
        const auto& forest=result.forest.vertices[i];const auto& road=result.road.vertices[i];
        for(const auto* v:{&forest,&road}){
            Need(GeometrySame(a,*v)&&Same(a.v,v->v),"Geometry, color, normal or rock changed");
            Need(std::isfinite(v->u)&&std::isfinite(v->v)&&v->u>=0.f&&v->v>=0.f&&v->u+v->v<=1.f,"Coverage invalid");
        }
        Need(VertexSame(forest,repeat.forest.vertices[i])&&VertexSame(road,repeat.road.vertices[i]),"Repeated vertex differs");
        if(!Same(a.u,forest.u))++changedForest;
        if(!Same(a.u,road.u))++changedRoad;
        if(membership[i]==3u)Need(Same(forest.u,road.u)&&Same(forest.v,road.v)&&
            1.f-forest.u-forest.v<=EntranceRoadDomainCoverageMargin+0.0000001f,
            "Shared seam has differing weights or excess residual base");
        float distance=std::numeric_limits<float>::max();
        for(const auto index:independentShared){const auto& s=source.vertices[index];
            distance=(std::min)(distance,std::hypot(a.x-s.x,a.z-s.z));}
        if(distance>=EntranceRoadDomainFadeDistance){
            Need(VertexSame(a,forest),"Forest changed beyond local seam fade");++unchangedFar;
        }
        if((membership[i]&2u)!=0&&std::abs(a.x)<.20f&&a.z>0.f&&a.z<20.f&&distance>=.75f){
            Need(road.u+road.v<.30f,"Road interior did not reveal distinct base material");++strongRoad;
        }
        if(a.z>=32.f||std::abs(a.x)>=20.f)Need(VertexSame(a,forest),"Bridge, river or distant floor altered");
    }
    Need(unchangedFar>40000&&changedForest>100&&changedRoad>100&&strongRoad>20,"Domain change incomplete or not localized");
    const auto& r=result.receipt;
    Need(r.sourceTriangles==96000&&r.roadTriangles>500&&r.forestTriangles+r.roadTriangles==r.sourceTriangles&&
        r.sharedVertices==independentShared.size()&&r.forestChangedVertices==changedForest&&r.roadChangedVertices==changedRoad,
        "Receipt counts mismatch");
    Need(r.sourceFingerprint==repeat.receipt.sourceFingerprint&&r.forestFingerprint==repeat.receipt.forestFingerprint&&
        r.roadFingerprint==repeat.receipt.roadFingerprint&&result.forest.indices==repeat.forest.indices&&
        result.road.indices==repeat.road.indices,"Deterministic partition differs");
    unsigned rejected{};
    const auto reject=[&](Mesh3D bad,const char* reason){const auto copy=bad;bool caught=false;
        try{(void)CreateEntranceRoadDomain(bad);}catch(const std::invalid_argument& e){
            Need(std::string_view(e.what())==reason,"Unexpected negative-control failure");caught=true;}
        Need(caught&&bad.indices==copy.indices&&bad.vertices.size()==copy.vertices.size(),"Invalid source accepted or changed");
        for(std::size_t i=0;i<bad.vertices.size();++i)Need(VertexSame(bad.vertices[i],copy.vertices[i]),"Rejected input changed");
        ++rejected;
    };
    auto bad=source;bad.indices.pop_back();reject(bad,"ENTRANCE_ROAD_INVALID_MESH");
    bad=source;bad.indices.back()=std::uint32_t(bad.vertices.size());reject(bad,"ENTRANCE_ROAD_INVALID_MESH");
    bad=source;bad.vertices.back().x=std::numeric_limits<float>::quiet_NaN();reject(bad,"ENTRANCE_ROAD_INVALID_VERTEX");
    bad=source;bad.vertices.back().u=.9f;bad.vertices.back().v=.2f;reject(bad,"ENTRANCE_ROAD_INVALID_COVERAGE");
    bad=source;bad.vertices.back().u=std::numeric_limits<float>::infinity();reject(bad,"ENTRANCE_ROAD_INVALID_COVERAGE");
    bad=source;bad.indices[1]=bad.indices[0];reject(bad,"ENTRANCE_ROAD_UNSUPPORTED_TRIANGLE");
    bad=source;bad.indices.insert(bad.indices.end(),source.indices.begin(),source.indices.begin()+3);
    reject(bad,"ENTRANCE_ROAD_DUPLICATE_TRIANGLE");
    bad=source;bad.indices=result.forest.indices;reject(bad,"ENTRANCE_ROAD_BOTH_DOMAINS_REQUIRED");
    const auto forestMaterial=HabitatGroundDefinition(true,true);
    const auto roadMaterial=EntranceRoadMaterialDefinition(forestMaterial);
    const auto canonicalRoad=GroundMaterialDefinition(false,true);
    const auto samePins=[](const auto& a,const auto& b){
        for(unsigned i=0;i<3;++i){
            if(a[i].expectedBytes!=b[i].expectedBytes||a[i].expectedSha256!=b[i].expectedSha256)return false;
        }
        return true;
    };
    const auto& textures=roadMaterial.textures;const auto& original=forestMaterial.textures;
    Need(textures.baseColorPath==canonicalRoad.textures.baseColorPath&&textures.roughnessPath==canonicalRoad.textures.roughnessPath&&
        textures.normalPath==canonicalRoad.textures.normalPath&&samePins(textures.integrity,canonicalRoad.textures.integrity)&&
        textures.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrm,"Road base material channels or encoding differ");
    for(unsigned i=0;i<2;++i){const auto& a=textures.additionalLayers[i];const auto& b=original.additionalLayers[i];
        Need(a.baseColorPath==b.baseColorPath&&a.roughnessPath==b.roughnessPath&&a.normalPath==b.normalPath&&
            a.surfaceEncoding==b.surfaceEncoding&&samePins(a.integrity,b.integrity),"Shared leaf or rock material changed");}
    Need(textures.maximumResolution==original.maximumResolution&&textures.alphaMode==original.alphaMode&&
        Same(textures.alphaCutoff,original.alphaCutoff),"Material sampling parameters changed");
    const auto& a=roadMaterial.material;const auto& b=forestMaterial.material;
    const std::array<float,13> roadParameters{a.tint.r,a.tint.g,a.tint.b,a.tint.a,a.surface.roughness,a.surface.metallic,
        a.surface.emissive,a.surface.occlusion,a.textureScale,a.normalScale,a.baseTextureScale,a.detailTextureScale,a.alphaCutoff};
    const std::array<float,13> forestParameters{b.tint.r,b.tint.g,b.tint.b,b.tint.a,b.surface.roughness,b.surface.metallic,
        b.surface.emissive,b.surface.occlusion,b.textureScale,b.normalScale,b.baseTextureScale,b.detailTextureScale,b.alphaCutoff};
    for(unsigned i=0;i<roadParameters.size();++i)Need(Same(roadParameters[i],forestParameters[i]),"Material numeric parameter changed");
    Need(a.materialClass==b.materialClass&&a.shadingModel==b.shadingModel&&a.alphaMode==b.alphaMode&&
        a.useUv==b.useUv&&a.castsShadow==b.castsShadow&&a.doubleSided==b.doubleSided,"Material mode changed");
    bool rejectedMaterial=false;
    try{(void)EntranceRoadMaterialDefinition(GroundMaterialDefinition());}catch(const std::invalid_argument&){rejectedMaterial=true;}
    Need(rejectedMaterial&&Q::ContextFingerprint(context)==contextSeal,"Material accepted unknown source or original world changed");
    std::cout<<"ROAD_EXPECTED entrance_road_source_fingerprint="<<r.sourceFingerprint<<'\n'
        <<"ROAD_EXPECTED entrance_road_forest_fingerprint="<<r.forestFingerprint<<'\n'
        <<"ROAD_EXPECTED entrance_road_road_fingerprint="<<r.roadFingerprint<<'\n'
        <<"ROAD_EXPECTED entrance_road_source_triangles="<<r.sourceTriangles<<'\n'
        <<"ROAD_EXPECTED entrance_road_forest_triangles="<<r.forestTriangles<<'\n'
        <<"ROAD_EXPECTED entrance_road_road_triangles="<<r.roadTriangles<<'\n'
        <<"ROAD_EXPECTED entrance_road_shared_vertices="<<r.sharedVertices<<'\n';
    std::cout<<"ROAD_DOMAIN_CPU_PASS profile="<<EntranceRoadDomainProfile<<" source_triangles="<<r.sourceTriangles
        <<" forest_triangles="<<r.forestTriangles<<" road_triangles="<<r.roadTriangles<<" shared_vertices="<<r.sharedVertices
        <<" forest_changed="<<r.forestChangedVertices<<" road_changed="<<r.roadChangedVertices
        <<" negative_controls="<<rejected<<" source_fingerprint="<<r.sourceFingerprint
        <<" forest_fingerprint="<<r.forestFingerprint<<" road_fingerprint="<<r.roadFingerprint<<" native=0 art=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"ROAD_DOMAIN_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
