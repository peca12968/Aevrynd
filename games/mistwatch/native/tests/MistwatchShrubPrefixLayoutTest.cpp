#include "MistwatchShrubArcDensity.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <fstream>

namespace {
using namespace Mistwatch;using namespace Aevrynd;using Token=ValidatedShrubArcPrefix;
unsigned checks{},negatives{};
void Need(bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);}
template<class T>void Reject(T action,const char* reason){bool rejected=false;try{action();}catch(const std::invalid_argument& e){Need(std::string_view(e.what())==reason,"WRONG_REJECTION");rejected=true;}
    Need(rejected,"EXPECTED_REJECTION");++negatives;std::cout<<"SHRUB_LAYOUT_NEGATIVE "<<reason<<'\n';}
struct Assets{Mesh3D leaf;std::array<FernColonyAsset,4> ferns;};
struct Export {
    std::vector<std::uint32_t> words;
    void Word(std::uint32_t n){words.push_back(n);}void Wide(std::uint64_t n){Word(std::uint32_t(n));Word(std::uint32_t(n>>32));}
    void Float(float n){Need(std::isfinite(n),"FINITE_OUTPUT");Word(std::bit_cast<std::uint32_t>(n));}
    void Vec(Vec3 p){Float(p.x);Float(p.y);Float(p.z);}void Pose(const Transform3D& p){Vec(p.position);Vec(p.rotation);Vec(p.scale);}
    void Mesh(const Mesh3D& m){Need(m.Valid(),"VALID_MESH");Word(unsigned(m.vertices.size()));Word(unsigned(m.indices.size()));
        for(const auto& v:m.vertices){Vec({v.x,v.y,v.z});Word(v.normal);Word(v.color);Float(v.u);Float(v.v);}for(auto i:m.indices)Word(i);}
    void Shrubs(const ShrubCanopyResult& r){Word(unsigned(r.plants.size()));Word(r.attempted);Word(r.rejectedClearance);Word(r.rejectedTerrain);Word(r.rejectedVegetation);Word(r.reachedBudget);Wide(r.fingerprint);
        for(const auto& v:r.variants){Float(v.radius);Float(v.height);Float(v.minimumY);Float(v.maximumY);Word(v.leafCount);Word(v.stemCount);Mesh(v.stems);Mesh(v.leaves);
            Word(unsigned(v.attachments.size()));for(const auto& p:v.attachments){Word(p.vertex);Vec(p.centre);Float(p.branchRadius);}}
        for(const auto& p:r.plants){Pose(p.pose);Word(p.variant);Float(p.radius);}}
    void Tail(const ShrubArcTrace& t,const ShrubArcDiagnostics& d){Word(unsigned(t.candidates.size()));for(const auto& p:t.candidates){Pose(p.plant.pose);Word(p.plant.variant);Float(p.plant.radius);Word(p.id);Word(p.accepted);}
        for(auto n:{d.prefixAttempts,d.generated,d.evaluated,d.consideredAnchors,d.domainSkipped,d.computedAnchors,d.truncatedAnchors})Word(n);
        Wide(d.maximumNeighbors);Wide(d.operations);Wide(d.neighborChecks);Wide(d.geometryOperations);Wide(d.proposalFingerprint);}
    void Save(const std::filesystem::path& p)const{Need(p.is_absolute()&&!std::filesystem::exists(p),"FRESH_OUTPUT");std::ofstream out(p,std::ios::binary);
        for(auto n:words)for(unsigned b=0;b<4;++b)out.put(char((n>>(b*8))&255u));out.close();Need(bool(out),"COMPLETE_WRITE");}
};
Assets Load(const std::filesystem::path& fern,const std::filesystem::path& leaf){Assets a;
    for(unsigned i=0;i<4;++i){auto m=LoadAevryndMeshFile(fern/FernMeshName(i));Need(bool(m),"REAL_FERN_LOAD");NormalizeFernAsset(m.mesh);a.ferns[i]=FernColonyAssetFromMesh(m.mesh);}
    auto m=LoadAevryndMeshFile(leaf);Need(bool(m),"REAL_LEAF_LOAD");a.leaf=std::move(m.mesh);return a;}
Token Make(const Assets& a,const HeightField3D& t,EntryLayoutId id){
#if defined(AEVRYND_SHRUB_LAYOUT_BEFORE)
    Need(id==EntryLayoutId::Original&&OriginalEntryLayoutActive,"BEFORE_ORIGINAL_ONLY");return Token::CreateOriginal64(a.leaf,t,{}, {},{}, {},a.ferns);
#else
    return Token::CreateOriginal64(a.leaf,t,{}, {},{}, {},a.ferns,{},false,nullptr,id);
#endif
}
void Validate(const Token& token,const ShrubCanopyResult& p,const Assets& a,const HeightField3D& t,EntryLayoutId id){
#if defined(AEVRYND_SHRUB_LAYOUT_BEFORE)
    Need(id==EntryLayoutId::Original,"BEFORE_ORIGINAL_ONLY");token.Validate(p,a.leaf,t,{}, {},{}, {},a.ferns);
#else
    token.Validate(p,a.leaf,t,{}, {},{}, {},a.ferns,{},false,nullptr,id);
#endif
}
ShrubCanopyResult Fresh(const Assets& a,const HeightField3D& t,ShrubArcTrace& trace,EntryLayoutId id,unsigned target=65){
#if defined(AEVRYND_SHRUB_LAYOUT_BEFORE)
    Need(id==EntryLayoutId::Original,"BEFORE_ORIGINAL_ONLY");return CreateShrubArcDensity(a.leaf,t,{}, {},{}, {},a.ferns,target,{},false,&trace);
#else
    return CreateShrubArcDensity(a.leaf,t,{}, {},{}, {},a.ferns,target,{},false,&trace,nullptr,id);
#endif
}
ShrubCanopyResult Reuse(const Token& token,const ShrubCanopyResult& p,const Assets& a,const HeightField3D& t,ShrubArcTrace& trace,EntryLayoutId id,unsigned target=65){
#if defined(AEVRYND_SHRUB_LAYOUT_BEFORE)
    Need(id==EntryLayoutId::Original,"BEFORE_ORIGINAL_ONLY");return CreateShrubArcDensityFromValidatedPrefix(token,p,a.leaf,t,{}, {},{}, {},a.ferns,target,{},false,&trace);
#else
    return CreateShrubArcDensityFromValidatedPrefix(token,p,a.leaf,t,{}, {},{}, {},a.ferns,target,{},false,&trace,nullptr,id);
#endif
}
Export Case(const Assets& a,const HeightField3D& t,EntryLayoutId id){
    const auto token=Make(a,t,id);const auto& prefix=token.Original();Need(prefix.reachedBudget&&prefix.plants.size()==64,"TRUE_64_PREFIX");Validate(token,prefix,a,t,id);
    ShrubArcTrace ft,ct;const auto fresh=Fresh(a,t,ft,id);const auto fd=LastShrubArcDiagnostics;
    const auto cached=Reuse(token,prefix,a,t,ct,id);const auto cd=LastShrubArcDiagnostics;
    Need(fresh.reachedBudget&&cached.reachedBudget&&cached.plants.size()==65&&!ct.candidates.empty()&&cd.evaluated>0,"ACTUAL_ARC_CONTINUATION_NONEMPTY");
    Export x,y;x.Shrubs(fresh);x.Tail(ft,fd);y.Shrubs(cached);y.Tail(ct,cd);Need(x.words==y.words,"FRESH_CACHED_ALL_RAW_FIELDS");
    for(unsigned i=0;i<64;++i)Need(ShrubCompositionDetail::Same(prefix.plants[i],cached.plants[i]),"PREFIX_POSE_RETAINED");
    const auto& layout=ResolveEntryLayout(id);for(const auto& p:cached.plants)Need(ForestCoverClearance(p.pose.position,p.radius,{},layout),"ACTUAL_POLICY_CLEARANCE");
    Reject([&]{Validate(Token{},prefix,a,t,id);},"SHRUB_ARC_PREFIX_TOKEN_REQUIRED");
    auto moving=token;const auto moved=std::move(moving);Validate(moved,prefix,a,t,id);
    Reject([&]{Validate(moving,prefix,a,t,id);},"SHRUB_ARC_PREFIX_TOKEN_REQUIRED");
    auto bad=prefix;bad.plants[0].pose.position.x=std::nextafter(bad.plants[0].pose.position.x,100.f);
    Reject([&]{Validate(token,bad,a,t,id);},"SHRUB_ARC_PREFIX_BITS_CHANGED");
    auto nan=t;nan.SetHeight(0,0,NAN);Reject([&]{Validate(token,prefix,a,nan,id);},"SHRUB_ARC_PREFIX_INPUT_CHANGED");
    ShrubArcTrace empty;Reject([&]{(void)Reuse(token,prefix,a,t,empty,id,63);},"SHRUB_ARC_PREFIX_TARGET");
    Reject([&]{(void)Fresh(a,t,empty,id,129);},"Invalid shrub bounded population");
    auto nonempty=ct;Reject([&]{(void)Reuse(token,prefix,a,t,nonempty,id);},"SHRUB_GAP_TRACE_NOT_EMPTY");
#if !defined(AEVRYND_SHRUB_LAYOUT_BEFORE)
    const auto other=id==EntryLayoutId::Original?EntryLayoutId::WestSupplyPilot:EntryLayoutId::Original;
    Reject([&]{Validate(token,prefix,a,t,other);},"SHRUB_ARC_PREFIX_LAYOUT_CHANGED");
    Reject([&]{(void)Reuse(token,prefix,a,t,empty,other,64);},"SHRUB_ARC_PREFIX_LAYOUT_CHANGED");Need(empty.candidates.empty(),"WRONG_LAYOUT_TRACE_UNTOUCHED");
    Reject([&]{Validate(token,prefix,a,t,static_cast<EntryLayoutId>(99));},"Unknown entry layout identity");
    Reject([&]{(void)Make(a,t,static_cast<EntryLayoutId>(99));},"Unknown entry layout identity");
    Reject([&]{(void)Fresh(a,t,empty,static_cast<EntryLayoutId>(99),0);},"Unknown entry layout identity");
#endif
    Export out;out.Word(0x534c5031u);out.Shrubs(prefix);out.Shrubs(cached);out.Tail(ct,cd);return out;
}
}
int main(int argc,char** argv){try{
    Need(argc==3||argc==4,"FERN_DIR_LEAF_OPTIONAL_OUTPUT");const auto a=Load(argv[1],argv[2]);HeightField3D terrain({{-40,0,-20},161,241,.5f});terrain.Fill(0);
    const std::filesystem::path output=argc==4?argv[3]:"";Need(output.empty()||(output.is_absolute()&&!std::filesystem::exists(output)&&std::filesystem::create_directory(output)),"FRESH_EXPORT_DIR");
    const auto original=Case(a,terrain,EntryLayoutId::Original);if(!output.empty())original.Save(output/"original.bin");
#if !defined(AEVRYND_SHRUB_LAYOUT_BEFORE)
    const auto pilot=Case(a,terrain,EntryLayoutId::WestSupplyPilot);if(!output.empty())pilot.Save(output/"pilot.bin");
    ShrubArcTrace trace,reference;const auto active=CreateShrubArcDensity(a.leaf,terrain,{}, {},{}, {},a.ferns,65,{},false,&trace);const auto diag=LastShrubArcDiagnostics;
    const auto explicitActive=Fresh(a,terrain,reference,ActiveEntryLayout.id);const auto explicitDiag=LastShrubArcDiagnostics;
    Export x,y;x.Shrubs(active);x.Tail(trace,diag);y.Shrubs(explicitActive);y.Tail(reference,explicitDiag);Need(x.words==y.words,"OLD_ARITY_DEFAULT_ACTIVE_ALL_FIELDS");
#endif
    std::cout<<"SHRUB_PREFIX_LAYOUT_UNIT_PASS checks="<<checks<<" negatives="<<negatives<<" original_words="<<original.words.size()<<" actual_prefix=64 actual_arc_total=65 full_world=0 native=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"SHRUB_PREFIX_LAYOUT_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
