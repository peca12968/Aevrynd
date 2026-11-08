#pragma once
#include <stdexcept>
#include <string_view>

namespace Mistwatch::ManualEntryRoute {
enum class Route { Shelter,Completion,DeathRetry };
inline Route Resolve(bool smoke,bool indexedProof,std::wstring_view requested) {
    if(requested.empty())return Route::Shelter;
    if(!smoke||indexedProof)throw std::invalid_argument("MANUAL_ROUTE_REQUIRES_ISOLATED_ENGINE_INPUT_SMOKE");
    if(requested==L"completion")return Route::Completion;
    if(requested==L"death-retry")return Route::DeathRetry;
    throw std::invalid_argument("MANUAL_ENTRY_UNKNOWN_ROUTE");
}
inline unsigned CaptureCount(Route route){
    switch(route){
    case Route::Shelter:return 2;
    case Route::Completion:return 3;
    case Route::DeathRetry:return 4;
    }
    throw std::invalid_argument("MANUAL_ENTRY_UNKNOWN_ROUTE");
}
inline const char* CaptureName(Route route,unsigned index) {
    if(index>=CaptureCount(route))throw std::invalid_argument("MANUAL_ENTRY_CAPTURE_INDEX");
    if(index==0)return "0-manual-menu.png";
    if(route==Route::Shelter)return "1-manual-F9.png";
    if(route==Route::DeathRetry){
        constexpr const char* phases[]{"1-manual-defeated.png","2-manual-retry.png","3-manual-recrossed.png"};
        return phases[index-1];
    }
    return index==1?"1-manual-bridge.png":"2-manual-complete-F9.png";
}
}
