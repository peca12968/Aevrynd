#include "../fallen/MistwatchAuthoredWorldQualification.h"
#include <iostream>
#include <functional>

namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
namespace {
unsigned negatives{};
template<class Action>void Reject(Action&& action,const char* name,const char* expected){
    bool rejected{};try{std::forward<Action>(action)();}catch(const std::exception& error){
        if(std::string_view(error.what())==expected)rejected=true;
        else throw std::runtime_error(std::string("NEGATIVE_FIRST_DIVERGENCE name=")+name+" actual="+error.what()+" expected="+expected);}
    Q::Detail::Need(rejected,"AUTHORED_NEGATIVE_ACCEPTED");++negatives;std::cout<<"CONTROL "<<name<<" rejected=true reason="<<expected<<'\n';
}
std::vector<W::Placement> FixedMappings(){
    // 只取 production 四姿態做此 context 單元測試；正式 qualifier 才提供完整 overlay 證書。
    std::vector<W::Placement> out;for(const auto& p:Mistwatch::FallenLogPlacements()){
        W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;out.push_back(std::move(q));}return out;
}
void SourceControls(const Q::SourceContext& source,const Aevrynd::HeightField3D& terrain){
    auto s=source;s.firs.firs[7].render.position.x=std::nextafter(s.firs.firs[7].render.position.x,INFINITY);
    Reject([&]{Q::ValidateSourceContext(s,terrain);},"source-fir-one-ulp","AUTHORED_SOURCE_CHANGED");
    s=source;s.understory.ferns[399].rotation.y=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{Q::ValidateSourceContext(s,terrain);},"source-fern-nan","AUTHORED_NONFINITE");
    s=source;s.legacyWorlds[0][0].id=1;s.fingerprint=Q::SourceFingerprint(s);
    Reject([&]{Q::ValidateSourceContext(s,terrain);},"source-facility-id-forged-seal","AUTHORED_SOURCE_TYPED_ROLES");
    s=source;s.legacyWorlds[1].erase(s.legacyWorlds[1].begin()+33);s.fingerprint=Q::SourceFingerprint(s);
    Reject([&]{Q::ValidateSourceContext(s,terrain);},"source-bridge-missing-forged-seal","AUTHORED_SOURCE_TYPED_ROLES");
    s=source;s.middle.trees.pop_back();s.fingerprint=Q::SourceFingerprint(s);
    Reject([&]{Q::ValidateSourceContext(s,terrain);},"source-middle-missing","AUTHORED_SOURCE_COUNTS");
}
void AuthorControls(const Q::Context& c,const Aevrynd::HeightField3D& terrain){
    auto a=c.author;a.firSourceIds[1]=a.firSourceIds[0];
    Reject([&]{Q::ValidateAuthor(a,c.source,terrain);},"author-duplicated-fir-source","AUTHORED_FIR_SOURCE_BITS");
    a=c.author;a.propSourceIds[0]=8;
    Reject([&]{Q::ValidateAuthor(a,c.source,terrain);},"author-wrong-log-source","AUTHORED_PROP_SOURCE_BITS");
    a=c.author;a.saplingSourceIds[0]=15;
    Reject([&]{Q::ValidateAuthor(a,c.source,terrain);},"author-withdrawn-sapling-reintroduced","AUTHORED_SAPLING_SOURCE_BITS");
    a=c.author;a.middleSourceIds[0]=16;
    Reject([&]{Q::ValidateAuthor(a,c.source,terrain);},"author-withdrawn-middle-reintroduced","AUTHORED_MIDDLE_SOURCE_BITS");
    a=c.author;a.understory.ferns[114]=c.source.understory.ferns[114];
    Reject([&]{Q::ValidateAuthor(a,c.source,terrain);},"author-approved-fern-unmoved","AUTHORED_FERN_SOURCE_BITS");
    a=c.author;a.understory.ferns[2].rotation.y=std::nextafter(a.understory.ferns[2].rotation.y,INFINITY);
    Reject([&]{Q::ValidateAuthor(a,c.source,terrain);},"author-unapproved-fern-one-ulp","AUTHORED_FERN_SOURCE_BITS");
    a=c.author;a.fallenProfile=Mistwatch::FallenLogProfile::LegacyCylinder;
    Reject([&]{Q::ValidateAuthor(a,c.source,terrain);},"author-wrong-profile","AUTHORED_AUTHOR_COUNTS");
    a=c.author;a.applied=false;
    Reject([&]{Q::ValidateAuthor(a,c.source,terrain);},"author-unapplied","AUTHORED_AUTHOR_COUNTS");
}
void WorldControls(const Q::Context& c){
    auto x=c;x.sourceMapped[0][0].id=1;
    Reject([&]{Q::ValidateTypedWorld(x,0);},"mapped-source-id-duplicate","AUTHORED_MAPPED_SOURCE_ROLES");
    x=c;x.worlds[0].back().id=0;
    Reject([&]{Q::ValidateTypedWorld(x,0);},"wood-typed-id-duplicate","AUTHORED_TYPED_WORLD_CHANGED");
    x=c;x.worlds[1].erase(x.worlds[1].begin()+33);
    Reject([&]{Q::ValidateTypedWorld(x,1);},"repaired-bridge-missing","AUTHORED_TYPED_WORLD_CHANGED");
    x=c;x.extras[0].pop_back();
    Reject([&]{Q::ValidateTypedWorld(x,0);},"extra-wood-missing","AUTHORED_EXTRAS_CHANGED");
    x=c;x.wood.pop_back();
    Reject([&]{Q::ValidateTypedWorld(x,0);},"wood-missing","AUTHORED_WORLD_DOMAIN");
    x=c;x.base[0][0].center.x=std::nextafter(x.base[0][0].center.x,INFINITY);
    Reject([&]{Q::ValidateTypedWorld(x,0);},"mission-one-ulp","AUTHORED_BASE_SNAPSHOT_CHANGED");
    x=c;x.worlds[0][0].box.extents.x=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{Q::ValidateTypedWorld(x,0);},"typed-box-nan","AUTHORED_TYPED_WORLD_CHANGED");
    x=c;x.author.mission[0].center.z=std::nextafter(x.author.mission[0].center.z,INFINITY);
    Reject([&]{Q::ValidateTypedWorld(x,0);},"author-base-oracle-not-self-referential","AUTHORED_BASE_INDEPENDENT_ORACLE");
}
void RetentionControls(const Q::Context& c,const Aevrynd::HeightField3D& terrain,const Q::Assets& assets,W::Budget& budget){
    Q::Detail::Need(c.retention.provenance.size()>1&&c.retention.provenance[0].origin==Mistwatch::ForestCoverPlantOrigin::Retained&&
        c.retention.provenance[1].origin==Mistwatch::ForestCoverPlantOrigin::Retained,"CONTROL_RETAINED_FIXTURE_DOMAIN");
    auto x=c;x.retention.provenance[1].sourcePlant=x.retention.provenance[0].sourcePlant;
    Reject([&]{Q::ValidateRetentionMappings(x,terrain,assets,budget);},"retained-source-reused","AUTHORED_RETAINED_SOURCE_DUPLICATE");
    x=c;x.retention.provenance[0].sourcePatch=(std::numeric_limits<unsigned>::max)();
    Reject([&]{Q::ValidateRetentionMappings(x,terrain,assets,budget);},"retained-source-patch-wrong","AUTHORED_RETAINED_SOURCE_BITS");
    x=c;x.retention.cover.plants[0].variant=5;
    Reject([&]{Q::ValidateRetentionMappings(x,terrain,assets,budget);},"grass-variant-out-of-domain","AUTHORED_RETENTION_PLANT_DOMAIN");
    x=c;x.retention.cover.plants[0].radius=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{Q::ValidateRetentionMappings(x,terrain,assets,budget);},"grass-radius-nan","AUTHORED_RETENTION_CANONICAL_POSE");
    x=c;x.retention.cover.plants[0].transform.position.y=std::nextafter(x.retention.cover.plants[0].transform.position.y,INFINITY);
    Reject([&]{Q::ValidateRetentionMappings(x,terrain,assets,budget);},"grass-ground-one-ulp","AUTHORED_RETENTION_CANONICAL_POSE");
    x=c;x.retention.cover.plants.pop_back();
    Reject([&]{Q::ValidateRetentionMappings(x,terrain,assets,budget);},"retention-shortfall","AUTHORED_RETENTION_COUNTS");
    x=c;x.sourceCover.plants[0].variant=(x.sourceCover.plants[0].variant+1)%5;
    Reject([&]{Q::ValidateRetentionMappings(x,terrain,assets,budget);},"old-canonical-grass-corrupt","AUTHORED_RETENTION_COUNTS");
    const auto fill=std::find_if(c.retention.provenance.begin(),c.retention.provenance.end(),[](const auto& p){return p.origin!=Mistwatch::ForestCoverPlantOrigin::Retained;});
    Q::Detail::Need(fill!=c.retention.provenance.end(),"CONTROL_REAL_FILL_REQUIRED");const auto at=static_cast<std::size_t>(fill-c.retention.provenance.begin());
    x=c;x.retention.provenance[at].sourcePlant=0;
    Reject([&]{Q::ValidateRetentionMappings(x,terrain,assets,budget);},"fill-falsely-aliases-old-source","AUTHORED_FILL_SOURCE_ALIAS");
}
}
int wmain(int argc,wchar_t** argv){try{
    Q::Detail::Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","USAGE: MistwatchAuthoredWorldQualificationTest --runtime-assets ABS");
    const std::filesystem::path root=std::filesystem::path(argv[2]).lexically_normal();Q::Detail::Need(root.is_absolute(),"AUTHORED_ASSET_ROOT_ABSOLUTE_REQUIRED");
    W::Budget budget;const auto assets=Q::LoadAssets(root);
    const auto explicitGenerated=Q::LoadAssets(root,root/"mistwatch-canopy-preview"/"meshes");
    Q::Detail::Need(explicitGenerated.fingerprint==assets.fingerprint&&Q::Detail::Same(explicitGenerated.derivedStem,assets.derivedStem),
        "AUTHORED_GENERATED_DIRECTORY_RECIPE_CHANGED");
    const auto terrain=Mistwatch::CreateTerrain();
    const auto source=Q::CreateSourceContext(terrain,assets,budget);const auto scan=FixedMappings();
    const auto context=Q::CreateContext(terrain,assets,source,scan,budget);Q::RepeatControls(context,terrain,assets,budget);
    SourceControls(source,terrain);AuthorControls(context,terrain);WorldControls(context);RetentionControls(context,terrain,assets,budget);
    auto corruptAssets=assets;corruptAssets.grass[0].vertices[0].u=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)Q::AssetsFingerprint(corruptAssets);},"grass-mesh-uv-nan","AUTHORED_NONFINITE");
    corruptAssets=assets;corruptAssets.grassIdentities[0]=0;
    Reject([&]{(void)Q::AssetsFingerprint(corruptAssets);},"grass-full-mesh-identity-missing","AUTHORED_GRASS_ASSET_STALE");
    corruptAssets=assets;corruptAssets.fernAssets[0].radius+=.01f;
    Reject([&]{(void)Q::AssetsFingerprint(corruptAssets);},"fern-measured-envelope-stale","AUTHORED_FERN_ASSET_STALE");
    corruptAssets=assets;corruptAssets.canopy[0].indices[0]=static_cast<unsigned>(corruptAssets.canopy[0].vertices.size());
    Reject([&]{(void)Q::AssetsFingerprint(corruptAssets);},"canopy-mesh-bad-index","AUTHORED_ASSET_TOPOLOGY");
    auto wrongPin=Q::FernPins[0];wrongPin.bytes+=4;
    Reject([&]{(void)Q::Detail::LoadPinned(root/wrongPin.path,wrongPin);},"asset-wrong-byte-count","AUTHORED_ASSET_BYTES");
    wrongPin=Q::FernPins[0];wrongPin.sha="0000000000000000000000000000000000000000000000000000000000000000";
    Reject([&]{(void)Q::Detail::LoadPinned(root/wrongPin.path,wrongPin);},"asset-wrong-sha","AUTHORED_ASSET_SHA_MISMATCH");
    auto badScan=scan;badScan[0].pose.position.x=std::nextafter(badScan[0].pose.position.x,INFINITY);
    Reject([&]{(void)Q::CreateContext(terrain,assets,source,badScan,budget);},"qualified-log-pose-one-ulp","AUTHORED_QUALIFIED_LOG_MAPPING_CHANGED");
    auto shortScan=scan;shortScan.pop_back();
    // 此負控的失敗名稱取自既有 source-role 契約，不改寫其規則。
    Reject([&]{(void)Q::CreateContext(terrain,assets,source,shortScan,budget);},"qualified-log-missing","ACTIVE_COHORT_IDENTITY_FAILED");
    Q::Detail::Need(Q::SourceFingerprint(source)==source.fingerprint&&Q::AssetsFingerprint(assets)==assets.fingerprint,"AUTHORED_ORIGINAL_INPUTS_MUTATED");
    budget.Time();std::cout<<"MISTWATCH_AUTHORED_WORLD_CONTEXT_CPU_PASS negatives="<<negatives
        <<"; before_solids="<<context.worlds[0].size()<<"; after_solids="<<context.worlds[1].size()
        <<"; canopy="<<context.canopy.plants.size()<<"; wood="<<context.wood.size()<<"; cover="<<context.retention.cover.plants.size()
        <<"; retained="<<context.retention.retained<<"; rejected="<<context.retention.rejected<<"; filled="<<context.retention.filled
        <<"; litter="<<context.litter.patches.size()<<"; fern_colonies="<<context.colonies.plants.size()<<"; shrubs="<<context.shrubs.plants.size()
        <<"; fingerprint="<<context.fingerprint<<"; native=false; full_overlay=false; full_route=false; save_load=false\n";return 0;
}catch(const std::exception& error){std::cerr<<"AUTHORED_CONTEXT_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
