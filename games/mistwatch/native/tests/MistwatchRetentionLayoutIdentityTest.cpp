#include "MistwatchForestCoverRetention.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace Mistwatch;using namespace Aevrynd;
unsigned checks{},negatives{};
void Need(bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);}
template<class T>void Reject(T action,const char* reason){bool rejected=false;try{action();}catch(const std::invalid_argument& e){Need(std::string_view(e.what())==reason,"WRONG_REJECTION");rejected=true;}
    Need(rejected,"EXPECTED_REJECTION");++negatives;std::cout<<"RETENTION_LAYOUT_NEGATIVE "<<reason<<'\n';}
// 由E4033獨立Python對固定flat微型fixture計算，非呼叫被測函式現填expected。
constexpr std::uint64_t OriginalFlat=15370971616063197296ull,OriginalGrouped=6901475789620038029ull;
constexpr std::uint64_t PilotFlat=1671050180195275871ull,PilotGrouped=3187562836263462882ull,GroupFingerprint=12787746466605821483ull;
constexpr std::array<std::uint64_t,5> AssetIds{0x84C8C7DA8EEB335Bull,0xCE3EB8A245F7C366ull,0x0AA49B54653A3B69ull,0x78ED34130EAC919Eull,0x6B95323ACE660F30ull};
struct Export {
    std::vector<std::uint32_t> words;
    void Word(std::uint32_t n){words.push_back(n);}void Wide(std::uint64_t n){Word(std::uint32_t(n));Word(std::uint32_t(n>>32));}
    void Float(float n){Need(std::isfinite(n),"FINITE_OUTPUT");Word(std::bit_cast<std::uint32_t>(n));}
    void Vec(Vec3 p){Float(p.x);Float(p.y);Float(p.z);}void Pose(const Transform3D& p){Vec(p.position);Vec(p.rotation);Vec(p.scale);}
    void Cover(const ForestCoverResult& r){Word(unsigned(r.plants.size()));Word(unsigned(r.patches.size()));Word(r.rejectedClearance);Word(r.rejectedTerrain);Word(r.rejectedSpacing);Word(r.reachedBudget);
        for(const auto& p:r.plants){Pose(p.transform);Word(p.variant);Word(p.patch);Float(p.radius);}
        for(const auto& p:r.patches){Vec(p.centre);Float(p.radius);Float(p.aspect);Float(p.yaw);Word(p.requested);Word(p.accepted);}}
    void Retention(const ForestCoverRetentionResult& r){Cover(r.cover);Word(r.retained);Word(r.rejected);Word(r.filled);
        Wide(r.originalInputFingerprint);Wide(r.finalInputFingerprint);Wide(r.provenanceFingerprint);Wide(r.fingerprint);
        Word(unsigned(r.provenance.size()));for(const auto& p:r.provenance){Word(unsigned(p.origin));Word(p.sourcePlant);Word(p.sourcePatch);}
        Word(unsigned(r.rejections.size()));for(const auto& p:r.rejections){Word(p.sourcePlant);Word(p.sourcePatch);Word(unsigned(p.gate));}}
    void Save(const std::filesystem::path& p)const{Need(p.is_absolute()&&!std::filesystem::exists(p),"FRESH_OUTPUT");std::ofstream out(p,std::ios::binary);
        for(auto n:words)for(unsigned b=0;b<4;++b)out.put(char((n>>(b*8))&255u));out.close();Need(bool(out),"COMPLETE_WRITE");}
};
using Token=ValidatedForestCoverOriginal;
ForestCoverWorldView View(const HeightField3D& terrain,const ForestUnderstory& u,std::span<const BoxCollider3D> wood,
    const GroupedCanopyVolume::ValidatedWood* grouped=nullptr,bool pilot=false){
    ForestCoverWorldView w{terrain,{},u,{},wood,1,pilot?(grouped?PilotGrouped:PilotFlat):(grouped?OriginalGrouped:OriginalFlat),grouped};
#if defined(AEVRYND_RETENTION_LAYOUT_BEFORE)
    Need(!pilot&&OriginalEntryLayoutActive,"BEFORE_ORIGINAL_ONLY");
#else
    w.layoutId=pilot?EntryLayoutId::WestSupplyPilot:EntryLayoutId::Original;
#endif
    return w;
}
Token Make(const HeightField3D& terrain,const ForestUnderstory& u,std::span<const BoxCollider3D> wood,
    const std::array<ForestCoverAsset,5>& assets,const ForestCoverSettings& settings){
#if defined(AEVRYND_RETENTION_LAYOUT_BEFORE)
    return Token::Create(terrain,{},u,{},assets,AssetIds,settings,wood);
#else
    return Token::Create(terrain,{},u,{},assets,AssetIds,settings,wood,EntryLayoutId::Original);
#endif
}
void Run(const std::filesystem::path& grassDir,const std::filesystem::path& output){
    Need(output.empty()||(output.is_absolute()&&!std::filesystem::exists(output)&&std::filesystem::create_directory(output)),"FRESH_EXPORT_DIRECTORY");
    std::array<ForestCoverAsset,5> assets;
    for(unsigned i=0;i<5;++i){auto loaded=LoadAevryndMeshFile(grassDir/GrassMeshName(i));Need(bool(loaded),"REAL_GRASS_LOAD");NormalizeGrassAsset(loaded.mesh);assets[i]=ForestCoverAssetFromMesh(loaded.mesh);}
    HeightField3D terrain({{-40,0,-20},161,241,.5f});terrain.Fill(0);ForestUnderstory u;
    const std::array<BoxCollider3D,1> wood{{{{1000,1,1000},{1,1,1}}}};
    const std::array<GroupedCanopyVolume::Group,1> groups{GroupedCanopyVolume::Group(wood)};
    const GroupedCanopyVolume::ValidatedWood grouped(groups,{1,1,GroupFingerprint});
    const auto original=View(terrain,u,wood),originalGroup=View(terrain,u,{},&grouped);
    Need(ForestCoverRetentionWorldFingerprint(original)==OriginalFlat&&ForestCoverRetentionWorldFingerprint(originalGroup)==OriginalGrouped,"INDEPENDENT_ORIGINAL_WORLD_ORACLES");
    ForestCoverSettings settings;settings.targetPlants=24;settings.maximumPatches=12;settings.attemptsPerPatch=64;settings.independentAnchors=16;
    const auto token=Make(terrain,u,wood,assets,settings);const auto& canonical=token.Original();Need(canonical.reachedBudget&&canonical.plants.size()==24,"NONEMPTY_ORIGINAL_SOURCE");
    const auto plain=CreateForestCoverRetained(canonical,original,original,assets,AssetIds,settings);
    const auto cached=CreateForestCoverRetainedFromValidatedOriginal(token,canonical,original,original,assets,AssetIds,settings);
    Export a,b;a.Retention(plain);b.Retention(cached);Need(a.words==b.words,"LEGACY_AND_TOKEN_ALL_FIELDS_EQUAL");
    Need(cached.retained==24&&cached.rejected==0&&cached.filled==0,"ORIGINAL_POPULATION_PRESERVED");
    Export baseline;baseline.Word(0x524c4931u);baseline.Cover(canonical);baseline.Retention(cached);
    if(!output.empty())baseline.Save(output/"original.bin");
    Reject([&]{Token{}.Validate(canonical,original,assets,AssetIds,settings);},"RETENTION_ORIGINAL_TOKEN_REQUIRED");
    auto moving=token;const auto moved=std::move(moving);moved.Validate(canonical,original,assets,AssetIds,settings);
    Reject([&]{moving.Validate(canonical,original,assets,AssetIds,settings);},"RETENTION_ORIGINAL_TOKEN_REQUIRED");
    auto changedIds=AssetIds;++changedIds[0];Reject([&]{token.Validate(canonical,original,assets,changedIds,settings);},"RETENTION_ORIGINAL_INPUT_CHANGED");
    auto corrupt=canonical;corrupt.plants[0].transform.position.x=std::nextafter(corrupt.plants[0].transform.position.x,100.f);
    Reject([&]{token.Validate(corrupt,original,assets,AssetIds,settings);},"Retention canonical source mismatch or shortfall");
#if !defined(AEVRYND_RETENTION_LAYOUT_BEFORE)
    const auto pilot=View(terrain,u,wood,nullptr,true),pilotGroup=View(terrain,u,{},&grouped,true);
    Need(ForestCoverRetentionWorldFingerprint(pilot)==PilotFlat&&ForestCoverRetentionWorldFingerprint(pilotGroup)==PilotGrouped,"INDEPENDENT_PILOT_WORLD_ORACLES");
    Need(OriginalFlat!=PilotFlat&&OriginalGrouped!=PilotGrouped,"SAME_GEOMETRY_DIFFERENT_POLICY_IDENTITY");
    const auto transition=CreateForestCoverRetainedFromValidatedOriginal(token,canonical,original,pilot,assets,AssetIds,settings);
    const auto flatPlain=CreateForestCoverRetained(canonical,original,pilot,assets,AssetIds,settings);
    Export t1,t2;t1.Retention(transition);t2.Retention(flatPlain);Need(t1.words==t2.words,"TRANSITION_PLAIN_TOKEN_ALL_FIELDS_EQUAL");
    const auto transitionGroup=CreateForestCoverRetainedFromValidatedOriginal(token,canonical,original,pilotGroup,assets,AssetIds,settings);
    Export c1,c2;c1.Cover(transition.cover);c2.Cover(transitionGroup.cover);Need(c1.words==c2.words,"FLAT_GROUPED_SAME_GEOMETRY_POLICY_RESULT");
    Need(transition.cover.reachedBudget&&transition.cover.plants.size()==24&&transitionGroup.cover.reachedBudget,"TRANSITION_FINITE_QUOTA");
    Need(transition.provenanceFingerprint!=cached.provenanceFingerprint&&transitionGroup.provenanceFingerprint!=transition.provenanceFingerprint,"VERSIONED_PROVENANCE_DIFFERS");
    for(const auto& plant:transition.cover.plants)Need(ForestCoverClearance(plant.transform.position,plant.radius,{},WestSupplyPilotLayout),"FINAL_PILOT_CLEARANCE");
    // 同一canonical人口、幾何、資產，只換具正確expected的來源身份，必須撞token身份門。
    Reject([&]{token.Validate(canonical,pilot,assets,AssetIds,settings);},"RETENTION_ORIGINAL_INPUT_CHANGED");
    Reject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(token,canonical,pilot,pilot,assets,AssetIds,settings);},"RETENTION_ORIGINAL_INPUT_CHANGED");
    const auto pilotToken=Token::Create(terrain,{},u,{},assets,AssetIds,settings,wood,EntryLayoutId::WestSupplyPilot);
    Reject([&]{pilotToken.Validate(pilotToken.Original(),original,assets,AssetIds,settings);},"RETENTION_ORIGINAL_INPUT_CHANGED");
    auto stale=pilot;stale.expectedInputFingerprint=OriginalFlat;
    Reject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(token,canonical,original,stale,assets,AssetIds,settings);},"Retention complete world fingerprint mismatch");
    auto unknown=original;unknown.layoutId=static_cast<EntryLayoutId>(99);
    Reject([&]{(void)ForestCoverRetentionWorldFingerprint(unknown);},"Unknown entry layout identity");
    auto zero=settings;zero.targetPlants=0;
    Reject([&]{(void)Token::Create(terrain,{},u,{},assets,AssetIds,zero,wood,static_cast<EntryLayoutId>(99));},"Unknown entry layout identity");
    auto nanTerrain=terrain;nanTerrain.SetHeight(0,0,NAN);const auto nan=View(nanTerrain,u,wood);
    Reject([&]{(void)ForestCoverRetentionWorldFingerprint(nan);},"Nonfinite retention input");
    if(!output.empty()){t1.Save(output/"transition-flat.bin");Export g;g.Retention(transitionGroup);g.Save(output/"transition-grouped.bin");}
    std::cout<<"RETENTION_LAYOUT_TRANSITION retained="<<transition.retained<<" rejected="<<transition.rejected<<" filled="<<transition.filled<<" fp="<<transition.fingerprint<<'\n';
#endif
    std::cout<<"RETENTION_LAYOUT_IDENTITY_UNIT_PASS checks="<<checks<<" negatives="<<negatives<<" original_words="<<baseline.words.size()<<" full_world=0 native=0\n";
}
}
int main(int argc,char** argv){try{Need(argc==2||argc==3,"GRASS_DIRECTORY_OPTIONAL_EXPORT");Run(argv[1],argc==3?std::filesystem::path(argv[2]):std::filesystem::path{});return 0;}
catch(const std::exception& e){std::cerr<<"RETENTION_LAYOUT_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
