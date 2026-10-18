#include "../fallen/MistwatchAuthoredWorldQualification.h"
#include <iostream>
#include <locale>
#include <string_view>

namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
namespace M=::Mistwatch;
namespace {
using Q::Detail::Need;
unsigned negatives{};
std::vector<W::Placement> FixedMappings(){
    // 此測試只提供原四固定姿態，沒有冒充完整倒木 overlay 證書。
    std::vector<W::Placement> out;
    for(const auto& p:M::FallenLogPlacements()){
        W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;
        out.push_back(std::move(q));
    }
    return out;
}
template<class Action>void Reject(Action&& action,const char* expected,W::Budget& budget){
    budget.Time();bool rejected{};
    try{std::forward<Action>(action)();}catch(const std::exception& error){
        if(std::string_view(error.what())!=expected)
            throw std::runtime_error(std::string("SHRUB_TRACE_NEGATIVE_UNEXPECTED reason=")+error.what()+" expected="+expected);
        rejected=true;
    }
    budget.Time();Need(rejected,"SHRUB_TRACE_NEGATIVE_ACCEPTED");++negatives;
}
bool SameVariant(const M::ShrubMesh& a,const M::ShrubMesh& b){
    if(!Q::Detail::Same(a.stems,b.stems)||!Q::Detail::Same(a.leaves,b.leaves)||
        !Q::Detail::Same(a.radius,b.radius)||!Q::Detail::Same(a.height,b.height)||
        !Q::Detail::Same(a.minimumY,b.minimumY)||!Q::Detail::Same(a.maximumY,b.maximumY)||
        a.leafCount!=b.leafCount||a.stemCount!=b.stemCount||a.attachments.size()!=b.attachments.size())return false;
    for(std::size_t i=0;i<a.attachments.size();++i){const auto& x=a.attachments[i];const auto& y=b.attachments[i];
        if(x.vertex!=y.vertex||!Q::Detail::Same(x.centre,y.centre)||!Q::Detail::Same(x.branchRadius,y.branchRadius))return false;}
    return true;
}
const char* StageName(M::ShrubSearchStage stage){
    switch(stage){case M::ShrubSearchStage::Clearance:return "clearance";
    case M::ShrubSearchStage::SolidVolume:return "solid-volume";
    case M::ShrubSearchStage::CanopyWood:return "canopy-wood";
    case M::ShrubSearchStage::Terrain:return "terrain";
    case M::ShrubSearchStage::Vegetation:return "vegetation";
    case M::ShrubSearchStage::Accepted:return "accepted";}
    throw std::runtime_error("SHRUB_TRACE_INVALID_STAGE");
}
struct TraceStats {
    std::array<unsigned,6> stages{};
    std::array<unsigned,16> exactMasks{};
    std::array<unsigned,4> overlaps{};
    std::uint64_t fingerprint{};
};
TraceStats ValidateTrace(const M::ShrubSearchTrace& trace,const M::ShrubCanopyResult& result,
    const Q::Context& context,const Q::Assets& assets,const Aevrynd::HeightField3D& terrain,W::Budget& budget){
    Need(trace.records.size()==result.attempted&&trace.records.size()<=M::ShrubSearchTrace::MaximumRecords,
        "SHRUB_TRACE_RECORD_COUNT");
    TraceStats stats;Q::Detail::Hash hash;
    std::uint32_t seed=0x53485242u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    unsigned accepted{};
    for(std::size_t i=0;i<trace.records.size();++i){budget.Op();const auto& t=trace.records[i];
        Need(t.attempt==i&&t.variant<4&&t.vegetationMask<16&&
            static_cast<unsigned>(t.stage)<stats.stages.size(),"SHRUB_TRACE_RECORD_DOMAIN");
        const unsigned variant=unsigned(random()*4.f);const float scale=.85f+.25f*random();
        const float x=(i%2?1.f:-1.f)*(6.f+22.f*random()),z=-8.f+92.f*random();
        const Aevrynd::Vec3 root{x,terrain.SampleHeight(x,z),z};const auto& mesh=result.variants[variant];
        const float radius=mesh.radius*scale;
        Need(t.variant==variant&&Q::Detail::Same(t.root,root)&&Q::Detail::Same(t.scale,scale)&&
            Q::Detail::Same(t.radius,radius),"SHRUB_TRACE_RNG_OR_FOOTPRINT_BITS");
        // 從真世界與已接受 prefix 獨立重算首門及四種占用，不從 trace 倒填。
        M::ShrubSearchStage expected=M::ShrubSearchStage::Clearance;unsigned mask{};
        if(M::ForestCoverClearance(root,radius,context.vegetation[0])){
            expected=M::ShrubSearchStage::SolidVolume;
            const M::CanopyVolume::PlantVolume volume{root,
                M::CanopyVolume::ConservativeLocalVolume(mesh.radius,mesh.minimumY,mesh.maximumY),scale,{}};
            if(M::CanopyVolume::ClearsCanopyWood(volume,context.vegetation[0])){
                expected=M::ShrubSearchStage::CanopyWood;
                if(M::CanopyVolume::ClearsCanopyWood(volume,context.wood)){
                    expected=M::ShrubSearchStage::Terrain;
                    if(M::ForestCoverFootprint(terrain,root,radius,{})){
                        for(std::size_t j=0;j<context.author.understory.ferns.size();++j){budget.Op();
                            const auto& f=context.author.understory.ferns[j];
                            if(std::hypot(x-f.position.x,z-f.position.z)<radius+assets.fernAssets[j%4].radius*f.scale.x+.03f)mask|=1u;
                        }
                        for(const auto& f:context.colonies.plants){budget.Op();
                            if(std::hypot(x-f.transform.position.x,z-f.transform.position.z)<radius+f.radius+.03f)mask|=2u;}
                        for(const auto& g:context.retention.cover.plants){budget.Op();
                            if(std::hypot(x-g.transform.position.x,z-g.transform.position.z)<radius+g.radius+.03f)mask|=4u;}
                        for(unsigned j=0;j<accepted;++j){budget.Op();const auto& p=result.plants[j];
                            if(std::hypot(x-p.pose.position.x,z-p.pose.position.z)<radius+p.radius+.3f)mask|=8u;}
                        expected=mask?M::ShrubSearchStage::Vegetation:M::ShrubSearchStage::Accepted;
                    }
                }
            }
        }
        Need(t.stage==expected&&t.vegetationMask==mask,"SHRUB_TRACE_FIRST_GATE_OR_MASK");
        ++stats.stages[static_cast<unsigned>(t.stage)];
        if(t.stage==M::ShrubSearchStage::Vegetation){Need(mask!=0,"SHRUB_TRACE_EMPTY_REJECT_MASK");
            ++stats.exactMasks[mask];for(unsigned bit=0;bit<4;++bit)stats.overlaps[bit]+=(mask&(1u<<bit))!=0;
        }else Need(mask==0,"SHRUB_TRACE_MASK_OUTSIDE_VEGETATION_REJECTION");
        if(t.stage==M::ShrubSearchStage::Accepted){
            Need(accepted<result.plants.size(),"SHRUB_TRACE_ACCEPTED_OVERFLOW");
            const Aevrynd::Transform3D pose{root,{0,random()*6.2831853f,0},{scale,scale,scale}};
            const auto& p=result.plants[accepted++];
            Need(Q::Detail::Same(pose,p.pose)&&p.variant==variant&&Q::Detail::Same(p.radius,radius),
                "SHRUB_TRACE_ACCEPTED_POSE_BITS");
        }
        hash.Word(t.attempt);hash.Vector(t.root);hash.Word(t.variant);hash.Float(t.scale);hash.Float(t.radius);
        hash.Word(static_cast<unsigned>(t.stage));hash.Word(t.vegetationMask);
    }
    Need(accepted==result.plants.size()&&stats.stages[0]+stats.stages[1]+stats.stages[2]==result.rejectedClearance&&
        stats.stages[3]==result.rejectedTerrain&&stats.stages[4]==result.rejectedVegetation&&stats.stages[5]==accepted,
        "SHRUB_TRACE_STAGE_TOTALS");
    unsigned unionCount{},stageCount{};for(auto n:stats.exactMasks)unionCount+=n;for(auto n:stats.stages)stageCount+=n;
    Need(unionCount==result.rejectedVegetation&&stageCount==result.attempted,"SHRUB_TRACE_UNION_TOTALS");
    stats.fingerprint=hash.value;budget.Time();return stats;
}
std::uint64_t TerrainIdentity(const Aevrynd::HeightField3D& terrain){
    Q::Detail::Hash h;const auto& s=terrain.Settings();h.Vector(s.origin);h.Word(s.width);h.Word(s.depth);h.Float(s.cellSize);
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)h.Float(terrain.Height(x,z));return h.value;
}
std::ostringstream Text(){std::ostringstream s;s.imbue(std::locale::classic());
    s<<std::setprecision(std::numeric_limits<float>::max_digits10);return s;}
void FloatCell(std::ostream& s,float f){Need(std::isfinite(f),"SHRUB_TRACE_EXPORT_NONFINITE");
    s<<'\t'<<f<<'\t'<<std::bit_cast<std::uint32_t>(f);}
void PoseCells(std::ostream& s,const Aevrynd::Transform3D& p){
    for(float f:{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,
        p.scale.x,p.scale.y,p.scale.z})FloatCell(s,f);
}
constexpr const char* PoseColumns="x\tx_bits\ty\ty_bits\tz\tz_bits\trx\trx_bits\try\try_bits\trz\trz_bits\tsx\tsx_bits\tsy\tsy_bits\tsz\tsz_bits";
struct FileSeal {std::size_t bytes{},lines{};std::string sha;};
std::string Sha(std::string_view text){
    const auto raw=std::as_bytes(std::span<const char>(text.data(),text.size()));std::array<std::byte,32> digest{};
    Need(Aevrynd::WorldDeltaSha2563D(raw,digest),"SHRUB_TRACE_EXPORT_SHA_FAILED");
    std::ostringstream h;h<<std::uppercase<<std::hex<<std::setfill('0');for(auto b:digest)h<<std::setw(2)<<std::to_integer<unsigned>(b);
    return h.str();
}
FileSeal WriteVerified(const std::filesystem::path& path,const std::string& text,std::size_t expectedLines){
    Need(!std::filesystem::exists(path)&&!text.empty()&&text.size()<=33554432&&text.back()=='\n',"SHRUB_TRACE_EXPORT_BOUND_OR_EXISTS");
    Need(static_cast<std::size_t>(std::count(text.begin(),text.end(),'\n'))==expectedLines,"SHRUB_TRACE_EXPORT_LINE_COUNT");
    const auto digest=Sha(text);std::ofstream out(path,std::ios::binary|std::ios::out);Need(bool(out),"SHRUB_TRACE_EXPORT_OPEN");
    out.write(text.data(),static_cast<std::streamsize>(text.size()));out.flush();Need(bool(out),"SHRUB_TRACE_EXPORT_WRITE");
    out.close();Need(!out.fail(),"SHRUB_TRACE_EXPORT_CLOSE");
    Need(std::filesystem::file_size(path)==text.size(),"SHRUB_TRACE_EXPORT_BYTES");
    std::ifstream in(path,std::ios::binary);Need(bool(in),"SHRUB_TRACE_EXPORT_READ_OPEN");std::string read(text.size(),'\0');
    in.read(read.data(),static_cast<std::streamsize>(read.size()));
    Need(in.gcount()==static_cast<std::streamsize>(read.size())&&read==text&&Sha(read)==digest,"SHRUB_TRACE_EXPORT_READBACK");
    Need(in.peek()==std::char_traits<char>::eof(),"SHRUB_TRACE_EXPORT_TRAILING_BYTES");return{text.size(),expectedLines,digest};
}
void FreshOutput(const std::filesystem::path& output){
    Need(output.is_absolute()&&output.has_filename(),"SHRUB_TRACE_OUTPUT_ABSOLUTE_REQUIRED");
    Need(!std::filesystem::exists(output),"SHRUB_TRACE_OUTPUT_EXISTS");
    Need(std::filesystem::is_directory(output.parent_path()),"SHRUB_TRACE_OUTPUT_PARENT_REQUIRED");
}
void Manifest(std::ostream& summary,const char* name,const FileSeal& f){summary<<name<<"_bytes="<<f.bytes<<'\n'
    <<name<<"_lines="<<f.lines<<'\n'<<name<<"_rows="<<(f.lines-1)<<'\n'<<name<<"_sha256="<<f.sha<<'\n';}
std::string Export(const std::filesystem::path& output,const M::ShrubSearchTrace& trace,const TraceStats& stats,
    const Q::Context& c,const Q::Assets& assets,const Aevrynd::HeightField3D& terrain,const M::ShrubCanopyResult& candidate,W::Budget& budget){
    FreshOutput(output);Need(std::filesystem::create_directory(output),"SHRUB_TRACE_OUTPUT_CREATE");
    auto summary=Text();summary<<"schema=shrub-search-trace-v1\nworld_cohort="<<Q::WorldCohort
        <<"\nsource_fingerprint="<<Q::SourceFingerprint(c.source)<<"\nassets_fingerprint="<<Q::AssetsFingerprint(assets)
        <<"\ncontext_fingerprint="<<Q::ContextFingerprint(c)<<"\nterrain_fingerprint="<<TerrainIdentity(terrain)
        <<"\nsource_before_fingerprint="<<c.source.fingerprint<<"\nsource_after_fingerprint="<<Q::SourceFingerprint(c.source)
        <<"\nassets_before_fingerprint="<<assets.fingerprint<<"\nassets_after_fingerprint="<<Q::AssetsFingerprint(assets)
        <<"\ncontext_before_fingerprint="<<c.fingerprint<<"\ncontext_after_fingerprint="<<Q::ContextFingerprint(c)
        <<"\nsource_cover_fingerprint="<<c.sourceCoverFingerprint<<"\ncover_fingerprint="<<c.coverFingerprint
        <<"\nretention_fingerprint="<<c.retentionFingerprint<<"\nretention_native_fingerprint="<<c.retention.fingerprint
        <<"\noriginal_cover_identity="<<Q::OriginalCoverIdentity<<"\noriginal_input_fingerprint="<<c.retention.originalInputFingerprint
        <<"\nfinal_input_fingerprint="<<c.retention.finalInputFingerprint<<"\nprovenance_fingerprint="<<c.retention.provenanceFingerprint
        <<"\ncanopy_fingerprint="<<c.canopy.fingerprint<<"\nwood_fingerprint="<<Q::Detail::BoxesIdentity(c.wood)
        <<"\nworld_before_typed_fingerprint="<<c.typedFingerprints[0]<<"\nworld_after_typed_fingerprint="<<c.typedFingerprints[1]
        <<"\nworld_before_boxes_fingerprint="<<c.worldFingerprints[0]<<"\nworld_after_boxes_fingerprint="<<c.worldFingerprints[1]
        <<"\nbaseline_shrub_fingerprint="<<c.shrubFingerprint<<"\ncandidate_shrub_fingerprint="<<Q::Detail::ShrubIdentity(candidate)
        <<"\ncandidate_native_placement_fingerprint="<<candidate.fingerprint<<"\ntrace_fingerprint="<<stats.fingerprint
        <<"\nseed=1397248578\nmaximum_attempts=24000\ntarget=128\nactual="<<candidate.plants.size()
        <<"\nreached=false\ncandidate128_passed=false\nnative=false\nart=false\nfull_overlay=false\nfull_route=false\nsave_load=false\n";
    // 摘要中的型別與半徑是實際輸入，拒絕原因不可由總數倒推。
    summary<<"vegetation_mask_1=original-fern\nvegetation_mask_2=colony-fern\nvegetation_mask_4=grass\nvegetation_mask_8=prior-accepted-shrub\n";
    summary<<"grass_origin_0=retained\ngrass_origin_1=original-patch-fill\ngrass_origin_2=new-patch-fill\nmissing_source_ordinal=4294967295\n";
    for(unsigned i=0;i<stats.stages.size();++i)summary<<"stage_"<<StageName(static_cast<M::ShrubSearchStage>(i))<<'='<<stats.stages[i]<<'\n';
    for(unsigned i=0;i<stats.exactMasks.size();++i)summary<<"exact_mask_"<<i<<'='<<stats.exactMasks[i]<<'\n';
    for(unsigned i=0;i<stats.overlaps.size();++i)summary<<"overlap_bit_"<<(1u<<i)<<'='<<stats.overlaps[i]<<'\n';
    for(unsigned i=0;i<candidate.variants.size();++i){const auto& v=candidate.variants[i];summary<<"variant_"<<i<<"_radius="<<v.radius
        <<"\nvariant_"<<i<<"_minimum_y="<<v.minimumY<<"\nvariant_"<<i<<"_maximum_y="<<v.maximumY<<'\n';}
    auto attempts=Text();attempts<<"attempt\tx\tx_bits\ty\ty_bits\tz\tz_bits\tvariant\tscale\tscale_bits\tradius\tradius_bits\tfirst_stage\tvegetation_mask\toriginal_fern\tcolony_fern\tgrass\tprior_shrub\taccepted_ordinal\n";
    unsigned accepted{};for(const auto& t:trace.records){budget.Op();attempts<<t.attempt;
        for(float f:{t.root.x,t.root.y,t.root.z})FloatCell(attempts,f);attempts<<'\t'<<t.variant;
        FloatCell(attempts,t.scale);FloatCell(attempts,t.radius);attempts<<'\t'<<StageName(t.stage)<<'\t'<<t.vegetationMask;
        for(unsigned bit=0;bit<4;++bit)attempts<<'\t'<<((t.vegetationMask&(1u<<bit))!=0);
        if(t.stage==M::ShrubSearchStage::Accepted)attempts<<'\t'<<accepted++;else attempts<<"\tnone";attempts<<'\n';}
    Manifest(summary,"attempts",WriteVerified(output/"attempts.tsv",attempts.str(),trace.records.size()+1));
    auto plants=Text();plants<<"cohort\tordinal\tvariant\t"<<PoseColumns
        <<"\tradius\tradius_bits\tactive_patch\torigin\tsource_plant\tsource_patch\tsecondary_root_radius\tcolony_id\n";
    std::size_t plantRows{};
    const auto plant=[&](const char* cohort,std::size_t id,unsigned variant,const Aevrynd::Transform3D& pose,float radius,
        const std::string& extra){budget.Op();plants<<cohort<<'\t'<<id<<'\t'<<variant;PoseCells(plants,pose);FloatCell(plants,radius);
        plants<<'\t'<<extra<<'\n';++plantRows;};
    for(std::size_t i=0;i<c.retention.cover.plants.size();++i){const auto& p=c.retention.cover.plants[i];const auto& o=c.retention.provenance[i];
        plant("grass",i,p.variant,p.transform,p.radius,std::to_string(p.patch)+'\t'+std::to_string(static_cast<unsigned>(o.origin))+'\t'+
            std::to_string(o.sourcePlant)+'\t'+std::to_string(o.sourcePatch)+"\tnone\tnone");}
    for(std::size_t i=0;i<c.author.understory.ferns.size();++i){const auto& p=c.author.understory.ferns[i];
        plant("original-fern",i,static_cast<unsigned>(i%4),p,assets.fernAssets[i%4].radius*p.scale.x,"none\tsource\t"+std::to_string(i)+"\tnone\tnone\tnone");}
    for(std::size_t i=0;i<c.colonies.plants.size();++i){const auto& p=c.colonies.plants[i];
        plant("colony-fern",i,p.variant,p.transform,p.radius,std::to_string(p.grassPatch)+"\tcolony\t"+std::to_string(i)+"\tnone\tnone\t"+std::to_string(p.colony));}
    for(const auto* list:{&c.shrubs,&candidate})for(std::size_t i=0;i<list->plants.size();++i){const auto& p=list->plants[i];
        plant(list==&candidate?"candidate90-shrub":"baseline64-shrub",i,p.variant,p.pose,p.radius,"none\tpopulation\t"+std::to_string(i)+"\tnone\tnone\tnone");}
    for(std::size_t i=0;i<c.canopy.plants.size();++i){const auto& p=c.canopy.plants[i];auto extra=Text();
        extra<<"none\t"<<(i<4?"original-four":"authored-four")<<'\t'<<i<<"\tnone\t"<<p.rootRadius<<"\tnone";
        plant("canopy",i,0,p.pose,p.crownRadius,extra.str());}
    Need(plantRows==4482,"SHRUB_TRACE_PLANT_MAP_ROWS");Manifest(summary,"plants",WriteVerified(output/"plants.tsv",plants.str(),plantRows+1));
    auto patches=Text();patches<<"cohort\tordinal\tx\tx_bits\ty\ty_bits\tz\tz_bits\tradius\tradius_bits\taspect\taspect_bits\tyaw\tyaw_bits\trequested\taccepted\n";
    std::size_t patchRows{};for(const auto* cover:{&c.sourceCover,&c.retention.cover})for(std::size_t i=0;i<cover->patches.size();++i){budget.Op();const auto& p=cover->patches[i];
        patches<<(cover==&c.sourceCover?"original":"active")<<'\t'<<i;
        for(float f:{p.centre.x,p.centre.y,p.centre.z,p.radius,p.aspect,p.yaw})FloatCell(patches,f);
        patches<<'\t'<<p.requested<<'\t'<<p.accepted<<'\n';++patchRows;}
    Manifest(summary,"patches",WriteVerified(output/"patches.tsv",patches.str(),patchRows+1));
    auto solids=Text();solids<<"bridge_state\tordinal\trole\tsource_id\tcx\tcx_bits\tcy\tcy_bits\tcz\tcz_bits\tex\tex_bits\tey\tey_bits\tez\tez_bits\n";
    std::size_t solidRows{};for(unsigned state=0;state<2;++state)for(std::size_t i=0;i<c.worlds[state].size();++i){budget.Op();const auto& s=c.worlds[state][i];
        solids<<state<<'\t'<<i<<'\t'<<static_cast<unsigned>(s.role)<<'\t'<<s.id;
        for(float f:{s.box.center.x,s.box.center.y,s.box.center.z,s.box.extents.x,s.box.extents.y,s.box.extents.z})FloatCell(solids,f);
        solids<<'\n';++solidRows;}
    Need(solidRows==17707,"SHRUB_TRACE_SOLID_MAP_ROWS");Manifest(summary,"solids",WriteVerified(output/"solids.tsv",solids.str(),solidRows+1));
    summary<<"solid_role_0=facility\nsolid_role_1=bridge\nsolid_role_2=mature-fir\nsolid_role_3=forest-prop\nsolid_role_4=sapling\nsolid_role_5=broadleaf\nsolid_role_6=middle\nsolid_role_7=canopy-wood\nwood_per_state="<<c.wood.size()<<'\n';
    auto heights=Text();heights<<"grid_x\tgrid_z\tx\tx_bits\tz\tz_bits\ty\ty_bits\tnormal_y\tnormal_y_bits\triver_distance\triver_distance_bits\triver_width\triver_width_bits\ttrail_distance\ttrail_distance_bits\n";
    const auto& grid=terrain.Settings();std::size_t terrainRows{};
    for(unsigned z=0;z<grid.depth;++z)for(unsigned x=0;x<grid.width;++x){budget.Op();const float wx=grid.origin.x+float(x)*grid.cellSize,wz=grid.origin.z+float(z)*grid.cellSize;
        heights<<x<<'\t'<<z;for(float f:{wx,wz,terrain.Height(x,z),terrain.SampleNormal(wx,wz).y,M::RiverDistance(wx,wz),M::RiverOuterWidth(wx),M::SettlementTrailDistance(wx,wz)})FloatCell(heights,f);
        heights<<'\n';++terrainRows;}
    Need(terrainRows==48441,"SHRUB_TRACE_TERRAIN_MAP_ROWS");Manifest(summary,"terrain",WriteVerified(output/"terrain.tsv",heights.str(),terrainRows+1));
    summary<<"budget_maximum_operations="<<W::Budget::MaximumOperations<<"\nbudget_maximum_milliseconds="<<W::Budget::MaximumMilliseconds
        <<"\nbudget_observed_operations="<<budget.operations<<"\nfactory_internal_operation_count_complete=false\n";
    const auto text=summary.str();budget.Time();const auto f=WriteVerified(output/"summary.txt",text,static_cast<std::size_t>(std::count(text.begin(),text.end(),'\n')));
    budget.Time();return f.sha;
}
}
int wmain(int argc,wchar_t** argv){try{
    Need((argc==3||argc==5)&&std::wstring_view(argv[1])==L"--runtime-assets"&&
        (argc==3||std::wstring_view(argv[3])==L"--output"),"USAGE: MistwatchShrubSearchTraceTest --runtime-assets ABS [--output ABS]");
    const auto root=std::filesystem::path(argv[2]).lexically_normal();Need(root.is_absolute(),"AUTHORED_ASSET_ROOT_ABSOLUTE_REQUIRED");
    const auto output=argc==5?std::filesystem::path(argv[4]).lexically_normal():std::filesystem::path{};
    if(argc==5)FreshOutput(output);
    W::Budget budget;std::cout<<"SHRUB_SEARCH_TRACE_STAGE load-context native=false\n"<<std::flush;
    const auto assets=Q::LoadAssets(root);const auto terrain=M::CreateTerrain();const auto source=Q::CreateSourceContext(terrain,assets,budget);
    const auto context=Q::CreateContext(terrain,assets,source,FixedMappings(),budget);
    Need(context.shrubs.reachedBudget&&context.shrubs.plants.size()==64&&context.retention.cover.plants.size()==3600&&
        context.author.understory.ferns.size()==420&&context.colonies.plants.size()==300&&context.canopy.plants.size()==8&&
        context.wood.size()==8617&&context.worlds[0].size()==8853&&context.worlds[1].size()==8854,"SHRUB_TRACE_BASELINE_FIXTURE");
    const auto sourceSeal=Q::SourceFingerprint(source),assetSeal=Q::AssetsFingerprint(assets),contextSeal=Q::ContextFingerprint(context),terrainSeal=TerrainIdentity(terrain);
    const auto generate=[&](unsigned target,M::ShrubSearchTrace* trace=nullptr){budget.Time();
        auto result=M::CreateShrubCanopy(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
            context.retention.cover,context.colonies,assets.fernAssets,target,context.wood,true,trace);budget.Time();return result;};
    M::ShrubSearchTrace shortTrace;
    const auto current=generate(64),currentTraced=generate(64,&shortTrace);
    Need(Q::Detail::SameShrubs(current,context.shrubs)&&Q::Detail::SameShrubs(current,currentTraced),"SHRUB_TRACE_DEFAULT64_BITS_CHANGED");
    const auto currentStats=ValidateTrace(shortTrace,currentTraced,context,assets,terrain,budget);
    std::cout<<"SHRUB_SEARCH_TRACE_STAGE search128-original-seed native=false\n"<<std::flush;
    const auto candidate=generate(128);M::ShrubSearchTrace trace;const auto traced=generate(128,&trace);
    Need(Q::Detail::SameShrubs(candidate,traced)&&Q::Detail::ShrubIdentity(candidate)==Q::Detail::ShrubIdentity(traced),"SHRUB_TRACE_ON_OFF_BITS_CHANGED");
    Need(!candidate.reachedBudget&&candidate.plants.size()==90&&candidate.attempted==24000&&candidate.rejectedClearance==11055&&
        candidate.rejectedTerrain==6486&&candidate.rejectedVegetation==6369,"SHRUB_TRACE_ACTUAL_SHORTFALL_CHANGED");
    for(unsigned i=0;i<4;++i)Need(SameVariant(candidate.variants[i],current.variants[i]),"SHRUB_TRACE_VARIANT_PREFIX_CHANGED");
    for(std::size_t i=0;i<64;++i){const auto& a=current.plants[i];const auto& b=candidate.plants[i];
        Need(Q::Detail::Same(a.pose,b.pose)&&a.variant==b.variant&&Q::Detail::Same(a.radius,b.radius),"SHRUB_TRACE_ORIGINAL64_PREFIX_BITS");}
    Need(shortTrace.records.size()<=trace.records.size(),"SHRUB_TRACE_DEFAULT_TRACE_SIZE");
    for(std::size_t i=0;i<shortTrace.records.size();++i){const auto& a=shortTrace.records[i];const auto& b=trace.records[i];
        Need(a.attempt==b.attempt&&Q::Detail::Same(a.root,b.root)&&a.variant==b.variant&&Q::Detail::Same(a.scale,b.scale)&&
            Q::Detail::Same(a.radius,b.radius)&&a.stage==b.stage&&a.vegetationMask==b.vegetationMask,"SHRUB_TRACE_DEFAULT_RECORD_PREFIX_BITS");}
    std::cout<<"SHRUB_SEARCH_TRACE_STAGE independent-stage-and-mask native=false\n"<<std::flush;
    const auto stats=ValidateTrace(trace,traced,context,assets,terrain,budget);
    M::ShrubSearchTrace existing;existing.records.push_back({});
    Reject([&]{(void)generate(64,&existing);},"SHRUB_TRACE_NOT_EMPTY",budget);
    Need(existing.records.size()==1&&existing.records[0].attempt==0&&existing.records[0].vegetationMask==0,"SHRUB_TRACE_EXISTING_RECORD_CHANGED");
    existing.records.resize(M::ShrubSearchTrace::MaximumRecords+1);
    Reject([&]{(void)generate(64,&existing);},"SHRUB_TRACE_NOT_EMPTY",budget);
    Need(existing.records.size()==M::ShrubSearchTrace::MaximumRecords+1,"SHRUB_TRACE_OVERSIZED_RECORDS_CHANGED");
    auto badGrass=context.retention.cover;badGrass.plants[0].radius=std::numeric_limits<float>::quiet_NaN();M::ShrubSearchTrace empty;
    Reject([&]{(void)M::CreateShrubCanopy(assets.shrubLeaf,terrain,context.vegetation[0],context.author.understory,
        badGrass,context.colonies,assets.fernAssets,64,context.wood,true,&empty);},"Invalid shrub existing grass footprint",budget);
    Need(empty.records.empty(),"SHRUB_TRACE_INVALID_INPUT_PARTIAL_RECORDS");
    Need(Q::SourceFingerprint(source)==sourceSeal&&Q::SourceFingerprint(context.source)==sourceSeal&&Q::AssetsFingerprint(assets)==assetSeal&&
        Q::ContextFingerprint(context)==contextSeal&&TerrainIdentity(terrain)==terrainSeal,"SHRUB_TRACE_IMMUTABLE_CONTEXT_SEAL");
    std::string summarySha="not-written";
    if(argc==5)summarySha=Export(output,trace,stats,context,assets,terrain,candidate,budget);
    Need(Q::SourceFingerprint(source)==sourceSeal&&Q::AssetsFingerprint(assets)==assetSeal&&Q::ContextFingerprint(context)==contextSeal&&
        TerrainIdentity(terrain)==terrainSeal,"SHRUB_TRACE_EXPORT_MUTATED_CONTEXT");budget.Time();
    std::cout<<"SHRUB_SEARCH_TRACE_PASS negatives="<<negatives<<" attempts="<<trace.records.size()<<" baseline64="<<current.plants.size()
        <<" actual="<<candidate.plants.size()<<" clearance="<<candidate.rejectedClearance<<" terrain="<<candidate.rejectedTerrain
        <<" vegetation="<<candidate.rejectedVegetation<<" trace_fingerprint="<<stats.fingerprint<<" baseline_trace_fingerprint="<<currentStats.fingerprint
        <<" summary_sha256="<<summarySha<<" output_written="<<(argc==5?"true":"false")
        <<" input_seals_verified_after_export=true candidate128_passed=false art=false native=false full_overlay=false full_route=false save_load=false\n";return 0;
}catch(const std::exception& error){std::cerr<<"SHRUB_SEARCH_TRACE_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
