#include "MistwatchGroundMicrorelief.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

int main(){try{
    namespace M=Mistwatch::GroundMicrorelief;unsigned checks{},negatives{};
    const auto need=[&](bool value){++checks;if(!value)throw std::runtime_error("MICRORELIEF_TEST_FAILED");};
    const auto reject=[&](bool capture,bool replay,bool leaf,std::wstring_view name){
        bool rejected{};try{(void)M::Resolve(capture,replay,leaf,name);}catch(const std::invalid_argument&){rejected=true;}
        need(rejected);++negatives;
    };
    for(bool capture:{false,true})for(bool replay:{false,true})for(bool leaf:{false,true}){
        need(!M::Resolve(capture,replay,leaf,L""));need(!M::Resolve(capture,replay,leaf,L"original"));
        if(capture!=replay&&leaf)need(M::Resolve(capture,replay,leaf,L"leaf-normal-detail-090-v1"));
        else reject(capture,replay,leaf,L"leaf-normal-detail-090-v1");
        for(const auto name:{L"0.9",L".90",L"LEAF-NORMAL-DETAIL-090-V1",L"leaf-normal-detail-090-v1 ",L"leaf-normal-detail-090-v1\n",L"original ",L"none"})reject(capture,replay,leaf,name);
    }
    std::wstring embedded=L"leaf-normal-detail-090-v1";embedded.push_back(L'\0');embedded+=L"other";reject(true,false,true,embedded);
    reject(true,false,true,std::wstring(10000,L'x'));
    need(std::bit_cast<std::uint32_t>(M::Strength(false))==std::bit_cast<std::uint32_t>(.65f));
    need(std::bit_cast<std::uint32_t>(M::Strength(true))==std::bit_cast<std::uint32_t>(.90f));
    need(M::Strength(true)>M::Strength(false)&&M::Strength(true)<=1.f);
    std::cout<<"GROUND_MICRORELIEF_POLICY_PASS checks="<<checks<<" negatives="<<negatives<<" default=.65 candidate=.90 GPU_verified=0 art_accepted=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
