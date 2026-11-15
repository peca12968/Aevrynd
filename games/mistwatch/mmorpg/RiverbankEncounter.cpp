#include "RiverbankEncounter.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Mistwatch {
namespace {
constexpr float AggroRadius = 15.f;
constexpr unsigned ResetDelay = 4 * RiverbankEncounter::TicksPerSecond;
constexpr unsigned RespawnDelay = 12 * RiverbankEncounter::TicksPerSecond;
constexpr unsigned MaxResource = 100;

float Distance(ArenaPosition left, ArenaPosition right) {
    return std::hypot(left.x - right.x, left.z - right.z);
}

bool Finite(ArenaPosition point) { return std::isfinite(point.x) && std::isfinite(point.z); }
}

RiverbankEncounter::RiverbankEncounter(ArenaPosition center) : anchor(center) {
    if (!Finite(center)) throw std::invalid_argument("Invalid riverbank arena position");
    Reset();
}

bool RiverbankEncounter::Join(std::uint64_t id, RiverbankClass playerClass) {
    if (!id || !IsRiverbankClass(playerClass)) return false;
    Participant player{};
    player.status = {playerClass, CombatStatsFor(playerClass).health, MaxResource};
    return party.try_emplace(id, std::move(player)).second;
}

bool RiverbankEncounter::Leave(std::uint64_t id) {
    if (tauntOwner == id) tauntOwner = 0;
    return party.erase(id) > 0;
}

bool RiverbankEncounter::RestoreRewards(std::uint64_t id, unsigned earned) {
    const auto player = party.find(id);
    if (player == party.end() || player->second.position || player->second.sequence) return false;
    player->second.status.rewards = earned;
    return true;
}

bool RiverbankEncounter::SetPosition(std::uint64_t id, ArenaPosition position) {
    const auto player = party.find(id);
    if (player == party.end() || !Finite(position)) return false;
    player->second.position = position;
    return true;
}

bool RiverbankEncounter::Submit(std::uint64_t id, std::uint64_t sequence, RiverbankAction action, std::uint64_t ally) {
    const auto player = party.find(id);
    if (player == party.end() || !sequence || sequence <= player->second.sequence || player->second.request ||
        (action != RiverbankAction::Attack && action != RiverbankAction::ClassSkill) ||
        (ally && (action != RiverbankAction::ClassSkill || player->second.status.playerClass != RiverbankClass::Priest))) return false;
    player->second.sequence = sequence;
    player->second.request = Request{action, ally};
    return true;
}

std::optional<EncounterPlayerStatus> RiverbankEncounter::Player(std::uint64_t id) const {
    const auto entry = party.find(id);
    return entry == party.end() ? std::nullopt : std::optional{entry->second.status};
}

bool RiverbankEncounter::InRange(const Participant& player, ArenaPosition from, float range) const {
    return player.position && Distance(*player.position, from) <= range;
}

std::uint64_t RiverbankEncounter::Target() const {
    const auto guarded = party.find(tauntOwner);
    if (guarded != party.end() && guarded->second.status.health && guarded->second.guardTicks &&
        guarded->second.position && InArena(*guarded->second.position, anchor) &&
        InRange(guarded->second, boss.position, AggroRadius)) return tauntOwner;
    std::uint64_t nearest{};
    float distance = AggroRadius;
    for (const auto& [id, member] : party) {
        if (!member.status.health || !member.position || !InArena(*member.position, anchor)) continue;
        const float next = Distance(*member.position, boss.position);
        if (next < distance) { nearest = id; distance = next; }
    }
    return nearest;
}

unsigned RiverbankEncounter::ResolveAction(std::uint64_t id, Participant& participant, const Request& request) {
    auto& status = participant.status;
    if (!status.health || !participant.position || !InArena(*participant.position, anchor)) return 0;
    const auto stats = CombatStatsFor(status.playerClass);
    if (request.action == RiverbankAction::Attack) {
        if (status.attackCooldown || !InRange(participant, boss.position, stats.attackRange)) return 0;
        status.attackCooldown = stats.attackCooldown;
        participant.contributed = true;
        return stats.attackDamage;
    }
    if (status.skillCooldown || status.resource < stats.skillCost) return 0;

    unsigned damage = 0;
    switch (status.playerClass) {
    case RiverbankClass::Guardian:
        if (!InRange(participant, boss.position, 6.f)) return 0;
        participant.guardTicks = 45;
        status.guarding = true;
        tauntOwner = id;
        break;
    case RiverbankClass::Ranger:
        if (!InRange(participant, boss.position, stats.attackRange)) return 0;
        damage = 28;
        break;
    case RiverbankClass::Priest: {
        auto ally = party.find(request.ally);
        if (!request.ally) {
            float lowestHealthFraction = 1.f;
            for (auto choice = party.begin(); choice != party.end(); ++choice) {
                const auto& health = choice->second.status;
                const float fraction = float(health.health) / float(CombatStatsFor(health.playerClass).health);
                if (fraction >= lowestHealthFraction || !InRange(choice->second, *participant.position, 9.f)) continue;
                lowestHealthFraction = fraction;
                ally = choice;
            }
        }
        if (ally == party.end() || !InRange(ally->second, *participant.position, 9.f)) return 0;
        auto& recipient = ally->second.status;
        const auto maximum = CombatStatsFor(recipient.playerClass).health;
        if (recipient.health == maximum) return 0;
        recipient.health = recipient.health ? std::min(maximum, recipient.health + 40u) : maximum * 2 / 5;
        break;
    }
    default: return 0;
    }
    status.resource -= stats.skillCost;
    status.skillCooldown = stats.skillCooldown;
    participant.contributed = true;
    return damage;
}

void RiverbankEncounter::Tick() {
    ++tick;
    unsigned damage = 0;
    for (auto& [id, participant] : party) {
        auto& status = participant.status;
        if (status.attackCooldown) --status.attackCooldown;
        if (status.skillCooldown) --status.skillCooldown;
        if (participant.guardTicks) --participant.guardTicks;
        status.guarding = participant.guardTicks != 0;
        if (tick % 5 == 0 && status.resource < MaxResource) ++status.resource;
        if (participant.request && boss.phase != EncounterPhase::Defeated)
            damage = std::min(BossHealth, damage + ResolveAction(id, participant, *participant.request));
        participant.request.reset();
    }
    if (boss.phase == EncounterPhase::Defeated) {
        if (--boss.phaseTicks == 0) Reset();
        return;
    }
    if (damage >= boss.health) { Finish(); return; }
    boss.health -= damage;

    const auto target = Target();
    disengagedTicks = target || boss.phase == EncounterPhase::Patrol ? 0 : disengagedTicks + 1;
    if (!target && boss.phase != EncounterPhase::Patrol && disengagedTicks >= ResetDelay) { Reset(); return; }
    if (boss.phase == EncounterPhase::Windup) {
        if (--boss.phaseTicks == 0) {
            for (auto& [id, victim] : party) {
                auto& status = victim.status;
                if (!status.health || !victim.position || !InArena(*victim.position, anchor) ||
                    !InRange(victim, boss.strikeCenter, StrikeRadius)) continue;
                const unsigned hit = status.guarding ? 16u : 46u;
                status.health -= std::min(status.health, hit);
            }
            boss.phase = EncounterPhase::Recovery;
            boss.phaseTicks = 18;
        }
        return;
    }
    if (boss.phase == EncounterPhase::Recovery && --boss.phaseTicks) return;
    boss.target = target;
    if (!target) {
        // 尚未交戰時巡邏。交戰後若隊伍撤離，等待首領重置。
        if (boss.phase == EncounterPhase::Patrol) {
            const ArenaPosition goal{anchor.x + (patrolLeg ? -2.f : 2.f), anchor.z};
            MoveBoss(goal, .035f);
            if (Distance(boss.position, goal) <= .001f) patrolLeg ^= 1u;
        } else boss.phase = EncounterPhase::Pursuit;
        return;
    }
    boss.phase = EncounterPhase::Pursuit;
    const auto position = *party.at(target).position;
    if (Distance(boss.position, position) <= 1.65f) {
        boss.phase = EncounterPhase::Windup;
        boss.strikeCenter = position;
        boss.phaseTicks = WindupTicks;
    } else {
        MoveBoss(position, .14f);
    }
}

void RiverbankEncounter::MoveBoss(ArenaPosition position, float step) {
    const auto distance = Distance(boss.position, position);
    if (distance <= step) { boss.position = position; return; }
    boss.position.x += (position.x - boss.position.x) * step / distance;
    boss.position.z += (position.z - boss.position.z) * step / distance;
}

void RiverbankEncounter::Finish() {
    boss.health = 0;
    boss.phase = EncounterPhase::Defeated;
    boss.target = 0;
    boss.phaseTicks = RespawnDelay;
    for (auto& [id, participant] : party) {
        auto& status = participant.status;
        if (participant.contributed && participant.position && InArena(*participant.position, anchor) &&
            status.rewards < (std::numeric_limits<unsigned>::max)()) ++status.rewards;
    }
}

void RiverbankEncounter::Reset() {
    boss = {EncounterPhase::Patrol, anchor, {}, BossHealth, 0, 0, boss.run + 1};
    disengagedTicks = patrolLeg = 0;
    tauntOwner = 0;
    for (auto& [id, participant] : party) participant.contributed = false;
}
}
