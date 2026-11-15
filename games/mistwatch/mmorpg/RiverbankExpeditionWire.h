#pragma once

#include "RiverbankBattleWire.h"
#include "RiverbankExpedition.h"

namespace Mistwatch::RiverbankExpeditionWire {
using RiverbankBattleWire::detail::Put;
using RiverbankBattleWire::detail::Read;
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'E'}, std::byte{'X'}, std::byte{1}};
constexpr std::size_t HeaderBytes = Magic.size() + 1;

struct Request { std::uint64_t sequence{}; bool interact{}; };
struct Reply { std::uint64_t sequence{}; ExpeditionStage stage{}; ExpeditionResult result{}; };

inline bool HasMagic(std::span<const std::byte> frame) {
    return frame.size() >= Magic.size() && std::ranges::equal(frame.first(Magic.size()), Magic);
}

inline auto EncodeRequest(Request request) {
    std::array<std::byte, HeaderBytes + 9> frame{};
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{1};
    Put(out, request.sequence);
    *out = std::byte(request.interact);
    return frame;
}

inline std::optional<Request> DecodeRequest(std::span<const std::byte> frame) {
    if (frame.size() != HeaderBytes + 9 || !HasMagic(frame) || frame[Magic.size()] != std::byte{1}) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const auto sequence = Read<std::uint64_t>(in);
    if (!sequence || std::to_integer<unsigned>(*in) > 1) return std::nullopt;
    return Request{sequence, *in == std::byte{1}};
}

inline auto EncodeReply(Reply reply) {
    std::array<std::byte, HeaderBytes + 10> frame{};
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{2};
    Put(out, reply.sequence);
    *out++ = std::byte(reply.stage);
    *out = std::byte(reply.result);
    return frame;
}

inline std::optional<Reply> DecodeReply(std::span<const std::byte> frame) {
    if (frame.size() != HeaderBytes + 10 || !HasMagic(frame) || frame[Magic.size()] != std::byte{2}) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const Reply reply{Read<std::uint64_t>(in), ExpeditionStage(*in), ExpeditionResult(*(in + 1))};
    if (!reply.sequence || reply.stage > ExpeditionStage::Complete || reply.result > ExpeditionResult::Downed) return std::nullopt;
    return reply;
}
}
