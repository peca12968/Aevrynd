#pragma once

#include "RiverbankProfileStore.h"
#include <algorithm>

namespace Mistwatch::RiverbankProfileWire {
constexpr std::array Prefix{std::byte{'A'}, std::byte{'E'}, std::byte{'R'}, std::byte{'P'}, std::byte{1}};
constexpr std::size_t ResponseBytes = Prefix.size() + 2;
constexpr std::size_t RequestBytes = ResponseBytes + RiverbankTicket{}.size();

struct Request {
    RiverbankTicket ticket;
    RiverbankClass choice;
};

inline bool HasPrefix(std::span<const std::byte> bytes) {
    return bytes.size() >= Prefix.size() && std::ranges::equal(bytes.first(Prefix.size()), Prefix);
}

inline auto EncodeRequest(const RiverbankTicket& ticket, RiverbankClass choice) {
    std::array<std::byte, RequestBytes> result{};
    auto next = std::ranges::copy(Prefix, result.begin()).out;
    *next++ = std::byte{1};
    *next++ = std::byte(choice);
    std::ranges::copy(ticket, next);
    return result;
}

inline std::optional<Request> DecodeRequest(std::span<const std::byte> bytes) {
    if (bytes.size() != RequestBytes || !HasPrefix(bytes) || bytes[Prefix.size()] != std::byte{1}) return std::nullopt;
    Request request{{}, RiverbankClass(bytes[Prefix.size() + 1])};
    std::ranges::copy(bytes.subspan(ResponseBytes), request.ticket.begin());
    if ((request.choice != RiverbankClass::None && !IsRiverbankClass(request.choice)) ||
        std::ranges::all_of(request.ticket, [](std::byte value) { return value == std::byte{}; })) return std::nullopt;
    return request;
}

inline auto EncodeResponse(RiverbankClass playerClass) {
    return std::array{Prefix[0], Prefix[1], Prefix[2], Prefix[3], Prefix[4], std::byte{2}, std::byte(playerClass)};
}

inline std::optional<RiverbankClass> DecodeResponse(std::span<const std::byte> bytes) {
    if (bytes.size() != ResponseBytes || !HasPrefix(bytes) || bytes[Prefix.size()] != std::byte{2}) return std::nullopt;
    const auto value = RiverbankClass(bytes.back());
    if (value != RiverbankClass::None && !IsRiverbankClass(value)) return std::nullopt;
    return value;
}
}
