// 此測試只核對有界解碼、已匹配網格與快速欄位；不重算完整世界接地證明，也不宣稱 GPU 或美術通過。
#include "MistwatchRouteGrassCookLoad.h"
#include <Aevrynd/MeshLoader.h>
#include <iostream>
#include <sstream>
#include <set>

namespace {
using namespace Aevrynd;
namespace L=Mistwatch::RouteGrassCookLoad;
namespace K=Mistwatch::RouteGrassCook;
namespace D=Mistwatch::CoreHerbClusters::Detail;
namespace A=Mistwatch::RouteGrassAuthor;
unsigned checks{},negatives{},plants{},parserCases{},identityCases{},approvedCases{},upperFixtures{},syntheticWeldFixtures{};
void Need(bool good,std::string_view label){++checks;if(!good)throw std::runtime_error(std::string(label));}
template<class Work>void Throws(Work&& work,std::string_view reason){
    bool caught{};try{work();}catch(const std::invalid_argument& error){
        if(std::string_view(error.what())!=reason)throw std::runtime_error(std::string("WRONG_NEGATIVE_REASON expected=")+std::string(reason)+" actual="+error.what());
        caught=true;
    }Need(caught,std::string("NEGATIVE_ACCEPTED ")+std::string(reason));++negatives;
}
std::map<std::string,std::string> Receipt(const std::filesystem::path& path){
    std::ifstream file(path);Need(bool(file),"COOK_TEST_RECEIPT_OPEN");std::map<std::string,std::string> result;std::string line;
    while(std::getline(file,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();const auto split=line.find('=');
        Need(split!=std::string::npos&&result.emplace(line.substr(0,split),line.substr(split+1)).second,"COOK_TEST_RECEIPT_FIELDS");}
    Need(file.eof(),"COOK_TEST_RECEIPT_READ");return result;
}
void Put(std::vector<std::byte>& bytes,std::size_t offset,std::uint32_t value){
    Need(offset<=bytes.size()&&bytes.size()-offset>=4,"COOK_TEST_MUTATION_BOUNDS");
    for(unsigned b=0;b<4;++b)bytes[offset+b]=std::byte((value>>(8*b))&255u);
}
std::uint32_t Word(std::span<const std::byte> bytes,std::size_t offset){
    Need(offset<=bytes.size()&&bytes.size()-offset>=4,"COOK_TEST_READ_BOUNDS");std::uint32_t value{};
    for(unsigned b=0;b<4;++b)value|=std::to_integer<std::uint32_t>(bytes[offset+b])<<(8*b);return value;
}
void PutFloat(std::vector<std::byte>& bytes,std::size_t offset,float value){Put(bytes,offset,std::bit_cast<std::uint32_t>(value));}
HeightField3D Terrain(const std::filesystem::path& path){
    const auto bytes=L::Read(path);BoundedBinaryReader3D in(bytes);Need(in.Word()==0x52475431u&&in.Word()==1,"COOK_TEST_TERRAIN_SCHEMA");
    HeightFieldSettings3D settings;settings.origin=L::Vector(in);settings.cellSize=L::Float(in);settings.width=in.Word();settings.depth=in.Word();
    Need(settings.width>=2&&settings.depth>=2&&std::uint64_t(settings.width)*settings.depth<=4194304&&
        std::uint64_t(settings.width)*settings.depth*4==in.Remaining(),"COOK_TEST_TERRAIN_BUDGET");
    HeightField3D result(settings);for(unsigned z=0;z<settings.depth;++z)for(unsigned x=0;x<settings.width;++x)
        Need(result.SetHeight(x,z,L::Float(in)),"COOK_TEST_TERRAIN_HEIGHT");
    result.RecalculateRange();Need(in.Remaining()==0,"COOK_TEST_TERRAIN_TRAILING");return result;
}
Mesh3D Mesh(const std::filesystem::path& path){auto loaded=LoadAevryndMeshFile(path);Need(bool(loaded),"COOK_TEST_MESH_LOAD");return std::move(loaded.mesh);}
struct Record {std::size_t begin{},vertices{},indices{},end{};unsigned vertexCount{},indexCount{};};
std::array<Record,128> Records(std::span<const std::byte> bytes){
    std::array<Record,128> result;std::size_t cursor=192;
    for(auto& r:result){r.begin=cursor;r.vertexCount=Word(bytes,cursor+264);r.indexCount=Word(bytes,cursor+268);
        r.vertices=cursor+272;r.indices=r.vertices+std::size_t(r.vertexCount)*28;r.end=r.indices+std::size_t(r.indexCount)*4;
        Need(r.end<=bytes.size(),"COOK_TEST_RECORD_BOUNDS");cursor=r.end;}
    Need(cursor==bytes.size(),"COOK_TEST_RECORD_EXHAUSTION");return result;
}
void Identity(const std::vector<std::byte>& bytes,const L::Approval& approval,const std::array<Record,128>& records){
    Throws([&]{(void)L::Decode(bytes,{});},"ROUTE_GRASS_LOAD_NO_APPROVAL");
    auto missing=approval;missing.build={};Throws([&]{(void)L::Decode(bytes,missing);},"ROUTE_GRASS_LOAD_NO_APPROVAL");
    auto wrong=approval;wrong.build[0]^=std::byte{1};Throws([&]{(void)L::Decode(bytes,wrong);},"ROUTE_GRASS_LOAD_BUILD_IDENTITY");
    // 固定標頭及第一筆固定欄位逐 byte；其餘 32 個等距區段及全部 128 筆幾何邊界。
    // 逐一重新雜湊完整七 MB 套件的每個 byte 會變成平方級工作，故大型網格用有界區段覆蓋。
    std::set<std::size_t> positions;for(std::size_t i=0;i<464;++i)positions.insert(i);
    for(std::size_t i=0;i<32;++i)positions.insert(i*(bytes.size()-1)/31);
    for(const auto& r:records)for(auto offset:{r.begin+156,r.vertices,r.indices,r.end-1})positions.insert(offset);
    for(auto offset:positions){auto altered=bytes;altered[offset]^=std::byte{0x80};
        Throws([&]{(void)L::Decode(altered,approval);},"ROUTE_GRASS_LOAD_BUNDLE_IDENTITY");++identityCases;}
    for(auto length:{std::size_t{0},std::size_t{191},std::size_t{192},bytes.size()-1}){
        const auto truncated=std::span(bytes).first(length);Throws([&]{(void)L::Decode(truncated,approval);},
            length<192?"ROUTE_GRASS_LOAD_SIZE":"ROUTE_GRASS_LOAD_BUNDLE_IDENTITY");}
    auto extra=bytes;extra.push_back(std::byte{});Throws([&]{(void)L::Decode(extra,approval);},"ROUTE_GRASS_LOAD_BUNDLE_IDENTITY");
    std::vector<std::byte> over(K::MaximumBytes+1);Throws([&]{(void)L::Decode(over,approval);},"ROUTE_GRASS_LOAD_SIZE");
}
void Parser(const std::vector<std::byte>& bytes,const L::Approval& approval,const Record& first){
    // 只為到達 parser 分支而建立可信測試 fixture；產品編譯端絕不能核准套件自述或重新雜湊的輸入。
    const auto rejected=[&](const std::vector<std::byte>& altered,std::string_view reason){
        auto fixture=approval;fixture.bundle=K::Hash(altered);Throws([&]{(void)L::Decode(altered,fixture);},reason);++parserCases;
    };
    const auto word=[&](std::size_t offset,std::uint32_t value,std::string_view reason){auto altered=bytes;Put(altered,offset,value);rejected(altered,reason);};
    const auto field=[&](std::size_t offset,std::uint32_t value,std::string_view reason){word(first.begin+offset,value,reason);};
    for(auto offset:{0u,4u,8u,12u})word(offset,0xffffffffu,"ROUTE_GRASS_LOAD_SCHEMA");
    word(16,Word(bytes,16)^1u,"ROUTE_GRASS_LOAD_BUILD_IDENTITY");
    // 清空一個 context 需同時清空上下兩字。
    {auto altered=bytes;Put(altered,48,0);Put(altered,52,0);rejected(altered,"ROUTE_GRASS_CONTEXT_EMPTY");}
    word(184,Word(bytes,184)^1u,"ROUTE_GRASS_LOAD_SURFACE_IDENTITY");
    word(176,Word(bytes,176)^1u,"ROUTE_GRASS_LOAD_PLAN_IDENTITY");
    for(auto offset:{8u,12u})field(offset,0xffffffffu,"ROUTE_GRASS_LOAD_VARIANT_WIND");
    for(auto offset:{136u,140u,156u,164u,168u})field(offset,2,"ROUTE_GRASS_LOAD_BOOLEAN");
    field(160,static_cast<unsigned>(Mistwatch::OuterUnderstory::Reject::Count),"ROUTE_GRASS_LOAD_REJECTION");
    for(auto offset:{40u,44u,48u,52u,56u,60u,64u,68u,72u,76u,80u,84u,88u,92u,112u,116u,172u,176u,180u,184u,188u})
        for(auto bad:{0x7fc00000u,0x7f800000u,0xff800000u})field(offset,bad,"ROUTE_GRASS_LOAD_NONFINITE");
    field(124,0x7ff00000u,"ROUTE_GRASS_LOAD_NONFINITE");
    for(auto offset:{256u,260u})field(offset,0,"ROUTE_GRASS_LOAD_MESH_SCHEMA");
    for(auto offset:{264u,268u})for(auto count:{0u,0xffffffffu})field(offset,count,"ROUTE_GRASS_LOAD_MESH_BUDGET");
    word(first.indices,first.vertexCount,"ROUTE_GRASS_LOAD_MESH_INDEX");
    for(auto offset:{0u,4u,16u,24u,32u})field(offset,Word(bytes,first.begin+offset)^1u,"ROUTE_GRASS_LOAD_PLANT_IDENTITY");
    for(auto offset:{52u,60u,64u,68u,72u,88u,92u,136u,140u}){
        const auto value=offset==136?0u:offset==140?1u:std::bit_cast<std::uint32_t>(offset==88?5.f:offset==64?0.f:2.f);
        field(offset,value,"ROUTE_GRASS_LOAD_POSE");
    }
    for(auto offset:{96u,100u,104u,108u,128u,132u,144u,148u,152u,196u,200u,204u})field(offset,0xffffffffu,"ROUTE_GRASS_LOAD_PROOF");
    field(192,0,"ROUTE_GRASS_LOAD_PROOF");
    field(156,0,"ROUTE_GRASS_LOAD_PROOF");
    for(const auto& [offset,value]:{std::pair{176u,0.f},std::pair{180u,-1.f},std::pair{180u,1.f},std::pair{184u,-1.f},std::pair{188u,1.f},std::pair{116u,-100.f}}){
        auto altered=bytes;PutFloat(altered,first.begin+offset,value);rejected(altered,"ROUTE_GRASS_LOAD_PROOF");}
    for(auto offset:{0u,4u,8u,20u,24u})word(first.vertices+offset,0x7fc00000u,"ROUTE_GRASS_LOAD_NONFINITE");
    {auto altered=bytes;altered.push_back(std::byte{});rejected(altered,"ROUTE_GRASS_LOAD_TRAILING");}
    for(auto length:{std::size_t{192},first.begin+16,first.vertices-4,bytes.size()-1}){
        auto altered=std::vector<std::byte>(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(length));
        rejected(altered,length==bytes.size()-1?"ROUTE_GRASS_LOAD_MESH_BUDGET":"BINARY_READER_TRUNCATED");}
}
template<class Change>void PlantNegative(const Mistwatch::RouteGrassPlan::Instance& original,const Mesh3D& source,
    const A::Proposal& author,float groundY,Change&& change,std::string_view reason){
    auto altered=original;change(altered);Throws([&]{L::ValidatePlant(altered,source,author,groundY);},reason);
}
void PlantFields(const Mistwatch::RouteGrassPlan::Instance& original,const Mesh3D& source,const A::Proposal& author,float groundY){
    using I=Mistwatch::RouteGrassPlan::Instance;
    const auto test=[&](auto&& change,std::string_view reason){PlantNegative(original,source,author,groundY,change,reason);};
    test([](I& a){++a.ordinal;},"ROUTE_GRASS_ADMIT_AUTHOR");test([](I& a){a.bin^=1;},"ROUTE_GRASS_ADMIT_AUTHOR");
    test([](I& a){a.plant.variant=(a.plant.variant+1)%5;},"ROUTE_GRASS_ADMIT_AUTHOR");
    test([](I& a){a.plant.sourceFingerprint^=1;},"ROUTE_GRASS_ADMIT_AUTHOR");
    for(unsigned component=0;component<9;++component)test([&](I& a){
        auto& p=a.plant;float* target=component==0?&p.pose.position.x:component==1?&p.pose.position.y:component==2?&p.pose.position.z:
            component==3?&p.pose.rotation.y:component==4?&p.pose.scale.x:component==5?&p.root.x:component==6?&p.root.y:component==7?&p.root.z:&p.radius;
        *target=std::nextafter(*target,std::numeric_limits<float>::infinity());},"ROUTE_GRASS_ADMIT_POSE");
    Throws([&]{L::ValidatePlant(original,source,author,groundY+.01f);},"ROUTE_GRASS_ADMIT_POSE");
    for(unsigned which=0;which<2;++which)test([&](I& a){auto& stats=a.plant.conformCounters;
        float& target=which?stats.sourceMaximumY:stats.sourceMinimumY;target=std::nextafter(target,std::numeric_limits<float>::infinity());},"ROUTE_GRASS_ADMIT_SOURCE_GEOMETRY");
    test([](I& a){a.plant.derivedMesh.indices[0]^=1;},"ROUTE_GRASS_ADMIT_SOURCE_GEOMETRY");
    test([](I& a){a.plant.derivedMesh.vertices.pop_back();},"ROUTE_GRASS_ADMIT_SOURCE_GEOMETRY");
    Throws([&]{L::ValidatePlant(original,Mesh3D{},author,groundY);},"ROUTE_GRASS_ADMIT_SOURCE_GEOMETRY");
    test([](I& a){a.plant.derivedMesh.indices[0]=static_cast<unsigned>(a.plant.derivedMesh.vertices.size());},"ROUTE_GRASS_ADMIT_SOURCE_GEOMETRY");
    for(unsigned which=0;which<5;++which)test([&](I& a){auto& v=a.plant.derivedMesh.vertices[0];
        if(which==4)v.color^=1;else{float& f=which==0?v.x:which==1?v.z:which==2?v.u:v.v;f=std::nextafter(f,std::numeric_limits<float>::infinity());}},"ROUTE_GRASS_ADMIT_MATERIAL_XZ");
    test([](I& a){a.plant.derivedMesh.vertices[0].normal^=0x01000000u;},"ROUTE_GRASS_ADMIT_WIND");
    test([](I& a){a.plant.derivedMesh.vertices[0].normal^=1;},"ROUTE_GRASS_ADMIT_NORMAL_IDENTITY");
    for(unsigned which=0;which<7;++which)test([&](I& a){auto& p=a.plant;auto& s=p.conformCounters;
        unsigned& target=which==0?s.sampledVertices:which==1?s.movedVertices:which==2?s.affectedFaces:which==3?s.rebuiltNormals:
            which==4?p.pinnedVertices:which==5?a.proof.windPinnedVertices:p.changedWindVertices;++target;},"ROUTE_GRASS_ADMIT_COUNTERS");
}
bool UpperField(const Mistwatch::RouteGrassPlan::Instance& original,const Mesh3D& source,const A::Proposal& author,float groundY){
    float minimum=source.vertices.front().y;for(const auto& v:source.vertices)minimum=(std::min)(minimum,v.y);
    // 真來源在較小 scale 下可能全部低於 35 cm；只選確實有上部頂點的當前植物，不能假設每種都存在。
    for(unsigned i=0;i<source.vertices.size();++i)if((double(source.vertices[i].y)-minimum)*author.scale>=double(.35f)){
        PlantNegative(original,source,author,groundY,[&](auto& a){a.plant.derivedMesh.vertices[i].y=std::nextafter(
            a.plant.derivedMesh.vertices[i].y,std::numeric_limits<float>::infinity());},"ROUTE_GRASS_ADMIT_UPPER_VERTEX");
        ++upperFixtures;return true;
    }return false;
}
void SyntheticWeld(const Mistwatch::RouteGrassPlan::Instance& original,const Mesh3D& source,const A::Proposal& author,float groundY){
    // 五種正式草網格沒有重複位置；此合成 fixture 只測 helper 的 weld 分支，不是 production golden 或解碼套件。
    unsigned lowest{};for(unsigned i=1;i<source.vertices.size();++i)if(source.vertices[i].y<source.vertices[lowest].y)lowest=i;
    auto raw=source;auto a=original;const auto s=source.vertices[lowest];auto v=a.plant.derivedMesh.vertices[lowest];
    const auto expectedW=D::RootWindBits(s.normal,0.);v.normal=(s.normal&0x00ffffffu)|expectedW;
    raw.vertices.push_back(s);a.plant.derivedMesh.vertices.push_back(v);
    auto& p=a.plant;++p.conformCounters.sampledVertices;p.conformCounters.movedVertices+=!L::Same(s.y,v.y);
    p.pinnedVertices+=UnpackNormalSnorm8(s.normal).w>=0;p.changedWindVertices+=expectedW!=(s.normal&0xff000000u);
    a.proof.windPinnedVertices=p.pinnedVertices;p.sourceFingerprint=D::MeshFingerprint(raw);a.proof.sourceFingerprint=p.sourceFingerprint;
    p.derivedFingerprint=D::MeshFingerprint(p.derivedMesh);a.proof.derivedFingerprint=p.derivedFingerprint;
    L::ValidatePlant(a,raw,author,groundY);Need(true,"COOK_TEST_SYNTHETIC_WELD_BASELINE");
    p.derivedMesh.vertices.back().y=std::nextafter(p.derivedMesh.vertices.back().y,std::numeric_limits<float>::infinity());
    Throws([&]{L::ValidatePlant(a,raw,author,groundY);},"ROUTE_GRASS_ADMIT_SPLIT_WELD");++syntheticWeldFixtures;
}
void RecordFields(const Mistwatch::RouteGrassPlan::Plan& plan){
    using I=Mistwatch::RouteGrassPlan::Instance;const auto& original=plan.instances[0];
    const auto test=[&](auto&& change,std::string_view reason){auto altered=original;change(altered);
        Throws([&]{L::ValidateRecord(altered,0,plan.context,plan.surfaceFingerprint);},reason);};
    test([](I& a){a.plant.variant=5;},"ROUTE_GRASS_LOAD_VARIANT_WIND");
    Throws([&]{L::ValidateRecord(original,128,plan.context,plan.surfaceFingerprint);},"ROUTE_GRASS_LOAD_VARIANT_WIND");
    test([](I& a){a.proof.sourceFingerprint^=1;},"ROUTE_GRASS_LOAD_PLANT_IDENTITY");
    test([](I& a){a.proof.derivedFingerprint^=1;},"ROUTE_GRASS_LOAD_PLANT_IDENTITY");
    test([](I& a){a.proof.surfaceFingerprint^=1;},"ROUTE_GRASS_LOAD_PLANT_IDENTITY");
    test([](I& a){a.plant.derivedMesh.vertices[0].color^=1;},"ROUTE_GRASS_LOAD_PLANT_IDENTITY");
    for(unsigned component=0;component<4;++component)test([&](I& a){auto& p=a.plant;
        float& target=component==0?p.pose.rotation.x:component==1?p.pose.rotation.z:component==2?p.pose.scale.y:p.pose.scale.z;target=2.f;},"ROUTE_GRASS_LOAD_POSE");
    test([](I& a){a.plant.rootWind=static_cast<Mistwatch::CoreHerbClusters::RootWind>(0);},"ROUTE_GRASS_LOAD_POSE");
    test([](I& a){a.proof.contact.safe=false;},"ROUTE_GRASS_LOAD_PROOF");
    for(unsigned field=0;field<7;++field)test([&](I& a){auto& c=a.proof.contact;auto& s=a.plant.conformCounters;
        if(field==6)s.maximumGroundDelta=std::numeric_limits<double>::quiet_NaN();
        else{float& value=field==0?c.minimumNormalY:field==1?c.maximumHeightDelta:field==2?c.minimumGapBound:
            field==3?c.maximumFootGapBound:field==4?s.sourceMinimumY:s.sourceMaximumY;value=std::numeric_limits<float>::quiet_NaN();}},"ROUTE_GRASS_LOAD_PROOF");
    for(unsigned i=0;i<plan.context.words.size();++i){auto changed=plan.context;changed.words[i]^=1;
        Throws([&]{Mistwatch::RouteGrassContext::RequireSame(plan.context,changed);},std::string("ROUTE_GRASS_CONTEXT_MISMATCH_")+std::string(Mistwatch::RouteGrassContext::Names[i]));
        changed=plan.context;changed.words[i]=0;Throws([&]{Mistwatch::RouteGrassContext::RequireSame(plan.context,changed);},"ROUTE_GRASS_CONTEXT_EMPTY");}
}
void ApprovedFields(const Mistwatch::RouteGrassPlan::Plan& plan,const L::Approval& approval){
    L::RequireApprovedPlan(plan,approval);Need(true,"COOK_TEST_APPROVED_PLAN_POSITIVE");
    Throws([&]{L::RequireApprovedPlan(plan,{});},"ROUTE_GRASS_LOAD_NO_APPROVAL");
    auto wrong=approval;wrong.build[0]^=std::byte{1};
    Throws([&]{L::RequireApprovedPlan(plan,wrong);},"ROUTE_GRASS_ADMIT_APPROVED_BYTES");
    wrong=approval;wrong.bundle[0]^=std::byte{1};
    Throws([&]{L::RequireApprovedPlan(plan,wrong);},"ROUTE_GRASS_ADMIT_APPROVED_BYTES");
    const auto test=[&](auto&& change,std::string_view reason="ROUTE_GRASS_ADMIT_APPROVED_BYTES"){
        auto altered=plan;change(altered);Throws([&]{L::RequireApprovedPlan(altered,approval);},reason);++approvedCases;
    };
    // 幾何及弱 FNV 封印一起重算仍不能取得編譯端核准；不把重新雜湊當成來源授權。
    test([](auto& altered){auto& a=altered.instances[0];a.plant.derivedMesh.vertices[0].color^=1;
        a.plant.derivedFingerprint=D::MeshFingerprint(a.plant.derivedMesh);a.proof.derivedFingerprint=a.plant.derivedFingerprint;
        altered.fingerprint=L::Fingerprint(altered);});
    for(unsigned which=0;which<2;++which)test([&](auto& altered){auto& c=altered.instances[0].proof.contact;
        float& gap=which?c.maximumFootGapBound:c.minimumGapBound;
        gap=std::nextafter(gap,which?0.f:std::numeric_limits<float>::infinity());
        Need(gap>=-Mistwatch::OuterUnderstory::ContactLimit&&gap<=Mistwatch::OuterUnderstory::ContactLimit,"COOK_TEST_APPROVED_LEGAL_GAP");});
    for(unsigned which=0;which<4;++which)test([&](auto& altered){auto& c=altered.instances[0].proof.contact;
        if(which==0)c.restingFootObserved=!c.restingFootObserved;
        else if(which==1)c.restingFootAboveLimit=!c.restingFootAboveLimit;
        else if(which==2)c.maximumRestingFootGap=std::nextafter(c.maximumRestingFootGap,std::numeric_limits<float>::infinity());
        else c.rejection=static_cast<Mistwatch::OuterUnderstory::Reject>((static_cast<unsigned>(c.rejection)+1)%static_cast<unsigned>(Mistwatch::OuterUnderstory::Reject::Count));});
    for(unsigned which=0;which<6;++which)test([&](auto& altered){auto& c=altered.instances[0].proof.contact;
        if(which==0)++c.cachedGroundFaces;else if(which==1)++c.aabbRejected;else if(which==2)++c.exactOverlayFaces;
        else if(which==3)++c.groundHeightPlanes;else if(which==4)++c.sourceHeightPlanes;else ++c.cachedHeightEvaluations;});
    for(unsigned which=0;which<2;++which)test([&](auto& altered){auto& stats=altered.instances[0].plant.conformCounters;
        float& value=which?stats.sourceMaximumY:stats.sourceMinimumY;value=std::nextafter(value,std::numeric_limits<float>::infinity());});
    for(unsigned which=0;which<4;++which)test([&](auto& altered){auto& stats=altered.instances[0].plant.conformCounters;
        unsigned& count=which==0?stats.sampledVertices:which==1?stats.movedVertices:which==2?stats.affectedFaces:stats.rebuiltNormals;++count;});
    test([](auto& altered){altered.instances[0].plant.conformCounters.maximumGroundDelta=std::nextafter(
        altered.instances[0].plant.conformCounters.maximumGroundDelta,std::numeric_limits<double>::infinity());});
    test([](auto& altered){altered.instances[0].proof.contact.safe=false;},"ROUTE_GRASS_COOK_PROOF_IDENTITY");
    test([](auto& altered){altered.instances[0].plant.derivedMesh.vertices[0].color^=1;},"ROUTE_GRASS_COOK_PROOF_IDENTITY");
}
}
int main(int argc,char** argv){try{
    Need(argc==3&&std::string_view(argv[1])=="--cooked-dir","COOK_TEST_ARGUMENTS");const std::filesystem::path directory(argv[2]);
    Need(directory.is_absolute(),"COOK_TEST_ABSOLUTE_PATH");const auto receipt=Receipt(directory/"cook.txt");
    Need(receipt.at("schema")=="mistwatch.route-grass-cook.1"&&receipt.at("population")=="128","COOK_TEST_RECEIPT_SCHEMA");
    const L::Approval approval{K::ParseDigest(receipt.at("bundle_sha256")),K::ParseDigest(receipt.at("source_runtime_build_sha256"))};
    const auto bytes=L::Read(directory/"route-grass.aecook");const auto plan=L::Decode(bytes,approval);Need(plan.instances.size()==128,"COOK_TEST_POPULATION");
    const auto encoded=K::Encode(plan,approval.build);Need(std::equal(bytes.begin(),bytes.end(),encoded.Data().begin(),encoded.Data().end()),"COOK_TEST_ENCODER_BYTE_EQUALITY");
    const auto records=Records(bytes);std::array<Mesh3D,5> sources;for(unsigned i=0;i<5;++i){sources[i]=Mesh(directory/("source-"+std::to_string(i)+".aemesh"));
        Need(D::MeshFingerprint(sources[i])==Mistwatch::RouteGrassSupport::SourceFingerprints[i],"COOK_TEST_SOURCE_IDENTITY");}
    const auto groundMesh=Mesh(directory/"ground.aemesh");Need(D::MeshFingerprint(groundMesh)==plan.context.words[5],"COOK_TEST_GROUND_IDENTITY");
    const Mistwatch::RouteGrassSupport::Ground ground(groundMesh);const auto terrain=Terrain(directory/"terrain.bin");
    Need(Mistwatch::RouteGrassContext::Terrain(terrain)==plan.context.words[6]&&Mistwatch::RouteGrassContext::Author(terrain)==plan.context.words[14],"COOK_TEST_TERRAIN_AUTHOR_IDENTITY");
    const auto authors=A::Create(terrain);std::array<float,128> groundY{};std::array<bool,5> variants{},upperVariants{};
    for(unsigned i=0;i<128;++i){groundY[i]=static_cast<float>(ground.Surface().At({authors[i].root.x,0,authors[i].root.z}).Approximate());
        L::ValidatePlant(plan.instances[i],sources[authors[i].variant],authors[i],groundY[i]);++plants;
        const auto disk=Mesh(directory/("grass-"+std::to_string(i)+".aemesh"));K::Bytes diskBytes;diskBytes.Mesh(disk);K::Bytes planBytes;planBytes.Mesh(plan.instances[i].plant.derivedMesh);
        Need(std::equal(diskBytes.Data().begin(),diskBytes.Data().end(),planBytes.Data().begin(),planBytes.Data().end()),"COOK_TEST_SAVED_GEOMETRY_BYTE_EQUALITY");
        if(!variants[authors[i].variant]){PlantFields(plan.instances[i],sources[authors[i].variant],authors[i],groundY[i]);
            SyntheticWeld(plan.instances[i],sources[authors[i].variant],authors[i],groundY[i]);variants[authors[i].variant]=true;}
        if(!upperVariants[authors[i].variant])upperVariants[authors[i].variant]=UpperField(plan.instances[i],sources[authors[i].variant],authors[i],groundY[i]);
    }
    for(bool present:variants)Need(present,"COOK_TEST_FIVE_VARIANTS");
    Need(upperFixtures>0&&syntheticWeldFixtures==5,"COOK_TEST_UPPER_WELD_FIXTURES");
    ApprovedFields(plan,approval);RecordFields(plan);Identity(bytes,approval,records);Parser(bytes,approval,records[0]);
    Throws([&]{(void)L::Read("relative-route-grass.aecook");},"ROUTE_GRASS_LOAD_PATH");
    Throws([&]{(void)L::Read(directory/"missing-test-route-grass.aecook");},"ROUTE_GRASS_LOAD_OPEN");
    std::cout<<"COOK_LOAD_TEST_PASS checks="<<checks<<" negatives="<<negatives<<" plants="<<plants<<" variants=5 identity_cases="<<identityCases
        <<" parser_cases="<<parserCases<<" approved_cases="<<approvedCases<<" upper_fixtures="<<upperFixtures<<" synthetic_weld_fixtures="<<syntheticWeldFixtures
        <<" full_world_proof_recomputed=0 GPU_verified=0 art_accepted=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"COOK_LOAD_TEST_FAIL "<<error.what()<<" checks="<<checks<<" negatives="<<negatives<<'\n';return 1;}}
