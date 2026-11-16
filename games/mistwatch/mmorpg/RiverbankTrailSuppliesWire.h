#pragma once

#include "RiverbankBattleWire.h"
#include "RiverbankTrailSupplies.h"

namespace Mistwatch::RiverbankTrailSuppliesWire {
using RiverbankBattleWire::detail::Put;
using RiverbankBattleWire::detail::Read;
inline constexpr std::uint32_t Capability = 1u << 17;
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'S'}, std::byte{'C'}, std::byte{1}};
constexpr std::size_t HeaderBytes = Magic.size() + 1;

struct Request { std::uint64_t sequence{}; TrailSupplyAction action{}; };
struct Reply { std::uint64_t sequence{}; TrailSupplyResult result{}; TrailSupplyState state{}; };

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
    const Request request{Read<std::uint64_t>(in), TrailSupplyAction(*in)};
    if (!request.sequence || request.action > TrailSupplyAction::Exchange) return std::nullopt;
    return request;
}

inline auto EncodeReply(Reply reply) {
    std::array<std::byte, HeaderBytes + 14> frame{};
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{2};
    Put(out, reply.sequence);
    *out++ = std::byte(reply.result);
    Put(out, std::uint32_t(reply.state.earnedHides));
    *out = std::byte(reply.state.ownsPack ? 1 : 0);
    return frame;
}

inline std::optional<Reply> DecodeReply(std::span<const std::byte> frame) {
    if (frame.size() != HeaderBytes + 14 || !HasMagic(frame) || frame[Magic.size()] != std::byte{2}) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const auto sequence = Read<std::uint64_t>(in);
    const auto result = TrailSupplyResult(*in++);
    const auto earned = Read<std::uint32_t>(in);
    const auto owned = std::to_integer<unsigned>(*in);
    if (!sequence || result > TrailSupplyResult::Rejected || owned > 1 ||
        (owned && earned < RiverbankTrailSupplies::HideCost) ||
        ((!owned) && (result == TrailSupplyResult::Exchanged || result == TrailSupplyResult::AlreadyOwned)))
        return std::nullopt;
    return Reply{sequence, result, {earned, owned != 0}};
}
}
