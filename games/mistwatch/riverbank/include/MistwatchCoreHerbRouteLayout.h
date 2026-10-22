#pragma once
#include "MistwatchCoreHerbClusters.h"

namespace Mistwatch::CoreHerbRouteLayout {
enum class Profile : std::uint8_t { Original,RouteEdgeV1 };
// 同一足額作者群落；只把東側第八群落拉近南段行走區，原模型、風、間距與提案上限不變。
inline constexpr CoreHerbClusters::AuthorLayout RouteEdgeLayout=[] {
    auto result=CoreHerbClusters::DefaultLayout;
    result.centres[7]={10.f,0,10.f};
    return result;
}();
inline Profile Resolve(bool capture,bool replay,bool coreHerbsEnabled,std::wstring_view name) {
    if(name.empty()||name==L"original")return Profile::Original;
    if(name!=L"core-herb-route-edge-v1")throw std::invalid_argument("CORE_HERB_LAYOUT_PROFILE");
    if(!coreHerbsEnabled||capture==replay)throw std::invalid_argument("CORE_HERB_LAYOUT_CONTEXT");
    return Profile::RouteEdgeV1;
}
inline const CoreHerbClusters::AuthorLayout& Layout(Profile profile) {
    switch(profile){
    case Profile::Original:return CoreHerbClusters::DefaultLayout;
    case Profile::RouteEdgeV1:return RouteEdgeLayout;
    }
    throw std::invalid_argument("CORE_HERB_LAYOUT_ENUM");
}
inline std::string_view Name(Profile profile) {
    switch(profile){
    case Profile::Original:return "original";
    case Profile::RouteEdgeV1:return "core-herb-route-edge-v1";
    }
    throw std::invalid_argument("CORE_HERB_LAYOUT_ENUM");
}
}
