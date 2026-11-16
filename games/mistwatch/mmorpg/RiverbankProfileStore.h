#pragma once

#include "RiverbankClass.h"
#include "RiverbankGuild.h"
#include "RiverbankTerritory.h"
#include "RiverbankExpedition.h"
#include "RiverbankGear.h"
#include "RiverbankTrailSupplies.h"
#include <filesystem>
#include <memory>
#include <optional>

namespace Mistwatch {
using RiverbankTicket = std::array<std::byte, 32>;

struct RiverbankProfile {
    std::int64_t id{};
    RiverbankClass playerClass{RiverbankClass::None};
    unsigned rewards{}, fieldHides{};
    bool trailPack{};
};

struct RiverbankWatchMember {
    std::int64_t profileId{};
    ArenaPosition position{};
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

    // 註冊專用連線只在持有主要資料庫租約時使用，SQLite WAL 仍以 FULL 保證落盤。
    std::unique_ptr<RiverbankProfileStore> OpenRegistrationConnection() const;

    // Write the whole eligible party’s rewards in one transaction before reporting a win.
    void Award(std::uint64_t world, std::uint64_t encounter, std::span<const std::int64_t> eligibleProfiles);

    void AwardFieldHides(std::uint64_t world, std::uint64_t encounter, std::span<const std::int64_t> eligibleProfiles);
    TrailSupplyResult ExchangeTrailPack(std::int64_t profileId);
    GuildState ReadGuild(std::int64_t profileId);
    GuildResult ChangeGuild(std::int64_t profileId, GuildAction action, std::int64_t target, std::string_view name);

    TerritoryState ReadTerritoryOwner();
    std::vector<TerritoryClaim> ReadTerritoryHistory();
    std::string GuildName(std::int64_t guildId);
    unsigned TerritoryRewards(std::int64_t profileId);
    void ClaimTerritory(std::int64_t previous, std::int64_t winner, std::span<const std::int64_t> recipients);

    RiverbankGearState ReadBossGear(std::int64_t profileId);
    GearResult ChangeBossGear(std::int64_t profileId, GearAction action);
    ExpeditionStage ReadExpedition(std::int64_t profileId, bool watchSurvey = false);
    ExpeditionResult InteractExpedition(std::int64_t profileId, ArenaPosition position, bool watchSurvey = false,
                                        std::span<const RiverbankWatchMember> watchMembers = {});

private:
    struct Database;
    explicit RiverbankProfileStore(std::unique_ptr<Database> connection);

    std::unique_ptr<Database> storage;
};
}
