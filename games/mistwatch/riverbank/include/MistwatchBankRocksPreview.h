#pragma once
#include <stdexcept>
#include <string_view>
namespace Mistwatch::BankRocksPreview {
inline constexpr std::wstring_view Argument=L"--forest-bank-rocks-preview";
// Manual CLI or validated forest replay opt-in, never an inherited global
// environment shortcut for ordinary gameplay or another world/capture profile.
inline bool Resolve(bool manualRequest,bool replay,bool forestWorld,std::wstring_view request){
    if(!manualRequest&&request.empty())return false;
    if(!forestWorld||(manualRequest&&replay))throw std::invalid_argument("BANK_ROCK_PREVIEW_CONTEXT");
    if(manualRequest){if(!request.empty())throw std::invalid_argument("BANK_ROCK_PREVIEW_DUPLICATE_REQUEST");return true;}
    if(!replay||request!=L"1")throw std::invalid_argument("BANK_ROCK_PREVIEW_EXPLICIT_REPLAY_REQUEST");
    return true;
}
}
