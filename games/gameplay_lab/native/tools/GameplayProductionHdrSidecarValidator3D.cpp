#include "GameplayProductionHdrSidecar3D.h"
#include <iostream>
#include <string>

namespace {std::string Hex(const Aevrynd::GameplayLab::GameplayProductionHash2563D& value){constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(64);for(const auto byte:value){result.push_back(digits[byte>>4]);result.push_back(digits[byte&15]);}return result;}}

int wmain(int argc,wchar_t** argv){
    if(argc!=2){std::cerr<<"usage: Aevrynd-Gameplay-Hdr-Validator.exe <file>\n";return 2;}
    const auto receipt=Aevrynd::GameplayLab::InspectGameplayProductionHdrSidecarFile3D(argv[1]);
    if(!receipt){std::cerr<<"invalid production HDR sidecar\n";return 3;}
    std::cout<<"{\"schemaVersion\":"<<receipt->schemaVersion<<",\"byteCount\":"<<receipt->byteCount<<",\"fileHash\":\""<<Hex(receipt->fileHash)<<"\",\"stableContentHash\":\""<<Hex(receipt->stableContentHash)<<"\",\"payloadHash\":\""<<Hex(receipt->payloadHash)<<"\",\"rootHash\":\""<<Hex(receipt->rootHash)<<"\"}\n";
    return 0;
}
