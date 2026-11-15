#pragma once

#include "RiverbankBattleWire.h"
#include "RiverbankTerritory.h"
#include <limits>

namespace Mistwatch::RiverbankTerritoryWire {
using RiverbankBattleWire::detail::Put;
using RiverbankBattleWire::detail::Read;
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'T'}, std::byte{'R'}, std::byte{1}};
constexpr std::size_t HeaderBytes = Magic.size() + 1;
constexpr std::size_t QueryBytes = HeaderBytes + sizeof(std::uint64_t);
constexpr std::size_t StateBytes = HeaderBytes + 4 * sizeof(std::uint64_t) + 2 * sizeof(std::uint32_t) + 3;

inline bool HasMagic(std::span<const std::byte> bytes) {
    return bytes.size() >= Magic.size() && std::ranges::equal(bytes.first(Magic.size()), Magic);
}

inline auto EncodeQuery(std::uint64_t sequence) {
    std::array<std::byte, QueryBytes> frame{};
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{1};
    Put(out, sequence);
    return frame;
}

inline std::uint64_t DecodeQuery(std::span<const std::byte> frame) {
    if (frame.size() != QueryBytes || !HasMagic(frame) || frame[Magic.size()] != std::byte{1}) return 0;
    auto* in = frame.data() + HeaderBytes;
    return Read<std::uint64_t>(in);
}

struct Reply {
    std::uint64_t sequence{};
    TerritoryState state;
};

inline std::vector<std::byte> EncodeState(std::uint64_t sequence, const TerritoryState& state) {
    std::vector<std::byte> frame(StateBytes + state.ownerName.size() + state.challengerName.size());
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{2};
    Put(out, sequence);
    for (const auto id : {state.revision, state.owner, state.challenger}) Put(out, std::uint64_t(id));
    Put(out, std::uint32_t(state.progress));
    Put(out, std::uint32_t(state.rewards));
    *out++ = std::byte(state.contested);
    for (const auto name : {std::string_view(state.ownerName), std::string_view(state.challengerName)}) {
        *out++ = std::byte(name.size());
        for (const auto character : name) *out++ = std::byte(character);
    }
    return frame;
}

inline std::optional<Reply> DecodeState(std::span<const std::byte> frame) {
    if (frame.size() < StateBytes || frame.size() > StateBytes + 2 * GuildNameBytes || !HasMagic(frame) ||
        frame[Magic.size()] != std::byte{2}) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const auto sequence = Read<std::uint64_t>(in), revision = Read<std::uint64_t>(in),
        owner = Read<std::uint64_t>(in), challenger = Read<std::uint64_t>(in);
    const auto progress = Read<std::uint32_t>(in), rewards = Read<std::uint32_t>(in);
    const auto contested = std::to_integer<unsigned>(*in++);
    constexpr auto limit = std::uint64_t((std::numeric_limits<std::int64_t>::max)());
    if (!sequence || revision > limit || owner > limit || challenger > limit || bool(owner) != bool(revision) ||
        (owner && owner == challenger) || contested > 1 || progress >= RiverbankTerritory::HoldTicks ||
        bool(progress) != bool(challenger)) return std::nullopt;
    const auto ownerBytes = std::to_integer<std::size_t>(*in++);
    if (ownerBytes > GuildNameBytes || frame.size() < StateBytes + ownerBytes || bool(ownerBytes) != bool(owner)) return std::nullopt;
    const auto* ownerName = reinterpret_cast<const char*>(in);
    in += ownerBytes;
    const auto challengerBytes = std::to_integer<std::size_t>(*in++);
    if (challengerBytes > GuildNameBytes || frame.size() != StateBytes + ownerBytes + challengerBytes ||
        bool(challengerBytes) != bool(challenger)) return std::nullopt;
    return Reply{sequence, {static_cast<std::int64_t>(revision), static_cast<std::int64_t>(owner),
        static_cast<std::int64_t>(challenger), progress, rewards, contested != 0,
        std::string(ownerName, ownerBytes), std::string(reinterpret_cast<const char*>(in), challengerBytes)}};
}
}
