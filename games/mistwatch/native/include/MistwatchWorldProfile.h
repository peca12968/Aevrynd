#pragma once
#include <stdexcept>
#include <string_view>

namespace Mistwatch {
enum class WorldProfile { LegacyCore, LegacyMiddle, ForestCanopy };

inline bool WorldProfileHasMiddle(WorldProfile profile) {
    switch (profile) {
    case WorldProfile::LegacyCore: return false;
    case WorldProfile::LegacyMiddle:
    case WorldProfile::ForestCanopy: return true;
    }
    throw std::invalid_argument("Invalid world profile value");
}
inline bool WorldProfileHasCanopy(WorldProfile profile) {
    switch (profile) {
    case WorldProfile::LegacyCore:
    case WorldProfile::LegacyMiddle: return false;
    case WorldProfile::ForestCanopy: return true;
    }
    throw std::invalid_argument("Invalid world profile value");
}
inline std::string_view WorldProfileName(WorldProfile profile) {
    switch (profile) {
    case WorldProfile::LegacyCore: return "legacy-core";
    case WorldProfile::LegacyMiddle: return "legacy-middle";
    case WorldProfile::ForestCanopy: return "forest-canopy";
    }
    throw std::invalid_argument("Invalid world profile value");
}
inline WorldProfile ResolveWorldProfile(bool validCapture, std::wstring_view worldFlag,
                                       std::wstring_view middleFlag) {
    // 一般遊玩固定使用新森林，完全忽略殘留診斷環境值。
    if (!validCapture) return WorldProfile::ForestCanopy;
    WorldProfile profile = WorldProfile::ForestCanopy;
    // 舊世界必須明確選取；只關閉繪圖不能移除碰撞或重排植被。
    if (worldFlag == L"legacy-core") profile = WorldProfile::LegacyCore;
    else if (worldFlag == L"legacy-middle") profile = WorldProfile::LegacyMiddle;
    else if (!worldFlag.empty() && worldFlag != L"forest-canopy")
        throw std::invalid_argument("Invalid diagnostic world profile");
    if (!middleFlag.empty()) {
        if (middleFlag != L"0" && middleFlag != L"1")
            throw std::invalid_argument("Invalid diagnostic middle flag");
        if ((middleFlag == L"1") != WorldProfileHasMiddle(profile))
            throw std::invalid_argument("Diagnostic world and middle flags disagree");
    }
    return profile;
}
} // Mistwatch 命名空間
