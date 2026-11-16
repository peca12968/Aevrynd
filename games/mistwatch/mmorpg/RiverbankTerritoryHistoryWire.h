#pragma once

#include "RiverbankTerritoryWire.h"

namespace Mistwatch::RiverbankTerritoryHistoryWire {
using RiverbankTerritoryWire::Put;
using RiverbankTerritoryWire::Read;
inline constexpr std::uint32_t Capability = 1u << 21;
inline constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'T'}, std::byte{'H'}, std::byte{1}};
inline constexpr std::size_t HeaderBytes = Magic.size() + 1;
inline constexpr std::size_t QueryBytes = HeaderBytes + 8;
inline constexpr std::size_t ReplyBytes = QueryBytes + 1;
inline constexpr std::size_t ClockBytes = 20;
inline constexpr std::size_t RowBytes = 8 + ClockBytes + 2;

inline bool HasMagic(std::span<const std::byte> bytes) {
    return bytes.size() >= Magic.size() && std::ranges::equal(bytes.first(Magic.size()), Magic);
}

inline auto Query(std::uint64_t sequence) {
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

inline bool ValidClock(std::string_view clock) {
    constexpr std::string_view pattern = "0000-00-00T00:00:00Z";
    return clock.size() == pattern.size() && std::ranges::equal(clock, pattern, [](char value, char mask) {
        return mask == '0' ? value >= '0' && value <= '9' : value == mask;
    });
}

struct Reply {
    std::uint64_t sequence{};
    std::vector<TerritoryClaim> claims;
};

inline std::vector<std::byte> EncodeReply(std::uint64_t sequence, std::span<const TerritoryClaim> claims) {
    if (!sequence || claims.size() > RiverbankTerritory::HistoryLimit) return {};
    std::size_t bytes = ReplyBytes;
    std::uint64_t previous = (std::numeric_limits<std::uint64_t>::max)();
    for (const auto& row : claims) {
        if (!row.revision || row.revision >= previous || !ValidClock(row.claimedUtc) || row.winner.empty() ||
            row.previousOwner.size() > GuildNameBytes || row.winner.size() > GuildNameBytes) return {};
        previous = row.revision;
        bytes += RowBytes + row.previousOwner.size() + row.winner.size();
    }
    std::vector<std::byte> frame(bytes);
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{2};
    Put(out, sequence);
    *out++ = std::byte(claims.size());
    for (const auto& row : claims) {
        Put(out, row.revision);
        for (char c : row.claimedUtc) *out++ = std::byte(c);
        for (const auto& guild : {std::string_view(row.previousOwner), std::string_view(row.winner)}) {
            *out++ = std::byte(guild.size());
            for (char c : guild) *out++ = std::byte(c);
        }
    }
    return frame;
}

inline std::optional<Reply> DecodeReply(std::span<const std::byte> frame) {
    if (frame.size() < ReplyBytes || !HasMagic(frame) || frame[Magic.size()] != std::byte{2}) return {};
    auto* in = frame.data() + HeaderBytes;
    const auto* const end = frame.data() + frame.size();
    Reply result{Read<std::uint64_t>(in), {}};
    const auto count = std::to_integer<std::size_t>(*in++);
    if (!result.sequence || count > RiverbankTerritory::HistoryLimit) return {};
    result.claims.reserve(count);
    std::uint64_t previous = (std::numeric_limits<std::uint64_t>::max)();
    for (std::size_t i = 0; i < count; ++i) {
        if (std::size_t(end - in) < RowBytes) return {};
        TerritoryClaim row{};
        row.revision = Read<std::uint64_t>(in);
        row.claimedUtc.assign(reinterpret_cast<const char*>(in), ClockBytes);
        in += ClockBytes;
        if (!row.revision || row.revision >= previous || !ValidClock(row.claimedUtc)) return {};
        for (auto* name : {&row.previousOwner, &row.winner}) {
            if (in == end) return {};
            const auto length = std::to_integer<std::size_t>(*in++);
            if (length > GuildNameBytes || std::size_t(end - in) < length) return {};
            name->assign(reinterpret_cast<const char*>(in), length);
            in += length;
        }
        if (row.winner.empty()) return {};
        previous = row.revision;
        result.claims.push_back(std::move(row));
    }
    return in == end ? std::optional<Reply>(std::move(result)) : std::nullopt;
}
}
