#pragma once
#include "MistwatchAuthoredNativeExpected.h"
#include "MistwatchQualificationRuntime.h"
#include "MistwatchShrubArcDensity.h"
#include "MistwatchShrubComposition.h"
#include "MistwatchWorkshopFrame.h"
#include "MistwatchRepairShelterDetail.h"
#include "MistwatchEntranceUnderstory.h"
#include "MistwatchSettlementSurface.h"
#include "MistwatchBoulderDetail.h"
#include "MistwatchForestGrassShape.h"
#include "MistwatchLandscapeStandFill.h"
#include "MistwatchPlayableArtProfile.h"

namespace Mistwatch::CurrentWorldExpected {
namespace Q=::Mistwatch::AuthoredQualification;
namespace N=::Mistwatch::AuthoredNativeExpected;
namespace W=::Mistwatch::FallenWorldSeat;
using Contract=N::Contract;
inline constexpr const char* WorldCohort="entrance-current-forest-surface-v1";
inline constexpr const char* RuntimeProfile="authored-current-forest-full-v5";
inline constexpr std::array<const char*,16> ProjectedKeys{{
    "shrubs_plants","shrubs_fingerprint","shrubs_author_instance_triangles","shrubs_render_instance_triangles","shrubs_submitted",
    "planked_workshop_profile","planked_workshop_vertices","planked_workshop_indices","planked_workshop_uv_fingerprint",
    "ground_art_surface_path","ground_art_surface_encoding","ground_packed_orm","ground_habitat_layer_surface_path",
    "ground_habitat_layer_surface_encoding","ground_loaded_texture_identity","ground_habitat_layer_source"}};
struct Result {
    Contract legacy,fields,extras,surface,artProfiles;
    std::vector<unsigned> suppressedGrass,suppressedBroadleaf;
};
namespace Detail {
inline void Need(bool value,const char* reason){Q::Detail::Need(value,reason);}
template<class Values>inline std::string Csv(const Values& values){std::ostringstream out;bool first=true;
    for(auto value:values){if(!first)out<<',';first=false;out<<value;}return out.str();}
inline void Field(Contract& out,const std::string& key,std::string value){Need(out.emplace(key,std::move(value)).second,"CURRENT_EXPECTED_DUPLICATE_KEY");}
template<class T>inline void Number(Contract& out,const std::string& key,T value){Field(out,key,std::to_string(value));}
inline void Replace(Contract& out,const char* key,std::string value){const auto found=out.find(key);
    Need(found!=out.end(),"CURRENT_EXPECTED_PROJECTED_KEY_MISSING");found->second=std::move(value);}
template<class T>inline void ReplaceNumber(Contract& out,const char* key,T value){Replace(out,key,std::to_string(value));}
inline std::uint64_t Placement(std::span<const ShrubPlant> plants){std::uint64_t h=14695981039346656037ull;
    for(const auto& p:plants){Need(p.variant<4&&std::isfinite(p.radius)&&p.radius>0,"CURRENT_EXPECTED_SHRUB_DOMAIN");
        for(float f:{p.pose.position.x,p.pose.position.y,p.pose.position.z,p.pose.rotation.y,p.pose.scale.x,p.radius}){
            Need(std::isfinite(f),"CURRENT_EXPECTED_NONFINITE");h^=std::bit_cast<std::uint32_t>(f);h*=1099511628211ull;}
        h^=p.variant;h*=1099511628211ull;}return h;}
inline std::uint64_t DomainMesh(const Aevrynd::Mesh3D& mesh){(void)Q::Detail::MeshIdentity(mesh);Q::Detail::Hash h;
    h.Word(static_cast<unsigned>(mesh.vertices.size()));h.Word(static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){h.Float(v.x);h.Float(v.y);h.Float(v.z);h.Word(v.normal);h.Word(v.color);h.Float(v.u);h.Float(v.v);}
    for(auto index:mesh.indices)h.Word(index);return h.value;}
inline std::uint64_t WorkshopMesh(const Aevrynd::Mesh3D& mesh){(void)Q::Detail::MeshIdentity(mesh);Q::Detail::Hash h;
    h.Word(static_cast<unsigned>(mesh.vertices.size()));h.Word(static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})h.Float(f);h.Word(v.normal);h.Word(v.color);}
    for(auto index:mesh.indices)h.Word(index);return h.value;}
inline Aevrynd::Mesh3D PinnedMesh(const std::filesystem::path& root,const std::string& relative){
    namespace R=::Mistwatch::QualificationRuntime;const auto entries=R::Expected(R::Profile::AuthoredCurrentForestFullV5);
    const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto* e){return e->path==relative;});
    Need(found!=entries.end()&&(*found)->kind==R::Kind::Copy,"CURRENT_EXPECTED_REVIEWED_MESH_PATH");
    const auto& e=**found;const std::string path(e.path),sha(e.sha256);
    Need(e.bytes<=(std::numeric_limits<std::size_t>::max)(),"CURRENT_EXPECTED_MESH_BYTES");
    const Q::AssetPin pin{path.c_str(),static_cast<std::size_t>(e.bytes),sha.c_str()};return Q::Detail::LoadPinned(root/path,pin);
}
inline void Ids(std::span<const unsigned> ids,unsigned limit){unsigned prior{};bool first=true;
    for(unsigned id:ids){Need(id<limit&&(first||id>prior),"CURRENT_EXPECTED_SUPPRESSION_IDS");prior=id;first=false;}}
inline void ValidateSuppression(const EntranceUnderstoryResult& under,const ForestCoverResult& grass,const RoadsideCoverResult& broadleaf){
    Ids(under.suppressedGrassIds,static_cast<unsigned>(grass.plants.size()));Ids(under.suppressedBroadleafIds,static_cast<unsigned>(broadleaf.plants.size()));
    std::vector<unsigned> g,b;const auto suppressed=[&](const ForestCoverPlant& p,float wind){
        for(const auto& s:under.plants)if(std::hypot(s.pose.position.x-p.transform.position.x,s.pose.position.z-p.transform.position.z)<
            (std::max)(.22f,.65f*(s.radius+p.radius+wind)))return true;return false;};
    for(unsigned i=0;i<grass.plants.size();++i)if(suppressed(grass.plants[i],.19f))g.push_back(i);
    for(unsigned i=0;i<broadleaf.plants.size();++i)if(suppressed(broadleaf.plants[i],0.f))b.push_back(i);
    Need(g==under.suppressedGrassIds&&b==under.suppressedBroadleafIds,"CURRENT_EXPECTED_EXACT_SUPPRESSION");
    std::uint64_t h=14695981039346656037ull;const auto mix=[&](unsigned v){h^=v;h*=1099511628211ull;};
    mix(0x47524153u);mix(static_cast<unsigned>(g.size()));for(auto id:g)mix(id);
    mix(0x42524f41u);mix(static_cast<unsigned>(b.size()));for(auto id:b)mix(id);
    Need(h==under.suppressionFingerprint,"CURRENT_EXPECTED_SUPPRESSION_FINGERPRINT");
}
inline void Ground(Result& out,const Q::Context& c,const Aevrynd::HeightField3D& terrain,W::Budget& budget,const std::filesystem::path& root){
    using namespace Aevrynd;auto source=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    std::vector<Vec3> roots;for(const auto& p:c.canopy.plants)roots.push_back(p.pose.position);
    const auto habitat=ApplyForestHabitatFloor(source,terrain,{c.author.firs.roots,roots});
    Need(N::Detail::Mesh(source)==habitat.fingerprint,"CURRENT_EXPECTED_GROUND_SOURCE");
    const auto entrance=CreateEntranceRoadDomain(source);const auto settlement=SettlementGround::Create(source,entrance);
    const auto split=SettlementSurface::Create(source,entrance,settlement);
    const std::array<const Mesh3D*,3> meshes{&split.forest,&split.entrance,&split.gravel};
    Need(split.sourceDomains.size()==source.indices.size()/3,"CURRENT_EXPECTED_GROUND_MEMBERSHIP");
    std::array<std::size_t,3> cursors{};
    for(std::size_t t=0;t<split.sourceDomains.size();++t){budget.Op();const auto domain=static_cast<unsigned>(split.sourceDomains[t]);
        Need(domain<3,"CURRENT_EXPECTED_GROUND_DOMAIN");const auto& mesh=*meshes[domain];
        for(unsigned corner=0;corner<3;++corner){Need(cursors[domain]<mesh.indices.size()&&mesh.indices[cursors[domain]++]==source.indices[t*3+corner],
            "CURRENT_EXPECTED_GROUND_INDEX_PARTITION");}}
    for(unsigned i=0;i<3;++i){Need(cursors[i]==meshes[i]->indices.size(),"CURRENT_EXPECTED_GROUND_INDEX_PARTITION");
        Need(source.vertices.size()==meshes[i]->vertices.size(),"CURRENT_EXPECTED_GROUND_VERTEX_MEMBERSHIP");
        for(std::size_t v=0;v<source.vertices.size();++v){const auto& a=source.vertices[v];const auto& b=meshes[i]->vertices[v];
            Need(Q::Detail::Same(a.x,b.x)&&Q::Detail::Same(a.y,b.y)&&Q::Detail::Same(a.z,b.z)&&a.color==b.color&&a.normal==b.normal,
                "CURRENT_EXPECTED_GROUND_GEOMETRY_CHANGED");}
        Need(DomainMesh(*meshes[i])==split.fingerprints[i],"CURRENT_EXPECTED_GROUND_FINGERPRINT");}
    const auto original=HabitatGroundDefinition(true,true),surface=N::ForestSurfaceDefinition();
    const auto road=EntranceRoadMaterialDefinition(original),gravel=SettlementSurface::MaterialDefinition(original);
    const auto forestId=N::Detail::MaterialSources(surface,root/"mistwatch-materials"),roadId=N::Detail::MaterialSources(road,root/"mistwatch-materials"),
        gravelId=N::Detail::MaterialSources(gravel,root/"mistwatch-materials");
    Need(forestId!=roadId&&forestId!=gravelId&&roadId!=gravelId,"CURRENT_EXPECTED_DISTINCT_MATERIAL_DOMAINS");
    const auto& receipt=settlement.partition.receipt;auto& e=out.extras;
    Field(e,"entrance_road_profile","entrance-road-material-domain-v1");
    Number(e,"entrance_road_source_fingerprint",receipt.sourceFingerprint);Number(e,"entrance_road_forest_fingerprint",receipt.forestFingerprint);
    Number(e,"entrance_road_road_fingerprint",receipt.roadFingerprint);Number(e,"entrance_road_source_triangles",receipt.sourceTriangles);
    Number(e,"entrance_road_forest_triangles",receipt.forestTriangles);Number(e,"entrance_road_road_triangles",receipt.roadTriangles);
    Number(e,"entrance_road_shared_vertices",receipt.sharedVertices);Number(e,"entrance_road_submitted",3);Number(e,"entrance_road_materials",2);
    Number(e,"entrance_road_texture_identity",roadId);Field(e,"entrance_road_base_path",road.textures.baseColorPath.generic_string());
    Field(e,"entrance_road_surface_path",road.textures.roughnessPath.generic_string());Field(e,"entrance_road_normal_path",road.textures.normalPath.generic_string());
    Number(e,"entrance_road_surface_encoding",static_cast<unsigned>(road.textures.surfaceEncoding));
    Number(e,"entrance_road_original_terrain_drawn",0);
    Field(e,"settlement_ground_profile","workshop-ground-domain-v1");Number(e,"settlement_ground_moved_triangles",settlement.movedSourceTriangles.size());
    Number(e,"settlement_ground_boundary_vertices",settlement.newBoundaryVertices.size());Number(e,"settlement_ground_changed_forest",settlement.changedForestVertices);
    Number(e,"settlement_ground_changed_road",settlement.changedRoadVertices);Number(e,"settlement_ground_original_forest_fingerprint",entrance.receipt.forestFingerprint);
    Number(e,"settlement_ground_original_road_fingerprint",entrance.receipt.roadFingerprint);
    Field(e,"settlement_surface_profile","workshop-gravel-surface-candidate-v1");Field(e,"entrance_road_receipt_scope","combined-source-before-material-split");
    Number(e,"settlement_surface_submitted",3);Number(e,"settlement_surface_material_identity",gravelId);
    Field(e,"settlement_surface_material_occlusion",N::Detail::Scalar(gravel.material.surface.occlusion));
    Field(e,"settlement_surface_base_path",gravel.textures.baseColorPath.generic_string());Field(e,"settlement_surface_orm_path",gravel.textures.roughnessPath.generic_string());
    Field(e,"settlement_surface_normal_path",gravel.textures.normalPath.generic_string());
    for(unsigned i=0;i<3;++i){const auto stem="settlement_surface_mesh_"+std::to_string(i);
        Number(e,stem+"_fingerprint",DomainMesh(*meshes[i]));Number(e,stem+"_triangles",meshes[i]->indices.size()/3);
        Number(e,"settlement_surface_channel_"+std::to_string(i)+"_bytes",gravel.textures.integrity[i].expectedBytes);
        Field(e,"settlement_surface_channel_"+std::to_string(i)+"_sha256",gravel.textures.integrity[i].expectedSha256);}
    Need(meshes[0]->indices.size()+meshes[1]->indices.size()+meshes[2]->indices.size()==source.indices.size(),"CURRENT_EXPECTED_GROUND_TRIANGLE_CONSERVATION");
}
inline void Structures(Result& out){auto& e=out.extras;const auto mesh=WorkshopFrame::Create();
    Need(mesh.vertices.size()==12480&&mesh.indices.size()==17160,"CURRENT_EXPECTED_WORKSHOP_TOPOLOGY");
    Need(WorkshopMesh(mesh)==WorkshopFrame::Fingerprint(mesh),"CURRENT_EXPECTED_WORKSHOP_FINGERPRINT");
    Replace(out.fields,"planked_workshop_profile","framed-workshop-v1");ReplaceNumber(out.fields,"planked_workshop_vertices",mesh.vertices.size());
    ReplaceNumber(out.fields,"planked_workshop_indices",mesh.indices.size());ReplaceNumber(out.fields,"planked_workshop_uv_fingerprint",N::Detail::Uv(mesh));
    Field(e,"workshop_frame_profile","framed-workshop-v1");Number(e,"workshop_frame_members",54);Number(e,"workshop_frame_fingerprint",WorkshopMesh(mesh));
    const auto shelter=ShelterDetail::Create();Field(e,"shelter_detail_profile","framed-corrugated-v1");
    Number(e,"shelter_detail_wood_vertices",shelter.wood.vertices.size());Number(e,"shelter_detail_wood_indices",shelter.wood.indices.size());
    Number(e,"shelter_detail_wood_fingerprint",shelter.woodFingerprint);Number(e,"shelter_detail_roof_vertices",shelter.roof.vertices.size());
    Number(e,"shelter_detail_roof_indices",shelter.roof.indices.size());Number(e,"shelter_detail_roof_fingerprint",shelter.roofFingerprint);
    Number(e,"shelter_detail_frame_members",shelter.frameMembers);Number(e,"shelter_detail_corrugations",shelter.corrugations);
    Field(e,"shelter_detail_roof_material","procedural-weathered-metal");
}
inline Contract ArtProfiles(){return {{"Shelter","framed-corrugated-v1"},{"Workshop","framed-workshop-v1"},{"Road","entrance-road-material-domain-v1"},
    {"Roadside","roadside-broadleaf-v1"},{"Understory","entrance-understory-v2"},{"BoulderColor","source-srgb-v1"},{"BoulderGeometry","lod0-source-v1"},
    {"Shrubs","woodland-feasible-arc-v3"},{"LandscapeTwig","crown-distance-lod-v1"},{"LandscapeStand","landscape-stand-fill-v2"}};}
inline void Landscape(Result& out,const Aevrynd::HeightField3D& terrain,const std::filesystem::path& root){
    std::array<Aevrynd::Mesh3D,5> meshes;constexpr std::array<const char*,4> slots{"bark","twig","dead-branches","trunk-c"};
    Aevrynd::Vec3 low{10000,10000,10000},high{-10000,-10000,-10000};
    for(unsigned slot=0;slot<5;++slot){const bool nearTwig=slot==4;const auto id=nearTwig?1u:slot;
        const auto name=std::string("mistwatch-forest/meshes/fir-tree-01-c-lod")+(nearTwig?"1":"2")+
            "-node0-mesh0-primitive"+std::to_string(id)+"-material"+std::to_string(id)+"-fir-tree-01-"+slots[id]+".aemesh";
        meshes[slot]=PinnedMesh(root,name);for(const auto& v:meshes[slot].vertices){
            low.x=(std::min)(low.x,v.x);low.y=(std::min)(low.y,v.y);low.z=(std::min)(low.z,v.z);
            high.x=(std::max)(high.x,v.x);high.y=(std::max)(high.y,v.y);high.z=(std::max)(high.z,v.z);}}
    const auto extents=(high-low)*.5f;const Aevrynd::MeshBounds3D bounds{(high+low)*.5f,Aevrynd::Length(extents),extents};
    const auto original=LandscapeTrees(terrain);const auto plan=LandscapeStandFill::Create(terrain,original,bounds);
    Need(plan.complete&&plan.combined.size()==616&&plan.addedBands.size()==96,"CURRENT_EXPECTED_LANDSCAPE_QUOTA");
    for(unsigned i=0;i<520;++i)Need(Q::Detail::Same(plan.combined[i],original[i]),"CURRENT_EXPECTED_LANDSCAPE_PREFIX");
    auto& e=out.extras;Field(e,"landscape_twig_profile","crown-distance-lod-v1");Field(e,"landscape_stand_profile","landscape-stand-fill-v2");
    Number(e,"landscape_stand_base_population",original.size());Number(e,"landscape_stand_added_population",plan.addedBands.size());
    Number(e,"landscape_stand_pose_fingerprint",plan.fingerprint);Number(e,"landscape_stand_source_fingerprint",plan.sourceFingerprint);
    Number(e,"landscape_twig_near_triangles",meshes[4].indices.size()/3);Number(e,"landscape_twig_far_triangles",meshes[1].indices.size()/3);
    Number(e,"landscape_twig_near_fingerprint",ForestGrassShapeFingerprint(meshes[4]));Number(e,"landscape_twig_far_fingerprint",ForestGrassShapeFingerprint(meshes[1]));
}
}
inline void ValidateProjection(const Contract& fields,const Contract& legacy){
    Detail::Need(fields.size()==153&&legacy.size()==153,"CURRENT_EXPECTED_153_KEYS");
    for(const auto& [key,value]:legacy){const auto found=fields.find(key);Detail::Need(found!=fields.end(),"CURRENT_EXPECTED_PROJECTION_MISSING");
        const bool changed=std::find(ProjectedKeys.begin(),ProjectedKeys.end(),key)!=ProjectedKeys.end();
        Detail::Need(changed||found->second==value,"CURRENT_EXPECTED_UNREVIEWED_PROJECTION");}
}
inline void ValidateFields(const Result& actual,const Result& expected){
    ValidateProjection(actual.fields,actual.legacy);N::ValidateFields(actual.legacy,expected.legacy);N::ValidateFields(actual.fields,expected.fields);
    N::ValidateFields(actual.extras,expected.extras);N::ValidateFields(actual.surface,expected.surface);N::ValidateFields(actual.artProfiles,expected.artProfiles);
    Detail::Need(actual.suppressedGrass==expected.suppressedGrass&&actual.suppressedBroadleaf==expected.suppressedBroadleaf,"CURRENT_EXPECTED_SUPPRESSION_IDS");
}
inline Result Build(const Q::Context& c,const Aevrynd::HeightField3D& terrain,const Q::Assets& assets,W::Budget& budget,const std::filesystem::path& root){
    using namespace Detail;Need(root.is_absolute(),"CURRENT_EXPECTED_ABSOLUTE_RUNTIME");Result out;
    // 固定 64 株、八樹及 8853/8854 固體的舊資格完整保留，不能將 candidate 回塞舊 Context。
    out.legacy=N::Build(c,terrain,assets,budget,root);Need(out.legacy.size()==153,"CURRENT_EXPECTED_153_KEYS");out.fields=out.legacy;
    out.surface=N::BuildForestSurface(root);out.artProfiles=ArtProfiles();
    const auto shrubs=CreateShrubArcDensity(assets.shrubLeaf,terrain,c.vegetation[0],c.author.understory,c.retention.cover,c.colonies,assets.fernAssets,128,c.wood,true);
    const auto composition=ShrubCompositionContract::PreservedPrefix128(c.shrubs,shrubs);composition.Validate(shrubs);
    Need(Placement(shrubs.plants)==shrubs.fingerprint&&Placement(std::span<const ShrubPlant>(shrubs.plants).first(64))==c.shrubs.fingerprint,
        "CURRENT_EXPECTED_SHRUB_FINGERPRINT");
    auto downstream=c;downstream.shrubs=shrubs;Q::ValidateDownstreamVolumes(downstream,terrain,assets,budget);
    for(unsigned state=0;state<2;++state)Q::ValidateTypedWorld(downstream,state);
    std::uint64_t authorTriangles{},renderTriangles{};
    for(const auto& p:shrubs.plants){budget.Op();const auto& v=shrubs.variants[p.variant];const auto crown=CreateShrubOpenCrown(v,p.variant);
        authorTriangles+=(v.stems.indices.size()+v.leaves.indices.size())/3;renderTriangles+=(v.stems.indices.size()+crown.leaves.indices.size())/3;}
    ReplaceNumber(out.fields,"shrubs_plants",shrubs.plants.size());ReplaceNumber(out.fields,"shrubs_fingerprint",Placement(shrubs.plants));
    ReplaceNumber(out.fields,"shrubs_author_instance_triangles",authorTriangles);ReplaceNumber(out.fields,"shrubs_render_instance_triangles",renderTriangles);
    ReplaceNumber(out.fields,"shrubs_submitted",shrubs.plants.size());
    for(const auto* key:{"ground_art_surface_path","ground_art_surface_encoding","ground_habitat_layer_surface_path","ground_habitat_layer_surface_encoding","ground_loaded_texture_identity"})
        Replace(out.fields,key,out.surface.at(key));
    Replace(out.fields,"ground_packed_orm","0");Replace(out.fields,"ground_habitat_layer_source","unrecognized");
    Need(GroundForestWoodArtSource(N::ForestSurfaceDefinition())=="ground037-moss-floor"&&HabitatGroundLayerSource(N::ForestSurfaceDefinition())=="unrecognized",
        "CURRENT_EXPECTED_EXACT_SURFACE_CLASSIFICATION");
    Structures(out);Ground(out,c,terrain,budget,root);auto& e=out.extras;
    Field(e,"shrubs_profile","woodland-feasible-arc-v3");Number(e,"shrubs_target",128);Number(e,"shrubs_preserved_prefix",64);
    const auto before=W::Boxes(c.worlds[0]),after=W::Boxes(c.worlds[1]);std::array<Aevrynd::Mesh3D,2> broadleafMeshes;
    std::array<NettleAssetBounds,2> bounds;
    for(unsigned i=0;i<2;++i){auto& mesh=broadleafMeshes[i];mesh=PinnedMesh(root,"mistwatch-nettle/meshes/"+NettleMeshName(RoadsideBroadleafVariants[i]));
        NormalizeNettleAsset(mesh,RoadsideBroadleafVariants[i]);bounds[i]=MeasureNettleAssetBounds(mesh);
        Number(e,"roadside_broadleaf_mesh_"+std::to_string(i)+"_identity",ForestGrassShapeFingerprint(mesh));
        Number(e,"roadside_broadleaf_mesh_"+std::to_string(i)+"_triangles",mesh.indices.size()/3);}
    const auto broadleaf=CreateRoadsideBroadleaf(terrain,c.retention.cover,c.author.understory,c.colonies,shrubs,assets.grassAssets,assets.fernAssets,bounds,before,after,composition);
    const auto under=CreateEntranceUnderstory(terrain,c.retention.cover,c.author.understory,c.colonies,shrubs,assets.grassAssets,assets.fernAssets,broadleaf,before,after,nullptr,composition);
    Need(broadleaf.plants.size()==96&&broadleaf.left==48&&broadleaf.right==48&&under.plants.size()==24&&under.clusterIds.size()==24,
        "CURRENT_EXPECTED_LAYERED_QUOTAS");ValidateSuppression(under,c.retention.cover,broadleaf);
    out.suppressedGrass=under.suppressedGrassIds;out.suppressedBroadleaf=under.suppressedBroadleafIds;
    std::array<unsigned,5> renderGrass{};std::array<unsigned,2> renderBroadleaf{};
    for(unsigned i=0;i<c.retention.cover.plants.size();++i)if(!std::binary_search(out.suppressedGrass.begin(),out.suppressedGrass.end(),i))
        ++renderGrass[c.retention.cover.plants[i].variant];
    for(unsigned i=0;i<broadleaf.plants.size();++i)if(!std::binary_search(out.suppressedBroadleaf.begin(),out.suppressedBroadleaf.end(),i))
        ++renderBroadleaf[broadleaf.plants[i].variant];
    Field(e,"forest_cover_retained_render_variants",Csv(renderGrass));Field(e,"roadside_broadleaf_retained_variants",Csv(renderBroadleaf));
    Field(e,"roadside_broadleaf_profile","roadside-broadleaf-v1");Number(e,"roadside_broadleaf_population",broadleaf.plants.size());
    Number(e,"roadside_broadleaf_fingerprint",broadleaf.fingerprint);Number(e,"roadside_broadleaf_left",broadleaf.left);Number(e,"roadside_broadleaf_right",broadleaf.right);
    Number(e,"roadside_broadleaf_attempted",broadleaf.attempted);Number(e,"roadside_broadleaf_materials",1);
    Field(e,"entrance_understory_profile","entrance-understory-v2");Number(e,"entrance_understory_population",under.plants.size());
    Number(e,"entrance_understory_fingerprint",under.fingerprint);Number(e,"entrance_understory_attempted",under.attempted);
    Number(e,"entrance_understory_left",under.left);Number(e,"entrance_understory_right",under.right);Number(e,"entrance_understory_clusters",under.clusterCounts.size());
    Number(e,"entrance_understory_suppressed_grass",under.suppressedGrassIds.size());Number(e,"entrance_understory_suppressed_broadleaf",under.suppressedBroadleafIds.size());
    Number(e,"entrance_understory_suppression_fingerprint",under.suppressionFingerprint);Number(e,"entrance_understory_submitted",under.plants.size());
    Number(e,"entrance_understory_available_shared_meshes",8);Field(e,"entrance_understory_cluster_counts",Csv(under.clusterCounts));
    const auto rawBoulder=PinnedMesh(root,std::string("mistwatch-boulders/meshes/")+BoulderDetail::MeshName);auto boulder=rawBoulder;RootBoulderMesh(boulder);
    Need(BoulderDetail::ExactRootedBounds(boulder)&&boulder.vertices.size()==81373&&boulder.indices.size()==198366,"CURRENT_EXPECTED_BOULDER_TOPOLOGY");
    std::size_t boulders{};for(const auto& p:c.author.props)boulders+=!p.FallenLog();Need(boulders==29,"CURRENT_EXPECTED_BOULDER_INSTANCES");
    Number(e,"boulder_instances",boulders);Number(e,"boulder_mesh_vertices",boulder.vertices.size());Number(e,"boulder_mesh_triangles",boulder.indices.size()/3);
    Number(e,"boulder_gpu_resources",1);Field(e,"boulder_geometry_profile","lod0-source-v1");Field(e,"boulder_color_profile","source-srgb-v1");
    Field(e,"boulder_base_path","materials/slot-0-base-source-srgb-v1.dds");Field(e,"boulder_base_sha256","d874d5ac2e2c01ca7bd87970ba23415ef285e051b0a501c74e383e6219758b79");
    Number(e,"canopy_cleanup_mesh_destroy_calls",3+c.canopy.plants.size());Number(e,"landscape_fir_cleanup_mesh_destroy_calls",5);
    Landscape(out,terrain,root);
    Need(out.extras.size()==105,"CURRENT_EXPECTED_EXTRA_KEYS");
    ValidateProjection(out.fields,out.legacy);budget.Time();return out;
}
}
