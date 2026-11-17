#include "LocalWorldTcp.h"
#include "LocalWorldClient.h"
#include "RiverbankCrouch.h"
#ifdef AEVRYND_MISTWATCH_PROFILES
#include "RiverbankProfileWire.h"
#include "RiverbankTerritoryWire.h"
#include "RiverbankTerritoryHistoryWire.h"
#endif
#include <algorithm>
#include <charconv>
#include <chrono>
#include <limits>
#include <optional>

namespace Mistwatch {
using namespace Aevrynd;
using Clock = std::chrono::steady_clock;
using namespace LocalWorldTcp;

namespace {
std::optional<std::uint64_t> InitialRoomEpochFromEnvironment() {
    char input[21]{};
    const auto length = GetEnvironmentVariableA("AEVRYND_MISTWATCH_INITIAL_ROOM_EPOCH", input, sizeof(input));
    if (!length) return std::nullopt;
    if (length >= sizeof(input)) throw std::runtime_error("Invalid room invitation epoch");
    std::uint64_t expected{};
    const auto parsed = std::from_chars(input, input + length, expected);
    if (parsed.ptr != input + length || parsed.ec != std::errc{} || !expected)
        throw std::runtime_error("Invalid room invitation epoch");
    return expected;
}

struct TailnetRoute {
    in_addr remote;
    in_addr local;
};

std::optional<TailnetRoute> ReadTailnetRoute() {
    char remote[16]{}, local[16]{};
    const auto remoteLength = GetEnvironmentVariableA("AEVRYND_MISTWATCH_TAILNET_REMOTE", remote, sizeof(remote));
    const auto localLength = GetEnvironmentVariableA("AEVRYND_MISTWATCH_TAILNET_LOCAL", local, sizeof(local));
    if (!remoteLength && !localLength) return std::nullopt;
    if (remoteLength >= sizeof(remote) || localLength >= sizeof(local))
        throw std::runtime_error("Invalid Riverbank tailnet addresses");
    const auto destination = TailnetIPv4(remote);
    const auto source = TailnetIPv4(local);
    if (!destination || !source) throw std::runtime_error("Both Riverbank tailnet IPv4 addresses are required");
    return TailnetRoute{*destination, *source};
}

#ifdef AEVRYND_MISTWATCH_PROFILES
unsigned TicketDigit(wchar_t value) {
    if (value >= L'0' && value <= L'9') return static_cast<unsigned>(value - L'0');
    if (value >= L'a' && value <= L'f') return static_cast<unsigned>(value - L'a') + 10;
    throw std::runtime_error("Invalid local Riverbank character ticket");
}

std::optional<RiverbankTicket> ProfileTicketFromEnvironment() {
    wchar_t text[65]{};
    const auto length = GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_PROFILE_TICKET", text, 65);
    if (!length) return std::nullopt;
    if (length != 64) throw std::runtime_error("Invalid local Riverbank character ticket length");
    RiverbankTicket result{};
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = std::byte((TicketDigit(text[i * 2]) << 4u) | TicketDigit(text[i * 2 + 1]));
    return result;
}
#endif
}

struct LocalWorldClient::State {
    Winsock winsock;
    const std::optional<TailnetRoute> tailnet = ReadTailnetRoute();
    std::optional<std::uint64_t> initialRoomEpoch = InitialRoomEpochFromEnvironment();
    std::optional<Peer> peer;
    std::optional<ReplicationSnapshot3D> snapshot;
    Clock::time_point lastPacket = Clock::now();
    std::uint64_t epoch{}, nextSequence{1}, lastSubmittedTick{};
    std::uint32_t id{};
    unsigned short port{};
    bool connected{}, reconnecting{};
    Clock::time_point retryAt{}, reconnectUntil{};
    RiverbankClass requestedClass{RiverbankClass::None};
#ifdef AEVRYND_MISTWATCH_PROFILES
    static constexpr auto ReconnectWindow = std::chrono::seconds(30);
    std::optional<RiverbankTicket> profileTicket = ProfileTicketFromEnvironment();
    bool CanRetryConnection() const noexcept {
        return profileTicket && connectedProfileClass != RiverbankClass::None && port &&
            !peer && !reconnecting;
    }

    bool RetryConnection() noexcept {
        if (!CanRetryConnection()) return false;
        retryAt = Clock::now();
        reconnectUntil = retryAt + ReconnectWindow;
        reconnecting = true;
        return true;
    }
    bool profileLookupComplete{};
    RiverbankClass connectedProfileClass{RiverbankClass::None};
    std::uint64_t nextGuildSequence{1}, pendingGuildSequence{};
    std::optional<RiverbankGuildWire::Response> guildReply;
    bool pendingGuildRoster{};
    std::optional<GuildRosterPage> guildRoster;
    std::uint64_t nextTerritorySequence{1}, pendingTerritorySequence{};
    std::optional<TerritoryState> territory;
    std::uint64_t nextHistorySequence{1}, pendingHistorySequence{};
    std::optional<std::vector<TerritoryClaim>> territoryHistory;
    std::uint64_t nextExpeditionSequence{1}, pendingExpeditionSequence{};
    std::optional<RiverbankExpeditionWire::Reply> expedition;
    std::uint64_t nextPartySequence{1}, pendingPartySequence{};
    bool pendingPartyPositions{};
    std::optional<RiverbankPartyWire::Reply> party;
    std::uint64_t nextGearSequence{1}, pendingGearSequence{};
    std::optional<RiverbankGearWire::Reply> gear;
    std::uint64_t nextTrailSupplySequence{1}, pendingTrailSupplySequence{};
    std::optional<RiverbankTrailSuppliesWire::Reply> trailSupplies;
#endif
#ifdef AEVRYND_MISTWATCH_BATTLE
    std::optional<RiverbankBattleWire::Encounter> encounter;
    std::uint64_t nextActionSequence{1};
    std::optional<RiverbankFieldWire::Reply> field;
    Clock::time_point nextFieldQuery{};
    std::uint64_t nextFieldSequence{1}, pendingFieldSequence{};
#endif

    explicit State(unsigned short localPort) : port(localPort) { (void)OpenTransport(); }

    bool Supports(std::uint32_t capability) const noexcept {
        if (!snapshot) return false;
        const auto self = std::ranges::find(snapshot->entities, NetworkEntityId3D{epoch, id}, &ReplicatedEntityState3D::id);
        return self != snapshot->entities.end() && (self->flags & capability) != 0;
    }

    bool OpenTransport() {
        Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        const BOOL immediate = TRUE;
        if (!port || !socket || !Nonblocking(socket.Get()) || setsockopt(socket.Get(), IPPROTO_TCP, TCP_NODELAY,
                reinterpret_cast<const char*>(&immediate), sizeof(immediate)) != 0) return false;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (tailnet) {
            sockaddr_in source{};
            source.sin_family = AF_INET;
            source.sin_addr = tailnet->local;
            // 私網連線固定使用本機的 Tailscale 位址，避免改走其他介面。
            if (::bind(socket.Get(), reinterpret_cast<const sockaddr*>(&source), sizeof(source)) != 0) return false;
            address.sin_addr = tailnet->remote;
        }
        address.sin_port = htons(port);
        connected = connect(socket.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
        if (!connected && WSAGetLastError() != WSAEWOULDBLOCK) return false;
        peer.emplace(Peer{std::move(socket), {}, {}, 0});
        lastPacket = Clock::now();
        return true;
    }

    bool jumpQueued{}, releaseJump{}, lastCrouch{};

    Status Poll(std::int16_t x, std::int16_t z, bool sprint, std::optional<bool> jumpInput, bool crouch) {
        const auto now = Clock::now();
        if (reconnecting && now >= reconnectUntil) return Disconnect(false);
        if (!peer) {
            if (!reconnecting) return Status::Disconnected;
            if (now < retryAt) return Status::Reconnecting;
            if (!OpenTransport()) return Disconnect();
        }
        bool waitingForChoice = id != 0 && requestedClass == RiverbankClass::None;
#ifdef AEVRYND_MISTWATCH_PROFILES
        waitingForChoice = waitingForChoice && (!profileTicket || profileLookupComplete);
#endif
        if (!waitingForChoice && now - lastPacket >= std::chrono::seconds(5)) return Disconnect();
        if (!connected) {
            const auto socket = peer->socket.Get();
            fd_set ready{}, failed{};
            FD_SET(socket, &ready);
            FD_SET(socket, &failed);
            timeval immediate{};
            const int result = select(0, nullptr, &ready, &failed, &immediate);
            if (result == SOCKET_ERROR || FD_ISSET(socket, &failed)) return Disconnect();
            if (!FD_ISSET(socket, &ready)) return reconnecting ? Status::Reconnecting : Status::Connecting;
            int error{};
            int size = sizeof(error);
            if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) != 0 || error)
                return Disconnect();
            connected = true;
        }
        bool invalid = false;
        if (!peer->Read([&](auto bytes) { invalid = !Receive(bytes); return !invalid; })) return Disconnect(!invalid);
        crouch = crouch && Supports(RiverbankCrouchCapability);
        if (crouch != lastCrouch) {
            if (!peer->Queue(RiverbankCrouchWire::Encode(crouch))) return Disconnect(false);
            lastCrouch = crouch;
        }
        if (crouch || Supports(RiverbankCrouchedFlag) || !jumpInput || !Supports(RiverbankJumpCapability)) jumpQueued=false;
        else jumpQueued|=*jumpInput;
        if (snapshot && !SubmitMovement(x, z, sprint && !crouch && !Supports(RiverbankCrouchedFlag))) return Disconnect(false);
        if (!peer->Flush()) return Disconnect();
        const auto waiting = reconnecting ? Status::Reconnecting : Status::Joining;
        if (!id) return reconnecting ? Status::Reconnecting : Status::Connecting;
#ifdef AEVRYND_MISTWATCH_PROFILES
        if (profileTicket && !profileLookupComplete) return waiting;
#endif
        if (requestedClass == RiverbankClass::None) return Status::ChoosingClass;
        if (!snapshot) return waiting;
#ifdef AEVRYND_MISTWATCH_BATTLE
        if (!encounter) return waiting;
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
        if (profileTicket) connectedProfileClass = requestedClass;
#endif
        reconnecting = false;
        return Status::Ready;
    }

    bool ChooseClass(RiverbankClass playerClass) {
        if (!peer || !id || requestedClass != RiverbankClass::None || !IsRiverbankClass(playerClass)) return false;
#ifdef AEVRYND_MISTWATCH_PROFILES
        if (profileTicket && !profileLookupComplete) return false;
        const bool queued = profileTicket ? peer->Queue(RiverbankProfileWire::EncodeRequest(*profileTicket, playerClass)) :
            peer->Queue(EncodeRiverbankClass(playerClass));
#else
        const bool queued = peer->Queue(EncodeRiverbankClass(playerClass));
#endif
        if (!queued) { Disconnect(false); return false; }
        requestedClass = playerClass;
        lastPacket = Clock::now();
        return true;
    }

    bool Receive(std::span<const std::byte> bytes) {
#ifdef AEVRYND_MISTWATCH_PROFILES
        if (RiverbankTrailSuppliesWire::HasMagic(bytes)) {
            const auto reply = RiverbankTrailSuppliesWire::DecodeReply(bytes);
            if (!profileTicket || !reply || !pendingTrailSupplySequence || reply->sequence != pendingTrailSupplySequence ||
                (trailSupplies && (reply->state.earnedHides < trailSupplies->state.earnedHides ||
                    (trailSupplies->state.ownsPack && !reply->state.ownsPack)))) return false;
            trailSupplies = *reply;
            pendingTrailSupplySequence = 0;
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankGearWire::HasMagic(bytes)) {
            const auto reply = RiverbankGearWire::DecodeReply(bytes);
            if (!profileTicket || !reply || !pendingGearSequence || reply->sequence != pendingGearSequence) return false;
            pendingGearSequence = 0;
            gear = *reply;
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankPartyWire::HasMagic(bytes)) {
            auto reply = RiverbankPartyWire::DecodeReply(bytes);
            if (!profileTicket || !reply || !pendingPartySequence || reply->sequence != pendingPartySequence ||
                reply->mapPositions != pendingPartyPositions || (reply->members && std::ranges::find(reply->online, id, &RiverbankPartyWire::Member::actor) == reply->online.end()))
                return false;
            pendingPartySequence = 0;
            party = std::move(*reply);
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankExpeditionWire::HasMagic(bytes)) {
            const auto reply = RiverbankExpeditionWire::DecodeReply(bytes);
            if (!profileTicket || !reply || !pendingExpeditionSequence || reply->sequence != pendingExpeditionSequence ||
                (expedition && reply->stage < expedition->stage)) return false;
            pendingExpeditionSequence = 0;
            expedition = *reply;
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankTerritoryWire::HasMagic(bytes)) {
            auto reply = RiverbankTerritoryWire::DecodeState(bytes);
            if (!profileTicket || !reply || !pendingTerritorySequence || reply->sequence != pendingTerritorySequence ||
                (territory && reply->state.revision < territory->revision)) return false;
            pendingTerritorySequence = 0;
            territory = std::move(reply->state);
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankTerritoryHistoryWire::HasMagic(bytes)) {
            auto reply = RiverbankTerritoryHistoryWire::DecodeReply(bytes);
            if (!profileTicket || !reply || !pendingHistorySequence || reply->sequence != pendingHistorySequence)
                return false;
            pendingHistorySequence = 0;
            territoryHistory = std::move(reply->claims);
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankGuildWire::HasMagic(bytes)) {
            if (pendingGuildRoster) {
                auto response = RiverbankGuildWire::DecodeRosterReply(bytes);
                if (!profileTicket || !response || !pendingGuildSequence || response->sequence != pendingGuildSequence ||
                    std::ranges::any_of(response->roster.members, [&](const auto& member) {
                        return member.self != (member.onlinePlayer == id);
                    })) return false;
                pendingGuildSequence = 0;
                pendingGuildRoster = false;
                guildRoster = std::move(response->roster);
                return true;
            }
            auto response = RiverbankGuildWire::DecodeResponse(bytes);
            if (!profileTicket || !response || !pendingGuildSequence || response->sequence != pendingGuildSequence) return false;
            pendingGuildSequence = 0;
            guildReply = std::move(response);
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankProfileWire::HasPrefix(bytes)) {
            const auto found = RiverbankProfileWire::DecodeResponse(bytes);
            if (!id || !profileTicket || !found || snapshot ||
                (reconnecting && *found != connectedProfileClass) ||
                (requestedClass != RiverbankClass::None && *found != requestedClass) ||
                (requestedClass == RiverbankClass::None && profileLookupComplete)) return false;
            profileLookupComplete = true;
            requestedClass = *found;
            lastPacket = Clock::now();
            return true;
        }
#endif
#ifdef AEVRYND_MISTWATCH_BATTLE
        if (RiverbankFieldWire::HasMagic(bytes)) {
            const auto next = RiverbankFieldWire::DecodeReply(bytes);
            if (!next || next->sequence != pendingFieldSequence ||
                (field && (next->field.run < field->field.run || next->hides < field->hides))) return false;
            field = *next;
            pendingFieldSequence = 0;
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankBattleWire::HasMagic(bytes)) {
            auto next = RiverbankBattleWire::DecodeEncounter(bytes);
            if (!snapshot || !next || next->epoch != epoch || next->owner != id || next->tick != snapshot->serverTick ||
                (encounter && next->tick <= encounter->tick) || next->player.playerClass != requestedClass) return false;
            encounter = std::move(next);
            lastPacket = Clock::now();
            return true;
        }
#endif
        ReplicationMessage3D message;
        if (!DecodeReplicationPacket3D(bytes, message)) return false;
        if (!id) {
            const auto* hello = std::get_if<ReplicationHandshake3D>(&message);
            if (!hello || (initialRoomEpoch && hello->sessionEpoch != *initialRoomEpoch)) return false;
            // 只限制邀請碼的首次登入。既有角色的伺服器重啟恢復維持原流程。
            initialRoomEpoch.reset();
            epoch = hello->sessionEpoch;
            id = hello->clientId;
#ifdef AEVRYND_MISTWATCH_PROFILES
            if (profileTicket && !peer->Queue(RiverbankProfileWire::EncodeRequest(*profileTicket, RiverbankClass::None))) return false;
#endif
        } else {
            auto* next = std::get_if<ReplicationSnapshot3D>(&message);
            if (!next || !next->full || next->sessionEpoch != epoch || next->clientId != id
                || (snapshot && next->serverTick <= snapshot->serverTick)
                || std::ranges::count_if(next->entities, [this](const auto& entry) {
                    return entry.id == NetworkEntityId3D{epoch, id} && ReadRiverbankClassFlags(entry.flags) == requestedClass;
                }) != 1 || std::ranges::any_of(next->entities, [this](const auto& entry) {
                    return entry.id.high != epoch || !IsRiverbankClass(ReadRiverbankClassFlags(entry.flags)) ||
                        bool(entry.flags & 1u) != (entry.id.low == id);
                })) return false;
            snapshot = std::move(*next);
        }
        lastPacket = Clock::now();
        return true;
    }

    bool SubmitMovement(std::int16_t x, std::int16_t z, bool sprint) {
#ifdef AEVRYND_MISTWATCH_BATTLE
        if (!encounter || !encounter->player.health) { jumpQueued=false; return true; }
#endif
        const bool jump=jumpQueued&&!releaseJump;
        if ((!x && !z && !jump && !releaseJump) || snapshot->serverTick == lastSubmittedTick) return true;
        // 以收到的伺服器時鐘排程。一次快照最多提出一筆移動，不補送失焦期間的按鍵。
        constexpr std::uint64_t lead = 3;
        if (snapshot->serverTick > (std::numeric_limits<std::uint64_t>::max)() - lead
            || nextSequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
        const ReplicationInputCommand3D input{epoch, snapshot->serverTick + lead, nextSequence, id, x, z, 0, jump,
                                              sprint && Supports(RiverbankSprintCapability)};
        std::vector<std::byte> bytes;
        if (!EncodeReplicationPacket3D(input, bytes) || !peer->Queue(bytes)) return false;
        ++nextSequence;
        lastSubmittedTick = snapshot->serverTick;
        if (jump) jumpQueued=false;
        releaseJump=jump;
        return true;
    }

    Status Disconnect(bool retryTransport = true) {
        peer.reset();
        snapshot.reset();
        connected = false;
        epoch = lastSubmittedTick = 0;
        jumpQueued=releaseJump=lastCrouch=false;
        nextSequence = 1;
        id = 0;
        requestedClass = RiverbankClass::None;
#ifdef AEVRYND_MISTWATCH_BATTLE
        encounter.reset();
        nextActionSequence = 1;
        field.reset();
        nextFieldQuery = {};
        nextFieldSequence = 1;
        pendingFieldSequence = 0;
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
        pendingGuildSequence = 0;
        guildReply.reset();
        pendingGuildRoster = false;
        guildRoster.reset();
        pendingTerritorySequence = 0;
        territory.reset();
        nextHistorySequence = 1;
        pendingHistorySequence = 0;
        territoryHistory.reset();
        pendingExpeditionSequence = pendingPartySequence = pendingGearSequence = 0;
        pendingPartyPositions = false;
        expedition.reset();
        party.reset();
        gear.reset();
        trailSupplies.reset();
        nextTrailSupplySequence = 1;
        pendingTrailSupplySequence = 0;
        nextPartySequence = nextGearSequence = 1;
        nextGuildSequence = nextTerritorySequence = nextExpeditionSequence = 1;
        profileLookupComplete = false;
        if (retryTransport && connectedProfileClass != RiverbankClass::None) {
            const auto now = Clock::now();
            if (!reconnecting) reconnectUntil = now + ReconnectWindow;
            reconnecting = true;
            retryAt = now + std::chrono::milliseconds(500);
            return Status::Reconnecting;
        }
#else
        (void)retryTransport;
#endif
        reconnecting = false;
        return Status::Disconnected;
    }
};

LocalWorldClient::LocalWorldClient(unsigned short port) : state_(std::make_unique<State>(port)) {}
LocalWorldClient::~LocalWorldClient() = default;
LocalWorldClient::Status LocalWorldClient::Poll(std::int16_t x, std::int16_t z, bool sprint, std::optional<bool> jumpInput, bool crouch) {
    return state_->Poll(x, z, sprint, jumpInput, crouch);
}
bool LocalWorldClient::CanCrouch() const noexcept { return state_->Supports(RiverbankCrouchCapability); }
bool LocalWorldClient::IsCrouched() const noexcept { return state_->Supports(RiverbankCrouchedFlag); }
bool LocalWorldClient::CanJump() const noexcept { return state_->Supports(RiverbankJumpCapability); }
bool LocalWorldClient::CanSprint() const noexcept { return state_->Supports(RiverbankSprintCapability); }
bool LocalWorldClient::ChooseClass(RiverbankClass playerClass) { return state_->ChooseClass(playerClass); }
RiverbankClass LocalWorldClient::SelectedClass() const noexcept {
    return state_->snapshot ? state_->requestedClass : RiverbankClass::None;
}
#ifdef AEVRYND_MISTWATCH_BATTLE
bool LocalWorldClient::RequestBattleAction(RiverbankAction action) {
    const auto* battle = Battle();
    if (action == RiverbankAction::Dodge) return false;
    const bool returnToCamp = action == RiverbankAction::ReturnToCamp;
    if (!battle || state_->nextActionSequence == (std::numeric_limits<std::uint64_t>::max)() ||
        (returnToCamp ? !CanReturnToCamp() : !battle->player.health)) return false;
    const auto message = RiverbankBattleWire::EncodeAction({state_->nextActionSequence, 0, action});
    if (!state_->peer->Queue(message)) return false;
    ++state_->nextActionSequence;
    return true;
}

bool LocalWorldClient::RequestDodge(std::int16_t x, std::int16_t z) {
    using namespace RiverbankBattleWire;
    const auto* battle = Battle();
    if (!CanDodge() || !battle || !battle->player.health || (!x && !z) ||
        state_->nextActionSequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
    const auto message = EncodeAction({state_->nextActionSequence, PackDodgeDirection(x,z), RiverbankAction::Dodge});
    if (!state_->peer->Queue(message)) return false;
    ++state_->nextActionSequence;
    return true;
}

bool LocalWorldClient::CanDodge() const noexcept {
    return state_->Supports(RiverbankBattleWire::DodgeCapability) && !IsCrouched();
}

bool LocalWorldClient::IsDodging() const noexcept {
    const auto* snapshot = Snapshot();
    return snapshot && std::ranges::any_of(snapshot->entities, [](const auto& actor) {
        return (actor.flags & 1u) && (actor.flags & RiverbankBattleWire::DodgingEntityFlag);
    });
}

bool LocalWorldClient::HasStructureCover() const noexcept {
    return state_->Supports(RiverbankBattleWire::StructureCoverCapability);
}

bool LocalWorldClient::HasTerritoryCombat() const noexcept {
    return state_->Supports(RiverbankBattleWire::TerritoryCombatCapability);
}

bool LocalWorldClient::CanReturnToCamp() const noexcept {
    const auto* battle = Battle();
    return battle && !battle->player.health && state_->Supports(RiverbankBattleWire::CampReturnCapability);
}

const RiverbankBattleWire::Encounter* LocalWorldClient::Battle() const noexcept {
    return state_->encounter ? &*state_->encounter : nullptr;
}

bool LocalWorldClient::RequestFieldHunt() {
    using namespace RiverbankFieldWire;
    const auto now = Clock::now();
    if (!state_->peer || !state_->snapshot || state_->pendingFieldSequence || now < state_->nextFieldQuery ||
        state_->nextFieldSequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
    if (!state_->Supports(Capability) ||
        !state_->peer->Queue(EncodeQuery(state_->nextFieldSequence))) return false;
    state_->pendingFieldSequence = state_->nextFieldSequence++;
    state_->nextFieldQuery = now + std::chrono::milliseconds(100);
    return true;
}

const RiverbankFieldWire::Reply* LocalWorldClient::FieldHunt() const noexcept {
    return state_->field ? &*state_->field : nullptr;
}
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
bool LocalWorldClient::CanRetryConnection() const noexcept { return state_->CanRetryConnection(); }
bool LocalWorldClient::RetryConnection() noexcept { return state_->RetryConnection(); }
bool LocalWorldClient::CanUseGuild() const noexcept {
    return state_->peer && state_->profileTicket && state_->snapshot && state_->encounter;
}

bool LocalWorldClient::CanUseTrailSupplies() const noexcept {
    return CanUseGuild() && state_->Supports(RiverbankTrailSuppliesWire::Capability);
}

bool LocalWorldClient::RequestTrailSupplies(TrailSupplyAction action) {
    if (!CanUseTrailSupplies() || state_->pendingTrailSupplySequence ||
        state_->nextTrailSupplySequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
    if (!state_->peer->Queue(RiverbankTrailSuppliesWire::EncodeRequest({state_->nextTrailSupplySequence, action}))) return false;
    state_->pendingTrailSupplySequence = state_->nextTrailSupplySequence++;
    return true;
}

const RiverbankTrailSuppliesWire::Reply* LocalWorldClient::TrailSupplies() const noexcept {
    return state_->trailSupplies ? &*state_->trailSupplies : nullptr;
}

bool LocalWorldClient::RequestGuild(GuildAction action, std::uint64_t target, std::string_view name) {
    if (!CanUseGuild() || state_->pendingGuildSequence || state_->nextGuildSequence == (std::numeric_limits<std::uint64_t>::max)())
        return false;
    const auto frame = RiverbankGuildWire::EncodeRequest({state_->nextGuildSequence, action, target, name});
    if (frame.empty() || !state_->peer->Queue(frame)) return false;
    state_->pendingGuildSequence = state_->nextGuildSequence++;
    return true;
}

const RiverbankGuildWire::Response* LocalWorldClient::GuildReply() const noexcept {
    return state_->guildReply ? &*state_->guildReply : nullptr;
}

bool LocalWorldClient::CanReadGuildRoster() const noexcept {
    return CanUseGuild() && state_->Supports(RiverbankGuildWire::RosterCapability);
}

bool LocalWorldClient::RequestGuildRoster(std::uint32_t page) {
    if (!CanReadGuildRoster() || state_->pendingGuildSequence ||
        state_->nextGuildSequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
    const auto frame = RiverbankGuildWire::EncodeRosterQuery({state_->nextGuildSequence, page});
    if (!state_->peer->Queue(frame)) return false;
    state_->pendingGuildSequence = state_->nextGuildSequence++;
    state_->pendingGuildRoster = true;
    return true;
}

const GuildRosterPage* LocalWorldClient::GuildRoster() const noexcept {
    return state_->guildRoster ? &*state_->guildRoster : nullptr;
}

bool LocalWorldClient::RequestTerritory() {
    if (!CanUseGuild() || state_->pendingTerritorySequence || state_->nextTerritorySequence == (std::numeric_limits<std::uint64_t>::max)())
        return false;
    if (!state_->peer->Queue(RiverbankTerritoryWire::EncodeQuery(state_->nextTerritorySequence))) return false;
    state_->pendingTerritorySequence = state_->nextTerritorySequence++;
    return true;
}

const TerritoryState* LocalWorldClient::Territory() const noexcept {
    return state_->territory ? &*state_->territory : nullptr;
}

bool LocalWorldClient::CanReadTerritoryHistory() const noexcept {
    return CanUseGuild() && state_->Supports(RiverbankTerritoryHistoryWire::Capability);
}

bool LocalWorldClient::RequestTerritoryHistory() {
    if (!CanReadTerritoryHistory() || state_->pendingHistorySequence ||
        state_->nextHistorySequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
    if (!state_->peer->Queue(RiverbankTerritoryHistoryWire::Query(state_->nextHistorySequence))) return false;
    state_->pendingHistorySequence = state_->nextHistorySequence++;
    return true;
}

const std::vector<TerritoryClaim>* LocalWorldClient::TerritoryHistory() const noexcept {
    return state_->territoryHistory ? &*state_->territoryHistory : nullptr;
}

bool LocalWorldClient::RequestGear(GearAction action) {
    if (!CanUseGuild() || action > GearAction::Unequip || state_->pendingGearSequence ||
        state_->nextGearSequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
    if (!state_->peer->Queue(RiverbankGearWire::EncodeRequest({state_->nextGearSequence, action}))) return false;
    state_->pendingGearSequence = state_->nextGearSequence++;
    return true;
}

const RiverbankGearWire::Reply* LocalWorldClient::Gear() const noexcept {
    return state_->gear ? &*state_->gear : nullptr;
}

bool LocalWorldClient::RequestParty(PartyAction action, std::uint32_t target) {
    if (!CanUseGuild() || state_->pendingPartySequence || state_->nextPartySequence == (std::numeric_limits<std::uint64_t>::max)())
        return false;
    const RiverbankPartyWire::Request request{state_->nextPartySequence, action, target,
        state_->Supports(RiverbankPartyWire::MapPositionsCapability)};
    if (!RiverbankPartyWire::ValidRequest(request) || !state_->peer->Queue(RiverbankPartyWire::EncodeRequest(request))) return false;
    state_->pendingPartySequence = state_->nextPartySequence++;
    state_->pendingPartyPositions = request.mapPositions;
    return true;
}

const RiverbankPartyWire::Reply* LocalWorldClient::Party() const noexcept {
    return state_->party ? &*state_->party : nullptr;
}

bool LocalWorldClient::RequestExpedition(bool interact) {
    if (!CanUseGuild() || state_->pendingExpeditionSequence || state_->nextExpeditionSequence == (std::numeric_limits<std::uint64_t>::max)())
        return false;
    const bool watch = state_->Supports(RiverbankExpeditionWire::WatchSurveyCapability);
    const bool routes = watch && state_->Supports(RiverbankExpeditionWire::WatchRouteCapability);
    if (!state_->peer->Queue(RiverbankExpeditionWire::EncodeRequest({state_->nextExpeditionSequence, interact, watch, routes}))) return false;
    state_->pendingExpeditionSequence = state_->nextExpeditionSequence++;
    return true;
}

const RiverbankExpeditionWire::Reply* LocalWorldClient::Expedition() const noexcept {
    return state_->expedition ? &*state_->expedition : nullptr;
}
#endif
std::uint32_t LocalWorldClient::PlayerId() const noexcept { return state_->id; }
const ReplicationSnapshot3D* LocalWorldClient::Snapshot() const noexcept {
    return state_->snapshot ? &*state_->snapshot : nullptr;
}
}
