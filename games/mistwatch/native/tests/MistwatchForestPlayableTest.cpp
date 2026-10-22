#include "MistwatchForestPlayable.h"
#include <array>
#include <iostream>

int main(){
    try{
    using namespace Mistwatch::ForestPlayable;
    unsigned checks{};
    const auto need=[&](bool yes){++checks;if(!yes)throw std::runtime_error("forest playable policy assertion");};
    std::array<std::wstring,6> empty{};
    const auto reject=[&](auto action,std::string_view message){bool failed=false;try{action();}catch(const std::invalid_argument& e){failed=std::string_view(e.what())==message;}need(failed);};
    need(!Resolve(false,false,false,L"",L"",empty));
    need(Resolve(true,false,false,L"",L"",empty));
    for(auto route:{L"shelter",L"completion",L"checkpoint",L"resume-save",L"reject-save",L"grove-walk"})need(Resolve(false,false,true,Profile,route,empty));
    reject([&]{Resolve(false,false,true,L"other",L"checkpoint",empty);},"FOREST_PLAYABLE_UNKNOWN_PROFILE");
    reject([&]{Resolve(false,true,true,Profile,L"checkpoint",empty);},"FOREST_PLAYABLE_CONTEXT");
    reject([&]{Resolve(false,false,false,Profile,L"",empty);},"FOREST_PLAYABLE_CONTEXT");
    reject([&]{Resolve(true,false,true,L"",L"checkpoint",empty);},"FOREST_PLAYABLE_CONTEXT");
    reject([&]{Resolve(true,false,false,Profile,L"",empty);},"FOREST_PLAYABLE_CONTEXT");
    reject([&]{Resolve(true,false,false,L"",L"checkpoint",empty);},"FOREST_PLAYABLE_CONTEXT");
    reject([&]{Resolve(false,false,true,Profile,L"unknown",empty);},"FOREST_PLAYABLE_ROUTE");
    reject([&]{Resolve(false,false,true,Profile,L"checkpoint",std::span<const std::wstring>(empty.data(),5));},"FOREST_PLAYABLE_DEPENDENCY_SCOPE");
    for(unsigned i=0;i<6;++i){auto low=empty;low[i]=L"old-override";
        reject([&]{Resolve(false,false,true,Profile,L"checkpoint",low);},"FOREST_PLAYABLE_LOW_LEVEL_OVERRIDE");
        reject([&]{Resolve(true,false,false,L"",L"",low);},"FOREST_PLAYABLE_LOW_LEVEL_OVERRIDE");
        need(!Resolve(false,false,true,L"",L"checkpoint",low));
    }
    std::cout<<"FOREST_PLAYABLE_POLICY_PASS; checks="<<checks<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
