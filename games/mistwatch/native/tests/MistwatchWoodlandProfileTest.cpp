#include "MistwatchWoodlandProfile.h"
#include <array>
#include <iostream>
#include <string>

namespace {
unsigned checks{},rejections{};
void Need(bool okay,const char* why) {
    ++checks;
    if(!okay)throw std::runtime_error(why);
}
struct Context {
    bool capture,replay,manual;
    const char* rejection;
    bool ordinaryResult,candidateResult;
};
// 獨立明列表格，不用重寫生產函式的分支作為預期值。
constexpr std::array<Context,8> Contexts{{
    {false,false,false,nullptr,false,false},
    {false,false,true,nullptr,true,true},
    {false,true,false,nullptr,false,true},
    {false,true,true,"WOODLAND_MANUAL_CONTEXT_CONFLICT",false,false},
    {true,false,false,nullptr,false,true},
    {true,false,true,"WOODLAND_MANUAL_CONTEXT_CONFLICT",false,false},
    {true,true,false,"WOODLAND_CONTEXT_CONFLICT",false,false},
    {true,true,true,"WOODLAND_CONTEXT_CONFLICT",false,false}
}};
void Reject(const Context& c,std::wstring_view requested,std::string_view reason) {
    bool caught{};
    try{(void)Mistwatch::ResolveWoodlandCandidate(c.capture,c.replay,c.manual,requested);}
    catch(const std::invalid_argument& e){
        caught=true;
        Need(std::string_view(e.what())==reason,"Exact rejection reason changed");
    }
    Need(caught,"Invalid profile/context was accepted");
    ++rejections;
}
void Accept(const Context& c,std::wstring_view requested,bool expected) {
    Need(Mistwatch::ResolveWoodlandCandidate(c.capture,c.replay,c.manual,requested)==expected,
        "Woodland policy result changed");
}
}
int main()try {
    constexpr std::wstring_view candidate=L"woodland-canopy-author-v2";
    for(const auto& c:Contexts){
        for(auto requested:{std::wstring_view{},std::wstring_view{L"original"},candidate}){
            if(c.rejection)Reject(c,requested,c.rejection);
            else if(!c.capture&&!c.replay&&!c.manual&&requested==candidate)
                Reject(c,requested,"WOODLAND_ISOLATION_REQUIRED");
            else Accept(c,requested,requested==candidate?c.candidateResult:c.ordinaryResult);
        }
        // 不 trim、不轉大小寫、不接受另一功能名稱，也不可忽略 NUL 後的內容。
        for(auto invalid:{L"unknown",L"ORIGINAL",L"Original",L" original",L"original ",
            L"original\n",L"\toriginal",L"WOODLAND-CANOPY-AUTHOR-V2",L"Woodland-canopy-author-v2",
            L" woodland-canopy-author-v2",L"woodland-canopy-author-v2 ",L"woodland-canopy-author-v2\n",
            L"woodland-canopy-author-v1",L"woodland-canopy-author-v3",L"woodland-feasible-arc-v3",
            L"woodland-canopy-author-v2-extra",L"true",L"1"})
            Reject(c,invalid,"WOODLAND_PROFILE_INVALID");
        // 所有非空真前綴皆不是完整名稱；空值另有明確合法語義。
        for(const auto valid:{std::wstring_view{L"original"},candidate}){
            for(std::size_t n=1;n<valid.size();++n)
                Reject(c,valid.substr(0,n),"WOODLAND_PROFILE_INVALID");
            auto embedded=std::wstring(valid);embedded.push_back(L'\0');embedded+=L"hidden";
            Reject(c,embedded,"WOODLAND_PROFILE_INVALID");
            auto terminator=std::wstring(valid);terminator.push_back(L'\0');
            Reject(c,terminator,"WOODLAND_PROFILE_INVALID");
        }
        const std::wstring overlong(4096,L'x');
        Reject(c,overlong,"WOODLAND_PROFILE_INVALID");
        const std::wstring nulOnly(1,L'\0');
        Reject(c,nulOnly,"WOODLAND_PROFILE_INVALID");
    }
    // 正常預設與明確手動選用分開；重複／交錯呼叫不能留下全域啟用狀態。
    for(unsigned repeat=0;repeat<3;++repeat){
        Need(!Mistwatch::ResolveWoodlandCandidate(false,false,false,L""),"Normal default enabled candidate");
        Need(Mistwatch::ResolveWoodlandCandidate(false,false,true,L"original"),"Manual CLI was overridden");
        Need(!Mistwatch::ResolveWoodlandCandidate(true,false,false,L"original"),"Manual state leaked to capture");
        Need(Mistwatch::ResolveWoodlandCandidate(false,true,false,candidate),"Replay opt-in unavailable");
        Need(!Mistwatch::ResolveWoodlandCandidate(false,false,false,L"original"),"Replay state leaked to normal");
    }
    std::cout<<"WOODLAND_PROFILE_CPU_PASS contexts="<<Contexts.size()<<" checks="<<checks
        <<" rejected="<<rejections<<" normal_default=0 manual_opt_in=1 exact_reasons=1 native=0 art=0\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"WOODLAND_PROFILE_FIRST_DIVERGENCE "<<e.what()<<'\n';
    return 1;
}
