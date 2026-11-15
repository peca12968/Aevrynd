#include "RiverbankProfileStore.h"
#include <Windows.h>
#include <iostream>
#include <stdexcept>

using namespace Mistwatch;

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        auto root = std::filesystem::path(argv[1]) / (L"profile-store-" + std::to_wstring(GetCurrentProcessId()));
        Require(std::filesystem::create_directory(root), "A fresh profile test directory is required");
        const auto file = root / L"河岸測試.sqlite3";
        RiverbankTicket guard{}, priest{};
        guard.front() = std::byte{45};
        priest.front() = std::byte{97};
        std::int64_t savedGuard{};
        {
            RiverbankProfileStore store(file);
            Require(!store.Find(guard) && !store.Create({}, RiverbankClass::Ranger) && !store.Create(guard, RiverbankClass::None),
                "Invalid or missing profile created a character");
            const auto first = store.Create(guard, RiverbankClass::Guardian);
            const auto second = store.Create(priest, RiverbankClass::Priest);
            Require(first && second && first->id != second->id, "Two profiles were not distinct");
            savedGuard = first->id;
            Require(!store.Create(guard, RiverbankClass::Ranger), "Existing profile changed class");
            Require(!store.ReadBossGear(first->id).earned && store.ChangeBossGear(first->id, GearAction::Equip) == GearResult::NotEarned,
                "Uncredited own character could equip a boss item");
            const std::array firstParty{first->id, second->id, first->id};
            store.Award(17, 5, firstParty);
            store.Award(17, 5, firstParty);
            Require(store.Find(guard)->rewards == 1 && store.Find(priest)->rewards == 1, "Same encounter rewarded more than once");

            Require(store.ReadBossGear(first->id).earned && !store.ReadBossGear(first->id).equipped &&
                store.ChangeBossGear(first->id, GearAction::Equip) == GearResult::Ok &&
                store.ChangeBossGear(first->id, GearAction::Equip) == GearResult::Ok,
                "Credited character could not equip its own reward");
            const std::array invalidParty{first->id, second->id + 40};
            bool rolledBack = false;
            try { store.Award(17, 6, invalidParty); } catch (const std::runtime_error&) { rolledBack = true; }
            Require(rolledBack && store.Find(guard)->rewards == 1 && store.Find(priest)->rewards == 1,
                "A failed reward group left a partial write");
            bool leaseRejected = false;
            try { RiverbankProfileStore duplicate(file); } catch (const std::runtime_error&) { leaseRejected = true; }
            Require(leaseRejected, "A second server could open a live profile database");
        }
        {
            RiverbankProfileStore reopened(file);
            const auto guardSaved = reopened.Find(guard);
            const auto priestSaved = reopened.Find(priest);
            Require(guardSaved && guardSaved->id == savedGuard && guardSaved->rewards == 1 &&
                guardSaved->playerClass == RiverbankClass::Guardian && priestSaved && priestSaved->rewards == 1,
                "Restart did not restore immutable classes and loot");
            Require(reopened.ReadBossGear(savedGuard).equipped &&
                reopened.ChangeBossGear(savedGuard, GearAction::Unequip) == GearResult::Ok &&
                reopened.ReadBossGear(savedGuard).earned && !reopened.ReadBossGear(savedGuard).equipped,
                "Restart or unequip lost the durable boss item reference");
            const std::array nextParty{guardSaved->id};
            reopened.Award(18, 5, nextParty);
            Require(reopened.Find(guard)->rewards == 2, "Same encounter number in a new world was not counted");
        }
        std::cout << "RIVERBANK_PROFILE_STORE_PASS restart=1 duplicates_rejected=1 rollback=1 single_writer=1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
