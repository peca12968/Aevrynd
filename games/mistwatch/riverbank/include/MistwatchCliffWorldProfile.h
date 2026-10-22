#pragma once
#include <array>
#include <stdexcept>
#include <string_view>

namespace Mistwatch::CliffWorld {
inline constexpr std::wstring_view Profile=L"rock-face-03-world-v1";
// 固定組合入口不接受其他實驗開關拼接，避免手動遊玩與回放測到不同世界。
inline bool Resolve(bool capture,bool replay,bool manual,std::wstring_view requested,
                    const std::array<std::wstring_view,11>& componentOverrides) {
    if(!requested.empty()&&requested!=Profile)throw std::invalid_argument("CLIFF_WORLD_PROFILE_INVALID");
    const bool enabled=manual||!requested.empty();
    if(!enabled)return false;
    if(capture||(manual&&replay)||(manual&&!requested.empty())||(!manual&&!replay))
        throw std::invalid_argument("CLIFF_WORLD_CONTEXT_CONFLICT");
    for(const auto value:componentOverrides)if(!value.empty())
        throw std::invalid_argument("CLIFF_WORLD_COMPONENT_OVERRIDE");
    return true;
}
}
