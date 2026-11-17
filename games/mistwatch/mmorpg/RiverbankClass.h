#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Mistwatch {
enum class RiverbankClass : std::uint8_t { None, Guardian, Ranger, Priest };
// 舊版伺服器未公告此旗標，客戶端維持原版行走封包。
inline constexpr std::uint32_t RiverbankSprintCapability = 1u << 20;
inline constexpr std::uint32_t RiverbankJumpCapability = 1u << 28;

constexpr bool IsRiverbankClass(RiverbankClass value) {
    return value >= RiverbankClass::Guardian && value <= RiverbankClass::Priest;
}

inline constexpr std::string_view RiverbankClassName(RiverbankClass value) {
    switch (value) {
    case RiverbankClass::Guardian: return "守衛";
    case RiverbankClass::Ranger: return "遊俠";
    case RiverbankClass::Priest: return "祭司";
    default: return {};
    }
}

// Bit zero remains the engine's self-entity marker. Bits 8–15 belong to the game layer.
constexpr std::uint32_t RiverbankClassFlags(RiverbankClass value) { return std::uint32_t(value) << 8u; }
constexpr RiverbankClass ReadRiverbankClassFlags(std::uint32_t flags) { return RiverbankClass((flags >> 8u) & 255u); }

inline constexpr std::array ClassSelectionPrefix{std::byte{'A'}, std::byte{'E'}, std::byte{'C'}, std::byte{'L'}, std::byte{1}};

inline constexpr auto EncodeRiverbankClass(RiverbankClass value) {
    return std::array{ClassSelectionPrefix[0], ClassSelectionPrefix[1], ClassSelectionPrefix[2],
        ClassSelectionPrefix[3], ClassSelectionPrefix[4], std::byte(value)};
}

inline RiverbankClass DecodeRiverbankClass(std::span<const std::byte> frame) {
    if (frame.size() != ClassSelectionPrefix.size() + 1) return RiverbankClass::None;
    for (std::size_t i = 0; i < ClassSelectionPrefix.size(); ++i) {
        if (ClassSelectionPrefix[i] != frame[i]) return RiverbankClass::None;
    }
    const auto value = RiverbankClass(std::to_integer<std::uint8_t>(frame.back()));
    return IsRiverbankClass(value) ? value : RiverbankClass::None;
}
}
