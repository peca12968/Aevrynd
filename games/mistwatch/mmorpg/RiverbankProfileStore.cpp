#include "RiverbankProfileStore.h"

#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Windows.h>
#include <winsqlite/winsqlite3.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace Mistwatch {
namespace {
constexpr char Schema[] = R"sql(
CREATE TABLE IF NOT EXISTS riverbank_character (
    id INTEGER PRIMARY KEY,
    ticket_sha256 BLOB NOT NULL UNIQUE CHECK(typeof(ticket_sha256) = 'blob' AND length(ticket_sha256) = 32),
    profession INTEGER NOT NULL CHECK(profession BETWEEN 1 AND 3)
);
CREATE TABLE IF NOT EXISTS riverbank_reward (
    character_id INTEGER NOT NULL REFERENCES riverbank_character(id),
    world INTEGER NOT NULL CHECK(world > 0),
    encounter INTEGER NOT NULL CHECK(encounter > 0),
    PRIMARY KEY (character_id, world, encounter)
) WITHOUT ROWID;
PRAGMA user_version = 1;
)sql";
constexpr char GuildSchema[] = R"sql(
CREATE TABLE riverbank_guild (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL COLLATE NOCASE CHECK(length(CAST(name AS BLOB)) BETWEEN 1 AND 48),
    retired INTEGER NOT NULL DEFAULT 0 CHECK(retired IN (0, 1))
);
CREATE UNIQUE INDEX riverbank_active_guild_name ON riverbank_guild(name) WHERE retired = 0;
CREATE TABLE riverbank_guild_member (
    character_id INTEGER PRIMARY KEY REFERENCES riverbank_character(id),
    guild_id INTEGER NOT NULL REFERENCES riverbank_guild(id),
    rank INTEGER NOT NULL CHECK(rank BETWEEN 1 AND 3)
);
CREATE INDEX riverbank_guild_roster ON riverbank_guild_member(guild_id);
CREATE UNIQUE INDEX riverbank_guild_leader ON riverbank_guild_member(guild_id) WHERE rank = 3;
CREATE TABLE riverbank_guild_invite (
    character_id INTEGER PRIMARY KEY REFERENCES riverbank_character(id),
    guild_id INTEGER NOT NULL REFERENCES riverbank_guild(id)
);
CREATE INDEX riverbank_guild_invites ON riverbank_guild_invite(guild_id);
PRAGMA user_version = 2;
)sql";
constexpr char TerritorySchema[] = R"sql(
CREATE TABLE riverbank_territory_claim (
    revision INTEGER PRIMARY KEY,
    previous_guild_id INTEGER REFERENCES riverbank_guild(id),
    guild_id INTEGER NOT NULL REFERENCES riverbank_guild(id),
    claimed_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    CHECK(previous_guild_id IS NULL OR previous_guild_id <> guild_id)
);
CREATE TABLE riverbank_territory_reward (
    revision INTEGER NOT NULL REFERENCES riverbank_territory_claim(revision),
    character_id INTEGER NOT NULL REFERENCES riverbank_character(id),
    PRIMARY KEY (revision, character_id)
) WITHOUT ROWID;
CREATE INDEX riverbank_territory_rewards_by_character ON riverbank_territory_reward(character_id);
PRAGMA user_version = 3;
)sql";
constexpr char ExpeditionSchema[] = R"sql(
CREATE TABLE riverbank_expedition (
    character_id INTEGER PRIMARY KEY REFERENCES riverbank_character(id),
    stage INTEGER NOT NULL CHECK(stage BETWEEN 1 AND 4),
    combat_world INTEGER,
    combat_encounter INTEGER,
    CHECK((stage IN (1, 2) AND combat_world IS NULL AND combat_encounter IS NULL) OR
          (stage IN (3, 4) AND combat_world IS NOT NULL AND combat_encounter IS NOT NULL)),
    FOREIGN KEY (character_id, combat_world, combat_encounter)
        REFERENCES riverbank_reward(character_id, world, encounter)
);
PRAGMA user_version = 4;
)sql";
constexpr char BossGearSchema[] = R"sql(
CREATE TABLE riverbank_boss_kit (
    character_id INTEGER PRIMARY KEY,
    reward_world INTEGER NOT NULL,
    reward_encounter INTEGER NOT NULL,
    FOREIGN KEY (character_id, reward_world, reward_encounter)
        REFERENCES riverbank_reward(character_id, world, encounter)
);
PRAGMA user_version = 5;
)sql";
constexpr char FieldHidesSchema[] = R"sql(
CREATE TABLE riverbank_field_hide (
    character_id INTEGER NOT NULL REFERENCES riverbank_character(id),
    world INTEGER NOT NULL,
    encounter INTEGER NOT NULL,
    PRIMARY KEY (character_id, world, encounter)
);
PRAGMA user_version = 6;
)sql";
constexpr char TrailSuppliesSchema[] = R"sql(
CREATE TABLE riverbank_trail_pack (
    character_id INTEGER NOT NULL PRIMARY KEY,
    first_world INTEGER NOT NULL,
    first_encounter INTEGER NOT NULL,
    second_world INTEGER NOT NULL,
    second_encounter INTEGER NOT NULL,
    CHECK (first_world < second_world OR (first_world = second_world AND first_encounter < second_encounter)),
    FOREIGN KEY (character_id, first_world, first_encounter)
        REFERENCES riverbank_field_hide(character_id, world, encounter),
    FOREIGN KEY (character_id, second_world, second_encounter)
        REFERENCES riverbank_field_hide(character_id, world, encounter)
);
PRAGMA user_version = 7;
)sql";

void Require(sqlite3* db, int code, int expected = SQLITE_OK) {
    if (code != expected) throw std::runtime_error(std::string("Riverbank profile database: ") + sqlite3_errmsg(db));
}

void Execute(sqlite3* db, const char* sql) { Require(db, sqlite3_exec(db, sql, nullptr, nullptr, nullptr)); }

constexpr int ProfileBusyTimeoutMs = 2500;
enum class TransactionWait { Normal, NoWait };

struct Transaction {
    sqlite3* db;
    bool active{};
    explicit Transaction(sqlite3* database, TransactionWait wait = TransactionWait::Normal) : db(database) {
        if (wait == TransactionWait::Normal) {
            Execute(db, "BEGIN IMMEDIATE");
            active = true;
            return;
        }
        // 只有取得寫入權才開始扣款。資料庫忙碌時不等待，以免阻塞整個遊戲更新。
        Require(db, sqlite3_busy_timeout(db, 0));
        const auto started = sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
        const auto restored = sqlite3_busy_timeout(db, ProfileBusyTimeoutMs);
        if (restored != SQLITE_OK && started == SQLITE_OK)
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        Require(db, restored);
        if (started == SQLITE_BUSY) return;
        Require(db, started);
        active = true;
    }
    ~Transaction() { if (active) sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr); }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    explicit operator bool() const noexcept { return active; }
    void Commit() { Execute(db, "COMMIT"); active = false; }
};

struct Statement {
    sqlite3* db;
    sqlite3_stmt* query{};
    Statement(sqlite3* database, const char* sql) : db(database) {
        const auto code = sqlite3_prepare_v2(db, sql, -1, &query, nullptr);
        if (code != SQLITE_OK) {
            sqlite3_finalize(query);
            Require(db, code);
        }
    }
    ~Statement() { sqlite3_finalize(query); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void Bind(int index, std::int64_t value) { Require(db, sqlite3_bind_int64(query, index, value)); }
    void Bind(int index, const RiverbankTicket& digest) {
        Require(db, sqlite3_bind_blob(query, index, digest.data(), static_cast<int>(digest.size()), SQLITE_TRANSIENT));
    }
    void Bind(int index, std::string_view text) {
        Require(db, sqlite3_bind_text(query, index, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT));
    }
    int Step() { return sqlite3_step(query); }
    std::int64_t Value(int column) { return sqlite3_column_int64(query, column); }
    std::string Text(int column) {
        const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(query, column));
        if (!value) throw std::runtime_error("Missing Riverbank text field");
        return {value, static_cast<std::size_t>(sqlite3_column_bytes(query, column))};
    }
    void Done() { Require(db, Step(), SQLITE_DONE); }
    void Reset() { Require(db, sqlite3_reset(query)); }
};

RiverbankTicket Digest(const RiverbankTicket& ticket) {
    RiverbankTicket digest;
    if (!Aevrynd::WorldDeltaSha2563D(ticket, digest)) throw std::runtime_error("Cannot hash the Riverbank profile ticket");
    return digest;
}

bool HasTicket(const RiverbankTicket& ticket) {
    return std::ranges::any_of(ticket, [](std::byte value) { return value != std::byte{}; });
}

std::int64_t PositiveId(std::uint64_t value) {
    if (!value || value > std::uint64_t((std::numeric_limits<std::int64_t>::max)()))
        throw std::invalid_argument("Riverbank reward identifier is outside the database range");
    return static_cast<std::int64_t>(value);
}

bool ValidGuildName(std::string_view name) {
    if (name.empty() || name.size() > GuildNameBytes || name.front() == ' ' || name.back() == ' ') return false;
    std::array<wchar_t, GuildNameBytes> wide{};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()),
        wide.data(), static_cast<int>(wide.size()));
    if (!count) return false;
    const auto characters = std::span(wide).first(static_cast<std::size_t>(count));
    return std::ranges::none_of(characters, [](wchar_t c) {
        return c < 0x20 || (c >= 0x7f && c <= 0x9f) || (c >= 0x202a && c <= 0x202e) || (c >= 0x2066 && c <= 0x2069);
    });
}

void SetGuildRank(sqlite3* db, std::int64_t profile, GuildRank rank) {
    Statement update(db, "UPDATE riverbank_guild_member SET rank = ?1 WHERE character_id = ?2");
    update.Bind(1, static_cast<std::int64_t>(rank));
    update.Bind(2, profile);
    update.Done();
}

void ClearGuildInvite(sqlite3* db, std::int64_t profile) {
    Statement remove(db, "DELETE FROM riverbank_guild_invite WHERE character_id = ?1");
    remove.Bind(1, profile);
    remove.Done();
}
}

struct RiverbankProfileStore::Database {
    HANDLE lease{INVALID_HANDLE_VALUE};
    sqlite3* handle{};
    std::filesystem::path path;
    void Open(const std::filesystem::path& file) {
        path = file;
        const int opened = sqlite3_open16(file.c_str(), &handle);
        if (!handle) throw std::runtime_error("Cannot open the Riverbank profile database");
        Require(handle, opened);
        // 同一個租約下的註冊和遊戲結算可能同時寫入。由 SQLite 排程，避免瞬間 SQLITE_BUSY 結束伺服器。
        Require(handle, sqlite3_busy_timeout(handle, ProfileBusyTimeoutMs));
    }
    ~Database() {
        if (handle) sqlite3_close(handle);
        if (lease != INVALID_HANDLE_VALUE) CloseHandle(lease);
    }
};

RiverbankProfileStore::RiverbankProfileStore(const std::filesystem::path& path) : storage(std::make_unique<Database>()) {
    if (!path.is_absolute() || path.filename().empty()) throw std::invalid_argument("An absolute Riverbank database path is required");
    std::filesystem::create_directories(path.parent_path());
    auto leasePath = path;
    leasePath += L".lock";
    storage->lease = CreateFileW(leasePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (storage->lease == INVALID_HANDLE_VALUE) throw std::runtime_error("Riverbank profile database is already open or inaccessible");
    storage->Open(path);
    auto* db = storage->handle;
    Execute(db, "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;");
    std::int64_t schema{};
    {
        Statement version(db, "PRAGMA user_version");
        Require(db, version.Step(), SQLITE_ROW);
        schema = version.Value(0);
    }
    if (schema < 0 || schema > 7) throw std::runtime_error("Unsupported Riverbank profile schema");
    if (schema < 7) {
        Transaction upgrade(db);
        if (!schema) Execute(db, Schema);
        if (schema < 2) Execute(db, GuildSchema);
        if (schema < 3) Execute(db, TerritorySchema);
        if (schema < 4) Execute(db, ExpeditionSchema);
        if (schema < 5) Execute(db, BossGearSchema);
        if (schema < 6) Execute(db, FieldHidesSchema);
        Execute(db, TrailSuppliesSchema);
        upgrade.Commit();
    }
}

RiverbankProfileStore::~RiverbankProfileStore() = default;

RiverbankProfileStore::RiverbankProfileStore(std::unique_ptr<Database> connection) : storage(std::move(connection)) {}

std::unique_ptr<RiverbankProfileStore> RiverbankProfileStore::OpenRegistrationConnection() const {
    if (storage->lease == INVALID_HANDLE_VALUE) throw std::logic_error("A database lease is required for the registration connection");
    auto connection = std::make_unique<Database>();
    connection->Open(storage->path);
    Execute(connection->handle, "PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
    return std::unique_ptr<RiverbankProfileStore>(new RiverbankProfileStore(std::move(connection)));
}

std::optional<RiverbankProfile> RiverbankProfileStore::Find(const RiverbankTicket& ticket) {
    if (!HasTicket(ticket)) return std::nullopt;
    auto* db = storage->handle;
    Statement read(db, "SELECT id, profession, (SELECT COUNT(*) FROM riverbank_reward WHERE character_id = c.id), "
                       "(SELECT COUNT(*) FROM riverbank_field_hide WHERE character_id = c.id), "
                       "EXISTS(SELECT 1 FROM riverbank_trail_pack WHERE character_id = c.id) "
                       "FROM riverbank_character AS c WHERE ticket_sha256 = ?1");
    read.Bind(1, Digest(ticket));
    const int code = read.Step();
    if (code == SQLITE_DONE) return std::nullopt;
    Require(db, code, SQLITE_ROW);
    const auto job = read.Value(1);
    const auto rewards = read.Value(2);
    const auto hides = read.Value(3);
    constexpr auto MaxCount = (std::numeric_limits<unsigned>::max)();
    const bool pack = read.Value(4) != 0;
    if (read.Value(0) <= 0 || job < 1 || job > 3 || rewards < 0 || rewards > MaxCount ||
        hides < 0 || hides > MaxCount || (pack && hides < RiverbankTrailSupplies::HideCost))
        throw std::runtime_error("Invalid Riverbank character record");
    return RiverbankProfile{read.Value(0), RiverbankClass(job), static_cast<unsigned>(rewards), static_cast<unsigned>(hides), pack};
}

std::optional<RiverbankProfile> RiverbankProfileStore::Create(const RiverbankTicket& ticket, RiverbankClass playerClass) {
    if (!HasTicket(ticket) || !IsRiverbankClass(playerClass)) return std::nullopt;
    if (const auto existing = Find(ticket)) return existing->playerClass == playerClass ? existing : std::nullopt;
    auto* db = storage->handle;
    Statement create(db, "INSERT OR IGNORE INTO riverbank_character(ticket_sha256, profession) VALUES (?1, ?2)");
    create.Bind(1, Digest(ticket));
    create.Bind(2, static_cast<std::int64_t>(playerClass));
    Require(db, create.Step(), SQLITE_DONE);
    const auto existing = Find(ticket);
    return existing && existing->playerClass == playerClass ? existing : std::nullopt;
}

void RiverbankProfileStore::Award(std::uint64_t world, std::uint64_t encounter, std::span<const std::int64_t> profiles) {
    if (profiles.empty()) return;
    const auto worldId = PositiveId(world);
    const auto encounterId = PositiveId(encounter);
    auto* db = storage->handle;
    Transaction award(db);
    Statement credit(db, "INSERT OR IGNORE INTO riverbank_reward(character_id, world, encounter) VALUES (?1, ?2, ?3)");
    Statement expedition(db, "UPDATE riverbank_expedition SET stage=3, combat_world=?2, combat_encounter=?3 "
                              "WHERE character_id=?1 AND stage=2");
    for (const auto id : profiles) {
        if (id <= 0) throw std::invalid_argument("Invalid Riverbank reward recipient");
        credit.Bind(1, id);
        credit.Bind(2, worldId);
        credit.Bind(3, encounterId);
        credit.Done();
        if (sqlite3_changes(db) == 1) {
            expedition.Bind(1, id);
            expedition.Bind(2, worldId);
            expedition.Bind(3, encounterId);
            expedition.Done();
            expedition.Reset();
        }
        credit.Reset();
    }
    award.Commit();
}

void RiverbankProfileStore::AwardFieldHides(std::uint64_t world, std::uint64_t encounter,
                                                std::span<const std::int64_t> profiles) {
    if (profiles.empty()) return;
    const auto worldId = PositiveId(world);
    const auto encounterId = PositiveId(encounter);
    auto* db = storage->handle;
    Transaction award(db);
    Statement credit(db, "INSERT OR IGNORE INTO riverbank_field_hide(character_id, world, encounter) VALUES (?1, ?2, ?3)");
    for (const auto id : profiles) {
        if (id <= 0) throw std::invalid_argument("Invalid field hunt reward recipient");
        credit.Bind(1, id);
        credit.Bind(2, worldId);
        credit.Bind(3, encounterId);
        credit.Done();
        credit.Reset();
    }
    award.Commit();
}

TrailSupplyResult RiverbankProfileStore::ExchangeTrailPack(std::int64_t profileId) {
    auto* db = storage->handle;
    Transaction exchange(db, TransactionWait::NoWait);
    if (!exchange) return TrailSupplyResult::Rejected;
    {
        Statement prior(db, "SELECT 1 FROM riverbank_trail_pack WHERE character_id=?1");
        prior.Bind(1, profileId);
        const auto result = prior.Step();
        if (result == SQLITE_ROW) return TrailSupplyResult::AlreadyOwned;
        Require(db, result, SQLITE_DONE);
    }
    // 只從該角色已落盤的兩筆獸皮收據兌換，保留原狩獵歷程。
    static_assert(RiverbankTrailSupplies::HideCost == 2);
    std::array<std::int64_t, 4> hides{};
    {
        Statement earned(db, "SELECT world,encounter FROM riverbank_field_hide WHERE character_id=?1 "
                             "ORDER BY world,encounter LIMIT 2");
        earned.Bind(1, profileId);
        for (std::size_t receipt = 0; receipt < RiverbankTrailSupplies::HideCost; ++receipt) {
            const auto result = earned.Step();
            if (result == SQLITE_DONE) return TrailSupplyResult::NotEnough;
            Require(db, result, SQLITE_ROW);
            hides[receipt * 2] = earned.Value(0);
            hides[receipt * 2 + 1] = earned.Value(1);
        }
    }
    Statement write(db, "INSERT INTO riverbank_trail_pack(character_id,first_world,first_encounter,second_world,second_encounter) "
                        "VALUES (?1,?2,?3,?4,?5)");
    write.Bind(1, profileId);
    for (std::size_t index = 0; index < hides.size(); ++index)
        write.Bind(static_cast<int>(index) + 2, hides[index]);
    write.Done();
    exchange.Commit();
    return TrailSupplyResult::Exchanged;
}

GuildState RiverbankProfileStore::ReadGuild(std::int64_t profileId) {
    auto* db = storage->handle;
    GuildState state;
    Statement guild(db, "SELECT g.id, m.rank, g.name FROM riverbank_guild_member AS m "
                        "JOIN riverbank_guild AS g ON g.id=m.guild_id AND g.retired=0 WHERE m.character_id=?1");
    guild.Bind(1, profileId);
    const auto found = guild.Step();
    if (found != SQLITE_DONE) {
        Require(db, found, SQLITE_ROW);
        return {guild.Value(0), GuildRank(guild.Value(1)), guild.Text(2), 0, {}};
    }
    Statement invite(db, "SELECT g.id, g.name FROM riverbank_guild_invite AS i "
                         "JOIN riverbank_guild AS g ON g.id=i.guild_id AND g.retired=0 WHERE i.character_id=?1");
    invite.Bind(1, profileId);
    const auto invited = invite.Step();
    if (invited != SQLITE_DONE) {
        Require(db, invited, SQLITE_ROW);
        state.inviteId = invite.Value(0);
        state.inviteName = invite.Text(1);
    }
    return state;
}

GuildResult RiverbankProfileStore::ChangeGuild(std::int64_t profile, GuildAction action, std::int64_t target, std::string_view name) {
    auto* db = storage->handle;
    if (profile <= 0 || action > GuildAction::Transfer) return GuildResult::Rejected;
    if (action == GuildAction::View) return GuildResult::Ok;
    Transaction change(db);
    const auto own = ReadGuild(profile);
    switch (action) {
    case GuildAction::Create: {
        if (own.guildId) return GuildResult::Rejected;
        if (!ValidGuildName(name)) return GuildResult::InvalidName;
        Statement create(db, "INSERT OR IGNORE INTO riverbank_guild(name) VALUES (?1)");
        create.Bind(1, name);
        create.Done();
        if (!sqlite3_changes(db)) return GuildResult::NameTaken;
        Statement leader(db, "INSERT INTO riverbank_guild_member(character_id, guild_id, rank) VALUES (?1, ?2, 3)");
        leader.Bind(1, profile);
        leader.Bind(2, sqlite3_last_insert_rowid(db));
        leader.Done();
        ClearGuildInvite(db, profile);
        break;
    }
    case GuildAction::Invite: {
        if (own.rank < GuildRank::Officer || target <= 0 || target == profile) return GuildResult::Rejected;
        Statement invite(db, "INSERT OR IGNORE INTO riverbank_guild_invite(character_id, guild_id) "
                             "SELECT id, ?2 FROM riverbank_character WHERE id=?1 AND "
                             "NOT EXISTS(SELECT 1 FROM riverbank_guild_member WHERE character_id=?1)");
        invite.Bind(1, target);
        invite.Bind(2, own.guildId);
        invite.Done();
        if (!sqlite3_changes(db)) return GuildResult::Rejected;
        break;
    }
    case GuildAction::Join:
    case GuildAction::Decline: {
        if (own.guildId || !target || own.inviteId != target) return GuildResult::Rejected;
        if (action == GuildAction::Join) {
            Statement join(db, "INSERT INTO riverbank_guild_member(character_id, guild_id, rank) VALUES (?1, ?2, 1)");
            join.Bind(1, profile);
            join.Bind(2, target);
            join.Done();
        }
        ClearGuildInvite(db, profile);
        break;
    }
    case GuildAction::Leave: {
        if (!own.guildId) return GuildResult::Rejected;
        if (own.rank == GuildRank::Leader) {
            Statement roster(db, "SELECT COUNT(*) FROM riverbank_guild_member WHERE guild_id=?1");
            roster.Bind(1, own.guildId);
            Require(db, roster.Step(), SQLITE_ROW);
            if (roster.Value(0) > 1) return GuildResult::TransferRequired;
            if (ReadTerritoryOwner().owner == own.guildId) return GuildResult::OwnsTerritory;
            Statement retire(db, "UPDATE riverbank_guild SET retired=1 WHERE id=?1");
            retire.Bind(1, own.guildId);
            retire.Done();
            Statement clear(db, "DELETE FROM riverbank_guild_invite WHERE guild_id=?1");
            clear.Bind(1, own.guildId);
            clear.Done();
        }
        Statement leave(db, "DELETE FROM riverbank_guild_member WHERE character_id=?1");
        leave.Bind(1, profile);
        leave.Done();
        break;
    }
    case GuildAction::Promote:
    case GuildAction::Demote:
    case GuildAction::Transfer: {
        if (own.rank != GuildRank::Leader || target <= 0 || target == profile) return GuildResult::Rejected;
        const auto other = ReadGuild(target);
        if (other.guildId != own.guildId || (action == GuildAction::Promote && other.rank != GuildRank::Member) ||
            (action == GuildAction::Demote && other.rank != GuildRank::Officer)) return GuildResult::Rejected;
        if (action == GuildAction::Transfer) {
            SetGuildRank(db, profile, GuildRank::Officer);
            SetGuildRank(db, target, GuildRank::Leader);
        } else SetGuildRank(db, target, action == GuildAction::Promote ? GuildRank::Officer : GuildRank::Member);
        break;
    }
    case GuildAction::View: return GuildResult::Ok;
    }
    change.Commit();
    return GuildResult::Ok;
}

TerritoryState RiverbankProfileStore::ReadTerritoryOwner() {
    auto* db = storage->handle;
    Statement read(db, "SELECT claim.revision, claim.guild_id, guild.name FROM riverbank_territory_claim AS claim "
                       "JOIN riverbank_guild AS guild ON guild.id=claim.guild_id ORDER BY claim.revision DESC LIMIT 1");
    TerritoryState state;
    const auto code = read.Step();
    if (code != SQLITE_DONE) {
        Require(db, code, SQLITE_ROW);
        state.revision = read.Value(0);
        state.owner = read.Value(1);
        state.ownerName = read.Text(2);
    }
    return state;
}

std::string RiverbankProfileStore::GuildName(std::int64_t guildId) {
    if (!guildId) return {};
    auto* db = storage->handle;
    Statement name(db, "SELECT name FROM riverbank_guild WHERE id=?1");
    name.Bind(1, guildId);
    Require(db, name.Step(), SQLITE_ROW);
    return name.Text(0);
}

unsigned RiverbankProfileStore::TerritoryRewards(std::int64_t profileId) {
    auto* db = storage->handle;
    Statement count(db, "SELECT COUNT(*) FROM riverbank_territory_reward WHERE character_id=?1");
    count.Bind(1, profileId);
    Require(db, count.Step(), SQLITE_ROW);
    const auto result = count.Value(0);
    if (result < 0 || result > (std::numeric_limits<unsigned>::max)()) throw std::runtime_error("Invalid territory reward count");
    return static_cast<unsigned>(result);
}

void RiverbankProfileStore::ClaimTerritory(std::int64_t previous, std::int64_t winner, std::span<const std::int64_t> recipients) {
    if (winner <= 0 || winner == previous || recipients.empty()) throw std::invalid_argument("Invalid territory claim");
    auto* db = storage->handle;
    Transaction claim(db);
    if (ReadTerritoryOwner().owner != previous) throw std::runtime_error("Territory owner changed before the claim committed");
    Statement event(db, "INSERT INTO riverbank_territory_claim(previous_guild_id, guild_id) "
                        "SELECT NULLIF(?1, 0), id FROM riverbank_guild WHERE id=?2 AND retired=0");
    event.Bind(1, previous);
    event.Bind(2, winner);
    event.Done();
    if (sqlite3_changes(db) != 1) throw std::runtime_error("Territory winner is no longer active");
    const auto revision = sqlite3_last_insert_rowid(db);
    Statement credit(db, "INSERT INTO riverbank_territory_reward(revision, character_id) SELECT ?1, character_id "
                         "FROM riverbank_guild_member WHERE character_id=?2 AND guild_id=?3");
    for (const auto profile : recipients) {
        credit.Bind(1, revision);
        credit.Bind(2, profile);
        credit.Bind(3, winner);
        credit.Done();
        if (sqlite3_changes(db) != 1) throw std::runtime_error("Territory reward recipient left the winning guild");
        credit.Reset();
    }
    claim.Commit();
}

RiverbankGearState RiverbankProfileStore::ReadBossGear(std::int64_t profile) {
    auto* db = storage->handle;
    Statement read(db, "SELECT EXISTS(SELECT 1 FROM riverbank_reward WHERE character_id=?1), "
                       "EXISTS(SELECT 1 FROM riverbank_boss_kit WHERE character_id=?1)");
    read.Bind(1, profile);
    Require(db, read.Step(), SQLITE_ROW);
    const RiverbankGearState gear{read.Value(0) != 0, read.Value(1) != 0};
    if (gear.equipped && !gear.earned) throw std::runtime_error("Equipped Riverbank gear lacks a valid reward");
    return gear;
}

GearResult RiverbankProfileStore::ChangeBossGear(std::int64_t profile, GearAction action) {
    auto* db = storage->handle;
    if (profile <= 0) return GearResult::Rejected;
    switch (action) {
    case GearAction::View:
        return GearResult::Ok;
    case GearAction::Equip: {
        // 以既有的首領結算作為裝備來源。相同角色只能裝備一份。
        Statement equip(db, "INSERT INTO riverbank_boss_kit(character_id, reward_world, reward_encounter) "
                            "SELECT character_id,world,encounter FROM riverbank_reward WHERE character_id=?1 "
                            "ORDER BY world,encounter LIMIT 1 ON CONFLICT(character_id) DO NOTHING");
        equip.Bind(1, profile);
        equip.Done();
        return ReadBossGear(profile).equipped ? GearResult::Ok : GearResult::NotEarned;
    }
    case GearAction::Unequip: {
        Statement unequip(db, "DELETE FROM riverbank_boss_kit WHERE character_id=?1");
        unequip.Bind(1, profile);
        unequip.Done();
        return GearResult::Ok;
    }
    }
    return GearResult::Rejected;
}

ExpeditionStage RiverbankProfileStore::ReadExpedition(std::int64_t profile) {
    auto* db = storage->handle;
    Statement progress(db, "SELECT stage FROM riverbank_expedition WHERE character_id=?1");
    progress.Bind(1, profile);
    const auto status = progress.Step();
    if (status == SQLITE_DONE) return ExpeditionStage::Available;
    Require(db, status, SQLITE_ROW);
    const auto stage = progress.Value(0);
    if (stage < 1 || stage > 4) throw std::runtime_error("Invalid Riverbank expedition stage");
    return ExpeditionStage(stage);
}

ExpeditionResult RiverbankProfileStore::InteractExpedition(std::int64_t profile, ArenaPosition position) {
    auto* db = storage->handle;
    Transaction interaction(db);
    const auto stage = ReadExpedition(profile);
    if (stage == ExpeditionStage::Hunt) return ExpeditionResult::HuntRequired;
    if (stage == ExpeditionStage::Complete) return ExpeditionResult::Finished;
    if (!RiverbankExpedition::CanInteract(stage, position)) return ExpeditionResult::TooFar;

    if (stage == ExpeditionStage::Available) {
        Statement accept(db, "INSERT INTO riverbank_expedition(character_id, stage) VALUES (?1, 1)");
        accept.Bind(1, profile);
        accept.Done();
    } else {
        Statement advance(db, "UPDATE riverbank_expedition SET stage=stage+1 WHERE character_id=?1 AND stage=?2");
        advance.Bind(1, profile);
        advance.Bind(2, static_cast<std::int64_t>(stage));
        advance.Done();
        if (sqlite3_changes(db) != 1) throw std::runtime_error("Riverbank expedition stage changed during interaction");
    }
    interaction.Commit();
    return ExpeditionResult::Advanced;
}
}
