#include "MistwatchForestCoverRetention.h"
#include "fallen/MistwatchAuthoredWorldQualification.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <type_traits>

namespace Q=::Mistwatch::AuthoredQualification;

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
void Require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
template<class Action> void Reject(Action action,const char* message) {
    bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}Require(rejected,message);
}
ForestCoverWorldView View(const HeightField3D& terrain,const std::vector<Vec3>& trees,const ForestUnderstory& understory,
    const std::vector<BoxCollider3D>& solids,const std::vector<BoxCollider3D>& wood) {
    ForestCoverWorldView view{terrain,trees,understory,solids,wood,wood.size(),0};
    view.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(view);return view;
}
void IndependentOutput(const ForestCoverResult& old,const ForestCoverRetentionResult& output,const ForestCoverWorldView& finalWorld,
    const std::array<ForestCoverAsset,5>& assets,const ForestCoverSettings& settings) {
    Require(output.cover.plants.size()==output.provenance.size(),"Provenance membership mismatch");
    Require(output.retained+output.rejected==old.plants.size()&&output.retained+output.filled==output.cover.plants.size(),"Population accounting mismatch");
    std::vector<bool> consumed(old.plants.size());std::vector<unsigned> patchCounts(output.cover.patches.size());
    unsigned retained{},filled{};
    for(std::size_t index=0;index<output.cover.plants.size();++index) {
        const auto& plant=output.cover.plants[index];const auto& provenance=output.provenance[index];
        Require(plant.variant<5&&plant.patch<output.cover.patches.size(),"Output variant/patch out of range");
        ++patchCounts[plant.patch];const auto root=plant.transform.position;
        Require(ForestCoverClearance(root,plant.radius,finalWorld.solids)&&ForestCoverFootprint(finalWorld.terrain,root,plant.radius,settings),"Output terrain/mission gate failed");
        Require(ForestCoverRetentionDetail::Same(root.y,finalWorld.terrain.SampleHeight(root.x,root.z)),"Root moved off terrain");
        Require(ForestCoverRetentionDetail::Same(plant.radius,assets[plant.variant].radius*plant.transform.scale.x),"Actual footprint radius changed");
        const CanopyVolume::PlantVolume volume{root,CanopyVolume::ConservativeLocalVolume(assets[plant.variant].radius,
            assets[plant.variant].minimumY,assets[plant.variant].maximumY),plant.transform.scale.x,plant.transform.rotation};
        Require(CanopyVolume::ClearsCanopyWood(volume,finalWorld.solids)&&CanopyVolume::ClearsCanopyWood(volume,finalWorld.wood),"Complete wood volume gate failed");
        if(provenance.origin==ForestCoverPlantOrigin::Retained) {
            ++retained;Require(provenance.sourcePlant<old.plants.size()&&!consumed[provenance.sourcePlant],"Retained source duplicate/out of range");
            consumed[provenance.sourcePlant]=true;auto canonical=old.plants[provenance.sourcePlant];
            Require(provenance.sourcePatch==canonical.patch,"Original source patch lost");
            canonical.patch=plant.patch;Require(ForestCoverRetentionDetail::Same(canonical,plant),"Safe original pose/variant/radius not bit preserved");
        }else {++filled;Require(provenance.sourcePlant==(std::numeric_limits<unsigned>::max)(),"Fill aliased source plant");}
        for(std::size_t other=0;other<index;++other) {
            const auto& previous=output.cover.plants[other];
            Require(std::hypot(root.x-previous.transform.position.x,root.z-previous.transform.position.z)>=
                settings.rootSeparationRatio*(plant.radius+previous.radius),"Output exact root separation failed");
        }
    }
    for(const auto& rejected:output.rejections) {
        Require(rejected.sourcePlant<old.plants.size()&&!consumed[rejected.sourcePlant]&&
            rejected.sourcePatch==old.plants[rejected.sourcePlant].patch,"Rejected source duplicate/out of range");
        consumed[rejected.sourcePlant]=true;
    }
    for(const bool seen:consumed)Require(seen,"Original source identity disappeared");
    Require(retained==output.retained&&filled==output.filled&&output.rejections.size()==output.rejected,"Origin accounting mismatch");
    for(std::size_t index=0;index<patchCounts.size();++index)Require(patchCounts[index]&&
        patchCounts[index]==output.cover.patches[index].accepted&&patchCounts[index]<=output.cover.patches[index].requested,"Active patch recount failed");
    Require(output.cover.patches.size()<=settings.maximumPatches&&output.cover.reachedBudget==(output.cover.plants.size()==settings.targetPlants),"Finite budget/shortfall contract failed");
}
unsigned tokenNegatives{};
template<class Action>void ExactReject(Action&& action,const char* name,const char* expected){
    bool rejected=false;
    try{std::forward<Action>(action)();}catch(const std::invalid_argument& error){
        if(std::string_view(error.what())!=expected)throw std::runtime_error(std::string("RETENTION_TOKEN_NEGATIVE_REASON ")+name+": "+error.what());
        rejected=true;
    }
    Require(rejected,"RETENTION_TOKEN_NEGATIVE_ACCEPTED");++tokenNegatives;
    std::cout<<"RETENTION_TOKEN_CONTROL name="<<name<<" reason="<<expected<<'\n';
}
void TokenControls(const ForestCoverWorldView& original,const std::array<ForestCoverAsset,5>& assets,
    const std::array<std::uint64_t,5>& identities,const ForestCoverSettings& settings,const ForestCoverResult& old){
    using Token=ValidatedForestCoverOriginal;
    const auto negativeStart=tokenNegatives;
    static_assert(!std::is_constructible_v<Token,ForestCoverResult>);
    static_assert(std::is_same_v<decltype(std::declval<const Token&>().Original()),const ForestCoverResult&>);
    const auto token=Token::Create(original.terrain,original.trees,original.understory,original.solids,assets,identities,settings,original.wood);
    Require(Q::Detail::SameCover(token.Original(),old),"Token original differs from independent generator");
    const auto validate=[&](const Token& value,const ForestCoverWorldView& world,const auto& a,const auto& ids,const auto& s){
        value.Validate(old,world,a,ids,s);};
    const auto copied=token;auto movedFrom=token;const auto moved=std::move(movedFrom);
    validate(copied,original,assets,identities,settings);validate(moved,original,assets,identities,settings);
    ExactReject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(Token{},old,original,original,assets,identities,settings);},
        "default-token","RETENTION_ORIGINAL_TOKEN_REQUIRED");
    ExactReject([&]{validate(movedFrom,original,assets,identities,settings);},"moved-token","RETENTION_ORIGINAL_TOKEN_REQUIRED");
    const auto inputReject=[&](auto&& action,const char* name){ExactReject(std::forward<decltype(action)>(action),name,"RETENTION_ORIGINAL_INPUT_CHANGED");};
    // 每個設定欄位都獨立變更；來源比對不僅依賴設定雜湊或記憶體位址。
    const auto changeSetting=[&](auto&& change,const char* name){auto changed=settings;change(changed);
        inputReject([&]{validate(token,original,assets,identities,changed);},name);};
    changeSetting([](auto& s){++s.seed;},"seed");
    changeSetting([](auto& s){++s.targetPlants;},"target");
    changeSetting([](auto& s){++s.maximumPatches;},"maximum-patches");
    changeSetting([](auto& s){++s.attemptsPerPatch;},"attempts");
    changeSetting([](auto& s){++s.independentAnchors;},"anchors");
    changeSetting([](auto& s){++s.minimumPlantsPerPatch;},"minimum-per-patch");
    changeSetting([](auto& s){++s.maximumPlantsPerPatch;},"maximum-per-patch");
    changeSetting([](auto& s){s.prioritizeRouteShoulders=!s.prioritizeRouteShoulders;},"priority");
    changeSetting([](auto& s){s.minimumNormalY=std::nextafter(s.minimumNormalY,1.f);},"normal-ulp");
    changeSetting([](auto& s){s.maximumFootprintHeightDelta=std::nextafter(s.maximumFootprintHeightDelta,0.f);},"height-delta-ulp");
    changeSetting([](auto& s){s.rootSeparationRatio=std::nextafter(s.rootSeparationRatio,1.f);},"root-spacing-ulp");
    for(unsigned slot=0;slot<5;++slot){
        for(unsigned field=0;field<4;++field){auto changed=assets;auto& a=changed[slot];
            float& value=field==0?a.radius:field==1?a.height:field==2?a.minimumY:a.maximumY;
            value=std::nextafter(value,std::numeric_limits<float>::infinity());
            inputReject([&]{validate(token,original,changed,identities,settings);},"asset-field-ulp");}
        auto changed=identities;++changed[slot];
        inputReject([&]{validate(token,original,assets,changed,settings);},"asset-identity");
    }
    auto terrainChanged=original.terrain;
    Require(terrainChanged.SetHeight(0,0,std::nextafter(terrainChanged.Height(0,0),std::numeric_limits<float>::infinity())),"Token terrain mutation failed");
    // view 的 terrain 是 reference，另建 view，不能用賦值修改原始測試來源。
    ForestCoverWorldView changedTerrain{terrainChanged,original.trees,original.understory,original.solids,original.wood,
        original.expectedWoodCount,original.expectedInputFingerprint};
    inputReject([&]{validate(token,changedTerrain,assets,identities,settings);},"terrain-height");
    auto grid=original.terrain.Settings();grid.origin.y=std::nextafter(grid.origin.y,std::numeric_limits<float>::infinity());
    HeightField3D originChanged(grid);originChanged.Fill(0.f);
    ForestCoverWorldView changedGrid{originChanged,original.trees,original.understory,original.solids,original.wood,
        original.expectedWoodCount,original.expectedInputFingerprint};
    inputReject([&]{validate(token,changedGrid,assets,identities,settings);},"terrain-grid");
    auto treeChanged=std::vector<Vec3>(original.trees.begin(),original.trees.end());
    if(treeChanged.empty())treeChanged.push_back({100,0,100});
    else treeChanged[0].x=std::nextafter(treeChanged[0].x,std::numeric_limits<float>::infinity());
    auto changedWorld=original;changedWorld.trees=treeChanged;
    inputReject([&]{validate(token,changedWorld,assets,identities,settings);},"tree-bits-or-count");
    auto understoryChanged=original.understory;
    if(understoryChanged.ferns.empty())understoryChanged.ferns.push_back({});
    else understoryChanged.ferns[0].rotation.x=-0.f;
    // understory 同樣為 const reference；各變體均使用新的 view。
    ForestCoverWorldView changedFern{original.terrain,original.trees,understoryChanged,original.solids,original.wood,
        original.expectedWoodCount,original.expectedInputFingerprint};
    inputReject([&]{validate(token,changedFern,assets,identities,settings);},"fern-bits-or-count");
    understoryChanged=original.understory;
    if(understoryChanged.saplings.empty())understoryChanged.saplings.push_back({});
    else understoryChanged.saplings[0].scale.z=std::nextafter(understoryChanged.saplings[0].scale.z,1.f);
    ForestCoverWorldView changedSapling{original.terrain,original.trees,understoryChanged,original.solids,original.wood,
        original.expectedWoodCount,original.expectedInputFingerprint};
    inputReject([&]{validate(token,changedSapling,assets,identities,settings);},"sapling-bits-or-count");
    auto solidChanged=std::vector<BoxCollider3D>(original.solids.begin(),original.solids.end());
    if(solidChanged.empty())solidChanged.push_back({{100,100,100},{1,1,1}});
    else solidChanged[0].center.y=std::nextafter(solidChanged[0].center.y,std::numeric_limits<float>::infinity());
    auto changedSolid=original;changedSolid.solids=solidChanged;
    inputReject([&]{validate(token,changedSolid,assets,identities,settings);},"solid-bits-or-count");
    auto woodChanged=std::vector<BoxCollider3D>(original.wood.begin(),original.wood.end());
    woodChanged[0].extents.x=std::nextafter(woodChanged[0].extents.x,std::numeric_limits<float>::infinity());
    auto changedWood=original;changedWood.wood=woodChanged;
    inputReject([&]{validate(token,changedWood,assets,identities,settings);},"wood-bits");
    // 透過真正新入口核對先後；失效 token 不能把較早的舊 preflight 錯誤換掉。
    auto missingFingerprint=original;missingFingerprint.expectedInputFingerprint=0;
    ExactReject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(Token{},old,missingFingerprint,original,assets,identities,settings);},
        "world-preflight-before-token","Retention world identity or finite budget mismatch");
    auto invalidSettings=settings;invalidSettings.minimumNormalY=std::numeric_limits<float>::quiet_NaN();
    ExactReject([&]{(void)CreateForestCoverRetained(old,original,original,assets,identities,invalidSettings);},
        "legacy-settings-error","Invalid forest cover settings");
    ExactReject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(Token{},old,original,original,assets,identities,invalidSettings);},
        "generator-validation-before-token","Invalid forest cover settings");
    auto invalidAssets=assets;invalidAssets[0].radius=4.1f;
    ExactReject([&]{(void)CreateForestCoverRetained(old,original,original,invalidAssets,identities,settings);},
        "legacy-radius-error","Invalid normalized grass asset footprint");
    ExactReject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(Token{},old,original,original,invalidAssets,identities,settings);},
        "asset-generator-validation-before-token","Invalid normalized grass asset footprint");
    auto aliased=identities;aliased[4]=aliased[0];
    ExactReject([&]{(void)CreateForestCoverRetained(old,original,original,assets,aliased,settings);},
        "legacy-identity-error","Retention asset identities aliased");
    ExactReject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(Token{},old,original,original,assets,aliased,settings);},
        "identity-preflight-before-token","Retention asset identities aliased");
    auto changedSource=changedWood;changedSource.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(changedSource);
    ExactReject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(token,old,changedSource,original,assets,identities,settings);},
        "rehashed-changed-original","RETENTION_ORIGINAL_INPUT_CHANGED");
    Require(tokenNegatives-negativeStart==53,"RETENTION_TOKEN_CONTROL_COUNT");
    Require(Q::Detail::SameCover(token.Original(),old),"RETENTION_TOKEN_SNAPSHOT_MUTATED");
}
void RealWorld(const std::filesystem::path& runtime){
    Require(runtime.is_absolute(),"RETENTION_RUNTIME_ABSOLUTE_REQUIRED");
    // 既有獨立 loader 逐檔核對 byte count／SHA／原生網格，driver 另封完整 runtime inventory。
    const auto assets=Q::LoadAssets(runtime);const auto terrain=CreateTerrain();Q::W::Budget budget;
    const auto source=Q::CreateSourceContext(terrain,assets,budget);
    std::vector<Q::W::Placement> mappings;
    for(const auto& p:FallenLogPlacements()){Q::W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;mappings.push_back(std::move(q));}
    const auto context=Q::CreateContext(terrain,assets,source,mappings,budget);
    // 改前獨立 forest-wood CPU 證書固定值；不是十二群組 woodland 的 3571／29。
    Require(context.sourceWood.size()==4335&&context.wood.size()==8617&&context.sourceCover.plants.size()==3600&&
        context.retention.cover.plants.size()==3600&&context.retention.retained==3580&&context.retention.rejected==20&&
        context.retention.filled==20&&context.retention.provenanceFingerprint==2906957650885998678ull&&
        context.retention.originalInputFingerprint==15091677462053052260ull&&
        context.retention.finalInputFingerprint==14460907931401333265ull,"RETENTION_REAL_4335_8617_3600_REQUIRED");
    const auto originalSolids=Q::W::Boxes(context.sourceMapped[0]);
    ForestCoverWorldView original{terrain,source.firs.roots,source.understory,originalSolids,context.sourceWood,4335,0};
    ForestCoverWorldView finalWorld{terrain,context.author.firs.roots,context.author.understory,context.vegetation[0],context.wood,8617,0};
    original.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(original);
    finalWorld.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(finalWorld);
    const auto sourceSeal=Q::SourceFingerprint(source),contextSeal=Q::ContextFingerprint(context),assetSeal=Q::AssetsFingerprint(assets);
    const auto before=std::chrono::steady_clock::now();
    const auto token=ValidatedForestCoverOriginal::Create(terrain,source.firs.roots,source.understory,originalSolids,
        assets.grassAssets,assets.grassIdentities,{},context.sourceWood);
    const auto factoryTime=std::chrono::steady_clock::now()-before;
    Require(Q::Detail::SameCover(token.Original(),context.sourceCover)&&
        Q::Detail::CoverIdentity(token.Original())==Q::Detail::CoverIdentity(context.sourceCover),"RETENTION_REAL_ORIGINAL_BITS_CHANGED");
    const auto oldStart=std::chrono::steady_clock::now();
    const auto legacy=CreateForestCoverRetained(context.sourceCover,original,finalWorld,assets.grassAssets,assets.grassIdentities);
    const auto legacyTime=std::chrono::steady_clock::now()-oldStart;
    const auto newStart=std::chrono::steady_clock::now();
    const auto reused=CreateForestCoverRetainedFromValidatedOriginal(token,context.sourceCover,original,finalWorld,assets.grassAssets,assets.grassIdentities);
    const auto reusedTime=std::chrono::steady_clock::now()-newStart;
    Require(Q::Detail::SameRetention(legacy,reused)&&Q::Detail::SameRetention(context.retention,reused)&&
        Q::Detail::RetentionIdentity(legacy)==Q::Detail::RetentionIdentity(reused),"RETENTION_REAL_ALL_BITS_CHANGED");
    IndependentOutput(context.sourceCover,reused,finalWorld,assets.grassAssets,{});
    const auto sealStart=std::chrono::steady_clock::now();
    for(unsigned i=0;i<64;++i)token.Validate(context.sourceCover,original,assets.grassAssets,assets.grassIdentities);
    const auto sealTime=std::chrono::steady_clock::now()-sealStart;
    TokenControls(original,assets.grassAssets,assets.grassIdentities,{},context.sourceCover);
    Require(sourceSeal==Q::SourceFingerprint(source)&&contextSeal==Q::ContextFingerprint(context)&&
        assetSeal==Q::AssetsFingerprint(assets),"RETENTION_REAL_ORIGINALS_MUTATED");
    const auto ms=[](auto d){return std::chrono::duration<double,std::milli>(d).count();};
    std::cout<<"FOREST_COVER_TOKEN_REAL_CPU_PASS source_wood=4335 final_wood=8617 grass=3600 retained=3580 rejected=20 filled=20"
        <<" all_bits=true provenance_bits=true rejection_bits=true fingerprint="<<reused.fingerprint
        <<" original_factory_ms="<<ms(factoryTime)<<" legacy_retention_ms="<<ms(legacyTime)
        <<" reused_retention_ms="<<ms(reusedTime)<<" source_seal_mean_ms="<<ms(sealTime)/64.
        <<" asset_identity="<<assetSeal<<" native=false GPU=false whole_startup_measured=false\n"<<std::flush;
}
}
int main(int argc,char** argv) {
    try {
        Require(argc==1||(argc==3&&std::string_view(argv[1])=="--runtime-assets"),"Expected optional --runtime-assets ABS");
        // 此為有限合成反例；真五 mesh 與八棵世界由獨立整合測試驗證。
        HeightField3D terrain({{-50,0,-20},101,121,1.f});terrain.Fill(0.f);
        std::vector<Vec3> trees;ForestUnderstory understory;std::vector<BoxCollider3D> solids;
        std::vector<BoxCollider3D> wood{{{40,10,90},{1,1,1}}};
        std::array<ForestCoverAsset,5> assets{};for(auto& asset:assets)asset={GrassMinimumRadius,GrassAssetHeight,0.f,GrassAssetHeight};
        const std::array<std::uint64_t,5> identities{11,22,33,44,55};
        ForestCoverSettings settings;settings.targetPlants=64;settings.maximumPatches=20;
        const auto original=View(terrain,trees,understory,solids,wood);
        const auto old=CreateForestCover(terrain,trees,understory,solids,assets,settings,wood,true);
        Require(old.reachedBudget&&old.plants.size()==64,"Synthetic canonical source shortfall");
        const auto token=ValidatedForestCoverOriginal::Create(terrain,trees,understory,solids,assets,identities,settings,wood);
        Require(Q::Detail::SameCover(old,token.Original()),"Synthetic opaque original changed");
        const auto unchanged=CreateForestCoverRetained(old,original,original,assets,identities,settings);
        const auto reusedUnchanged=CreateForestCoverRetainedFromValidatedOriginal(token,old,original,original,assets,identities,settings);
        Require(Q::Detail::SameRetention(unchanged,reusedUnchanged),"Unchanged reuse proof bits changed");
        Require(unchanged.retained==64&&!unchanged.rejected&&!unchanged.filled&&unchanged.cover.reachedBudget,"Unchanged world lost safe source");
        IndependentOutput(old,unchanged,original,assets,settings);
        const auto repeat=CreateForestCoverRetained(old,original,original,assets,identities,settings);
        Require(unchanged.fingerprint==repeat.fingerprint&&ForestCoverRetentionDetail::Same(unchanged.cover,repeat.cover),"Same inputs changed retention proof");
        const auto badSource=[&](ForestCoverResult corrupt,const char* message) {
            Reject([&](){(void)CreateForestCoverRetained(corrupt,original,original,assets,identities,settings);},message);
            ExactReject([&]{(void)CreateForestCoverRetainedFromValidatedOriginal(token,corrupt,original,original,assets,identities,settings);},
                message,"Retention canonical source mismatch or shortfall");
        };
        auto corrupt=old;corrupt.plants[0].transform.rotation.x=std::nextafter(0.f,1.f);badSource(corrupt,"One-bit pose corruption accepted");
        corrupt=old;corrupt.plants[0].radius=std::nextafter(corrupt.plants[0].radius,1.f);badSource(corrupt,"One-bit radius corruption accepted");
        corrupt=old;corrupt.plants[0].variant=(corrupt.plants[0].variant+1)%5;badSource(corrupt,"Variant corruption accepted");
        corrupt=old;corrupt.plants[0].patch=static_cast<unsigned>(old.patches.size());badSource(corrupt,"Out-of-range patch accepted");
        corrupt=old;corrupt.plants[1]=corrupt.plants[0];badSource(corrupt,"Duplicate source root accepted");
        corrupt=old;corrupt.plants.pop_back();badSource(corrupt,"Incomplete canonical population accepted");
        corrupt=old;++corrupt.patches[0].accepted;badSource(corrupt,"Patch count corruption accepted");
        corrupt=old;corrupt.patches[0].yaw=std::nextafter(corrupt.patches[0].yaw,100.f);badSource(corrupt,"Patch shape corruption accepted");
        corrupt=old;++corrupt.rejectedTerrain;badSource(corrupt,"Source rejection receipt corruption accepted");
        auto mismatched=original;--mismatched.expectedWoodCount;
        Reject([&](){(void)CreateForestCoverRetained(old,mismatched,original,assets,identities,settings);},"Missing original wood proof accepted");
        Reject([&](){(void)CreateForestCoverRetained(old,original,mismatched,assets,identities,settings);},"Missing final wood proof accepted");
        std::vector<BoxCollider3D> changedWood=wood;changedWood[0].center.x=std::nextafter(40.f,50.f);
        auto changedView=original;changedView.wood=changedWood;
        Reject([&](){(void)CreateForestCoverRetained(old,original,changedView,assets,identities,settings);},"Complete wood fingerprint corruption accepted");
        changedWood[0].extents.y=std::numeric_limits<float>::quiet_NaN();
        Reject([&](){(void)CreateForestCoverRetained(old,original,changedView,assets,identities,settings);},"NaN wood accepted");
        auto aliased=identities;aliased[4]=aliased[1];
        Reject([&](){(void)CreateForestCoverRetained(old,original,original,assets,aliased,settings);},"Aliased mesh identity accepted");
        auto badSettings=settings;badSettings.maximumPatches=401;
        Reject([&](){(void)CreateForestCoverRetained(old,original,original,assets,identities,badSettings);},"Expanded patch budget accepted");
        badSettings=settings;badSettings.attemptsPerPatch=241;
        Reject([&](){(void)CreateForestCoverRetained(old,original,original,assets,identities,badSettings);},"Expanded attempt budget accepted");
        changedWood=wood;const auto blockedRoot=old.plants[0].transform.position;
        changedWood.push_back({blockedRoot+Vec3{0,.3f,0},{.4f,.4f,.4f}});
        const auto occupied=View(terrain,trees,understory,solids,changedWood);
        const auto filled=CreateForestCoverRetained(old,original,occupied,assets,identities,settings);
        Require(Q::Detail::SameRetention(filled,CreateForestCoverRetainedFromValidatedOriginal(token,old,original,occupied,assets,identities,settings)),
            "Changed-world fill reuse proof bits changed");
        Require(filled.rejected&&filled.filled&&filled.cover.reachedBudget,"New wood rejection/refill not exercised");
        bool firstRejected=false;for(const auto& rejection:filled.rejections)firstRejected|=rejection.sourcePlant==0&&rejection.gate==ForestCoverRetentionGate::Volume;
        Require(firstRejected,"Actual low wood volume did not reject original first root");
        IndependentOutput(old,filled,occupied,assets,settings);
        const auto filledRepeat=CreateForestCoverRetained(old,original,occupied,assets,identities,settings);
        Require(filled.fingerprint==filledRepeat.fingerprint&&ForestCoverRetentionDetail::Same(filled.cover,filledRepeat.cover),"Finite fill not repeatable");
        std::vector<BoxCollider3D> allBlocked{{{0,0,40},{100,100,100}}};
        const auto exhausted=View(terrain,trees,understory,allBlocked,wood);
        const auto shortfall=CreateForestCoverRetained(old,original,exhausted,assets,identities,settings);
        Require(Q::Detail::SameRetention(shortfall,CreateForestCoverRetainedFromValidatedOriginal(token,old,original,exhausted,assets,identities,settings)),
            "Final-world shortfall reuse proof bits changed");
        Require(!shortfall.cover.reachedBudget&&shortfall.cover.plants.empty()&&shortfall.cover.patches.empty()&&
            shortfall.rejected==64&&!shortfall.retained&&!shortfall.filled,"Bounded shortfall hidden by unsafe fallback");
        IndependentOutput(old,shortfall,exhausted,assets,settings);
        HeightField3D raised({{-50,0,-20},101,121,1.f});raised.Fill(1.f);
        const auto movedGround=View(raised,trees,understory,solids,wood);
        const auto regrounded=CreateForestCoverRetained(old,original,movedGround,assets,identities,settings);
        Require(Q::Detail::SameRetention(regrounded,CreateForestCoverRetainedFromValidatedOriginal(token,old,original,movedGround,assets,identities,settings)),
            "Changed-ground reuse proof bits changed");
        Require(!regrounded.retained&&regrounded.rejected==64&&regrounded.cover.reachedBudget,"Changed terrain source Y was silently moved and called retained");
        for(const auto& rejection:regrounded.rejections)Require(rejection.gate==ForestCoverRetentionGate::GroundAnchor,"Changed terrain wrong first gate");
        IndependentOutput(old,regrounded,movedGround,assets,settings);
        TokenControls(original,assets,identities,settings,old);
        if(argc==3)RealWorld(std::filesystem::path(argv[2]).lexically_normal());
        std::cout<<"FOREST_COVER_RETENTION_SYNTHETIC_CPU_PASS old="<<old.plants.size()<<" unchanged_retained="<<unchanged.retained<<
            " changed_retained="<<filled.retained<<" rejected="<<filled.rejected<<" fill="<<filled.filled<<
            " shortfall="<<shortfall.rejected<<" fingerprint="<<filled.fingerprint<<" native_visual=UNVERIFIED\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 1;}
}
