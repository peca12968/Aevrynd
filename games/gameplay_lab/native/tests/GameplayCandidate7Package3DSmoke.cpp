#include "GameplayCandidate7Package3D.h"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <iostream>
bool Verify(const std::filesystem::path& path,std::string_view expected){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    const auto close=[&]{if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);};
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0){close();return false;}
    std::ifstream input(path,std::ios::binary);std::array<unsigned char,65536> buffer{};
    while(input){input.read(reinterpret_cast<char*>(buffer.data()),buffer.size());if(input.gcount()>0&&BCryptHashData(hash,buffer.data(),ULONG(input.gcount()),0)<0){close();return false;}}
    std::array<unsigned char,32> result{};if(!input.eof()||BCryptFinishHash(hash,result.data(),ULONG(result.size()),0)<0){close();return false;}close();
    std::string hex;for(auto b:result){hex.push_back("0123456789abcdef"[b>>4]);hex.push_back("0123456789abcdef"[b&15]);}return hex==expected;
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    try{
        auto package=GameplayLab::LoadCandidate7Package3D(argv[1],Verify);
        auto arms=GameplayLab::BuildCandidate7Arms3D(package.components[0].asset);
        if(!arms||arms.model.skeletonCompatibilityId!=package.components[0].asset.model.skeletonCompatibilityId){std::cerr<<"arms status="<<int(arms.status)<<'\n';return 3;}
        bool rejected=false;try{(void)GameplayLab::LoadCandidate7Package3D(argv[1],[](const auto&,auto){return false;});}catch(const std::exception&){rejected=true;}
        if(!rejected)return 4;
        std::cout<<"CANDIDATE7_PACKAGE=PASS components=4 pinnedHashes=8 arms="<<arms.stats.retainedTriangles<<" negativeIdentity=PASS scope=CPU-not-posed-visual\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 10;}
}
