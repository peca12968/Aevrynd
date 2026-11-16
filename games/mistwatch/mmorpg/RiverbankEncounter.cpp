#include "RiverbankEncounter.h"
#include "RiverbankGear.h"
#include "RiverbankTrailSupplies.h"
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

RiverbankEncounter::RiverbankEncounter(ArenaPosition center) : northBank{center, false}, pasture{EastPastureCenter, true} {
    if (!Finite(center)) throw std::invalid_argument("Invalid riverbank arena position");
    Reset(northBank);
    Reset(pasture);
}

bool RiverbankEncounter::Join(std::uint64_t id, RiverbankClass playerClass) {
    if (!id || !IsRiverbankClass(playerClass)) return false;
    Participant player{};
    player.status = {playerClass, CombatStatsFor(playerClass).health, MaxResource};
    return party.try_emplace(id, std::move(player)).second;
}

bool RiverbankEncounter::Leave(std::uint64_t id) {
    if (northBank.tauntOwner == id) northBank.tauntOwner = 0;
    if (pasture.tauntOwner == id) pasture.tauntOwner = 0;
    return party.erase(id) > 0;
}

bool RiverbankEncounter::ReturnToCamp(std::uint64_t id, std::uint64_t sequence) {
    const auto found = party.find(id);
    if (found == party.end() || !sequence || sequence <= found->second.sequence || found->second.status.health) return false;
    auto& player = found->second;
    player.sequence = sequence;
    player.request.reset();
    player.position = ArenaPosition{};
    player.status.health = CombatStatsFor(player.status.playerClass).health;
    player.guardTicks = 0;
    player.status.guarding = false;
    // 回營地放棄這一輪戰鬥的參與資格，已領取的戰利品與進度仍保留。
    player.contributed = false;
    player.fieldContributionRun = 0;
    if (northBank.tauntOwner == id) northBank.tauntOwner = 0;
    if (pasture.tauntOwner == id) pasture.tauntOwner = 0;
    return true;
}

bool RiverbankEncounter::RestoreRewards(std::uint64_t id, unsigned earned, unsigned fieldHides) {
    const auto player = party.find(id);
    if (player == party.end() || player->second.position || player->second.sequence) return false;
    player->second.status.rewards = earned;
    player->second.fieldHides = fieldHides;
    return true;
}

bool RiverbankEncounter::SetBossGear(std::uint64_t id, bool equipped) {
    const auto player = party.find(id);
    if (player == party.end()) return false;
    player->second.bossGear = equipped;
    return true;
}

bool RiverbankEncounter::SetTrailPack(std::uint64_t id, bool equipped) {
    const auto player = party.find(id);
    if (player == party.end()) return false;
    player->second.trailPack = equipped;
    return true;
}

std::optional<SuspendedRiverbankCombat> RiverbankEncounter::Suspend(std::uint64_t id) const {
    const auto player = party.find(id);
    if (player == party.end()) return std::nullopt;
    return SuspendedRiverbankCombat{player->second.status, player->second.contributed ? Boss().run : 0,
                                    player->second.fieldContributionRun};
}

bool RiverbankEncounter::RestoreCombat(std::uint64_t id, const SuspendedRiverbankCombat& saved, std::uint64_t elapsedTicks) {
    const auto player = party.find(id);
    if (player == party.end() || player->second.position || player->second.sequence ||
        player->second.status.playerClass != saved.status.playerClass) return false;

    auto& status = player->second.status;
    const auto cooldown = [elapsedTicks](unsigned ticks) { return elapsedTicks < ticks ? ticks - static_cast<unsigned>(elapsedTicks) : 0u; };
    status.health = saved.status.health;
    const auto recovered = (elapsedTicks / RiverbankTrailSupplies::RecoveryPulseTicks) *
        RiverbankTrailSupplies::RecoveryPerPulse(player->second.trailPack);
    status.resource = static_cast<unsigned>((std::min)(std::uint64_t(MaxResource), saved.status.resource + recovered));
    status.attackCooldown = cooldown(saved.status.attackCooldown);
    status.skillCooldown = cooldown(saved.status.skillCooldown);
    player->second.contributed = Boss().phase != EncounterPhase::Defeated && saved.contributionRun == Boss().run;
    if (Field().phase != EncounterPhase::Defeated && saved.fieldContributionRun == Field().run)
        player->second.fieldContributionRun = saved.fieldContributionRun;
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

unsigned RiverbankEncounter::FieldHides(std::uint64_t id) const {
    const auto entry = party.find(id);
    return entry == party.end() ? 0u : entry->second.fieldHides;
}

bool RiverbankEncounter::InRange(const Participant& player, ArenaPosition from, float range) const {
    return player.position && Distance(*player.position, from) <= range;
}

std::uint64_t RiverbankEncounter::Target(const Site& site) const {
    const auto& boss = site.state;
    const float sight = site.field ? 8.f : AggroRadius;
    const auto guarded = party.find(site.tauntOwner);
    if (guarded != party.end() && guarded->second.status.health && guarded->second.guardTicks &&
        guarded->second.position && site.Contains(*guarded->second.position) &&
        InRange(guarded->second, boss.position, sight)) return site.tauntOwner;
    std::uint64_t nearest{};
    float distance = sight;
    for (const auto& [id, member] : party) {
        if (!member.status.health || !member.position || !site.Contains(*member.position)) continue;
        const float next = Distance(*member.position, boss.position);
        if (next < distance) { nearest = id; distance = next; }
    }
    return nearest;
}

unsigned RiverbankEncounter::ResolveAction(Site& site, std::uint64_t id, Participant& participant, const Request& request,
                                           std::span<const SupportMembership> groups) {
    auto& status = participant.status;
    if (!status.health) return 0;
    const auto& boss = site.state;
    const auto stats = CombatStatsFor(status.playerClass);
    if (request.action == RiverbankAction::Attack) {
        if (status.attackCooldown || !InRange(participant, boss.position, stats.attackRange)) return 0;
        status.attackCooldown = stats.attackCooldown;
        CreditContribution(site, participant);
        return stats.attackDamage;
    }
    if (status.skillCooldown || status.resource < stats.skillCost) return 0;

    unsigned damage = 0;
    switch (status.playerClass) {
    case RiverbankClass::Guardian:
        if (!InRange(participant, boss.position, 6.f)) return 0;
        participant.guardTicks = 45;
        status.guarding = true;
        site.tauntOwner = id;
        break;
    case RiverbankClass::Ranger:
        if (!InRange(participant, boss.position, stats.attackRange)) return 0;
        damage = participant.bossGear ? RiverbankBossGearStats::VolleyDamage : 28u;
        break;
    case RiverbankClass::Priest: {
        // 以伺服器本次結算的隊伍判定支援資格。未組隊的玩家可彼此支援。
        const auto groupFor = [groups](std::uint64_t actor) {
            const auto member = std::ranges::find(groups, actor, &SupportMembership::actor);
            return member == groups.end() ? std::uint64_t{} : member->group;
        };
        const auto casterGroup = groupFor(id);
        auto ally = party.find(request.ally);
        if (!request.ally) {
            float lowestHealthFraction = 1.f;
            for (auto choice = party.begin(); choice != party.end(); ++choice) {
                if (groupFor(choice->first) != casterGroup) continue;
                const auto& health = choice->second.status;
                const float fraction = float(health.health) / float(CombatStatsFor(health.playerClass).health);
                if (fraction >= lowestHealthFraction || !InRange(choice->second, *participant.position, 9.f)) continue;
                lowestHealthFraction = fraction;
                ally = choice;
            }
        }
        if (ally == party.end() || groupFor(ally->first) != casterGroup ||
            !InRange(ally->second, *participant.position, 9.f)) return 0;
        auto& recipient = ally->second.status;
        const auto maximum = CombatStatsFor(recipient.playerClass).health;
        if (recipient.health == maximum) return 0;
        const auto heal = participant.bossGear ? RiverbankBossGearStats::HealAmount : 40u;
        recipient.health = recipient.health ? std::min(maximum, recipient.health + heal) : maximum * 2 / 5;
        break;
    }
    default: return 0;
    }
    status.resource -= stats.skillCost;
    status.skillCooldown = stats.skillCooldown;
    CreditContribution(site, participant);
    return damage;
}

void RiverbankEncounter::CreditContribution(const Site& site, Participant& participant) {
    if (site.field) participant.fieldContributionRun = site.state.run;
    else participant.contributed = true;
}

void RiverbankEncounter::Tick(std::span<const SupportMembership> groups) {
    ++tick;
    unsigned bossDamage = 0, fieldDamage = 0;
    for (auto& [id, participant] : party) {
        auto& status = participant.status;
        if (status.attackCooldown) --status.attackCooldown;
        if (status.skillCooldown) --status.skillCooldown;
        if (participant.guardTicks) --participant.guardTicks;
        status.guarding = participant.guardTicks != 0;
        if (tick % RiverbankTrailSupplies::RecoveryPulseTicks == 0)
            status.resource = std::min(MaxResource, status.resource + RiverbankTrailSupplies::RecoveryPerPulse(participant.trailPack));
        if (participant.request && participant.position) {
            if (northBank.Contains(*participant.position) && Boss().phase != EncounterPhase::Defeated)
                bossDamage = std::min(BossHealth, bossDamage + ResolveAction(northBank, id, participant, *participant.request, groups));
            else if (pasture.Contains(*participant.position) && Field().phase != EncounterPhase::Defeated)
                fieldDamage = std::min(FieldHealth, fieldDamage + ResolveAction(pasture, id, participant, *participant.request, groups));
        }
        participant.request.reset();
    }
    Advance(northBank, bossDamage);
    Advance(pasture, fieldDamage);
}

void RiverbankEncounter::Advance(Site& site, unsigned damage) {
    auto& boss = site.state;
    if (boss.phase == EncounterPhase::Defeated) {
        if (--boss.phaseTicks == 0) Reset(site);
        return;
    }
    if (damage >= boss.health) { Finish(site); return; }
    boss.health -= damage;

    const auto target = Target(site);
    site.disengagedTicks = target || boss.phase == EncounterPhase::Patrol ? 0 : site.disengagedTicks + 1;
    if (!target && boss.phase != EncounterPhase::Patrol && site.disengagedTicks >= ResetDelay) { Reset(site); return; }
    if (boss.phase == EncounterPhase::Windup) {
        if (--boss.phaseTicks == 0) {
            for (auto& [id, victim] : party) {
                auto& status = victim.status;
                if (!status.health || !victim.position || !site.Contains(*victim.position) ||
                    !InRange(victim, boss.strikeCenter, site.field ? FieldStrikeRadius : StrikeRadius)) continue;
                const auto guardedHit = site.field ? 8u : victim.bossGear ? RiverbankBossGearStats::GuardDamageTaken : 16u;
                const unsigned hit = status.guarding ? guardedHit : site.field ? 22u : 46u;
                status.health -= std::min(status.health, hit);
            }
            boss.phase = EncounterPhase::Recovery;
            boss.phaseTicks = 18;
        }
        return;
    }
    if (boss.phase == EncounterPhase::Recovery) {
        if (site.field && !target) {
            boss.target = 0;
            // 擊倒最後一個目標後退開，保留原脫戰計時與野豬生命值。
            constexpr float clearance = 2.80f;
            const float distance = Distance(boss.position, boss.strikeCenter);
            if (distance < clearance) {
                const float dx = distance > .001f ? (boss.position.x - boss.strikeCenter.x) / distance : 1.f;
                const float dz = distance > .001f ? (boss.position.z - boss.strikeCenter.z) / distance : 0.f;
                ArenaPosition goal{std::clamp(boss.strikeCenter.x + dx * clearance, site.anchor.x - 4.f, site.anchor.x + 4.f),
                                   std::clamp(boss.strikeCenter.z + dz * clearance, site.anchor.z - 4.f, site.anchor.z + 4.f)};
                const float room = Distance(goal, boss.strikeCenter);
                if (room < clearance - .001f && Distance(site.anchor, boss.strikeCenter) > room) goal = site.anchor;
                Move(site, goal, .15f);
            }
        }
        if (--boss.phaseTicks) return;
    }
    boss.target = target;
    if (!target) {
        // Patrol is only for an unengaged encounter. A withdrawn party lets an engaged boss reset.
        if (boss.phase == EncounterPhase::Patrol) {
            const ArenaPosition goal{site.anchor.x + (site.patrolLeg ? -2.f : 2.f), site.anchor.z};
            Move(site, goal, site.field ? .055f : .035f);
            if (Distance(boss.position, goal) <= .001f) site.patrolLeg ^= 1u;
        } else boss.phase = EncounterPhase::Pursuit;
        return;
    }
    boss.phase = EncounterPhase::Pursuit;
    const auto position = *party.at(target).position;
    if (Distance(boss.position, position) <= (site.field ? 1.30f : 1.65f)) {
        boss.phase = EncounterPhase::Windup;
        boss.strikeCenter = position;
        boss.phaseTicks = site.field ? FieldWindupTicks : WindupTicks;
    } else {
        Move(site, position, site.field ? .15f : .14f);
    }
}

void RiverbankEncounter::Move(Site& site, ArenaPosition position, float step) {
    auto& boss = site.state;
    const auto distance = Distance(boss.position, position);
    if (distance <= step) { boss.position = position; return; }
    boss.position.x += (position.x - boss.position.x) * step / distance;
    boss.position.z += (position.z - boss.position.z) * step / distance;
}

void RiverbankEncounter::Finish(Site& site) {
    auto& boss = site.state;
    boss.health = 0;
    boss.phase = EncounterPhase::Defeated;
    boss.target = 0;
    boss.phaseTicks = site.field ? FieldRespawnTicks : RespawnDelay;
    for (auto& [id, participant] : party) {
        const bool contributed = site.field ? participant.fieldContributionRun == boss.run : participant.contributed;
        if (!contributed || !participant.position || !site.Contains(*participant.position)) continue;
        auto& reward = site.field ? participant.fieldHides : participant.status.rewards;
        if (reward < (std::numeric_limits<unsigned>::max)()) ++reward;
    }
}

void RiverbankEncounter::Reset(Site& site) {
    site.state = {EncounterPhase::Patrol, site.anchor, {}, site.MaxHealth(), 0, 0, site.state.run + 1};
    site.disengagedTicks = site.patrolLeg = 0;
    site.tauntOwner = 0;
    if (!site.field)
        for (auto& [id, participant] : party) participant.contributed = false;
}

}
