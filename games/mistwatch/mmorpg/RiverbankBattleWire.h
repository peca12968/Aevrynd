#pragma once

#include "RiverbankEncounter.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>

namespace Mistwatch::RiverbankBattleWire {
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'C'}, std::byte{'B'}};
constexpr std::size_t HeaderBytes = Magic.size() + 2;
inline constexpr std::uint32_t CampReturnCapability = 1u << 18;
inline constexpr std::uint32_t StructureCoverCapability = 1u << 24;
inline constexpr std::uint32_t DownedEntityFlag = 1u << 19;
enum class Kind : std::uint8_t { Action = 1, Encounter = 2 };

struct Action {
    std::uint64_t sequence{}, ally{};
    RiverbankAction kind{};
};

struct Encounter {
    std::uint64_t epoch{}, tick{};
    std::uint32_t owner{};
    EncounterPlayerStatus player;
    RiverbankBossStatus boss;
};

inline bool HasMagic(std::span<const std::byte> input) {
    return input.size() >= Magic.size() && std::ranges::equal(input.first(Magic.size()), Magic);
}

namespace detail {
template<std::unsigned_integral T>
void Put(std::byte*& out, T value) {
    for (std::size_t byte = 0; byte < sizeof(T); ++byte) *out++ = std::byte(value >> (8 * byte));
}

template<std::unsigned_integral T>
T Read(const std::byte*& input) {
    T value{};
    for (std::size_t byte = 0; byte < sizeof(T); ++byte)
        value = static_cast<T>(std::uint64_t(value) | (std::uint64_t(std::to_integer<unsigned>(*input++)) << (8 * byte)));
    return value;
}

inline void Header(std::byte*& out, Kind kind) {
    out = std::ranges::copy(Magic, out).out;
    *out++ = std::byte{1};
    *out++ = std::byte(kind);
}

inline bool Accept(std::span<const std::byte> input, std::size_t size, Kind kind) {
    return input.size() == size && HasMagic(input) && input[Magic.size()] == std::byte{1} &&
        input[Magic.size() + 1] == std::byte(kind);
}

inline void PutPoint(std::byte*& out, ArenaPosition point) {
    Put(out, std::bit_cast<std::uint32_t>(point.x));
    Put(out, std::bit_cast<std::uint32_t>(point.z));
}

inline ArenaPosition ReadPoint(const std::byte*& input) {
    return {std::bit_cast<float>(Read<std::uint32_t>(input)), std::bit_cast<float>(Read<std::uint32_t>(input))};
}
}

inline constexpr std::size_t ActionBytes = HeaderBytes + 2 * sizeof(std::uint64_t) + 1;
inline constexpr std::size_t EncounterBytes = HeaderBytes + 4 * sizeof(std::uint64_t) + 12 * sizeof(std::uint32_t) + 3;

inline auto EncodeAction(Action value) {
    std::array<std::byte, ActionBytes> result{};
    auto* out = result.data();
    detail::Header(out, Kind::Action);
    detail::Put(out, value.sequence);
    detail::Put(out, value.ally);
    *out = std::byte(value.kind);
    return result;
}

inline std::optional<Action> DecodeAction(std::span<const std::byte> frame) {
    if (!detail::Accept(frame, ActionBytes, Kind::Action)) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const Action value{detail::Read<std::uint64_t>(in), detail::Read<std::uint64_t>(in), RiverbankAction(*in)};
    if (!value.sequence || (value.kind != RiverbankAction::Attack && value.kind != RiverbankAction::ClassSkill &&
        (value.kind != RiverbankAction::ReturnToCamp || value.ally))) return std::nullopt;
    return value;
}

inline auto EncodeEncounter(const Encounter& value) {
    using namespace detail;
    std::array<std::byte, EncounterBytes> result{};
    auto* out = result.data();
    Header(out, Kind::Encounter);
    Put(out, value.epoch);
    Put(out, value.tick);
    Put(out, value.owner);
    const auto& player = value.player;
    *out++ = std::byte(player.playerClass);
    *out++ = std::byte(player.guarding ? 1 : 0);
    for (unsigned field : {player.health, player.resource, player.attackCooldown, player.skillCooldown, player.rewards})
        Put(out, std::uint32_t(field));
    const auto& boss = value.boss;
    *out++ = std::byte(boss.phase);
    PutPoint(out, boss.position);
    PutPoint(out, boss.strikeCenter);
    Put(out, std::uint32_t(boss.health));
    Put(out, std::uint32_t(boss.phaseTicks));
    Put(out, boss.target);
    Put(out, boss.run);
    return result;
}

inline std::optional<Encounter> DecodeEncounter(std::span<const std::byte> frame) {
    using namespace detail;
    if (!Accept(frame, EncounterBytes, Kind::Encounter)) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    Encounter value{};
    value.epoch = Read<std::uint64_t>(in);
    value.tick = Read<std::uint64_t>(in);
    value.owner = Read<std::uint32_t>(in);
    auto& player = value.player;
    player.playerClass = RiverbankClass(*in++);
    const unsigned guarding = std::to_integer<unsigned>(*in++);
    if (!value.epoch || !value.tick || !value.owner || !IsRiverbankClass(player.playerClass) || guarding > 1) return std::nullopt;
    player.guarding = guarding != 0;
    for (auto* field : {&player.health, &player.resource, &player.attackCooldown, &player.skillCooldown, &player.rewards})
        *field = Read<std::uint32_t>(in);
    auto& boss = value.boss;
    boss.phase = EncounterPhase(*in++);
    boss.position = ReadPoint(in);
    boss.strikeCenter = ReadPoint(in);
    boss.health = Read<std::uint32_t>(in);
    boss.phaseTicks = Read<std::uint32_t>(in);
    boss.target = Read<std::uint64_t>(in);
    boss.run = Read<std::uint64_t>(in);
    const auto stats = CombatStatsFor(player.playerClass);
    if (player.health > stats.health || player.resource > 100 || player.attackCooldown > stats.attackCooldown ||
        player.skillCooldown > stats.skillCooldown || (player.guarding && player.playerClass != RiverbankClass::Guardian) ||
        boss.phase > EncounterPhase::Defeated || !boss.run || boss.health > RiverbankEncounter::BossHealth ||
        (boss.phase == EncounterPhase::Defeated) != (boss.health == 0) || boss.phaseTicks > 12 * RiverbankEncounter::TicksPerSecond ||
        !std::isfinite(boss.position.x) || !std::isfinite(boss.position.z) ||
        !std::isfinite(boss.strikeCenter.x) || !std::isfinite(boss.strikeCenter.z)) return std::nullopt;
    return value;
}
}
