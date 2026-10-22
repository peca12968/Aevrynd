#include "MistwatchShrubCanopy.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace Mistwatch;using namespace Aevrynd;
unsigned checks{};
void Need(bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);}
struct Export {
    std::vector<std::uint32_t> words;
    void Word(std::uint32_t n){words.push_back(n);}void Float(float n){Need(std::isfinite(n),"FINITE_EXPORT");Word(std::bit_cast<std::uint32_t>(n));}
    void Vec(Vec3 p){Float(p.x);Float(p.y);Float(p.z);}void Pose(const Transform3D& p){Vec(p.position);Vec(p.rotation);Vec(p.scale);}
    void Mesh(const Mesh3D& m){Need(m.Valid(),"VALID_EXPORTED_MESH");Word(unsigned(m.vertices.size()));Word(unsigned(m.indices.size()));
        for(const auto& v:m.vertices){Vec({v.x,v.y,v.z});Word(v.normal);Word(v.color);Float(v.u);Float(v.v);}for(auto i:m.indices)Word(i);}
    void Save(const std::filesystem::path& p)const{Need(p.is_absolute()&&!std::filesystem::exists(p),"FRESH_OUTPUT");std::ofstream out(p,std::ios::binary);
        for(auto n:words)for(unsigned b=0;b<4;++b)out.put(char((n>>(b*8))&255u));out.close();Need(bool(out),"COMPLETE_EXPORT");}
};
struct Assets{std::array<ForestCoverAsset,5> grass;std::array<FernColonyAsset,4> ferns;Mesh3D leaf;};
Assets Load(const std::filesystem::path& grass,const std::filesystem::path& fern,const std::filesystem::path& leaf){
    Assets result;
    for(unsigned i=0;i<5;++i){auto loaded=LoadAevryndMeshFile(grass/GrassMeshName(i));Need(bool(loaded),"GRASS_LOAD");
        Need(loaded.mesh.vertices.size()==GrassVertexCounts[i]&&loaded.mesh.indices.size()==GrassTriangleCounts[i]*3u,"GRASS_RAW_TOPOLOGY");
        NormalizeGrassAsset(loaded.mesh);result.grass[i]=ForestCoverAssetFromMesh(loaded.mesh);}
    for(unsigned i=0;i<4;++i){auto loaded=LoadAevryndMeshFile(fern/FernMeshName(i));Need(bool(loaded),"FERN_LOAD");
        Need(loaded.mesh.vertices.size()==FernVertexCounts[i]&&loaded.mesh.indices.size()==FernTriangleCounts[i]*3u,"FERN_RAW_TOPOLOGY");
        NormalizeFernAsset(loaded.mesh);result.ferns[i]=FernColonyAssetFromMesh(loaded.mesh);}
    auto loaded=LoadAevryndMeshFile(leaf);Need(bool(loaded),"LEAF_LOAD");result.leaf=std::move(loaded.mesh);return result;
}
FernColonyResult Ferns(const HeightField3D& terrain,const ForestCoverResult& grass,const std::array<FernColonyAsset,4>& assets,bool original){
    FernColonySettings settings;settings.targetPlants=1;settings.maximumColonies=12;settings.attemptsPerColony=64;
#if defined(AEVRYND_VEGETATION_POLICY_BEFORE)
    Need(original&&OriginalEntryLayoutActive,"BEFORE_ORIGINAL_ONLY");return CreateFernColonies(terrain,grass,{}, {},assets,settings);
#else
    if(!original)return CreateFernColonies(terrain,grass,{}, {},assets,settings);
    return CreateFernColonies(terrain,grass,{}, {},assets,settings,{},false,nullptr,OriginalEntryLayout);
#endif
}
ShrubCanopyResult Shrubs(const HeightField3D& terrain,const Assets& a,const ForestCoverResult& grass,const FernColonyResult& ferns,ShrubSearchTrace& trace,bool original,
    std::span<const BoxCollider3D> solids={}){
#if defined(AEVRYND_VEGETATION_POLICY_BEFORE)
    Need(original&&OriginalEntryLayoutActive,"BEFORE_ORIGINAL_ONLY");return CreateShrubCanopy(a.leaf,terrain,solids,{},grass,ferns,a.ferns,4,{},false,&trace);
#else
    if(!original)return CreateShrubCanopy(a.leaf,terrain,solids,{},grass,ferns,a.ferns,4,{},false,&trace);
    return CreateShrubCanopy(a.leaf,terrain,solids,{},grass,ferns,a.ferns,4,{},false,&trace,nullptr,OriginalEntryLayout);
#endif
}
Export Run(const Assets& a,bool original){
    // 真資產的小型平地單元，不生成／宣稱正式森林或地形資格。
    HeightField3D terrain({{-40,0,-20},161,241,.5f});terrain.Fill(0);
    ForestCoverSettings settings;settings.targetPlants=24;settings.maximumPatches=12;settings.attemptsPerPatch=64;settings.independentAnchors=16;
    const auto& layout=original?OriginalEntryLayout:ActiveEntryLayout;
    const auto grass=CreateForestCover(terrain,{}, {},{},a.grass,settings,{},false,layout);
    Need(grass.reachedBudget&&grass.plants.size()==24,"SMALL_REAL_GRASS_BUDGET");
    const auto ferns=Ferns(terrain,grass,a.ferns,original);Need(ferns.reachedBudget&&ferns.plants.size()==1,"SMALL_REAL_FERN_BUDGET");
    ShrubSearchTrace trace;const auto shrubs=Shrubs(terrain,a,grass,ferns,trace,original);Need(shrubs.reachedBudget&&shrubs.plants.size()==4,"SMALL_REAL_SHRUB_BUDGET");
    Export out;out.Word(0x564c5031u);
    for(const auto& asset:a.grass){out.Float(asset.radius);out.Float(asset.height);out.Float(asset.minimumY);out.Float(asset.maximumY);}
    for(const auto& asset:a.ferns){out.Float(asset.radius);out.Float(asset.height);out.Float(asset.minimumY);out.Word(asset.vertices);out.Word(asset.triangles);}
    out.Word(unsigned(grass.plants.size()));out.Word(unsigned(grass.patches.size()));out.Word(grass.rejectedClearance);out.Word(grass.rejectedTerrain);out.Word(grass.rejectedSpacing);out.Word(grass.reachedBudget);
    for(const auto& p:grass.plants){Need(ForestCoverClearance(p.transform.position,p.radius,{},layout),"GRASS_LAYOUT");out.Pose(p.transform);out.Word(p.variant);out.Word(p.patch);out.Float(p.radius);}
    for(const auto& p:grass.patches){out.Vec(p.centre);out.Float(p.radius);out.Float(p.aspect);out.Float(p.yaw);out.Word(p.requested);out.Word(p.accepted);}
    out.Word(unsigned(ferns.plants.size()));out.Word(unsigned(ferns.colonies.size()));out.Word(ferns.rejectedClearance);out.Word(ferns.rejectedTerrain);out.Word(ferns.rejectedExistingFern);out.Word(ferns.rejectedGrass);out.Word(ferns.rejectedSpacing);out.Word(ferns.reachedBudget);
    for(const auto& p:ferns.plants){Need(ForestCoverClearance(p.transform.position,p.radius,{},layout),"FERN_LAYOUT");out.Pose(p.transform);out.Word(p.variant);out.Word(p.colony);out.Word(p.grassPatch);out.Float(p.radius);}
    for(const auto& p:ferns.colonies){out.Vec(p.centre);out.Float(p.radius);out.Word(p.grassPatch);out.Word(p.requested);out.Word(p.accepted);}
    out.Word(shrubs.attempted);out.Word(shrubs.rejectedClearance);out.Word(shrubs.rejectedTerrain);out.Word(shrubs.rejectedVegetation);out.Word(shrubs.reachedBudget);
    out.Word(std::uint32_t(shrubs.fingerprint));out.Word(std::uint32_t(shrubs.fingerprint>>32));out.Word(unsigned(shrubs.plants.size()));
    for(const auto& v:shrubs.variants){out.Float(v.radius);out.Float(v.height);out.Float(v.minimumY);out.Float(v.maximumY);out.Word(v.leafCount);out.Word(v.stemCount);
        out.Mesh(v.stems);out.Mesh(v.leaves);out.Word(unsigned(v.attachments.size()));
        for(const auto& p:v.attachments){out.Word(p.vertex);out.Vec(p.centre);out.Float(p.branchRadius);}}
    for(const auto& p:shrubs.plants){Need(ForestCoverClearance(p.pose.position,p.radius,{},layout),"SHRUB_LAYOUT");out.Pose(p.pose);out.Word(p.variant);out.Float(p.radius);}
    out.Word(unsigned(trace.records.size()));
    for(const auto& p:trace.records){out.Word(p.attempt);out.Vec(p.root);out.Word(p.variant);out.Float(p.scale);out.Float(p.radius);out.Word(unsigned(p.stage));out.Word(p.vegetationMask);}
    auto bad=a.ferns;bad[0].radius=0;bool rejected=false;
    try{(void)Ferns(terrain,grass,bad,original);}catch(const std::invalid_argument& e){rejected=std::string_view(e.what())=="Invalid measured fern asset";}
    Need(rejected,"BAD_FERN_REJECTED_BEFORE_WORK");
    const std::array<BoxCollider3D,1> invalid{{{{NAN,0,0},{1,1,1}}}};ShrubSearchTrace emptyTrace;rejected=false;
    try{(void)Shrubs(terrain,a,grass,ferns,emptyTrace,original,invalid);}catch(const std::invalid_argument& e){rejected=std::string_view(e.what())=="Invalid shrub solid";}
    Need(rejected&&emptyTrace.records.empty(),"BAD_SHRUB_REJECTED_WITHOUT_TRACE");return out;
}
}
int main(int argc,char** argv){try{
    Need(argc==4||argc==5,"GRASS_DIR_FERN_DIR_LEAF_OPTIONAL_EXPORT");const auto assets=Load(argv[1],argv[2],argv[3]);const auto original=Run(assets,true);
#if !defined(AEVRYND_VEGETATION_POLICY_BEFORE)
    const auto active=Run(assets,false);
    if constexpr(OriginalEntryLayoutActive)Need(active.words==original.words,"DEFAULT_ORIGINAL_ALL_FIELDS");
#endif
    if(argc==5)original.Save(std::filesystem::path(argv[4]));
    std::cout<<"VEGETATION_LAYOUT_REAL_ASSET_UNIT_PASS checks="<<checks<<" words="<<original.words.size()<<" grass=24 fern=1 shrubs=4 full_world=0 native=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"VEGETATION_LAYOUT_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
