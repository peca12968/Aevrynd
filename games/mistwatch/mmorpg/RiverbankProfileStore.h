#pragma once

#include "RiverbankClass.h"
#include "RiverbankGuild.h"
#include "RiverbankTerritory.h"
#include "RiverbankExpedition.h"
#include "RiverbankGear.h"
#include <filesystem>
#include <memory>
#include <optional>

namespace Mistwatch {
using RiverbankTicket = std::array<std::byte, 32>;

struct RiverbankProfile {
    std::int64_t id{};
    RiverbankClass playerClass{RiverbankClass::None};
    unsigned rewards{};
};

// The dedicated local server owns the database and never stores the raw client ticket.
class RiverbankProfileStore {
public:
    explicit RiverbankProfileStore(const std::filesystem::path& database);
    ~RiverbankProfileStore();
    RiverbankProfileStore(const RiverbankProfileStore&) = delete;
    RiverbankProfileStore& operator=(const RiverbankProfileStore&) = delete;

    std::optional<RiverbankProfile> Find(const RiverbankTicket& ticket);
    std::optional<RiverbankProfile> Create(const RiverbankTicket& ticket, RiverbankClass playerClass);

    // Write the whole eligible party’s rewards in one transaction before reporting a win.
    void Award(std::uint64_t world, std::uint64_t encounter, std::span<const std::int64_t> eligibleProfiles);

    GuildState ReadGuild(std::int64_t profileId);
    GuildResult ChangeGuild(std::int64_t profileId, GuildAction action, std::int64_t target, std::string_view name);

    TerritoryState ReadTerritoryOwner();
    std::string GuildName(std::int64_t guildId);
    unsigned TerritoryRewards(std::int64_t profileId);
    void ClaimTerritory(std::int64_t previous, std::int64_t winner, std::span<const std::int64_t> recipients);

    RiverbankGearState ReadBossGear(std::int64_t profileId);
    GearResult ChangeBossGear(std::int64_t profileId, GearAction action);
    ExpeditionStage ReadExpedition(std::int64_t profileId);
    ExpeditionResult InteractExpedition(std::int64_t profileId, ArenaPosition position);

private:
    struct Database;
    std::unique_ptr<Database> storage;
};
}
