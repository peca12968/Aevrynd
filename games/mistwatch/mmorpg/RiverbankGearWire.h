#pragma once

#include "RiverbankBattleWire.h"
#include "RiverbankGear.h"

namespace Mistwatch::RiverbankGearWire {
using RiverbankBattleWire::detail::Put;
using RiverbankBattleWire::detail::Read;
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'G'}, std::byte{'R'}, std::byte{1}};
constexpr std::size_t HeaderBytes = Magic.size() + 1;

struct Request { std::uint64_t sequence{}; GearAction action{}; };
struct Reply { std::uint64_t sequence{}; GearResult result{}; RiverbankGearState gear{}; };

inline bool HasMagic(std::span<const std::byte> frame) {
    return frame.size() >= Magic.size() && std::ranges::equal(frame.first(Magic.size()), Magic);
}

inline auto EncodeRequest(Request request) {
    std::array<std::byte, HeaderBytes + 9> frame{};
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{1};
    Put(out, request.sequence);
    *out = std::byte(request.action);
    return frame;
}

inline std::optional<Request> DecodeRequest(std::span<const std::byte> frame) {
    if (frame.size() != HeaderBytes + 9 || !HasMagic(frame) || frame[Magic.size()] != std::byte{1}) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const Request request{Read<std::uint64_t>(in), GearAction(*in)};
    if (!request.sequence || request.action > GearAction::Unequip) return std::nullopt;
    return request;
}

inline auto EncodeReply(Reply reply) {
    std::array<std::byte, HeaderBytes + 10> frame{};
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{2};
    Put(out, reply.sequence);
    *out++ = std::byte(reply.result);
    *out = std::byte((reply.gear.earned ? 1 : 0) | (reply.gear.equipped ? 2 : 0));
    return frame;
}

inline std::optional<Reply> DecodeReply(std::span<const std::byte> frame) {
    if (frame.size() != HeaderBytes + 10 || !HasMagic(frame) || frame[Magic.size()] != std::byte{2}) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const auto sequence = Read<std::uint64_t>(in);
    const auto result = GearResult(*in++);
    const auto flags = std::to_integer<unsigned>(*in);
    if (!sequence || result > GearResult::Rejected || flags > 3 || flags == 2 ||
        (result == GearResult::NotEarned && flags)) return std::nullopt;
    return Reply{sequence, result, {bool(flags & 1), bool(flags & 2)}};
}
}
