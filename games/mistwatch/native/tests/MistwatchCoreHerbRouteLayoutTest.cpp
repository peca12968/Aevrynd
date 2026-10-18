#include "MistwatchCoreHerbRouteLayout.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// 此檔只驗證作者配置與入口政策；完整世界、整面支撐與原生畫面須另行驗收。
namespace {
namespace H=Mistwatch::CoreHerbClusters;
namespace R=Mistwatch::CoreHerbRouteLayout;
unsigned checks{},negatives{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}

// 獨立固定作者基準，不由受測 Layout 或 LayoutFingerprint 回填預期值。
constexpr H::AuthorLayout GoldenOriginal{{{{-10.5f,0,0.f},{-10.5f,0,6.f},{-19.f,0,23.f},
    {-8.f,0,49.f},{-16.5f,0,68.f},{-19.25f,0,77.75f},{17.5f,0,-.5f},{14.f,0,11.f},{18.4f,0,24.f},
    {17.f,0,54.f},{18.f,0,65.f},{19.2f,0,78.5f}}},
    {{.5235987755982988f,0,0,0,1.5707963267948966f,-.916297857297023f,0,0,0,0,0,-.5235987755982988f}}};
constexpr H::AuthorLayout GoldenRoute=[] {
    auto value=GoldenOriginal;value.centres[7]={10.f,0.f,10.f};return value;
}();
constexpr std::uint64_t OriginalFingerprint=2774174745427001144ull;
constexpr std::uint64_t RouteFingerprint=8183224680590209384ull;
std::array<float,4> Fields(const H::AuthorLayout& value,unsigned i){
    return {value.centres[i].x,value.centres[i].y,value.centres[i].z,value.rotationOffsets[i]};
}
std::uint64_t IndependentFingerprint(const H::AuthorLayout& value){
    std::uint64_t h=14695981039346656037ull;
    const auto word=[&](std::uint32_t v){
        for(unsigned byte=0;byte<4;++byte){h^=(v>>(byte*8))&255u;h*=1099511628211ull;}
    };
    word(0x43484c31u);
    for(unsigned i=0;i<12;++i)for(float f:Fields(value,i))word(std::bit_cast<std::uint32_t>(f));
    return h;
}
void SameLayout(const H::AuthorLayout& actual,const H::AuthorLayout& expected,const char* why){
    for(unsigned i=0;i<12;++i){
        const auto a=Fields(actual,i),b=Fields(expected,i);
        for(unsigned field=0;field<4;++field)Need(Bits(a[field],b[field]),why);
    }
}
template<class Work>void Reject(Work work,std::string_view expected){
    bool rejected=false;
    try{work();}catch(const std::invalid_argument& e){
        Need(std::string_view(e.what())==expected,"unexpected rejection reason");rejected=true;
    }
    Need(rejected,"invalid input accepted");++negatives;
}
void Constants(){
    Need(H::ClusterCount==12&&H::PerCluster==16&&H::Target==192,"plant population changed");
    Need(H::AttemptsPerCluster==512&&H::MaximumProposals==6144,"proposal budget changed");
    Need(H::AngularStrata==32&&H::RadialStrata==16,"stratification changed");
    Need(H::SamplingProfile=="stratified-polar-512-v1","sampling profile changed");
    Need(H::Profile=="core-herb-clusters-v1","base plant profile changed");
    Need(H::SourceFingerprints[0]==5775871692697253043ull&&
         H::SourceFingerprints[1]==3304028173848676471ull,"source identity changed");
    Need(Bits(H::WindRadius,.19f)&&Bits(H::WindUp,.011f),"wind envelope changed");
    Need(H::RootWindProfile=="root-pinned-v1","root wind profile changed");
    Need(Bits(H::RootPinnedHeight,.035f)&&Bits(H::RootWindFadeHeight,.35f),"root wind heights changed");
}
void PolicyCases(){
    const std::wstring candidate=L"core-herb-route-edge-v1";
    for(bool capture:{false,true})for(bool replay:{false,true})for(bool enabled:{false,true}){
        Need(R::Resolve(capture,replay,enabled,L"")==R::Profile::Original,"blank changed original");
        Need(R::Resolve(capture,replay,enabled,L"original")==R::Profile::Original,"original context restricted");
        if(enabled&&(capture!=replay))
            Need(R::Resolve(capture,replay,enabled,candidate)==R::Profile::RouteEdgeV1,"valid candidate rejected");
        else Reject([&]{(void)R::Resolve(capture,replay,enabled,candidate);},"CORE_HERB_LAYOUT_CONTEXT");
    }
    // 不修剪、不忽略大小寫、不接受模糊前綴、尾碼或嵌入控制字。
    std::vector<std::wstring> invalid{
        L"unknown",L"Original",L"ORIGINAL",L" original",L"original ",L"original\t",L"original\n",
        L"core-herb-route-edge",L"core-herb-route-edge-v",L"core-herb-route-edge-v2",
        L"CORE-HERB-ROUTE-EDGE-V1",L"Core-herb-route-edge-v1",L"xcore-herb-route-edge-v1",
        L"core-herb-route-edge-v1-extra",L" core-herb-route-edge-v1",L"core-herb-route-edge-v1 ",
        L"core-herb-route-edge-v1\t",L"core-herb-route-edge-v1\n",L"core-herb-route-edge-v1\r",
        L" ",L"\t",L"\n",L"\r",L"\x1f",L"core-herb-route-edge-v1\u00a0",L"core-herb-route-edge-v1\u200b",
        std::wstring(L"original\0suffix",15),candidate+std::wstring(1,L'\0'),
        std::wstring(1,L'\0')+candidate,candidate+std::wstring(L"\0suffix",7),
        std::wstring(65536,L'x'),candidate+std::wstring(65536,L'x')};
    for(const auto& name:invalid)for(bool capture:{false,true})for(bool replay:{false,true})for(bool enabled:{false,true})
        Reject([&]{(void)R::Resolve(capture,replay,enabled,std::wstring_view(name.data(),name.size()));},"CORE_HERB_LAYOUT_PROFILE");
    for(int value:{-1,2,3,255}){
        const auto profile=static_cast<R::Profile>(value);
        Reject([&]{(void)R::Layout(profile);},"CORE_HERB_LAYOUT_ENUM");
        Reject([&]{(void)R::Name(profile);},"CORE_HERB_LAYOUT_ENUM");
    }
}
void LayoutCases(){
    Need(IndependentFingerprint(GoldenOriginal)==OriginalFingerprint,"independent original golden hash changed");
    Need(IndependentFingerprint(GoldenRoute)==RouteFingerprint,"independent route golden hash changed");
    SameLayout(H::DefaultLayout,GoldenOriginal,"default author layout changed");
    const auto& original=R::Layout(R::Profile::Original);
    const auto& route=R::Layout(R::Profile::RouteEdgeV1);
    Need(&original==&H::DefaultLayout,"original query must retain default reference");
    Need(&original!=&route,"candidate aliases original layout");
    Need(R::Name(R::Profile::Original)=="original","original name changed");
    Need(R::Name(R::Profile::RouteEdgeV1)=="core-herb-route-edge-v1","candidate name changed");
    SameLayout(original,GoldenOriginal,"original fields differ");
    SameLayout(route,GoldenRoute,"candidate changed more than approved coordinates");
    Need(IndependentFingerprint(original)==OriginalFingerprint,"original query independent hash differs");
    Need(IndependentFingerprint(route)==RouteFingerprint,"candidate query independent hash differs");
    Need(H::Detail::LayoutFingerprint(original)==OriginalFingerprint,"production original hash differs from independent oracle");
    Need(H::Detail::LayoutFingerprint(route)==RouteFingerprint,"production candidate hash differs from independent oracle");
    unsigned changed{};
    for(unsigned i=0;i<12;++i){
        const auto before=Fields(original,i),after=Fields(route,i);
        for(unsigned field=0;field<4;++field){
            const bool different=!Bits(before[field],after[field]);
            Need(different==(i==7&&(field==0||field==2)),"unexpected author field delta");
            changed+=different?1u:0u;
        }
    }
    Need(changed==2,"candidate must change exactly two scalar fields");
    const auto originalSnapshot=original,routeSnapshot=route;
    // 重排、交錯查詢只能取同一常量；不得依查詢先後改變 Default 或另一布局。
    constexpr std::array<unsigned,8> order{7,0,5,2,6,1,4,3};
    for(unsigned pass=0;pass<8;++pass)for(unsigned context:order){
        const bool capture=(context&1u)!=0,replay=(context&2u)!=0,enabled=(context&4u)!=0;
        const auto base=R::Resolve(capture,replay,enabled,pass%2?L"original":L"");
        Need(&R::Layout(base)==&original,"repeated original reference changed");
        Need(&R::Layout(R::Profile::RouteEdgeV1)==&route,"repeated candidate reference changed");
        if(enabled&&(capture!=replay)){
            const auto chosen=R::Resolve(capture,replay,enabled,L"core-herb-route-edge-v1");
            Need(&R::Layout(chosen)==&route,"resolved candidate reference changed");
        }
        SameLayout(original,originalSnapshot,"original mutated during queries");
        SameLayout(route,routeSnapshot,"candidate mutated during queries");
    }
    PolicyCases();
    SameLayout(original,originalSnapshot,"invalid policy query mutated original");
    SameLayout(route,routeSnapshot,"invalid policy query mutated candidate");
    SameLayout(H::DefaultLayout,GoldenOriginal,"policy query modified default");
}
}
int main(){
    try{
        Constants();LayoutCases();
        std::cout<<"CORE_HERB_ROUTE_LAYOUT_CPU_CONTRACT_OK; checks="<<checks<<"; negatives="<<negatives
            <<"; original_author="<<OriginalFingerprint<<"; route_author="<<RouteFingerprint
            <<"; geometry_verified=0; world_verified=0; exact_support_verified=0; native_verified=0; art_accepted=0\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"CORE_HERB_ROUTE_LAYOUT_CONTRACT_FAILED: "<<e.what()<<'\n';return 1;}
}
