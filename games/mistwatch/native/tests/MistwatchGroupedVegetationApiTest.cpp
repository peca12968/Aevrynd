#include "fallen/MistwatchAuthoredWorldQualification.h"
#include "MistwatchGroupedCanopyVolume.h"
#include <iostream>

using namespace Mistwatch;
namespace Q=::Mistwatch::AuthoredQualification;
namespace G=::Mistwatch::GroupedCanopyVolume;
namespace {
unsigned checks{},negatives{};
void Need(bool value,const char* gate){++checks;if(!value)throw std::runtime_error(gate);}
template<class Action>void Reject(Action&& action,const char* message){bool caught{};
    try{action();}catch(const std::invalid_argument& e){Need(std::string_view(e.what())==message,"NEGATIVE_WRONG_REASON");caught=true;}
    Need(caught,"NEGATIVE_NOT_REJECTED");++negatives;}
struct Hash {
    std::uint64_t value;
    void Word(std::uint32_t n){for(unsigned i=0;i<4;++i){value^=(n>>(8*i))&255u;value*=1099511628211ull;}}
    void Count(std::uint64_t n){Word(static_cast<std::uint32_t>(n));Word(static_cast<std::uint32_t>(n>>32));}
    void Float(float f){Word(std::bit_cast<std::uint32_t>(f));}
    void Vector(Aevrynd::Vec3 v){Float(v.x);Float(v.y);Float(v.z);}
    void Pose(const Aevrynd::Transform3D& p){Vector(p.position);Vector(p.rotation);Vector(p.scale);}
    void Box(const Aevrynd::BoxCollider3D& b){Vector(b.center);Vector(b.extents);}
};
// 固定小型 fixture 的獨立組證據，不呼叫 Measure 來背書同一份輸入。
G::Expected GroupOracle(std::span<const G::Group> groups){Hash h{14695981039346656037ull};
    h.Word(0x47574356u);h.Word(1);h.Count(groups.size());std::size_t total{};
    for(std::size_t i=0;i<groups.size();++i){h.Word(0x47525042u);h.Count(i);h.Count(groups[i].size());
        for(const auto& b:groups[i])h.Box(b);total+=groups[i].size();}
    h.Word(0x454e4447u);h.Count(total);return {groups.size(),total,h.value};}
std::uint64_t WorldOracle(const ForestCoverWorldView& w){Hash h{1469598103934665603ull};h.Word(w.groupedWood?0x52545732u:0x52545731u);
    const auto& s=w.terrain.Settings();h.Vector(s.origin);h.Word(s.width);h.Word(s.depth);h.Float(s.cellSize);h.Count(w.terrain.SampleCount());
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)h.Float(w.terrain.Height(x,z));
    h.Count(w.trees.size());for(auto p:w.trees)h.Vector(p);
    for(const auto* pop:{&w.understory.ferns,&w.understory.saplings}){h.Count(pop->size());for(const auto& p:*pop)h.Pose(p);}
    h.Count(w.solids.size());for(const auto& b:w.solids)h.Box(b);
    if(w.groupedWood){const auto& id=w.groupedWood->Identity();h.Count(id.groupCount);h.Count(id.totalParts);h.Count(id.fingerprint);
        std::size_t ordinal{};for(const auto& group:w.groupedWood->Groups()){h.Count(ordinal++);h.Count(group.size());for(const auto& b:group)h.Box(b);}}
    else{h.Count(w.wood.size());for(const auto& b:w.wood)h.Box(b);}
    return h.value;
}
void SameRetentionPopulation(const ForestCoverRetentionResult& a,const ForestCoverRetentionResult& b){
    Need(Q::Detail::SameCover(a.cover,b.cover),"RETENTION_ALL_PLANT_PATCH_BITS_COUNTERS");
    Need(a.retained==b.retained&&a.rejected==b.rejected&&a.filled==b.filled&&a.provenance.size()==b.provenance.size()&&a.rejections.size()==b.rejections.size(),"RETENTION_COUNTS");
    for(std::size_t i=0;i<a.provenance.size();++i){const auto& x=a.provenance[i];const auto& y=b.provenance[i];
        Need(x.origin==y.origin&&x.sourcePlant==y.sourcePlant&&x.sourcePatch==y.sourcePatch,"RETENTION_PROVENANCE");}
    for(std::size_t i=0;i<a.rejections.size();++i){const auto& x=a.rejections[i];const auto& y=b.rejections[i];
        Need(x.sourcePlant==y.sourcePlant&&x.sourcePatch==y.sourcePatch&&x.gate==y.gate,"RETENTION_REJECTION_ORDER");}
}
void SameFern(const FernColonyResult& a,const FernColonyResult& b){
    Need(Q::Detail::ColonyIdentity(a)==Q::Detail::ColonyIdentity(b),"FERN_COMPLETE_IDENTITY");
    Need(a.plants.size()==b.plants.size()&&a.colonies.size()==b.colonies.size(),"FERN_COUNTS");
    for(std::size_t i=0;i<a.plants.size();++i){const auto& x=a.plants[i];const auto& y=b.plants[i];
        Need(Q::Detail::Same(x.transform,y.transform)&&Q::Detail::Same(x.radius,y.radius)&&x.variant==y.variant&&x.colony==y.colony&&x.grassPatch==y.grassPatch,"FERN_BITS");}
}
void SameShrub(const ShrubCanopyResult& a,const ShrubCanopyResult& b,const ShrubSearchTrace& x,const ShrubSearchTrace& y){
    Need(Q::Detail::ShrubIdentity(a)==Q::Detail::ShrubIdentity(b),"SHRUB_ALL_MESH_ATTACHMENT_POSE_IDENTITY");
    Need(x.records.size()==y.records.size(),"SHRUB_TRACE_COUNT");
    for(std::size_t i=0;i<x.records.size();++i){const auto& p=x.records[i];const auto& q=y.records[i];
        Need(p.attempt==q.attempt&&Q::Detail::Same(p.root,q.root)&&p.variant==q.variant&&Q::Detail::Same(p.scale,q.scale)&&
            Q::Detail::Same(p.radius,q.radius)&&p.stage==q.stage&&p.vegetationMask==q.vegetationMask,"SHRUB_EVERY_ATTEMPT_BITS");}
}
}
int wmain(int argc,wchar_t** argv){try{
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","USAGE_RUNTIME_ASSETS");
    const auto assets=Q::LoadAssets(std::filesystem::path(argv[2]));
    Aevrynd::HeightField3D terrain({{-40,0,-20},161,241,.5f});terrain.Fill(0);
    ForestUnderstory understory;const std::vector<Aevrynd::Vec3> trees;
    const std::vector<Aevrynd::BoxCollider3D> solids;
    const std::array<Aevrynd::BoxCollider3D,1> distantWood{{{{1000,1,1000},{1,1,1}}}};
    const std::array<G::Group,1> one{{G::Group(distantWood)}};const G::ValidatedWood grouped(one,GroupOracle(one));
    ForestCoverSettings settings;settings.targetPlants=24;settings.maximumPatches=12;settings.attemptsPerPatch=64;settings.independentAnchors=16;
    const auto canonical=CreateForestCover(terrain,trees,understory,solids,assets.grassAssets,settings,distantWood,true);
    Need(canonical.reachedBudget&&canonical.plants.size()==24,"NONEMPTY_CANONICAL_FIXTURE");
    ForestCoverWorldView original{terrain,trees,understory,solids,distantWood,distantWood.size(),0};original.expectedInputFingerprint=WorldOracle(original);
    auto finalFlat=original;ForestCoverWorldView finalGrouped{terrain,trees,understory,solids,{},distantWood.size(),0,&grouped};
    finalGrouped.expectedInputFingerprint=WorldOracle(finalGrouped);
    Need(ForestCoverRetentionWorldFingerprint(original)==original.expectedInputFingerprint,"RTW1_UNCHANGED_ORACLE");
    Need(ForestCoverRetentionWorldFingerprint(finalGrouped)==finalGrouped.expectedInputFingerprint&&
        finalGrouped.expectedInputFingerprint!=original.expectedInputFingerprint,"RTW2_EXPLICIT_ORACLE");
    const auto keptFlat=CreateForestCoverRetained(canonical,original,finalFlat,assets.grassAssets,assets.grassIdentities,settings);
    const auto keptGroup=CreateForestCoverRetained(canonical,original,finalGrouped,assets.grassAssets,assets.grassIdentities,settings);
    SameRetentionPopulation(keptFlat,keptGroup);Need(keptFlat.retained==24&&keptGroup.cover.reachedBudget,"RETENTION_POSITIVE_NONVACUOUS");
    Need(keptFlat.finalInputFingerprint!=keptGroup.finalInputFingerprint&&keptFlat.fingerprint!=keptGroup.fingerprint,"NEW_PROVENANCE_DOMAIN");
    FernColonySettings fernSettings;fernSettings.targetPlants=1;fernSettings.maximumColonies=12;fernSettings.attemptsPerColony=64;
    const auto fernDefault=CreateFernColonies(terrain,canonical,{},solids,assets.fernAssets,fernSettings);
    const auto fernExplicit=CreateFernColonies(terrain,canonical,{},solids,assets.fernAssets,fernSettings,{},false,nullptr);SameFern(fernDefault,fernExplicit);
    const auto fernFlat=CreateFernColonies(terrain,canonical,{},solids,assets.fernAssets,fernSettings,distantWood,true);
    const auto fernGroup=CreateFernColonies(terrain,canonical,{},solids,assets.fernAssets,fernSettings,{},true,&grouped);
    SameFern(fernFlat,fernGroup);Need(fernFlat.reachedBudget&&!fernFlat.plants.empty(),"FERN_NONEMPTY_EQUIVALENCE");
    ShrubSearchTrace traceFlat,traceGroup,traceDefault,traceExplicit;
    const auto shrubDefault=CreateShrubCanopy(assets.shrubLeaf,terrain,solids,understory,{}, {},assets.fernAssets,4,{},false,&traceDefault);
    const auto shrubExplicit=CreateShrubCanopy(assets.shrubLeaf,terrain,solids,understory,{}, {},assets.fernAssets,4,{},false,&traceExplicit,nullptr);
    SameShrub(shrubDefault,shrubExplicit,traceDefault,traceExplicit);
    const auto shrubFlat=CreateShrubCanopy(assets.shrubLeaf,terrain,solids,understory,{}, {},assets.fernAssets,4,distantWood,true,&traceFlat);
    const auto shrubGroup=CreateShrubCanopy(assets.shrubLeaf,terrain,solids,understory,{}, {},assets.fernAssets,4,{},true,&traceGroup,&grouped);
    SameShrub(shrubFlat,shrubGroup,traceFlat,traceGroup);Need(shrubFlat.reachedBudget&&shrubFlat.plants.size()==4,"SHRUB_NONEMPTY_EQUIVALENCE");
    // 最後一組完整包住候選域；必須實際拒絕，而非只驗群組身分。
    const std::array<Aevrynd::BoxCollider3D,1> blocking{{{{0,0,40},{50,20,60}}}};
    const std::array<G::Group,2> two{{G::Group(distantWood),G::Group(blocking)}};const G::ValidatedWood lastBlocked(two,GroupOracle(two));
    auto blockedView=finalGrouped;blockedView.groupedWood=&lastBlocked;blockedView.expectedWoodCount=2;blockedView.expectedInputFingerprint=WorldOracle(blockedView);
    const auto blockedGrass=CreateForestCoverRetained(canonical,original,blockedView,assets.grassAssets,assets.grassIdentities,settings);
    Need(blockedGrass.cover.plants.empty()&&blockedGrass.rejected==24&&!blockedGrass.cover.reachedBudget,"LAST_GROUP_RETENTION_PARTICIPATES");
    for(const auto& rejected:blockedGrass.rejections)Need(rejected.gate==ForestCoverRetentionGate::Volume,"LAST_GROUP_RETENTION_VOLUME_GATE");
    const auto blockedFern=CreateFernColonies(terrain,canonical,{},solids,assets.fernAssets,fernSettings,{},true,&lastBlocked);
    Need(blockedFern.plants.empty()&&!blockedFern.reachedBudget&&blockedFern.rejectedClearance>0,"LAST_GROUP_FERN_PARTICIPATES");
    ShrubSearchTrace blockedTrace;
    const auto blockedShrub=CreateShrubCanopy(assets.shrubLeaf,terrain,solids,understory,{}, {},assets.fernAssets,1,{},true,&blockedTrace,&lastBlocked);
    Need(blockedShrub.plants.empty()&&!blockedShrub.reachedBudget&&blockedShrub.attempted==24000,"LAST_GROUP_SHRUB_PARTICIPATES");
    Need(std::any_of(blockedTrace.records.begin(),blockedTrace.records.end(),[](const auto& r){return r.stage==ShrubSearchStage::CanopyWood;}),"LAST_GROUP_SHRUB_STAGE");
    // 零人口仍先拒絕混用、失效、未啟用完整體積，以及 original 冒用群組。
    auto movedFrom=grouped;const auto movedTo=std::move(movedFrom);Need(movedTo.Identity()==grouped.Identity(),"MOVED_FIXTURE");
    FernColonySettings zeroFern=fernSettings;zeroFern.targetPlants=0;auto zeroCover=settings;zeroCover.targetPlants=0;
    Reject([&]{(void)CreateFernColonies(terrain,{}, {},solids,assets.fernAssets,zeroFern,distantWood,true,&grouped);},"Invalid grouped fern canopy wood");
    Reject([&]{(void)CreateFernColonies(terrain,{}, {},solids,assets.fernAssets,zeroFern,{},false,&grouped);},"Invalid grouped fern canopy wood");
    Reject([&]{(void)CreateFernColonies(terrain,{}, {},solids,assets.fernAssets,zeroFern,{},true,&movedFrom);},"Invalid grouped fern canopy wood");
    Reject([&]{(void)CreateShrubCanopy(assets.shrubLeaf,terrain,solids,understory,{}, {},assets.fernAssets,0,distantWood,true,nullptr,&grouped);},"Invalid grouped shrub canopy wood");
    Reject([&]{(void)CreateShrubCanopy(assets.shrubLeaf,terrain,solids,understory,{}, {},assets.fernAssets,0,{},false,nullptr,&grouped);},"Invalid grouped shrub canopy wood");
    Reject([&]{(void)CreateShrubCanopy(assets.shrubLeaf,terrain,solids,understory,{}, {},assets.fernAssets,0,{},true,nullptr,&movedFrom);},"Invalid grouped shrub canopy wood");
    auto bad=finalGrouped;bad.wood=distantWood;
    Reject([&]{(void)CreateForestCoverRetained({},original,bad,assets.grassAssets,assets.grassIdentities,zeroCover);},"Invalid grouped retention wood");
    Reject([&]{(void)ForestCoverRetentionWorldFingerprint(bad);},"Invalid grouped retention wood");
    auto badMoved=finalGrouped;badMoved.groupedWood=&movedFrom;
    Reject([&]{(void)CreateForestCoverRetained({},original,badMoved,assets.grassAssets,assets.grassIdentities,zeroCover);},"Invalid grouped retention wood");
    Reject([&]{(void)ForestCoverRetentionWorldFingerprint(badMoved);},"Invalid grouped retention wood");
    Reject([&]{(void)CreateForestCoverRetained({},finalGrouped,finalGrouped,assets.grassAssets,assets.grassIdentities,zeroCover);},"Retention original world must use flat canopy wood");
    auto badCount=finalGrouped;badCount.expectedWoodCount=2;
    Reject([&]{(void)CreateForestCoverRetained({},original,badCount,assets.grassAssets,assets.grassIdentities,zeroCover);},"Retention world identity or finite budget mismatch");
    auto badFingerprint=finalGrouped;badFingerprint.expectedInputFingerprint=original.expectedInputFingerprint;
    Reject([&]{(void)CreateForestCoverRetained({},original,badFingerprint,assets.grassAssets,assets.grassIdentities,zeroCover);},"Retention complete world fingerprint mismatch");
    auto forged=canonical;forged.plants[0].transform.position.x=std::nextafter(forged.plants[0].transform.position.x,INFINITY);
    Reject([&]{(void)CreateForestCoverRetained(forged,original,finalGrouped,assets.grassAssets,assets.grassIdentities,settings);},"Retention canonical source mismatch or shortfall");
    // 同一扁平順序、不同分組邊界仍有不同 RTW2 身分。
    const std::array<Aevrynd::BoxCollider3D,3> boxes{{distantWood[0],{{1002,1,1000},{1,1,1}},{{1004,1,1000},{1,1,1}}}};
    const std::array<G::Group,2> splitA{{G::Group(boxes).first(1),G::Group(boxes).subspan(1)}},splitB{{G::Group(boxes).first(2),G::Group(boxes).subspan(2)}};
    const G::ValidatedWood boundaryA(splitA,GroupOracle(splitA)),boundaryB(splitB,GroupOracle(splitB));
    auto a=finalGrouped,b=finalGrouped;a.groupedWood=&boundaryA;b.groupedWood=&boundaryB;
    Need(ForestCoverRetentionWorldFingerprint(a)!=ForestCoverRetentionWorldFingerprint(b),"RTW2_GROUP_BOUNDARY_PRESERVED");
    std::array<G::Group,12> twelve;for(auto& g:twelve)g=distantWood;const G::ValidatedWood maxGroups(twelve,GroupOracle(twelve));
    Need(CreateFernColonies(terrain,{}, {},solids,assets.fernAssets,zeroFern,{},true,&maxGroups).reachedBudget,"TWELVE_GROUP_ENTRY_ACCEPTED");
    std::array<G::Group,13> thirteen;for(auto& g:thirteen)g=distantWood;
    Reject([&]{(void)G::ValidatedWood(thirteen,{13,13,0});},"GROUPED_CANOPY_VOLUME_GROUP_COUNT");
    std::cout<<"GROUPED_VEGETATION_API_CPU_PASS checks="<<checks<<" negative_count="<<negatives
        <<" retained="<<keptGroup.retained<<" fern="<<fernGroup.plants.size()<<" shrub="<<shrubGroup.plants.size()
        <<" rtw1="<<original.expectedInputFingerprint<<" rtw2="<<finalGrouped.expectedInputFingerprint
        <<" full_world_qualified=false native_passed=false\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
