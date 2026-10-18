#include "../fallen/MistwatchAuthoredWorldQualification.h"
#include "MistwatchShrubGapDensity.h"
#include "MistwatchShrubArcDensity.h"
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
namespace {
unsigned negatives{};
template<class Action>void Reject(Action&& action,const char* name,const char* expected,W::Budget& budget){
    budget.Time();bool rejected{};
    try{std::forward<Action>(action)();}catch(const std::exception& error){
        if(std::string_view(error.what())==expected)rejected=true;
        else throw std::runtime_error(std::string("SHRUB_NEGATIVE_FIRST_DIVERGENCE name=")+name+
            " actual="+error.what()+" expected="+expected);
    }
    budget.Time();Q::Detail::Need(rejected,"SHRUB_DENSITY_NEGATIVE_ACCEPTED");++negatives;
    std::cout<<"CONTROL "<<name<<" rejected=true reason="<<expected<<'\n';
}
std::vector<W::Placement> FixedMappings(){
    // 只提供 production 原四倒木的固定姿態；不冒充完整倒木 overlay 證書。
    std::vector<W::Placement> out;
    for(const auto& p:Mistwatch::FallenLogPlacements()){
        W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;
        out.push_back(std::move(q));
    }
    return out;
}
bool SameVariant(const Mistwatch::ShrubMesh& a,const Mistwatch::ShrubMesh& b){
    if(!Q::Detail::Same(a.stems,b.stems)||!Q::Detail::Same(a.leaves,b.leaves)||
        !Q::Detail::Same(a.radius,b.radius)||!Q::Detail::Same(a.height,b.height)||
        !Q::Detail::Same(a.minimumY,b.minimumY)||!Q::Detail::Same(a.maximumY,b.maximumY)||
        a.leafCount!=b.leafCount||a.stemCount!=b.stemCount||a.attachments.size()!=b.attachments.size())return false;
    for(std::size_t i=0;i<a.attachments.size();++i){const auto& x=a.attachments[i];const auto& y=b.attachments[i];
        if(x.vertex!=y.vertex||!Q::Detail::Same(x.centre,y.centre)||!Q::Detail::Same(x.branchRadius,y.branchRadius))return false;}
    return true;
}
std::uint64_t PlacementFingerprint(std::span<const Mistwatch::ShrubPlant> plants){
    // 與原位置收據同字序，但獨立從實際植物重算；九個姿態分量另以逐 bit prefix 門驗證。
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& p:plants){const auto root=p.pose.position;
        for(const float f:{root.x,root.y,root.z,p.pose.rotation.y,p.pose.scale.x,p.radius}){
            Q::Detail::Need(std::isfinite(f),"SHRUB_DENSITY_NONFINITE_FINGERPRINT");
            hash^=std::bit_cast<std::uint32_t>(f);hash*=1099511628211ull;}
        hash^=p.variant;hash*=1099511628211ull;
    }
    return hash;
}
template<class Values>void Csv(const Values& values){
    bool first=true;for(const auto value:values){if(!first)std::cout<<',';std::cout<<value;first=false;}
}
}
int wmain(int argc,wchar_t** argv){try{
    Q::Detail::Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets",
        "USAGE: MistwatchShrubDensityQualificationTest --runtime-assets ABS");
    const auto root=std::filesystem::path(argv[2]).lexically_normal();
    Q::Detail::Need(root.is_absolute(),"AUTHORED_ASSET_ROOT_ABSOLUTE_REQUIRED");
    W::Budget budget;
    std::cout<<"SHRUB_DENSITY_STAGE stage=load-author-context target=128 native=false\n"<<std::flush;
    const auto assets=Q::LoadAssets(root);const auto terrain=Mistwatch::CreateTerrain();
    const auto source=Q::CreateSourceContext(terrain,assets,budget);const auto scan=FixedMappings();
    const auto context=Q::CreateContext(terrain,assets,source,scan,budget);
    Q::Detail::Need(context.shrubs.reachedBudget&&context.shrubs.plants.size()==64&&context.canopy.plants.size()==8&&
        context.wood.size()==8617&&context.worlds[0].size()==8853&&context.worlds[1].size()==8854,
        "SHRUB_DENSITY_BASELINE_FIXTURE_DOMAIN");
    const auto contextSeal=Q::ContextFingerprint(context),sourceSeal=Q::SourceFingerprint(source),assetSeal=Q::AssetsFingerprint(assets);
    // 歷代不足額是明確負控；不能將舊 90/101 株當成新 128 株資格通過。
    const auto validateLegacy=[&](const Mistwatch::ShrubCanopyResult& legacy,unsigned count,unsigned attempts,
        unsigned clearance,unsigned terrainRejected,unsigned vegetation,const char* profile){
        Q::Detail::Need(!legacy.reachedBudget&&legacy.plants.size()==count&&count<128&&legacy.attempted==attempts&&
            attempts<=24000&&legacy.rejectedClearance==clearance&&legacy.rejectedTerrain==terrainRejected&&
            legacy.rejectedVegetation==vegetation&&count+clearance+terrainRejected+vegetation==attempts,
            "SHRUB_LEGACY_SHORTFALL_ACCOUNTING");
        for(unsigned i=0;i<4;++i)Q::Detail::Need(SameVariant(legacy.variants[i],context.shrubs.variants[i]),
            "SHRUB_LEGACY_VARIANT_CHANGED");
        for(unsigned i=0;i<64;++i){const auto& a=context.shrubs.plants[i];const auto& b=legacy.plants[i];
            Q::Detail::Need(Q::Detail::Same(a.pose,b.pose)&&Q::Detail::Same(a.radius,b.radius)&&a.variant==b.variant,
                "SHRUB_LEGACY_PREFIX_CHANGED");}
        Q::Detail::Need(PlacementFingerprint(std::span<const Mistwatch::ShrubPlant>(legacy.plants).first(64))==context.shrubs.fingerprint&&
            PlacementFingerprint(legacy.plants)==legacy.fingerprint,"SHRUB_LEGACY_FINGERPRINT");
        auto legacyContext=context;legacyContext.shrubs=legacy;
        Q::ValidateDownstreamVolumes(legacyContext,terrain,assets,budget);
        for(unsigned state=0;state<2;++state)Q::ValidateTypedWorld(legacyContext,state);
        std::cout<<"SHRUB_LEGACY_SHORTFALL_VERIFIED profile="<<profile<<" target=128 actual="<<count
            <<" attempts="<<attempts<<" fingerprint="<<legacy.fingerprint<<" candidate128_passed=false\n"<<std::flush;
    };
    const auto legacyGenerate=[&](bool gap){
        budget.Time();
        auto result=gap?Mistwatch::CreateShrubGapDensity(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
            context.retention.cover,context.colonies,assets.fernAssets,128,context.wood,true):
            Mistwatch::CreateShrubCanopy(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
                context.retention.cover,context.colonies,assets.fernAssets,128,context.wood,true);
        budget.Time();return result;
    };
    for(bool gap:{false,true}){
        const auto legacy=legacyGenerate(gap),repeat=legacyGenerate(gap);
        Q::Detail::Need(Q::Detail::SameShrubs(legacy,repeat)&&Q::Detail::ShrubIdentity(legacy)==Q::Detail::ShrubIdentity(repeat),
            "SHRUB_LEGACY_REPEAT_CHANGED");
        if(gap)validateLegacy(legacy,101,22809,10529,6113,6066,"woodland-gap-density-v2");
        else validateLegacy(legacy,90,24000,11055,6486,6369,"woodland-density-v1");
    }
    const auto generate=[&](unsigned target,Mistwatch::ShrubArcTrace* trace=nullptr){budget.Time();
        auto out=Mistwatch::CreateShrubArcDensity(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
            context.retention.cover,context.colonies,assets.fernAssets,target,context.wood,true,trace);
        budget.Time();return out;};
    std::cout<<"SHRUB_DENSITY_STAGE stage=generate-production-feasible-arc-v3 target=128 native=false\n"<<std::flush;
    Mistwatch::ShrubArcTrace originalTrace;
    const auto legacyArcStart=std::chrono::steady_clock::now();
    const auto candidate=generate(128,&originalTrace);
    const auto legacyArcTime=std::chrono::steady_clock::now()-legacyArcStart;
    const auto candidateDiagnostics=Mistwatch::LastShrubArcDiagnostics;
    const auto validateAccounting=[&](const Mistwatch::ShrubCanopyResult& plants,const Mistwatch::ShrubArcDiagnostics& d){
        Q::Detail::Need(d.prefixAttempts==context.shrubs.attempted&&d.prefixAttempts+d.generated<=24000&&
            d.prefixAttempts+d.evaluated==plants.attempted&&d.evaluated<=d.generated&&
            plants.attempted==plants.plants.size()+plants.rejectedClearance+plants.rejectedTerrain+plants.rejectedVegetation,
            "SHRUB_ARC_GENERATED_EVALUATED_ACCOUNTING");
    };
    validateAccounting(candidate,candidateDiagnostics);
    std::cout<<"SHRUB_DENSITY_GENERATED target=128 actual="<<candidate.plants.size()<<" reached="<<candidate.reachedBudget
        <<" attempts="<<candidate.attempted<<" clearance="<<candidate.rejectedClearance<<" terrain="<<candidate.rejectedTerrain
        <<" vegetation="<<candidate.rejectedVegetation<<" native=false\n"<<std::flush;
    Q::Detail::Need(candidate.reachedBudget&&candidate.plants.size()==128,"SHRUB_DENSITY_128_SHORTFALL");
    Q::Detail::Need(candidate.attempted<=24000&&candidate.attempted>context.shrubs.attempted,
        "SHRUB_DENSITY_ATTEMPT_BOUND_CHANGED");
    for(unsigned i=0;i<4;++i)Q::Detail::Need(SameVariant(candidate.variants[i],context.shrubs.variants[i]),
        "SHRUB_DENSITY_ORIGINAL_VARIANT_BITS_CHANGED");
    for(std::size_t i=0;i<64;++i){budget.Op();const auto& a=context.shrubs.plants[i];const auto& b=candidate.plants[i];
        Q::Detail::Need(Q::Detail::Same(a.pose,b.pose)&&Q::Detail::Same(a.radius,b.radius)&&a.variant==b.variant,
            "SHRUB_DENSITY_ORIGINAL_64_PREFIX_CHANGED");}
    const auto prefixHash=PlacementFingerprint(std::span<const Mistwatch::ShrubPlant>(candidate.plants).first(64));
    Q::Detail::Need(prefixHash==context.shrubs.fingerprint&&PlacementFingerprint(candidate.plants)==candidate.fingerprint,
        "SHRUB_DENSITY_INDEPENDENT_PLACEMENT_FP");
    std::cout<<"SHRUB_DENSITY_STAGE stage=repeat-and-two-bridge-volumes native=false\n"<<std::flush;
    const auto repeat=generate(128);
    const auto repeatDiagnostics=Mistwatch::LastShrubArcDiagnostics;
    validateAccounting(repeat,repeatDiagnostics);
    Q::Detail::Need(candidateDiagnostics.generated==repeatDiagnostics.generated&&candidateDiagnostics.evaluated==repeatDiagnostics.evaluated&&
        candidateDiagnostics.proposalFingerprint==repeatDiagnostics.proposalFingerprint,"SHRUB_ARC_PROPOSAL_REPEAT_CHANGED");
    Q::Detail::Need(Q::Detail::SameShrubs(candidate,repeat)&&
        Q::Detail::ShrubIdentity(candidate)==Q::Detail::ShrubIdentity(repeat),"SHRUB_DENSITY_FULL_REPEAT_CHANGED");
    auto candidateContext=context;candidateContext.shrubs=candidate;
    // 不呼叫固定 64 配額的 ValidateContext；新版數量僅在此獨立候選測試驗證。
    Q::ValidateDownstreamVolumes(candidateContext,terrain,assets,budget);
    Q::Detail::Need(Q::Detail::SameCover(candidateContext.retention.cover,context.retention.cover)&&
        Q::Detail::SameRetention(candidateContext.retention,context.retention)&&
        Q::Detail::SameColonies(candidateContext.colonies,context.colonies)&&
        Q::Detail::SameLitter(candidateContext.litter,context.litter),"SHRUB_DENSITY_OTHER_POPULATIONS_CHANGED");
    for(unsigned state=0;state<2;++state)Q::ValidateTypedWorld(candidateContext,state);

    // 每個負控只接受精確的語義理由；預算、時間、IO 或不合法 fixture 不能當成通過。
    Reject([&]{(void)generate(129);},"target-129","Invalid shrub bounded population",budget);
    auto badGrass=context.retention.cover;badGrass.plants[0].radius=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)Mistwatch::CreateShrubArcDensity(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        badGrass,context.colonies,assets.fernAssets,128,context.wood,true);},
        "grass-input-nan","Invalid shrub existing grass footprint",budget);
    auto badSolids=context.vegetation[0];badSolids[0].extents.y=-1.f;
    Reject([&]{(void)Mistwatch::CreateShrubArcDensity(assets.shrubLeaf,terrain,badSolids,context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,128,context.wood,true);},
        "solid-negative-extent","Invalid shrub solid",budget);
    auto corrupt=candidateContext;corrupt.shrubs.plants[64].variant=4;
    Reject([&]{Q::ValidateDownstreamVolumes(corrupt,terrain,assets,budget);},
        "added-shrub-variant","AUTHORED_SHRUB_VARIANT",budget);
    corrupt=candidateContext;auto& moved=corrupt.shrubs.plants[64].pose.position;
    moved.y=std::nextafter(moved.y,std::numeric_limits<float>::infinity());
    Reject([&]{Q::ValidateDownstreamVolumes(corrupt,terrain,assets,budget);},
        "added-shrub-position-one-ulp","AUTHORED_SHRUB_FOOTPRINT",budget);
    corrupt=candidateContext;corrupt.shrubs.plants[64].pose.position.y+=.125f;
    Reject([&]{Q::ValidateDownstreamVolumes(corrupt,terrain,assets,budget);},
        "added-shrub-root-height","AUTHORED_SHRUB_FOOTPRINT",budget);
    corrupt=candidateContext;corrupt.shrubs.plants[64].radius=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{Q::ValidateDownstreamVolumes(corrupt,terrain,assets,budget);},
        "added-shrub-radius-nan","AUTHORED_SHRUB_FOOTPRINT",budget);
    corrupt=candidateContext;corrupt.shrubs.plants[127]=corrupt.shrubs.plants[0];
    Reject([&]{Q::ValidateDownstreamVolumes(corrupt,terrain,assets,budget);},
        "added-shrub-duplicates-old-root","AUTHORED_SHRUB_SELF_SPACING",budget);
    corrupt=candidateContext;const auto newRoot=corrupt.shrubs.plants[64].pose.position;
    corrupt.vegetation[1].push_back({newRoot,{.01f,2.f,.01f}});
    Reject([&]{Q::ValidateDownstreamVolumes(corrupt,terrain,assets,budget);},
        "added-shrub-repaired-world-obstacle","AUTHORED_SHRUB_TWO_WORLD_VOLUME",budget);
    Q::Detail::Need(negatives==9,"SHRUB_DENSITY_NEGATIVE_COUNT");

    // 新工廠的原 64 株必須比對獨立建立的 context；不能用候選截取自證。
    using Prefix=Mistwatch::ValidatedShrubArcPrefix;
    static_assert(std::is_same_v<decltype(std::declval<const Prefix&>().Original()),const Mistwatch::ShrubCanopyResult&>);
    static_assert(!std::is_constructible_v<Prefix,Mistwatch::ShrubCanopyResult>);
    const auto prefixStart=std::chrono::steady_clock::now();
    const auto prefix=Prefix::CreateOriginal64(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);
    const auto prefixTime=std::chrono::steady_clock::now()-prefixStart;
    Q::Detail::Need(Q::Detail::SameShrubs(prefix.Original(),context.shrubs)&&
        Q::Detail::ShrubIdentity(prefix.Original())==Q::Detail::ShrubIdentity(context.shrubs),"SHRUB_PREFIX_INDEPENDENT_ORIGINAL_CHANGED");
    const auto reuse=[&](const Prefix& token,const Mistwatch::ShrubCanopyResult& original,unsigned target,
        Mistwatch::ShrubArcTrace* trace=nullptr){return Mistwatch::CreateShrubArcDensityFromValidatedPrefix(token,original,
            assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,context.retention.cover,
            context.colonies,assets.fernAssets,target,context.wood,true,trace);};
    Mistwatch::ShrubArcTrace reusedTrace;
    const auto reusedStart=std::chrono::steady_clock::now();
    const auto reused=reuse(prefix,context.shrubs,128,&reusedTrace);
    const auto reusedTime=std::chrono::steady_clock::now()-reusedStart;
    const auto reusedDiagnostics=Mistwatch::LastShrubArcDiagnostics;
    const auto sameDiagnostics=[](const Mistwatch::ShrubArcDiagnostics& a,const Mistwatch::ShrubArcDiagnostics& b){
        return a.prefixAttempts==b.prefixAttempts&&a.generated==b.generated&&a.evaluated==b.evaluated&&
            a.consideredAnchors==b.consideredAnchors&&a.domainSkipped==b.domainSkipped&&a.computedAnchors==b.computedAnchors&&
            a.truncatedAnchors==b.truncatedAnchors&&a.maximumNeighbors==b.maximumNeighbors&&a.operations==b.operations&&
            a.neighborChecks==b.neighborChecks&&a.geometryOperations==b.geometryOperations&&a.proposalFingerprint==b.proposalFingerprint;
    };
    Q::Detail::Need(Q::Detail::SameShrubs(candidate,reused)&&Q::Detail::ShrubIdentity(candidate)==Q::Detail::ShrubIdentity(reused)&&
        sameDiagnostics(candidateDiagnostics,reusedDiagnostics)&&originalTrace.candidates.size()==reusedTrace.candidates.size(),
        "SHRUB_PREFIX_ARC_BITS_OR_DIAGNOSTICS_CHANGED");
    for(std::size_t i=0;i<originalTrace.candidates.size();++i){const auto& a=originalTrace.candidates[i];const auto& b=reusedTrace.candidates[i];
        Q::Detail::Need(a.id==b.id&&a.accepted==b.accepted&&Q::Detail::Same(a.plant.pose,b.plant.pose)&&
            Q::Detail::Same(a.plant.radius,b.plant.radius)&&a.plant.variant==b.plant.variant,"SHRUB_PREFIX_TRACE_BITS_CHANGED");}
    (void)Mistwatch::ShrubCompositionContract::PreservedPrefix128(context.shrubs,reused);
    const auto reused64=reuse(prefix,context.shrubs,64);
    Q::Detail::Need(Q::Detail::SameShrubs(reused64,context.shrubs),"SHRUB_PREFIX_64_RETURN_CHANGED");
    const auto copied=prefix;
    copied.Validate(context.shrubs,assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);
    auto movedFrom=prefix;const auto movedToken=std::move(movedFrom);
    movedToken.Validate(context.shrubs,assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);
    unsigned prefixNegatives{};
    const auto rejectPrefix=[&](auto&& action,const char* name,const char* expected){
        const auto oldCount=negatives;Reject(std::forward<decltype(action)>(action),name,expected,budget);
        negatives=oldCount;++prefixNegatives;
    };
    rejectPrefix([&]{(void)reuse(Prefix{},context.shrubs,128);},"prefix-default-token","SHRUB_ARC_PREFIX_TOKEN_REQUIRED");
    rejectPrefix([&]{(void)reuse(movedFrom,context.shrubs,128);},"prefix-moved-token","SHRUB_ARC_PREFIX_TOKEN_REQUIRED");
    rejectPrefix([&]{(void)reuse(prefix,context.shrubs,63);},"prefix-target-63","SHRUB_ARC_PREFIX_TARGET");
    rejectPrefix([&]{(void)reuse(prefix,context.shrubs,129);},"prefix-target-129","SHRUB_ARC_PREFIX_TARGET");
    Mistwatch::ShrubArcTrace occupiedTrace;occupiedTrace.candidates.push_back({});
    rejectPrefix([&]{(void)reuse(prefix,context.shrubs,128,&occupiedTrace);},"prefix-trace-not-empty","SHRUB_GAP_TRACE_NOT_EMPTY");
    // 逐一更動原前綴的所有資料種類，連同未納入放置雜湊的姿態分量與網格屬性。
    const auto corruptPrefix=[&](auto&& change,const char* name){auto original=context.shrubs;change(original);
        rejectPrefix([&]{(void)reuse(prefix,original,128);},name,"SHRUB_ARC_PREFIX_BITS_CHANGED");};
    corruptPrefix([](auto& a){a.plants[0].pose.rotation.x=-0.f;},"prefix-pose-signed-zero");
    corruptPrefix([](auto& a){a.plants[0].pose.position.y=std::nextafter(a.plants[0].pose.position.y,INFINITY);},"prefix-root-ulp");
    corruptPrefix([](auto& a){a.plants[0].radius=std::nextafter(a.plants[0].radius,INFINITY);},"prefix-radius-ulp");
    corruptPrefix([](auto& a){a.plants[0].variant=4;},"prefix-variant");
    corruptPrefix([](auto& a){a.plants.pop_back();},"prefix-count");
    corruptPrefix([](auto& a){++a.attempted;},"prefix-attempted");
    corruptPrefix([](auto& a){++a.rejectedClearance;},"prefix-clearance-count");
    corruptPrefix([](auto& a){++a.rejectedTerrain;},"prefix-terrain-count");
    corruptPrefix([](auto& a){++a.rejectedVegetation;},"prefix-vegetation-count");
    corruptPrefix([](auto& a){a.fingerprint^=1;},"prefix-fingerprint");
    corruptPrefix([](auto& a){a.reachedBudget=false;},"prefix-reached");
    corruptPrefix([](auto& a){a.variants[0].stems.vertices[0].color^=1;},"prefix-stem-color");
    corruptPrefix([](auto& a){a.variants[0].leaves.vertices[0].normal^=1;},"prefix-leaf-normal");
    corruptPrefix([](auto& a){a.variants[0].leaves.vertices[0].u+=.001f;},"prefix-leaf-uv");
    corruptPrefix([](auto& a){a.variants[0].leaves.indices[0]^=1;},"prefix-leaf-index");
    corruptPrefix([](auto& a){a.variants[0].height+=.001f;},"prefix-height");
    corruptPrefix([](auto& a){++a.variants[0].leafCount;},"prefix-leaf-count");
    corruptPrefix([](auto& a){a.variants[0].attachments[0].branchRadius+=.001f;},"prefix-attachment");
    const auto validateInput=[&](const auto& l,const auto& t,const auto& s,const auto& u,const auto& g,const auto& c,
        const auto& f,const auto& w,bool full,const Mistwatch::GroupedCanopyVolume::ValidatedWood* grouped=nullptr){
        prefix.Validate(context.shrubs,l,t,s,u,g,c,f,w,full,grouped);};
    const auto inputRejected=[&](auto&& action,const char* name){rejectPrefix(std::forward<decltype(action)>(action),name,
        "SHRUB_ARC_PREFIX_INPUT_CHANGED");};
    auto leavesChanged=assets.shrubLeaf;leavesChanged.vertices[0].v+=.001f;
    inputRejected([&]{validateInput(leavesChanged,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);},"prefix-leaf-source");
    auto terrainChanged=terrain;Q::Detail::Need(terrainChanged.SetHeight(0,0,terrain.Height(0,0)+.001f),"SHRUB_PREFIX_TERRAIN_FIXTURE");
    inputRejected([&]{validateInput(assets.shrubLeaf,terrainChanged,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);},"prefix-terrain-source");
    auto solidsChanged=context.vegetation[0];solidsChanged[0].center.x+=.001f;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,solidsChanged,context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);},"prefix-solid-source");
    auto woodChanged=context.wood;woodChanged[0].extents.x+=.001f;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,woodChanged,true);},"prefix-wood-source");
    auto understoryChanged=context.author.understory;understoryChanged.ferns[0].rotation.y+=.001f;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],understoryChanged,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);},"prefix-fern-source");
    understoryChanged=context.author.understory;understoryChanged.saplings[0].scale.x+=.001f;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],understoryChanged,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);},"prefix-sapling-source");
    auto grassChanged=context.retention.cover;++grassChanged.plants[0].patch;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        grassChanged,context.colonies,assets.fernAssets,context.wood,true);},"prefix-grass-source");
    grassChanged=context.retention.cover;grassChanged.patches[0].yaw+=.001f;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        grassChanged,context.colonies,assets.fernAssets,context.wood,true);},"prefix-grass-patch-source");
    auto coloniesChanged=context.colonies;++coloniesChanged.plants[0].grassPatch;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,coloniesChanged,assets.fernAssets,context.wood,true);},"prefix-colony-source");
    coloniesChanged=context.colonies;++coloniesChanged.colonies[0].accepted;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,coloniesChanged,assets.fernAssets,context.wood,true);},"prefix-colony-metadata-source");
    auto fernAssetsChanged=assets.fernAssets;++fernAssetsChanged[0].vertices;
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,fernAssetsChanged,context.wood,true);},"prefix-fern-asset-source");
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,false);},"prefix-full-solid-source");
    const std::array<Mistwatch::GroupedCanopyVolume::Group,1> groups{context.wood};
    const auto groupedExpected=Mistwatch::GroupedCanopyVolume::Measure(groups);
    const Mistwatch::GroupedCanopyVolume::ValidatedWood grouped(groups,groupedExpected);
    inputRejected([&]{validateInput(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true,&grouped);},"prefix-grouped-presence");
    const auto groupedPrefix=Prefix::CreateOriginal64(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,{},true,&grouped);
    Q::Detail::Need(Q::Detail::SameShrubs(groupedPrefix.Original(),context.shrubs),"SHRUB_PREFIX_GROUPED_ORIGINAL_CHANGED");
    groupedPrefix.Validate(context.shrubs,assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,{},true,&grouped);
    const std::array<Mistwatch::GroupedCanopyVolume::Group,1> changedGroups{woodChanged};
    const Mistwatch::GroupedCanopyVolume::ValidatedWood changedGrouped(changedGroups,
        Mistwatch::GroupedCanopyVolume::Measure(changedGroups));
    inputRejected([&]{groupedPrefix.Validate(context.shrubs,assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,{},true,&changedGrouped);},"prefix-grouped-source");
    inputRejected([&]{groupedPrefix.Validate(context.shrubs,assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,{},true);},"prefix-grouped-missing");
    Q::Detail::Need(prefixNegatives==38&&negatives==9,"SHRUB_PREFIX_NEGATIVE_COUNT");
    const auto validationStart=std::chrono::steady_clock::now();
    for(unsigned i=0;i<64;++i)prefix.Validate(context.shrubs,assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        context.retention.cover,context.colonies,assets.fernAssets,context.wood,true);
    const auto validationTime=std::chrono::steady_clock::now()-validationStart;
    Q::Detail::Need(Q::Detail::SameShrubs(prefix.Original(),context.shrubs),"SHRUB_PREFIX_TOKEN_MUTATED");
    const auto milliseconds=[](auto elapsed){return std::chrono::duration<double,std::milli>(elapsed).count();};
    std::cout<<"SHRUB_PREFIX_REUSE_CPU_PASS independent_original64=true full_input_bits=true original_arc_bits=true"
        <<" diagnostics_bits=true trace_bits=true composition_gate=true prefix_negatives="<<prefixNegatives
        <<" original_attempts="<<prefix.Original().attempted<<" original_factory_ms="<<milliseconds(prefixTime)
        <<" legacy_arc_ms="<<milliseconds(legacyArcTime)<<" reused_arc_ms="<<milliseconds(reusedTime)
        <<" input_validation_mean_ms="<<milliseconds(validationTime)/64.
        <<" avoided_second_prefix_search=true native=false GPU=false\n"<<std::flush;

    std::array<std::uint64_t,4> authorVariantTriangles{},renderVariantTriangles{},variantInstances{};
    std::array<unsigned,4> renderLeaves{},stemIndices{},leafIndices{};
    for(unsigned i=0;i<4;++i){const auto& a=candidate.variants[i];const auto crown=Mistwatch::CreateShrubOpenCrown(a,i);
        Q::Detail::Need(crown.authorLeafCount==504&&crown.renderLeafCount==224&&crown.authorStemCount==7&&
            Q::Detail::Same(crown.stems,a.stems),"SHRUB_DENSITY_OPEN_CROWN_RECIPE_CHANGED");
        authorVariantTriangles[i]=(a.stems.indices.size()+a.leaves.indices.size())/3;
        renderVariantTriangles[i]=(crown.stems.indices.size()+crown.leaves.indices.size())/3;
        stemIndices[i]=static_cast<unsigned>(crown.stems.indices.size());leafIndices[i]=static_cast<unsigned>(crown.leaves.indices.size());
        renderLeaves[i]=crown.renderLeafCount;
    }
    unsigned initial{},left{},right{},addedInitial{};
    for(std::size_t i=0;i<candidate.plants.size();++i){const auto& p=candidate.plants[i];++variantInstances[p.variant];
        const auto r=p.pose.position;left+=r.x<0;right+=r.x>0;
        const bool inInitialRegion=r.z<26.f&&std::abs(r.x)<18.f;initial+=inInitialRegion;addedInitial+=i>=64&&inInitialRegion;}
    std::uint64_t authorInstanceTriangles{},renderInstanceTriangles{};
    for(unsigned i=0;i<4;++i){authorInstanceTriangles+=variantInstances[i]*authorVariantTriangles[i];
        renderInstanceTriangles+=variantInstances[i]*renderVariantTriangles[i];}
    Q::Detail::Need(contextSeal==context.fingerprint&&Q::ContextFingerprint(context)==contextSeal&&
        sourceSeal==source.fingerprint&&Q::SourceFingerprint(source)==sourceSeal&&
        assetSeal==assets.fingerprint&&Q::AssetsFingerprint(assets)==assetSeal,"SHRUB_DENSITY_ORIGINAL_INPUTS_MUTATED");
    budget.Time();
    std::cout<<"SHRUB_DENSITY_CPU_PASS profile=woodland-feasible-arc-v3 target=128 plants="<<candidate.plants.size()
        <<" prefix=64 prefix_fingerprint="<<prefixHash<<" fingerprint="<<candidate.fingerprint
        <<" identity="<<Q::Detail::ShrubIdentity(candidate)<<" negatives="<<negatives
        <<" before_solids="<<context.worlds[0].size()<<" after_solids="<<context.worlds[1].size()
        <<" canopy="<<context.canopy.plants.size()<<" wood="<<context.wood.size()
        <<" grass="<<context.retention.cover.plants.size()<<" original_ferns="<<context.author.understory.ferns.size()
        <<" fern_colonies="<<context.colonies.plants.size()<<" attempts="<<candidate.attempted
        <<" generated_with_prefix="<<candidateDiagnostics.prefixAttempts+candidateDiagnostics.generated
        <<" evaluated_with_prefix="<<candidateDiagnostics.prefixAttempts+candidateDiagnostics.evaluated
        <<" legacy_shortfalls_verified=2"
        <<" rejected_clearance="<<candidate.rejectedClearance<<" rejected_terrain="<<candidate.rejectedTerrain
        <<" rejected_vegetation="<<candidate.rejectedVegetation<<" initial="<<initial<<" added_initial="<<addedInitial
        <<" left="<<left<<" right="<<right<<" variant_instances=";Csv(variantInstances);
    std::cout<<" author_variant_triangles=";Csv(authorVariantTriangles);
    std::cout<<" render_variant_triangles=";Csv(renderVariantTriangles);
    std::cout<<" author_instance_triangles="<<authorInstanceTriangles<<" render_instance_triangles="<<renderInstanceTriangles
        <<" render_leaf_counts=";Csv(renderLeaves);std::cout<<" uploaded_stem_index_expectations=";Csv(stemIndices);
    std::cout<<" uploaded_leaf_index_expectations=";Csv(leafIndices);
    std::cout<<" operations="<<budget.operations<<" elapsed_ms="<<budget.Elapsed()
        <<" native=false GPU=false art=false full_overlay=false full_route=false save_load=false\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"SHRUB_DENSITY_FIRST_DIVERGENCE "<<error.what()
    <<" native=false GPU=false art=false\n";return 1;}}
