#pragma once
#include <Aevrynd/EnvironmentIbl3D.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <array>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Mistwatch::ForestIbl {
// 現有 xanderklinge 合法來源的內容資格；不是森林美術或 GPU 驗收通過聲明。
inline constexpr std::wstring_view Candidate=L"xanderklinge-forest-ibl-v1";
struct Spec {
    std::string_view relativePath;
    std::size_t bytes;
    std::string_view sha256;
    Aevrynd::EnvironmentIblTextureKind3D kind;
    unsigned width,height,mips,faces;
};
inline constexpr std::array<Spec,3> Specs{{
    {"photoreal-cooked/ibl/environment-specular.dds",131380,"6FBA886F002D6FCD9FDDA655C0C604611BCB6C1F9C6B5E0D67AB255282A6FAD1",
        Aevrynd::EnvironmentIblTextureKind3D::SpecularCube,128,128,8,6},
    {"photoreal-cooked/ibl/environment-irradiance.dds",8500,"B058AA7278FC3127ACE7685E9710A613E904C18337F3EE676D5D1647FAA9DE5E",
        Aevrynd::EnvironmentIblTextureKind3D::IrradianceCube,32,32,6,6},
    {"photoreal-cooked/ibl/environment-brdf.dds",65684,"B36B75CC849301004B795F89A1DAED5F5F78B8686B93679EF15B763006F2B2F6",
        Aevrynd::EnvironmentIblTextureKind3D::BrdfLut,128,128,1,1}
}};
inline bool Resolve(bool capture,bool replay,std::wstring_view value){
    if(value.size()>96)throw std::invalid_argument("FOREST_IBL_PROFILE");
    if(value.empty()||value==L"original")return false;
    if(value!=Candidate)throw std::invalid_argument("FOREST_IBL_PROFILE");
    if(capture==replay)throw std::invalid_argument("FOREST_IBL_ISOLATED_CONTEXT");
    return true;
}
inline Aevrynd::EnvironmentIblDefinition3D Definition(const std::filesystem::path& root){
    if(!root.is_absolute()||root.native().find(L'\0')!=std::wstring::npos)throw std::invalid_argument("FOREST_IBL_ROOT");
    for(const auto& part:root)if(part==L"."||part==L"..")throw std::invalid_argument("FOREST_IBL_ROOT");
    return {root/std::filesystem::path(Specs[0].relativePath),root/std::filesystem::path(Specs[1].relativePath),root/std::filesystem::path(Specs[2].relativePath),0.f};
}
inline bool ExactHash(std::span<const std::byte> bytes,std::string_view expected){
    std::array<std::byte,32> digest{};
    if(expected.size()!=64||!Aevrynd::WorldDeltaSha2563D(bytes,digest))return false;
    constexpr char digits[]="0123456789ABCDEF";
    for(unsigned i=0;i<32;++i){const unsigned n=std::to_integer<unsigned>(digest[i]);
        if(expected[i*2]!=digits[n>>4]||expected[i*2+1]!=digits[n&15])return false;}
    return true;
}
// 雜湊的是完整 DDS 位元組，不接受可交換來源、局部 payload hash 或套件自行宣告的 expected hash。
// 呼叫者須保持同一文件的只讀鎖，並交由原生 EnvironmentIbl parser 與 store 驗證其結構。
inline bool ValidatePayloads(const std::array<std::span<const std::byte>,3>& payloads){
    for(unsigned i=0;i<3;++i)if(payloads[i].size()!=Specs[i].bytes||!ExactHash(payloads[i],Specs[i].sha256))return false;
    return true;
}
}
