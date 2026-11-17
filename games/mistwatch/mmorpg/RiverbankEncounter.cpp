#include "RiverbankEncounter.h"
#include "RiverbankGear.h"
#include "RiverbankTrailSupplies.h"
#include "RiverbankStructureMovement.h"
#include "RiverbankTerritory.h"
#include "RiverbankStrikeCue.h"
#include <CharacterDynamicContact3D.h>
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

// 共用單人模式人體半寬與野豬身體半徑。多人伺服器目前只裁定水平接觸。
constexpr auto PlayerWidth = RiverbankEncounter::PlayerContactHalfWidth;
constexpr Aevrynd::Vec3 PlayerHalfExtent{PlayerWidth,.90f,PlayerWidth};
constexpr float BoarRadius=RiverbankEncounter::BoarBodyRadius, BoarHalfHeight=.80f, ContactMargin=.002f;

Aevrynd::Vec3 Planar(ArenaPosition point) { return {point.x,0.f,point.z}; }

bool SweepBody(Aevrynd::Vec3 player,Aevrynd::Vec3 boar,Aevrynd::Vec3 relative,
               Aevrynd::BoxCylinderContact3D& contact) {
    const float broad=BoarRadius+PlayerHalfExtent.x+Aevrynd::Length(relative);
    if(std::abs(player.x-boar.x)>broad||std::abs(player.z-boar.z)>broad){
        contact={};
        return true;
    }
    return Aevrynd::QueryBoxCylinderContact3D(player,PlayerHalfExtent,relative,{boar,BoarRadius,BoarHalfHeight},contact);
}

Aevrynd::Vec3 ClipBodyStep(Aevrynd::Vec3 player,Aevrynd::Vec3 boar,Aevrynd::Vec3 relative,bool allowSlide) {
    using Kind=Aevrynd::BoxCylinderContactKind3D;
    const float length=Aevrynd::Length(relative);
    if(length==0.f)return {};
    Aevrynd::BoxCylinderContact3D hit;
    if(!SweepBody(player,boar,relative,hit))return {};
    if(hit.kind==Kind::Clear)return relative;
    const float into=Aevrynd::Dot(relative,hit.horizontalNormal);
    if(hit.kind==Kind::StartedOverlapping){
        Aevrynd::BoxCylinderContact3D end;
        // 重生或舊座標已重疊時，只接受沿表面向外且降低穿入量的位移。
        if(into>0.f&&SweepBody(player+relative,boar,{},end)&&
           (end.kind==Kind::Clear||end.horizontalPenetration<hit.horizontalPenetration))return relative;
        return {};
    }
    if(hit.kind!=Kind::SideEntry)return {};
    const auto backed=relative*std::max(0.f,static_cast<float>(hit.first)-ContactMargin/length);
    if(!allowSlide||into>=0.f)return backed;
    const float normalLength=Aevrynd::Dot(hit.horizontalNormal,hit.horizontalNormal);
    if(normalLength<=0.f)return backed;
    const auto tangent=relative-hit.horizontalNormal*(into/normalLength);
    // 快照只有起點與終點，側滑候選本身也必須是一條安全直線。
    Aevrynd::BoxCylinderContact3D side;
    if(Aevrynd::Dot(tangent,relative)>Aevrynd::Dot(backed,relative)&&
       SweepBody(player,boar,tangent,side)&&side.kind==Kind::Clear)return tangent;
    return backed;
}
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
    player.guardTicks = player.dodgeTicks = 0;
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
                                    player->second.fieldContributionRun, player->second.dodgeCooldown};
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
    player->second.dodgeCooldown = cooldown(saved.dodgeCooldown);
    player->second.contributed = Boss().phase != EncounterPhase::Defeated && saved.contributionRun == Boss().run;
    if (Field().phase != EncounterPhase::Defeated && saved.fieldContributionRun == Field().run)
        player->second.fieldContributionRun = saved.fieldContributionRun;
    return true;
}

bool RiverbankEncounter::SetPosition(std::uint64_t id, ArenaPosition position, std::int64_t guild) {
    const auto player = party.find(id);
    if (player == party.end() || !Finite(position) || guild < 0) return false;
    player->second.position = position;
    player->second.guild = guild;
    return true;
}

ArenaPosition RiverbankEncounter::ConstrainPlayerMovement(ArenaPosition from,ArenaPosition translation) const {
    if(!Finite(from)||!Finite(translation))return {};
    auto allowed=Planar(translation);
    for(const auto* site:{&northBank,&pasture})
        if(site->state.health&&site->state.phase!=EncounterPhase::Defeated)
            allowed=ClipBodyStep(Planar(from),Planar(site->state.position),allowed,true);
    return {allowed.x,allowed.z};
}

bool RiverbankEncounter::StartDodge(std::uint64_t id, std::uint64_t sequence, ArenaPosition direction) {
    const auto entry = party.find(id);
    const auto length = std::hypot(direction.x, direction.z);
    if (entry == party.end() || !sequence || sequence <= entry->second.sequence ||
        !std::isfinite(length) || length == 0.f) return false;
    auto& player = entry->second;
    if (player.request) return false;
    player.sequence = sequence;
    if (!player.position || !player.status.health || player.dodgeCooldown || player.status.resource < DodgeCost) return false;

    player.status.resource -= DodgeCost;
    player.dodgeTicks = DodgeTicks;
    player.dodgeCooldown = DodgeCooldownTicks;
    player.dodgeStep = {direction.x / length * DodgeStepMeters, direction.z / length * DodgeStepMeters};
    return true;
}

std::optional<ArenaPosition> RiverbankEncounter::DodgeStep(std::uint64_t id) const {
    const auto entry = party.find(id);
    if (entry == party.end() || !entry->second.status.health || !entry->second.dodgeTicks) return {};
    return entry->second.dodgeStep;
}

bool RiverbankEncounter::Submit(std::uint64_t id, std::uint64_t sequence, RiverbankAction action, std::uint64_t ally) {
    const auto player = party.find(id);
    if (player == party.end() || !sequence || sequence <= player->second.sequence || player->second.request ||
        player->second.dodgeTicks || (action != RiverbankAction::Attack && action != RiverbankAction::ClassSkill) ||
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
        InRange(guarded->second, boss.position, sight) &&
        RiverbankStructureMovement::HasLine(boss.position,*guarded->second.position)) return site.tauntOwner;
    std::uint64_t nearest{};
    float distance = sight;
    for (const auto& [id, member] : party) {
        if (!member.status.health || !member.position || !site.Contains(*member.position)) continue;
        const float next = Distance(*member.position, boss.position);
        if (next < distance && RiverbankStructureMovement::HasLine(boss.position,*member.position)) {
            nearest = id;
            distance = next;
        }
    }
    return nearest;
}

unsigned RiverbankEncounter::ResolveAttack(Site& site, std::uint64_t id, Participant& participant, RiverbankAction action) {
    auto& status = participant.status;
    if (!status.health) return 0;
    const auto& boss = site.state;
    const auto stats = CombatStatsFor(status.playerClass);
    const auto canTargetBoss = [&](float range) {
        return InRange(participant,boss.position,range) &&
            RiverbankStructureMovement::HasLine(*participant.position,boss.position);
    };
    if (action == RiverbankAction::Attack) {
        if (status.attackCooldown || !canTargetBoss(stats.attackRange)) return 0;
        status.attackCooldown = stats.attackCooldown;
        CreditContribution(site, participant);
        return stats.attackDamage;
    }
    if (status.skillCooldown || status.resource < stats.skillCost) return 0;

    unsigned damage = 0;
    switch (status.playerClass) {
    case RiverbankClass::Guardian:
        if (!canTargetBoss(6.f)) return 0;
        participant.guardTicks = 45;
        status.guarding = true;
        site.tauntOwner = id;
        break;
    case RiverbankClass::Ranger:
        if (!canTargetBoss(stats.attackRange)) return 0;
        damage = participant.bossGear ? RiverbankBossGearStats::VolleyDamage : 28u;
        break;
    default: return 0;
    }
    status.resource -= stats.skillCost;
    status.skillCooldown = stats.skillCooldown;
    CreditContribution(site, participant);
    return damage;
}

void RiverbankEncounter::ResolveTerritoryAttack(Participant& participant, RiverbankAction action) {
    auto& status = participant.status;
    const auto stats = CombatStatsFor(status.playerClass);
    const bool skill = action == RiverbankAction::ClassSkill;
    const bool unavailable = skill ? status.skillCooldown || status.resource < stats.skillCost : status.attackCooldown;
    if (!status.health || unavailable) return;

    const bool shields = skill && status.playerClass == RiverbankClass::Guardian;
    const float range = shields ? 6.f : stats.attackRange;
    const auto from = *participant.position;
    Participant* nearest = nullptr;
    float nearestDistance = range;
    for (auto& entry : party) {
        auto& target = entry.second;
        if (!target.guild || target.guild == participant.guild || !target.status.health || (!shields && target.dodgeTicks) ||
            !target.position || !RiverbankTerritory::Contains(target.position->x, target.position->z)) continue;
        const float distance = Distance(from, *target.position);
        if (distance > range || (nearest && distance >= nearestDistance) ||
            !RiverbankStructureMovement::HasLine(from, *target.position)) continue;
        nearest = &target;
        nearestDistance = distance;
    }
    if (!nearest) return;

    if (!skill) {
        status.attackCooldown = stats.attackCooldown;
        nearest->pendingTerritoryDamage += stats.attackDamage;
    } else {
        status.resource -= stats.skillCost;
        status.skillCooldown = stats.skillCooldown;
        if (shields) {
            participant.guardTicks = 45;
            status.guarding = true;
        } else {
            nearest->pendingTerritoryDamage += participant.bossGear ? RiverbankBossGearStats::VolleyDamage : 28u;
        }
    }
}

void RiverbankEncounter::ResolveSupport(std::uint64_t id, Participant& participant, std::uint64_t target,
                                        std::span<const SupportMembership> groups) {
    auto& status = participant.status;
    const auto stats = CombatStatsFor(status.playerClass);
    if (!status.health || status.skillCooldown || status.resource < stats.skillCost) return;

    // 救援以施術者和隊友的距離及視線判定，戰鬥區邊界只影響參戰資格。
    const auto groupFor = [groups](std::uint64_t actor) {
        const auto member = std::ranges::find(groups, actor, &SupportMembership::actor);
        return member == groups.end() ? std::uint64_t{} : member->group;
    };
    const auto casterGroup = groupFor(id);
    const auto eligible = [&](const auto& entry) {
        const auto& [actor, member] = entry;
        if (!InRange(member, *participant.position, 9.f)) return false;
        // 圈內公會成員依公會判定支援，施術者也必須在圈內。
        if (member.guild && RiverbankTerritory::Contains(member.position->x, member.position->z)) {
            if (participant.guild != member.guild ||
                !RiverbankTerritory::Contains(participant.position->x, participant.position->z)) return false;
        } else if (groupFor(actor) != casterGroup) {
            return false;
        }
        return RiverbankStructureMovement::HasLine(*participant.position, *member.position);
    };

    auto ally = party.find(target);
    if (!target) {
        float lowestHealthFraction = 1.f;
        for (auto choice = party.begin(); choice != party.end(); ++choice) {
            const auto& health = choice->second.status;
            const float fraction = float(health.health) / float(CombatStatsFor(health.playerClass).health);
            if (fraction >= lowestHealthFraction || !eligible(*choice)) continue;
            lowestHealthFraction = fraction;
            ally = choice;
        }
    }
    if (ally == party.end() || !eligible(*ally)) return;

    auto& recipient = ally->second.status;
    const auto maximum = CombatStatsFor(recipient.playerClass).health;
    if (recipient.health == maximum) return;
    const auto heal = participant.bossGear ? RiverbankBossGearStats::HealAmount : 40u;
    recipient.health = recipient.health ? std::min(maximum, recipient.health + heal) : maximum * 2 / 5;
    status.resource -= stats.skillCost;
    status.skillCooldown = stats.skillCooldown;

    for (const auto* site : {&northBank, &pasture}) {
        if (site->state.phase != EncounterPhase::Defeated && site->Contains(*participant.position) &&
            site->Contains(*ally->second.position)) {
            CreditContribution(*site, participant);
            break;
        }
    }
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
            const auto request = *participant.request;
            if (request.action == RiverbankAction::ClassSkill && status.playerClass == RiverbankClass::Priest)
                ResolveSupport(id, participant, request.ally, groups);
            else if (participant.guild && RiverbankTerritory::Contains(participant.position->x, participant.position->z))
                ResolveTerritoryAttack(participant, request.action);
            else if (northBank.Contains(*participant.position) && Boss().phase != EncounterPhase::Defeated)
                bossDamage = std::min(BossHealth, bossDamage + ResolveAttack(northBank, id, participant, request.action));
            else if (pasture.Contains(*participant.position) && Field().phase != EncounterPhase::Defeated)
                fieldDamage = std::min(FieldHealth, fieldDamage + ResolveAttack(pasture, id, participant, request.action));
        }
        participant.request.reset();
    }
    // 同一更新的雙方出手先集中，再依守衛的防護狀態扣血。
    for (auto& entry : party) {
        auto& participant = entry.second;
        const auto pending = std::exchange(participant.pendingTerritoryDamage, 0u);
        const auto damage = participant.status.guarding ? (pending + 1u) / 2u : pending;
        participant.status.health -= std::min(participant.status.health, damage);
    }
    Advance(northBank, bossDamage);
    Advance(pasture, fieldDamage);
    for (auto& [id, participant] : party) {
        if (participant.dodgeTicks) --participant.dodgeTicks;
        if (participant.dodgeCooldown) --participant.dodgeCooldown;
    }
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
                if (!status.health || victim.dodgeTicks || !victim.position ||
                    StrikeExposureAt(site.field, *victim.position, boss.position, boss.strikeCenter,
                                     true, site.anchor) != RiverbankStrikeExposure::Exposed) continue;
                const auto guardedHit = site.field ? 8u : victim.bossGear ? RiverbankBossGearStats::GuardDamageTaken : 16u;
                const unsigned hit = status.guarding ? guardedHit : site.field ? 22u : 46u;
                status.health -= std::min(status.health, hit);
            }
            boss.phase = EncounterPhase::Recovery;
            boss.phaseTicks = RecoveryTicks;
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
    if (Distance(boss.position, position) > (site.field ? 1.30f : 1.65f)) {
        Move(site, position, site.field ? .15f : .14f);
        const auto towards=Planar(boss.position)-Planar(position);
        Aevrynd::BoxCylinderContact3D contact;
        // 接觸面阻擋追擊時可蓄力，避免野豬因抵達新的停步距離而一直追撞。
        if(!SweepBody(Planar(position),Planar(boss.position),Aevrynd::Normalize(towards)*(ContactMargin*2.f),contact)||
           contact.kind==Aevrynd::BoxCylinderContactKind3D::Clear)return;
    }
    boss.phase = EncounterPhase::Windup;
    boss.strikeCenter = position;
    boss.phaseTicks = site.field ? FieldWindupTicks : WindupTicks;
}

void RiverbankEncounter::Move(Site& site,ArenaPosition position,float step) const {
    auto& boss=site.state;
    const auto previous=Planar(boss.position);
    const auto requested=Planar(position)-previous;
    const float distance=Aevrynd::Length(requested);
    if(distance==0.f)return;
    const auto delta=requested*std::min(1.f,step/distance);
    const auto structure=RiverbankStructureMovement::ResolveStep(boss.position,{delta.x,delta.z},
                                                               {BoarRadius,BoarHalfHeight,BoarRadius});
    auto allowed=Planar(structure);
    for(const auto& [id,player]:party)
        if(player.status.health&&player.position)
            allowed=ClipBodyStep(Planar(*player.position),previous,allowed*-1.f,false)*-1.f;
    boss.position={previous.x+allowed.x,previous.z+allowed.z};
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
