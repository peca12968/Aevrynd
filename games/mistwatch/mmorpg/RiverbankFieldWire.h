#pragma once

#include "RiverbankBattleWire.h"

namespace Mistwatch::RiverbankFieldWire {
inline constexpr std::uint32_t Capability = 1u << 16;
inline constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'F'}, std::byte{'H'}};
inline constexpr std::size_t HeaderBytes = Magic.size() + 2;
inline constexpr std::size_t QueryBytes = HeaderBytes + 8;
inline constexpr std::size_t StateBytes = HeaderBytes + 3 * 8 + 7 * 4 + 1;
struct Reply {
    std::uint64_t sequence{};
    RiverbankBossStatus field;
    unsigned hides{};
};

inline bool HasMagic(std::span<const std::byte> frame) {
    return frame.size() >= Magic.size() && std::ranges::equal(frame.first(Magic.size()), Magic);
}

inline auto EncodeQuery(std::uint64_t sequence) {
    std::array<std::byte, QueryBytes> encoded{};
    auto* out = std::ranges::copy(Magic, encoded.data()).out;
    *out++ = std::byte{1};
    *out++ = std::byte{1};
    RiverbankBattleWire::detail::Put(out, sequence);
    return encoded;
}

inline std::uint64_t DecodeQuery(std::span<const std::byte> frame) {
    if (frame.size() != QueryBytes || !HasMagic(frame) || frame[4] != std::byte{1} || frame[5] != std::byte{1})
        return 0;
    auto* in = frame.data() + HeaderBytes;
    return RiverbankBattleWire::detail::Read<std::uint64_t>(in);
}

inline auto EncodeReply(const Reply& reply) {
    using namespace RiverbankBattleWire::detail;
    std::array<std::byte, StateBytes> encoded{};
    auto* out = std::ranges::copy(Magic, encoded.data()).out;
    *out++ = std::byte{1};
    *out++ = std::byte{2};
    Put(out, reply.sequence);
    Put(out, reply.field.run);
    Put(out, reply.field.target);
    PutPoint(out, reply.field.position);
    PutPoint(out, reply.field.strikeCenter);
    Put(out, std::uint32_t(reply.field.health));
    Put(out, std::uint32_t(reply.field.phaseTicks));
    Put(out, std::uint32_t(reply.hides));
    *out = std::byte(reply.field.phase);
    return encoded;
}

inline std::optional<Reply> DecodeReply(std::span<const std::byte> frame) {
    using namespace RiverbankBattleWire::detail;
    if (frame.size() != StateBytes || !HasMagic(frame) || frame[4] != std::byte{1} || frame[5] != std::byte{2})
        return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    Reply reply;
    reply.sequence = Read<std::uint64_t>(in);
    auto& field = reply.field;
    field.run = Read<std::uint64_t>(in);
    field.target = Read<std::uint64_t>(in);
    field.position = ReadPoint(in);
    field.strikeCenter = ReadPoint(in);
    field.health = Read<std::uint32_t>(in);
    field.phaseTicks = Read<std::uint32_t>(in);
    reply.hides = Read<std::uint32_t>(in);
    field.phase = EncounterPhase(*in);
    using Hunt = RiverbankEncounter;
    if (!reply.sequence || !field.run || field.phase > EncounterPhase::Defeated || field.health > Hunt::FieldHealth ||
        (field.phase == EncounterPhase::Defeated) != (field.health == 0) || !Hunt::InPasture(field.position) ||
        !std::isfinite(field.strikeCenter.x) || !std::isfinite(field.strikeCenter.z) ||
        field.phaseTicks > Hunt::FieldRespawnTicks ||
        (field.phase == EncounterPhase::Windup &&
            (!Hunt::InPasture(field.strikeCenter) || !field.phaseTicks || field.phaseTicks > Hunt::FieldWindupTicks)))
        return std::nullopt;
    return reply;
}
}
