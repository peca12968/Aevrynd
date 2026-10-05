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
    const std::array<std::wstring_view, 7> worlds{
        L"", L"legacy-core", L"legacy-middle", L"forest-canopy", L"forest-wood", L"garbage", L"FOREST-CANOPY"};
    const std::array<std::wstring_view, 5> middles{L"", L"0", L"1", L"garbage", L"00"};
    for (auto world : worlds) for (auto middle : middles)
        check(ResolveWorldProfile(false, world, middle) == WorldProfile::ForestWood);
    check(ResolveWorldProfile(true, L"", L"") == WorldProfile::ForestWood);
    check(ResolveWorldProfile(true, L"", L"1") == WorldProfile::ForestWood);
    for (auto profile : {WorldProfile::LegacyCore, WorldProfile::LegacyMiddle, WorldProfile::ForestCanopy,WorldProfile::ForestWood}) {
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
    check(WorldProfileHasCanopy(WorldProfile::ForestWood));
    check(WorldProfileHasMiddle(WorldProfile::ForestWood));
    check(WorldProfileHasWoodCollision(WorldProfile::ForestWood));
    check(!WorldProfileHasWoodCollision(WorldProfile::ForestCanopy));
    check(!WorldProfileHasWoodCollision(WorldProfile::LegacyCore));
    check(!WorldProfileHasWoodCollision(WorldProfile::LegacyMiddle));
    for (auto flag : {L"garbage", L"FOREST-CANOPY", L"legacy-core ", L" legacy-middle"})
        reject([&] { ResolveWorldProfile(true, flag, L""); });
    const std::wstring embedded(L"forest-canopy\0suffix", 20);
    reject([&] { ResolveWorldProfile(true, embedded, L""); });
    reject([&] { ResolveWorldProfile(true, L"", L"0"); });
    const auto invalid = static_cast<WorldProfile>(-1);
    reject([&] { WorldProfileHasMiddle(invalid); });
    reject([&] { WorldProfileHasCanopy(invalid); });
    reject([&] { WorldProfileName(invalid); });
    reject([&] { WorldProfileHasWoodCollision(invalid); });
    std::cout << "WORLD_PROFILE_PASS positives=" << positives << " negatives=" << negatives << '\n';
}
