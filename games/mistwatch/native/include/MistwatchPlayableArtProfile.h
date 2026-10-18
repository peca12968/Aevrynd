#pragma once
#include <stdexcept>
#include <string_view>

namespace Mistwatch::PlayableArt {
enum class Feature {Shelter,Workshop,Road,Roadside,Understory,BoulderColor,BoulderGeometry,Shrubs,LandscapeTwig,LandscapeStand};

inline std::wstring_view Resolve(bool capture,Feature feature,std::wstring_view requested) {
    std::wstring_view accepted;
    // 先驗證功能身分；正常遊玩也不能把未知列舉悄悄當成某項預設。
    switch(feature) {
    case Feature::Shelter:accepted=L"framed-corrugated-v1";break;
    case Feature::Workshop:accepted=L"framed-workshop-v1";break;
    case Feature::Road:accepted=L"entrance-road-material-domain-v1";break;
    case Feature::Roadside:accepted=L"roadside-broadleaf-v1";break;
    case Feature::Understory:accepted=L"entrance-understory-v2";break;
    case Feature::BoulderColor:accepted=L"source-srgb-v1";break;
    case Feature::BoulderGeometry:accepted=L"lod0-source-v1";break;
    case Feature::Shrubs:accepted=L"woodland-feasible-arc-v3";break;
    case Feature::LandscapeTwig:accepted=L"crown-distance-lod-v1";break;
    case Feature::LandscapeStand:accepted=L"landscape-stand-fill-v2";break;
    default:throw std::invalid_argument("Unknown playable art feature");
    }
    // 一般玩家永遠採用已接受版本，不繼承舊診斷環境變數或錯誤字串。
    if(!capture)return accepted;
    if(requested.empty()||requested==L"original")return L"original";
    if(requested==accepted)return accepted;
    if(feature==Feature::Roadside&&requested==L"roadside-ground-cover-v1")return L"roadside-ground-cover-v1";
    if(feature==Feature::Shrubs&&requested==L"woodland-density-v1")return L"woodland-density-v1";
    if(feature==Feature::Shrubs&&requested==L"woodland-gap-density-v2")return L"woodland-gap-density-v2";
    if(feature==Feature::LandscapeTwig&&requested==L"crown-lod1-source-v1")return L"crown-lod1-source-v1";
    throw std::invalid_argument("Unsupported capture art profile for feature");
}
}
