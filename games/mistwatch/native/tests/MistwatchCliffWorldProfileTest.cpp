#include "MistwatchCliffWorldProfile.h"
#include <iostream>

int main(){
    using namespace Mistwatch::CliffWorld;
    unsigned checks=0,rejections=0;
    try{
        for(unsigned bits=0;bits<8;++bits)for(unsigned request=0;request<3;++request)
            for(unsigned overrideIndex=0;overrideIndex<12;++overrideIndex){
                const bool capture=(bits&1)!=0,replay=(bits&2)!=0,manual=(bits&4)!=0;
                const std::wstring_view requested=request==0?L"":request==1?Profile:L"unsupported";
                std::array<std::wstring_view,11> components{};
                if(overrideIndex)components[overrideIndex-1]=L"original";
                // 無此功能時不攔截既有組合；新功能僅允許純CLI或單獨回放selector。
                const bool enabled=manual||request!=0;
                const bool expectedReject=request==2||(enabled&&(capture||(manual&&replay)||
                    (manual&&request!=0)||(!manual&&!replay)||overrideIndex!=0));
                bool rejected=false,value=false;
                try{value=Resolve(capture,replay,manual,requested,components);}
                catch(const std::invalid_argument&){rejected=true;}
                if(rejected!=expectedReject||(!rejected&&value!=enabled))
                    throw std::runtime_error("Cliff world selector truth table mismatch");
                ++checks;rejections+=rejected;
            }
        std::cout<<"CLIFF_WORLD_PROFILE_PASS; checks="<<checks<<"; rejected="<<rejections<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
