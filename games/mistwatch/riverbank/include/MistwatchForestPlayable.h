#pragma once
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Mistwatch::ForestPlayable {
inline constexpr std::wstring_view Profile=L"upstream-grove-wip-v1";
inline constexpr std::wstring_view Argument=L"--forest-grove-candidate";
inline constexpr std::wstring_view PrototypeArgument=L"--prototype";

constexpr std::wstring_view SelectRiverbankArgument(std::wstring_view arguments,bool capture,bool replay) noexcept {
    const auto first=arguments.find_first_not_of(L" \t");
    if(first==std::wstring_view::npos)return capture||replay?std::wstring_view{}:Argument;
    const auto argument=arguments.substr(first,arguments.find_last_not_of(L" \t")-first+1);
    return argument==PrototypeArgument?std::wstring_view{}:argument;
}

// 只有明示的新完整世界入口可操作／存檔；舊低階原型的限制仍由原路徑檢查。
inline bool Resolve(bool manual,bool capture,bool replay,std::wstring_view requested,
                    std::wstring_view route,std::span<const std::wstring> dependencies){
    if(!manual&&requested.empty())return false;
    if(!requested.empty()&&requested!=Profile)throw std::invalid_argument("FOREST_PLAYABLE_UNKNOWN_PROFILE");
    if(capture||manual==replay||(manual&&(!requested.empty()||!route.empty()))||(!manual&&requested.empty()))
        throw std::invalid_argument("FOREST_PLAYABLE_CONTEXT");
    if(dependencies.size()!=6)throw std::invalid_argument("FOREST_PLAYABLE_DEPENDENCY_SCOPE");
    for(const auto& value:dependencies)if(!value.empty())throw std::invalid_argument("FOREST_PLAYABLE_LOW_LEVEL_OVERRIDE");
    if(replay&&route!=L"shelter"&&route!=L"completion"&&route!=L"checkpoint"&&route!=L"resume-save"&&route!=L"reject-save"&&route!=L"grove-walk")
        throw std::invalid_argument("FOREST_PLAYABLE_ROUTE");
    return true;
}
}
