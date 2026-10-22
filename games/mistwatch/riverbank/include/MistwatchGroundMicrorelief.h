#pragma once
#include <stdexcept>
#include <string_view>

namespace Mistwatch::GroundMicrorelief {
inline constexpr std::string_view Candidate="leaf-normal-detail-090-v1";
inline bool Resolve(bool capture,bool replay,bool leafGround,std::wstring_view name){
    if(name.empty()||name==L"original")return false;
    if(name!=L"leaf-normal-detail-090-v1")throw std::invalid_argument("GROUND_MICRORELIEF_PROFILE");
    if(capture==replay||!leafGround)throw std::invalid_argument("GROUND_MICRORELIEF_CONTEXT");
    return true;
}
// 只提高既有真地表貼圖的法線细節；不改normalScale覆蓋標記、幾何、UV、光源或接地。
// 此值不是新的預設或美術通過，必須在同鏡位實機驗收。
inline constexpr float Strength(bool candidate) noexcept{return candidate?.90f:.65f;}
}
