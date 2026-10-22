#include "MistwatchEntranceCanopy.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace Aevrynd;using namespace Mistwatch;
unsigned checks{};
void Need(bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);}
struct Export {
    std::vector<std::uint32_t> words;
    void Word(std::uint32_t n){words.push_back(n);}
    void Float(float n){Need(std::isfinite(n),"FINITE_EXPORT");Word(std::bit_cast<std::uint32_t>(n));}
    void Vec(Vec3 p){Float(p.x);Float(p.y);Float(p.z);}
    void Pose(const Transform3D& p){Vec(p.position);Vec(p.rotation);Vec(p.scale);}
    void Save(const std::filesystem::path& p)const {
        Need(p.is_absolute()&&!std::filesystem::exists(p),"FRESH_OUTPUT");std::ofstream out(p,std::ios::binary);
        for(auto n:words)for(unsigned b=0;b<4;++b)out.put(char((n>>(b*8))&255u));
        out.close();Need(bool(out),"EXPORT_COMPLETE");
    }
};
bool Clear(Vec3 p,float radius,std::span<const BoxCollider3D> solids,bool original) {
#if defined(AEVRYND_FOREST_POLICY_BEFORE)
    Need(original&&OriginalEntryLayoutActive,"BEFORE_ORIGINAL_ONLY");return ForestCoverClearance(p,radius,solids);
#else
    return ForestCoverClearance(p,radius,solids,original?OriginalEntryLayout:ActiveEntryLayout);
#endif
}
ForestCoverResult Grass(const HeightField3D& terrain,const std::array<ForestCoverAsset,5>& assets,bool original){
    ForestCoverSettings settings;settings.targetPlants=64;settings.maximumPatches=16;
#if defined(AEVRYND_FOREST_POLICY_BEFORE)
    Need(original,"BEFORE_ORIGINAL_ONLY");return CreateForestCover(terrain,{}, {},{},assets,settings);
#else
    return CreateForestCover(terrain,{}, {},{},assets,settings,{},false,original?OriginalEntryLayout:ActiveEntryLayout);
#endif
}
CanopyPopulation Canopy(const std::array<Mesh3D,3>& meshes,const CanopyGeometry& geometry,const HeightField3D& terrain,bool original){
    const std::array<FernColonyAsset,4> ferns{};
#if defined(AEVRYND_FOREST_POLICY_BEFORE)
    Need(original,"BEFORE_ORIGINAL_ONLY");return CreateCanopyPopulation(meshes,geometry,terrain,{}, {},{}, {},ferns,{},1);
#else
    return CreateCanopyPopulation(meshes,geometry,terrain,{}, {},{}, {},ferns,{},1,original?OriginalEntryLayout:ActiveEntryLayout);
#endif
}
Export Run(bool original){
    Export out;const auto terrain=CreateTerrain();
    const std::array<BoxCollider3D,1> solids{{{{-14,0,3},{1,2,1}}}};
    // 真實作者函式的格點探針；不是世界生成或可玩性認證。
    for(float radius:{.25f,.75f,1.5f})for(int z=-20;z<=100;++z)for(int x=-36;x<=36;++x){
        const Vec3 root{float(x),terrain.SampleHeight(float(x),float(z)),float(z)};
        const auto result=Clear(root,radius,solids,original);out.Word(result?1u:0u);
#if !defined(AEVRYND_FOREST_POLICY_BEFORE)
        if(!original)Need(result==ForestCoverClearance(root,radius,solids),"DEFAULT_ACTIVE_CLEARANCE");
#endif
    }
    Need(!Clear({NAN,0,0},.5f,{},original)&&!Clear({0,0,0},0,{},original)&&!Clear({0,0,0},INFINITY,{},original),"INVALID_CLEARANCE_REJECTED");
    // 小型單元fixture只驗API傳遞，不把占位尺寸稱為正式資產的驗收。
    std::array<ForestCoverAsset,5> assets;for(auto& a:assets)a={GrassMinimumRadius,GrassAssetHeight,0,GrassAssetHeight};
    const auto grass=Grass(terrain,assets,original);Need(grass.reachedBudget&&grass.plants.size()==64,"SMALL_GRASS_BUDGET");
    out.Word(unsigned(grass.plants.size()));out.Word(unsigned(grass.patches.size()));
    out.Word(grass.rejectedClearance);out.Word(grass.rejectedTerrain);out.Word(grass.rejectedSpacing);
    for(const auto& p:grass.plants){Need(Clear(p.transform.position,p.radius,{},original),"GRASS_EXPLICIT_POLICY");
        out.Pose(p.transform);out.Word(p.variant);out.Word(p.patch);out.Float(p.radius);}
    for(const auto& p:grass.patches){out.Vec(p.centre);out.Float(p.radius);out.Float(p.aspect);out.Float(p.yaw);out.Word(p.requested);out.Word(p.accepted);}
    auto trunk=Mesh3D::CreateUnitBox();for(auto& v:trunk.vertices){v.x*=.2f;v.z*=.2f;v.y+=.5f;}
    auto crown=Mesh3D::CreateUnitBox();for(auto& v:crown.vertices){v.x*=.6f;v.z*=.6f;v.y+=1.6f;}
    const std::array<Mesh3D,3> meshes{trunk,crown,trunk};const auto geometry=MeasureCanopy(meshes);
    const auto canopy=Canopy(meshes,geometry,terrain,original);Need(canopy.reachedBudget&&canopy.plants.size()==1,"SMALL_CANOPY_BUDGET");
    out.Word(canopy.attempts);out.Word(canopy.clearanceRejected);out.Word(canopy.rootRejected);out.Word(canopy.spacingRejected);out.Word(canopy.meshRejected);
    out.Word(std::uint32_t(canopy.fingerprint));out.Word(std::uint32_t(canopy.fingerprint>>32));out.Word(unsigned(canopy.groundChecks));
    const auto& p=canopy.plants[0];Need(Clear(p.pose.position,p.crownRadius,{},original),"CANOPY_EXPLICIT_POLICY");
    out.Pose(p.pose);out.Vec(p.lower.center);out.Vec(p.lower.extents);out.Float(p.crownRadius);out.Float(p.rootRadius);
    out.Word(unsigned(p.rootedTrunk.vertices.size()));out.Word(unsigned(p.rootedTrunk.indices.size()));
    for(const auto& v:p.rootedTrunk.vertices){out.Vec({v.x,v.y,v.z});out.Word(v.normal);out.Word(v.color);out.Float(v.u);out.Float(v.v);}
    for(auto i:p.rootedTrunk.indices)out.Word(i);
    // 入口正式作者的原身份檢查仍先行；不以此負控冒充完整八冠實測。
    EntranceCanopyMetadata metadata;bool rejected=false;
    try{
#if defined(AEVRYND_FOREST_POLICY_BEFORE)
        (void)AppendEntranceCanopy({},meshes,geometry,trunk,terrain,{}, {},{},metadata);
#else
        (void)AppendEntranceCanopy({},meshes,geometry,trunk,terrain,{}, {},{},metadata,original?OriginalEntryLayout:ActiveEntryLayout);
#endif
    }catch(const std::runtime_error& e){rejected=std::string_view(e.what())=="ENTRANCE_CANOPY_FIRST_DIVERGENCE anchor=0 gate=original-four-forestwood-identity";}
    Need(rejected&&metadata.appended==0&&metadata.faceChecks==0,"ORIGINAL_ENTRY_IDENTITY_GATE_RETAINED");
    return out;
}
}
int main(int argc,char** argv){try{
    Need(argc==1||argc==2,"ARGS");const auto original=Run(true);
#if !defined(AEVRYND_FOREST_POLICY_BEFORE)
    const auto active=Run(false);
    if constexpr(OriginalEntryLayoutActive)Need(active.words==original.words,"DEFAULT_ORIGINAL_UNCHANGED");
    else Need(active.words!=original.words,"PILOT_POLICY_MUST_DIFFER");
#endif
    if(argc==2)original.Save(std::filesystem::path(argv[1]));
    std::cout<<"FOREST_LAYOUT_POLICY_UNIT_PASS checks="<<checks<<" words="<<original.words.size()
        <<" full_world=0 production_assets=0 native=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FOREST_LAYOUT_POLICY_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
