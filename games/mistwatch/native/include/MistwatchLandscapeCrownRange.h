#pragma once
#include <stdexcept>
#include <string_view>

namespace Mistwatch::LandscapeCrownRange {
struct Policy {
    std::string_view label;
    float switchDistance;
    float transitionWidth;
};
inline constexpr Policy Original{"original",60.f,16.f};
inline constexpr Policy Extended{"extended-100m-v1",100.f,24.f};

// 僅改變既有雙層樹冠的距離政策，不改資源、保守聯集邊界或距離量測方式。
// 一般玩家固定原政策；呼叫端必須先確認擷取模式有效，不能只看環境旗標。
[[nodiscard]] inline Policy Resolve(bool validCapture,std::wstring_view requested) {
    if(!validCapture)return Original;
    if(requested.empty()||requested==L"original")return Original;
    if(requested==L"extended-100m-v1")return Extended;
    throw std::invalid_argument("LANDSCAPE_CROWN_RANGE_PROFILE");
}
}
