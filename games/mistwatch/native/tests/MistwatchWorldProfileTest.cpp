#include "MistwatchWorldProfile.h"
#include <array>
#include <iostream>
#include <string>

int main() {
    using namespace Mistwatch;
    unsigned positives = 0, negatives = 0;
    const auto check = [&](bool value) {
        if (!value) throw std::runtime_error("World profile contract failed");
        ++positives;
    };
    const auto reject = [&](auto action) {
        bool rejected = false;
        try { action(); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Invalid profile accepted");
        ++negatives;
    };
    const std::array<std::wstring_view, 6> worlds{
        L"", L"legacy-core", L"legacy-middle", L"forest-canopy", L"garbage", L"FOREST-CANOPY"};
    const std::array<std::wstring_view, 5> middles{L"", L"0", L"1", L"garbage", L"00"};
    for (auto world : worlds) for (auto middle : middles)
        check(ResolveWorldProfile(false, world, middle) == WorldProfile::ForestCanopy);
    check(ResolveWorldProfile(true, L"", L"") == WorldProfile::ForestCanopy);
    check(ResolveWorldProfile(true, L"", L"1") == WorldProfile::ForestCanopy);
    for (auto profile : {WorldProfile::LegacyCore, WorldProfile::LegacyMiddle, WorldProfile::ForestCanopy}) {
        const auto name = WorldProfileName(profile);
        const std::wstring flag(name.begin(), name.end());
        check(ResolveWorldProfile(true, flag, L"") == profile);
        check(ResolveWorldProfile(true, flag, WorldProfileHasMiddle(profile) ? L"1" : L"0") == profile);
        reject([&] { ResolveWorldProfile(true, flag, WorldProfileHasMiddle(profile) ? L"0" : L"1"); });
        for (auto invalid : {L"garbage", L"00", L" 1", L"1 "})
            reject([&] { ResolveWorldProfile(true, flag, invalid); });
    }
    check(!WorldProfileHasMiddle(WorldProfile::LegacyCore));
    check(WorldProfileHasMiddle(WorldProfile::LegacyMiddle));
    check(WorldProfileHasMiddle(WorldProfile::ForestCanopy));
    check(!WorldProfileHasCanopy(WorldProfile::LegacyCore));
    check(!WorldProfileHasCanopy(WorldProfile::LegacyMiddle));
    check(WorldProfileHasCanopy(WorldProfile::ForestCanopy));
    for (auto flag : {L"garbage", L"FOREST-CANOPY", L"legacy-core ", L" legacy-middle"})
        reject([&] { ResolveWorldProfile(true, flag, L""); });
    const std::wstring embedded(L"forest-canopy\0suffix", 20);
    reject([&] { ResolveWorldProfile(true, embedded, L""); });
    reject([&] { ResolveWorldProfile(true, L"", L"0"); });
    const auto invalid = static_cast<WorldProfile>(-1);
    reject([&] { WorldProfileHasMiddle(invalid); });
    reject([&] { WorldProfileHasCanopy(invalid); });
    reject([&] { WorldProfileName(invalid); });
    std::cout << "WORLD_PROFILE_PASS positives=" << positives << " negatives=" << negatives << '\n';
}
