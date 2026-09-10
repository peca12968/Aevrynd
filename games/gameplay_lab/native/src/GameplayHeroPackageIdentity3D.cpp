#include "GameplayHeroPackageIdentity3D.h"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <sstream>

namespace GameplayLab {namespace {
std::string Sha256(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Hero package missing/unreadable file");
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    struct Cleanup {BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("Hero SHA256 initialization failed");
    std::array<unsigned char,65536> buffer{};std::uint64_t bytes=0;
    while(input){input.read(reinterpret_cast<char*>(buffer.data()),buffer.size());const auto count=input.gcount();bytes+=std::uint64_t(count);if(bytes>64u*1024u*1024u)throw std::runtime_error("Hero identity input exceeds limit");if(count&&BCryptHashData(hash,buffer.data(),ULONG(count),0)<0)throw std::runtime_error("Hero SHA256 update failed");}
    if(!input.eof())throw std::runtime_error("Hero identity input read failed");
    std::array<unsigned char,32> digest{};if(BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)<0)throw std::runtime_error("Hero SHA256 finish failed");
    std::string result;for(auto value:digest){result.push_back("0123456789abcdef"[value>>4]);result.push_back("0123456789abcdef"[value&15]);}return result;
}
std::string Quote(std::string_view value){std::string result="\"";for(unsigned char c:value){if(c=='"'||c=='\\'){result+='\\';result+=char(c);}else if(c<32){const char* hex="0123456789abcdef";result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}else result+=char(c);}return result+'"';}
}
void ValidateCandidate7ComponentIdentity3D(const Candidate7Package3D& package){
    using namespace Aevrynd;const auto& base=package.components[0].asset;
    constexpr std::array<std::size_t,4> slots{6,6,2,4};
    for(std::size_t index=0;index<4;++index){const auto& part=package.components[index];const auto& asset=part.asset;
        HumanoidSkeletonMap3D map;
        if(asset.jointNames.size()!=53||!asset.model.Valid()||!ResolveCharacterRig3D(asset,map)||asset.jointNames!=base.jointNames||asset.model.skeletonCompatibilityId!=base.model.skeletonCompatibilityId||asset.materialSlotNames.size()!=slots[index]||!part.sidecar||part.sidecar.materials.size()!=slots[index])throw std::runtime_error("Hero component identity/material contract failed");
        // 不只相信compatibility欄位；每個局部bind與inverse bind亦必須逐值相同。
        for(std::size_t j=0;j<53;++j){const auto& a=asset.model.skeleton.joints[j];const auto& b=base.model.skeleton.joints[j];if(a.parent!=b.parent)throw std::runtime_error("Hero component parent mismatch");const auto ma=a.bindTransform.Matrix(),mb=b.bindTransform.Matrix();for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)if(ma[r][c]!=mb[r][c]||asset.model.inverseBindMatrices[j][r][c]!=base.model.inverseBindMatrices[j][r][c])throw std::runtime_error("Hero component bind mismatch");}
    }
}
SelectedHeroPackage3D SelectHeroPackage3D(const HeroPackageSelection3D& selection){
    if(selection.schemaVersion!=1||selection.heroId!="candidate7-mpfb-v1"||selection.root.empty())throw std::runtime_error("Unsupported explicit hero selection");
    struct File {std::filesystem::path path;std::string sha;};std::vector<File> verified;
    auto package=LoadCandidate7Package3D(selection.root,[&](const auto& path,std::string_view expected){const auto actual=Sha256(path);if(actual!=expected)throw std::runtime_error("Hero pinned SHA256 mismatch");verified.push_back({path,actual});return true;});
    if(verified.size()!=8)throw std::runtime_error("Hero package identity file count mismatch");
    ValidateCandidate7ComponentIdentity3D(package);
    // 再讀一次抓取載入期間普通檔案變更；不是對惡意並行寫入者的檔案快照隔離承諾。
    for(const auto& file:verified)if(Sha256(file.path)!=file.sha)throw std::runtime_error("Hero file changed while loading");
    std::ostringstream receipt;receipt<<"{\"schemaVersion\":1,\"actualHeroId\":\"candidate7-mpfb-v1\",\"validationStage\":\"CPU_LOADED_NOT_RENDERED\",\"jointCount\":53,\"skeletonCompatibilityId\":\""<<package.components[0].asset.model.skeletonCompatibilityId.value<<"\",\"componentCount\":4,\"files\":[";
    for(std::size_t i=0;i<verified.size();++i){if(i)receipt<<',';receipt<<"{\"component\":"<<Quote(std::array<const char*,4>{"body","upper","lower","hair"}[i/2])<<",\"kind\":"<<Quote(i%2?"material-sidecar":"aeskin")<<",\"path\":"<<Quote(verified[i].path.generic_string())<<",\"sha256\":"<<Quote(verified[i].sha)<<'}';}
    receipt<<"],\"materialCounts\":[6,6,2,4],\"legacyNineMaterialContractModified\":false,\"rendered\":false,\"visualApproved\":false}";
    return {std::move(package),"candidate7-mpfb-v1",receipt.str()};
}
}
