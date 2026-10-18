#include "MistwatchPlayableArtProfile.h"
#include <array>
#include <iostream>
#include <limits>
#include <string>

namespace P=::Mistwatch::PlayableArt;
namespace {
unsigned checks{},rejections{};
void Need(bool pass,const char* reason){++checks;if(!pass)throw std::runtime_error(reason);}
void Reject(bool capture,P::Feature feature,std::wstring_view value){
    bool invalid=false;try{(void)P::Resolve(capture,feature,value);}catch(const std::invalid_argument&){invalid=true;}
    Need(invalid,"Unsupported profile or unknown feature accepted");++rejections;
}
struct Case {P::Feature feature;std::wstring_view accepted;};
constexpr std::array<Case,10> Cases{{
    {P::Feature::Shelter,L"framed-corrugated-v1"},
    {P::Feature::Workshop,L"framed-workshop-v1"},
    {P::Feature::Road,L"entrance-road-material-domain-v1"},
    {P::Feature::Roadside,L"roadside-broadleaf-v1"},
    {P::Feature::Understory,L"entrance-understory-v2"},
    {P::Feature::BoulderColor,L"source-srgb-v1"},
    {P::Feature::BoulderGeometry,L"lod0-source-v1"},
    {P::Feature::Shrubs,L"woodland-feasible-arc-v3"},
    {P::Feature::LandscapeTwig,L"crown-distance-lod-v1"},
    {P::Feature::LandscapeStand,L"landscape-stand-fill-v2"}
}};
}
int main()try{
    constexpr std::array<std::wstring_view,9> stale{{L"",L"original",L"invalid",L"ORIGINAL",L" original",
        L"original ",L"entrance-understory-v1",L"roadside-ground-cover-v1",L"source-srgb-v2"}};
    for(const auto& c:Cases){
        for(auto requested:stale)Need(P::Resolve(false,c.feature,requested)==c.accepted,"Normal player inherited stale override");
        for(const auto& other:Cases)Need(P::Resolve(false,c.feature,other.accepted)==c.accepted,"Cross-feature override changed normal player");
        Need(P::Resolve(true,c.feature,L"")==L"original","Empty capture profile is not original");
        Need(P::Resolve(true,c.feature,L"original")==L"original","Explicit original capture unavailable");
        Need(P::Resolve(true,c.feature,c.accepted)==c.accepted,"Accepted capture profile unavailable");
        for(const auto& other:Cases)if(other.feature!=c.feature)Reject(true,c.feature,other.accepted);
        for(auto typo:{L"invalid",L"ORIGINAL",L" original",L"original ",L"entrance-understory-v1",L"source-srgb-v2"})Reject(true,c.feature,typo);
        auto suffixed=std::wstring(c.accepted)+L"-typo";Reject(true,c.feature,suffixed);
        // 不可用C字串前綴比對略過內嵌NUL後面的內容。
        auto embedded=std::wstring(c.accepted);embedded.push_back(L'\0');embedded+=L"unexpected";
        Reject(true,c.feature,embedded);
        Need(P::Resolve(false,c.feature,embedded)==c.accepted,"Malformed residue changed normal player");
        if(c.feature==P::Feature::Roadside)
            Need(P::Resolve(true,c.feature,L"roadside-ground-cover-v1")==L"roadside-ground-cover-v1","Legacy roadside capture unavailable");
        else Reject(true,c.feature,L"roadside-ground-cover-v1");
        for(auto legacy:{L"woodland-density-v1",L"woodland-gap-density-v2"}){
            if(c.feature==P::Feature::Shrubs)
                Need(P::Resolve(true,c.feature,legacy)==legacy,"Legacy shrub capture unavailable");
            else Reject(true,c.feature,legacy);
            Need(P::Resolve(false,c.feature,legacy)==c.accepted,"Legacy shrub override leaked into normal player");
        }
        if(c.feature==P::Feature::LandscapeTwig)
            Need(P::Resolve(true,c.feature,L"crown-lod1-source-v1")==L"crown-lod1-source-v1","Legacy full-detail crown capture unavailable");
        else Reject(true,c.feature,L"crown-lod1-source-v1");
        Need(P::Resolve(false,c.feature,L"crown-lod1-source-v1")==c.accepted,"Legacy crown override leaked into normal player");
    }
    for(int raw:{-1,10,99,(std::numeric_limits<int>::min)(),(std::numeric_limits<int>::max)()})
        for(bool capture:{false,true}){
            for(auto requested:stale)Reject(capture,static_cast<P::Feature>(raw),requested);
            for(const auto& c:Cases)Reject(capture,static_cast<P::Feature>(raw),c.accepted);
        }
    // 回傳生命期不得依賴呼叫端的requested儲存空間。
    std::wstring_view stable;
    {std::wstring temporary=L"framed-workshop-v1";stable=P::Resolve(true,P::Feature::Workshop,temporary);}
    Need(stable==L"framed-workshop-v1","Resolved view dangles after request lifetime");
    std::cout<<"PLAYABLE_ART_PROFILE_CPU_PASS features=10 checks="<<checks<<" rejected="<<rejections
        <<" normal_override_ignored=1 capture_exact=1 unknown_rejected=1 native=0 art=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"PLAYABLE_ART_PROFILE_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
