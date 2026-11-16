#include "RiverbankEncounter.h"
#include "RiverbankBattleWire.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Mistwatch;

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void LiveBodyStopsCrossingWithoutTrappingThePlayer() {
    RiverbankEncounter arena;
    const auto step=arena.ConstrainPlayerMovement({13.2f,15.f},{.1f,0.f});
    Require(step.x>.047f&&step.x<.051f&&step.z==0.f,"The live boar did not stop the player at its footprint");
    const auto tangent=arena.ConstrainPlayerMovement({13.24f,15.f},{.07f,.07f});
    Require(std::abs(tangent.x)<.00001f&&tangent.z>.069f,"The player could not move along the body");
    const auto inward=arena.ConstrainPlayerMovement({13.30f,15.f},{.1f,0.f});
    const auto escape=arena.ConstrainPlayerMovement({13.30f,15.f},{-.1f,0.f});
    Require(inward.x==0.f&&escape.x==-.1f,"Overlapping old positions must allow escape only");
    const auto open=arena.ConstrainPlayerMovement({1.f,1.f},{.1f,.03f});
    Require(open.x==.1f&&open.z==.03f,"The wild boar affected movement in the camp");

    Require(arena.Join(1,RiverbankClass::Guardian)&&arena.SetPosition(1,{13.f,15.f}),"Contact case setup failed");
    for(unsigned tick=0;tick<8&&arena.Field().phase!=EncounterPhase::Windup;++tick)arena.Tick();
    Require(arena.Field().phase==EncounterPhase::Windup&&arena.Field().target==1&&
            arena.Field().position.x>=14.749f&&arena.Field().position.x<=14.76f,
            "The wild boar crossed the player or stalled before beginning its windup");
}

void RolesAndTelegraph() {
    RiverbankEncounter arena({0, 0});
    Require(arena.Join(1, RiverbankClass::Guardian) && arena.Join(2, RiverbankClass::Ranger) &&
            arena.Join(3, RiverbankClass::Priest), "Party creation failed");
    Require(arena.SetPosition(1, {0, 1}) && arena.SetPosition(2, {0, 4}) && arena.SetPosition(3, {0, 7}), "Invalid setup positions");
    Require(arena.Submit(1, 1, RiverbankAction::ClassSkill) && arena.Submit(2, 1, RiverbankAction::ClassSkill), "Class skills did not queue");
    Require(!arena.Submit(2, 1, RiverbankAction::Attack) && !arena.Submit(2, 2, RiverbankAction::Attack), "Duplicated per-tick action was accepted");
    arena.Tick();
    Require(arena.Boss().phase == EncounterPhase::Windup && arena.Boss().target == 1 && arena.Boss().phaseTicks == 20,
            "First windup did not lock onto guarding player");
    Require(arena.Boss().health == 302 && arena.Player(2)->resource == 68 && arena.Player(2)->skillCooldown == 100,
            "Ranger volley was not charged or resolved");
    Require(arena.Submit(2, 2, RiverbankAction::ClassSkill), "Next volley request failed");
    for (unsigned step = 0; step < RiverbankEncounter::WindupTicks; ++step) arena.Tick();
    Require(arena.Boss().phase == EncounterPhase::Recovery && arena.Boss().health == 302, "Enemy strike or cooldown failed");
    Require(arena.Player(1)->health == 134 && arena.Player(2)->health == 105 && arena.Player(3)->health == 100,
            "Windup did not respect range and guard reduction");
    Require(arena.Submit(3, 1, RiverbankAction::ClassSkill, 1), "Priest heal request failed");
    arena.Tick();
    Require(arena.Player(1)->health == 150 && arena.Player(3)->resource == 60 && arena.Player(3)->skillCooldown == 120,
            "Priest did not heal the ally and consume its own resource");
}

void DefeatOnceAndReset() {
    RiverbankEncounter arena({0, 0});
    Require(arena.Join(1, RiverbankClass::Guardian) && arena.Join(2, RiverbankClass::Ranger) &&
            arena.Join(3, RiverbankClass::Priest) && arena.Join(4, RiverbankClass::Priest), "Party creation failed");
    Require(arena.SetPosition(4, {0, 25}), "Spectator setup failed");
    for (unsigned frame = 1; frame <= 250 && arena.Boss().health; ++frame) {
        const auto boss = arena.Boss();
        const bool dodge = boss.phase == EncounterPhase::Windup && boss.phaseTicks == 1;
        Require(arena.SetPosition(1, {boss.position.x + (dodge ? 3.f : 0.f), 1.f}) &&
                arena.SetPosition(2, {boss.position.x, 5.f}) && arena.SetPosition(3, {boss.position.x, 8.f}), "Position update failed");
        for (std::uint64_t member = 1; member < 4; ++member)
            Require(arena.Submit(member, frame, RiverbankAction::Attack), "Attack queue rejected an increasing sequence");
        arena.Tick();
        if (frame == 1) Require(arena.Boss().health == 293, "Same-tick attacks did not resolve together");
    }
    Require(arena.Boss().phase == EncounterPhase::Defeated, "Party did not defeat encounter");
    for (std::uint64_t member = 1; member < 4; ++member) Require(arena.Player(member)->rewards == 1, "Participant missed its reward");
    Require(arena.Player(4)->rewards == 0 && arena.Player(1)->health == 150, "Spectator reward or movement evasion was wrong");
    const auto defeated = arena.Boss();
    for (unsigned step = 0; step < defeated.phaseTicks; ++step) arena.Tick();
    Require(arena.Boss().phase == EncounterPhase::Patrol && arena.Boss().health == RiverbankEncounter::BossHealth &&
            arena.Boss().run == defeated.run + 1 && arena.Player(1)->rewards == 1, "Defeat rewarded twice or respawn failed");
}

void DownedAndRejoin() {
    RiverbankEncounter arena({0, 0});
    Require(arena.Join(1, RiverbankClass::Ranger) && arena.Join(2, RiverbankClass::Priest), "Party creation failed");
    Require(arena.SetPosition(1, {0, 1}) && arena.SetPosition(2, {0, 6}), "Position update failed");
    for (unsigned frame = 0; frame < 150 && arena.Player(1)->health; ++frame) arena.Tick();
    Require(!arena.Player(1)->health, "The player did not go down after three strikes");
    using Member = RiverbankEncounter::SupportMembership;
    Require(arena.Submit(2, 1, RiverbankAction::ClassSkill, 1), "Revive request failed");
    arena.Tick(std::array{Member{2, 9}});
    Require(arena.Player(1)->health == 0 && arena.Player(2)->resource == 100 &&
            arena.Player(2)->skillCooldown == 0, "Off-team revive changed health or consumed resources");
    Require(arena.Submit(2, 2, RiverbankAction::ClassSkill, 1), "Teammate revive request failed");
    arena.Tick(std::array{Member{1, 9}, Member{2, 9}});
    Require(arena.Player(1)->health == 42 && arena.Player(2)->resource == 60,
            "The priest did not revive the ranger after they joined the same group");
    Require(arena.Leave(1) && !arena.Player(1) && !arena.Submit(1, 9, RiverbankAction::Attack), "Disconnected player's request remained valid");
}

void FieldBoarDisengagesFromDownedPlayer() {
    RiverbankEncounter arena;
    constexpr ArenaPosition victim{16.275f, 15.f};
    Require(arena.Join(1, RiverbankClass::Ranger) && arena.SetPosition(1, victim), "Field defeat setup failed");
    for (int tick = 0; tick < 280 && arena.Player(1)->health; ++tick) arena.Tick();
    const auto struck = arena.Field();
    Require(!arena.Player(1)->health && struck.phase == EncounterPhase::Recovery,
        "Field boar did not down the lone player");
    for (unsigned tick = 0; tick < struck.phaseTicks; ++tick) {
        arena.Tick();
        Require(RiverbankEncounter::InPasture(arena.Field().position), "Disengaging boar left the pasture");
    }
    const auto field = arena.Field();
    const float distance = std::hypot(field.position.x - victim.x, field.position.z - victim.z);
    std::cout << "field_clearance_m=" << distance << '\n';
    Require(distance >= 2.70f && field.health == struck.health && field.run == struck.run && !field.target,
        "Field boar stayed over the downed player or reset the encounter");

    for (int tick = 0; tick < 90 && arena.Field().run == struck.run; ++tick) arena.Tick();
    Require(arena.Field().phase == EncounterPhase::Patrol && arena.Field().run == struck.run + 1,
        "Uncontested field encounter did not reset normally");
}

void CampReturnPreservesRewards() {
    RiverbankEncounter arena;
    Require(arena.Join(1, RiverbankClass::Ranger) && arena.RestoreRewards(1, 7, 2) &&
            arena.SetPosition(1, {0, 48}) && arena.Submit(1, 17, RiverbankAction::Attack), "Camp return setup failed");
    Require(!arena.ReturnToCamp(1, 18), "A standing player could return to camp");
    for (unsigned frame = 0; frame < 180 && arena.Player(1)->health; ++frame) arena.Tick();
    const auto downed = arena.Suspend(1);
    Require(downed && !downed->status.health && downed->contributionRun, "Camp return did not start with an active downed player");
    Require(!arena.ReturnToCamp(1, 17) && arena.ReturnToCamp(1, 18) && !arena.ReturnToCamp(1, 18),
            "Camp return did not enforce the action sequence");
    const auto recovered = arena.Suspend(1);
    Require(recovered && recovered->status.health == 105 && recovered->status.resource == downed->status.resource &&
            recovered->status.rewards == 7 && arena.FieldHides(1) == 2 && !recovered->contributionRun &&
            !recovered->fieldContributionRun, "Camp return lost prior rewards or retained combat credit");
    namespace Wire = Mistwatch::RiverbankBattleWire;
    Require(bool(Wire::DecodeAction(Wire::EncodeAction({18, 0, RiverbankAction::ReturnToCamp}))) &&
            !Wire::DecodeAction(Wire::EncodeAction({18, 2, RiverbankAction::ReturnToCamp})),
            "Camp return accepted a request targeting another character");
}

void RestoreLiveCombat() {
    RiverbankEncounter arena({0, 0});
    Require(arena.Join(1, RiverbankClass::Ranger) && arena.SetPosition(1, {0, 1}) &&
        arena.Submit(1, 1, RiverbankAction::ClassSkill), "Reconnect combat setup failed");
    arena.Tick();
    for (unsigned step = 0; step < RiverbankEncounter::WindupTicks; ++step) arena.Tick();
    const auto saved = arena.Suspend(1);
    Require(saved && saved->status.health == 59 && saved->status.skillCooldown > 10 &&
        saved->contributionRun == arena.Boss().run, "Reconnect did not save the active encounter");
    Require(arena.Leave(1) && arena.Join(3, RiverbankClass::Ranger) && arena.RestoreRewards(3, 7) &&
        arena.RestoreCombat(3, *saved, 10), "Live combat did not resume");
    const auto restored = arena.Suspend(3);
    Require(restored && restored->status.health == saved->status.health && restored->status.rewards == 7 &&
        restored->status.resource == saved->status.resource + 2 &&
        restored->status.skillCooldown == saved->status.skillCooldown - 10 && restored->contributionRun == saved->contributionRun,
        "Reconnect changed health, persistent rewards, cooldowns, or contribution");
}

void NorthBankBoundary() {
    RiverbankEncounter arena;
    Require(arena.Join(1, RiverbankClass::Guardian) && arena.Join(2, RiverbankClass::Ranger), "North bank party setup failed");
    Require(arena.SetPosition(1, {-2.5f, 30.52f}) && arena.SetPosition(2, {2.7f, 39.8257f}), "Approach setup failed");
    for (unsigned tick = 1; tick <= 120; ++tick) {
        Require(arena.Submit(2, tick, RiverbankAction::ClassSkill), "Ranged request was not queued");
        arena.Tick();
        const auto& boss = arena.Boss();
        Require(RiverbankEncounter::InArena(boss.position) && boss.phase == EncounterPhase::Patrol &&
                boss.target == 0 && boss.health == RiverbankEncounter::BossHealth && arena.Player(2)->resource == 100,
                "Boss left the clearing or allowed an attack from the riverbed");
    }
    Require(arena.SetPosition(2, {2.7f, 44.f}) && arena.Submit(2, 121, RiverbankAction::ClassSkill), "Arena entry setup failed");
    bool sawWindup = false;
    for (unsigned tick = 0; tick < 100 && !sawWindup; ++tick) {
        arena.Tick();
        const auto& boss = arena.Boss();
        Require(RiverbankEncounter::InArena(boss.position) && boss.health == 302, "Arena combat did not resolve inside the clearing");
        sawWindup = boss.phase == EncounterPhase::Windup && RiverbankEncounter::InArena(boss.strikeCenter);
    }
    Require(sawWindup, "Entering the north bank did not trigger a valid windup");
}

void BossGearChangesSkillResults() {
    RiverbankEncounter arena({0, 0});
    Require(arena.Join(1, RiverbankClass::Guardian) && arena.Join(2, RiverbankClass::Priest), "Gear test setup failed");
    auto injured = *arena.Suspend(1);
    injured.status.health = 80;
    Require(arena.RestoreCombat(1, injured, 0) && arena.SetPosition(1, {0, 1}) &&
            arena.SetPosition(2, {0, 6}) && arena.SetBossGear(2, true) && arena.SetBossGear(1, true), "Gear did not equip");
    Require(arena.Submit(2, 1, RiverbankAction::ClassSkill, 1) && arena.Submit(1, 1, RiverbankAction::ClassSkill),
            "Equipped skills failed to queue");
    arena.Tick();
    Require(arena.Player(1)->health == 126 && arena.Player(2)->resource == 60, "Equipped priest heal amount is incorrect");
    for (unsigned i = 0; i < RiverbankEncounter::WindupTicks; ++i) arena.Tick();
    Require(arena.Player(1)->health == 114, "Equipped guardian did not mitigate the guarded strike");
}

void BattleWireRoundTrip() {
    namespace Wire = Mistwatch::RiverbankBattleWire;
    const auto action = Wire::EncodeAction({0x1000200030004000ull, 7, RiverbankAction::ClassSkill});
    const auto decodedAction = Wire::DecodeAction(action);
    Require(decodedAction && decodedAction->sequence == 0x1000200030004000ull && decodedAction->ally == 7 &&
            decodedAction->kind == RiverbankAction::ClassSkill, "Battle action did not preserve identity and sequence");
    Require(!Wire::DecodeAction(std::span(action).first(action.size() - 1)), "Truncated battle action was accepted");

    Wire::Encounter frame{99, 125, 2, {RiverbankClass::Ranger, 105, 68, 11, 90, 2, false},
        {EncounterPhase::Windup, {2.25f, 47.5f}, {3.f, 46.25f}, 221, 17, 2, 5}};
    const auto encoded = Wire::EncodeEncounter(frame);
    const auto read = Wire::DecodeEncounter(encoded);
    Require(read && read->owner == 2 && read->tick == 125 && read->player.skillCooldown == 90 &&
            read->boss.position.z == 47.5f && read->boss.strikeCenter.z == 46.25f && read->boss.phaseTicks == 17 &&
            read->boss.run == 5, "Battle state did not preserve timing, resources, and warning position");
    frame.boss.health = 0;
    Require(!Wire::DecodeEncounter(Wire::EncodeEncounter(frame)), "A live encounter with zero boss health was accepted");
    frame.boss.health = 221;
    frame.boss.position.x = (std::numeric_limits<float>::quiet_NaN)();
    Require(!Wire::DecodeEncounter(Wire::EncodeEncounter(frame)), "A nonfinite boss position was accepted");
}
}

int main() {
    try {
        LiveBodyStopsCrossingWithoutTrappingThePlayer();
        RolesAndTelegraph();
        DefeatOnceAndReset();
        DownedAndRejoin();
        CampReturnPreservesRewards();
        FieldBoarDisengagesFromDownedPlayer();
        RestoreLiveCombat();
        NorthBankBoundary();
        BattleWireRoundTrip();
        BossGearChangesSkillResults();
        std::cout << "RIVERBANK_ENCOUNTER_RULES_PASS roles=3 windup=1 simultaneous_attacks=3 revive=1 reward_once=1 respawn=1 north_bank_boundary=1 wire=1\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
