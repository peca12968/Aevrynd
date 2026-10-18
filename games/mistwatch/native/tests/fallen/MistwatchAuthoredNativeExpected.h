#pragma once
#include "MistwatchAuthoredWorldQualification.h"
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchHabitatLeafMaterial.h"
#include "MistwatchShelterWeatheredUv.h"
#include "MistwatchWorkshopVisual.h"
#include "MistwatchShrubCanopy.h"
#include "MistwatchHutWoodMaterial.h"
#include "MistwatchForestSurfaceProfile.h"
#include <map>
#include <numeric>
#include <string_view>

namespace Mistwatch::AuthoredNativeExpected {
namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
using Contract=std::map<std::string,std::string>;
struct FernSubmission {std::uint64_t plants{},triangles{};std::array<unsigned,4> variants{};};
namespace Detail {
inline void Need(bool value,const char* reason){Q::Detail::Need(value,reason);}
template<class T,std::size_t N>inline std::string Csv(const std::array<T,N>& values){
    std::ostringstream out;for(std::size_t i=0;i<N;++i){if(i)out<<',';out<<values[i];}return out.str();
}
inline void Field(Contract& c,const std::string& key,std::string value){Need(c.emplace(key,std::move(value)).second,"AUTHORED_EXPECTED_DUPLICATE_KEY");}
template<class T>inline void Number(Contract& c,const std::string& key,T value){Field(c,key,std::to_string(value));}
inline void Boolean(Contract& c,const std::string& key,bool value){Field(c,key,value?"1":"0");}
inline std::string Scalar(float value){Need(std::isfinite(value),"AUTHORED_EXPECTED_NONFINITE");std::ostringstream out;out<<value;return out.str();}
inline std::uint64_t Poses(std::span<const W::Pose> poses){Q::Detail::Hash h;
    for(std::size_t i=0;i<poses.size();++i){h.Pose(poses[i]);h.Word(static_cast<unsigned>(i%4));}return h.value;
}
inline std::uint64_t Colonies(std::span<const FernColonyPlant> plants){Q::Detail::Hash h;for(const auto& p:plants){h.Pose(p.transform);h.Word(p.variant);}return h.value;}
inline std::uint64_t Uv(const Aevrynd::Mesh3D& mesh){(void)Q::Detail::MeshIdentity(mesh);Q::Detail::Hash h;
    for(const auto& v:mesh.vertices){h.Float(v.u);h.Float(v.v);}return h.value;
}
// 不依靠 production 收據雜湊：逐位元重算完整地表內容與不含 UV 的幾何。
inline std::uint64_t Mesh(const Aevrynd::Mesh3D& mesh,bool uv=true){(void)Q::Detail::MeshIdentity(mesh);Q::Detail::Hash h;
    for(const auto& v:mesh.vertices){h.Float(v.x);h.Float(v.y);h.Float(v.z);if(uv){h.Float(v.u);h.Float(v.v);}h.Word(v.color);h.Word(v.normal);}
    for(auto i:mesh.indices)h.Word(i);return h.value;
}
inline void SameGeometry(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b){
    Need(a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"AUTHORED_EXPECTED_GEOMETRY_TOPOLOGY");
    for(std::size_t i=0;i<a.vertices.size();++i){const auto& x=a.vertices[i];const auto& y=b.vertices[i];
        Need(Q::Detail::Same(x.x,y.x)&&Q::Detail::Same(x.y,y.y)&&Q::Detail::Same(x.z,y.z)&&x.color==y.color&&x.normal==y.normal,
            "AUTHORED_EXPECTED_GEOMETRY_CHANGED");}
}
inline std::string Sha(std::span<const std::byte> bytes){std::array<std::byte,32> digest{};
    Need(Aevrynd::WorldDeltaSha2563D(bytes,digest),"AUTHORED_EXPECTED_SOURCE_SHA_FAILED");
    constexpr char hex[]="0123456789abcdef";std::string out;out.reserve(64);
    for(auto b:digest){const auto v=std::to_integer<unsigned>(b);out+=hex[v>>4];out+=hex[v&15];}return out;
}
inline void VerifyMap(const std::filesystem::path& root,const std::filesystem::path& relative,const Aevrynd::MaterialTextureIntegrity3D& pin){
    Need(root.is_absolute()&&!relative.empty()&&!relative.is_absolute(),"AUTHORED_EXPECTED_ASSET_ROOT");
    for(const auto& part:relative)Need(part!="..","AUTHORED_EXPECTED_SOURCE_PATH");
    Need(pin.expectedBytes>0&&pin.expectedBytes<=33554432&&pin.expectedSha256.size()==64,"AUTHORED_EXPECTED_SOURCE_PIN");
    const auto path=root/relative;Need(std::filesystem::file_size(path)==pin.expectedBytes,"AUTHORED_EXPECTED_SOURCE_BYTES");
    std::ifstream stream(path,std::ios::binary);Need(bool(stream),"AUTHORED_EXPECTED_SOURCE_OPEN");
    std::vector<std::byte> raw(static_cast<std::size_t>(pin.expectedBytes));stream.read(reinterpret_cast<char*>(raw.data()),static_cast<std::streamsize>(raw.size()));
    Need(stream.gcount()==static_cast<std::streamsize>(raw.size()),"AUTHORED_EXPECTED_SOURCE_READ");
    Need(Sha(raw)==pin.expectedSha256,"AUTHORED_EXPECTED_SOURCE_SHA");
}
// Renderer 的封印語義身分：只採真 bytes/SHA 與編碼，不含檔案路徑或 GPU handle。
inline std::uint64_t MaterialSources(const Aevrynd::MaterialAssetDefinition3D& definition,
    const std::filesystem::path& root){
    const auto& m=definition.material;
    for(float value:{m.tint.r,m.tint.g,m.tint.b,m.tint.a,m.surface.roughness,m.surface.metallic,m.surface.emissive,m.surface.occlusion,
        m.textureScale,m.normalScale,m.baseTextureScale,m.detailTextureScale,m.alphaCutoff})
        Need(std::isfinite(value),"AUTHORED_EXPECTED_NONFINITE");
    const auto& t=definition.textures;
    const std::array<std::filesystem::path,9> paths{t.baseColorPath,t.roughnessPath,t.normalPath,
        t.additionalLayers[0].baseColorPath,t.additionalLayers[0].roughnessPath,t.additionalLayers[0].normalPath,
        t.additionalLayers[1].baseColorPath,t.additionalLayers[1].roughnessPath,t.additionalLayers[1].normalPath};
    const std::array<Aevrynd::MaterialTextureIntegrity3D,9> pins{t.integrity[0],t.integrity[1],t.integrity[2],
        t.additionalLayers[0].integrity[0],t.additionalLayers[0].integrity[1],t.additionalLayers[0].integrity[2],
        t.additionalLayers[1].integrity[0],t.additionalLayers[1].integrity[1],t.additionalLayers[1].integrity[2]};
    Need(t.maximumResolution>0&&t.maximumResolution<=4096&&t.alphaMode==Aevrynd::MaterialAlphaMode3D::Opaque,
        "AUTHORED_EXPECTED_GROUND_RECIPE");
    Q::Detail::Hash hash;hash.value=1469598103934665603ull;
    hash.Wide(9);hash.Word(t.maximumResolution);hash.Word(static_cast<unsigned>(t.alphaMode));hash.Word(static_cast<unsigned>(t.surfaceEncoding));
    for(const auto& l:t.additionalLayers)hash.Word(static_cast<unsigned>(l.surfaceEncoding));
    for(unsigned i=0;i<9;++i){VerifyMap(root,paths[i],pins[i]);
        hash.Wide(i);hash.Wide(pins[i].expectedBytes);for(char raw:pins[i].expectedSha256){const auto byte=static_cast<unsigned char>(raw);hash.value^=byte;hash.value*=1099511628211ull;}}
    return hash.value?hash.value:1;
}
inline void Habitat(Contract& out,const Q::Context& c,const Aevrynd::HeightField3D& terrain,W::Budget& budget){
    std::vector<W::V> canopyRoots;for(const auto& p:c.canopy.plants)canopyRoots.push_back(p.pose.position);
    const ForestHabitatRoots roots{c.author.firs.roots,canopyRoots};
    auto floor=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    auto landscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    const auto floorOriginal=floor,landscapeOriginal=landscape;
    const auto floorReceipt=ApplyForestHabitatFloor(floor,terrain,roots);
    const auto landscapeReceipt=ApplyForestHabitatFloor(landscape,terrain,roots,true);
    SameGeometry(floorOriginal,floor);SameGeometry(landscapeOriginal,landscape);
    Need(Mesh(floor)==floorReceipt.fingerprint&&Mesh(landscape)==landscapeReceipt.fingerprint,"AUTHORED_EXPECTED_HABITAT_HASH");
    for(unsigned group=0;group<2;++group){const auto& before=group?landscapeOriginal:floorOriginal;const auto& after=group?landscape:floor;
        for(std::size_t i=0;i<before.vertices.size();++i){budget.Op();const auto& a=before.vertices[i];const auto& b=after.vertices[i];
            Need(Q::Detail::Same(a.v,b.v),"AUTHORED_EXPECTED_HABITAT_ROCK_CHANGED");
            if((a.z>32.f&&a.z<40.f)||std::abs(a.x)>=50.f||a.z<=-20.f||a.z>=100.f)
                Need(Q::Detail::Same(a.u,b.u),"AUTHORED_EXPECTED_HABITAT_BOUNDARY_CHANGED");}}
    Field(out,"ground_habitat_revision","root-soil-and-mission-spurs-v1");Number(out,"ground_habitat_fir_roots",roots.firs.size());
    Number(out,"ground_habitat_canopy_roots",roots.canopy.size());Number(out,"ground_habitat_terrain_fingerprint",Mesh(floor));
    Number(out,"ground_habitat_landscape_fingerprint",Mesh(landscape));Number(out,"ground_habitat_terrain_changed",floorReceipt.changed);
    Number(out,"ground_habitat_terrain_soil_dominant",floorReceipt.soilDominant);Number(out,"ground_habitat_terrain_moss_dominant",floorReceipt.mossDominant);
    Number(out,"ground_habitat_landscape_changed",landscapeReceipt.changed);
    Number(out,"terrain_vertices",floor.vertices.size());Number(out,"terrain_triangles",floor.indices.size()/3);
}
inline constexpr std::array<Q::AssetPin,3> BroadleafPins{{
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh",163400,"4182C1D2D3E726F5C77DD84ADF59EA5D4E022D49B1F211B34B488DBC165B68CC"},
    Q::ShrubPin,
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh",57760,"1C765173EBC17A3BE167C35EF1B8DF8329FB4CF98F468F4DA13FD42533B677FD"}}};
inline void PinnedAssets(const Q::Assets& assets,const std::filesystem::path& root){
    const auto pinned=Q::LoadAssets(root);Need(pinned.fingerprint==assets.fingerprint,"AUTHORED_EXPECTED_PINNED_ASSETS");
    for(unsigned i=0;i<3;++i)Need(Q::Detail::Same(pinned.canopy[i],assets.canopy[i]),"AUTHORED_EXPECTED_PINNED_ASSETS");
    for(unsigned i=0;i<4;++i)Need(Q::Detail::Same(pinned.fern[i],assets.fern[i]),"AUTHORED_EXPECTED_PINNED_ASSETS");
    for(unsigned i=0;i<5;++i)Need(Q::Detail::Same(pinned.grass[i],assets.grass[i]),"AUTHORED_EXPECTED_PINNED_ASSETS");
    Need(Q::Detail::Same(pinned.derivedStem,assets.derivedStem)&&Q::Detail::Same(pinned.shrubLeaf,assets.shrubLeaf),"AUTHORED_EXPECTED_PINNED_ASSETS");
}
inline void CanopyRecipe(const Q::Context& c,const Aevrynd::HeightField3D& terrain,const Q::Assets& assets){
    const auto geometry=MeasureCanopy(assets.canopy);EntranceCanopyMetadata metadata;
    const auto canonical=AppendEntranceCanopy(c.source.canopy,assets.canopy,geometry,assets.derivedStem,terrain,
        c.author.VegetationSolids(false),c.author.understory,assets.fernAssets,metadata);
    Need(canonical.plants.size()==c.canopy.plants.size()&&canonical.fingerprint==c.canopy.fingerprint&&
        canonical.groundChecks==c.canopy.groundChecks&&metadata.originalFingerprint==c.canopyMetadata.originalFingerprint&&
        metadata.faceChecks==c.canopyMetadata.faceChecks&&metadata.exactOverlayFaces==c.canopyMetadata.exactOverlayFaces&&
        metadata.appended==c.canopyMetadata.appended&&metadata.woodParts==c.canopyMetadata.woodParts,"AUTHORED_EXPECTED_CANOPY_RECIPE");
    for(std::size_t i=0;i<canonical.plants.size();++i){const auto& a=canonical.plants[i];const auto& b=c.canopy.plants[i];
        const auto root=PlantDerivedRootStem(assets.derivedStem,geometry,a.pose,terrain);
        Need(Q::Detail::Same(a.pose,b.pose)&&Q::Detail::Same(root,b.rootedTrunk)&&Q::Detail::Same(ExtendRootCollision(a.lower,root,a.pose),b.lower)&&
            Q::Detail::Same(a.crownRadius,b.crownRadius)&&Q::Detail::Same(a.rootRadius,b.rootRadius),"AUTHORED_EXPECTED_CANOPY_ROOT_RECIPE");}
}
inline void StructureSources(const std::filesystem::path& root){
    const auto definition=HutWeatheredWoodDefinition();const auto& t=definition.textures;
    Need(definition.material.useUv&&definition.material.textureScale==1.f&&definition.material.normalScale==.22f,
        "AUTHORED_EXPECTED_STRUCTURE_MATERIAL");
    VerifyMap(root,t.baseColorPath,t.integrity[0]);VerifyMap(root,t.roughnessPath,t.integrity[1]);VerifyMap(root,t.normalPath,t.integrity[2]);
}
inline Aevrynd::Mesh3D OriginalWorkshop(){
    // 與原四個 WorkshopSolids 一一配對；只檢查板化後 UV 以外的內容沒有被候選改動。
    const auto solids=WorkshopSolids();Need(solids.size()==4,"AUTHORED_EXPECTED_WORKSHOP_SOLIDS");
    constexpr std::array<unsigned,4> boards{20,16,20,20},axes{0,2,0,0};Aevrynd::Mesh3D result;
    for(unsigned i=0;i<4;++i){const auto p=Aevrynd::CreatePlankedPanel3D(solids[i].center,solids[i].extents*2.f,axes[i],boards[i],.007f,.004f,i*3u);
        Need(bool(p),"AUTHORED_EXPECTED_WORKSHOP_PANEL");const auto base=static_cast<std::uint32_t>(result.vertices.size());
        result.vertices.insert(result.vertices.end(),p->vertices.begin(),p->vertices.end());for(auto index:p->indices)result.indices.push_back(base+index);}
    return result;
}
inline void InsideSolids(const Aevrynd::Mesh3D& mesh,std::span<const W::Box> solids,std::span<const unsigned> boards){
    Need(solids.size()==boards.size(),"AUTHORED_EXPECTED_STRUCTURE_MEMBERSHIP");std::size_t first{};
    for(std::size_t part=0;part<solids.size();++part){const auto& box=solids[part];const auto count=std::size_t(boards[part])*96;
        Need(first<=mesh.vertices.size()&&count<=mesh.vertices.size()-first,"AUTHORED_EXPECTED_STRUCTURE_MEMBERSHIP");
        for(std::size_t i=first;i<first+count;++i){const auto& v=mesh.vertices[i];
            Need(std::abs(v.x-box.center.x)<=box.extents.x+.00001f&&std::abs(v.y-box.center.y)<=box.extents.y+.00001f&&
                std::abs(v.z-box.center.z)<=box.extents.z+.00001f,"AUTHORED_EXPECTED_STRUCTURE_ESCAPES_ORIGINAL");}first+=count;}
    Need(first==mesh.vertices.size(),"AUTHORED_EXPECTED_STRUCTURE_MEMBERSHIP");
}
}

inline FernSubmission SceneOriginalFerns(const Q::Context& c,const Aevrynd::HeightField3D& terrain,
    const Q::Assets& assets,std::string_view scene){
    Detail::Need(scene=="initial"||scene=="river-overlook","AUTHORED_EXPECTED_FIXED_SCENE");W::VerifyCurrentTerrain(terrain);
    Q::ValidateAuthor(c.author,c.source,terrain);
    Detail::Need(c.author.understory.ferns.size()==420&&Q::AssetsFingerprint(assets)==assets.fingerprint,"AUTHORED_EXPECTED_FERN_INPUT");
    // 固定 capture 不推進角色 Step；ResetController 不改 session.feet 的作者 Y=0。
    const float z=scene=="initial"?0.f:30.f;const W::V feet{0,0,z};FernSubmission out;
    for(std::size_t i=0;i<c.author.understory.ferns.size();++i){const auto& p=c.author.understory.ferns[i];Q::Detail::Hash finite;finite.Pose(p);
        if(UnderstoryVisibility(Aevrynd::Length(p.position-feet))<=0)continue;const auto variant=i%4;
        ++out.plants;++out.variants[variant];out.triangles+=assets.fern[variant].indices.size()/3;}
    return out;
}
inline std::string SceneJson(const FernSubmission& s){return "{\"plants\":\""+std::to_string(s.plants)+"\",\"triangles\":\""+
    std::to_string(s.triangles)+"\",\"variants\":\""+Detail::Csv(s.variants)+"\"}";}

// 完整 default 作者世界；submitted/uploaded 欄只描述待驗證 recipe，不偽造 GPU 實績。
inline Contract Build(const Q::Context& c,const Aevrynd::HeightField3D& terrain,const Q::Assets& assets,
    W::Budget& budget,const std::filesystem::path& runtimeAssets){
    using namespace Detail;Need(runtimeAssets.is_absolute(),"AUTHORED_EXPECTED_ASSET_ROOT");
    Q::ValidateContext(c,terrain,assets,budget);PinnedAssets(assets,runtimeAssets);CanopyRecipe(c,terrain,assets);
    Q::RepeatControls(c,terrain,assets,budget);const auto& cover=c.retention.cover;const auto& author=c.author;
    const ForestCoverSettings coverSettings{};const FernColonySettings colonySettings{};
    Contract out;std::array<unsigned,5> coverVariants{};std::array<unsigned,4> colonyVariants{};
    for(const auto& p:cover.plants){Need(p.variant<5,"AUTHORED_EXPECTED_GRASS_VARIANT");++coverVariants[p.variant];}
    for(const auto& p:c.colonies.plants){Need(p.variant<4,"AUTHORED_EXPECTED_FERN_VARIANT");++colonyVariants[p.variant];}
    std::array<std::uint64_t,4> renderLeaves{},stemIndices{},leafIndices{};std::uint64_t authorTriangles{},renderTriangles{};
    for(unsigned i=0;i<4;++i){const auto crown=CreateShrubOpenCrown(c.shrubs.variants[i],i);(void)Q::Detail::MeshIdentity(crown.stems);
        (void)Q::Detail::MeshIdentity(crown.leaves);renderLeaves[i]=crown.renderLeafCount;stemIndices[i]=crown.stems.indices.size();leafIndices[i]=crown.leaves.indices.size();}
    for(const auto& p:c.shrubs.plants){Need(p.variant<4,"AUTHORED_EXPECTED_SHRUB_VARIANT");
        authorTriangles+=(c.shrubs.variants[p.variant].stems.indices.size()+c.shrubs.variants[p.variant].leaves.indices.size())/3;
        renderTriangles+=(stemIndices[p.variant]+leafIndices[p.variant])/3;}
    std::uint64_t broadleafTriangles{};for(const auto& pin:BroadleafPins){auto m=Q::Detail::LoadPinned(runtimeAssets/pin.path,pin);
        RootBroadleafMesh(m);(void)Q::Detail::MeshIdentity(m);broadleafTriangles+=m.indices.size()/3;}
    std::uint64_t sourceTriangles{},surfaceCells{},enclosedCells{},primitiveTriangles{};std::vector<W::Box> wood;
    for(const auto& plant:c.canopy.plants){budget.Time();(void)Q::Detail::MeshIdentity(plant.rootedTrunk);
        const auto proxy=CreateCanopyWoodProxy(plant.rootedTrunk,assets.canopy[2],plant.pose);
        sourceTriangles+=proxy.sourceTriangles;surfaceCells+=proxy.surfaceCells;enclosedCells+=proxy.enclosedGridCells;
        const auto triangles=(plant.rootedTrunk.indices.size()+assets.canopy[1].indices.size()+assets.canopy[2].indices.size())/3;
        if(wood.empty())primitiveTriangles=triangles;else Need(primitiveTriangles==triangles,"AUTHORED_EXPECTED_ROOTED_RECIPE");
        wood.insert(wood.end(),proxy.parts.begin(),proxy.parts.end());}
    Q::Detail::SameBoxes(wood,c.wood,"AUTHORED_EXPECTED_WOOD_RECOMPUTE");
    Field(out,"world_profile","forest-wood");Field(out,"forest_cover_mode","fixed_cluster_patches");
    Number(out,"ground_cover_instances",cover.plants.size());Number(out,"forest_cover_seed",coverSettings.seed);
    Number(out,"forest_cover_authored_population",coverSettings.targetPlants);Number(out,"forest_cover_maximum_patches",coverSettings.maximumPatches);
    Number(out,"forest_cover_patch_minimum_plants",coverSettings.minimumPlantsPerPatch);Number(out,"forest_cover_patch_maximum_plants",coverSettings.maximumPlantsPerPatch);
    Number(out,"forest_cover_independent_anchors",coverSettings.independentAnchors);Number(out,"forest_cover_patches",cover.patches.size());
    Boolean(out,"forest_cover_reached_budget",cover.reachedBudget);Number(out,"forest_cover_fingerprint",Q::Detail::NativeCoverIdentity(cover));
    Number(out,"forest_cover_clearance_colliders",c.worlds[0].size());Number(out,"forest_cover_clearance_collider_fingerprint",Q::Detail::BoxesIdentity(W::Boxes(c.worlds[0])));
    Field(out,"forest_cover_variants",Csv(coverVariants));Number(out,"fern_colony_population",c.colonies.plants.size());
    Number(out,"fern_colony_authored_population",colonySettings.targetPlants);Number(out,"fern_colonies",c.colonies.colonies.size());Number(out,"fern_colony_seed",colonySettings.seed);
    Boolean(out,"fern_colony_reached_budget",c.colonies.reachedBudget);Number(out,"fern_colony_fingerprint",Colonies(c.colonies.plants));
    Number(out,"fern_colony_original_fingerprint",Poses(author.understory.ferns));Field(out,"fern_colony_variants",Csv(colonyVariants));
    Number(out,"understory_ferns",author.understory.ferns.size());Number(out,"understory_saplings",author.understory.saplings.size());
    Boolean(out,"shrubs_enabled",true);Number(out,"shrubs_plants",c.shrubs.plants.size());Number(out,"shrubs_fingerprint",c.shrubs.fingerprint);
    Number(out,"shrubs_author_leaf_count",c.shrubs.variants[0].leafCount);Field(out,"shrubs_render_leaf_counts",Csv(renderLeaves));
    Number(out,"shrubs_author_instance_triangles",authorTriangles);Number(out,"shrubs_render_instance_triangles",renderTriangles);
    Number(out,"shrubs_uploaded_meshes",stemIndices.size()+leafIndices.size());Field(out,"shrubs_uploaded_stem_indices",Csv(stemIndices));
    Field(out,"shrubs_uploaded_leaf_indices",Csv(leafIndices));Number(out,"shrubs_submitted",c.shrubs.plants.size());Boolean(out,"middle_layer_comparison",true);
    Number(out,"middle_layer_population",author.middle.trees.size());Number(out,"middle_layer_target",author.middle.trees.size());
    Boolean(out,"middle_layer_reached_budget",author.middle.reachedBudget);Number(out,"middle_layer_attempted",author.middle.attempted);
    Number(out,"middle_layer_transform_fingerprint",MiddleLayerTransformFingerprint(author.middle.trees));Number(out,"submitted_middle_layer",author.middle.trees.size());
    Number(out,"submitted_middle_layer_triangles",author.middle.trees.size()*broadleafTriangles);
    Field(out,"fir_layout","forest-wood-mature");Number(out,"fir_layout_population",author.firs.firs.size());Boolean(out,"fir_layout_reached_budget",author.firs.reachedBudget);
    Number(out,"fir_layout_fingerprint",author.firs.fingerprint);Number(out,"fir_layout_patch_centres",author.firs.patchCentres.size());
    Field(out,"canopy_collision_mode","wood-grid-conservative");Number(out,"canopy_collision_parts",wood.size());Field(out,"canopy_collision_spacing_m","0.15");
    Number(out,"canopy_wood_source_triangles",sourceTriangles);Number(out,"canopy_wood_surface_cells",surfaceCells);Number(out,"canopy_wood_enclosed_grid_cells",enclosedCells);
    Boolean(out,"canopy_world_profile",true);Boolean(out,"canopy_enabled",true);Number(out,"canopy_plants",c.canopy.plants.size());Number(out,"canopy_population_fingerprint",c.canopy.fingerprint);
    Number(out,"canopy_ground_checks",c.canopy.groundChecks);Number(out,"canopy_rooted_meshes",c.canopy.plants.size());Number(out,"canopy_primitive_triangles",primitiveTriangles);
    Number(out,"canopy_materials",assets.canopy.size());Number(out,"canopy_submitted_slots",c.canopy.plants.size()*assets.canopy.size());
    Boolean(out,"trail_litter_enabled",true);Number(out,"trail_litter_patches",c.litter.patches.size());Number(out,"trail_litter_branches",c.litter.branchCount);
    Number(out,"trail_litter_stones",c.litter.stoneCount);Number(out,"trail_litter_fingerprint",c.litter.fingerprint);Number(out,"trail_litter_triangles",(c.litter.branches.indices.size()+c.litter.stones.indices.size())/3);
    Need(out.size()==70,"AUTHORED_EXPECTED_ORIGINAL_KEYS");
    Field(out,"grass_shape_profile","source-geometry");Field(out,"understory_mix_profile","original");
    for(unsigned variant=0;variant<5;++variant){const auto prefix="grass_render_mesh_"+std::to_string(variant)+"_";
        Number(out,prefix+"identity",Q::Detail::MeshIdentity(assets.grass[variant]));Number(out,prefix+"vertices",assets.grass[variant].vertices.size());
        Number(out,prefix+"triangles",assets.grass[variant].indices.size()/3);}
    Field(out,"forest_cover_retention_revision","original-safe-pose-v1");Number(out,"forest_cover_original_fingerprint",Q::Detail::NativeCoverIdentity(c.sourceCover));
    Number(out,"forest_cover_retained",c.retention.retained);Number(out,"forest_cover_rejected_original",c.retention.rejected);Number(out,"forest_cover_filled",c.retention.filled);
    Number(out,"forest_cover_provenance_fingerprint",c.retention.fingerprint);Number(out,"forest_cover_source_world_fingerprint",c.retention.originalInputFingerprint);
    Number(out,"forest_cover_final_world_fingerprint",c.retention.finalInputFingerprint);
    Field(out,"entrance_canopy_layout","entrance-authored-world-v1");Boolean(out,"entrance_author_applied",author.applied);
    Number(out,"entrance_author_fingerprint",author.fingerprint);Number(out,"entrance_author_source_fir_fingerprint",author.sourceFirFingerprint);
    Number(out,"entrance_author_source_middle_fingerprint",author.sourceMiddleFingerprint);Number(out,"entrance_author_withdrawals",c.source.firs.firs.size()-author.firs.firs.size()+
        c.source.props.size()-author.props.size()+c.source.understory.saplings.size()-author.understory.saplings.size()+c.source.middle.trees.size()-author.middle.trees.size());
    std::size_t moved{};for(std::size_t i=0;i<author.understory.ferns.size();++i)moved+=!Q::Detail::Same(author.understory.ferns[i],c.source.understory.ferns[i]);
    Number(out,"entrance_author_fern_relocations",moved);Number(out,"entrance_author_fir_target",author.firs.firs.size());Number(out,"entrance_author_middle_target",author.middle.trees.size());
    Number(out,"entrance_canopy_original_fingerprint",c.canopyMetadata.originalFingerprint);Number(out,"entrance_canopy_appended",c.canopyMetadata.appended);
    Number(out,"entrance_canopy_full_face_checks",c.canopyMetadata.faceChecks);Number(out,"entrance_canopy_exact_overlay_faces",c.canopyMetadata.exactOverlayFaces);
    Number(out,"entrance_canopy_new_wood_parts",c.canopyMetadata.woodParts);
    Habitat(out,c,terrain,budget);
    const auto definition=HabitatGroundDefinition(true,true);const auto& t=definition.textures;
    Need(definition.material.materialClass==Aevrynd::MaterialClass3D::Terrain&&HabitatGroundLayerSource(definition)=="forest-leaves-04-habitat",
        "AUTHORED_EXPECTED_LEAF_RECIPE");
    Field(out,"ground_source",std::string(GroundForestWoodArtSource(definition)));Field(out,"ground_habitat_layer_source",std::string(HabitatGroundLayerSource(definition)));
    Field(out,"ground_height_alpha","shared_core_normalized");Number(out,"ground_texture_layers",1+t.additionalLayers.size());
    Number(out,"ground_material_class",static_cast<unsigned>(definition.material.materialClass));Number(out,"ground_maximum_resolution",t.maximumResolution);
    Field(out,"ground_normal_scale",Scalar(Aevrynd::TerrainLayerCoverageNormalScaleMarker3D));Field(out,"ground_texture_scale",Scalar(definition.material.textureScale));Boolean(out,"ground_use_uv",true);
    // Main 的正式作者設定；不採樣 GPU 收據，也不由 Normalize 的平滑補值建立 oracle。
    Aevrynd::TerrainDistanceMaterialSettings3D distance;distance.normalDetailStrength=.65f;distance.mesoPartitionEnabled=false;
    Field(out,"ground_distance_normal_strength",Scalar(distance.normalDetailStrength));Boolean(out,"ground_meso_partition",distance.mesoPartitionEnabled);
    Field(out,"ground_art_base_color_path",t.baseColorPath.generic_string());Field(out,"ground_art_surface_path",t.roughnessPath.generic_string());Field(out,"ground_art_normal_path",t.normalPath.generic_string());
    Number(out,"ground_art_surface_encoding",static_cast<unsigned>(t.surfaceEncoding));Boolean(out,"ground_packed_orm",t.surfaceEncoding==Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrm);
    Field(out,"ground_habitat_layer_base_path",t.additionalLayers[0].baseColorPath.generic_string());Field(out,"ground_habitat_layer_surface_path",t.additionalLayers[0].roughnessPath.generic_string());
    Field(out,"ground_habitat_layer_normal_path",t.additionalLayers[0].normalPath.generic_string());Number(out,"ground_habitat_layer_surface_encoding",static_cast<unsigned>(t.additionalLayers[0].surfaceEncoding));
    Number(out,"ground_loaded_texture_identity",MaterialSources(definition,runtimeAssets/"mistwatch-materials"));
    const auto shelter=ShelterWeatheredUv::Create(),shelterOriginal=CreatePlankedRepairShelter();SameGeometry(shelterOriginal,shelter);
    StructureSources(runtimeAssets/"mistwatch-materials");const auto shelterSolids=RepairShelterSolids();InsideSolids(shelter,shelterSolids,ShelterBoardCounts);
    Need(shelter.vertices.size()==7392&&shelter.indices.size()==10164,"AUTHORED_EXPECTED_SHELTER_COUNTS");
    Need(Uv(shelter)==ShelterWeatheredUv::Fingerprint(shelter),"AUTHORED_EXPECTED_SHELTER_UV_HASH");
    Boolean(out,"planked_shelter_enabled",true);Boolean(out,"planked_shelter_uv_aligned",true);Boolean(out,"planked_shelter_roof_uv_aligned",true);
    Field(out,"planked_shelter_wood_profile",ShelterWeatheredUv::Profile);Number(out,"planked_shelter_uv_fingerprint",Uv(shelter));
    Number(out,"planked_shelter_vertices",shelter.vertices.size());Number(out,"planked_shelter_indices",shelter.indices.size());
    const auto workshop=WorkshopVisual::Create();Need(workshop.vertices.size()==7296&&workshop.indices.size()==10032,"AUTHORED_EXPECTED_WORKSHOP_COUNTS");
    SameGeometry(OriginalWorkshop(),workshop);const auto workshopSolids=WorkshopSolids();InsideSolids(workshop,workshopSolids,WorkshopVisual::BoardCounts);
    Need(Uv(workshop)==WorkshopVisual::Fingerprint(workshop),"AUTHORED_EXPECTED_WORKSHOP_UV_HASH");
    Boolean(out,"planked_workshop_enabled",true);Field(out,"planked_workshop_profile",WorkshopVisual::Profile);Number(out,"planked_workshop_vertices",workshop.vertices.size());
    Number(out,"planked_workshop_indices",workshop.indices.size());Number(out,"planked_workshop_uv_fingerprint",Uv(workshop));budget.Time();return out;
}
// 供逐欄負控重用；此 overload 的 expected 必須由 Build 取得，正式入口仍會重新 Build。
inline void ValidateFields(const Contract& contract,const Contract& expected){
    Detail::Need(!contract.empty()&&contract.size()<=256,"AUTHORED_EXPECTED_KEY_COUNT");
    for(const auto& [key,value]:contract){Detail::Need(!key.empty()&&key.size()<=96&&value.size()>0&&value.size()<=256,
        "AUTHORED_EXPECTED_FIELD_BOUND");Detail::Need(value.find_first_of("\r\n\t")==std::string::npos,"AUTHORED_EXPECTED_FIELD_TYPE");}
    Detail::Need(contract.size()==expected.size(),"AUTHORED_EXPECTED_KEY_COUNT");
    for(const auto& [key,value]:expected){const auto found=contract.find(key);Detail::Need(found!=contract.end(),"AUTHORED_EXPECTED_MISSING_KEY");
        Detail::Need(found->second==value,"AUTHORED_EXPECTED_VALUE_OR_TYPE");}
}
inline void Validate(const Contract& contract,const Q::Context& context,const Aevrynd::HeightField3D& terrain,
    const Q::Assets& assets,W::Budget& budget,const std::filesystem::path& runtimeAssets){
    ValidateFields(contract,Build(context,terrain,assets,budget,runtimeAssets));
}
// 獨立重建目前森林地表配方；不呼叫 Apply 建立自己的預期，也不讀原生收據。
// 本入口不涵蓋目前灌木、建築及地面分域幾何，不能替代舊 153 欄或完整世界資格。
inline Aevrynd::MaterialAssetDefinition3D ForestSurfaceDefinition(){
    auto ground=HabitatGroundDefinition(true,true);
    ground.textures.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    ground.textures.roughnessPath="moss-height/Ground037_SurfaceOrmHeight_Level0.png";
    ground.textures.integrity[1]={1531546,"012d2378aa6e62cca05ad66d69b512966d5894694b7b24d39afd0ca7ca655f38"};
    auto& leaf=ground.textures.additionalLayers[0];leaf.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    leaf.roughnessPath="leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png";
    leaf.integrity[1]={9102955,"9f3259f4772f294edca1de03fa4e8d7ed65c0924238b1a6d0b27b8fed7c0ef85"};
    return ground;
}
inline Aevrynd::TerrainDistanceMaterialSettings3D ForestSurfaceDistance(){
    Aevrynd::TerrainDistanceMaterialSettings3D settings;settings.normalDetailStrength=.65f;
    settings.mesoPartitionEnabled=false;settings.soilLayerScale=1.75f;return settings;
}
inline Aevrynd::TerrainParallaxSettings3D ForestSurfaceParallax(){
    Aevrynd::TerrainParallaxSettings3D settings;settings.enabled=true;settings.depthMeters=.015f;return settings;
}
inline void ValidateForestSurfaceRecipe(const Aevrynd::MaterialAssetDefinition3D& ground,
    const Aevrynd::TerrainDistanceMaterialSettings3D& distance,const Aevrynd::TerrainParallaxSettings3D& parallax){
    using namespace Detail;const auto expected=ForestSurfaceDefinition();
    const auto& a=ground.textures;const auto& b=expected.textures;
    const auto& m=ground.material;const auto& e=expected.material;
    const std::array actualFloats{m.tint.r,m.tint.g,m.tint.b,m.tint.a,m.surface.roughness,m.surface.metallic,m.surface.emissive,m.surface.occlusion,
        m.textureScale,m.normalScale,m.baseTextureScale,m.detailTextureScale,m.alphaCutoff};
    const std::array expectedFloats{e.tint.r,e.tint.g,e.tint.b,e.tint.a,e.surface.roughness,e.surface.metallic,e.surface.emissive,e.surface.occlusion,
        e.textureScale,e.normalScale,e.baseTextureScale,e.detailTextureScale,e.alphaCutoff};
    for(unsigned i=0;i<actualFloats.size();++i)Need(Q::Detail::Same(actualFloats[i],expectedFloats[i]),"SURFACE_EXPECTED_MATERIAL");
    Need(ground.name==expected.name&&a.maximumResolution==b.maximumResolution&&a.alphaMode==b.alphaMode&&
        Q::Detail::Same(a.alphaCutoff,b.alphaCutoff)&&m.castsShadow==e.castsShadow&&m.doubleSided==e.doubleSided&&m.useUv==e.useUv&&
        m.materialClass==e.materialClass&&m.shadingModel==e.shadingModel&&m.alphaMode==e.alphaMode&&
        m.textures.value==e.textures.value&&m.textures.semanticIdentity==e.textures.semanticIdentity,"SURFACE_EXPECTED_MATERIAL");
    const auto layer=[&](const auto& actual,const auto& wanted){
        Need(actual.baseColorPath==wanted.baseColorPath&&actual.roughnessPath==wanted.roughnessPath&&actual.normalPath==wanted.normalPath&&
            actual.surfaceEncoding==wanted.surfaceEncoding,"SURFACE_EXPECTED_BINDING");
        for(unsigned i=0;i<3;++i)Need(actual.integrity[i].expectedBytes==wanted.integrity[i].expectedBytes&&
            actual.integrity[i].expectedSha256==wanted.integrity[i].expectedSha256,"SURFACE_EXPECTED_PIN");};
    layer(a,b);for(unsigned i=0;i<2;++i)layer(a.additionalLayers[i],b.additionalLayers[i]);
    Need(distance==ForestSurfaceDistance(),"SURFACE_EXPECTED_DISTANCE");
    Need(parallax==ForestSurfaceParallax(),"SURFACE_EXPECTED_PARALLAX");
    Need(Aevrynd::ValidMaterialSurfaceEncodingDefinition3D(a)&&Aevrynd::MaterialHeightLayerMask3D(a)==3u,"SURFACE_EXPECTED_HEIGHT_MASK");
}
inline Contract BuildForestSurface(const std::filesystem::path& runtimeAssets){
    using namespace Detail;Need(runtimeAssets.is_absolute(),"AUTHORED_EXPECTED_ASSET_ROOT");
    const auto definition=ForestSurfaceDefinition();const auto& t=definition.textures;
    const auto distance=ForestSurfaceDistance();const auto parallax=ForestSurfaceParallax();
    ValidateForestSurfaceRecipe(definition,distance,parallax);Contract out;
    Field(out,"profile","forest-moss-leaf-metric-v1");
    Field(out,"ground_art_surface_path",t.roughnessPath.generic_string());
    Number(out,"ground_art_surface_bytes",t.integrity[1].expectedBytes);Field(out,"ground_art_surface_sha256",t.integrity[1].expectedSha256);
    Number(out,"ground_art_surface_encoding",static_cast<unsigned>(t.surfaceEncoding));
    const auto& leaf=t.additionalLayers[0];Field(out,"ground_habitat_layer_surface_path",leaf.roughnessPath.generic_string());
    Number(out,"ground_habitat_layer_surface_bytes",leaf.integrity[1].expectedBytes);Field(out,"ground_habitat_layer_surface_sha256",leaf.integrity[1].expectedSha256);
    Number(out,"ground_habitat_layer_surface_encoding",static_cast<unsigned>(leaf.surfaceEncoding));
    Number(out,"ground_rock_surface_encoding",static_cast<unsigned>(t.additionalLayers[1].surfaceEncoding));
    Number(out,"height_layer_mask",Aevrynd::MaterialHeightLayerMask3D(t));Field(out,"ground_soil_layer_scale",Scalar(distance.soilLayerScale));
    Number(out,"ground_loaded_texture_identity",MaterialSources(definition,runtimeAssets/"mistwatch-materials"));
    Boolean(out,"parallax_enabled",parallax.enabled);Field(out,"parallax_depth_meters",Scalar(parallax.depthMeters));
    // 直接逐個原始 float word 重算；不以 renderer fingerprint 函式驗證自身。
    std::uint64_t fingerprint=14695981039346656037ull;
    for(float value:{parallax.depthMeters,parallax.fadeStart,parallax.fadeEnd,parallax.minimumNormalY,parallax.minimumViewCosine,parallax.maximumUvOffset})
        for(unsigned byte=0;byte<4;++byte){fingerprint^=(std::bit_cast<std::uint32_t>(value)>>(8*byte))&255u;fingerprint*=1099511628211ull;}
    Number(out,"parallax_settings_fingerprint",fingerprint);return out;
}
}
