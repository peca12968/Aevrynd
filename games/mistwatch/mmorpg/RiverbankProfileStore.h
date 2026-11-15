#pragma once

#include "RiverbankClass.h"
#include "RiverbankGuild.h"
#include "RiverbankTerritory.h"
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

// 資料庫由專用伺服器管理，角色票證僅保存雜湊。
class RiverbankProfileStore {
public:
    explicit RiverbankProfileStore(const std::filesystem::path& database);
    ~RiverbankProfileStore();
    RiverbankProfileStore(const RiverbankProfileStore&) = delete;
    RiverbankProfileStore& operator=(const RiverbankProfileStore&) = delete;

    std::optional<RiverbankProfile> Find(const RiverbankTicket& ticket);
    std::optional<RiverbankProfile> Create(const RiverbankTicket& ticket, RiverbankClass playerClass);

    // 先以一筆交易完整寫入符合資格的隊伍獎勵，再回報勝利。
    void Award(std::uint64_t world, std::uint64_t encounter, std::span<const std::int64_t> eligibleProfiles);

    GuildState ReadGuild(std::int64_t profileId);
    GuildResult ChangeGuild(std::int64_t profileId, GuildAction action, std::int64_t target, std::string_view name);

    TerritoryState ReadTerritoryOwner();
    std::string GuildName(std::int64_t guildId);
    unsigned TerritoryRewards(std::int64_t profileId);
    void ClaimTerritory(std::int64_t previous, std::int64_t winner, std::span<const std::int64_t> recipients);

private:
    struct Database;
    std::unique_ptr<Database> storage;
};
}
