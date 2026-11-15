#pragma once

#include "RiverbankClass.h"
#include <map>
#include <optional>
#include <span>

namespace Mistwatch {
struct ArenaPosition {
    float x{}, z{};
};

enum class EncounterPhase : std::uint8_t { Patrol, Pursuit, Windup, Recovery, Defeated };
enum class RiverbankAction : std::uint8_t { Attack = 1, ClassSkill = 2 };

struct ClassCombatStats {
    unsigned health, attackDamage, attackCooldown, skillCost, skillCooldown;
    float attackRange;
};

inline constexpr ClassCombatStats CombatStatsFor(RiverbankClass playerClass) {
    switch (playerClass) {
    case RiverbankClass::Guardian: return {150, 16, 12, 25, 80, 2.6f};
    case RiverbankClass::Ranger: return {105, 13, 14, 32, 100, 13.f};
    case RiverbankClass::Priest: return {100, 8, 16, 40, 120, 9.f};
    default: return {};
    }
}

struct EncounterPlayerStatus {
    RiverbankClass playerClass{};
    unsigned health{}, resource{}, attackCooldown{}, skillCooldown{}, rewards{};
    bool guarding{};
};

struct SuspendedRiverbankCombat {
    EncounterPlayerStatus status;
    std::uint64_t contributionRun{};
};

struct RiverbankBossStatus {
    EncounterPhase phase{};
    ArenaPosition position{}, strikeCenter{};
    unsigned health{}, phaseTicks{};
    std::uint64_t target{}, run{};
};

class RiverbankEncounter {
public:
    static constexpr unsigned TicksPerSecond = 20;
    static constexpr unsigned BossHealth = 330;
    static constexpr unsigned WindupTicks = 20;
    static constexpr float StrikeRadius = 2.4f;
    static constexpr ArenaPosition NorthBankCenter{0.f, 48.f};
    // The fight stays on the dry clearing. The riverbed starts south of the arena.
    static constexpr bool InArena(ArenaPosition point, ArenaPosition center = NorthBankCenter) {
        return point.x >= center.x - 8.f && point.x <= center.x + 8.f &&
               point.z >= center.z - 5.f && point.z <= center.z + 12.f;
    }
    explicit RiverbankEncounter(ArenaPosition anchor = NorthBankCenter);

    bool Join(std::uint64_t id, RiverbankClass playerClass);
    bool RestoreRewards(std::uint64_t id, unsigned earned);
    bool SetBossGear(std::uint64_t id, bool equipped);
    std::optional<SuspendedRiverbankCombat> Suspend(std::uint64_t id) const;
    bool RestoreCombat(std::uint64_t id, const SuspendedRiverbankCombat& saved, std::uint64_t elapsedTicks);
    bool Leave(std::uint64_t id);
    bool SetPosition(std::uint64_t id, ArenaPosition position);
    bool Submit(std::uint64_t id, std::uint64_t sequence, RiverbankAction action, std::uint64_t ally = 0);
    struct SupportMembership {
        std::uint64_t actor{}, group{};
    };
    void Tick(std::span<const SupportMembership> groups = {});
    std::optional<EncounterPlayerStatus> Player(std::uint64_t id) const;
    const RiverbankBossStatus& Boss() const { return boss; }

private:
    struct Request { RiverbankAction action; std::uint64_t ally; };
    struct Participant {
        EncounterPlayerStatus status;
        std::optional<ArenaPosition> position;
        std::optional<Request> request;
        std::uint64_t sequence{};
        unsigned guardTicks{};
        bool contributed{}, bossGear{};
    };

    bool InRange(const Participant& player, ArenaPosition from, float range) const;
    std::uint64_t Target() const;
    unsigned ResolveAction(std::uint64_t id, Participant& participant, const Request& request,
                           std::span<const SupportMembership> groups);
    void MoveBoss(ArenaPosition target, float step);
    void Reset();
    void Finish();

    ArenaPosition anchor;
    std::map<std::uint64_t, Participant> party;
    RiverbankBossStatus boss;
    std::uint64_t tick{}, tauntOwner{};
    unsigned disengagedTicks{}, patrolLeg{};
};
}
