#pragma once
#include <stdexcept>
#include <string_view>

namespace Mistwatch::ManualEntryRoute {
enum class Route { Shelter,Completion };
inline Route Resolve(bool smoke,bool indexedProof,std::wstring_view requested) {
    if(requested.empty())return Route::Shelter;
    if(!smoke||indexedProof)throw std::invalid_argument("MANUAL_ROUTE_REQUIRES_ISOLATED_ENGINE_INPUT_SMOKE");
    if(requested==L"completion")return Route::Completion;
    throw std::invalid_argument("MANUAL_ENTRY_UNKNOWN_ROUTE");
}
inline unsigned CaptureCount(Route route){return route==Route::Completion?3u:2u;}
inline const char* CaptureName(Route route,unsigned index) {
    if(index>=CaptureCount(route))throw std::invalid_argument("MANUAL_ENTRY_CAPTURE_INDEX");
    if(index==0)return "0-manual-menu.png";
    if(route==Route::Shelter)return "1-manual-F9.png";
    return index==1?"1-manual-bridge.png":"2-manual-complete-F9.png";
}
}
