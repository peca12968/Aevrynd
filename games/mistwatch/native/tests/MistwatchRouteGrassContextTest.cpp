// 合成上下文封印測試；不代表完整森林、離線套件授權、GPU或美術驗收。
#include "MistwatchRouteGrassContext.h"
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace Aevrynd;
namespace C=Mistwatch::RouteGrassContext;
namespace A=Mistwatch::RouteGrassAuthor;
unsigned checks{},negatives{},heightMutations{},authorProposals{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class Work>void Throws(Work&& work,std::string_view reason){
    bool caught{};try{work();}catch(const std::invalid_argument& e){
        Need(std::string_view(e.what())==reason,"CONTEXT_NEGATIVE_REASON");caught=true;
    }Need(caught,"CONTEXT_INVALID_ACCEPTED");++negatives;
}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
// 預期值由獨立byte序列計算，不呼叫受測Wide/Word/Float/Fingerprint。
struct Bytes {
    std::vector<unsigned char> value;
    void U32(std::uint32_t word){for(unsigned i=0;i<4;++i)value.push_back(static_cast<unsigned char>(word>>(i*8)));}
    void U64(std::uint64_t word){for(unsigned i=0;i<8;++i)value.push_back(static_cast<unsigned char>(word>>(i*8)));}
    void F32(float f){U32(std::bit_cast<std::uint32_t>(f));}
    std::uint64_t Hash()const{std::uint64_t h=14695981039346656037ull;
        for(auto byte:value){h^=byte;h*=1099511628211ull;}return h;}
};
std::uint64_t SealOracle(const C::Seal& seal){Bytes b;b.U32(0x52474331u);b.U32(1);
    for(auto word:seal.words)b.U64(word);return b.Hash();}
std::uint64_t TerrainOracle(const HeightField3D& terrain){const auto& s=terrain.Settings();Bytes b;
    b.U32(0x52475431u);b.U32(1);for(float f:{s.origin.x,s.origin.y,s.origin.z,s.cellSize})b.F32(f);
    b.U32(s.width);b.U32(s.depth);
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)b.F32(terrain.Height(x,z));return b.Hash();}
std::uint64_t PolicyOracle(){Bytes b;b.U32(0x52475031u);b.U32(1);b.U32(1);
    for(float f:{.19f,.011f,.035f,.35f,.04f,.88f,.24f,.03f,.65f,.000002f,.00005f})b.F32(f);return b.Hash();}
void Seals(){
    constexpr std::array<std::string_view,16> names{
        "source_0","source_1","source_2","source_3","source_4","ground","terrain","world_before",
        "world_after","wood","retained","forest_grass","roadside_broadleaf","core_herb","author","policy"};
    Need(C::Schema==1&&C::Names==names,"CONTEXT_SCHEMA_OR_NAMES");C::Seal approved;
    for(unsigned i=0;i<16;++i)approved.words[i]=0xfedcba9876543210ull+i;
    C::RequireSame(approved,approved);Need(approved==C::Seal{approved.words},"CONTEXT_EQUALITY");
    Need(C::Fingerprint(approved)==SealOracle(approved)&&C::Fingerprint(approved)!=0,"CONTEXT_FINGERPRINT_ORACLE");
    for(unsigned i=0;i<16;++i){auto current=approved;current.words[i]^=std::uint64_t{1}<<(i*3);
        const auto reason=std::string("ROUTE_GRASS_CONTEXT_MISMATCH_")+std::string(names[i]);
        Throws([&]{C::RequireSame(approved,current);},reason);
        Throws([&]{C::RequireSame(current,approved);},reason);
        Need(!(approved==current)&&C::Fingerprint(current)==SealOracle(current)&&
            C::Fingerprint(current)!=C::Fingerprint(approved),"CONTEXT_FIELD_HASH_NOT_BOUND");
        auto empty=approved;empty.words[i]=0;
        Throws([&]{C::RequireSame(approved,empty);},"ROUTE_GRASS_CONTEXT_EMPTY");
        Throws([&]{C::RequireSame(empty,approved);},"ROUTE_GRASS_CONTEXT_EMPTY");
        Throws([&]{(void)C::Fingerprint(empty);},"ROUTE_GRASS_CONTEXT_EMPTY");
    }
    const C::Seal empty{};Throws([&]{C::RequireSame(empty,empty);},"ROUTE_GRASS_CONTEXT_EMPTY");
    Throws([&]{(void)C::Fingerprint(empty);},"ROUTE_GRASS_CONTEXT_EMPTY");
    auto multiple=approved;multiple.words[1]^=1;multiple.words[15]=0;
    Throws([&]{C::RequireSame(approved,multiple);},"ROUTE_GRASS_CONTEXT_MISMATCH_source_1");
    auto wide=approved;wide.words[5]=(std::numeric_limits<std::uint64_t>::max)();
    Need(C::Fingerprint(wide)==SealOracle(wide),"CONTEXT_WIDE_WORD_LOST");C::RequireSame(wide,wide);
}
HeightField3D TerrainFixture(){HeightField3D t({{-3.f,2.f,-4.f},4,3,.75f});
    for(unsigned z=0;z<3;++z)for(unsigned x=0;x<4;++x)
        Need(t.SetHeight(x,z,float(int(x)-int(z)*4)*.125f),"CONTEXT_FIXTURE_HEIGHT");return t;}
void Terrain(){
    const auto baseline=TerrainFixture();const auto hash=C::Terrain(baseline);
    Need(hash==TerrainOracle(baseline)&&hash!=0,"CONTEXT_TERRAIN_ORACLE");
    for(unsigned z=0;z<3;++z)for(unsigned x=0;x<4;++x){auto changed=baseline;
        const auto old=changed.Height(x,z);Need(changed.SetHeight(x,z,std::nextafter(old,
            (std::numeric_limits<float>::infinity)())),"CONTEXT_MUTATION_HEIGHT");
        Need(C::Terrain(changed)!=hash&&C::Terrain(changed)==TerrainOracle(changed),"CONTEXT_HEIGHT_NOT_BOUND");++heightMutations;
    }
    for(unsigned field=0;field<6;++field){auto changed=baseline;
        // 底層物件並非const；故意損壞設定以檢查封印入口，不配置巨大陣列。
        auto& s=const_cast<HeightFieldSettings3D&>(changed.Settings());
        switch(field){case 0:s.origin.x+=1;break;case 1:s.origin.y+=1;break;case 2:s.origin.z+=1;break;
            case 3:s.cellSize+=.25f;break;case 4:s.width=3;s.depth=4;break;case 5:s.width=6;s.depth=2;break;}
        Need(C::Terrain(changed)!=hash&&C::Terrain(changed)==TerrainOracle(changed),"CONTEXT_GRID_NOT_BOUND");
    }
    auto signedZero=baseline;Need(signedZero.SetHeight(0,0,-0.f),"CONTEXT_ZERO_HEIGHT");
    Need(C::Terrain(signedZero)!=hash&&C::Terrain(signedZero)==TerrainOracle(signedZero),"CONTEXT_SIGNED_ZERO_LOST");
    constexpr auto maximum=(std::numeric_limits<float>::max)();
    for(unsigned axis=0;axis<4;++axis){auto changed=baseline;auto& s=const_cast<HeightFieldSettings3D&>(changed.Settings());
        if(axis==0)s.origin.x=maximum;else if(axis==1)s.origin.y=maximum;else if(axis==2)s.origin.z=-maximum;else s.cellSize=maximum;
        // 新契約在查詢前拒絕超大有限座標／cell，不能將hash成功當成可查詢地表。
        Throws([&]{(void)C::Terrain(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN_RANGE");
        Throws([&]{(void)C::Author(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN_RANGE");
    }
    for(unsigned field=0;field<3;++field)for(float sign:{-1.f,1.f}){
        auto boundary=baseline;auto& s=const_cast<HeightFieldSettings3D&>(boundary.Settings());
        if(field==0)s.origin.x=sign<0?-1000000.f:1000000.f-float(s.width-1)*s.cellSize;
        else if(field==1)s.origin.y=sign*1000000.f;
        else s.origin.z=sign<0?-1000000.f:1000000.f-float(s.depth-1)*s.cellSize;
        Need(C::Terrain(boundary)==TerrainOracle(boundary),"CONTEXT_TERRAIN_RANGE_BOUNDARY");
        auto outside=boundary;auto& altered=const_cast<HeightFieldSettings3D&>(outside.Settings());
        auto& coordinate=field==0?altered.origin.x:(field==1?altered.origin.y:altered.origin.z);
        coordinate=std::nextafter(coordinate,sign*(std::numeric_limits<float>::infinity)());
        Throws([&]{(void)C::Terrain(outside);},"ROUTE_GRASS_CONTEXT_TERRAIN_RANGE");
        Throws([&]{(void)C::Author(outside);},"ROUTE_GRASS_CONTEXT_TERRAIN_RANGE");
    }
    for(float invalid:{(std::numeric_limits<float>::infinity)(),-(std::numeric_limits<float>::infinity)(),
                       (std::numeric_limits<float>::quiet_NaN)()}){
        for(unsigned axis=0;axis<4;++axis){auto changed=baseline;auto& s=const_cast<HeightFieldSettings3D&>(changed.Settings());
            if(axis==0)s.origin.x=invalid;else if(axis==1)s.origin.y=invalid;else if(axis==2)s.origin.z=invalid;else s.cellSize=invalid;
            Throws([&]{(void)C::Terrain(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN");
            Throws([&]{(void)C::Author(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN");}
        for(unsigned z=0;z<3;++z)for(unsigned x=0;x<4;++x){auto changed=baseline;Need(changed.SetHeight(x,z,invalid),"CONTEXT_INVALID_HEIGHT_SET");
            Throws([&]{(void)C::Terrain(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN_HEIGHT");
            Throws([&]{(void)C::Author(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN_HEIGHT");}
    }
    for(float invalid:{0.f,-1.f}){auto changed=baseline;const_cast<HeightFieldSettings3D&>(changed.Settings()).cellSize=invalid;
        Throws([&]{(void)C::Terrain(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN");
        Throws([&]{(void)C::Author(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN");}
    for(unsigned field=0;field<2;++field)for(unsigned invalid:{0u,1u,(std::numeric_limits<unsigned>::max)()}){
        auto changed=baseline;auto& s=const_cast<HeightFieldSettings3D&>(changed.Settings());
        if(field==0)s.width=invalid;else s.depth=invalid;
        Throws([&]{(void)C::Terrain(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN");
        Throws([&]{(void)C::Author(changed);},"ROUTE_GRASS_CONTEXT_TERRAIN");}
}
// 固定128筆已核准作者序列；這份預期不從受測Ordinals回填。
constexpr std::array<unsigned,128> expectedOrdinals{
    1538,2784,5188,6244,9910,11140,12964,15360,97,3171,5761,6335,9139,11425,12195,14883,
    1346,2592,5442,10592,13166,15072,1,3267,5571,9237,11713,12103,15075,10,2786,5348,
    11564,13170,14882,1187,3459,4035,11617,14031,14787,1260,2498,5564,11468,13268,15842,1333,
    3175,4227,11905,13607,16035,258,2690,11568,13556,15170,1047,2955,5667,11521,13041,14789,
    1508,2694,11968,13982,15554,1143,2387,4515,11235,12945,14695,938,2502,12066,13330,15362,
    2481,4325,11427,12755,14601,270,2794,12068,13332,15652,2579,5573,11621,12567,14775,658,
    3694,10150,13526,15940,2197,4231,11141,13623,15263,466,3646,11516,13334,15460,2389,4135,
    12005,564,11612,13624,15078,2485,4423,11813,756,11710,15270,2103,4039,11431,854,2295};
std::array<A::Proposal,128> AuthorOracle(float height){
    std::array<A::Proposal,128> result{};std::array<bool,128> assigned{};std::uint32_t state=0x52534331u;
    const auto random=[&](){state=1664525u*state+1013904223u;return float(state>>8)/16777216.f;};
    for(unsigned ordinal=0;ordinal<16128;++ordinal){const unsigned row=ordinal/96,column=(ordinal%96)/2;
        const float side=(ordinal%2)==0?-1.f:1.f;
        const float x=side*(2.4f+.45f*float(column))+(random()-.5f)*.18f;
        const float z=-6.f+.525f*float(row)+(random()-.5f)*.18f;
        const auto variant=(std::min)(static_cast<unsigned>(random()*5.f),4u);
        for(unsigned i=0;i<128;++i)if(expectedOrdinals[i]==ordinal){
            result[i]={{x,height,z},ordinal,row*8/168+(side>0?8u:0u),variant,.5f+.25f*std::sin(x*1.7f+z)};assigned[i]=true;}
    }
    for(bool value:assigned)Need(value,"CONTEXT_ORACLE_ORDINAL_NOT_FOUND");return result;
}
std::uint64_t AuthorHashOracle(const std::array<A::Proposal,128>& p){Bytes b;b.U32(0x52474131u);b.U32(1);
    b.U32(168);b.U32(48);b.U32(128);for(const auto& value:p){b.U32(value.ordinal);b.U32(value.bin);b.U32(value.variant);
        for(float f:{value.root.x,value.root.y,value.root.z,value.scale})b.F32(f);}return b.Hash();}
void Author(){
    Need(A::Target==128&&A::Rows==168&&A::Columns==48&&A::MaximumProposals==16128&&
         A::Ordinals==expectedOrdinals,"CONTEXT_AUTHOR_CONFIGURATION");
    HeightField3D terrain({{-32,2,-8},65,96,1.f});terrain.Fill(2.f);
    const auto expected=AuthorOracle(2.f),actual=A::Create(terrain),replay=A::Create(terrain);
    std::array<unsigned,2> sides{};std::array<unsigned,16> bins{};std::array<unsigned,5> variants{};
    for(unsigned i=0;i<128;++i){const auto& p=actual[i];const auto& q=expected[i];const auto& r=replay[i];
        Need(p.ordinal==q.ordinal&&p.bin==q.bin&&p.variant==q.variant,"CONTEXT_AUTHOR_IDENTITY_ORACLE");
        Need(Same(p.root.x,q.root.x)&&Same(p.root.y,q.root.y)&&Same(p.root.z,q.root.z)&&Same(p.scale,q.scale),"CONTEXT_AUTHOR_POSE_ORACLE");
        Need(p.ordinal==r.ordinal&&p.bin==r.bin&&p.variant==r.variant&&Same(p.root.x,r.root.x)&&
            Same(p.root.y,r.root.y)&&Same(p.root.z,r.root.z)&&Same(p.scale,r.scale),"CONTEXT_AUTHOR_REPLAY");
        Need(terrain.Contains(p.root.x,p.root.z)&&p.bin<16&&p.variant<5,"CONTEXT_AUTHOR_OUT_OF_BOUNDS");
        for(unsigned j=0;j<i;++j)Need(p.ordinal!=actual[j].ordinal,"CONTEXT_AUTHOR_DUPLICATE");
        ++sides[p.root.x>0?1u:0u];++bins[p.bin];++variants[p.variant];++authorProposals;
    }
    Need(sides==std::array<unsigned,2>{64,64},"CONTEXT_AUTHOR_SIDE_COUNT");
    for(auto count:bins)Need(count>0,"CONTEXT_AUTHOR_BIN_EMPTY");
    for(auto count:variants)Need(count>0,"CONTEXT_AUTHOR_VARIANT_EMPTY");
    Need(C::Author(terrain)==AuthorHashOracle(expected)&&C::Author(terrain)!=0,"CONTEXT_AUTHOR_HASH_ORACLE");
    auto changed=terrain;changed.Fill(3.f);const auto altered=AuthorOracle(3.f);
    Need(C::Author(changed)==AuthorHashOracle(altered)&&C::Author(changed)!=C::Author(terrain),"CONTEXT_AUTHOR_HEIGHT_NOT_BOUND");
}
void Policy(){Need(C::Policy()==PolicyOracle()&&C::Policy()!=0,"CONTEXT_POLICY_ORACLE");
    Need(Same(Mistwatch::CoreHerbClusters::WindRadius,.19f)&&Same(Mistwatch::CoreHerbClusters::WindUp,.011f)&&
         Same(Mistwatch::CoreHerbClusters::RootPinnedHeight,.035f)&&Same(Mistwatch::CoreHerbClusters::RootWindFadeHeight,.35f)&&
         Same(Mistwatch::OuterUnderstory::ContactLimit,.04f)&&Same(Mistwatch::OuterUnderstory::SlopeLimit,.88f)&&
         Same(Mistwatch::OuterUnderstory::HeightLimit,.24f),"CONTEXT_ORIGINAL_POLICY_CONSTANTS");}
}
int main(){try{Seals();Terrain();Author();Policy();
    std::cout<<"ROUTE_GRASS_CONTEXT_SYNTHETIC_OK; checks="<<checks<<"; negatives="<<negatives
             <<"; height_mutations="<<heightMutations<<"; author_proposals="<<authorProposals
             <<"; full_world=0; package_admission=0; gpu_readback=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
