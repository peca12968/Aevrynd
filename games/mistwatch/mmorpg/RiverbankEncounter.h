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
enum class RiverbankAction : std::uint8_t { Attack = 1, ClassSkill = 2, ReturnToCamp = 3 };

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
    std::uint64_t contributionRun{}, fieldContributionRun{};
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
    static constexpr unsigned FieldHealth = 84;
    static constexpr unsigned FieldWindupTicks = 24;
    static constexpr float FieldStrikeRadius = 1.5f;
    static constexpr unsigned FieldRespawnTicks = 8 * TicksPerSecond;
    static constexpr ArenaPosition EastPastureCenter{15.f, 15.f};
    static constexpr bool InPasture(ArenaPosition point) {
        return point.x >= 11.f && point.x <= 19.f && point.z >= 11.f && point.z <= 19.f;
    }
    explicit RiverbankEncounter(ArenaPosition anchor = NorthBankCenter);

    bool Join(std::uint64_t id, RiverbankClass playerClass);
    bool RestoreRewards(std::uint64_t id, unsigned earned, unsigned fieldHides = 0);
    bool SetBossGear(std::uint64_t id, bool equipped);
    bool SetTrailPack(std::uint64_t id, bool equipped);
    std::optional<SuspendedRiverbankCombat> Suspend(std::uint64_t id) const;
    bool RestoreCombat(std::uint64_t id, const SuspendedRiverbankCombat& saved, std::uint64_t elapsedTicks);
    bool Leave(std::uint64_t id);
    bool ReturnToCamp(std::uint64_t id, std::uint64_t sequence);
    bool SetPosition(std::uint64_t id, ArenaPosition position);
    bool Submit(std::uint64_t id, std::uint64_t sequence, RiverbankAction action, std::uint64_t ally = 0);
    struct SupportMembership {
        std::uint64_t actor{}, group{};
    };
    void Tick(std::span<const SupportMembership> groups = {});
    std::optional<EncounterPlayerStatus> Player(std::uint64_t id) const;
    const RiverbankBossStatus& Boss() const { return northBank.state; }
    const RiverbankBossStatus& Field() const { return pasture.state; }
    unsigned FieldHides(std::uint64_t id) const;

private:
    struct Request { RiverbankAction action; std::uint64_t ally; };
    struct Participant {
        EncounterPlayerStatus status;
        std::optional<ArenaPosition> position;
        std::optional<Request> request;
        std::uint64_t sequence{};
        unsigned guardTicks{};
        std::uint64_t fieldContributionRun{};
        unsigned fieldHides{};
        bool contributed{}, bossGear{}, trailPack{};
    };

    struct Site {
        ArenaPosition anchor;
        bool field;
        RiverbankBossStatus state{};
        std::uint64_t tauntOwner{};
        unsigned disengagedTicks{}, patrolLeg{};

        bool Contains(ArenaPosition point) const { return field ? InPasture(point) : InArena(point, anchor); }
        unsigned MaxHealth() const { return field ? FieldHealth : BossHealth; }
    };

    bool InRange(const Participant& player, ArenaPosition from, float range) const;
    std::uint64_t Target(const Site& site) const;
    unsigned ResolveAction(Site& site, std::uint64_t id, Participant& participant, const Request& request,
                           std::span<const SupportMembership> groups);
    void CreditContribution(const Site& site, Participant& participant);
    static void Move(Site& site, ArenaPosition target, float step);
    void Advance(Site& site, unsigned damage);
    void Reset(Site& site);
    void Finish(Site& site);

    std::map<std::uint64_t, Participant> party;
    Site northBank, pasture;
    std::uint64_t tick{};
};
}
