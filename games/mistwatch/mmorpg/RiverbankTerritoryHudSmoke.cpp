#include "RiverbankTerritoryHud.h"
#include <array>
#include <iostream>

using Mistwatch::GuildRank;
using Mistwatch::ReadRiverbankTerritoryHud;
using Mistwatch::RiverbankTerritory;
using Mistwatch::RiverbankTerritoryDisplayName;
using Mistwatch::RiverbankTerritoryPhase;
using Mistwatch::TerritoryState;

int main() {
    RiverbankTerritory field;
    const auto view = [&] {
        TerritoryState state{};
        state.owner = field.Owner();
        state.challenger = field.Challenger();
        state.progress = field.Progress();
        state.contested = field.Contested();
        return ReadRiverbankTerritoryHud(state);
    };
    const std::array invading{RiverbankTerritory::Presence{11, 2, GuildRank::Officer},
                              RiverbankTerritory::Presence{12, 2, GuildRank::Member}};
    const std::array contest{invading[0], invading[1], RiverbankTerritory::Presence{21, 1, GuildRank::Member}};
    if (view().phase != RiverbankTerritoryPhase::Unclaimed || view().showProgress) return 1;
    field.Tick(contest);
    if (const auto zero = view(); zero.phase != RiverbankTerritoryPhase::Contested ||
        !zero.showProgress || zero.percent || zero.fraction != 0.f) return 2;
    field.ApplyOwnership(1);
    if (view().phase != RiverbankTerritoryPhase::Held || view().showProgress) return 3;
    for (unsigned i = 0; i < 60; ++i) field.Tick(invading);
    const auto underway = view();
    if (underway.phase != RiverbankTerritoryPhase::Capturing || !underway.showProgress ||
        underway.percent != 25 || underway.fraction != .25f) return 4;
    for (unsigned i = 0; i < 50; ++i)
        if (!field.Tick(contest).empty()) return 5;
    const auto paused = view();
    if (paused.phase != RiverbankTerritoryPhase::Contested || !paused.showProgress ||
        paused.percent != underway.percent || paused.fraction != underway.fraction) return 6;
    for (unsigned i = 0; i < 179; ++i) field.Tick(invading);
    if (view().phase != RiverbankTerritoryPhase::Capturing || view().percent != 99) return 7;
    if (field.Tick(invading).size() != 2) return 8;
    field.ApplyOwnership(2);
    if (view().phase != RiverbankTerritoryPhase::Held || view().showProgress) return 9;
    if (RiverbankTerritoryDisplayName("北岸瞭望") != "北岸瞭望" ||
        RiverbankTerritoryDisplayName("abcdefghijklmnopqrstu") != "abcdefghijklmnopq…" ||
        RiverbankTerritoryDisplayName("abcdefghijklmnop界她們") != "abcdefghijklmnop界…") return 10;
    std::cout << "TERRITORY_HUD_CLAIM_CONTEST_RESUME_AND_OWNERSHIP_VERIFIED\n";
}
